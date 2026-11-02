//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#if defined( _WIN32 ) && !defined( _X360 )
#include "winlite.h"
#endif
#include "tier0/dbg.h"
#include "tier0/tslist.h"
#include "tier0/icommandline.h"
#include "vstdlib/jobthread.h"
#include "vstdlib/random.h"
#include "tier1/functors.h"
#include "tier1/fmtstr.h"
#include "tier1/utlvector.h"
#include "tier1/generichash.h"
#include "tier0/vprof.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

#include "tier0/memdbgon.h"


class CJobThread;

//-----------------------------------------------------------------------------

inline void ServiceJobAndRelease( CJob *pJob, intp iThread = -1 )
{
	// TryLock() would only fail if another thread has entered
	// Execute() or Abort()
	if ( !pJob->IsFinished() && pJob->TryLock() )
	{
		// ...service the request
		pJob->SetServiceThread( iThread );
		pJob->Execute();
		pJob->Unlock();
	}
	pJob->Release();
}

//-----------------------------------------------------------------------------
// dimhotepus: Wakes idle worker threads, all platforms.
//
// Workers used to wait on kernel events: on Windows via WaitForMultipleObjects, where the
// shared queue event was Set/Reset (a syscall each, ~300ns) whenever the queue flipped
// between empty and non-empty, which with fast workers is nearly every job, and every Set
// woke *all* idle workers for a single job. On POSIX they polled with ~100ms latency.
//
// Now an idle worker sleeps on one condition variable and re-checks its "has work"
// predicate under this mutex. Producers change state first, then Notify(). Notify() is a
// fence plus one load when nobody sleeps (no syscall), and a job any worker can take wakes
// only one sleeper.
//
// No lost wakeups: sleeper registers (RMW on m_nSleepers) under the mutex and then re-checks
// for work; producer publishes work, then reads sleeper count with an RMW on the same
// variable. RMWs on one variable are totally ordered, so either producer's RMW comes later
// and sees the sleeper (then takes the mutex, which sleeper only releases inside wait, and
// notifies it), or sleeper's RMW comes later, synchronizes with producer's and sees the work.
//-----------------------------------------------------------------------------
class CWorkerWake final
{
public:
	// Work any worker can take (shared queue job).
	void NotifyOne()
	{
		Notify( false );
	}

	// Work for a specific worker (direct queue job, message), wake all, others go back to sleep.
	void NotifyAll()
	{
		Notify( true );
	}

	template <typename Predicate>
	void WaitUntil( Predicate &&hasWork )
	{
		std::unique_lock<std::mutex> lock( m_mutex );

		m_nSleepers.fetch_add( 1, std::memory_order_acq_rel );

		while ( !hasWork() )
		{
			// Timeout is only a safety net, every state change notifies.
			m_cv.wait_for( lock, std::chrono::milliseconds( 500 ) );
		}

		m_nSleepers.fetch_sub( 1, std::memory_order_relaxed );
	}

private:
	void Notify( bool bAll )
	{
		// RMW (not plain load) to read latest count, see class comment.
		if ( m_nSleepers.fetch_add( 0, std::memory_order_acq_rel ) == 0 )
			return;

		{
			std::lock_guard<std::mutex> lock( m_mutex );
		}

		if ( bAll )
			m_cv.notify_all();
		else
			m_cv.notify_one();
	}

	std::mutex				m_mutex;
	std::condition_variable	m_cv;
	std::atomic_int			m_nSleepers{ 0 };
};

//-----------------------------------------------------------------------------

class alignas(16) CJobQueue final : public CAlignedNewDelete<16>
{
public:
	CJobQueue()
	{
		for ( auto &queue : m_pQueues )
		{
			queue = new CTSQueue<CJob *>;
		}
	}

	~CJobQueue()
	{
		for ( auto *queue : m_pQueues )
		{
			delete queue;
		}
	}

	CJobQueue( const CJobQueue & ) = delete;
	CJobQueue &operator=( const CJobQueue & ) = delete;

	[[nodiscard]] int Count() const
	{
		return m_nItems.load( std::memory_order_relaxed );
	}

	[[nodiscard]] int Count( JobPriority_t priority ) const
	{
		return m_pQueues[ClampPriority( priority )]->Count();
	}


	CJob *PrePush()
	{
		if ( Count() >= m_nMaxItems )
		{
			CJob *pOverflowJob;
			if ( Pop( &pOverflowJob ) )
			{
				return pOverflowJob;
			}
		}
		return nullptr;
	}

	int Push( CJob *pJob, [[maybe_unused]] int iThread = -1 )
	{
		pJob->AddRef();

		CJob *pOverflowJob;
		int nOverflow = 0;
		while ( ( pOverflowJob = PrePush() ) != nullptr )
		{
			// dimhotepus: Service the job popped off the full queue, not the one being pushed.
			// Servicing pJob here executed the new job early and leaked a reference to the
			// overflow job (which was then never executed and never released).
			ServiceJobAndRelease( pOverflowJob );
			nOverflow++;
		}

		// dimhotepus: Push and count update are done atomically under the queue lock.
		// Previously the item was pushed outside the lock while Pop() reserved an item by
		// count first and then scanned priority sub-queues one by one, so a concurrent
		// Pop() could take "its" item from another priority, find nothing, and leave a job
		// stranded in the queue with m_nItems == 0 (never executed, waiters hang forever).
		bool bWasEmpty;
		{
			AUTO_LOCK( m_mutex );

			m_pQueues[ClampPriority( pJob->GetPriority() )]->PushItem( pJob );
			bWasEmpty = m_nItems.fetch_add( 1, std::memory_order_relaxed ) == 0;
		}

		// Notify outside the queue lock, so woken worker doesn't block on it.
		if ( m_pWake )
		{
			if ( m_bWakeAll )
			{
				// Only owner thread consumes, it only sleeps when queue is empty.
				if ( bWasEmpty )
					m_pWake->NotifyAll();
			}
			else
			{
				// Every job may need its own worker, wake one per job (free when none sleep).
				m_pWake->NotifyOne();
			}
		}

		return nOverflow;
	}

	bool Pop( CJob **ppJob )
	{
		AUTO_LOCK( m_mutex );

		if ( m_nItems.load( std::memory_order_relaxed ) == 0 )
		{
			*ppJob = nullptr;
			return false;
		}

		for ( int i = JP_HIGH; i >= JP_LOW; --i )
		{
			if ( m_pQueues[i]->PopItem( ppJob ) )
			{
				m_nItems.fetch_sub( 1, std::memory_order_relaxed );
				return true;
			}
		}

		AssertMsg( 0, "Expected at least one queue item" );
		*ppJob = nullptr;
		return false;
	}

	// Who to wake when job becomes available. bWakeAll for queues consumed by a single
	// specific worker (direct queues), otherwise one worker is woken per job.
	void SetWake( CWorkerWake *pWake, bool bWakeAll )
	{
		m_pWake = pWake;
		m_bWakeAll = bWakeAll;
	}

	// Aborts and releases all queued jobs. Returns number of jobs removed.
	int Flush()
	{
		CUtlVector<CJob *> jobs;

		{
			AUTO_LOCK( m_mutex );

			jobs.EnsureCapacity( m_nItems.load( std::memory_order_relaxed ) );

			CJob *pJob;
			for ( int i = JP_HIGH; i >= JP_LOW; --i )
			{
				while ( m_pQueues[i]->PopItem( &pJob ) )
				{
					jobs.AddToTail( pJob );
				}
			}

			m_nItems.store( 0, std::memory_order_relaxed );
		}

		// dimhotepus: Abort/Release outside of the queue lock. Release may run arbitrary
		// destructors (functor payloads) which must not execute while holding the lock.
		for ( auto *j : jobs )
		{
			j->Abort();
			j->Release();
		}

		return static_cast<int>( jobs.Count() );
	}

private:
	[[nodiscard]] static int ClampPriority( int priority )
	{
		AssertMsg( priority >= JP_LOW && priority <= JP_HIGH, "Invalid job priority %d", priority );
		return std::clamp( priority, static_cast<int>( JP_LOW ), static_cast<int>( JP_HIGH ) );
	}

	CTSQueue<CJob *>	*m_pQueues[JP_HIGH + 1];
	std::atomic_int		m_nItems{ 0 };
	int					m_nMaxItems{ INT_MAX };
	CWorkerWake			*m_pWake{ nullptr };
	bool				m_bWakeAll{ false };
	CThreadMutex		m_mutex;

};

//-----------------------------------------------------------------------------
//
// CThreadPool
//
//-----------------------------------------------------------------------------
// dimhotepus: Fix aligned alloc.
class CThreadPool : public CAlignedNewDelete<16, CRefCounted1<IThreadPool, CRefCountServiceMT>>
{
public:
	CThreadPool();
	~CThreadPool() override;

	//-----------------------------------------------------
	// Thread functions
	//-----------------------------------------------------
	bool Start( const ThreadPoolStartParams_t &startParams = ThreadPoolStartParams_t() ) override { return Start( startParams, nullptr ); }
	bool Start( const ThreadPoolStartParams_t &startParams, const char *pszNameOverride ) override;
	bool Stop( int timeout = TT_INFINITE ) override;
	void Distribute( bool bDistribute = true, int *pAffinityTable = nullptr ) override;

	//-----------------------------------------------------
	// Functions for any thread
	//-----------------------------------------------------
	unsigned GetJobCount() const override { return m_nJobs; }
	intp NumThreads() const override;
	intp NumIdleThreads() const override;

	//-----------------------------------------------------
	// Pause/resume processing jobs
	//-----------------------------------------------------
	int SuspendExecution() override;
	int ResumeExecution() override;

	//-----------------------------------------------------
	// Offer the current thread to the pool
	//-----------------------------------------------------
	unsigned YieldWait( CThreadEvent **pEvents, int nEvents, bool bWaitAll = true, unsigned timeout = TT_INFINITE ) override;
	unsigned YieldWait( CJob **, int nJobs, bool bWaitAll = true, unsigned timeout = TT_INFINITE ) override;
	void Yield( unsigned timeout ) override;

	//-----------------------------------------------------
	// Add a native job to the queue (master thread)
	//-----------------------------------------------------
	void AddJob( CJob * ) override;
	void InsertJobInQueue( CJob * );

	//-----------------------------------------------------
	// All threads execute pFunctor asap. Thread will either wake up
	//  and execute or execute pFunctor right after completing current job and
	//  before looking for another job.
	//-----------------------------------------------------
	void ExecuteHighPriorityFunctor( CFunctor *pFunctor ) override;

	//-----------------------------------------------------
	// Add an function object to the queue (master thread)
	//-----------------------------------------------------
	// dimhotepus: flags unsigned -> unsigned char
	void AddFunctorInternal( CFunctor *, CJob ** = nullptr, const char *pszDescription = nullptr, unsigned char flags = 0 ) override;

	//-----------------------------------------------------
	// Remove a job from the queue (master thread)
	//-----------------------------------------------------
	void ChangePriority( CJob *p, JobPriority_t priority ) override;

	//-----------------------------------------------------
	// Bulk job manipulation (blocking)
	//-----------------------------------------------------
	int ExecuteToPriority( JobPriority_t toPriority, JobFilter_t pfnFilter = nullptr ) override;
	int AbortAll() override;

	void Reserved1() override {}

	void WaitForIdle( bool bAll = true );

private:
	enum
	{
		IO_STACKSIZE = ( 64 * 1024 ),
		COMPUTATION_STACKSIZE = 0,
	};

	//-----------------------------------------------------
	//
	//-----------------------------------------------------
	CJob *PeekJob();
	CJob *GetDummyJob() override;

	class CYieldDeadline;
	unsigned YieldWaitImpl( CThreadEvent **pEvents, int nEvents, bool bWaitAll, const CYieldDeadline &deadline );
	[[nodiscard]] bool ServiceOneSharedJob();
	// Posts message to worker without waiting for reply and wakes it.
	void CallWorkerAsync( CJobThread *pThread, unsigned message, CFunctor *pFunctor = nullptr );

	//-----------------------------------------------------
	// Thread functions
	//-----------------------------------------------------
	int Run();

private:
	friend class CJobThread;

	// Declared before queues which point to it.
	CWorkerWake				m_WorkerWake;
	CJobQueue				m_SharedQueue;
	CInterlockedInt			m_nIdleThreads;
	CUtlVector<CJobThread *> m_Threads;
	CUtlVector<CThreadEvent *>		m_IdleEvents;

	CThreadMutex			m_SuspendMutex;
	int						m_nSuspend{ 0 };
	CInterlockedInt			m_nJobs;

	// Some jobs should only be executed on the threadpool thread(s). Ie: the rendering thread has the GL context
	//	and the main thread coming in and "helping" with jobs breaks that pretty nicely. This flag states that
	//	only the threadpool threads should execute these jobs.
	bool					m_bExecOnThreadPoolThreadsOnly{ 0 };
};

//-----------------------------------------------------------------------------

JOB_INTERFACE IThreadPool *CreateThreadPool()
{
	return new CThreadPool;
}

JOB_INTERFACE void DestroyThreadPool( IThreadPool *pPool )
{
	delete pPool;
}

//-----------------------------------------------------------------------------

class CGlobalThreadPool : public CThreadPool
{
public:
	bool Start( const ThreadPoolStartParams_t &startParamsIn ) override
	{
		int nThreads = ( CommandLine()->ParmValue( "-threads", -1 ) - 1 );
		ThreadPoolStartParams_t startParams = startParamsIn;

		if ( nThreads >= 0 )
		{
			startParams.nThreads = nThreads;
		}
		else
		{
			// Cap the GlobPool threads at 4.
			startParams.nThreadsMax = 4;
		}
		return CThreadPool::Start( startParams, "GlobJob" );
	}

	bool OnFinalRelease() override
	{
		AssertMsg( 0, "Releasing global thread pool object!" );
		return false;
	}
};

//-----------------------------------------------------------------------------
// dimhotepus: Fix aligned alloc.
class CJobThread final : public CAlignedNewDelete<16, CWorkerThread>
{
public:
	CJobThread( CThreadPool *pOwner, intp iThread ) : 
		m_SharedQueue( pOwner->m_SharedQueue ),
		m_pOwner( pOwner ),
		m_iThread( iThread )
	{
		m_DirectQueue.SetWake( &pOwner->m_WorkerWake, true );
	}

	// Called by pool right after posting a message via CallWorker().
	void SignalCall()
	{
		m_bCallPending.store( true, std::memory_order_release );
	}

	CThreadEvent &GetIdleEvent()
	{
		return m_IdleEvent;
	}

	[[nodiscard]] bool IsIdle() const
	{
		return m_bIdle.load( std::memory_order_acquire );
	}

	CJobQueue &AccessDirectQueue()
	{ 
		return m_DirectQueue;
	}

private:
	// dimhotepus: Message check is an atomic flag. PeekCall() is a kernel wait call on
	// Windows, and worker checked it after every job.
	[[nodiscard]] bool HasCall() const
	{
		return m_bCallPending.load( std::memory_order_acquire );
	}

	[[nodiscard]] bool TakeCall()
	{
		// Pool serializes messages and waits for reply before next one, so clearing here,
		// before Reply(), can't drop a newer message.
		return m_bCallPending.exchange( false, std::memory_order_acq_rel );
	}

	unsigned Wait()
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_IDLE, "%s", __FUNCTION__ );

		// dimhotepus: Block until there is a message or a job, no polling, no kernel events.
		m_pOwner->m_WorkerWake.WaitUntil( [this]()
		{
			return HasCall() || m_DirectQueue.Count() > 0 || m_SharedQueue.Count() > 0;
		} );

		return WAIT_OBJECT_0;
	}

	int Run() override
	{


		// Wait for either a call from the master thread, or an item in the queue...
		unsigned waitResult;
		bool	 bExit = false;

		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

		++m_pOwner->m_nIdleThreads;
		m_bIdle.store( true, std::memory_order_release );
		// Startup handshake for CThreadPool::Start(), set once.
		m_IdleEvent.Set();
		while (!bExit && ( ( waitResult = Wait() ) != WAIT_FAILED ) )
		{
			if ( TakeCall() )
			{
				Assert( PeekCall() );
				CFunctor *pFunctor = nullptr;
				tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s PeekCall():%d", __FUNCTION__, GetCallParam() );

				switch ( GetCallParam( &pFunctor ) )
				{
				case TPM_EXIT:
					Reply( true );
					bExit = true;
					break;

				case TPM_SUSPEND:
					Reply( true );
					SuspendCooperative();
					break;

				case TPM_RUNFUNCTOR:
					if( pFunctor )
					{
						( *pFunctor )();
						Reply( true );
					}
					else
					{
						Assert( pFunctor );
						Reply( false );
					}
					break;

				default:
					AssertMsg( 0, "Unknown call to thread" );
					Reply( false );
					break;
				}
			}
			else
			{
				tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s !PeekCall()", __FUNCTION__ );

				CJob *pJob;
				bool bTookJob = false;
				do
				{
					if ( !m_DirectQueue.Pop( &pJob) )
					{
						if ( !m_SharedQueue.Pop( &pJob ) )
						{
							// Nothing to process, return to wait state
							break;
						}
					}
					if ( !bTookJob )
					{
						// dimhotepus: Atomic flag instead of m_IdleEvent Reset()/Set(), which were
						// two kernel calls per job when workers drain queue job by job.
						m_bIdle.store( false, std::memory_order_relaxed );
						--m_pOwner->m_nIdleThreads;
						bTookJob = true;
					}
					ServiceJobAndRelease( pJob, m_iThread );
					--m_pOwner->m_nJobs;
				} while ( !HasCall() );

				if ( bTookJob )
				{
					++m_pOwner->m_nIdleThreads;
					m_bIdle.store( true, std::memory_order_release );
				}
			}
		}
		--m_pOwner->m_nIdleThreads;
		m_bIdle.store( false, std::memory_order_release );
		return 0;
	}

	CJobQueue			m_DirectQueue;
	CJobQueue &			m_SharedQueue;
	std::atomic_bool	m_bCallPending{ false };
	CThreadPool *		m_pOwner;
	CThreadManualEvent	m_IdleEvent;
	std::atomic_bool	m_bIdle{ false };
	intp				m_iThread;
};

//-----------------------------------------------------------------------------

static CGlobalThreadPool g_ThreadPool;
IThreadPool *g_pThreadPool = &g_ThreadPool;

//-----------------------------------------------------------------------------
//
// CThreadPool
//
//-----------------------------------------------------------------------------

CThreadPool::CThreadPool() :
	m_nIdleThreads( 0 ),
	m_nJobs( 0 )
{
	m_SharedQueue.SetWake( &m_WorkerWake, false );
}

//---------------------------------------------------------

void CThreadPool::CallWorkerAsync( CJobThread *pThread, unsigned message, CFunctor *pFunctor )
{
	// Timeout 0: post and return, caller waits for reply. Waking after the post (not before)
	// guarantees worker sees the message when it re-checks.
	pThread->CallWorker( message, 0, false, pFunctor );
	pThread->SignalCall();
	m_WorkerWake.NotifyAll();
}

//---------------------------------------------------------

CThreadPool::~CThreadPool()
{
	Stop();
}

//---------------------------------------------------------
// 
//---------------------------------------------------------
intp CThreadPool::NumThreads() const
{
	return m_Threads.Count();
}

//---------------------------------------------------------
// 
//---------------------------------------------------------
intp CThreadPool::NumIdleThreads() const
{
	return m_nIdleThreads;
}

void CThreadPool::ExecuteHighPriorityFunctor( CFunctor *pFunctor )
{
	// dimhotepus: Serialize with SuspendExecution/ResumeExecution/Stop. All of them talk to
	// workers through the single CallWorker() slot, so interleaving two callers could
	// overwrite each other's message before the worker reads it.
	AUTO_LOCK( m_SuspendMutex );

	// dimhotepus: Suspended workers cannot reply, and the caller holds the suspension
	// (e.g. functor issued from a job run by ExecuteToPriority), so waiting would deadlock.
	if ( m_nSuspend > 0 )
	{
		AssertMsg( false, "ExecuteHighPriorityFunctor called while thread pool is suspended" );
		Warning( "ExecuteHighPriorityFunctor ignored: thread pool is suspended.\n" );
		return;
	}

	for ( auto &&t : m_Threads )
	{
		CallWorkerAsync( t, TPM_RUNFUNCTOR, pFunctor );
	}

	for ( auto &&t : m_Threads )
	{
		t->WaitForReply();
	}
}

//---------------------------------------------------------
// Pause/resume processing jobs
//---------------------------------------------------------
int CThreadPool::SuspendExecution()
{
	AUTO_LOCK( m_SuspendMutex );

	// If not already suspended
	if ( m_nSuspend == 0 )
	{
		// Make sure state is correct
		for ( auto &&t : m_Threads )
		{
			CallWorkerAsync( t, TPM_SUSPEND );
		}

		for ( auto &&t : m_Threads )
		{
			t->WaitForReply();
		}

		// Because worker must signal before suspending, we could reach
		// here with the thread not actually suspended
		for ( auto &&t : m_Threads )
		{
			t->BWaitForThreadSuspendCooperative();
		}
	}

	return m_nSuspend++;
}

//---------------------------------------------------------

int CThreadPool::ResumeExecution()
{
	AUTO_LOCK( m_SuspendMutex );
	AssertMsg( m_nSuspend >= 1, "Attempted resume when not suspended");
	int result = m_nSuspend--;
	if ( m_nSuspend == 0 )
	{
		for ( auto &&t : m_Threads )
		{
			t->ResumeCooperative();
		}
	}
	return result;
}

//---------------------------------------------------------

void CThreadPool::WaitForIdle( bool bAll )
{
	// dimhotepus: Idle state is an atomic flag now (idle events were toggled per job).
	// Rarely used, so polling is fine. Same 60s limit as before.
	const double flEnd = Plat_FloatTime() + 60.0;
	for (;;)
	{
		intp nIdle = 0;
		for ( auto &&t : m_Threads )
		{
			if ( t->IsIdle() )
				++nIdle;
		}

		if ( bAll ? nIdle == m_Threads.Count() : nIdle > 0 || m_Threads.Count() == 0 )
			return;
		if ( Plat_FloatTime() >= flEnd )
			return;

		ThreadSleep( 1 );
	}
}

//---------------------------------------------------------

// dimhotepus: Max events per single OS wait (WaitForMultipleObjects is limited to
// MAXIMUM_WAIT_OBJECTS == 64). Larger sets are waited on in chunks. Intentionally not
// derived from MAX_THREADS: it is an OS limit and must not grow with thread count.
static constexpr int kMaxEventsPerWait = 62;

class CThreadPool::CYieldDeadline
{
public:
	explicit CYieldDeadline( unsigned timeout )
		: m_bInfinite( timeout == TT_INFINITE ),
		  m_flEnd( m_bInfinite ? 0.0 : Plat_FloatTime() + timeout / 1000.0 )
	{
	}

	[[nodiscard]] bool Expired() const
	{
		return !m_bInfinite && Plat_FloatTime() >= m_flEnd;
	}

	// Milliseconds to block in OS wait.
	[[nodiscard]] unsigned Remaining() const
	{
		if ( m_bInfinite )
			return TT_INFINITE;

		const double flLeftMs = ( m_flEnd - Plat_FloatTime() ) * 1000.0;
		if ( flLeftMs <= 0.0 )
			return 0;

		constexpr double kMaxFiniteMs = static_cast<double>( TT_INFINITE - 1 );
		return flLeftMs >= kMaxFiniteMs
			? static_cast<unsigned>( TT_INFINITE - 1 )
			// Round up so we never spin with a 0ms wait before the deadline.
			: static_cast<unsigned>( flLeftMs ) + 1;
	}

private:
	const bool m_bInfinite;
	const double m_flEnd;
};

//---------------------------------------------------------

bool CThreadPool::ServiceOneSharedJob()
{
	CJob *pJob;
	if ( !m_bExecOnThreadPoolThreadsOnly && m_SharedQueue.Pop( &pJob ) )
	{
		ServiceJobAndRelease( pJob );
		--m_nJobs;
		return true;
	}
	return false;
}

//---------------------------------------------------------

unsigned CThreadPool::YieldWaitImpl( CThreadEvent **pEvents, int nEvents, bool bWaitAll, const CYieldDeadline &deadline )
{
	Assert( nEvents > 0 && nEvents <= kMaxEventsPerWait );

	// Always wait for zero milliseconds initially, to let us process jobs on this thread.
	unsigned waitMs = 0;
	for (;;)
	{
		const unsigned result = ThreadWaitForEvents( nEvents, pEvents, bWaitAll, waitMs );
		if ( result != TW_TIMEOUT )
			return result;

		// dimhotepus: Honor caller timeout. It was silently replaced by an infinite wait,
		// so CJob::WaitForFinish( 10 ) could block forever.
		if ( deadline.Expired() )
			return TW_TIMEOUT;

		if ( ServiceOneSharedJob() )
		{
			waitMs = 0;
		}
		else
		{
			// Since there are no jobs for the main thread block until the events signal.
			// The only disadvantage to this is that if a job thread creates a new job
			// then the main thread will not be available to pick it up, but if that
			// is a problem you can just create more worker threads. Debugging test runs
			// of TF2 suggests that jobs are only ever added from the main thread which
			// means that there is no disadvantage.
			// Waiting on the events instead of busy spinning has multiple advantages.
			// It avoids wasting CPU time/electricity, it makes it more obvious in profiles
			// when the main thread is idle versus busy, and it allows ready thread analysis
			// in xperf to find out what woke up a waiting thread.
			// It also avoids unnecessary CPU starvation -- seen on customer traces of TF2.
			waitMs = deadline.Remaining();
		}
	}
}

//---------------------------------------------------------

unsigned CThreadPool::YieldWait( CThreadEvent **pEvents, int nEvents, bool bWaitAll, unsigned timeout )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_IDLE, "%s(%u) SPINNING %t", __FUNCTION__, timeout, tmSendCallStack( TELEMETRY_LEVEL0, 0 ) );

	if ( nEvents <= 0 || !pEvents )
	{
		// Waiting for all of nothing is trivially satisfied, waiting for any of nothing is not.
		return bWaitAll ? WAIT_OBJECT_0 : TW_FAILED;
	}

	const CYieldDeadline deadline( timeout );

	if ( nEvents <= kMaxEventsPerWait )
	{
		return YieldWaitImpl( pEvents, nEvents, bWaitAll, deadline );
	}

	// dimhotepus: Large sets. Previously > 62 jobs failed immediately with TW_FAILED, and
	// callers like CJobSet::WaitForFinish ignored that and released still running jobs.
	if ( bWaitAll )
	{
		for ( int iBase = 0; iBase < nEvents; iBase += kMaxEventsPerWait )
		{
			const int nChunk = (std::min)( kMaxEventsPerWait, nEvents - iBase );
			const unsigned result = YieldWaitImpl( pEvents + iBase, nChunk, true, deadline );
			if ( result == TW_TIMEOUT || result == TW_FAILED )
				return result;
		}
		return WAIT_OBJECT_0;
	}

	// Wait for any of a large set: poll chunks, help with jobs in between.
	for (;;)
	{
		for ( int iBase = 0; iBase < nEvents; iBase += kMaxEventsPerWait )
		{
			const int nChunk = (std::min)( kMaxEventsPerWait, nEvents - iBase );
			const unsigned result = ThreadWaitForEvents( nChunk, pEvents + iBase, false, 0 );
			if ( result == TW_FAILED )
				return result;
			if ( result != TW_TIMEOUT )
				return result + static_cast<unsigned>( iBase );
		}

		if ( deadline.Expired() )
			return TW_TIMEOUT;

		if ( !ServiceOneSharedJob() )
		{
			ThreadSleep( 1 );
		}
	}
}

//---------------------------------------------------------

unsigned CThreadPool::YieldWait( CJob **ppJobs, int nJobs, bool bWaitAll, unsigned timeout )
{
	if ( nJobs <= 0 || !ppJobs )
	{
		return YieldWait( static_cast<CThreadEvent **>( nullptr ), 0, bWaitAll, timeout );
	}

	// Inline storage covers MAX_THREADS jobs (largest CJobSet), larger arrays spill to heap.
	// dimhotepus: Don't wait on (and so don't create completion events for) finished jobs.
	// Completion events are created lazily on first access, which costs a syscall.
	if ( !bWaitAll )
	{
		for ( int i = 0; i < nJobs; i++ )
		{
			if ( ppJobs[i]->IsFinished() )
				return WAIT_OBJECT_0 + static_cast<unsigned>( i );
		}
	}

	CUtlVectorFixedGrowable<CThreadEvent *, MAX_THREADS> handles;
	handles.EnsureCapacity( nJobs );

	for ( int i = 0; i < nJobs; i++ )
	{
		// Wait-any must keep indices, so all jobs are included there.
		if ( !bWaitAll || !ppJobs[i]->IsFinished() )
			handles.AddToTail( ppJobs[i]->AccessEvent() );
	}

	if ( handles.Count() == 0 )
		return WAIT_OBJECT_0;

	return YieldWait( handles.Base(), static_cast<int>( handles.Count() ), bWaitAll, timeout );
}

//---------------------------------------------------------

void CThreadPool::Yield( unsigned timeout )
{
	// @MULTICORE (toml 10/24/2006): not implemented
	ThreadSleep( timeout );
}

//---------------------------------------------------------
// Add a job to the queue
//---------------------------------------------------------

void CThreadPool::AddJob( CJob *pJob )
{
	if ( !pJob )
	{
		return;
	}

	if ( pJob->m_ThreadPoolData != JOB_NO_DATA )
	{
		Warning( "Cannot add a thread job already committed to another thread pool\n" );
		return;
	}

	// dimhotepus: Detect job still pending in other pool. m_ThreadPoolData is never assigned,
	// so check above never fires.
	if ( pJob->m_pThreadPool && pJob->m_pThreadPool != this && pJob->GetStatus() == JOB_STATUS_PENDING )
	{
		Warning( "Cannot add a thread job '%s' still pending in another thread pool\n", pJob->Describe() );
		return;
	}

	if ( m_Threads.Count() == 0 )
	{
		// So only threadpool jobs are supposed to execute the jobs, but there are no threadpool threads?
		Assert( !m_bExecOnThreadPoolThreadsOnly );

		pJob->Execute();
		return;
	}

	const unsigned char flags = pJob->GetFlags();

	// dimhotepus: JF_SERIAL jobs must always be queued. Running one inline (or bumping
	// its priority) while older serial jobs are still queued breaks serial ordering.
	if ( !m_bExecOnThreadPoolThreadsOnly && ( ( flags & ( JF_IO | JF_QUEUE | JF_SERIAL ) ) == 0 ) /* @TBD && !m_queue.Count() */ )
	{
		if ( !NumIdleThreads() )
		{
			pJob->Execute();
			return;
		}
		pJob->SetPriority( JP_HIGH );
	}


	if ( !pJob->CanExecute() )
	{
		// Already handled
		ExecuteOnce( Warning( "Attempted to add job to job queue that has already been completed\n" ) );
		return;
	}

	pJob->m_pThreadPool = this;
	pJob->m_status = JOB_STATUS_PENDING;
	// dimhotepus: Count job before it becomes visible to workers. Worker could pop, run
	// and decrement first, making unsigned GetJobCount() transiently return ~4 billion.
	++m_nJobs;
	InsertJobInQueue( pJob );
}

//---------------------------------------------------------
//
//---------------------------------------------------------

void CThreadPool::InsertJobInQueue( CJob *pJob )
{
	CJobQueue *pQueue;

	if ( !( pJob->GetFlags() & JF_SERIAL ) )
	{
		int iThread = pJob->GetServiceThread();
		if ( iThread == -1 || !m_Threads.IsValidIndex( iThread ) )
		{
			pQueue = &m_SharedQueue;
		}
		else
		{
			pQueue = &(m_Threads[iThread]->AccessDirectQueue());
		}
	}
	else
	{
		pQueue = &(m_Threads[0]->AccessDirectQueue());
	}

	m_nJobs -= pQueue->Push( pJob );
}

//---------------------------------------------------------
// Add an function object to the queue (master thread)
//---------------------------------------------------------

// dimhotepus: flags unsigned -> unsigned char
void CThreadPool::AddFunctorInternal( CFunctor *pFunctor, CJob **ppJob, const char *pszDescription, unsigned char flags )
{
	// Note: assumes caller has handled refcount
	CJob *pJob = new CFunctorJob( pFunctor, pszDescription );

	pJob->SetFlags( flags );

	AddJob( pJob );

	if ( ppJob )
	{
		*ppJob = pJob;
	}
	else
	{
		pJob->Release();
	}
}

//---------------------------------------------------------
// Remove a job from the queue
//---------------------------------------------------------

void CThreadPool::ChangePriority( CJob *pJob, JobPriority_t priority )
{
	if ( !pJob )
	{
		return;
	}

	// Right now, only support upping the priority
	if ( pJob->GetPriority() < priority )
	{
		pJob->SetPriority( priority );

		// dimhotepus: Only re-queue jobs that are actually pending in *this* pool's shared
		// queue. Re-pushing finished/foreign jobs, or serial/thread-bound jobs (which live in
		// a worker's direct queue and must keep their order) is wrong.
		const bool bInSharedQueue = pJob->m_pThreadPool == this &&
			pJob->GetStatus() == JOB_STATUS_PENDING &&
			!( pJob->GetFlags() & JF_SERIAL ) &&
			!m_Threads.IsValidIndex( pJob->GetServiceThread() );

		if ( bInSharedQueue )
		{
			// The job is now in the queue twice (the stale entry is skipped when popped since
			// it's finished by then). Count the extra entry, as every popped entry is uncounted.
			++m_nJobs;
			m_nJobs -= m_SharedQueue.Push( pJob );
		}
	}
	else
	{
		ExecuteOnce( if ( pJob->GetPriority() != priority ) DevMsg( "CThreadPool::RemoveJob not implemented right now" ) );
	}

}

//---------------------------------------------------------
// Execute to a specified priority
//---------------------------------------------------------

int CThreadPool::ExecuteToPriority( JobPriority_t iToPriority, JobFilter_t pfnFilter )
{
	SuspendExecution();
	RunCodeAtScopeExit(ResumeExecution());

	int nExecuted = 0;
	const int nJobsTotal = static_cast<int>( GetJobCount() );
	CUtlVector<CJob *> jobsToPutBack;

	const auto drainQueue = [&]( CJobQueue &queue, JobPriority_t priority )
	{
		CJob *pJob;
		// dimhotepus: Drain only jobs present on entry. Other threads may keep adding jobs
		// while we run them one by one, so draining until empty could never return.
		// Also check Pop() result. Workers are suspended, but other threads can still
		// pop (YieldWait), so Count() > 0 does not guarantee Pop() succeeds (was nullptr deref).
		int nToDrain = queue.Count( priority );
		while ( nToDrain-- > 0 && queue.Pop( &pJob ) )
		{
			if ( pfnFilter && !(*pfnFilter)( pJob ) )
			{
				if ( pJob->CanExecute() )
				{
					jobsToPutBack.EnsureCapacity( nJobsTotal );
					jobsToPutBack.AddToTail( pJob );
				}
				else
				{
					--m_nJobs;
					pJob->Release(); // an already serviced job in queue, may as well ditch it (as in, main thread probably force executed)
				}
				continue;
			}

			ServiceJobAndRelease( pJob );
			--m_nJobs;
			nExecuted++;
		}
	};

	for ( int iCurPriority = JP_HIGH; iCurPriority >= iToPriority; --iCurPriority )
	{
		const auto priority = static_cast<JobPriority_t>( iCurPriority );

		for ( auto &&t : m_Threads )
		{
			drainQueue( t->AccessDirectQueue(), priority );
		}

		drainQueue( m_SharedQueue, priority );
	}

	// Jobs keep their m_nJobs slot while re-queued.
	for ( auto &&j : jobsToPutBack )
	{
		InsertJobInQueue( j );
		j->Release();
	}

	return nExecuted;
}

//---------------------------------------------------------
//
//---------------------------------------------------------

int CThreadPool::AbortAll()
{
	SuspendExecution();
	RunCodeAtScopeExit(ResumeExecution());

	int iAborted = m_SharedQueue.Flush();

	for ( auto &&t : m_Threads )
	{
		iAborted += t->AccessDirectQueue().Flush();
	}

	// dimhotepus: Subtract instead of zeroing. Jobs concurrently being serviced via
	// YieldWait on other threads still decrement the counter when they finish, so zeroing
	// made the counter underflow.
	m_nJobs -= iAborted;

	return iAborted;
}

//---------------------------------------------------------
// CThreadPool thread functions
//---------------------------------------------------------

bool CThreadPool::Start( const ThreadPoolStartParams_t &startParams, const char *pszName )
{
	int nThreads = startParams.nThreads;

	m_bExecOnThreadPoolThreadsOnly = startParams.bExecOnThreadPoolThreadsOnly;

	if ( nThreads < 0 )
	{
		const CPUInformation &ci = *GetCPUInformation();
		if ( startParams.bIOThreads )
		{
			nThreads = ci.m_nLogicalProcessors;
		}
		else
		{
			// dimhotepus: Use physical CPU cores count directly instead of dances with HT and logical cores.
			// The 12th Gen Intel Core processors performance hybrid architecture combines high-performance
			// P-cores with highly efficient E-cores in one silicon chip.  So physical != logical / 2 (HT).
			// See https://www.intel.com/content/www/us/en/developer/articles/guide/12th-gen-intel-core-processor-gamedev-guide.html
			// dimhotepus: At least one thread even on 1 core CPU as we expect thread pool to work.
			nThreads = max( 1, ci.m_nPhysicalProcessors - 1 );
			// dimhotepus: Max 6 cores instead of old 4. Handle 6 good enough (+need to optimise).
			if ( nThreads > 5 )
			{
				// Current >6 processor configs don't really work so well, probably due to cache issues? (toml 7/12/2007)
				DevMsg( "Defaulting to limit of 5 worker threads, use -threads on command line if want more\n" );
				nThreads = 5;
			}
		}

		if ( ( startParams.nThreadsMax >= 0 ) && ( nThreads > startParams.nThreadsMax ) )
		{
			nThreads = startParams.nThreadsMax;
		}
	}

	if ( nThreads <= 0 )
	{
		// dimhotepus: Correctly report start failure.
		return false;
	}

	// dimhotepus: Bound thread count. Affinity table has TP_MAX_POOL_THREADS entries
	// and CJob stores servicing thread index in a char.
	if ( nThreads > TP_MAX_POOL_THREADS )
	{
		Warning( "Thread pool '%s': clamping %d requested threads to %d.\n",
			pszName ? pszName : "", nThreads, TP_MAX_POOL_THREADS );
		nThreads = TP_MAX_POOL_THREADS;
	}

	AssertMsg( m_Threads.Count() == 0, "Thread pool started twice without Stop()" );

	int nStackSize = startParams.nStackSize;

	if ( nStackSize < 0 )
	{
		if ( startParams.bIOThreads )
		{
			nStackSize = IO_STACKSIZE;
		}
		else
		{
			nStackSize = COMPUTATION_STACKSIZE;
		}
	}

	int priority = startParams.iThreadPriority;

	if ( priority == SHRT_MIN )
	{
		if ( startParams.bIOThreads )
		{
			// dimhotepus: Highest -> normal. No sense in highest priority for IO threads.
			priority = THREAD_PRIORITY_NORMAL;
		}
		else
		{
			// dimhotepus: Current -> above normal. Compute threads above normal priority for compute threads.
			priority = THREAD_PRIORITY_ABOVE_NORMAL;
		}
	}

	bool bDistribute;
	if ( startParams.fDistribute != TRS_NONE )
	{
		bDistribute = startParams.fDistribute == TRS_TRUE;
	}
	else
	{
		bDistribute = !startParams.bIOThreads;
	}

	//--------------------------------------------------------

	m_Threads.EnsureCapacity( nThreads );
	m_IdleEvents.EnsureCapacity( nThreads );

	if ( !pszName )
	{
		pszName = ( startParams.bIOThreads ) ? "IOJobX" : "CmpJobX";
	}
	while ( nThreads-- )
	{
		intp iThread = m_Threads.AddToTail();
		m_IdleEvents.AddToTail();

		auto *jobThread = new CJobThread( this, iThread );
		m_Threads[iThread] = jobThread;
		m_IdleEvents[iThread] = &jobThread->GetIdleEvent();
		jobThread->SetName( CFmtStr( "%s%zd", pszName, iThread ) );
		// dimhotepus: If thread failed to start, waiting on its idle event hangs forever.
		if ( !jobThread->Start( nStackSize ) )
		{
			Warning( "Thread pool: failed to start worker thread %s%zd.\n", pszName, iThread );

			m_Threads.Remove( iThread );
			m_IdleEvents.Remove( iThread );
			delete jobThread;
			break;
		}
		jobThread->GetIdleEvent().Wait();

#ifdef WIN32
		ThreadSetPriority( (ThreadHandle_t)jobThread->GetThreadHandle(), priority );
#endif
	}

	if ( m_Threads.Count() == 0 )
	{
		return false;
	}

	Distribute( bDistribute, startParams.bUseAffinityTable ? (int *)startParams.iAffinityTable : nullptr );

	return true;
}

//---------------------------------------------------------

void CThreadPool::Distribute( bool bDistribute, int *pAffinityTable )
{
	if ( bDistribute )
	{
		const CPUInformation &ci = *GetCPUInformation();
		// dimhotepus: Need to rethink as 12th Gen Intel Core has >2 HT per core.
		DWORD nHwThreadsPer = ci.m_nPhysicalProcessors != ci.m_nLogicalProcessors ? 2 : 1;
		if ( ci.m_nLogicalProcessors > 1 )
		{
			if ( !pAffinityTable )
			{
#if defined( IS_WINDOWS_PC )
				ThreadHandle_t hMainThread = ThreadGetCurrentHandle();
				SetThreadIdealProcessor( hMainThread, 0 );

				// dimhotepus: Ideal processor must be < MAXIMUM_PROCESSORS (processor group size),
				// otherwise SetThreadIdealProcessor fails on >64 logical CPU systems.
				const DWORD nLogicalProcs = std::min(
					static_cast<DWORD>( ci.m_nLogicalProcessors ),
					static_cast<DWORD>( MAXIMUM_PROCESSORS ) );

				DWORD iProc = 0;
				for ( auto &&t : m_Threads )
				{
					iProc += nHwThreadsPer;
					if ( iProc >= nLogicalProcs )
					{
						iProc %= nLogicalProcs;
						if ( nHwThreadsPer > 1 )
						{
							iProc = ( iProc + 1 ) % nHwThreadsPer;
						}
					}
					SetThreadIdealProcessor( t->GetThreadHandle(), iProc );
				}
#else
				// no affinity table, distribution is cycled across all available
				int iProc = 0;
				for ( intp i = 0; i < m_Threads.Count(); i++ )
				{
					iProc += nHwThreadsPer;
					if ( iProc >= ci.m_nLogicalProcessors )
					{
						iProc %= ci.m_nLogicalProcessors;
						if ( nHwThreadsPer > 1 )
						{
							iProc = ( iProc + 1 ) % nHwThreadsPer;
						}
					}
#ifdef WIN32
					ThreadSetAffinity( (ThreadHandle_t)m_Threads[i]->GetThreadHandle(), static_cast<intp>( 1 ) << iProc );
#endif
				}
#endif
			}
			else
			{
#ifdef WIN32
				size_t i = 0;
				// distribution is from affinity table
				for ( auto &&t : m_Threads )
				{
					ThreadSetAffinity( (ThreadHandle_t)t->GetThreadHandle(), pAffinityTable[i++] );
				}
#endif
			}
		}
	}
	else
	{
#ifdef WIN32
		DWORD_PTR dwProcessAffinity, dwSystemAffinity;
		if ( GetProcessAffinityMask( GetCurrentProcess(), &dwProcessAffinity, &dwSystemAffinity ) )
		{
			for ( auto &&t : m_Threads )
			{
				ThreadSetAffinity( (ThreadHandle_t)t->GetThreadHandle(), dwProcessAffinity );
			}
		}
#endif
	}
}

//---------------------------------------------------------

bool CThreadPool::Stop( [[maybe_unused]] int timeout )
{
	{
		AUTO_LOCK( m_SuspendMutex );

		// dimhotepus: Cooperatively suspended workers never see TPM_EXIT, so Stop() on a
		// suspended pool hung forever. Resume them first.
		if ( m_nSuspend > 0 )
		{
			for ( auto &&t : m_Threads )
			{
				t->ResumeCooperative();
			}
			m_nSuspend = 0;
		}

		// dimhotepus: Post exit to all workers first, then wait for replies, so they shut down
		// in parallel instead of one after another.
		CUtlVectorFixed<CJobThread *, MAX_THREADS> exiting;
		for ( auto &&t : m_Threads )
		{
			// dimhotepus: Thread may be already gone (e.g. killed at process shutdown).
			if ( t->IsAlive() )
			{
				CallWorkerAsync( t, TPM_EXIT );
				exiting.AddToTail( t );
			}
		}

		for ( auto &&t : exiting )
		{
			t->WaitForReply();
		}
	}

	for ( auto &&t : m_Threads )
	{
		while( t->IsAlive() )
		{
			ThreadSleep( 1 );
		}

		// dimhotepus: Abort jobs left in the direct queue before it is destroyed together with
		// the thread. They leaked and never signaled completion, so waiters hung forever.
		t->AccessDirectQueue().Flush();
	}

	m_SharedQueue.Flush();

	for ( auto &&t : m_Threads )
	{
		delete t;
	}

	m_nJobs = 0;
	m_nIdleThreads = 0;
	m_Threads.RemoveAll();
	m_IdleEvents.RemoveAll();

	return true;
}

//---------------------------------------------------------

CJob *CThreadPool::GetDummyJob()
{
	class CDummyJob final : public CJob
	{
	public:
		CDummyJob()
		{
			Execute();
		}

		JobStatus_t DoExecute() override { return JOB_OK; }
	};

	static CDummyJob dummyJob;

	dummyJob.AddRef();
	return &dummyJob;
}