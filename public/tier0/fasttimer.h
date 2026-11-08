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

	uint64			m_Int64;
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

	// Return number of cycles per second on this processor.
	// dimhotepus: int64 -> uint64.
	[[nodiscard]] static inline uint64 GetClockSpeed();

private:
	CCycleCount	m_Duration;
#ifdef DEBUG_FASTTIMER
	bool m_bRunning;		// Are we currently running?
#endif
};


// This is a helper class that times whatever block of code it's in
class CTimeScope
{
public:
				CTimeScope( CFastTimer *pTimer );
				~CTimeScope();

private:	
	CFastTimer	*m_pTimer;
};

inline CTimeScope::CTimeScope( CFastTimer *pTotal )
{
	m_pTimer = pTotal;
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

	void		End();

private:	
	CCycleCount	*m_pTotal;
	CFastTimer	m_Timer;
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

inline void CTimeAdder::End()
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
// program exit or at each iteration
//
//	Macros used because dbg.h uses this header, thus Msg() is unavailable
// -------------------------------------------------------------------------- // 

#define PROFILE_SCOPE(name) \
	class C##name##ACC : public CAverageCycleCounter \
	{ \
	public: \
		~C##name##ACC() \
		{ \
			Msg("%-48s: %6.3f avg (%8.1f total, %7.3f peak, %5d iters)\n",  \
				#name, \
				GetAverageMilliseconds(), \
				GetTotalMilliseconds(), \
				GetPeakMilliseconds(), \
				GetIters() ); \
		} \
	}; \
	static C##name##ACC name##_ACC; \
	CAverageTimeMarker name##_ATM( &name##_ACC )

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
		CFastTimer	m_Timer; \
	} name##_TSM;


// -------------------------------------------------------------------------- // 

class CAverageCycleCounter
{
public:
	CAverageCycleCounter();
	
	void Init();
	void MarkIter( const CCycleCount &duration );
	
	[[nodiscard]] unsigned GetIters() const;
	
	[[nodiscard]] double GetAverageMilliseconds() const;
	[[nodiscard]] double GetTotalMilliseconds() const;
	[[nodiscard]] double GetPeakMilliseconds() const;

private:
	unsigned	m_nIters{ 0 };
	CCycleCount m_Total;
	CCycleCount	m_Peak;
};

// -------------------------------------------------------------------------- // 

class CAverageTimeMarker
{
public:
	explicit CAverageTimeMarker( CAverageCycleCounter *pCounter );
	~CAverageTimeMarker();
	
private:
	CAverageCycleCounter *m_pCounter;
	CFastTimer	m_Timer;
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
	// dimhotepus: Adjust by half of measurement overhead as we measure between
	// 2 points.
	uint32          coreId;
	// dimhotepus: Marked volatile to prevent optimizer from caching RDTSCP result
	volatile uint64 timestamp = Plat_Rdtscp( coreId );
	m_Int64 = timestamp - ( g_ClockRtscpOverhead / 2 );
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
	CCycleCount c;
	c.Sample();
	return c.GetLongCycles();
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
	// dimhotepus: Added compiler barrier to prevent reordering of timed code
	std::atomic_thread_fence( std::memory_order_release );
	m_Duration.Sample();
	std::atomic_thread_fence( std::memory_order_acquire );

#ifdef DEBUG_FASTTIMER
	m_bRunning = true;
#endif
}


inline void CFastTimer::End()
{
	// dimhotepus: Added acquire barrier before sampling to prevent hoisting
	std::atomic_thread_fence( std::memory_order_release );
	CCycleCount cnt;
	cnt.Sample();
	std::atomic_thread_fence( std::memory_order_acquire );

	m_Duration.m_Int64 = cnt.m_Int64 - m_Duration.m_Int64;

#ifdef DEBUG_FASTTIMER
	m_bRunning = false;
#endif
}

inline CCycleCount CFastTimer::GetDurationInProgress() const
{
	CCycleCount cnt;
	cnt.Sample();

	CCycleCount result;
	result.m_Int64 = cnt.m_Int64 - m_Duration.m_Int64;
	
	return result;
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

inline CAverageCycleCounter::CAverageCycleCounter() = default;

inline void CAverageCycleCounter::Init()
{
	m_Total.Init();
	m_Peak.Init();
	m_nIters = 0;
}

inline void CAverageCycleCounter::MarkIter( const CCycleCount &duration )
{
	++m_nIters;
	m_Total += duration;
	if ( m_Peak.IsLessThan( duration ) )
		m_Peak = duration;
}

inline unsigned CAverageCycleCounter::GetIters() const
{
	return m_nIters;
}

inline double CAverageCycleCounter::GetAverageMilliseconds() const
{
	if ( m_nIters )
		return (m_Total.GetMillisecondsF() / static_cast<double>(m_nIters));
	else
		return 0;
}

inline double CAverageCycleCounter::GetTotalMilliseconds() const
{
	return m_Total.GetMillisecondsF();
}

inline double CAverageCycleCounter::GetPeakMilliseconds() const
{
	return m_Peak.GetMillisecondsF();
}

// -------------------------------------------------------------------------- // 

inline CAverageTimeMarker::CAverageTimeMarker( CAverageCycleCounter *pCounter )
{
	m_pCounter = pCounter;
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
	uint64 m_lCycleLimit;
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
