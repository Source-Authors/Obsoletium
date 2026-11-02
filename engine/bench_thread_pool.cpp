//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Thread pool micro and frame-pattern benchmarks.
//
// Uses only the public IThreadPool / CJob API, so the same file measures the
// current pool and any replacement.
//
//=============================================================================

#include "bench_thread_pool.h"

#include <algorithm>
#include <atomic>

#include "tier0/dbg.h"
#include "tier0/platform.h"
#include "tier0/threadtools.h"
#include "vstdlib/jobthread.h"
#include "tier1/functors.h"
#include "tier1/utlvector.h"

namespace
{

std::atomic< int > g_nBenchDone{ 0 };
double             g_flSpinItersPerUs = 0;

// Busy work of roughly flUs microseconds, independent of timer resolution.
void SpinWork( double flUs )
{
	volatile double x = 1.0;
	const int       nIters = static_cast< int >( flUs * g_flSpinItersPerUs );
	for ( int i = 0; i < nIters; ++i )
		x = x * 1.0000001 + 0.5;
}

// Only used to size work items. Reported "ideal" times are always measured, never derived
// from calibration, so remaining clock drift can't distort results.
void CalibrateSpin()
{
	// Warm up first, so CPU leaves low-power clocks before we measure.
	g_flSpinItersPerUs = 100;
	const double flWarmEnd = Plat_FloatTime() + 0.2;
	while ( Plat_FloatTime() < flWarmEnd )
		SpinWork( 1000 );

	// Best of several short runs.
	double flBest = 0;
	for ( int i = 0; i < 5; ++i )
	{
		const double t0 = Plat_FloatTime();
		SpinWork( 20000 ); // 2M iterations at guessed rate
		const double us = ( Plat_FloatTime() - t0 ) * 1e6;
		flBest = std::max( flBest, 100 * 20000 / us );
	}
	g_flSpinItersPerUs = flBest;
}

// Median wall time of fn() in microseconds.
template< typename Fn >
double MedianUs( int nRuns, Fn &&fn )
{
	CUtlVector< double > samples;
	for ( int i = 0; i < nRuns; ++i )
	{
		const double t0 = Plat_FloatTime();
		fn();
		samples.AddToTail( ( Plat_FloatTime() - t0 ) * 1e6 );
	}
	double *p = samples.Base();
	std::sort( p, p + samples.Count() );
	return p[samples.Count() / 2];
}

void TinyJob()
{
	g_nBenchDone.fetch_add( 1, std::memory_order_relaxed );
}
void WorkJob( double flUs )
{
	SpinWork( flUs );
	g_nBenchDone.fetch_add( 1, std::memory_order_relaxed );
}

std::atomic< double > g_flJobStart{ 0 };
void                  StampJob()
{
	g_flJobStart.store( Plat_FloatTime(), std::memory_order_relaxed );
}

void WaitDone( int nTarget )
{
	while ( g_nBenchDone.load( std::memory_order_relaxed ) < nTarget )
		ThreadPause();
}

struct Stats_t
{
	double median, p99, max;
};

Stats_t ComputeStats( CUtlVector< double > &samples )
{
	double    *p = samples.Base();
	const intp n = samples.Count();
	std::sort( p, p + n );
	return { p[n / 2], p[std::min< intp >( n - 1, n * 99 / 100 )], p[n - 1] };
}

void LoopItem( intp const & )
{
	SpinWork( 0.05 );
}

struct CEmptyJob : CJob
{
	JobStatus_t DoExecute() override { return JOB_OK; }
};

// Average ns per call of fn over N calls.
template< typename Fn >
double NsPerOp( int N, Fn &&fn )
{
	const double t0 = Plat_FloatTime();
	for ( int i = 0; i < N; ++i )
		fn();
	return ( Plat_FloatTime() - t0 ) / N * 1e9;
}

// Breaks job lifecycle cost [1] into its parts.
void RunJobCostBreakdown()
{
	constexpr int N = 100000;

	const double flAlloc = NsPerOp( N, []
	                                {
		char *volatile p = new char[128];
		delete[] p; } );
	const double flEvent = NsPerOp( N, []
	                                {
										CThreadEvent ev( true ); // as CJob::m_CompleteEvent
									} );
	CThreadEvent evSet( true );
	const double flEventSet = NsPerOp( N, [&evSet]
	                                   { evSet.Set(); } );
	const double flMutex = NsPerOp( N, []
	                                {
										CThreadMutex mtx; // as CJob::m_mutex
									} );
	CThreadMutex mtxLock;
	const double flLock = NsPerOp( N, [&mtxLock]
	                               { mtxLock.Lock(); mtxLock.Unlock(); } );
	const double flFunctor = NsPerOp( N, []
	                                  { CreateFunctor( TinyJob )->Release(); } );
	const double flJob = NsPerOp( N, []
	                              { ( new CEmptyJob )->Release(); } );
	const double flJobExec = NsPerOp( N, []
	                                  {
		CJob *pJob = new CEmptyJob;
		pJob->Execute();
		pJob->Release(); } );

	Msg( "[0] breakdown: heap new+delete %6.0f ns | event create+destroy %6.0f ns | event Set %6.0f ns\n",
	     flAlloc, flEvent, flEventSet );
	Msg( "[0]            mutex create+destroy %6.0f ns | mutex lock+unlock %6.0f ns | functor create+release %6.0f ns\n",
	     flMutex, flLock, flFunctor );
	Msg( "[0]            empty CJob new+release %6.0f ns | + Execute() %6.0f ns\n", flJob, flJobExec );
}

} // namespace

namespace s1::engine::tests::thread_pool
{

//-----------------------------------------------------------------------------
// Runs all scenarios on pPool (defaults to global pool). Takes ~10-20 seconds.
//-----------------------------------------------------------------------------
void RunThreadPoolBenchmark( IThreadPool *pPool )
{
	if ( !pPool )
		pPool = g_pThreadPool;

	const intp nThreads = pPool->NumThreads();
	if ( nThreads <= 0 )
	{
		Warning( "Thread pool benchmark: pool has no threads.\n" );
		return;
	}

	CalibrateSpin();
#if defined( _DEBUG ) || defined( DEBUG )
	Msg( "=== Thread pool benchmark: %zd worker threads, DEBUG build (timings not representative) ===\n", nThreads );
#else
	Msg( "=== Thread pool benchmark: %zd worker threads, release build ===\n", nThreads );
#endif

	RunJobCostBreakdown();

	// 1. Cost of a job object alone: functor + job alloc, sync objects, execute, free.
	{
		constexpr int N = 100000;
		const double  t0 = Plat_FloatTime();
		for ( int i = 0; i < N; ++i )
		{
			CJob *pJob = new CFunctorJob( CreateFunctor( TinyJob ) );
			pJob->Execute();
			pJob->Release();
		}
		Msg( "[1] job object lifecycle (no pool)      : %7.0f ns/job\n", ( Plat_FloatTime() - t0 ) / N * 1e9 );
	}

	// 2. Throughput of tiny jobs submitted by main thread.
	{
		constexpr int N = 200000;
		g_nBenchDone = 0;
		const double t0 = Plat_FloatTime();
		for ( int i = 0; i < N; ++i )
			pPool->QueueCall( TinyJob )->Release();
		const double tSubmit = Plat_FloatTime() - t0;
		WaitDone( N );
		const double tAll = Plat_FloatTime() - t0;
		Msg( "[2] tiny jobs: submit %5.0f ns/job, end-to-end %5.0f ns/job\n", tSubmit / N * 1e9, tAll / N * 1e9 );
	}

	// 3. Wake latency: one job submitted while all workers sleep.
	{
		CUtlVector< double > samples;
		for ( int i = 0; i < 300; ++i )
		{
			ThreadSleep( 2 );
			g_flJobStart = 0;
			const double t0 = Plat_FloatTime();
			pPool->QueueCall( StampJob )->WaitForFinishAndRelease();
			samples.AddToTail( ( g_flJobStart.load() - t0 ) * 1e6 );
		}
		const Stats_t s = ComputeStats( samples );
		Msg( "[3] wake latency      : median %7.1f us, p99 %7.1f us, max %7.1f us\n", s.median, s.p99, s.max );
	}

	// 4. Frame pattern: fork-join burst of K jobs of D us, caller waits (and helps).
	//    Overhead = wall time - ideal. Ideal is the critical path with measured job time:
	//    K jobs on (workers + helping caller) need ceil(K / P) rounds even if perfect.
	//    "warm": bursts back to back (several jobs systems in a row within a frame).
	//    "cold": 2ms pause before each burst, workers are asleep (first burst of a frame).
	for ( double flUs : { 5.0, 20.0, 100.0 } )
	{
		const int    K = static_cast< int >( std::min< intp >( nThreads * 4, MAX_THREADS ) );
		const double flJobUs = MedianUs( 200, [flUs]
		                                 { SpinWork( flUs ); } );
		const intp   nParticipants = nThreads + 1;
		const double flIdealUs = flJobUs * static_cast< double >( ( K + nParticipants - 1 ) / nParticipants );

		const auto runBurst = [&]()
		{
			const double t0 = Plat_FloatTime();
			CJobSet      jobs;
			for ( int k = 0; k < K; ++k )
				jobs += pPool->QueueCall( WorkJob, flUs );
			jobs.WaitForFinish( pPool );
			return ( Plat_FloatTime() - t0 ) * 1e6 - flIdealUs;
		};

		CUtlVector< double > warm, cold;
		for ( int iter = 0; iter < 300; ++iter )
			warm.AddToTail( runBurst() );
		for ( int iter = 0; iter < 100; ++iter )
		{
			ThreadSleep( 2 );
			cold.AddToTail( runBurst() );
		}

		const Stats_t w = ComputeStats( warm );
		const Stats_t c = ComputeStats( cold );
		Msg( "[4] burst %2d x %5.1f us (measured %5.1f us, ideal %6.1f us) overhead:\n", K, flUs, flJobUs, flIdealUs );
		Msg( "[4]     warm: median %6.1f us, p99 %6.1f us, max %6.1f us\n", w.median, w.p99, w.max );
		Msg( "[4]     cold: median %6.1f us, p99 %6.1f us, max %6.1f us\n", c.median, c.p99, c.max );
	}

	// 5. Jobs spawned by workers (the case work stealing targets).
	{
		constexpr int nOuter = 64, nInner = 500;
		g_nBenchDone = 0;
		const double t0 = Plat_FloatTime();
		for ( int i = 0; i < nOuter; ++i )
		{
			pPool->QueueCall( +[]( IThreadPool *pp )
			                  {
				constexpr int nInner = 500;
				for ( int k = 0; k < nInner; ++k )
					pp->QueueCall( TinyJob )->Release(); },
			                  pPool )
			->Release();
		}
		WaitDone( nOuter * nInner );
		Msg( "[5] worker-spawned tiny jobs           : %7.0f ns/job\n", ( Plat_FloatTime() - t0 ) / ( nOuter * nInner ) * 1e9 );
	}

	// 6. Fine-grained parallel loop (per-item shared counter), on global pool.
	{
		constexpr intp nItems = 200000;
		// Ideal: measured single-thread time split evenly over workers + caller.
		const double         flSerialUs = MedianUs( 20, [=]
		                                            {
			for ( intp i = 0; i < nItems; ++i )
				LoopItem( i ); } );
		const double         flIdealUs = flSerialUs / static_cast< double >( g_pThreadPool->NumThreads() + 1 );
		CUtlVector< double > samples;
		for ( int i = 0; i < 50; ++i )
		{
			const double t0 = Plat_FloatTime();
			ParallelLoopProcess( "bench", 0, nItems, LoopItem );
			samples.AddToTail( ( Plat_FloatTime() - t0 ) * 1e6 );
		}
		const Stats_t s = ComputeStats( samples );
		Msg( "[6] ParallelLoopProcess 200k x 50ns   : median %7.0f us (ideal %6.0f us, serial %6.0f us)\n", s.median, flIdealUs, flSerialUs );
	}

	Msg( "=== done ===\n" );
}

} // namespace s1::engine::tests::thread_pool
