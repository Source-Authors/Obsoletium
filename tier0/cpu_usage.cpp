// Copyright Valve Corporation, All rights reserved.
//
// Return the system-wide CPU usage as a value from 0.0 to 1.0, measured
// between the previous call and this one.
//
// Call this once in a while (every few seconds). The first call returns zero.
// Calls closer together than the OS clock granularity return the previous value.

#include "stdafx.h"

#include "tier0/platform.h"

#include <cstdint>
#include <mutex>

#if defined(WIN32)
#include "winlite.h"
#elif defined(POSIX)
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#if defined(OSX)
#include <mach/mach.h>
#elif defined(_LINUX)
#include <system_error>
#include <tuple>
#include "posix_file_stream.h"
#endif
#endif

namespace
{

// Cumulative CPU time summed over all processors, in OS-specific units.
struct CpuTimes
{
	uint64_t idle;
	uint64_t total;  // Includes idle.
};

#if defined(WIN32)

[[nodiscard]] constexpr uint64_t FileTimeToU64( const FILETIME& ft ) noexcept
{
	return ( static_cast<uint64_t>( ft.dwHighDateTime ) << 32U ) | ft.dwLowDateTime;
}

// GetSystemTimes is documented (XP SP1+), covers all processor groups and
// needs no processor count: the times are already summed over all CPUs.
[[nodiscard]] bool ReadCpuTimes( CpuTimes& out ) noexcept
{
	FILETIME idle, kernel, user;
	if ( !::GetSystemTimes( &idle, &kernel, &user ) )
	{
		return false;
	}

	// Kernel time includes idle time.
	out.idle = FileTimeToU64( idle );
	out.total = FileTimeToU64( kernel ) + FileTimeToU64( user );
	return true;
}

#elif defined(OSX)

[[nodiscard]] bool ReadCpuTimes( CpuTimes& out ) noexcept
{
	// mach_host_self() adds a send right on every call; take it once.
	static const mach_port_t host{ mach_host_self() };

	host_cpu_load_info_data_t info;
	mach_msg_type_number_t count{ HOST_CPU_LOAD_INFO_COUNT };
	if ( host_statistics( host, HOST_CPU_LOAD_INFO,
			reinterpret_cast<host_info_t>( &info ), &count ) != KERN_SUCCESS )
	{
		return false;
	}

	const uint64_t user{ info.cpu_ticks[CPU_STATE_USER] };
	const uint64_t system{ info.cpu_ticks[CPU_STATE_SYSTEM] };
	const uint64_t nice{ info.cpu_ticks[CPU_STATE_NICE] };
	const uint64_t idle{ info.cpu_ticks[CPU_STATE_IDLE] };

	out.idle = idle;
	out.total = user + system + nice + idle;
	return true;
}

#elif defined(_LINUX)

// First line of /proc/stat:
// cpu  user nice system idle iowait irq softirq steal guest guest_nice
// guest/guest_nice are already included in user/nice, so they are not summed.
[[nodiscard]] bool ReadCpuTimes( CpuTimes& out ) noexcept
{
	auto [f, errc] = s1::posix::posix_file_stream_factory::open( "/proc/stat", "r" );
	if ( errc )
	{
		return false;
	}

	char line[512];
	if ( std::get<std::error_code>( f.gets( line ) ) )
	{
		return false;
	}

	unsigned long long user = 0, nice = 0, system = 0, idle = 0,
		iowait = 0, irq = 0, softirq = 0, steal = 0;
	// Older kernels may report fewer fields; the first four always exist.
	const int fields{ std::sscanf( line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
		&user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal ) };
	if ( fields < 4 )
	{
		return false;
	}

	out.idle = idle + iowait;
	out.total = user + nice + system + idle + iowait + irq + softirq + steal;
	return true;
}

#else

[[nodiscard]] bool ReadCpuTimes( CpuTimes& ) noexcept
{
	return false;
}

#endif

#if defined(POSIX)
// Fallback when per-CPU times are unavailable: 1-minute load average
// normalized by the number of online CPUs and clamped to [0, 1].
// Approximate: load counts runnable (and on Linux, blocked-on-IO) tasks.
[[nodiscard]] float LoadAverageUsage() noexcept
{
	double loadavg[1];
	if ( getloadavg( loadavg, 1 ) < 1 )
	{
		return 0.0f;
	}

	const long cpus{ sysconf( _SC_NPROCESSORS_ONLN ) };
	const double usage{ loadavg[0] / static_cast<double>( cpus > 0 ? cpus : 1 ) };
	return static_cast<float>( usage < 0.0 ? 0.0 : usage > 1.0 ? 1.0 : usage );
}
#endif

}  // namespace

float GetCPUUsage()
{
	static std::mutex mutex;
	static CpuTimes previous{};
	static bool hasPrevious{ false };
	static float lastUsage{ 0.0f };

	CpuTimes current;
	if ( !ReadCpuTimes( current ) )
	{
#if defined(POSIX)
		return LoadAverageUsage();
#else
		return 0.0f;
#endif
	}

	std::scoped_lock lock{ mutex };

	if ( !hasPrevious )
	{
		// First call: no interval to measure yet.
		previous = current;
		hasPrevious = true;
		return 0.0f;
	}

	// Counters are monotonic; anything else (no tick elapsed, counter reset)
	// means there is no valid interval, so keep the previous sample and value.
	if ( current.total <= previous.total || current.idle < previous.idle )
	{
		return lastUsage;
	}

	const uint64_t totalDelta{ current.total - previous.total };
	const uint64_t idleDelta{ current.idle - previous.idle };

	lastUsage = idleDelta >= totalDelta
		? 0.0f
		: static_cast<float>( 1.0 - static_cast<double>( idleDelta ) / static_cast<double>( totalDelta ) );
	previous = current;

	return lastUsage;
}