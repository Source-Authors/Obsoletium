//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:	A utility for a discrete job-oriented worker thread.
//
//			The class CThreadPool is both the job queue, and the 
//			worker thread. Except when the main thread attempts to 
//			synchronously execute a job, most of the inter-thread locking 
//			on the queue. 
//
//			The queue threading model uses a manual reset event for optimal 
//			throughput. Adding to the queue is guarded by a semaphore that 
//			will block the inserting thread if the queue has overflown. 
//			This prevents the worker thread from being starved out even if 
//			not running at a higher priority than the master thread.
//
//			The thread function waits for jobs, services jobs, and manages 
//			communication between the worker and master threads. The nature 
//			of the work is opaque to the Executer.
//
//			CJob instances actually do the work. The base class 
//			calls virtual methods for job primitives, so derivations don't 
//			need to worry about threading models. All of the variants of 
//			job and OS can be expressed in this hierarchy. Instances of 
//			CJob are the items placed in the queue, and by 
//			overriding the job primitives they are the manner by which 
//			users of the Executer control the state of the job. 
//
//=============================================================================

#include <atomic>
#include <new>
#include <type_traits>
#include <utility>

#include "tier0/threadtools.h"
#include "tier1/refcount.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utlvector.h"
#include "tier1/functors.h"
#include "tier0/vprof_telemetry.h"

#include "vstdlib/vstdlib.h"

#ifndef JOBTHREAD_H
#define JOBTHREAD_H

#ifdef AddJob  // windows.h print function collisions
#undef AddJob
#undef GetJob
#endif

#ifdef VSTDLIB_DLL_EXPORT
#define JOB_INTERFACE	DLL_EXPORT
#define JOB_OVERLOAD	DLL_GLOBAL_EXPORT
#define JOB_CLASS		DLL_CLASS_EXPORT
#else
#define JOB_INTERFACE	DLL_IMPORT
#define JOB_OVERLOAD	DLL_GLOBAL_IMPORT
#define JOB_CLASS		DLL_CLASS_IMPORT
#endif

#if defined( _WIN32 )
#pragma once
#endif

// Max upper bound of threads to process in parallel.
constexpr inline int MAX_THREADS = 64;
// dimhotepus: Single source for pool size, CJobSet capacity and parallel helper fan-out.
// CJob stores servicing thread index in a (possibly signed) char.
static_assert( MAX_THREADS > 0 && MAX_THREADS <= 127, "MAX_THREADS must fit CJob servicing thread index" );

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

class CJob;

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
enum JobStatusEnum_t
{
	// Use negative for errors
	JOB_OK,						// operation is successful
	JOB_STATUS_PENDING,			// file is properly queued, waiting for service
	JOB_STATUS_INPROGRESS,		// file is being accessed
	JOB_STATUS_ABORTED,			// file was aborted by caller
	JOB_STATUS_UNSERVICED,		// file is not yet queued
};

using JobStatus_t = int;

enum JobFlags_t
{
	JF_IO				= ( 1 << 0 ),	// The job primarily blocks on IO or hardware
	JF_BOOST_THREAD		= ( 1 << 1 ),	// Up the thread priority to max allowed while processing task
	JF_SERIAL			= ( 1 << 2 ),	// Job cannot be executed out of order relative to other "strict" jobs
	JF_QUEUE			= ( 1 << 3 ),	// Queue it, even if not an IO job
};

enum JobPriority_t
{
	JP_LOW,
	JP_NORMAL,
	JP_HIGH
};

// dimhotepus: Same bound as MAX_THREADS, one source of truth.
constexpr inline int TP_MAX_POOL_THREADS{MAX_THREADS};
struct ThreadPoolStartParams_t
{
	ThreadPoolStartParams_t( bool bIOThreads_ = false, unsigned nThreads_ = -1, int *pAffinities = nullptr, ThreeState_t fDistribute_ = TRS_NONE, unsigned nStackSize_ = -1, int iThreadPriority_ = SHRT_MIN )
		: nThreads( nThreads_ ),
		  fDistribute( fDistribute_ ),
		  nStackSize( nStackSize_ ),
		  iThreadPriority( iThreadPriority_ ),
		  bIOThreads( bIOThreads_ )
	{
		bExecOnThreadPoolThreadsOnly = false;

		// dimhotepus: Ensure all affinities are used if smb breaks pool affinities logic.
		// Done unconditionally so table tail is never left uninitialized.
		BitwiseSet(iAffinityTable, 0xFF);

		bUseAffinityTable = ( pAffinities != nullptr ) && ( fDistribute == TRS_TRUE ) && ( nThreads != -1 );
		if ( bUseAffinityTable )
		{
			// user supplied an optional 1:1 affinity mapping to override normal distribute behavior
			nThreads = MIN( TP_MAX_POOL_THREADS, nThreads );
			for ( int i = 0; i < nThreads; i++ )
			{
				iAffinityTable[i] = pAffinities[i];
			}
		}
	}

	int				nThreads;
	int				nThreadsMax{ -1 };
	ThreeState_t	fDistribute;
	int				nStackSize;
	int				iThreadPriority;
	int				iAffinityTable[TP_MAX_POOL_THREADS];

	bool			bIOThreads : 1;
	bool			bUseAffinityTable : 1;
	bool			bExecOnThreadPoolThreadsOnly : 1;
};

//-----------------------------------------------------------------------------
//
// IThreadPool
//
//-----------------------------------------------------------------------------

using JobFilter_t = bool (*)(CJob *);

//---------------------------------------------------------
// Messages supported through the CallWorker() method
//---------------------------------------------------------
enum ThreadPoolMessages_t
{
	TPM_EXIT,		// Exit the thread
	TPM_SUSPEND,		// Suspend after next operation
	TPM_RUNFUNCTOR,	// Run functor, reply when done.
};

//---------------------------------------------------------

abstract_class IThreadPool : public IRefCounted
{
public:
	virtual ~IThreadPool() = default;

	//-----------------------------------------------------
	// Thread functions
	//-----------------------------------------------------
	virtual bool Start( const ThreadPoolStartParams_t &startParams = ThreadPoolStartParams_t() ) = 0;
	virtual bool Stop( int timeout = TT_INFINITE ) = 0;

	//-----------------------------------------------------
	// Functions for any thread
	//-----------------------------------------------------
	[[nodiscard]] virtual unsigned GetJobCount() const = 0;
	[[nodiscard]] virtual intp NumThreads() const = 0;
	[[nodiscard]] virtual intp NumIdleThreads() const = 0;

	//-----------------------------------------------------
	// Pause/resume processing jobs
	//-----------------------------------------------------
	virtual int SuspendExecution() = 0;
	virtual int ResumeExecution() = 0;

	//-----------------------------------------------------
	// Offer the current thread to the pool
	//-----------------------------------------------------
	// dimhotepus: int -> unsigned.
	virtual unsigned YieldWait( CThreadEvent **pEvents, int nEvents, bool bWaitAll = true, unsigned timeout = TT_INFINITE ) = 0;
	// dimhotepus: int -> unsigned.
	virtual unsigned YieldWait( CJob **, int nJobs, bool bWaitAll = true, unsigned timeout = TT_INFINITE ) = 0;
	virtual void Yield( unsigned timeout ) = 0;

	bool YieldWait( CThreadEvent &event, unsigned timeout = TT_INFINITE );
	bool YieldWait( CJob *, unsigned timeout = TT_INFINITE );

	//-----------------------------------------------------
	// Add a native job to the queue (master thread)
	//-----------------------------------------------------
	virtual void AddJob( CJob * ) = 0;

	//-----------------------------------------------------
	// All threads execute pFunctor asap. Thread will either wake up
	//  and execute or execute pFunctor right after completing current job and
	//  before looking for another job.
	//-----------------------------------------------------
	virtual void ExecuteHighPriorityFunctor( CFunctor *pFunctor ) = 0;

	//-----------------------------------------------------
	// Add an function object to the queue (master thread)
	//-----------------------------------------------------
	// dimhotepus: flags unsigned -> unsigned char
	virtual void AddFunctor( CFunctor *pFunctor, CJob **ppJob = nullptr, const char *pszDescription = nullptr, unsigned char flags = 0 ) { AddFunctorInternal( RetAddRef( pFunctor ), ppJob, pszDescription, flags ); }

	//-----------------------------------------------------
	// Change the priority of an active job
	//-----------------------------------------------------
	virtual void ChangePriority( CJob *p, JobPriority_t priority ) = 0;

	//-----------------------------------------------------
	// Bulk job manipulation (blocking)
	//-----------------------------------------------------
	int ExecuteAll( JobFilter_t pfnFilter = nullptr )	{ return ExecuteToPriority( JP_LOW, pfnFilter ); }
	virtual int ExecuteToPriority( JobPriority_t toPriority, JobFilter_t pfnFilter = nullptr  ) = 0;
	virtual int AbortAll() = 0;

	//-----------------------------------------------------
	virtual void Reserved1() = 0;

	//-----------------------------------------------------
	// Add an arbitrary call to the queue (master thread) 
	//
	// Avert thy eyes! Imagine rather:
	//
	// CJob *AddCall( <function>, [args1, [arg2,]...]
	// CJob *AddCall( <object>, <function>, [args1, [arg2,]...]
	// CJob *AddRefCall( <object>, <function>, [args1, [arg2,]...]
	// CJob *QueueCall( <function>, [args1, [arg2,]...]
	// CJob *QueueCall( <object>, <function>, [args1, [arg2,]...]
	//-----------------------------------------------------

	template <typename FUNCTION_RETTYPE, typename... FuncArgs, typename... Args >
	auto AddCall( FUNCTION_RETTYPE( *pfnProxied )( FuncArgs... ), Args&&... args ) ->
		std::enable_if_t<
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				Args...
			>,
			CJob*
		>
	{
		// dimhotepus: Always go through AddJob(). It runs the job inline on the caller when no
		// thread is idle (same as the old header fast path), but unlike the header it knows
		// whether the pool only allows execution on its own threads.
		CJob* pJob;
		AddFunctorInternal( CreateFunctor( pfnProxied, std::forward<Args>( args )... ), &pJob );

		return pJob;
	}

	//-------------------------------------

	template <
		typename OBJECT_TYPE,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs, typename... Args>
	auto AddCall(
		OBJECT_TYPE* pObject,
		FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FuncArgs... ),
		Args&&... args ) ->
		std::enable_if_t<
			std::is_base_of_v<FUNCTION_CLASS, OBJECT_TYPE>
			&&
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				FUNCTION_CLASS&,
				Args...
			>,
			CJob*
		>
	{
		// dimhotepus: Always go through AddJob(). It runs the job inline on the caller when no
		// thread is idle (same as the old header fast path), but unlike the header it knows
		// whether the pool only allows execution on its own threads.
		CJob* pJob;
		AddFunctorInternal( CreateFunctor( pObject, pfnProxied, std::forward<Args>( args )... ), &pJob );
		return pJob;
	}

	//-------------------------------------

	template <
		typename OBJECT_TYPE,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	auto AddCall(
		OBJECT_TYPE* pObject,
		FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FuncArgs... ) const,
		Args&&... args ) ->
		std::enable_if_t<
			std::is_base_of_v<FUNCTION_CLASS, OBJECT_TYPE>
			&&
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				const FUNCTION_CLASS&,
				Args...
			>,
			CJob*
		>
	{
		// dimhotepus: Always go through AddJob(). It runs the job inline on the caller when no
		// thread is idle (same as the old header fast path), but unlike the header it knows
		// whether the pool only allows execution on its own threads.
		CJob* pJob;
		AddFunctorInternal( CreateFunctor( pObject, pfnProxied, std::forward<Args>( args )... ), &pJob );
		return pJob;
	}

	//-------------------------------------

	template <
		typename OBJECT_TYPE,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	auto AddRefCall(
		OBJECT_TYPE* pObject,
		FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FuncArgs... ),
		Args&&... args ) ->
		std::enable_if_t<
			std::is_base_of_v<FUNCTION_CLASS, OBJECT_TYPE>
			&&
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				FUNCTION_CLASS&,
				Args...
			>,
			CJob*
		>
	{
		// dimhotepus: Always go through AddJob(). It runs the job inline on the caller when no
		// thread is idle (same as the old header fast path), but unlike the header it knows
		// whether the pool only allows execution on its own threads.
		CJob* pJob;
		AddFunctorInternal( CreateRefCountingFunctor( pObject, pfnProxied, std::forward<Args>( args )... ), &pJob );
		return pJob;
	}

	//-------------------------------------

	template <
		typename OBJECT_TYPE,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	auto AddRefCall(
		OBJECT_TYPE* pObject,
		FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FuncArgs... ) const,
		Args&&... args ) ->
		std::enable_if_t<
			std::is_base_of_v<FUNCTION_CLASS, OBJECT_TYPE>
			&&
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				const FUNCTION_CLASS&,
				Args...
			>,
			CJob*
		>
	{
		// dimhotepus: Always go through AddJob(). It runs the job inline on the caller when no
		// thread is idle (same as the old header fast path), but unlike the header it knows
		// whether the pool only allows execution on its own threads.
		CJob* pJob;
		AddFunctorInternal( CreateRefCountingFunctor( pObject, pfnProxied, std::forward<Args>( args )... ), &pJob );
		return pJob;
	}

	//-----------------------------------------------------------------------------

	template <typename FUNCTION_RETTYPE, typename... FuncArgs, typename... Args>
	auto QueueCall( FUNCTION_RETTYPE( *pfnProxied )( FuncArgs... ), Args&&... args ) ->
		std::enable_if_t<
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				Args...
			>,
			CJob*
		>
	{
		CJob* pJob;
		AddFunctorInternal( CreateFunctor( pfnProxied, std::forward<Args>( args )... ), &pJob, nullptr, JF_QUEUE );
		return pJob;
	}

	//-------------------------------------

	template <
		typename OBJECT_TYPE,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	auto QueueCall(
		OBJECT_TYPE* pObject,
		FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FuncArgs... ),
		Args&&... args ) ->
		std::enable_if_t<
			std::is_base_of_v<FUNCTION_CLASS, OBJECT_TYPE>
			&&
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				FUNCTION_CLASS&,
				Args...
			>,
			CJob*
		>
	{
		CJob *pJob;
		AddFunctorInternal( CreateFunctor( pObject, pfnProxied, std::forward<Args>( args )... ), &pJob, nullptr, JF_QUEUE );
		return pJob;
	}

	//-------------------------------------

	template <
		typename OBJECT_TYPE,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	auto QueueCall(
		OBJECT_TYPE* pObject,
		FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FuncArgs... ) const,
		Args&&... args ) ->
		std::enable_if_t<
			std::is_base_of_v<FUNCTION_CLASS, OBJECT_TYPE>
			&&
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				const FUNCTION_CLASS&,
				Args...
			>,
			CJob*
		>
	{
		CJob *pJob;
		AddFunctorInternal( CreateFunctor( pObject, pfnProxied, std::forward<Args>( args )... ), &pJob, nullptr, JF_QUEUE );
		return pJob;
	}

	//-------------------------------------

	template <
		typename OBJECT_TYPE,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	auto QueueRefCall(
		OBJECT_TYPE* pObject,
		FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FuncArgs... ),
		Args&&... args ) ->
		std::enable_if_t<
			std::is_base_of_v<FUNCTION_CLASS, OBJECT_TYPE>
			&&
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				FUNCTION_CLASS&,
				Args...
			>,
			CJob*
		>
	{
		CJob* pJob;
		AddFunctorInternal( CreateRefCountingFunctor( pObject, pfnProxied, std::forward<Args>( args )... ), &pJob, nullptr, JF_QUEUE );
		return pJob;
	}

	//-------------------------------------

	template <
		typename OBJECT_TYPE,
		typename FUNCTION_CLASS,
		typename FUNCTION_RETTYPE,
		typename... FuncArgs,
		typename... Args>
	auto QueueRefCall(
		OBJECT_TYPE* pObject,
		FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FuncArgs... ) const,
		Args&&... args ) ->
		std::enable_if_t<
			std::is_base_of_v<FUNCTION_CLASS, OBJECT_TYPE>
			&&
			std::is_invocable_r_v<
				FUNCTION_RETTYPE,
				decltype(pfnProxied),
				const FUNCTION_CLASS&,
				Args...
			>,
			CJob*
		>
	{
		CJob* pJob;
		AddFunctorInternal( CreateRefCountingFunctor( pObject, pfnProxied, std::forward<Args>( args )... ), &pJob, nullptr, JF_QUEUE );
		return pJob;
	}

private:
	// dimhotepus: flags unsigned -> unsigned char
	virtual void AddFunctorInternal( CFunctor *, CJob ** = nullptr, const char *pszDescription = nullptr, unsigned char flags = 0 ) = 0;

	//-----------------------------------------------------
	// Services for internal use by job instances
	//-----------------------------------------------------
	friend class CJob;

	virtual CJob *GetDummyJob() = 0;

public:
	virtual void Distribute( bool bDistribute = true, int *pAffinityTable = nullptr ) = 0;

	virtual bool Start( const ThreadPoolStartParams_t &startParams, const char *pszNameOverride ) = 0;
};

//-----------------------------------------------------------------------------

JOB_INTERFACE IThreadPool *CreateThreadPool();
JOB_INTERFACE void DestroyThreadPool( IThreadPool *pPool );

//-----------------------------------------------------------------------------

JOB_INTERFACE IThreadPool *g_pThreadPool;

//-----------------------------------------------------------------------------
// Class to combine the metadata for an operation and the ability to perform
// the operation. Meant for inheritance. All functions inline, defers to executor
//-----------------------------------------------------------------------------
DECLARE_POINTER_HANDLE( ThreadPoolData_t );
const inline ThreadPoolData_t JOB_NO_DATA{reinterpret_cast<ThreadPoolData_t>(-1)};

class CJob : public CRefCounted1<IRefCounted, CRefCountServiceMT>
{
public:
	CJob( JobPriority_t priority = JP_NORMAL )
	  : m_priority( priority ),
		m_ThreadPoolData( JOB_NO_DATA )
	{
		m_szDescription[ 0 ] = 0;
	}

	virtual ~CJob()
	{
		// dimhotepus: Accessor holds a reference while creating, so creation can't be in progress here.
		Assert( m_eventState.load( std::memory_order_relaxed ) != EVENT_CREATING );
		if ( m_eventState.load( std::memory_order_acquire ) == EVENT_READY )
		{
			CompleteEvent().~CThreadEvent();
		}
	}

	//-----------------------------------------------------
	// Priority (not thread safe)
	//-----------------------------------------------------
	void SetPriority( JobPriority_t priority )		{ m_priority = priority; }
	JobPriority_t GetPriority() const				{ return m_priority; }

	//-----------------------------------------------------

	void SetFlags( unsigned char flags )					{ m_flags = flags; }
	unsigned char GetFlags() const						{ return m_flags; }

	//-----------------------------------------------------

	void SetServiceThread( intp iServicingThread )	{ m_iServicingThread = (char)iServicingThread; }
	int GetServiceThread() const					{ return m_iServicingThread; }
	void ClearServiceThread()						{ m_iServicingThread = -1; }

	//-----------------------------------------------------
	// Fast queries
	//-----------------------------------------------------
	// dimhotepus: Status is read without lock by other threads, so it is atomic. Read once
	// per query so concurrent transitions can't produce inconsistent answers.
	bool Executed() const							{ return ( GetStatus() == JOB_OK );	}
	bool CanExecute() const							{ const JobStatus_t s = GetStatus(); return ( s == JOB_STATUS_PENDING || s == JOB_STATUS_UNSERVICED ); }
	bool IsFinished() const							{ const JobStatus_t s = GetStatus(); return ( s != JOB_STATUS_PENDING && s != JOB_STATUS_INPROGRESS && s != JOB_STATUS_UNSERVICED ); }
	JobStatus_t GetStatus() const					{ return m_status.load( std::memory_order_acquire ); }

	/// Slam the status to a particular value.  This is named "slam" instead of "set,"
	/// to warn you that it should only be used in unusual situations.  Otherwise, the
	/// job manager really should manage the status for you, and you should not manhandle it.
	void SlamStatus(JobStatus_t s) { m_status.store( s, std::memory_order_release ); }
	
	//-----------------------------------------------------
	// Try to acquire ownership (to satisfy). If you take the lock, you must either execute or abort.
	//-----------------------------------------------------
	bool TryLock()										{ return m_mutex.TryLock(); }
	void Lock()											{ m_mutex.Lock(); }
	void Unlock()										{ m_mutex.Unlock(); }

	//-----------------------------------------------------
	// Thread event support (safe for NULL this to simplify code )
	//-----------------------------------------------------
	// dimhotepus: Yield to the pool job was added to (helps it make progress), not always global one.
	bool WaitForFinish( uint32 dwTimeout = TT_INFINITE ) { return ( !IsFinished() ) ? ( m_pThreadPool ? m_pThreadPool : g_pThreadPool )->YieldWait( this, dwTimeout ) : true; }
	bool WaitForFinishAndRelease( uint32 dwTimeout = TT_INFINITE ) { bool bResult = WaitForFinish( dwTimeout); Release(); return bResult; }
	// dimhotepus: Completion event is created on first access. Creating + closing a kernel
	// event per job (~0.9us on Windows) plus Set() on completion (~0.3us) was ~85% of a
	// job's cost, while most jobs are fire-and-forget or already finished when waited on.
	// Event created after completion starts signaled.
	CThreadEvent *AccessEvent()
	{
		if ( m_eventState.load( std::memory_order_acquire ) != EVENT_READY )
		{
			short expected = EVENT_NONE;
			if ( m_eventState.compare_exchange_strong( expected, EVENT_CREATING, std::memory_order_acq_rel ) )
			{
				::new ( static_cast<void *>( m_CompleteEventStorage ) ) CThreadEvent( true );

				// Pairs with RMW in SignalComplete(). Both are read-modify-writes of
				// m_eventState, so one is ordered after the other: either completer reads
				// EVENT_READY and sets event, or this exchange reads after completer's RMW and
				// so sees final status below. Double Set() is harmless.
				m_eventState.exchange( EVENT_READY, std::memory_order_acq_rel );
				if ( IsFinished() )
					CompleteEvent().Set();
			}
			else
			{
				// Other thread is creating it, takes about a microsecond.
				while ( m_eventState.load( std::memory_order_acquire ) != EVENT_READY )
					ThreadPause();
			}
		}
		return &CompleteEvent();
	}

	//-----------------------------------------------------
	// Perform the job
	//-----------------------------------------------------
	JobStatus_t Execute();
	JobStatus_t TryExecute();
	JobStatus_t ExecuteAndRelease()					{ JobStatus_t status = Execute(); Release(); return status;	}
	JobStatus_t TryExecuteAndRelease()				{ JobStatus_t status = TryExecute(); Release(); return status;	}

	//-----------------------------------------------------
	// Terminate the job, discard if partially or wholly fulfilled
	//-----------------------------------------------------
	JobStatus_t Abort( bool bDiscard = true );

	virtual char const *Describe() const			{ return m_szDescription[ 0 ] ? m_szDescription : "Job"; }
	virtual void SetDescription( const char *pszDescription )
	{
		if( pszDescription )
			V_strcpy_safe( m_szDescription, pszDescription );
		else
			m_szDescription[ 0 ] = '\0';
	}

private:
	//-----------------------------------------------------
	friend class CThreadPool;

	// dimhotepus: Status is read without lock by other threads, so it is atomic. Read once
	// per query so concurrent transitions can't produce inconsistent answers.
	std::atomic<JobStatus_t>	m_status{ JOB_STATUS_UNSERVICED };
	static_assert( sizeof( std::atomic<JobStatus_t> ) == sizeof( JobStatus_t ), "CJob layout must not change" );
	JobPriority_t		m_priority;
	CThreadMutex		m_mutex;
	unsigned char		m_flags{ 0 };
	char				m_iServicingThread{ -1 };
	// dimhotepus: Was unused 'short m_reserved', now lazy completion event state.
	// Same size and alignment, member offsets of CJob are unchanged.
	enum : short { EVENT_NONE = 0, EVENT_CREATING = 1, EVENT_READY = 2 };
	std::atomic<short>	m_eventState{ EVENT_NONE };
	static_assert( sizeof( std::atomic<short> ) == sizeof( short ) && alignof( std::atomic<short> ) == alignof( short ),
		"CJob layout must not change" );
	ThreadPoolData_t	m_ThreadPoolData;
	IThreadPool *		m_pThreadPool{ nullptr };
	// dimhotepus: Was 'CThreadEvent m_CompleteEvent', now raw storage of the same size and
	// alignment, constructed lazily in AccessEvent().
	alignas( CThreadEvent ) unsigned char	m_CompleteEventStorage[sizeof( CThreadEvent )];
	char				m_szDescription[ 32 ];

	CThreadEvent &CompleteEvent()
	{
		return *std::launder( reinterpret_cast<CThreadEvent *>( m_CompleteEventStorage ) );
	}

	// Called after status became final.
	void SignalComplete()
	{
		// RMW (not plain load) to read latest state, pairs with exchange in AccessEvent().
		if ( m_eventState.fetch_add( 0, std::memory_order_acq_rel ) == EVENT_READY )
			CompleteEvent().Set();
	}

	//-----------------------------------------------------
	CJob( const CJob &fromRequest ) = delete;
	void operator=(const CJob &fromRequest ) = delete;
private:
	virtual JobStatus_t DoExecute() = 0;
	virtual JobStatus_t DoAbort( [[maybe_unused]] bool bDiscard ) { return JOB_STATUS_ABORTED; }
	virtual void DoCleanup() {}
};

//-----------------------------------------------------------------------------

class CFunctorJob : public CJob
{
public:
	CFunctorJob( CFunctor *pFunctor, const char *pszDescription = nullptr )
		: m_pFunctor( pFunctor )
	{
		if ( pszDescription )
			V_strcpy_safe( m_szDescription, pszDescription );
		else
			m_szDescription[0] = '\0';
	}

	JobStatus_t DoExecute() override
	{
		(*m_pFunctor)();
		return JOB_OK;
	}

	const char *Describe() const override
	{
		return m_szDescription;
	}

	// dimhotepus: CFunctorJob shadows base description buffer, so base SetDescription()
	// wrote buffer nobody read (e.g. CParallelProcessor job names were lost).
	void SetDescription( const char *pszDescription ) override
	{
		CJob::SetDescription( pszDescription );

		if ( pszDescription )
			V_strcpy_safe( m_szDescription, pszDescription );
		else
			m_szDescription[0] = '\0';
	}

private:
	CRefPtr<CFunctor> m_pFunctor;
	// dimhotepus: 16 -> 32
	char m_szDescription[32];
};

//-----------------------------------------------------------------------------
// Utility for managing multiple jobs
//-----------------------------------------------------------------------------

class CJobSet
{
public:
	CJobSet( CJob *pJob = nullptr )
	{
		if ( pJob )
			Add( pJob );
	}

	CJobSet( CJob **ppJobs, int nJobs )
	{
		if ( ppJobs )
		{
			for ( int i = 0; i < nJobs; ++i )
			{
				if ( ppJobs[i] )
					Add( ppJobs[i] );
			}
		}
	}

	// dimhotepus: CJobSet owns one reference per job. Implicit copy assignment copied raw
	// pointers: the target's previous jobs leaked and copied jobs were released twice.
	// Copies now add a reference, moves transfer ownership.
	CJobSet( const CJobSet &other )
	{
		CopyFrom( other );
	}

	CJobSet( CJobSet &&other ) noexcept
	{
		MoveFrom( other );
	}

	CJobSet &operator=( const CJobSet &other )
	{
		if ( this != &other )
		{
			// Take new references before dropping old ones, sets may share jobs.
			CJobSet copy( other );
			ReleaseAll();
			MoveFrom( copy );
		}
		return *this;
	}

	CJobSet &operator=( CJobSet &&other ) noexcept
	{
		if ( this != &other )
		{
			ReleaseAll();
			MoveFrom( other );
		}
		return *this;
	}

	~CJobSet()
	{
		ReleaseAll();
	}

	void operator+=( CJob *pJob )
	{
		if ( pJob )
			Add( pJob );
	}

	void operator-=( CJob *pJob )
	{
		m_jobs.FindAndRemove( pJob );
	}

	void Execute( bool bRelease = true )
	{
		for ( auto *j : m_jobs )
		{
			j->Execute();
			if ( bRelease )
			{
				j->Release();
			}
		}

		if ( bRelease )
			m_jobs.RemoveAll();
	}

	void Abort( bool bRelease = true )
	{
		for ( auto *j : m_jobs )
		{
			j->Abort();
			if ( bRelease )
			{
				j->Release();
			}
		}

		if ( bRelease )
			m_jobs.RemoveAll();
	}

	void WaitForFinish( bool bRelease = true )
	{
		for ( auto *j : m_jobs )
		{
			j->WaitForFinish();
			if ( bRelease )
			{
				j->Release();
			}
		}

		if ( bRelease )
			m_jobs.RemoveAll();
	}

	void WaitForFinish( IThreadPool *pPool, bool bRelease = true )
	{
		Assert(m_jobs.Count() <= INT_MAX);
		pPool->YieldWait( m_jobs.Base(), static_cast<int>(m_jobs.Count()) );

		if ( bRelease )
		{
			for ( auto *j : m_jobs )
			{
				j->Release();
			}

			m_jobs.RemoveAll();
		}
	}

	// Max jobs set holds. Jobs above that are executed synchronously on add.
	[[nodiscard]] static constexpr intp MaxJobs() { return MAX_THREADS; }

private:
	// dimhotepus: Storage is fixed, CUtlMemoryFixed can't grow and only asserts, so adding
	// past capacity overran the inline buffer. Instead finish overflow job right away, so it
	// is still complete when set is waited on.
	void Add( CJob *pJob )
	{
		if ( m_jobs.Count() < MaxJobs() )
		{
			m_jobs.AddToTail( pJob );
			return;
		}

		AssertMsg( false, "CJobSet is full (%zd jobs), finishing job '%s' synchronously",
			static_cast<intp>( MaxJobs() ), pJob->Describe() );

		if ( pJob->GetStatus() == JOB_STATUS_UNSERVICED )
		{
			// Not queued anywhere, nobody else would ever run it.
			pJob->Execute();
		}
		else
		{
			// Queued or running: wait for its turn instead of executing it here. Executing a
			// queued job on the caller would let it jump ahead of older JF_SERIAL jobs.
			pJob->WaitForFinish();
		}
		pJob->Release();
	}

	void ReleaseAll()
	{
		for ( auto *j : m_jobs )
		{
			j->Release();
		}
		m_jobs.RemoveAll();
	}

	void CopyFrom( const CJobSet &other )
	{
		const intp nJobs = other.m_jobs.Count();
		if ( nJobs )
		{
			m_jobs.AddMultipleToTail( nJobs, other.m_jobs.Base() );
			for ( auto *j : m_jobs )
			{
				j->AddRef();
			}
		}
	}

	// Leaves 'other' empty, its references now belong to us. Element-wise so it works for
	// inline (fixed) storage too.
	void MoveFrom( CJobSet &other ) noexcept
	{
		const intp nJobs = other.m_jobs.Count();
		if ( nJobs )
		{
			m_jobs.AddMultipleToTail( nJobs, other.m_jobs.Base() );
			other.m_jobs.RemoveAll();
		}
	}

	// dimhotepus: 16 -> MAX_THREADS. Parallel helpers queue up to MAX_THREADS - 1 jobs.
	CUtlVectorFixed<CJob *, MAX_THREADS> m_jobs;
};

//-----------------------------------------------------------------------------
// Job helpers
//-----------------------------------------------------------------------------

#define ThreadExecute g_pThreadPool->QueueCall
#define ThreadExecuteRef g_pThreadPool->QueueRefCall

#define BeginExecuteParallel()	do { CJobSet jobSet
#define EndExecuteParallel()	jobSet.WaitForFinish( g_pThreadPool ); } while (0)

#define ExecuteParallel			jobSet += g_pThreadPool->QueueCall
// dimhotepus: QueueCallRef -> QueueRefCall. Macro referenced a non-existent method.
#define ExecuteRefParallel		jobSet += g_pThreadPool->QueueRefCall


//-----------------------------------------------------------------------------
// Work splitting: array split, best when cost per item is roughly equal
//-----------------------------------------------------------------------------
template <
	typename FUNCTION_RETTYPE,
	typename ITERTYPE1,
	typename ITERTYPE2,
	typename... FuncArgs,
	typename... Args>
auto IterRangeParallel(
	FUNCTION_RETTYPE( *pfnProxied )( ITERTYPE1, ITERTYPE2, FuncArgs... ),
	ITERTYPE1 from,
	ITERTYPE2 to,
	Args&& ... args ) ->
	std::enable_if_t<
		std::is_invocable_r_v<
			FUNCTION_RETTYPE,
			decltype(pfnProxied),
			ITERTYPE1,
			ITERTYPE2,
			Args...
		>
	>
{
	int nIdle = g_pThreadPool->NumIdleThreads();
	ITERTYPE1 range = to - from;
	int nThreads = MIN( nIdle + 1, range );
	if ( nThreads > MAX_THREADS )
		nThreads = MAX_THREADS;
	if ( nThreads < 2 )
		FunctorDirectCall( pfnProxied, from, to, std::forward<Args>( args )... );
	else
	{
		ITERTYPE1 nIncrement = range / nThreads;

		CJobSet jobSet;
		while ( --nThreads )
		{
			ITERTYPE2 thisTo = from + nIncrement;
			// dimhotepus: Do not std::forward in a loop, rvalue args were moved-from after 1st job.
			jobSet += g_pThreadPool->AddCall( pfnProxied, from, thisTo, args... );
			from = thisTo;
		}
		FunctorDirectCall( pfnProxied, from, to, std::forward<Args>( args )... );
		jobSet.WaitForFinish( g_pThreadPool );
	}
}

template <
	typename OBJECT_TYPE,
	typename FUNCTION_CLASS,
	typename FUNCTION_RETTYPE,
	typename ITERTYPE1,
	typename ITERTYPE2,
	typename... FuncArgs,
	typename... Args>
auto IterRangeParallel(
	OBJECT_TYPE* pObject,
	FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( ITERTYPE1, ITERTYPE2, FuncArgs... ),
	ITERTYPE1 from,
	ITERTYPE2 to,
	Args&& ... args ) ->
	std::enable_if_t<
		std::is_base_of_v<FUNCTION_CLASS, OBJECT_TYPE>
		&&
		std::is_invocable_r_v<
			FUNCTION_RETTYPE,
			decltype(pfnProxied),
			FUNCTION_CLASS&,
			ITERTYPE1,
			ITERTYPE2,
			Args...
		>
	>
{
	intp nIdle = g_pThreadPool->NumIdleThreads();
	ITERTYPE1 range = to - from;
	intp nThreads = MIN( nIdle + 1, range );
	if ( nThreads > MAX_THREADS )
		nThreads = MAX_THREADS;
	if ( nThreads < 2 )
		FunctorDirectCall( pObject, pfnProxied, from, to, std::forward<Args>( args )... );
	else
	{
		ITERTYPE1 nIncrement = range / nThreads;

		CJobSet jobSet;
		while ( --nThreads )
		{
			ITERTYPE2 thisTo = from + nIncrement;
			// dimhotepus: Do not std::forward in a loop, rvalue args were moved-from after 1st job.
			jobSet += g_pThreadPool->AddCall( pObject, pfnProxied, from, thisTo, args... );
			from = thisTo;
		}
		FunctorDirectCall( pObject, pfnProxied, from, to, std::forward<Args>( args )... );
		jobSet.WaitForFinish( g_pThreadPool );
	}
}

//-----------------------------------------------------------------------------
// Work splitting: competitive, best when cost per item varies a lot
//-----------------------------------------------------------------------------

namespace jobthread_detail
{
	// dimhotepus: Claims next batch [begin, end) from shared index, end <= limit.
	// Per-item atomic increments made all threads fight over one cache line, so loops of
	// small items ran at ~2x their ideal time. Batches shrink as work runs out (guided
	// scheduling): remaining / divisor, at least 1. Large batches early keep atomics rare,
	// small ones at the end keep threads finishing together even with uneven item costs.
	// Returns false when nothing is left.
	inline bool ClaimBatch( std::atomic<intp> &index, intp limit, intp divisor, intp &begin, intp &end )
	{
		intp cur = index.load( std::memory_order_relaxed );
		for (;;)
		{
			if ( cur >= limit )
				return false;

			const intp remaining = limit - cur;
			const intp n = divisor > 1 && remaining > divisor ? remaining / divisor : 1;

			if ( index.compare_exchange_weak( cur, cur + n, std::memory_order_relaxed, std::memory_order_relaxed ) )
			{
				begin = cur;
				end = cur + n;
				return true;
			}
		}
	}

	// Two batches per participant on average up front.
	[[nodiscard]] constexpr intp BatchDivisor( intp nParticipants )
	{
		return nParticipants > 0 ? nParticipants * 2 : 1;
	}
}

template <typename T>
class CJobItemProcessor
{
public:
	using ItemType_t = T;
	void Begin() {}
	// void Process( ItemType_t & ) {}
	void End() {}
};

template <typename T>
class CFuncJobItemProcessor : public CJobItemProcessor<T>
{
public:
	void Init(void (*pfnProcess)( T & ), void (*pfnBegin)() = nullptr, void (*pfnEnd)() = nullptr )
	{
		m_pfnProcess = pfnProcess;
		m_pfnBegin = pfnBegin;
		m_pfnEnd = pfnEnd;
	}

	void Begin()						{ if ( m_pfnBegin ) (*m_pfnBegin)(); }
	void Process( T &item )	{ (*m_pfnProcess)( item ); }
	void End()							{ if ( m_pfnEnd ) (*m_pfnEnd)(); }

protected:
	void (*m_pfnProcess)( T & );
	void (*m_pfnBegin)();
	void (*m_pfnEnd)();
};

template <typename T, class OBJECT_TYPE, class FUNCTION_CLASS = OBJECT_TYPE >
class CMemberFuncJobItemProcessor : public CJobItemProcessor<T>
{
public:
	void Init( OBJECT_TYPE *pObject, void (FUNCTION_CLASS::*pfnProcess)( T & ), void (FUNCTION_CLASS::*pfnBegin)() = nullptr, void (FUNCTION_CLASS::*pfnEnd)() = nullptr )
	{
		m_pObject = pObject;
		m_pfnProcess = pfnProcess;
		m_pfnBegin = pfnBegin;
		m_pfnEnd = pfnEnd;
	}

	void Begin()						{ if ( m_pfnBegin ) ((*m_pObject).*m_pfnBegin)(); }
	void Process( T &item )				{ ((*m_pObject).*m_pfnProcess)( item ); }
	void End()							{ if ( m_pfnEnd ) ((*m_pObject).*m_pfnEnd)(); }

protected:
	OBJECT_TYPE *m_pObject;
	void (FUNCTION_CLASS::*m_pfnProcess)( T & );
	void (FUNCTION_CLASS::*m_pfnBegin)();
	void (FUNCTION_CLASS::*m_pfnEnd)();
};

template <typename ITEM_TYPE, class ITEM_PROCESSOR_TYPE>
class CParallelProcessor
{
public:
	CParallelProcessor( const char *pszDescription )
	{
		m_pItems = nullptr;
		m_nItems = 0;
		m_nBatchDivisor = 1;
		m_nNext.store( 0, std::memory_order_relaxed );
		m_szDescription = pszDescription;
	}

	void Run( ITEM_TYPE *pItems, size_t nItems, intp nMaxParallel = PTRDIFF_MAX, IThreadPool *pThreadPool = nullptr )
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "Run %s %zd", m_szDescription, nItems );

		if ( nItems == 0 )
			return;

		if ( !pThreadPool )
			pThreadPool = g_pThreadPool;

		m_pItems = pItems;
		m_nItems = static_cast<intp>( nItems );
		m_nNext.store( 0, std::memory_order_relaxed );
		m_nBatchDivisor = 1;

		intp nJobs = nItems - 1;

		if ( nJobs > nMaxParallel )
			nJobs = nMaxParallel;

		if (! pThreadPool )									// only possible on linux
		{
			DoExecute( );
			return;
		}

		intp nThreads = pThreadPool->NumThreads();
		if ( nJobs > nThreads )
			nJobs = nThreads;

		if ( nJobs > MAX_THREADS )
			nJobs = MAX_THREADS;

		if ( nJobs > 1 )
		{
			// Set before jobs are queued, queueing publishes it to workers.
			m_nBatchDivisor = jobthread_detail::BatchDivisor( nJobs + 1 );

			CJob **jobs = stackallocT( CJob*, nJobs );
			intp i = nJobs;

			while( i-- )
			{
				jobs[i] = pThreadPool->QueueCall( this, &CParallelProcessor<ITEM_TYPE, ITEM_PROCESSOR_TYPE>::DoExecute );
				jobs[i]->SetDescription( m_szDescription );
			}

			DoExecute();

			for ( i = 0; i < nJobs; i++ )
			{
				jobs[i]->Abort(); // will either abort ones that never got a thread, or noop on ones that did
				jobs[i]->Release();
			}
		}
		else
			DoExecute();
	}

	ITEM_PROCESSOR_TYPE m_ItemProcessor;

private:
	void DoExecute()
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "DoExecute %s", m_szDescription );

		intp iBegin, iEnd;
		if ( jobthread_detail::ClaimBatch( m_nNext, m_nItems, m_nBatchDivisor, iBegin, iEnd ) )
		{
			m_ItemProcessor.Begin();

			do
			{
				for ( intp i = iBegin; i < iEnd; ++i )
					m_ItemProcessor.Process( m_pItems[i] );
			}
			while ( jobthread_detail::ClaimBatch( m_nNext, m_nItems, m_nBatchDivisor, iBegin, iEnd ) );

			m_ItemProcessor.End();
		}
	}
	ITEM_TYPE *					m_pItems;
	intp						m_nItems;
	intp						m_nBatchDivisor;
	// dimhotepus: Own cache line, it is the only member written by all threads.
	alignas( 64 ) std::atomic<intp>	m_nNext;
	// Next cache line: keep read-only member off the hot counter line.
	alignas( 64 ) const char *	m_szDescription;
};

template <typename ITEM_TYPE> 
inline void ParallelProcess( const char *pszDescription, ITEM_TYPE *pItems, size_t nItems, void (*pfnProcess)( ITEM_TYPE & ), void (*pfnBegin)() = nullptr, void (*pfnEnd)() = nullptr, intp nMaxParallel = PTRDIFF_MAX )
{
	CParallelProcessor<ITEM_TYPE, CFuncJobItemProcessor<ITEM_TYPE>> processor( pszDescription );
	processor.m_ItemProcessor.Init( pfnProcess, pfnBegin, pfnEnd );
	processor.Run( pItems, nItems, nMaxParallel );

}

template <typename ITEM_TYPE, typename OBJECT_TYPE, typename FUNCTION_CLASS > 
inline void ParallelProcess( const char *pszDescription, ITEM_TYPE *pItems, size_t nItems, OBJECT_TYPE *pObject, void (FUNCTION_CLASS::*pfnProcess)( ITEM_TYPE & ), void (FUNCTION_CLASS::*pfnBegin)() = NULL, void (FUNCTION_CLASS::*pfnEnd)() = NULL, intp nMaxParallel = PTRDIFF_MAX )
{
	CParallelProcessor<ITEM_TYPE, CMemberFuncJobItemProcessor<ITEM_TYPE, OBJECT_TYPE, FUNCTION_CLASS>> processor( pszDescription );
	processor.m_ItemProcessor.Init( pObject, pfnProcess, pfnBegin, pfnEnd );
	processor.Run( pItems, nItems, nMaxParallel );
}

// Parallel Process that lets you specify threadpool
template <typename ITEM_TYPE> 
inline void ParallelProcess( const char *pszDescription, IThreadPool *pPool, ITEM_TYPE *pItems, size_t nItems, void (*pfnProcess)( ITEM_TYPE & ), void (*pfnBegin)() = nullptr, void (*pfnEnd)() = nullptr, intp nMaxParallel = PTRDIFF_MAX )
{
	CParallelProcessor<ITEM_TYPE, CFuncJobItemProcessor<ITEM_TYPE>> processor( pszDescription );
	processor.m_ItemProcessor.Init( pfnProcess, pfnBegin, pfnEnd );
	processor.Run( pItems, nItems, nMaxParallel, pPool );
}


template <class ITEM_PROCESSOR_TYPE>
class CParallelLoopProcessor
{
public:
	explicit CParallelLoopProcessor( const char *pszDescription ) : m_ItemProcessor{}
	{
		m_lIndex.store( 0, std::memory_order_relaxed );
		m_lLimit = 0;
		m_nBatchDivisor = 1;
		m_nActive.store( 0, std::memory_order_relaxed );

		m_szDescription = pszDescription;
	}

	void Run( intp lBegin, intp nItems, intp nMaxParallel = PTRDIFF_MAX )
	{
		if ( nItems > 0 )
		{
			m_lIndex.store( lBegin, std::memory_order_relaxed );
			m_lLimit = lBegin + nItems;

			intp nJobs = g_pThreadPool->NumIdleThreads();

			if ( nMaxParallel < nJobs )
				nJobs = nMaxParallel;
			// Calling thread processes items too, no need in more helpers than items left.
			if ( nJobs > nItems - 1 )
				nJobs = nItems - 1;
			if ( nJobs > MAX_THREADS )
				nJobs = MAX_THREADS;
			if ( nJobs < 0 )
				nJobs = 0;

			// Set before jobs are queued, queueing publishes it to workers.
			m_nBatchDivisor = jobthread_detail::BatchDivisor( nJobs + 1 );

			// dimhotepus: Keep job handles and Abort() them after own pass instead of busy
			// spinning on a counter:
			// * Abort() cancels helpers which never started (items are all taken by now), so
			//   nested use from a pool thread no longer deadlocks waiting for queued jobs no
			//   free thread can pick up;
			// * Abort() blocks on job lock while helper is running, which also gives proper
			//   happens-before for item results (spin used relaxed loads, no ordering).
			CJob **ppJobs = stackallocT( CJob *, nJobs + 1 );

			for ( intp i = 0; i < nJobs; ++i )
			{
				ppJobs[i] = ThreadExecute( this, &CParallelLoopProcessor<ITEM_PROCESSOR_TYPE>::DoExecute );
			}

			DoExecute();

			for ( intp i = 0; i < nJobs; ++i )
			{
				ppJobs[i]->Abort(); // will either abort ones that never got a thread, or wait on ones that did
				ppJobs[i]->Release();
			}

			Assert( m_nActive.load( std::memory_order_acquire ) == 0 );
		}
	}

	ITEM_PROCESSOR_TYPE m_ItemProcessor;

private:
	void DoExecute()
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "DoExecute %s", m_szDescription );

		m_nActive.fetch_add( 1, std::memory_order_relaxed );

		m_ItemProcessor.Begin();

		const intp lLimit = m_lLimit;

		intp lBegin, lEnd;
		while ( jobthread_detail::ClaimBatch( m_lIndex, lLimit, m_nBatchDivisor, lBegin, lEnd ) )
		{
			for ( intp lIndex = lBegin; lIndex < lEnd; ++lIndex )
				m_ItemProcessor.Process( lIndex );
		}

		m_ItemProcessor.End();

		m_nActive.fetch_sub( 1, std::memory_order_release );
	}
	
	intp						m_lLimit;
	intp						m_nBatchDivisor;

	// dimhotepus: Own cache line, it is the only member written by all threads.
	alignas( 64 ) std::atomic<intp>	m_lIndex;

	// Next cache line: keep rarely written members off the hot counter line.
#ifdef PLATFORM_64BITS
	alignas( 64 ) std::atomic_int64_t	m_nActive;
#else
	alignas( 64 ) std::atomic_int32_t	m_nActive;
#endif

	const char *				m_szDescription;
};

inline void ParallelLoopProcess( const char *szDescription, intp lBegin, size_t nItems, void (*pfnProcess)( intp const & ), void (*pfnBegin)() = nullptr, void (*pfnEnd)() = nullptr, intp nMaxParallel = PTRDIFF_MAX )
{
	CParallelLoopProcessor< CFuncJobItemProcessor< intp const > > processor( szDescription );
	processor.m_ItemProcessor.Init( pfnProcess, pfnBegin, pfnEnd );
	processor.Run( lBegin, nItems, nMaxParallel );

}

template < typename OBJECT_TYPE, typename FUNCTION_CLASS > 
inline void ParallelLoopProcess( const char *szDescription, intp lBegin, size_t nItems, OBJECT_TYPE *pObject, void (FUNCTION_CLASS::*pfnProcess)( intp const & ), void (FUNCTION_CLASS::*pfnBegin)() = NULL, void (FUNCTION_CLASS::*pfnEnd)() = NULL, intp nMaxParallel = PTRDIFF_MAX )
{
	CParallelLoopProcessor< CMemberFuncJobItemProcessor< intp const, OBJECT_TYPE, FUNCTION_CLASS> > processor( szDescription );
	processor.m_ItemProcessor.Init( pObject, pfnProcess, pfnBegin, pfnEnd );
	processor.Run( lBegin, nItems, nMaxParallel );
}


template <class Derived>
class CParallelProcessorBase
{
protected:
	using ThisParallelProcessorBase_t = CParallelProcessorBase<Derived>;
	using ThisParallelProcessorDerived_t = Derived;

public:
	// dimhotepus: Allow to choose thread pool to run on.
	explicit CParallelProcessorBase(IThreadPool *pPool = nullptr)
	{
		m_nActive.store( 0, std::memory_order_relaxed );
		m_pThreadPool = pPool ? pPool : g_pThreadPool;
		m_szDescription = nullptr;
	}
	void SetDescription( const char *pszDescription )
	{
		m_szDescription = pszDescription;
	}

protected:
	void Run( intp nMaxParallel = PTRDIFF_MAX, intp threadOverride = -1 )
	{
		intp i = m_pThreadPool->NumIdleThreads();

		if ( nMaxParallel < i)
			i = nMaxParallel;
		if ( i > MAX_THREADS )
			i = MAX_THREADS;

		CJob **ppJobs = stackallocT( CJob *, ( i > 0 ? i : 0 ) + 1 );
		intp nJobs = 0;

		while( i -- > 0 )
		{
			if ( threadOverride == -1 || i == threadOverride - 1 )
			{
				ppJobs[nJobs++] = m_pThreadPool->QueueCall( this, &ThisParallelProcessorBase_t::DoExecute );
			}
		}

		// dimhotepus: If requested override thread does not exist nothing was executed at all.
		// Run on calling thread in such case.
		const bool bRunOnCaller = threadOverride == -1 || threadOverride == 0 || nJobs == 0;

		if ( bRunOnCaller )
		{
			DoExecute();

			// dimhotepus: Abort() cancels helpers which never started (work is drained by now)
			// and blocks until running ones finish. Replaces relaxed busy spin which deadlocked
			// when used from pool threads and had no happens-before with helper results.
			for ( intp j = 0; j < nJobs; ++j )
			{
				ppJobs[j]->Abort();
				ppJobs[j]->Release();
			}
		}
		else
		{
			// Only helper(s) do the work, must wait for them, not cancel.
			for ( intp j = 0; j < nJobs; ++j )
			{
				ppJobs[j]->WaitForFinishAndRelease();
			}
		}

		Assert( m_nActive.load( std::memory_order_acquire ) == 0 );
	}

protected:
	void OnBegin() {}
	bool OnProcess() { return false; }
	void OnEnd() {}

	// dimhotepus: Allow to choose thread pool to run on.
	IThreadPool *				m_pThreadPool;

private:
	void DoExecute()
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "DoExecute %s", m_szDescription );

		m_nActive.fetch_add( 1, std::memory_order_relaxed );

		static_cast<Derived *>( this )->OnBegin();

		while ( static_cast<Derived *>( this )->OnProcess() )
			continue;

		static_cast<Derived *>( this )->OnEnd();

		m_nActive.fetch_sub(1, std::memory_order_release);
	}

	const char *				m_szDescription;
#ifdef PLATFORM_64BITS
	std::atomic_int64_t			m_nActive;
#else
	std::atomic_int32_t			m_nActive;
#endif
};




//-----------------------------------------------------------------------------
// Raw thread launching
//-----------------------------------------------------------------------------

inline unsigned FunctorExecuteThread( void *pParam )
{
	auto *pFunctor = (CFunctor *)pParam;
	(*pFunctor)();
	pFunctor->Release();
	return 0;
}

inline ThreadHandle_t ThreadExecuteSoloImpl( CFunctor *pFunctor, const char *pszName = nullptr )
{
	ThreadId_t threadId;
	ThreadHandle_t hThread = CreateSimpleThread( FunctorExecuteThread, pFunctor, &threadId );
	if ( !hThread )
	{
		// dimhotepus: Thread never ran, so it never released the functor.
		pFunctor->Release();
		return hThread;
	}
	if ( pszName )
		ThreadSetDebugName( threadId, pszName );
	return hThread;
}

// dimhotepus: Keep job alive while solo thread executes it, caller may release job right away.
inline ThreadHandle_t ThreadExecuteSolo( CJob *pJob ) { return ThreadExecuteSoloImpl( CreateRefCountingFunctor( pJob, &CJob::Execute ), pJob->Describe() ); }

template <typename... Args>
inline ThreadHandle_t ThreadExecuteSolo( const char *pszName, Args&& ... args ) { return ThreadExecuteSoloImpl( CreateFunctor( std::forward<Args>( args )... ), pszName ); }

template <typename... Args>
inline ThreadHandle_t ThreadExecuteSoloRef( const char *pszName, Args&& ... args ) { return ThreadExecuteSoloImpl( CreateRefCountingFunctor( std::forward<Args>( args )... ), pszName ); }

//-----------------------------------------------------------------------------

inline bool IThreadPool::YieldWait( CThreadEvent &event, unsigned timeout )
{
	CThreadEvent *pEvent = &event;
	// dimhotepus: Failure is not success.
	const unsigned result = YieldWait( &pEvent, 1, true, timeout );
	return result != TW_TIMEOUT && result != TW_FAILED;
}

inline bool IThreadPool::YieldWait( CJob *pJob, unsigned timeout )
{
	// dimhotepus: Failure is not success.
	const unsigned result = YieldWait( &pJob, 1, true, timeout );
	return result != TW_TIMEOUT && result != TW_FAILED;
}

//-----------------------------------------------------------------------------

inline JobStatus_t CJob::Execute()
{
	if ( IsFinished() )
		return GetStatus();

	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s %s %d", __FUNCTION__, Describe(), GetStatus() );

	// dimhotepus: Self reference must outlive the lock. Previously Release() ran before
	// AUTO_LOCK destructor, so if DoExecute() dropped the last outside reference the
	// mutex was unlocked after the job was deleted (use-after-free).
	AddRef();

	JobStatus_t result;

	{
		AUTO_LOCK( m_mutex );

		switch ( GetStatus() )
		{
		case JOB_STATUS_UNSERVICED:
		case JOB_STATUS_PENDING:
			{
				// Service it
				SlamStatus( JOB_STATUS_INPROGRESS );
				result = DoExecute();
				SlamStatus( result );
				DoCleanup();
				SignalComplete();
				break;
			}

		case JOB_STATUS_INPROGRESS:
			AssertMsg(0, "Mutex Should have protected use while processing");
			[[fallthrough]];

		case JOB_OK:
			[[fallthrough]];
		case JOB_STATUS_ABORTED:
			result = GetStatus();
			break;

		default:
			result = GetStatus();
			AssertMsg( result < JOB_OK, "Unknown job state");
		}
	}

	Release();

	return result;
}


//---------------------------------------------------------

inline JobStatus_t CJob::TryExecute()
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s %s %d", __FUNCTION__, Describe(), GetStatus() );

	// TryLock() would only fail if another thread has entered
	// Execute() or Abort()
	if ( !IsFinished() && TryLock() )
	{
		// ...service the request
		const JobStatus_t result = Execute();
		Unlock();
		// dimhotepus: Do not touch members after Unlock(), job may be already gone.
		return result;
	}
	return GetStatus();
}

//---------------------------------------------------------

inline JobStatus_t CJob::Abort( bool bDiscard )
{
	if ( IsFinished() )
		return GetStatus();

	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s %s %d", __FUNCTION__, Describe(), GetStatus() );

	// dimhotepus: Self reference must outlive the lock, see Execute().
	AddRef();

	JobStatus_t result;

	{
		AUTO_LOCK( m_mutex );

		switch ( GetStatus() )
		{
		case JOB_STATUS_UNSERVICED:
		case JOB_STATUS_PENDING:
			{
				tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "CJob::DoAbort" );

				result = DoAbort( bDiscard );
				SlamStatus( result );
				if ( bDiscard )
					DoCleanup();
				SignalComplete();
			}
			break;

		case JOB_STATUS_ABORTED:
		case JOB_STATUS_INPROGRESS:
		case JOB_OK:
			result = GetStatus();
			break;

		default:
			result = GetStatus();
			AssertMsg( result < JOB_OK, "Unknown job state");
		}
	}

	Release();

	return result;
}

//-----------------------------------------------------------------------------

#endif // JOBTHREAD_H