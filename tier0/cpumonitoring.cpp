// Copyright (c) Valve Corporation, All rights reserved.
//
// A non-trivial number of Valve customers hit performance problems because their CPUs overheat
// and are thermally throttled. While thermal throttling is better than melting it is still a
// hardware flaw and it leads to a bad user experience. In some cases the CPU frequency drops
// (constantly or occasionally) by 50-75%, leading to equal or greater framerate drops.
//
// This is equivalent to a car that goes into limp-home mode to let it continue running after the
// radiator fails -- it's better than destroying the engine, but clearly it needs to be fixed.
//
// When CPU monitoring is enabled a bunch of background threads are created that wake up at
// the set frequency, spin in a loop to measure the actual usable CPU frequency, then sleep again.
// A delay loop is used to measure the frequency because this is portable (it works for Intel,
// AMD and ARM and handles both frequency throttling and duty-cycle reductions) and it doesn't
// require administrator privileges. This technique has been used in VTrace for a while.
//
// NOTE: the TSC cannot be used for this. On every CPU with an invariant TSC it ticks at a
// constant rate regardless of the actual core clock, so it never shows throttling.
//
// m_percentage is relative to the clock the OS reports:
//   Windows:     CallNtPowerInformation CurrentMhz (usually the nominal clock).
//   Linux:       cpufreq base_frequency, else cpuinfo_max_freq (may include boost).
//   macOS Intel: hw.cpufrequency (nominal clock).
// When the OS reports nothing (Apple Silicon, Linux without cpufreq) it is relative to the
// highest frequency measured since monitoring was enabled.
//
// Everything is standard C++17 except what the standard library cannot express:
//   - thread affinity and priority (Win32 / pthread / QoS), applied by each thread to itself;
//   - the OS-reported clock (CallNtPowerInformation / sysfs / sysctl);
//   - the CPU list (current processor group on Windows, process affinity on Linux);
//   - ThreadSetDebugName and Plat_FloatTime, kept so names and timestamps match the engine.
//
// Platform notes:
//   Windows: one thread per CPU of the current processor group, pinned, high priority.
//   Linux:   one thread per CPU the process may run on, pinned, normal priority
//            (raising priority needs CAP_SYS_NICE).
//   macOS:   one unpinned thread per logical CPU at user-interactive QoS, which steers them
//            to performance cores on Apple Silicon. macOS has no hard thread affinity.
//
// This code doesn't use normal worker threads because of the special purpose nature of this
// work. The threads are started on demand and are never terminated, in order to simplify
// the code.

#include "stdafx.h"
#include "tier0/cpumonitoring.h"

#if defined(PLATFORM_WINDOWS_PC)
#define CPUMON_WINDOWS 1
#elif defined(LINUX) || defined(_LINUX)
#define CPUMON_LINUX 1
#elif defined(OSX)
#define CPUMON_MAC 1
#endif

#include "tier0/threadtools.h"  // ThreadSetDebugName

#include <algorithm>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#if defined(CPUMON_WINDOWS)
#include "winlite.h"
#include <powrprof.h>
#pragma comment(lib, "PowrProf.lib")
#elif defined(CPUMON_LINUX)
#include <fstream>
#include <pthread.h>
#include <sched.h>
#else
#include <pthread.h>
#include <sys/qos.h>
#include <sys/sysctl.h>
#endif

namespace {

constexpr unsigned kPostMeasureMs = 5;
constexpr unsigned kMinimumDelayMs = 300;

constexpr int kMaxCPUs = 128;

// State shared between the heartbeat thread and callers.
struct SharedState
{
	// This lock protects everything below.
	std::mutex lock;
	// Wakes the heartbeat when delayMs changes.
	std::condition_variable settingsChanged;
	CPUFrequencyResults results{};
	// Highest frequency measured since monitoring was (re)enabled. Baseline for
	// m_percentage when the OS does not report a clock.
	double peakHz = 0;
	// Interval between measurements; 0 means disabled.
	unsigned delayMs = 0;
	// Bumped on every SetCPUMonitoringInterval call.
	std::uint64_t settingsVersion = 0;
	// Set when monitoring is (re-)enabled: measure without waiting for the interval.
	bool measureNow = false;
};

// Detached threads never terminate and keep using this state during and after
// static destruction, so it is intentionally leaked rather than a static object.
[[nodiscard]] SharedState& Shared()
{
	static SharedState* state = new SharedState;
	return *state;
}

// ---------------------------------------------------------------------------
// Measurement
// ---------------------------------------------------------------------------

// The delay loop: a chain of dependent add/xor pairs. Both instructions have 1-cycle latency
// on every x86 and ARM64 core, so the chain runs at exactly one op per cycle at the current
// core clock, throttled or not. Add and xor are mixed so a compiler cannot fold the chain
// into a closed form (a pure add chain is linear and collapses into multiplies).
//
// The chain must stay in registers. Compiled without optimization, every op goes through
// the stack and costs a store-to-load forward (~5 cycles), under-reporting the clock ~5x.
// So the loop is written in assembly wherever the compiler allows it, and the C++
// fallback is forced to be optimized.
constexpr unsigned kDelayLoops = 50000;
constexpr unsigned kOpsPerLoop = 32;

#if ( defined(__GNUC__) || defined(__clang__) ) && ( defined(__x86_64__) || defined(__i386__) )

#define CPUMON_X2( s ) s s
#define CPUMON_X4( s ) CPUMON_X2( s ) CPUMON_X2( s )
#define CPUMON_X16( s ) CPUMON_X4( CPUMON_X4( s ) )

// GCC / Clang / clang-cl on x86: native-width registers, operand size inferred.
void RunDelayLoop()
{
	std::uintptr_t a = 1, b = 1, n = kDelayLoops;
	// "memory" keeps the loop between the surrounding clock reads.
	__asm__ __volatile__(
		"1:\n\t"
		CPUMON_X16( "add %[b], %[a]\n\t" "xor %[a], %[b]\n\t" )
		"sub $1, %[n]\n\t"
		"jnz 1b"
		: [a] "+r"( a ), [b] "+r"( b ), [n] "+r"( n )
		:
		: "cc", "memory" );
}

#elif ( defined(__GNUC__) || defined(__clang__) ) && defined(__aarch64__)

#define CPUMON_X2( s ) s s
#define CPUMON_X4( s ) CPUMON_X2( s ) CPUMON_X2( s )
#define CPUMON_X16( s ) CPUMON_X4( CPUMON_X4( s ) )

// GCC / Clang on ARM64 (Linux, Apple Silicon).
void RunDelayLoop()
{
	std::uintptr_t a = 1, b = 1, n = kDelayLoops;
	__asm__ __volatile__(
		"1:\n\t"
		CPUMON_X16( "add %[a], %[a], %[b]\n\t" "eor %[b], %[b], %[a]\n\t" )
		"subs %[n], %[n], #1\n\t"
		"b.ne 1b"
		: [a] "+r"( a ), [b] "+r"( b ), [n] "+r"( n )
		:
		: "cc", "memory" );
}

#elif defined(_MSC_VER) && defined(_M_IX86)

#define CPUMON_MSVC_STEP __asm add eax, edx __asm xor edx, eax
#define CPUMON_MSVC_X2   CPUMON_MSVC_STEP CPUMON_MSVC_STEP
#define CPUMON_MSVC_X4   CPUMON_MSVC_X2 CPUMON_MSVC_X2
#define CPUMON_MSVC_X16  CPUMON_MSVC_X4 CPUMON_MSVC_X4 CPUMON_MSVC_X4 CPUMON_MSVC_X4

// MSVC x86 inline assembly.
void RunDelayLoop()
{
	__asm
	{
		mov eax, 1
		mov edx, 1
		mov ecx, kDelayLoops
	cpumon_top:
	}
	CPUMON_MSVC_X16
	__asm
	{
		dec ecx
		jnz cpumon_top
	}
}
#undef CPUMON_MSVC_STEP

#else

#define CPUMON_X2( s ) s s
#define CPUMON_X4( s ) CPUMON_X2( s ) CPUMON_X2( s )
#define CPUMON_X16( s ) CPUMON_X4( CPUMON_X4( s ) )

// MSVC x64 / ARM64 have no inline assembly. Force this function to be optimized even in
// debug builds. If your toolchain ignores this (verify in the disassembly), build this file
// with /O2 and without /RTC.
#if defined(_MSC_VER)
#pragma optimize( "gt", on )
#endif

// Keeps the chain observable so it is not optimized away or moved.
volatile std::uint64_t g_delayLoopSink;

void RunDelayLoop()
{
	std::uint64_t a = g_delayLoopSink | 1U;
	std::uint64_t b = a;

	for ( unsigned i = 0; i < kDelayLoops; ++i )
	{
		CPUMON_X16( a += b; b ^= a; )
		// Off the critical path; pins the loop between the two clock reads.
		g_delayLoopSink = a;
	}

	g_delayLoopSink = a + b;
}

#if defined(_MSC_VER)
#pragma optimize( "", on )
#endif

#endif

#undef CPUMON_X16
#undef CPUMON_X4
#undef CPUMON_X2

// Runs the delay loop once and returns the core clock it ran at, in Hz.
[[nodiscard]] std::int64_t MeasureDelayLoopFrequency()
{
	using Clock = std::chrono::steady_clock;

	const auto start = Clock::now();
	RunDelayLoop();
	const auto end = Clock::now();

	const auto elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>( end - start ).count();
	if ( elapsedNs <= 0 )
	{
		return 0;
	}

	constexpr std::uint64_t kCycles = std::uint64_t{ kDelayLoops } * kOpsPerLoop;
	return static_cast<std::int64_t>( kCycles * 1000000000ULL / static_cast<std::uint64_t>( elapsedNs ) );
}

// Measures the CPU frequency until it stops rising, and returns the highest sample.
//
// A core that was idle runs at a low clock and needs time to ramp up after the thread wakes
// (about 1 ms with hardware-managed P-states, tens of ms when the OS manages them), so a fixed
// handful of 0.3 ms samples mostly measures the ramp. Sampling stops once kStableSamples in a
// row fail to beat the best by more than 1%, or after kMaxSampling.
//
// The best sample wins because any given sample might be slowed by interrupts or context
// switches, so the fastest run should indicate the true performance.
[[nodiscard]] std::int64_t GetSampledFrequency()
{
	using Clock = std::chrono::steady_clock;

	// Seven seems like a good number -- it makes it likely that a couple of the
	// samples will not hit any interrupts.
	constexpr int kStableSamples = 7;
	constexpr auto kMaxSampling = std::chrono::milliseconds( 50 );

	const auto deadline = Clock::now() + kMaxSampling;

	std::int64_t best = 0;
	int sinceImprovement = 0;

	while ( sinceImprovement < kStableSamples && Clock::now() < deadline )
	{
		const std::int64_t frequency = MeasureDelayLoopFrequency();

		// More than 1% above the best: still ramping up.
		if ( frequency > best + best / 100 )
		{
			sinceImprovement = 0;
		}
		else
		{
			++sinceImprovement;
		}

		best = std::max( best, frequency );
	}

	return best;
}

// ---------------------------------------------------------------------------
// Platform: thread placement (no standard equivalent)
// ---------------------------------------------------------------------------

// Pins the calling thread to cpu (if >= 0) and raises its priority/QoS.
void ConfigureCurrentThread( [[maybe_unused]] int cpu )
{
#if defined(CPUMON_WINDOWS)
	if ( cpu >= 0 )
	{
		::SetThreadAffinityMask( ::GetCurrentThread(), DWORD_PTR{ 1 } << cpu );
	}
	::SetThreadPriority( ::GetCurrentThread(), THREAD_PRIORITY_HIGHEST );
#elif defined(CPUMON_LINUX)
	// Priority is left alone: raising it needs CAP_SYS_NICE.
	if ( cpu >= 0 )
	{
		cpu_set_t set;
		CPU_ZERO( &set );
		CPU_SET( cpu, &set );
		pthread_setaffinity_np( pthread_self(), sizeof( set ), &set );
	}
#else
	// No hard affinity on macOS. User-interactive QoS is the highest class an
	// app can use and steers threads to performance cores on Apple Silicon.
	pthread_set_qos_class_self_np( QOS_CLASS_USER_INTERACTIVE, 0 );
#endif
}

// Returns the logical CPUs to measure (-1 = unpinned).
[[nodiscard]] std::vector<int> EnumerateCpus()
{
	std::vector<int> cpus;

#if defined(CPUMON_WINDOWS)
	// CPUs of the current processor group: a thread affinity mask cannot address
	// more than that (32 CPUs in a 32-bit process).
	SYSTEM_INFO systemInfo;
	::GetSystemInfo( &systemInfo );

	const int winCpuCount = std::min( { static_cast<int>( systemInfo.dwNumberOfProcessors ),
		static_cast<int>( sizeof( DWORD_PTR ) * CHAR_BIT ), kMaxCPUs } );
	for ( int cpu = 0; cpu < winCpuCount; ++cpu )
	{
		cpus.push_back( cpu );
	}
#elif defined(CPUMON_LINUX)
	// Only CPUs this process may run on (respects taskset / cpusets).
	cpu_set_t allowed;
	CPU_ZERO( &allowed );
	if ( sched_getaffinity( 0, sizeof( allowed ), &allowed ) == 0 )
	{
		for ( int cpu = 0; cpu < CPU_SETSIZE && static_cast<int>( cpus.size() ) < kMaxCPUs; ++cpu )
		{
			if ( CPU_ISSET( cpu, &allowed ) )
			{
				cpus.push_back( cpu );
			}
		}
	}
#endif

	if ( cpus.empty() )
	{
		// macOS, or the platform query failed: unpinned threads.
		const int macCpuCount = std::clamp( static_cast<int>( std::thread::hardware_concurrency() ), 1, kMaxCPUs );
		cpus.assign( static_cast<size_t>( macCpuCount ), -1 );
	}

	return cpus;
}

// ---------------------------------------------------------------------------
// Platform: reported clock (no standard equivalent)
// ---------------------------------------------------------------------------

#if defined(CPUMON_WINDOWS)

/*
Note that this structure definition was accidentally omitted from WinNT.h. This error will be
corrected in the future. In the meantime, to compile your application, include the structure
definition contained in this topic in your source code.

See https://learn.microsoft.com/en-us/windows/win32/power/processor-power-information-str
*/
struct PROCESSOR_POWER_INFORMATION {
	ULONG Number;
	ULONG MaxMhz;
	ULONG CurrentMhz;
	ULONG MhzLimit;
	ULONG MaxIdleState;
	ULONG CurrentIdleState;
};

class ReportedClock
{
public:
	explicit ReportedClock( const std::vector<int>& )
		// CallNtPowerInformation needs room for every processor in the system, or it fails.
		: m_powerInfo( ::GetActiveProcessorCount( ALL_PROCESSOR_GROUPS ) )
	{
	}

	// Highest clock Windows reports across all processors, in Hz, or 0 on failure.
	// Queried every round: it can change with the power plan.
	[[nodiscard]] double QueryHz()
	{
		if ( m_powerInfo.empty() )
		{
			return 0;
		}

		const auto size = static_cast<ULONG>( m_powerInfo.size() * sizeof( m_powerInfo[0] ) );
		if ( ::CallNtPowerInformation( ProcessorInformation, nullptr, 0, m_powerInfo.data(), size ) != 0 )
		{
			return 0;
		}

		ULONG maxMhz = 0;
		for ( const auto& info : m_powerInfo )
		{
			maxMhz = std::max( maxMhz, info.CurrentMhz );
		}
		return static_cast<double>( maxMhz ) * 1e6;
	}

private:
	std::vector<PROCESSOR_POWER_INFORMATION> m_powerInfo;
};

#elif defined(CPUMON_LINUX)

class ReportedClock
{
public:
	// Static on Linux, so it is read once. Highest across measured CPUs: on hybrid
	// CPUs that is a performance core, matching the max-across-threads measurement.
	explicit ReportedClock( const std::vector<int>& cpus )
	{
		std::uint64_t maxKhz = 0;
		for ( const int cpu : cpus )
		{
			if ( cpu < 0 )
			{
				continue;
			}

			// base_frequency (intel_pstate) is the nominal clock; cpuinfo_max_freq
			// is the hardware maximum and may include boost.
			std::uint64_t khz = ReadCpufreqKhz( cpu, "base_frequency" );
			if ( !khz )
			{
				khz = ReadCpufreqKhz( cpu, "cpuinfo_max_freq" );
			}
			maxKhz = std::max( maxKhz, khz );
		}

		m_hz = static_cast<double>( maxKhz ) * 1e3;
	}

	[[nodiscard]] double QueryHz() const { return m_hz; }

private:
	// Reads a cpufreq value (kHz) for one CPU, or 0 if unavailable.
	[[nodiscard]] static std::uint64_t ReadCpufreqKhz( int cpu, const char* file )
	{
		std::ifstream in{ "/sys/devices/system/cpu/cpu" + std::to_string( cpu ) + "/cpufreq/" + file };
		std::uint64_t khz = 0;
		return ( in >> khz ) ? khz : 0;
	}

	double m_hz = 0;
};

#else  // CPUMON_MAC

class ReportedClock
{
public:
	// Intel Macs only. Apple Silicon has no single nominal clock and reports nothing.
	explicit ReportedClock( const std::vector<int>& )
	{
		std::uint64_t hz = 0;
		size_t len = sizeof( hz );
		if ( sysctlbyname( "hw.cpufrequency", &hz, &len, nullptr, 0 ) == 0 )
		{
			m_hz = static_cast<double>( hz );
		}
	}

	[[nodiscard]] double QueryHz() const { return m_hz; }

private:
	double m_hz = 0;
};

#endif

// ---------------------------------------------------------------------------
// Monitor
// ---------------------------------------------------------------------------

class CPUMonitor
{
public:
	// One-time thread setup. Returns false if monitoring cannot run.
	[[nodiscard]] bool Start()
	{
		const std::vector<int> cpus = EnumerateCpus();
		m_reportedClock.emplace( cpus );

		// Fixed size before any thread starts: threads hold references into it.
		m_slots.resize( cpus.size() );

		int started = 0;
		for ( size_t i = 0; i < cpus.size(); ++i )
		{
			Slot& slot = m_slots[ static_cast<size_t>( started ) ];
			slot.index = static_cast<int>( i );
			slot.cpu = cpus[i];

			// Only count threads that actually run, so the heartbeat never waits for a
			// completion that cannot come.
			if ( StartDetached( [this, &slot] { MeasureThread( slot ); } ) )
			{
				++started;
			}
		}

		{
			std::lock_guard<std::mutex> lock{ m_syncMutex };
			m_numSlots = started;
		}

		if ( started == 0 )
		{
			return false;
		}

		// Create the thread which tells the measurement threads to wake up periodically.
		return StartDetached( [this] { HeartbeatThread(); } );
	}

private:
	struct Slot
	{
		std::int64_t frequency = 0;  // Guarded by m_syncMutex.
		int index = 0;               // For the thread name.
		int cpu = -1;                // Logical CPU to pin to, or -1 for unpinned.
	};

	template <typename F>
	[[nodiscard]] static bool StartDetached( F&& fn ) noexcept
	{
		try
		{
			std::thread( std::forward<F>( fn ) ).detach();
			return true;
		}
		catch ( ... )
		{
			// std::system_error (no resources) or std::bad_alloc.
			return false;
		}
	}

	// Measurement thread, one per logical CPU.
	void MeasureThread( Slot& slot )
	{
		ConfigureCurrentThread( slot.cpu );

		// dimhotepus: Add thread name to aid debugging.
		const std::string threadName = "CPUBurnMon#" + std::to_string( slot.index );
		ThreadSetDebugName( threadName.c_str() );

		std::uint64_t seenGeneration = 0;
		for ( ; ; )
		{
			// Wait until the heartbeat says it's time to measure. The generation
			// counter guarantees exactly one measurement per thread per round.
			{
				std::unique_lock<std::mutex> lock{ m_syncMutex };
				m_wakeCv.wait( lock, [&] { return m_generation != seenGeneration; } );
				seenGeneration = m_generation;
			}

			const std::int64_t frequency = GetSampledFrequency();

			// Tell the heartbeat thread that one thread has completed.
			{
				std::lock_guard<std::mutex> lock{ m_syncMutex };
				slot.frequency = frequency;
				if ( --m_pending == 0 )
				{
					m_doneCv.notify_one();
				}
			}
		}
	}

	// Runs one measurement round on every thread; returns the highest frequency in Hz.
	[[nodiscard]] std::int64_t MeasureRound()
	{
		int numSlots;
		{
			std::lock_guard<std::mutex> lock{ m_syncMutex };
			numSlots = m_numSlots;
			m_pending = numSlots;
			++m_generation;
		}
		m_wakeCv.notify_all();

		// Avoid having the heartbeat thread fighting for cycles. Not strictly necessary.
		ThreadSleep( kPostMeasureMs );

		// Wait for all of the measurement threads to finish.
		std::unique_lock<std::mutex> lock{ m_syncMutex };
		m_doneCv.wait( lock, [this] { return m_pending == 0; } );

		std::int64_t maxActualFreq = 0;
		for ( int i = 0; i < numSlots; ++i )
		{
			maxActualFreq = std::max( maxActualFreq, m_slots[ static_cast<size_t>( i ) ].frequency );
		}
		return maxActualFreq;
	}

	// Master control thread to periodically wake the measurement threads.
	void HeartbeatThread()
	{
		// dimhotepus: Add thread name to aid debugging.
		ThreadSetDebugName( "CPUBurnHeartbeat" );

		ReportedClock& reportedClock = *m_reportedClock;
		SharedState& shared = Shared();

		for ( ; ; )
		{
			// While disabled, block until monitoring is enabled again: no periodic
			// wakeups and no load. Measurement threads are already blocked on m_wakeCv.
			{
				std::unique_lock<std::mutex> lock{ shared.lock };
				shared.settingsChanged.wait( lock, [&] { return shared.delayMs != 0; } );
				shared.measureNow = false;
			}

			const double reportedHz = reportedClock.QueryHz();
			const std::int64_t maxActualFreq = MeasureRound();

			// Read and write all the state that is shared with the main thread while holding the lock.
			std::unique_lock<std::mutex> lock{ shared.lock };

			// TODO: kFudgeFactor was tuned for the old measurement; recalibrate.
			constexpr double kFudgeFactor = 1.03;	// Make results match reality better
			const double actualHz = static_cast<double>( maxActualFreq ) * kFudgeFactor;

			shared.peakHz = std::max( shared.peakHz, actualHz );
			const double baselineHz = reportedHz > 0 ? reportedHz : shared.peakHz;

			shared.results.m_timeStamp = Plat_FloatTime();
			shared.results.m_GHz = static_cast<float>( actualHz / 1e9 );
			shared.results.m_percentage = baselineHz > 0
				? static_cast<float>( 100.0 * actualHz / baselineHz )
				: 0.0f;

			if ( shared.results.m_percentage > 0 &&
				 ( shared.results.m_lowestPercentage == 0 || shared.results.m_percentage < shared.results.m_lowestPercentage ) )
			{
				shared.results.m_lowestPercentage = shared.results.m_percentage;
			}

			// Wait out the interval, waking early when the settings change:
			//   disabled          -> leave, then block above until re-enabled;
			//   (re-)enabled      -> leave and measure right away (measureNow);
			//   interval changed  -> restart the wait with the new interval.
			std::uint64_t version = shared.settingsVersion;
			while ( shared.delayMs != 0 && !shared.measureNow &&
					shared.settingsChanged.wait_for( lock, std::chrono::milliseconds( shared.delayMs ),
						[&] { return shared.settingsVersion != version; } ) )
			{
				version = shared.settingsVersion;
			}
		}
	}

	std::vector<Slot> m_slots;
	std::optional<ReportedClock> m_reportedClock;  // Constructed in Start().

	std::mutex m_syncMutex;
	std::condition_variable m_wakeCv;  // Heartbeat -> measurement threads.
	std::condition_variable m_doneCv;  // Measurement threads -> heartbeat.
	std::uint64_t m_generation = 0;    // Bumped once per measurement round.
	int m_pending = 0;                 // Threads still measuring this round.
	int m_numSlots = 0;                // Threads that started.
};

// Threads are detached and never terminate, so the monitor must outlive static
// destruction: it is intentionally leaked, like the shared state.
CPUMonitor& GetMonitor()
{
	static CPUMonitor* monitor = new CPUMonitor;
	return *monitor;
}

}  // namespace


PLATFORM_INTERFACE CPUFrequencyResults GetCPUFrequencyResults( bool fGetDisabledResults )
{
	SharedState& shared = Shared();
	std::lock_guard<std::mutex> lock{ shared.lock };

	// Results of a disabled monitor are stale: only return them when asked to.
	if ( shared.delayMs != 0 || fGetDisabledResults )
	{
		return shared.results;
	}

	// Return zero initialized struct.
	return {};
}

PLATFORM_INTERFACE bool SetCPUMonitoringInterval( unsigned nDelayMilliseconds )
{
	// Clamp the delay to a minimum value to save users from running the
	// measurements too frequently. 0 disables monitoring.
	if ( nDelayMilliseconds && nDelayMilliseconds < kMinimumDelayMs )
		nDelayMilliseconds = kMinimumDelayMs;

	// Threads are only created the first time monitoring is enabled.
	if ( nDelayMilliseconds )
	{
		static const bool s_started = GetMonitor().Start();
		if ( !s_started )
			return false;
	}

	SharedState& shared = Shared();
	{
		std::lock_guard<std::mutex> lock{ shared.lock };
		if ( nDelayMilliseconds && shared.delayMs == 0 )
		{
			// If we are enabling/re-enabling then reset the stats and measure right away.
			shared.results = {};
			shared.peakHz = 0;
			shared.measureNow = true;
		}
		shared.delayMs = nDelayMilliseconds;
		++shared.settingsVersion;
	}
	shared.settingsChanged.notify_one();

	return true;
}
