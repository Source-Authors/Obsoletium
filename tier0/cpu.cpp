// Copyright Valve Corporation, All rights reserved.

#include "pch_tier0.h"

#if defined(_LINUX)
#include <cstdlib>
#elif defined(OSX)
#include <sys/sysctl.h>
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

static bool cpuid(unsigned long function,
	unsigned long& out_eax,
	unsigned long& out_ebx,
	unsigned long& out_ecx,
	unsigned long& out_edx)
{
#if defined(GNUC)
	asm("mov %%ebx, %%esi\n\t"
		"cpuid\n\t"
		"xchg %%esi, %%ebx"
		: "=a" (out_eax),
		"=S" (out_ebx),
		"=c" (out_ecx),
		"=d" (out_edx)
		: "a" (function) 
		);
	return true;
#elif defined( _X360 )
	return false;
#else
	bool retval = true;

	__try
	{
			int pCPUInfo[4];
			__cpuid(pCPUInfo, (int)function);
			out_eax = pCPUInfo[0];
			out_ebx = pCPUInfo[1];
			out_ecx = pCPUInfo[2];
			out_edx = pCPUInfo[3];
	} 
	__except(EXCEPTION_EXECUTE_HANDLER)
	{ 
		retval = false;
	}

	return retval;
#endif
}

// Return the Processor's vendor identification string, or "Generic_x86" if it doesn't exist on this CPU
static const tchar* GetProcessorVendorId()
{
#if defined( _X360 ) || defined( _PS3 )
	return "PPC";
#else
	unsigned long unused, VendorIDRegisters[3];

	static tchar VendorID[13];
	
	memset( VendorID, 0, sizeof(VendorID) );
	if ( !cpuid(0,unused, VendorIDRegisters[0], VendorIDRegisters[2], VendorIDRegisters[1] ) )
	{
		if ( IsPC() )
		{
			_tcscpy( VendorID, _T( "Generic_x86" ) ); 
		}
		else if ( IsX360() )
		{
			_tcscpy( VendorID, _T( "PowerPC" ) ); 
		}
	}
	else
	{
		memcpy( VendorID+0, &(VendorIDRegisters[0]), sizeof( VendorIDRegisters[0] ) );
		memcpy( VendorID+4, &(VendorIDRegisters[1]), sizeof( VendorIDRegisters[1] ) ); //-V112
		memcpy( VendorID+8, &(VendorIDRegisters[2]), sizeof( VendorIDRegisters[2] ) );
	}

	return VendorID;
#endif
}

static bool CheckMMXTechnology()
{
#if defined( _X360 ) || defined( _PS3 ) 
	return true;
#else
	unsigned long eax,ebx,edx,unused;
	if ( !cpuid(1,eax,ebx,unused,edx) )
		return false;

	return ( edx & 0x800000 ) != 0;
#endif
}


static bool CheckSSETechnology()
{
#if defined( _X360 ) || defined( _PS3 )
	return true;
#else
	unsigned long eax,ebx,edx,unused;
	if ( !cpuid(1,eax,ebx,unused,edx) )
	{
		return false;
	}

	return ( edx & 0x2000000L ) != 0;
#endif
}

static bool CheckSSE2Technology()
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	unsigned long eax,ebx,edx,unused;
	if ( !cpuid(1,eax,ebx,unused,edx) )
		return false;

	return ( edx & 0x04000000 ) != 0;
#endif
}

static bool CheckSSE3Technology()
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	unsigned long eax,ebx,edx,ecx;
	if( !cpuid(1,eax,ebx,ecx,edx) )
		return false;

	return ( ecx & 0x00000001 ) != 0;	// bit 1 of ECX
#endif
}

static bool CheckSSSE3Technology()
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	// SSSE 3 is implemented by both Intel and AMD
	// detection is done the same way for both vendors
	unsigned long eax,ebx,edx,ecx;
	if( !cpuid(1,eax,ebx,ecx,edx) )
		return false;

	return ( ecx & ( 1 << 9 ) ) != 0;	// bit 9 of ECX
#endif
}

static bool CheckSSE41Technology()
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	// SSE 4.1 is implemented by both Intel and AMD
	// detection is done the same way for both vendors
	unsigned long eax,ebx,edx,ecx;
	if( !cpuid(1,eax,ebx,ecx,edx) )
		return false;

	return ( ecx & ( 1 << 19 ) ) != 0;	// bit 19 of ECX
#endif
}

static bool CheckSSE42Technology()
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	// SSE 4.2 is implemented by both Intel and AMD
	// detection is done the same way for both vendors
	unsigned long eax,ebx,edx,ecx;
	if( !cpuid(1,eax,ebx,ecx,edx) )
		return false;

	return ( ecx & ( 1 << 20 ) ) != 0;	// bit 20 of ECX
#endif
}


static bool CheckSSE4aTechnology()
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	// SSE 4a is an AMD-only feature
	const char *pchVendor = GetProcessorVendorId();
	if ( 0 != V_tier0_stricmp( pchVendor, "AuthenticAMD" ) )
		return false;

	unsigned long eax,ebx,edx,ecx;
	if( !cpuid( 0x80000001,eax,ebx,ecx,edx) )
		return false;

	return ( ecx & ( 1 << 6 ) ) != 0;	// bit 6 of ECX
#endif
}

static bool Check3DNowTechnology()
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	unsigned long eax, unused;
	if ( !cpuid(0x80000000,eax,unused,unused,unused) ) //-V112
		return false;

	if ( eax > 0x80000000L ) //-V112
	{
		if ( !cpuid(0x80000001,unused,unused,unused,eax) )
			return false;

		return ( eax & 1<<31 ) != 0;
	}

	return false;
#endif
}

static bool CheckCMOVTechnology()
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	unsigned long eax,ebx,edx,unused;
	if ( !cpuid(1,eax,ebx,unused,edx) )
		return false;

	return ( edx & (1<<15) ) != 0;
#endif
}

static bool CheckFCMOVTechnology()
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	unsigned long eax,ebx,edx,unused;
	if ( !cpuid(1,eax,ebx,unused,edx) )
		return false;

	return ( edx & (1<<16) ) != 0;
#endif
}

static bool CheckRDTSCTechnology()
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	unsigned long eax,ebx,edx,unused;
	if ( !cpuid(1,eax,ebx,unused,edx) )
		return false;

	return ( edx & 0x10 ) != 0;
#endif
}

static bool CheckPopcntTechnology( unsigned long ecx )
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	return ( ecx & ( 1U << 23U ) ) != 0;	// bit 23 of ECX
#endif
}

// Returns the number of logical processors per physical processors.
static uint8 LogicalProcessorsPerPackage()
{
#if defined( _X360 )
	return 6;
#else
	// EBX[23:16] indicate number of logical processors per package
	const unsigned NUM_LOGICAL_BITS = 0x00FF0000;

	unsigned long unused, reg_ebx = 0;

	if ( !cpuid(1,unused,reg_ebx,unused,unused) )
		return 1;

	return (uint8) ((reg_ebx & NUM_LOGICAL_BITS) >> 16);
#endif
}

#ifdef _WIN32

struct CpuCoreInfo
{
	unsigned physical_cores;
	unsigned logical_cores;
};

// Helper function to count set bits in the processor mask.
static unsigned CountSetBits( ULONG_PTR mask, bool is_popcnt_supported )
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

static CpuCoreInfo GetProcessorCoresInfo( bool is_popcnt_supported )
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

// Measure the processor clock speed by sampling the cycle count, waiting
// for some fraction of a second, then measuring the elapsed number of cycles.
static int64 CalculateClockSpeed()
{
#if defined( _WIN32 )
#if !defined( _X360 )
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

#else
	return 3200000000LL;
#endif
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

	// Fill out the structure, and return it:
	pi.m_Size = sizeof(pi);

	// Grab the processor frequency:
	pi.m_Speed = CalculateClockSpeed();
	
	// Get the logical and physical processor counts:
	pi.m_nLogicalProcessors = LogicalProcessorsPerPackage();
	pi.m_nPhysicalProcessors = 1U;

	unsigned long eax, ebx, edx, ecx;
	if (cpuid(1, eax, ebx, ecx, edx))
	{
		pi.m_nModel = eax; // full CPU model info
		pi.m_nFeatures[0] = edx; // x87+ features
		pi.m_nFeatures[1] = ecx; // sse3+ features
		pi.m_nFeatures[2] = ebx; // some additional features
	}

#if defined(_WIN32) && !defined( _X360 )
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
#elif defined( _X360 )
	pi.m_nPhysicalProcessors = 3;
	pi.m_nLogicalProcessors  = 6;
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
		while ( fgets( buf, ARRAYSIZE(buf), fpCpuInfo ) )
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
		Assert( !"couldn't read cpu information from /proc/cpuinfo" );
	}
#elif defined(OSX)
	int mib[2] { CTL_HW, HW_NCPU }, num_cpu = 1;
	size_t len = sizeof(num_cpu);

	sysctl(mib, 2, &num_cpu, &len, nullptr, 0);

	pi.m_nPhysicalProcessors = num_cpu;
	pi.m_nLogicalProcessors  = num_cpu;
#endif

	// Determine Processor Features:
	pi.m_bRDTSC        = CheckRDTSCTechnology();
	pi.m_bCMOV         = CheckCMOVTechnology();
	pi.m_bFCMOV        = CheckFCMOVTechnology();
	pi.m_bMMX          = CheckMMXTechnology();
	pi.m_bSSE          = CheckSSETechnology();
	pi.m_bSSE2         = CheckSSE2Technology();
	pi.m_bSSE3         = CheckSSE3Technology();
	pi.m_bSSSE3		   = CheckSSSE3Technology();
	pi.m_bSSE4a        = CheckSSE4aTechnology();
	pi.m_bSSE41        = CheckSSE41Technology();
	pi.m_bSSE42        = CheckSSE42Technology();
	pi.m_b3DNow        = Check3DNowTechnology();
	pi.m_szProcessorID = (tchar*)GetProcessorVendorId();
	// dimhotepus: Correctly check HyperThreading support.
	pi.m_bHT		   = pi.m_nPhysicalProcessors != pi.m_nLogicalProcessors;

	return &pi;
}
