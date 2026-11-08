// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"

#include "tier0/fasttimer.h"

#include <chrono>  // std::steady_clock
#include <mutex>   // std::once_flag, std::call_once

#if defined( _MSC_VER ) && !defined( __clang__ )
MSVC_BEGIN_WARNING_OVERRIDE_SCOPE();
MSVC_DISABLE_WARNING(4073); // initializers put in library initialization area
#pragma init_seg( lib )
MSVC_END_WARNING_OVERRIDE_SCOPE();
#endif

// g_dwClockSpeed is only exported for backwards compatibility.
PLATFORM_INTERFACE unsigned long g_dwClockSpeed;

// dimhotepus: Clocks/sec -> counter ticks/sec, not always the CPU clock.
uint64 g_ClockSpeed{ 0 }; // Counter ticks/sec
// Storing CPU clock speed in a 32-bit variable is dangerous and can already overflow
// on some CPUs. This variable is deprecated.
unsigned long g_dwClockSpeed{ 0 };
double        g_ClockSpeedMicrosecondsMultiplier{ 0.0 };
double        g_ClockSpeedMillisecondsMultiplier{ 0.0 };
double        g_ClockSpeedSecondsMultiplier{ 0.0 };

uint64 g_ClockRtscpOverhead{ 0 };

namespace
{

// dimhotepus: Use rate of the counter Plat_Rdtscp() actually reads instead of
// CPUInformation::m_Speed everywhere.  This is what every tick <-> time
// conversion needs, and it is not the CPU core clock on ARM64 or on the
// generic steady_clock fallback.  Returns 0 if unknown.
[[nodiscard]] uint64 QueryCounterFrequency()
{
#if defined( PLATFORM_INTEL )
	// TSC.  CPUInformation::m_Speed is the measured TSC rate.
	// dimhotepus: Reject non-positive m_Speed instead of wrapping int64 -> uint64.
	const int64 speed{ GetCPUInformation()->m_Speed };
	return speed > 0 ? static_cast< uint64 >( speed ) : 0;
#elif defined( PLATFORM_ARM_64 )
	// CNTVCT_EL0 ticks at CNTFRQ_EL0 (typically 19.2 MHz .. 1 GHz).
#if defined( _MSC_VER ) && !defined( __clang__ )
	// ARM64_SYSREG(3, 3, 14, 0, 0) == CNTFRQ_EL0.
	return static_cast< uint64 >( _ReadStatusReg( 0x5F00 ) );
#else
	uint64 freq{ 0 };
	__asm__ __volatile__( "mrs %0, cntfrq_el0" : "=r"( freq ) );
	return freq;
#endif
#else
	// Plat_Rdtscp() returns std::chrono::steady_clock ticks.
	using Period = std::chrono::steady_clock::period;
	static_assert( Period::num == 1, "steady_clock tick must be 1/N seconds" );
	return static_cast< uint64 >( Period::den );
#endif
}

// dimhotepus: Fallback when the platform does not report the counter rate:
// measure it against steady_clock.  Spins ~20 ms, so only used when the query
// fails.
[[nodiscard]] uint64 CalibrateCounterFrequency()
{
	using Clock = std::chrono::steady_clock;

	uint32                  coreId{ 0 };
	const Clock::time_point t0{ Clock::now() };
	const uint64            c0{ Plat_Rdtscp( coreId ) };

	Clock::time_point t1{};
	do
	{
		t1 = Clock::now();
	} while ( t1 - t0 < std::chrono::milliseconds( 20 ) );

	const uint64 c1{ Plat_Rdtscp( coreId ) };
	const double seconds{ std::chrono::duration< double >( t1 - t0 ).count() };

	// seconds >= 0.02, so the quotient is < 2^64 and the cast is defined.
	return c1 > c0 && seconds > 0.0 ? static_cast< uint64 >( static_cast< double >( c1 - c0 ) / seconds ) : 0;
}

} // namespace

// Constructor init the clock speed.
static CClockSpeedInit g_ClockSpeedInit CONSTRUCT_EARLY{};

void CClockSpeedInit::Init()
{
	// dimhotepus: Make Init() idempotent: extra or concurrent calls never
	// rewrite the globals while other threads read them.
	static std::once_flag s_initOnce{};

	std::call_once( s_initOnce, []
	{
		// dimhotepus: Real counter rate, calibrate when unknown.
		uint64 freq{ QueryCounterFrequency() };
		if ( freq == 0 )
			freq = CalibrateCounterFrequency();

		AssertMsg( freq != 0, "Unable to determine timer counter frequency.\n" );

		g_ClockSpeed = freq;

		// Avoid integer overflow when writing to g_dwClockSpeed
		g_dwClockSpeed = s1::tier0::fasttimer_detail::SaturateToULong( freq );

		const double clockSpeed{ static_cast< double >( freq ) };
		// dimhotepus: Leave the multipliers at 0 rather than +inf if the rate
		// is unknown: timings then read as 0 instead of inf / NaN.
		const double clockSpeedReciprocal = freq ? 1.0 / clockSpeed : 0.0;

		g_ClockSpeedMicrosecondsMultiplier = 1000000.0 * clockSpeedReciprocal;
		g_ClockSpeedMillisecondsMultiplier = 1000.0 * clockSpeedReciprocal;
		g_ClockSpeedSecondsMultiplier = 1.0 * clockSpeedReciprocal;

		g_ClockRtscpOverhead = Plat_MeasureRtscpOverhead();
	} );
}
