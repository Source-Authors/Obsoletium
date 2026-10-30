// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#define PLAT_CPU_X86 1
#endif

#if defined(PLAT_CPU_X86) && defined(_MSC_VER)
#include <intrin.h>
#endif

#if defined(PLAT_CPU_X86) && (defined(__clang__) || defined(__GNUC__))
#include <cpuid.h>
#endif

#include <bitset>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>

#if defined(_WIN32)
#include "winlite.h"
#elif defined(OSX)
#include <sys/sysctl.h>
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

namespace
{

// Executes cpuid without any validation. Returns false on non-x86.
bool RawCpuid( unsigned function, unsigned subfunction, unsigned ( &regs )[4] )
{
#if !defined(PLAT_CPU_X86)
	regs[0] = regs[1] = regs[2] = regs[3] = 0;
	return false;
#elif (defined(__clang__) || defined(__GNUC__)) && defined(__cpuid_count)
	__cpuid_count( function, subfunction, regs[0], regs[1], regs[2], regs[3] );
	return true;
#else
	int info[4];
	__cpuidex( info, static_cast<int>( function ), static_cast<int>( subfunction ) );
	for ( int i = 0; i < 4; ++i ) regs[i] = static_cast<unsigned>( info[i] );
	return true;
#endif
}

// Highest supported leaf for the range the function belongs to. Querying a leaf
// above it returns undefined data (Intel echoes the highest basic leaf).
unsigned MaxCpuidLeaf( unsigned function )
{
	static const unsigned maxBasic = []
	{
		unsigned regs[4];
		return RawCpuid( 0, 0, regs ) ? regs[0] : 0U;
	}();
	static const unsigned maxExtended = []
	{
		unsigned regs[4];
		// No extended range if the result is not itself an extended leaf.
		return RawCpuid( 0x80000000U, 0, regs ) && regs[0] >= 0x80000000U ? regs[0] : 0U;
	}();

	return ( function & 0x80000000U ) ? maxExtended : maxBasic;
}

bool cpuidex( unsigned function,
	unsigned subfunction,
	unsigned& out_eax,
	unsigned& out_ebx,
	unsigned& out_ecx,
	unsigned& out_edx )
{
	unsigned regs[4]{ 0, 0, 0, 0 };
	const bool ok{ function <= MaxCpuidLeaf( function ) && RawCpuid( function, subfunction, regs ) };

	out_eax = regs[0];
	out_ebx = regs[1];
	out_ecx = regs[2];
	out_edx = regs[3];

	return ok;
}

bool cpuid( unsigned function,
	unsigned& out_eax,
	unsigned& out_ebx,
	unsigned& out_ecx,
	unsigned& out_edx )
{
	return cpuidex( function, 0, out_eax, out_ebx, out_ecx, out_edx );
}

constexpr const char* GenericCpuName()
{
#if !defined(_WIN64) && !defined(__x86_64__)
	return "Generic_x86";
#else
	return "Generic_x86_64";
#endif
}

// Copies |in| to |out| without leading/trailing whitespace, truncating to fit.
template <size_t outSize>
void TrimSpaces( const char* in, char ( &out )[outSize] )
{
	static_assert( outSize > 0 );

	const char* begin{ in };
	while ( *begin && std::isspace( static_cast<unsigned char>( *begin ) ) ) ++begin;

	const char* end{ begin + std::strlen( begin ) };
	while ( end > begin && std::isspace( static_cast<unsigned char>( end[-1] ) ) ) --end;

	size_t length{ static_cast<size_t>( end - begin ) };
	if ( length > outSize - 1 ) length = outSize - 1;

	std::memcpy( out, begin, length );
	out[length] = '\0';
}

// Return the Processor's vendor identification string,
// or "Generic_x86{_64}" if it doesn't exist on this CPU.
// Not thread-safe by itself; only called from ComputeCPUInformation().
const char* GetProcessorVendorId()
{
	static char vendorId[16];
	if ( vendorId[0] != '\0' )
	{
		return vendorId;
	}

#ifndef OSX
	unsigned unused, regs[3];
	BitwiseClear( vendorId );

	// Vendor string is EBX, EDX, ECX.
	if ( !cpuid( 0, unused, regs[0], regs[2], regs[1] ) )
	{
		strcpy( vendorId, GenericCpuName() );
	}
	else
	{
		std::memcpy( vendorId, regs, sizeof( regs ) );
	}

	return vendorId;
#else
	size_t len{ sizeof( vendorId ) - 1 };
	if ( sysctlbyname( "machdep.cpu.vendor", vendorId, &len, nullptr, 0 ) == 0 )
	{
		vendorId[sizeof( vendorId ) - 1] = '\0';
		return vendorId;
	}

	vendorId[0] = '\0';
	return "N/A";
#endif
}

// Return the Processor's brand.
// Not thread-safe by itself; only called from ComputeCPUInformation().
const char* GetProcessorBrand()
{
	static char cpuBrand[128];
	if ( cpuBrand[0] != '\0' )
	{
		return cpuBrand;
	}

#ifdef OSX
	char brandRaw[128]{};
	// Works both on Intel Macs and M1+
	size_t len{ sizeof( brandRaw ) - 1 };

	if ( sysctlbyname( "machdep.cpu.brand_string", brandRaw, &len, nullptr, 0 ) == 0 )
	{
		TrimSpaces( brandRaw, cpuBrand );
		return cpuBrand;
	}

	return "N/A";
#else
	// Brand string is 48 bytes spread over leaves 0x80000002..0x80000004.
	// cpuid() fails if any of them is above the highest extended leaf.
	char brandRaw[3 * 16 + 1]{};
	bool ok{ true };

	for ( unsigned i = 0; i < 3 && ok; ++i )
	{
		unsigned regs[4];
		ok = cpuid( 0x80000002U + i, regs[0], regs[1], regs[2], regs[3] );
		std::memcpy( brandRaw + i * sizeof( regs ), regs, sizeof( regs ) );
	}

	if ( ok )
	{
		TrimSpaces( brandRaw, cpuBrand );
	}

	if ( cpuBrand[0] == '\0' )
	{
		strcpy( cpuBrand, GenericCpuName() );
	}

	return cpuBrand;
#endif
}

bool CheckMMXTechnology( unsigned edx )
{
	return ( edx & ( 1U << 23U ) ) != 0;
}

bool CheckSSETechnology( unsigned edx )
{
	return ( edx & ( 1U << 25U ) ) != 0;
}

bool CheckSSE2Technology( unsigned edx )
{
	return ( edx & ( 1U << 26U ) ) != 0;
}

bool CheckSSE3Technology( unsigned ecx )
{
	return ( ecx & 1U ) != 0;
}

bool CheckSSSE3Technology( unsigned ecx )
{
	return ( ecx & ( 1U << 9U ) ) != 0;	// bit 9 of ECX
}

bool CheckSSE41Technology( unsigned ecx )
{
	return ( ecx & ( 1U << 19U ) ) != 0;	// bit 19 of ECX
}

bool CheckSSE42Technology( unsigned ecx )
{
	return ( ecx & ( 1U << 20U ) ) != 0;	// bit 20 of ECX
}

bool CheckSSE4aTechnology()
{
	// SSE4a: CPUID 0x80000001 ECX bit 6. Reserved (zero) on Intel, set on
	// AMD and Hygon, so no vendor check is needed.
	unsigned eax, ebx, ecx, edx;
	if ( !cpuid( 0x80000001U, eax, ebx, ecx, edx ) )
		return false;

	return ( ecx & ( 1U << 6U ) ) != 0;
}

bool Check3DNowTechnology()
{
	unsigned eax, ebx, ecx, edx;
	if ( !cpuid( 0x80000001U, eax, ebx, ecx, edx ) )
		return false;

	return ( edx & ( 1U << 31U ) ) != 0;
}

bool CheckCMOVTechnology( unsigned edx )
{
	return ( edx & ( 1U << 15U ) ) != 0;
}

bool CheckFCMOVTechnology( unsigned edx )
{
	// Has x87 FPU and CMOV => have FCMOV.
	return ( edx & 1U ) != 0 && CheckCMOVTechnology( edx );
}

bool CheckRDTSCTechnology( unsigned edx )
{
	return ( edx & ( 1U << 4U ) ) != 0;
}

bool CheckHTTTechnology( unsigned edx )
{
	return ( edx & ( 1U << 28U ) ) != 0;
}

// EBX[23:16] of leaf 1 is the max number of logical processors per package.
// Only meaningful when the HTT bit is set.
unsigned LogicalProcessorsPerPackage( unsigned ebx )
{
	return ( ebx >> 16U ) & 0xFFU;
}

// Clamp a processor count into [1, 255].
uint8 ClampProcessorCount( unsigned count )
{
	if ( count == 0 ) return 1;
	return static_cast<uint8>( count > UCHAR_MAX ? UCHAR_MAX : count );
}

#ifdef _WIN32

struct CpuCoreInfo
{
	unsigned physical_cores;
	unsigned logical_cores;
};

unsigned CountSetBits( KAFFINITY mask )
{
	return static_cast<unsigned>( std::bitset<sizeof( KAFFINITY ) * CHAR_BIT>( mask ).count() );
}

// Uses the Ex variant so systems with more than 64 logical processors
// (multiple processor groups) are counted fully.  Requires Windows 7+.
CpuCoreInfo GetProcessorCoresInfo()
{
	DWORD size{ 0 };
	if ( ::GetLogicalProcessorInformationEx( RelationProcessorCore, nullptr, &size ) ||
		 ::GetLastError() != ERROR_INSUFFICIENT_BUFFER )
	{
		return { 1, 1 };
	}

	HANDLE heap{ ::GetProcessHeap() };
	auto* buffer = static_cast<BYTE*>( ::HeapAlloc( heap, 0, size ) );
	if ( !buffer )
	{
		return { 1, 1 };
	}

	RunCodeAtScopeExit( ::HeapFree( heap, 0, buffer ) );

	if ( !::GetLogicalProcessorInformationEx( RelationProcessorCore,
			reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>( buffer ), &size ) )
	{
		return { 1, 1 };
	}

	unsigned physical{ 0 }, logical{ 0 };

	for ( DWORD offset = 0; offset < size; )
	{
		const auto* info =
			reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>( buffer + offset );
		if ( info->Size == 0 )
		{
			break;
		}

		if ( info->Relationship == RelationProcessorCore )
		{
			++physical;
			// A hyperthreaded core supplies more than one logical processor.
			for ( WORD group = 0; group < info->Processor.GroupCount; ++group )
			{
				logical += CountSetBits( info->Processor.GroupMask[group].Mask );
			}
		}

		offset += info->Size;
	}

	return { physical, logical };
}

#endif  // _WIN32

#if defined(POSIX)
// Move this declaration out of the QueryCurrentCpuFrequency() function because
// otherwise clang warns that it is non-obvious whether it is a variable
// or a function declaration: [-Wvexing-parse]
uint64 CalculateCPUFreq(); // from cpu_linux.cpp
#endif

}  // namespace

int64 QueryCurrentCpuFrequency();

// Measure the processor clock speed by sampling the cycle count, waiting for
// some fraction of a second, then measuring the elapsed number of cycles.
int64 QueryCurrentCpuFrequency()
{
#if defined(_WIN32)
	LARGE_INTEGER qpcFrequency;
	::QueryPerformanceFrequency( &qpcFrequency );

	// Take ~1/128 of a second for the measurement.
	const LONGLONG waitQpc{ qpcFrequency.QuadPart >> 7 };
	const uint64_t overhead{ static_cast<uint64_t>( Plat_MeasureRtscpOverhead() ) };

	constexpr int kMaxAttempts{ 16 };
	for ( int attempt = 0; attempt < kMaxAttempts; ++attempt )
	{
		LARGE_INTEGER startQpc, curQpc;
		uint32_t cpuIdStart, cpuIdEnd;

		::QueryPerformanceCounter( &startQpc );
		const uint64_t startTicks{ Plat_Rdtscp( cpuIdStart ) };
		do
		{
			::QueryPerformanceCounter( &curQpc );
		}
		while ( curQpc.QuadPart - startQpc.QuadPart < waitQpc );
		const uint64_t endTicks{ Plat_Rdtscp( cpuIdEnd ) };

		// dimhotepus: Thread migrated to another CPU core, results are
		// approximate. Retry, but accept the last attempt anyway.
		if ( cpuIdStart != cpuIdEnd && attempt + 1 < kMaxAttempts )
		{
			continue;
		}

		const uint64_t elapsedTicks{ endTicks - startTicks };
		if ( elapsedTicks <= overhead )
		{
			// Steam was seeing Divide-by-zero crashes on some Windows machines due to
			// WIN64_AMD_DUALCORE_TIMER_WORKAROUND that can cause rdtsc to effectively
			// stop. Return a plausible speed below and get on with our day.
			break;
		}

		// Scale by the time that actually elapsed, not the requested wait:
		// the loop overshoots and the thread may be preempted.
		const uint64_t elapsedQpc{ static_cast<uint64_t>( curQpc.QuadPart - startQpc.QuadPart ) };
		return static_cast<int64>( ( elapsedTicks - overhead ) *
			static_cast<uint64_t>( qpcFrequency.QuadPart ) / elapsedQpc );
	}

	return 2000000000;
#elif defined(POSIX)
	const int64 freq{ static_cast<int64>( CalculateCPUFreq() ) };
	if ( freq == 0 )  // Couldn't calculate clock speed.
	{
		Error( "Unable to determine CPU Frequency.\n" );
	}
	return freq;
#endif  // POSIX
}

namespace
{

CPUInformation ComputeCPUInformation()
{
	CPUInformation pi;
	memset( &pi, 0, sizeof( pi ) );

	// Grab the processor frequency:
	pi.m_Speed = QueryCurrentCpuFrequency();

	unsigned eax{ 0 }, ebx{ 0 }, ecx{ 0 }, edx{ 0 };
	if ( cpuid( 1, eax, ebx, ecx, edx ) )
	{
		pi.m_nModel = eax; // full CPU model info
		pi.m_nFeatures[0] = edx; // x87+ features
		pi.m_nFeatures[1] = ecx; // sse3+ features
		pi.m_nFeatures[2] = ebx; // some additional features
	}

	// Default for platforms without a better source below.
	pi.m_nLogicalProcessors =
		ClampProcessorCount( CheckHTTTechnology( edx ) ? LogicalProcessorsPerPackage( ebx ) : 1U );
	pi.m_nPhysicalProcessors = 1U;

#if defined(_WIN32)
	// dimhotepus: Correctly compute CPU cores count.
	const CpuCoreInfo coreInfo{ GetProcessorCoresInfo() };

	pi.m_nPhysicalProcessors = ClampProcessorCount( coreInfo.physical_cores );
	pi.m_nLogicalProcessors = ClampProcessorCount( coreInfo.logical_cores );

#elif defined(_LINUX)
	FILE *fpCpuInfo = fopen( "/proc/cpuinfo", "r" );
	RunCodeAtScopeExitOpt( fpCpuInfo, fclose( fpCpuInfo ) );
	if ( fpCpuInfo )
	{
		unsigned nLogicalProcs = 0;
		int nProcId = -1, nCoreId = -1;
		constexpr unsigned kMaxPhysicalCores = 1024;
		int anKnownIds[kMaxPhysicalCores];
		unsigned nKnownIdCount = 0;
		char buf[512];

		while ( fgets( buf, static_cast<int>( std::size( buf ) ), fpCpuInfo ) )
		{
			char *value = strchr( buf, ':' );
			if ( !value )
			{
				continue;
			}

			// Terminate the key at ':' and strip trailing whitespace.
			*value = '\0';
			for ( char *p = value - 1; p >= buf && std::isspace( static_cast<unsigned char>( *p ) ); --p )
			{
				*p = '\0';
			}
			for ( char *p = buf; *p; ++p )
			{
				*p = static_cast<char>( std::tolower( static_cast<unsigned char>( *p ) ) );
			}

			if ( !strcmp( buf, "processor" ) )
			{
				++nLogicalProcs;
				nProcId = nCoreId = -1;
			}
			else if ( !strcmp( buf, "physical id" ) )
			{
				nProcId = atoi( value + 1 );
			}
			else if ( !strcmp( buf, "core id" ) )
			{
				nCoreId = atoi( value + 1 );
			}

			if ( nProcId != -1 && nCoreId != -1 ) // as soon as we have a complete id, process it
			{
				const int nId = ( nProcId << 16 ) + nCoreId;
				unsigned i = 0;
				while ( i < nKnownIdCount && anKnownIds[i] != nId ) { ++i; }
				if ( i == nKnownIdCount && nKnownIdCount < kMaxPhysicalCores )
					anKnownIds[nKnownIdCount++] = nId;
				nProcId = nCoreId = -1;
			}
		}

		pi.m_nLogicalProcessors = ClampProcessorCount( nLogicalProcs );
		// No "physical id"/"core id" (e.g. many ARM kernels): assume no SMT.
		pi.m_nPhysicalProcessors =
			ClampProcessorCount( nKnownIdCount ? nKnownIdCount : nLogicalProcs );
	}
	else
	{
		pi.m_nPhysicalProcessors = 1;
		pi.m_nLogicalProcessors  = 1;
		AssertMsg( false, "couldn't read cpu information from /proc/cpuinfo" );
	}
#elif defined(OSX)
	// hw.physicalcpu / hw.logicalcpu are available on both Intel and Apple Silicon.
	int physicalCount = 1, logicalCount = 1;
	size_t len = sizeof( physicalCount );
	if ( sysctlbyname( "hw.physicalcpu", &physicalCount, &len, nullptr, 0 ) != 0 )
		physicalCount = 1;

	len = sizeof( logicalCount );
	if ( sysctlbyname( "hw.logicalcpu", &logicalCount, &len, nullptr, 0 ) != 0 )
		logicalCount = 1;

	pi.m_nPhysicalProcessors = ClampProcessorCount( static_cast<unsigned>( physicalCount > 0 ? physicalCount : 1 ) );
	pi.m_nLogicalProcessors = ClampProcessorCount( static_cast<unsigned>( logicalCount > 0 ? logicalCount : 1 ) );
#endif

	// Determine Processor Features:
	pi.m_bRDTSC           = CheckRDTSCTechnology( edx );
	pi.m_bCMOV            = CheckCMOVTechnology( edx );
	pi.m_bFCMOV           = CheckFCMOVTechnology( edx );
	pi.m_bSSE             = CheckSSETechnology( edx );
	pi.m_bSSE2            = CheckSSE2Technology( edx );
	pi.m_b3DNow           = Check3DNowTechnology();
	pi.m_bMMX             = CheckMMXTechnology( edx );
	// dimhotepus: Correctly check HyperThreading support.
	pi.m_bHT              = pi.m_nLogicalProcessors > pi.m_nPhysicalProcessors;
	pi.m_bSSE3            = CheckSSE3Technology( ecx );
	pi.m_bSSSE3           = CheckSSSE3Technology( ecx );
	pi.m_bSSE4a           = CheckSSE4aTechnology();
	pi.m_bSSE41           = CheckSSE41Technology( ecx );
	pi.m_bSSE42           = CheckSSE42Technology( ecx );
	pi.m_szProcessorID    = GetProcessorVendorId();
	pi.m_szProcessorBrand = GetProcessorBrand();

	// Mark struct as ready and filled.
	pi.m_Size = sizeof( pi );

	return pi;
}

}  // namespace

MUST_CHECK_RETURN const CPUInformation *GetCPUInformation()
{
	// Thread-safe one-time initialization.
	static const CPUInformation pi{ ComputeCPUInformation() };
	return &pi;
}