// Copyright Valve Corporation, All rights reserved.

#ifndef SE_PUBLIC_TIER0_FASTTIMER_H_
#define SE_PUBLIC_TIER0_FASTTIMER_H_

#include <atomic>   // std::atomic, std::atomic_signal_fence
#include <climits>  // INT_MAX, ULONG_MAX
#include <cstdint>  // UINT64_MAX

#include "platform.h"  // Plat_Rdtscp + <intrin.h> / <x86intrin.h>
#include "dbg.h"

// dimhotepus: Document what the clock globals hold and when they are valid.
// Counter ticks per second of the counter read by Plat_Rdtscp() (TSC on x86,
// CNTVCT_EL0 on ARM64, steady_clock elsewhere).
//
// All of these are written exactly once by CClockSpeedInit::Init() during
// tier0 static initialization, before any other thread can exist, and are
// read-only afterwards.  Until then they are zero; every conversion below
// tolerates zero instead of dividing by it.
PLATFORM_INTERFACE uint64 g_ClockSpeed;

PLATFORM_INTERFACE double g_ClockSpeedMicrosecondsMultiplier;
PLATFORM_INTERFACE double g_ClockSpeedMillisecondsMultiplier;
PLATFORM_INTERFACE double g_ClockSpeedSecondsMultiplier;

// Median cost, in ticks, of two back-to-back Plat_Rdtscp() calls.  Subtracted
// from every measured duration so an empty timed block reads ~0.
PLATFORM_INTERFACE uint64 g_ClockRtscpOverhead;

// dimhotepus: Helpers for overflow-safe, sanitizer-clean conversions.
namespace s1::tier0::fasttimer_detail
{

inline constexpr uint64 kUint64Max{ UINT64_MAX };

// dimhotepus: Replaces a * b / d which overflowed uint64 for long durations
// (~1.7 hours at 3 GHz for microseconds) and divided by zero before init.
//
// floor( a * b / d ) without the a * b intermediate overflowing.
//
// Exact as long as b * d fits in 64 bits, which holds for every caller here
// (one side is a clock rate ~1e10, the other is 1e3 or 1e6).  Saturates at
// UINT64_MAX when the true result does not fit.  Returns 0 for d == 0, so
// conversions done before clock initialization yield 0 instead of SIGFPE.
[[nodiscard]] constexpr uint64 MulDivU64( uint64 a, uint64 b, uint64 d ) noexcept
{
	if ( d == 0 )
		return 0;

	const uint64 q{ a / d };
	const uint64 r{ a % d };

	if ( b != 0 && q > kUint64Max / b )
		return kUint64Max;

	const uint64 hi{ q * b };
	const uint64 lo{ r * b / d };  // r < d, so r * b < d * b: no overflow.

	return hi > kUint64Max - lo ? kUint64Max : hi + lo;
}

// dimhotepus: Saturating narrowing for the legacy unsigned long getters
// (32-bit on Windows), instead of silently wrapping.
[[nodiscard]] constexpr unsigned long SaturateToULong( uint64 v ) noexcept
{
	if constexpr ( sizeof( unsigned long ) < sizeof( uint64 ) )
	{
		if ( v > static_cast<uint64>( ULONG_MAX ) )
			return ULONG_MAX;
	}
	return static_cast<unsigned long>( v );
}

// dimhotepus: Compiler-only barrier, keeps the compiler from moving memory accesses
// across a timestamp read.  Emits no instruction on any target.  CPU-side ordering is
// already provided by the LFENCE / ISB in Plat_Rdtscp().
FORCEINLINE void CompilerBarrier() noexcept
{
	std::atomic_signal_fence( std::memory_order_seq_cst );
}

[[nodiscard]] FORCEINLINE uint64 ElapsedTicks( uint64 start, uint64 end ) noexcept
{
	if ( end <= start )
		return 0;

	const uint64 elapsed{ end - start };
	const uint64 overhead{ g_ClockRtscpOverhead };
	return elapsed > overhead ? elapsed - overhead : 0;
}

}  // namespace s1::tier0::fasttimer_detail

class CCycleCount
{
friend class CFastTimer;

public:
					CCycleCount();
					CCycleCount( uint64 cycles );

	void			Sample();	// Sample the clock. This takes about 34 clocks to execute (or 26,000 calls per millisecond on a P900).

	void			Init();		// Set to zero.
	void			Init( float initTimeMsec );
	void			Init( double initTimeMsec )		{ Init( static_cast<float>( initTimeMsec ) ); }
	void			Init( uint64 cycles );
	[[nodiscard]] bool			IsLessThan( CCycleCount const &other ) const;					// Compare two counts.

	// Convert to other time representations. These functions are slow, so it's preferable to call them
	// during display rather than inside a timing block.
	[[nodiscard]] unsigned long	GetCycles()  const;
	[[nodiscard]] uint64			GetLongCycles() const;

	[[nodiscard]] unsigned long	GetMicroseconds() const;
	[[nodiscard]] uint64			GetUlMicroseconds() const;
	[[nodiscard]] double			GetMicrosecondsF() const; 	
	void			SetMicroseconds( unsigned long nMicroseconds );

	[[nodiscard]] unsigned long	GetMilliseconds() const;
	[[nodiscard]] double			GetMillisecondsF() const;

	[[nodiscard]] double			GetSeconds() const;

	CCycleCount&	operator+=( CCycleCount const &other );

	// dest = rSrc1 + rSrc2
	static void		Add( CCycleCount const &rSrc1, CCycleCount const &rSrc2, CCycleCount &dest );	// Add two samples together.
	
	// dest = rSrc1 - rSrc2
	static void		Sub( CCycleCount const &rSrc1, CCycleCount const &rSrc2, CCycleCount &dest );	// Add two samples together.

	static uint64	GetTimestamp();

	uint64			m_Int64{ 0 };
};

class PLATFORM_CLASS CClockSpeedInit
{
public:
	CClockSpeedInit()
	{
		Init();
	}

	static void Init();
};

class CFastTimer
{
public:
	// These functions are fast to call and should be called from your sampling code.
	void				Start();
	void				End();

	[[nodiscard]] const CCycleCount &	GetDuration() const;	// Get the elapsed time between Start and End calls.
	[[nodiscard]] CCycleCount 		GetDurationInProgress() const; // Call without ending. Not that cheap.

	// Return number of counter ticks per second.
	// dimhotepus: int64 -> uint64.
	[[nodiscard]] static inline uint64 GetClockSpeed();

private:
	CCycleCount	m_Duration{};	// Holds the start stamp while running, the duration after End().
#ifdef DEBUG_FASTTIMER
	bool m_bRunning{ false };		// Are we currently running?
#endif
};


// This is a helper class that times whatever block of code it's in
class CTimeScope
{
public:
	// dimhotepus: explicit, no implicit conversion from timer pointer.
	explicit	CTimeScope( CFastTimer *pTimer ) noexcept;
				~CTimeScope();

	// dimhotepus: Non-copyable, copy would End() the timer twice.
	CTimeScope( const CTimeScope & ) = delete;
	CTimeScope &operator=( const CTimeScope & ) = delete;

private:
	CFastTimer	*m_pTimer{ nullptr };
};

inline CTimeScope::CTimeScope( CFastTimer *pTotal ) noexcept
	: m_pTimer{ pTotal }
{
	m_pTimer->Start();
}

inline CTimeScope::~CTimeScope()
{
	m_pTimer->End();
}

// This is a helper class that times whatever block of code it's in and
// adds the total (int microseconds) to a global counter.
class CTimeAdder
{
public:
	explicit CTimeAdder( CCycleCount *pTotal );
	~CTimeAdder();

	// dimhotepus: Non-copyable, copy would add the duration twice.
	CTimeAdder( const CTimeAdder & ) = delete;
	CTimeAdder &operator=( const CTimeAdder & ) = delete;

	void		End() noexcept;

private:
	CCycleCount	*m_pTotal{ nullptr };
	CFastTimer	m_Timer{};
};

inline CTimeAdder::CTimeAdder( CCycleCount *pTotal )
{
	m_pTotal = pTotal;
	m_Timer.Start();
}

inline CTimeAdder::~CTimeAdder()
{
	End();
}

inline void CTimeAdder::End() noexcept
{
	if( m_pTotal )
	{
		m_Timer.End();
		*m_pTotal += m_Timer.GetDuration();
		m_pTotal = nullptr;
	}
}



// -------------------------------------------------------------------------- //
// Simple tool to support timing a block of code, and reporting the results on
// program exit or at each iteration.
//
// dimhotepus: PROFILE_SCOPE may be hit from several threads at once: the
// counter is atomic.  Its report runs from a static destructor at process
// exit, so do not use it in code that may still run after tier0 logging has
// shut down.
// -------------------------------------------------------------------------- //
#define PROFILE_SCOPE(name) \
	class C##name##ACC : public CAverageCycleCounter \
	{ \
	public: \
		~C##name##ACC() \
		{ \
			Msg("%-48s: %6.3f avg (%8.1f total, %7.3f peak, %5u iters)\n",  \
				#name, \
				GetAverageMilliseconds(), \
				GetTotalMilliseconds(), \
				GetPeakMilliseconds(), \
				GetIters() ); \
		} \
	}; \
	static C##name##ACC name##_ACC{}; \
	CAverageTimeMarker name##_ATM{ &name##_ACC }

#define TIME_SCOPE(name) \
	class CTimeScopeMsg_##name \
	{ \
	public: \
		CTimeScopeMsg_##name() { m_Timer.Start(); } \
		~CTimeScopeMsg_##name() \
		{ \
			m_Timer.End(); \
			Msg( #name "time: %.4fms\n", m_Timer.GetDuration().GetMillisecondsF() ); \
		} \
	private:	\
		CFastTimer	m_Timer{}; \
	} name##_TSM{};


// -------------------------------------------------------------------------- //

// dimhotepus: Thread-safe accumulator.  MarkIter() may be called
// concurrently.  Init() must not race with MarkIter().  Getters may run
// concurrently with MarkIter(); the average is then computed from two
// independently loaded values.
class CAverageCycleCounter
{
public:
	CAverageCycleCounter() noexcept = default;

	// dimhotepus: Non-copyable (atomic members).
	CAverageCycleCounter( const CAverageCycleCounter & ) = delete;
	CAverageCycleCounter &operator=( const CAverageCycleCounter & ) = delete;

	void Init() noexcept;
	void MarkIter( const CCycleCount &duration ) noexcept;

	[[nodiscard]] unsigned GetIters() const noexcept;

	[[nodiscard]] double GetAverageMilliseconds() const noexcept;
	[[nodiscard]] double GetTotalMilliseconds() const noexcept;
	[[nodiscard]] double GetPeakMilliseconds() const noexcept;

private:
	// dimhotepus: unsigned / CCycleCount -> std::atomic.
	std::atomic<unsigned>	m_nIters{ 0 };
	std::atomic<uint64>		m_Total{ 0 };
	std::atomic<uint64>		m_Peak{ 0 };
};

// -------------------------------------------------------------------------- //

class CAverageTimeMarker
{
public:
	explicit CAverageTimeMarker( CAverageCycleCounter *pCounter ) noexcept;
	~CAverageTimeMarker();

	// dimhotepus: Non-copyable, copy would mark the iteration twice.
	CAverageTimeMarker( const CAverageTimeMarker & ) = delete;
	CAverageTimeMarker &operator=( const CAverageTimeMarker & ) = delete;

private:
	CAverageCycleCounter *m_pCounter{ nullptr };
	CFastTimer	m_Timer{};
};


// -------------------------------------------------------------------------- // 
// CCycleCount inlines.
// -------------------------------------------------------------------------- // 

inline CCycleCount::CCycleCount()
{
	Init( static_cast<uint64>(0) );
}

inline CCycleCount::CCycleCount( uint64 cycles )
{
	Init( cycles );
}

inline void CCycleCount::Init()
{
	Init( static_cast<uint64>(0) );
}

inline void CCycleCount::Init( float initTimeMsec )
{
	if ( g_ClockSpeedMillisecondsMultiplier > 0.0 )
		Init( static_cast<uint64>( static_cast<double>( initTimeMsec ) / g_ClockSpeedMillisecondsMultiplier) ); //-V2004
	else
		Init( static_cast<uint64>(0) );
}

inline void CCycleCount::Init( uint64 cycles )
{
	m_Int64 = cycles;
}

inline void CCycleCount::Sample()
{
	// dimhotepus: Plat_Rdtscp is not cached by the optimizer.
	// dimhotepus: Overhead is now subtracted in ElapsedTicks.
	uint32 coreId{ 0 };
	m_Int64 = Plat_Rdtscp( coreId );
}

inline CCycleCount& CCycleCount::operator+=( CCycleCount const &other )
{
	m_Int64 += other.m_Int64;
	return *this;
}


inline void CCycleCount::Add( CCycleCount const &rSrc1, CCycleCount const &rSrc2, CCycleCount &dest )
{
	dest.m_Int64 = rSrc1.m_Int64 + rSrc2.m_Int64;
}

inline void CCycleCount::Sub( CCycleCount const &rSrc1, CCycleCount const &rSrc2, CCycleCount &dest )
{
	dest.m_Int64 = rSrc1.m_Int64 - rSrc2.m_Int64;
}

inline uint64 CCycleCount::GetTimestamp()
{
	// dimhotepus: Read counter directly, no temporary CCycleCount.
	uint32 coreId{ 0 };
	return Plat_Rdtscp( coreId );
}

inline bool CCycleCount::IsLessThan(CCycleCount const &other) const
{
	return m_Int64 < other.m_Int64;
}


inline unsigned long CCycleCount::GetCycles() const
{
	// dimhotepus: Saturate instead of truncate.
	return s1::tier0::fasttimer_detail::SaturateToULong( m_Int64 );
}

inline uint64 CCycleCount::GetLongCycles() const
{
	return m_Int64;
}

inline unsigned long CCycleCount::GetMicroseconds() const
{
	// dimhotepus: Overflow-safe and saturating.
	return s1::tier0::fasttimer_detail::SaturateToULong( GetUlMicroseconds() );
}

inline uint64 CCycleCount::GetUlMicroseconds() const
{
	// dimhotepus: m_Int64 * 1000000 overflowed, division by zero before init.
	return s1::tier0::fasttimer_detail::MulDivU64( m_Int64, 1000000, g_ClockSpeed );
}


inline double CCycleCount::GetMicrosecondsF() const
{
	return static_cast<double>( m_Int64 ) * g_ClockSpeedMicrosecondsMultiplier;
}


inline void	CCycleCount::SetMicroseconds( unsigned long nMicroseconds )
{
	// dimhotepus: nMicroseconds * g_ClockSpeed overflowed with 64-bit unsigned long.
	m_Int64 = s1::tier0::fasttimer_detail::MulDivU64( nMicroseconds, g_ClockSpeed, 1000000 );
}


inline unsigned long CCycleCount::GetMilliseconds() const
{
	// dimhotepus: Overflow-safe and saturating, no division by zero before init.
	return s1::tier0::fasttimer_detail::SaturateToULong(
		s1::tier0::fasttimer_detail::MulDivU64( m_Int64, 1000, g_ClockSpeed ) );
}


inline double CCycleCount::GetMillisecondsF() const
{
	return static_cast<double>( m_Int64 ) * g_ClockSpeedMillisecondsMultiplier;
}


inline double CCycleCount::GetSeconds() const
{
	return static_cast<double>( m_Int64 ) * g_ClockSpeedSecondsMultiplier;
}


// -------------------------------------------------------------------------- //
// CFastTimer inlines.
// -------------------------------------------------------------------------- // 
inline void CFastTimer::Start()
{
#ifdef DEBUG_FASTTIMER
	m_bRunning = true;
#endif

	// dimhotepus: Compiler barriers keep the timed code from being moved
	// across the sample.  They cost nothing at runtime and are TSan-clean,
	// unlike the former std::atomic_thread_fence.
	s1::tier0::fasttimer_detail::CompilerBarrier();
	m_Duration.Sample();
	s1::tier0::fasttimer_detail::CompilerBarrier();
}


inline void CFastTimer::End()
{
	// dimhotepus: Compiler barriers instead of std::atomic_thread_fence.
	s1::tier0::fasttimer_detail::CompilerBarrier();
	const uint64 end{ CCycleCount::GetTimestamp() };
	s1::tier0::fasttimer_detail::CompilerBarrier();

	// dimhotepus: Subtract sampling overhead, clamp backwards steps to 0.
	m_Duration.m_Int64 = s1::tier0::fasttimer_detail::ElapsedTicks( m_Duration.m_Int64, end );

#ifdef DEBUG_FASTTIMER
	// dimhotepus: Catch End() without Start().
	Assert( m_bRunning );
	m_bRunning = false;
#endif
}

inline CCycleCount CFastTimer::GetDurationInProgress() const
{
#ifdef DEBUG_FASTTIMER
	// dimhotepus: Catch call on a stopped timer.
	Assert( m_bRunning );
#endif
	// dimhotepus: Subtract sampling overhead, clamp backwards steps to 0.
	return CCycleCount{ s1::tier0::fasttimer_detail::ElapsedTicks(
		m_Duration.m_Int64, CCycleCount::GetTimestamp() ) };
}


inline uint64 CFastTimer::GetClockSpeed()
{
	return g_ClockSpeed;
}


inline CCycleCount const& CFastTimer::GetDuration() const
{
#ifdef DEBUG_FASTTIMER
	Assert( !m_bRunning );
#endif
	return m_Duration;
}


// -------------------------------------------------------------------------- //
// CAverageCycleCounter inlines

inline void CAverageCycleCounter::Init() noexcept
{
	// dimhotepus: Atomic stores.
	m_Total.store( 0, std::memory_order_relaxed );
	m_Peak.store( 0, std::memory_order_relaxed );
	m_nIters.store( 0, std::memory_order_relaxed );
}

inline void CAverageCycleCounter::MarkIter( const CCycleCount &duration ) noexcept
{
	// dimhotepus: Atomic RMW instead of racy read-modify-write.
	const uint64 ticks{ duration.GetLongCycles() };

	m_nIters.fetch_add( 1, std::memory_order_relaxed );
	m_Total.fetch_add( ticks, std::memory_order_relaxed );

	// dimhotepus: Atomic max.  Usually a single load: the CAS only runs on a
	// new peak.
	uint64 peak{ m_Peak.load( std::memory_order_relaxed ) };
	while ( peak < ticks &&
			!m_Peak.compare_exchange_weak( peak, ticks, std::memory_order_relaxed, std::memory_order_relaxed ) )
	{
		// do nothing.
	}
}

inline unsigned CAverageCycleCounter::GetIters() const noexcept
{
	return m_nIters.load( std::memory_order_relaxed );
}

inline double CAverageCycleCounter::GetAverageMilliseconds() const noexcept
{
	// dimhotepus: Load iteration count once.
	const unsigned iters{ GetIters() };
	return iters ? GetTotalMilliseconds() / static_cast<double>( iters ) : 0.0;
}

inline double CAverageCycleCounter::GetTotalMilliseconds() const noexcept
{
	return CCycleCount{ m_Total.load( std::memory_order_relaxed ) }.GetMillisecondsF();
}

inline double CAverageCycleCounter::GetPeakMilliseconds() const noexcept
{
	return CCycleCount{ m_Peak.load( std::memory_order_relaxed ) }.GetMillisecondsF();
}

// -------------------------------------------------------------------------- // 

inline CAverageTimeMarker::CAverageTimeMarker( CAverageCycleCounter *pCounter ) noexcept
	: m_pCounter{ pCounter }
{
	// dimhotepus: Catch null counter early instead of crashing in destructor.
	Assert( m_pCounter );
	m_Timer.Start();
}

inline CAverageTimeMarker::~CAverageTimeMarker()
{
	m_Timer.End();
	m_pCounter->MarkIter( m_Timer.GetDuration() );
}


// CLimitTimer
// Use this to time whether a desired interval of time has passed.  It's extremely fast
// to check while running.  NOTE: CMicroSecOverage() and CMicroSecLeft() are not as fast to check.
// dimhotepus: A default-constructed timer reports its limit as already reached.
class CLimitTimer
{
public:
	CLimitTimer() = default;
	explicit CLimitTimer( uint64 cMicroSecDuration ) { SetLimit( cMicroSecDuration ); }
	void SetLimit( uint64 m_cMicroSecDuration );
	[[nodiscard]] bool BLimitReached() const;

	[[nodiscard]] int CMicroSecOverage() const;
	[[nodiscard]] uint64 CMicroSecLeft() const; 

private:
	// dimhotepus: Initialize, was read uninitialized by a default-constructed timer.
	uint64 m_lCycleLimit{ 0 };
};


//-----------------------------------------------------------------------------
// Purpose: Initializes the limit timer with a period of time to measure.
// Input  : cMicroSecDuration -		How long a time period to measure.
//									Huge values (e.g. UINT64_MAX) mean "never".
//-----------------------------------------------------------------------------
inline void CLimitTimer::SetLimit( uint64 cMicroSecDuration )
{
	using namespace s1::tier0::fasttimer_detail;

	// dimhotepus: cMicroSecDuration * g_ClockSpeed overflowed.
	const uint64 dlCycles{ MulDivU64( cMicroSecDuration, g_ClockSpeed, 1000000 ) };
	const uint64 now{ CCycleCount::GetTimestamp() };
	// dimhotepus: Saturate, a wrapped limit gave an arbitrary deadline.
	m_lCycleLimit = dlCycles > kUint64Max - now ? kUint64Max : now + dlCycles;
}


//-----------------------------------------------------------------------------
// Purpose: Determines whether our specified time period has passed
// Output:	true if at least the specified time period has passed
//-----------------------------------------------------------------------------
inline bool CLimitTimer::BLimitReached() const
{
	return CCycleCount::GetTimestamp() >= m_lCycleLimit;
}


//-----------------------------------------------------------------------------
// Purpose: If we're over our specified time period, return the amount of the overage.
// Output:	# of microseconds since we reached our specified time period (saturates at INT_MAX).
//-----------------------------------------------------------------------------
inline int CLimitTimer::CMicroSecOverage() const
{
	const uint64 lcCycles{ CCycleCount::GetTimestamp() };

	if ( lcCycles < m_lCycleLimit )
		return 0;

	// dimhotepus: Overflow-safe conversion, saturate to INT_MAX instead of
	// wrapping to a negative int.
	const uint64 us{ s1::tier0::fasttimer_detail::MulDivU64( lcCycles - m_lCycleLimit, 1000000, g_ClockSpeed ) };
	return us > static_cast<uint64>( INT_MAX ) ? INT_MAX : static_cast<int>( us );
}


//-----------------------------------------------------------------------------
// Purpose: If we're under our specified time period, return the amount under.
// Output:	# of microseconds until we reached our specified time period, 0 if we've passed it
//-----------------------------------------------------------------------------
inline uint64 CLimitTimer::CMicroSecLeft() const
{
	const uint64 lcCycles{ CCycleCount::GetTimestamp() };

	if ( lcCycles >= m_lCycleLimit )
		return 0;

	// dimhotepus: Overflow-safe conversion, no division by zero before init.
	return s1::tier0::fasttimer_detail::MulDivU64( m_lCycleLimit - lcCycles, 1000000, g_ClockSpeed );
}


#endif  // !SE_PUBLIC_TIER0_FASTTIMER_H_
