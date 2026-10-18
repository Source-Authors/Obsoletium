// Copyright Valve Corporation, All rights reserved.
//
// Determine CPU speed under Linux / macOS.

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <system_error>
#include <tuple>

#include "tier0/platform.h"
#include "posix_file_stream.h"

#ifdef OSX
#include <sys/sysctl.h>
#endif

#if defined(__x86_64__) || defined(__i386__)
#define PLAT_HAS_TSC 1
#endif

namespace
{

// Parses a non-negative decimal integer. Returns 0 on any error.
uint64 ParseUint64( const char* text )
{
	if ( !text ) return 0;

	while ( std::isspace( static_cast<unsigned char>( *text ) ) ) ++text;

	// strtoull silently accepts and negates a leading '-'.
	if ( !std::isdigit( static_cast<unsigned char>( *text ) ) ) return 0;

	errno = 0;
	char* end = nullptr;
	const unsigned long long value{ std::strtoull( text, &end, 10 ) };
	if ( errno == ERANGE || end == text ) return 0;

	return static_cast<uint64>( value );
}

#ifdef PLAT_HAS_TSC

uint64 MonotonicNs()
{
	timespec ts;
#ifdef CLOCK_MONOTONIC_RAW
	// Not slewed by NTP.
	clock_gettime( CLOCK_MONOTONIC_RAW, &ts );
#else
	clock_gettime( CLOCK_MONOTONIC, &ts );
#endif
	return static_cast<uint64>( ts.tv_sec ) * 1000000000ULL + static_cast<uint64>( ts.tv_nsec );
}

// Measures the TSC rate in Hz against the monotonic clock: the same quantity
// the Windows path measures, and what rdtsc-based timers need.
// Takes a handful of short samples and returns the median, which is robust
// against preemption without needing all samples to agree.
uint64 MeasureTscFrequency()
{
	constexpr int kSamples{ 5 };
	constexpr long kSampleNs{ 5 * 1000 * 1000 };  // 5 ms

	double samples[kSamples];
	int count{ 0 };
	uint32 coreId;

	for ( int attempt = 0; attempt < kSamples * 2 && count < kSamples; ++attempt )
	{
		const uint64 startNs{ MonotonicNs() };
		const uint64 startTsc{ Plat_Rdtsc() };

		const timespec sleepFor{ 0, kSampleNs };
		nanosleep( &sleepFor, nullptr );  // Early wakeup (EINTR) is fine: we measure.

		const uint64 endNs{ MonotonicNs() };
		const uint64 endTsc{ Plat_Rdtscp(coreId) };

		if ( endNs <= startNs || endTsc <= startTsc )
		{
			continue;
		}

		samples[count++] =
			static_cast<double>( endTsc - startTsc ) * 1e9 / static_cast<double>( endNs - startNs );
	}

	if ( count < 3 )
	{
		return 0;
	}

	std::sort( samples, samples + count );
	const uint64 median{ static_cast<uint64>( samples[count / 2] ) };

	// Some old CPUs have broken TSCs that increment very slowly or unevenly.
	constexpr uint64 kMinPlausibleHz{ 10000000 };
	return median >= kMinPlausibleHz ? median : 0;
}

#endif  // PLAT_HAS_TSC

#ifdef OSX

uint64 GetCPUFreqFromOS()
{
	uint64_t frequency{ 0 };
	size_t len{ sizeof( frequency ) };

	// Only for Intel Macs. M1+ have multiple CPUs with different frequency.
	if ( sysctlbyname( "hw.cpufrequency", &frequency, &len, nullptr, 0 ) == 0 && frequency )
	{
		return frequency;
	}

	// Assume 2.4 GHz for M1+ for now.
	return 2400000000ULL;
}

#else

// cpufreq reports kHz. Fails if the kernel has no CPU scaling support
// (common in VMs and containers).
uint64 GetCPUFreqFromSysfs()
{
	auto [f, errc] = s1::posix::posix_file_stream_factory::open(
		"/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r" );
	if ( errc )
	{
		return 0;
	}

	char buf[64]{};
	if ( std::get<std::error_code>( f.gets( buf ) ) )
	{
		return 0;
	}

	const uint64 khz{ ParseUint64( buf ) };
	return khz <= UINT64_MAX / 1000 ? khz * 1000 : 0;
}

// "cpu MHz" from /proc/cpuinfo: the *current* frequency of the first CPU,
// so it varies with scaling. Last resort only. Absent on most ARM kernels.
uint64 GetCPUFreqFromProc()
{
	auto [f, errc] = s1::posix::posix_file_stream_factory::open( "/proc/cpuinfo", "r" );
	if ( errc )
	{
		return 0;
	}

	char line[1024];
	while ( !std::get<std::error_code>( f.gets( line ) ) )
	{
		if ( std::strncmp( line, "cpu MHz", 7 ) != 0 )
		{
			continue;
		}

		const char* colon{ std::strchr( line, ':' ) };
		double mhz{ 0 };
		if ( colon && std::sscanf( colon + 1, "%lf", &mhz ) == 1 && mhz > 0 )
		{
			return static_cast<uint64>( mhz * 1000000.0 );
		}
	}

	return 0;
}

uint64 GetCPUFreqFromOS()
{
	const uint64 sysfs{ GetCPUFreqFromSysfs() };
	return sysfs ? sysfs : GetCPUFreqFromProc();
}

#endif  // OSX

}  // namespace

// Returns the CPU (TSC) frequency in Hz, or 0 if it could not be determined.
uint64 CalculateCPUFreq()
{
#ifdef _LINUX
	// Manual override, in MHz.
	if ( const char* cpuMhz{ std::getenv( "CPU_MHZ" ) } )
	{
		constexpr uint64 kMaxMhz{ 1000000 };  // 1 THz, sanity cap.
		const uint64 mhz{ ParseUint64( cpuMhz ) };
		if ( mhz > 0 && mhz <= kMaxMhz )
		{
			return mhz * 1000000;
		}
		// Invalid value: ignore and detect normally.
	}
#endif

#ifdef PLAT_HAS_TSC
	if ( const uint64 measured{ MeasureTscFrequency() } )
	{
		return measured;
	}
#endif

	return GetCPUFreqFromOS();
}