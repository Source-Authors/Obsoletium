// Copyright Valve Corporation, All rights reserved.

#include "pch_tier0.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif

#if (defined(__clang__) || defined(__GNUC__)) && (defined(__x86_64__) || defined(__i386__))
#include <cpuid.h>
#endif

#include <array>
#include <vector>

#if defined(_WIN32)
#include "winlite.h"
#elif defined(_LINUX)
#include <cstdlib>
#elif defined(OSX)
#include <sys/sysctl.h>
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

namespace
{

bool cpuid(unsigned int function,
	unsigned int& out_eax,
	unsigned int& out_ebx,
	unsigned int& out_ecx,
	unsigned int& out_edx)
{
	int CPUInfo[4] = { -1 };
#if (defined(__clang__) || defined(__GNUC__)) && defined(__cpuid)
	__cpuid(function, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
#else
	__cpuid(CPUInfo, (int)function);
#endif

	out_eax = CPUInfo[0];
	out_ebx = CPUInfo[1];
	out_ecx = CPUInfo[2];
	out_edx = CPUInfo[3];
	
	return true;
}

bool cpuidex(unsigned int function,
	unsigned int subfunction,
	unsigned int& out_eax,
	unsigned int& out_ebx,
	unsigned int& out_ecx,
	unsigned int& out_edx)
{
	int CPUInfo[4] = { -1 };
#if (defined(__clang__) || defined(__GNUC__)) && defined(__cpuid)
	__cpuid_count(function, subfunction, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
#else
	__cpuidex(CPUInfo, (int)function, (int)subfunction);
#endif

	out_eax = CPUInfo[0];
	out_ebx = CPUInfo[1];
	out_ecx = CPUInfo[2];
	out_edx = CPUInfo[3];
	
	return true;
}

// Return the Processor's vendor identification string,
// or "Generic_x86{_64}" if it doesn't exist on this CPU.
const char* GetProcessorVendorId()
{
#if defined( _PS3 )
	return "PPC";
#else
	static char vendorId[16];
	if (vendorId[0] != '\0')
	{
		return vendorId;
	}
	
#ifndef OSX
	unsigned int unused, regs[3];
	memset( vendorId, 0, sizeof(vendorId) );

	if ( !cpuid(0,unused, regs[0], regs[2], regs[1] ) )
	{
		if ( IsPC() )
		{
#if !defined(_WIN64) && !defined(__x86_64__)
			strcpy( vendorId, "Generic_x86" );
#else
			strcpy( vendorId, "Generic_x86_64" );
#endif
		}
	}
	else
	{
		memcpy( vendorId+0, &regs[0], sizeof( regs[0] ) );
		memcpy( vendorId+4, &regs[1], sizeof( regs[1] ) ); //-V112
		memcpy( vendorId+8, &regs[2], sizeof( regs[2] ) );
	}
#else
	// Works both on Intel Macs and M1+
	size_t len = sizeof(vendorId);

	if (sysctlbyname("machdep.cpu.vendor",
		vendorId, &len, nullptr, 0) == 0)
	{
		return vendorId;
	}

	return "N/A";
#endif

	return vendorId;
#endif
}

// Trim spaces around data.
void TrimSpaces( char (&in)[128], char (&out)[128] )
{
	size_t i{0};
	// Trim leading space.
	while (std::isspace( in[i] )) ++i;
	
	if (in[i] == '\0')
	{
	  out[0] = '\0';
	  return;
	}
	
	// Trim trailing space.
	char *end{in + strlen( in ) - 1};
	
	while (end > in && std::isspace( *end )) end--;
	
	// Write new null terminator character.
	end[1] = '\0';
	
#ifdef _WIN32
	strncpy_s( out, &in[0] + i, end + 1 - in + i );
#else
	const size_t size{static_cast<size_t>(end + 1 - in) + i};
	strncpy( out, &in[0] + i, size );

	out[ std::size(out) - 1 ] = '\0';
#endif  // _WIN32
}

// Get CPU brand.
const char* GetCpuBrand
(
	const std::vector<std::array<unsigned, 4>> &in,
	char (&brand)[128]
)
{
	char brand_raw[128]{'\0'};

	std::memcpy(brand_raw, in[2].data(), sizeof(in[2]));
	std::memcpy(brand_raw + sizeof(in[2]), in[3].data(), sizeof(in[3]));
	std::memcpy(brand_raw + sizeof(in[2]) + sizeof(in[3]), in[4].data(),  //-V119
				sizeof(in[4]));

	TrimSpaces(brand_raw, brand);

	return brand;
}

// Return the Processor's brand.
const char* GetProcessorBrand()
{
#if defined( _PS3 )
	return "PPC";
#else
	static char cpuBrand[128]{'\0'};
	if (cpuBrand[0] != '\0')
	{
		return cpuBrand;
	}

#ifdef OSX
	char brand_raw[128]{'\0'};
	// Works both on Intel Macs and M1+
	size_t len = sizeof(brand_raw);

	if (sysctlbyname("machdep.cpu.brand_string",
		brand_raw, &len, nullptr, 0) == 0)
	{
		TrimSpaces(brand_raw, cpuBrand);
		return cpuBrand;
	}

	return "N/A";
#else
	// Calling cpuid with 0x80000000 as the function_id argument gets the
	// number of the highest valid extended ID.
	unsigned eax, ebx, ecx, edx;
	if ( !cpuid(0x80000000U, eax, ebx, ecx, edx) )
	{
		if ( IsPC() )
		{
#if !defined(_WIN64) && !defined(__x86_64__)
			strcpy( cpuBrand, "Generic_x86" );
#else
			strcpy( cpuBrand, "Generic_x86_64" );
#endif
		}

		return cpuBrand;
	}

	const unsigned extFuncsCount = eax;

	std::vector<std::array<unsigned, 4>> extendedData;
	extendedData.reserve(
		std::max(extFuncsCount - 0x80000000U, 0U) + 1U);

	for (unsigned i = 0x80000000U; i <= extFuncsCount; ++i) {
		cpuidex( i, 0, eax, ebx, ecx, edx );

		extendedData.emplace_back(std::array<unsigned, 4>{ eax, ebx, ecx, edx });
	}

	return GetCpuBrand( extendedData, cpuBrand );
#endif
#endif
}

bool CheckMMXTechnology( unsigned edx )
{
#if defined( _PS3 ) 
	return true;
#else
	return ( edx & ( 1U << 23U ) ) != 0;
#endif
}

bool CheckSSETechnology( unsigned edx )
{
#if defined( _PS3 )
	return true;
#else
	return ( edx & ( 1U << 25U ) ) != 0;
#endif
}

bool CheckSSE2Technology( unsigned edx )
{
#if defined( _PS3 )
	return false;
#else
	return ( edx & ( 1U << 26U ) ) != 0;
#endif
}

bool CheckSSE3Technology( unsigned ecx )
{
#if defined( _PS3 )
	return false;
#else
	return ( ecx & 1U ) != 0;
#endif
}

bool CheckSSSE3Technology( unsigned ecx )
{
#if defined( _PS3 )
	return false;
#else
	// SSSE 3 is implemented by both Intel and AMD
	// detection is done the same way for both vendors
	return ( ecx & ( 1U << 9U ) ) != 0;	// bit 9 of ECX
#endif
}

bool CheckSSE41Technology( unsigned ecx )
{
#if defined( _PS3 )
	return false;
#else
	// SSE 4.1 is implemented by both Intel and AMD
	// detection is done the same way for both vendors
	return ( ecx & ( 1U << 19U ) ) != 0;	// bit 19 of ECX
#endif
}

bool CheckSSE42Technology( unsigned ecx )
{
#if defined( _PS3 )
	return false;
#else
	// SSE 4.2 is implemented by both Intel and AMD
	// detection is done the same way for both vendors
	return ( ecx & ( 1U << 20U ) ) != 0;	// bit 20 of ECX
#endif
}

bool CheckSSE4aTechnology()
{
#if defined( _PS3 )
	return false;
#else
	// SSE 4a is an AMD-only feature
	const char *vendorId = GetProcessorVendorId();
	if ( 0 != V_tier0_stricmp( vendorId, "AuthenticAMD" ) )
		return false;

	unsigned int eax,ebx,edx,ecx;
	if( !cpuid( 0x80000001U,eax,ebx,ecx,edx) )
		return false;

	return ( ecx & ( 1U << 6U ) ) != 0;	// bit 6 of ECX
#endif
}

bool Check3DNowTechnology()
{
#if defined( _PS3 )
	return false;
#else
	unsigned int eax, unused;
	if ( !cpuid(0x80000000U,eax,unused,unused,unused) ) //-V112
		return false;

	if ( eax > 0x80000000U ) //-V112
	{
		if ( !cpuid(0x80000001U,unused,unused,unused,eax) )
			return false;

		return ( eax & 1U << 31U ) != 0;
	}

	return false;
#endif
}

bool CheckCMOVTechnology( unsigned edx )
{
#if defined( _PS3 )
	return false;
#else
	return ( edx & (1U << 15U) ) != 0;
#endif
}

bool CheckFCMOVTechnology( unsigned edx )
{
#if defined( _PS3 )
	return false;
#else
	// Has x87 FPU and CMOV => have FCMOV.
	return ( edx & 1U ) != 0 && CheckCMOVTechnology( edx );
#endif
}

bool CheckRDTSCTechnology( unsigned edx )
{
#if defined( _PS3 )
	return false;
#else
	return ( edx & (1U << 4U) ) != 0;
#endif
}

bool CheckPopcntTechnology( unsigned int ecx )
{
#if defined( _PS3 )
	return false;
#else
	return ( ecx & ( 1U << 23U ) ) != 0;	// bit 23 of ECX
#endif
}

// Returns the number of logical processors per physical processors.
uint8 LogicalProcessorsPerPackage( unsigned ebx )
{
	// EBX[23:16] indicate number of logical processors per package
	constexpr unsigned NUM_LOGICAL_BITS = 0x00FF0000U;

	return (uint8) ((ebx & NUM_LOGICAL_BITS) >> 16U);
}

#ifdef _WIN32

struct CpuCoreInfo
{
	unsigned physical_cores;
	unsigned logical_cores;
};

// Helper function to count set bits in the processor mask.
unsigned CountSetBits( ULONG_PTR mask, bool is_popcnt_supported )
{
#if !(defined(_M_ARM) || defined(_M_ARM64) || defined(_WIN64))
	if ( is_popcnt_supported )
	{
		return __popcnt(mask);
	}
#elif defined(_WIN64)
	if ( is_popcnt_supported )
	{
		// Requires CPU support.
		return static_cast<unsigned>(__popcnt64(mask));
	}
#endif

	constexpr size_t kLeftShift{sizeof(ULONG_PTR) * 8 - 1};
	size_t bit_test{static_cast<size_t>(1U) << kLeftShift};

	unsigned bits_num{0};
	for (size_t i = 0; i <= kLeftShift; ++i)
	{
		bits_num += ((mask & bit_test) ? 1U : 0U);

		bit_test /= 2;
	}

	return bits_num;
}

CpuCoreInfo GetProcessorCoresInfo( bool is_popcnt_supported )
{
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION *buffer{nullptr};
	DWORD size{0};

	while ( true )
	{
		const BOOL rc{ ::GetLogicalProcessorInformation( buffer, &size ) };
		if ( rc ) break;
 
		if ( ::GetLastError() == ERROR_INSUFFICIENT_BUFFER )
		{
			if ( buffer ) ::HeapFree( ::GetProcessHeap(), 0, buffer );

			buffer = static_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION*>(
				::HeapAlloc( ::GetProcessHeap(), 0, static_cast<size_t>( size ) ) );
			if ( !buffer )
			{
				// Allocation failure.
				return { 1, 1 };
			}
		}
		else
		{
			// Error GetLastError()
			return { 1, 1 };
		}
	}

	SYSTEM_LOGICAL_PROCESSOR_INFORMATION *it{buffer};
	size_t offset{0};
	unsigned logical_cores_num{0}, physical_cores_num{0};

	while ( offset + sizeof(*it) <= static_cast<size_t>(size) )
	{
		switch (it->Relationship)
		{
			case RelationProcessorCore:
				physical_cores_num++;
				// A hyperthreaded core supplies more than one logical processor.
				logical_cores_num += CountSetBits( it->ProcessorMask, is_popcnt_supported );
				break;

			default:
				// Unsupported LOGICAL_PROCESSOR_RELATIONSHIP value.
				break;
		}

		offset += sizeof(*it);
		++it;
	}

	::HeapFree( GetProcessHeap(), 0, buffer );

	return { physical_cores_num, logical_cores_num };
}

#endif

#if defined(POSIX)
// Move this declaration out of the CalculateClockSpeed() function because
// otherwise clang warns that it is non-obvious whether it is a variable
// or a function declaration: [-Wvexing-parse]
uint64 CalculateCPUFreq(); // from cpu_linux.cpp
#endif

}  // namespace

// Measure the processor clock speed by sampling the cycle count, waiting
// for some fraction of a second, then measuring the elapsed number of cycles.
static int64 CalculateClockSpeed()
{
#if defined( _WIN32 )
	LARGE_INTEGER waitTime, startCount, curCount;
	CCycleCount start, end;

	// Take 1/32 of a second for the measurement.
	QueryPerformanceFrequency( &waitTime );
	int scale = 5;
	waitTime.QuadPart >>= scale;

	QueryPerformanceCounter( &startCount );
	start.Sample();
	do
	{
		QueryPerformanceCounter( &curCount );
	}
	while ( curCount.QuadPart - startCount.QuadPart < waitTime.QuadPart );
	end.Sample();

	int64 freq = (end.m_Int64 - start.m_Int64) << scale;
	if ( freq == 0 )
	{
		// Steam was seeing Divide-by-zero crashes on some Windows machines due to
		// WIN64_AMD_DUALCORE_TIMER_WORKAROUND that can cause rdtsc to effectively
		// stop. Staging doesn't have the workaround but I'm checking in the fix
		// anyway. Return a plausible speed and get on with our day.
		freq = 2000000000;
	}
	return freq;
#elif defined(POSIX)
	int64 freq =(int64)CalculateCPUFreq();
	if ( freq == 0 ) // couldn't calculate clock speed
	{
		Error( "Unable to determine CPU Frequency\n" );
	}
	return freq;
#endif
}

const CPUInformation* GetCPUInformation()
{
	static CPUInformation pi;

	// Has the structure already been initialized and filled out?
	if ( pi.m_Size == sizeof(pi) )
		return &pi;

	// Redundant, but just in case the user somehow messes with the size.
	memset(&pi, 0x0, sizeof(pi));

	// Grab the processor frequency:
	pi.m_Speed = CalculateClockSpeed();

	unsigned int eax, ebx, edx, ecx;
	if (cpuid(1, eax, ebx, ecx, edx))
	{
		pi.m_nModel = eax; // full CPU model info
		pi.m_nFeatures[0] = edx; // x87+ features
		pi.m_nFeatures[1] = ecx; // sse3+ features
		pi.m_nFeatures[2] = ebx; // some additional features
	}
	
	// Get the logical and physical processor counts:
	pi.m_nLogicalProcessors = LogicalProcessorsPerPackage( ebx );
	pi.m_nPhysicalProcessors = 1U;

#if defined(_WIN32)
	// dimhotepus: Correctly compute CPU cores count.
	CpuCoreInfo cpu_core_info{ GetProcessorCoresInfo( CheckPopcntTechnology( ecx ) ) };

	// Ensure 256+ CPUs are at least 256.
	pi.m_nPhysicalProcessors =
		static_cast<uint8>( min( cpu_core_info.physical_cores, static_cast<unsigned>(UCHAR_MAX) ) );
	pi.m_nLogicalProcessors =
		static_cast<uint8>( min( cpu_core_info.logical_cores, static_cast<unsigned>(UCHAR_MAX) ) );

	// Make sure I always report at least one, when running WinXP with the /ONECPU switch, 
	// it likes to report 0 processors for some reason.
	if ( pi.m_nPhysicalProcessors == 0 && pi.m_nLogicalProcessors == 0 )
	{
		pi.m_nPhysicalProcessors = 1;
		pi.m_nLogicalProcessors  = 1;
	}
#elif defined(_LINUX)
	// TODO: poll /dev/cpuinfo when we have some benefits from multithreading
	FILE *fpCpuInfo = fopen( "/proc/cpuinfo", "r" );
	if ( fpCpuInfo )
	{
		int nLogicalProcs = 0;
		int nProcId = -1, nCoreId = -1;
		const int kMaxPhysicalCores = 128;
		int anKnownIds[kMaxPhysicalCores];
		int nKnownIdCount = 0;
		char buf[255];
		while ( fgets( buf, std::size(buf), fpCpuInfo ) )
		{
			if ( char *value = strchr( buf, ':' ) )
			{
				for ( char *p = value - 1; p > buf && isspace((unsigned char)*p); --p )
				{
					*p = 0;
				}
				for ( char *p = buf; p < value && *p; ++p )
				{
					*p = tolower((unsigned char)*p);
				}
				if ( !strcmp( buf, "processor" ) )
				{
					++nLogicalProcs;
					nProcId = nCoreId = -1;
				}
				else if ( !strcmp( buf, "physical id" ) )
				{
					nProcId = atoi( value+1 );
				}
				else if ( !strcmp( buf, "core id" ) )
				{
					nCoreId = atoi( value+1 );
				}

				if (nProcId != -1 && nCoreId != -1) // as soon as we have a complete id, process it
				{
					int i = 0, nId = (nProcId << 16) + nCoreId;
					while ( i < nKnownIdCount && anKnownIds[i] != nId ) { ++i; }
					if ( i == nKnownIdCount && nKnownIdCount < kMaxPhysicalCores )
						anKnownIds[nKnownIdCount++] = nId;
					nProcId = nCoreId = -1;
				}
			}
		}
		fclose( fpCpuInfo );
		pi.m_nLogicalProcessors = MAX( 1, nLogicalProcs );
		pi.m_nPhysicalProcessors = MAX( 1, nKnownIdCount );
	}
	else
	{
		pi.m_nPhysicalProcessors = 1;
		pi.m_nLogicalProcessors  = 1;
		AssertMsg( false, "couldn't read cpu information from /proc/cpuinfo" );
	}
#elif defined(OSX)
	uint32_t physical_core_count = 1, logical_core_count = 1;
	size_t len = sizeof(uint32_t);

	// Works both for Intel Macs and M1+
	// See https://discussions.apple.com/thread/252285216?sortBy=best
	pi.m_nPhysicalProcessors =
		sysctlbyname("machdep.cpu.core_count",
			&physical_core_count, &len, nullptr, 0) == 0
		? physical_core_count : 1;
	pi.m_nLogicalProcessors =
		sysctlbyname("machdep.cpu.thread_count",
			&logical_core_count, &len, nullptr, 0) == 0
		? logical_core_count : 1;
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
	pi.m_bHT              = pi.m_nPhysicalProcessors != pi.m_nLogicalProcessors;
	pi.m_bSSE3            = CheckSSE3Technology( ecx );
	pi.m_bSSSE3           = CheckSSSE3Technology( ecx );
	pi.m_bSSE4a           = CheckSSE4aTechnology();
	pi.m_bSSE41           = CheckSSE41Technology( ecx );
	pi.m_bSSE42           = CheckSSE42Technology( ecx );
	pi.m_szProcessorID    = GetProcessorVendorId();
	pi.m_szProcessorBrand = GetProcessorBrand();

	// Mark struct as ready and filled, return it:
	pi.m_Size = sizeof(pi);

	return &pi;
}
