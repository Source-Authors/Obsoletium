// Copyright Valve Corporation, All rights reserved.
//
// Memory allocation!

#include "stdafx.h"

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

#include "tier0/memalloc.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <utility>

#include "tier0/dbg.h"
#include "tier0/fasttimer.h"
#include "tier0/threadtools.h"
#include "mem_helpers.h"
#include "memstd.h"

#if defined( _WIN32 )
#include <malloc.h>          // _msize
#elif defined( __APPLE__ )
#include <malloc/malloc.h>   // malloc_size, malloc_zone_pressure_relief
#include <sys/mman.h>
#include <unistd.h>
#else
#include <malloc.h>          // malloc_usable_size, malloc_trim
#include <sys/mman.h>
#include <unistd.h>
#endif

// dimhotepus: Win64 used to define FORCE_PROCESS_HEAP but not ALLOW_PROCESS_HEAP, and
// all process-heap code is compiled only under ALLOW_PROCESS_HEAP, so on Win64 the
// "force" never took effect and -processheap was unavailable. Now every Windows target
// (x86, x64, ARM64) gets the opt-in switch and the default (small block heap + CRT) is
// unchanged everywhere.
#if defined( _WIN32 )
// Define this to force using the OS Heap* functions for allocations. This is useful
// in conjunction with AppVerifier/PageHeap in order to find memory problems, and
// also allows ETW/xperf tracing to be used to record allocations.
// Normally the command-line option -processheap can be used instead.
//#define FORCE_PROCESS_HEAP

#define ALLOW_PROCESS_HEAP
#endif

#if (defined(NDEBUG) && !defined(USE_MEM_DEBUG))
// dimhotepus: Case-insensitive (ASCII) search of the WHOLE command line, without copying.
// The old code upper-cased a 512-byte copy, so switches past 512 chars were ignored.
// pszUpperToken must be upper case.
[[maybe_unused]] static bool CommandLineHasToken( const char *pszCmdLine, const char *pszUpperToken )
{
	if ( !pszCmdLine || !*pszUpperToken )
		return false;

	const auto upper = []( char c ) { return ( c >= 'a' && c <= 'z' ) ? static_cast<char>( c - 'a' + 'A' ) : c; };
	for ( const char *pStart = pszCmdLine; *pStart; ++pStart )
	{
		const char *a = pStart;
		const char *b = pszUpperToken;
		while ( *a && *b && upper( *a ) == *b )
		{
			++a;
			++b;
		}
		if ( !*b )
			return true;
	}
	return false;
}
#endif

#if (defined(NDEBUG) && !defined(USE_MEM_DEBUG)) && defined(ALLOW_PROCESS_HEAP)
// Track this to decide how to handle out-of-memory.
static bool s_bPageHeapEnabled = false;
#endif

#ifdef TIME_ALLOC
CAverageCycleCounter g_MallocCounter;
CAverageCycleCounter g_ReallocCounter;
CAverageCycleCounter g_FreeCounter;

#define PrintOne( name ) \
	Msg("%-48s: %6.4f avg (%8.1f total, %7.3f peak, %5d iters)\n",  \
		#name, \
		g_##name##Counter.GetAverageMilliseconds(), \
		g_##name##Counter.GetTotalMilliseconds(), \
		g_##name##Counter.GetPeakMilliseconds(), \
		g_##name##Counter.GetIters() ); \
	memset( &g_##name##Counter, 0, sizeof(g_##name##Counter) )

void PrintAllocTimes()
{
	PrintOne( Malloc );
	PrintOne( Realloc );
	PrintOne( Free );
}

#define PROFILE_ALLOC(name) CAverageTimeMarker name##_ATM( &g_##name##Counter )

#else
#define PROFILE_ALLOC( name ) ((void)0)
#define PrintAllocTimes() ((void)0)
#endif

#if (defined(NDEBUG) && !defined(USE_MEM_DEBUG))

// Support for CHeapMemAlloc for easy switching to using the process heap.
#ifdef ALLOW_PROCESS_HEAP

// dimhotepus: g_pMemAlloc is switched to CHeapMemAlloc during static initialization,
// after s_StdMemAlloc may already have handed out blocks. CRT blocks are fine (the UCRT
// allocates from the process heap), but small block heap blocks passed to HeapFree /
// HeapReAlloc / HeapSize corrupted the process heap. These route them back.
static bool IsStdSmallBlock( const void *pMem );
static void StdFree( void *pMem );
static size_t StdGetSize( void *pMem );

// Round a size up to a multiple of 4 KiB to aid in calculating how much
// memory is required if full pageheap is enabled.
static constexpr size_t RoundUpToPage( size_t nSize )
{
	nSize += 0xFFF;
	nSize &= ~0xFFF;
	return nSize;
}

class CHeapMemAlloc final : public IMemAlloc
{
public:
	CHeapMemAlloc()
	{
		// Make sure that we return 64-bit addresses in 64-bit builds.
		ReserveBottomMemory();

		// Do all allocations with the shared process heap so that we can still
		// allocate from one DLL and free in another.
		m_heap = GetProcessHeap();
	}

	void Init( bool bZeroMemory )
	{
		m_HeapFlags = bZeroMemory ? HEAP_ZERO_MEMORY : 0;

		// Can't use Msg here because it isn't necessarily initialized yet.
		if ( s_bPageHeapEnabled )
		{
			OutputDebugStringA("PageHeap is on. Memory use will be larger than normal.\n" );
		}
		else
		{
			OutputDebugStringA("PageHeap is off. Memory use will be normal.\n" );
		}

		if ( bZeroMemory )
		{
			OutputDebugStringA( "  HEAP_ZERO_MEMORY is specified.\n" );
		}
	}

	// Release versions
	void *Alloc( size_t nSize ) override
	{
		// Ensure that the constructor has run already. Poorly defined
		// order of construction can result in the allocator being used
		// before it is constructed. Which could be bad.
		if ( !m_heap )
			__debugbreak();

		void* pMem = HeapAlloc( m_heap, m_HeapFlags, nSize );
		if ( pMem )
		{
			m_nOutstandingBytes.fetch_add( nSize, std::memory_order_relaxed );
			m_nOutstandingPageHeapBytes.fetch_add( RoundUpToPage( nSize ), std::memory_order_relaxed );
			m_nOutstandingAllocations.fetch_add( 1, std::memory_order_relaxed );
			m_nLifetimeAllocations.fetch_add( 1, std::memory_order_relaxed );
		}
		else if ( nSize )
		{
			// Having PageHeap enabled leads to lots of allocation failures. These
			// then lead to crashes. In order to avoid confusion about the cause of
			// these crashes, halt immediately on allocation failures.
			__debugbreak();
			m_nAllocFailures.fetch_add( 1, std::memory_order_relaxed );
		}

		return pMem;
	}

	void *Realloc( void *pMem, size_t nSize ) override
	{
		if ( pMem && IsStdSmallBlock( pMem ) )
		{
			// Move the small block into the process heap.
			if ( !nSize )
			{
				StdFree( pMem );
				return nullptr;
			}

			void *pNewMem = Alloc( nSize );
			if ( pNewMem )
			{
				memcpy( pNewMem, pMem, (std::min)( nSize, StdGetSize( pMem ) ) );
				StdFree( pMem );
			}
			return pNewMem;
		}

		// If you pass zero to HeapReAlloc then it fails (with GetLastError() saying S_OK!)
		// so only call HeapReAlloc if pMem is non-zero.
		if ( pMem )
		{
			if ( !nSize )
			{
				// Call the regular free function.
				Free( pMem );
				return 0;
			}

			size_t nOldSize = HeapSize( m_heap, 0, pMem );
			void* pNewMem = HeapReAlloc( m_heap, m_HeapFlags, pMem, nSize );

			// If we successfully allocated the requested memory (zero counts as
			// success if we requested zero bytes) then update the counters for the
			// change.
			if ( pNewMem )
			{
				if ( nSize >= nOldSize )
				{
					m_nOutstandingBytes.fetch_add( nSize - nOldSize, std::memory_order_relaxed );
					m_nOutstandingPageHeapBytes.fetch_add( RoundUpToPage( nSize ) - RoundUpToPage( nOldSize ), std::memory_order_relaxed );
				}
				else
				{
					m_nOutstandingBytes.fetch_sub( nOldSize - nSize, std::memory_order_relaxed );
					m_nOutstandingPageHeapBytes.fetch_sub( RoundUpToPage( nOldSize ) - RoundUpToPage( nSize ), std::memory_order_relaxed );
				}

				// Outstanding allocation count isn't affected by Realloc, but
				// lifetime allocation count is.
				m_nLifetimeAllocations.fetch_add( 1, std::memory_order_relaxed );
			}
			else
			{
				// Having PageHeap enabled leads to lots of allocation failures. These
				// then lead to crashes. In order to avoid confusion about the cause of
				// these crashes, halt immediately on allocation failures.
				__debugbreak();
				m_nAllocFailures.fetch_add( 1, std::memory_order_relaxed );
			}

			return pNewMem;
		}

		// Call the regular alloc function.
		return Alloc( nSize );
	}

	void Free( void *pMem ) override
	{
		if ( pMem && IsStdSmallBlock( pMem ) )
		{
			StdFree( pMem );
			return;
		}

		if ( pMem )
		{
			const size_t nOldSize = HeapSize( m_heap, 0, pMem );
			m_nOutstandingBytes.fetch_sub( nOldSize, std::memory_order_relaxed );
			m_nOutstandingPageHeapBytes.fetch_sub( RoundUpToPage( nOldSize ), std::memory_order_relaxed );
			m_nOutstandingAllocations.fetch_sub( 1, std::memory_order_relaxed );
			HeapFree( m_heap, 0, pMem );
		}
	}
	void *Expand_NoLongerSupported( void *pMem, size_t nSize ) override { return nullptr; }

	// Debug versions
	void *Alloc( size_t nSize, const char *pFileName, int nLine ) override { return Alloc( nSize ); }
	void *Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine ) override { return Realloc(pMem, nSize); }
	void  Free( void *pMem, const char *pFileName, int nLine ) override { Free( pMem ); }
	void *Expand_NoLongerSupported( void *pMem, size_t nSize, const char *pFileName, int nLine ) override { return nullptr; }

#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
	// Not currently implemented
#error "Please define your platform"
#endif

	virtual void *RegionAlloc( int region, size_t nSize ) { __debugbreak(); return nullptr; }
	virtual void *RegionAlloc( int region, size_t nSize, const char *pFileName, int nLine ) { __debugbreak(); return nullptr; }

	// Returns size of a particular allocation
	// If zero is returned then return the total size of allocated memory.
	virtual size_t GetSize( void *pMem )
	{
		if ( !pMem )
		{
			return m_nOutstandingBytes.load( std::memory_order_relaxed );
		}
		if ( IsStdSmallBlock( pMem ) )
		{
			return StdGetSize( pMem );
		}
		return HeapSize( m_heap, 0, pMem );
	}

    // Force file + line information for an allocation
	virtual void PushAllocDbgInfo( const char *pFileName, int nLine ) {}
    virtual void PopAllocDbgInfo() {}

	virtual long CrtSetBreakAlloc( long lNewBreakAlloc ) { return 0; }
	virtual	int CrtSetReportMode( int nReportType, int nReportMode ) { return 0; }
	virtual int CrtIsValidHeapPointer( const void *pMem ) { return 0; }
	virtual int CrtIsValidPointer( const void *pMem, unsigned int size, int access ) { return 0; }
	virtual int CrtCheckMemory( void ) { return 0; }
	virtual int CrtSetDbgFlag( int nNewFlag ) { return 0; }
	virtual void CrtMemCheckpoint( _CrtMemState *pState ) {}
	virtual void* CrtSetReportFile( int nRptType, void* hFile ) { return 0; }
	virtual void* CrtSetReportHook( void* pfnNewHook ) { return 0; }
	virtual int CrtDbgReport( int nRptType, const char * szFile,
			int nLine, const char * szModule, const char * pMsg ) { return 0; }
	virtual int heapchk() { return -2/*_HEAPOK*/; }

	virtual void DumpStats()
	{
		constexpr size_t MiB = 1024u * 1024;

		Msg( "Sorry -- no stats saved to file memstats.txt when the heap allocator is enabled.\n" );
		// Print requested memory.
		Msg( "%zu MiB allocated.\n", m_nOutstandingBytes.load( std::memory_order_relaxed ) / MiB );
		// Print memory after rounding up to pages.
		Msg( "%zu MiB assuming maximum PageHeap overhead.\n", m_nOutstandingPageHeapBytes.load( std::memory_order_relaxed ) / MiB );
		// Print memory after adding in reserved page after every allocation.  Do 64-bit calculations
		// because the pageHeap required memory can easily go over 4 GiB.
		// dimhotepus: uint64, size_t is 32-bit on x86 and this overflowed there.
		const uint64 pageHeapBytes = static_cast<uint64>( m_nOutstandingPageHeapBytes.load( std::memory_order_relaxed ) ) +
			static_cast<uint64>( m_nOutstandingAllocations.load( std::memory_order_relaxed ) ) * 4096;
		Msg( "%llu MiB address space used assuming maximum PageHeap overhead.\n",
			static_cast<unsigned long long>( pageHeapBytes / MiB ) );
		Msg( "%zu outstanding allocations (%zd delta).\n",
			m_nOutstandingAllocations.load(std::memory_order_relaxed),
			(intp)m_nOutstandingAllocations.load(std::memory_order_relaxed) - (intp)m_nOldOutstandingAllocations );
		Msg( "%zu lifetime allocations (%zd delta).\n",
			m_nLifetimeAllocations.load(std::memory_order_relaxed),
			(intp)m_nLifetimeAllocations.load(std::memory_order_relaxed) - (intp)m_nOldLifetimeAllocations );
		Msg( "%zu allocation failures.\n", m_nAllocFailures.load(std::memory_order_relaxed) );

		// Update the numbers on outstanding and lifetime allocation counts so
		// that we can print out deltas.
		m_nOldOutstandingAllocations = m_nOutstandingAllocations.load(std::memory_order_relaxed);
		m_nOldLifetimeAllocations = m_nLifetimeAllocations.load(std::memory_order_relaxed);
	}
	virtual void DumpStatsFileBase( char const *pchFileBase ) {}
	virtual size_t ComputeMemoryUsedBy( char const *pchSubStr ) { return 0; }
	virtual void GlobalMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory ) {}

	virtual bool IsDebugHeap() { return false; }

	virtual uint32 GetDebugInfoSize() { return 0; }
	virtual void SaveDebugInfo( void *pvDebugInfo ) { }
	virtual void RestoreDebugInfo( const void *pvDebugInfo ) {}	
	virtual void InitDebugInfo( void *pvDebugInfo, const char *pchRootFileName, int nLine ) {}

	virtual void GetActualDbgInfo( const char *&pFileName, int &nLine ) {}
	virtual void RegisterAllocation( const char *pFileName, int nLine, size_t nLogicalSize, size_t nActualSize, unsigned nTime ) {}
	virtual void RegisterDeallocation( const char *pFileName, int nLine, size_t nLogicalSize, size_t nActualSize, unsigned nTime ) {}

	virtual int GetVersion() { return MEMALLOC_VERSION; }

	virtual void OutOfMemory( size_t nBytesAttempted = 0 ) {}

	void CompactHeap() override {}
	virtual void CompactIncremental() {}

	virtual MemAllocFailHandler_t SetAllocFailHandler( MemAllocFailHandler_t pfnMemAllocFailHandler ) { return 0; }

	void DumpBlockStats( void *p ) {}

#if defined( _MEMTEST )
	// Not currently implemented
#error "Please define your platform"
#endif

	virtual size_t MemoryAllocFailed() { return 0; }

private:
	// Handle to the process heap.
	HANDLE m_heap{nullptr};
	uint32 m_HeapFlags{0};

	// Total outstanding bytes allocated.
	std::atomic_size_t m_nOutstandingBytes{0};

	// Total outstanding committed bytes assuming that all allocations are
	// put on individual 4-KiB pages (true when using full PageHeap from
	// App Verifier).
	std::atomic_size_t m_nOutstandingPageHeapBytes{0};

	// Total outstanding allocations. With PageHeap enabled each allocation
	// requires an extra 4-KiB page of address space.
	std::atomic_size_t m_nOutstandingAllocations{0};
	std::size_t m_nOldOutstandingAllocations{0};

	// Total allocations without subtracting freed memory.
	std::atomic_size_t m_nLifetimeAllocations{0};
	std::size_t m_nOldLifetimeAllocations{0};

	// Total number of allocation failures.
	std::atomic_size_t m_nAllocFailures{0};
};

#endif //ALLOW_PROCESS_HEAP

//-----------------------------------------------------------------------------
// Singletons...
//-----------------------------------------------------------------------------
#ifdef _MSC_VER
#pragma warning( disable:4074 ) // warning C4074: initializers put in compiler reserved initialization area
#pragma init_seg( compiler )
#endif

static CStdMemAlloc s_StdMemAlloc CONSTRUCT_EARLY;

#ifdef ALLOW_PROCESS_HEAP
static bool IsStdSmallBlock( [[maybe_unused]] const void *pMem )
{
#ifdef MEM_SBH_ENABLED
	return s_StdMemAlloc.m_SmallBlockHeap.IsOwner( const_cast<void *>( pMem ) );
#else
	return false;
#endif
}

static void StdFree( void *pMem )
{
	s_StdMemAlloc.Free( pMem );
}

static size_t StdGetSize( void *pMem )
{
	return s_StdMemAlloc.GetSize( pMem );
}
#endif

#ifndef TIER0_VALIDATE_HEAP
IMemAlloc *g_pMemAlloc = &s_StdMemAlloc;
#else
IMemAlloc *g_pActualAlloc = &s_StdMemAlloc;
#endif

#if defined(ALLOW_PROCESS_HEAP) && !defined(TIER0_VALIDATE_HEAP)
void EnableHeapMemAlloc( bool bZeroMemory )
{
	// Place this here to guarantee it is constructed
	// before we call Init.
	static CHeapMemAlloc s_HeapMemAlloc;
	// dimhotepus: Atomic exchange, so concurrent callers cannot both run Init().
	static std::atomic<bool> s_initCalled{false};

	if ( !s_initCalled.exchange( true, std::memory_order_acq_rel ) )
	{
		s_HeapMemAlloc.Init( bZeroMemory );
		g_pMemAlloc = &s_HeapMemAlloc;
	}
}

// Check whether PageHeap (part of App Verifier) has been enabled for this process.
// It specifically checks whether it was enabled by the EnableAppVerifier.bat
// batch file. This can be used to automatically enable -processheap when
// App Verifier is in use.
static bool IsPageHeapEnabled( bool& bETWHeapEnabled )
{
	// Assume false.
	bool result = false;
	bETWHeapEnabled = false;

	// First we get the application's name so we can look in the registry
	// for App Verifier settings.
	HMODULE exeHandle = GetModuleHandleA( nullptr );
	if ( exeHandle )
	{
		char appName[ MAX_PATH ];
		if ( GetModuleFileNameA( exeHandle, appName, std::size( appName ) ) )
		{
			// Guarantee null-termination -- not guaranteed on Windows XP!
			appName[ std::size( appName ) - 1 ] = '\0';
			// Find the file part of the name.
			const char* pFilePart = strrchr( appName, '\\' );
			if ( pFilePart )
			{
				++pFilePart;
				size_t len = strlen( pFilePart );
				if ( len > 0 && pFilePart[ len - 1 ] == ' ' )
				{
					OutputDebugStringA( "Trailing space on executable name! This will cause Application Verifier and ETW Heap tracing to fail!\n" );
					DebuggerBreakIfDebugging();
				}

				// Generate the key name for App Verifier settings for this process.
				char regPathName[ MAX_PATH ];
				snprintf( regPathName, std::size( regPathName ),
							"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\%s",
							pFilePart );

				HKEY key;
				if ( RegOpenKeyA( HKEY_LOCAL_MACHINE, regPathName, &key ) == ERROR_SUCCESS )
				{
					RunCodeAtScopeExit(RegCloseKey( key ));

					// If PageHeapFlags exists then that means that App Verifier is enabled
					// for this application. The StackTraceDatabaseSizeInMB is only
					// set by Valve's enabling batch file so this indicates that
					// a developer at Valve is using App Verifier.
					if ( RegQueryValueExA( key, "StackTraceDatabaseSizeInMB", 0, NULL, NULL, NULL ) == ERROR_SUCCESS &&
								RegQueryValueExA( key, "PageHeapFlags", 0, NULL, NULL, NULL) == ERROR_SUCCESS )
					{
						result = true;
					}

					if ( RegQueryValueExA( key, "TracingFlags", 0, NULL, NULL, NULL) == ERROR_SUCCESS )
						bETWHeapEnabled = true;
				}
			}
		}
	}

	return result;
}

// Check for various allocator overrides such as -processheap and -reservelowmem.
// Returns true if -processheap is enabled, by a command line switch or other method.
static bool CheckWindowsAllocSettings( const char* pszCommandLine )
{
	// Are we doing ETW heap profiling?
	bool bETWHeapEnabled = false;
	s_bPageHeapEnabled = IsPageHeapEnabled( bETWHeapEnabled );

	// Should we reserve the bottom 4 GiB of RAM in order to flush out pointer
	// truncation bugs? This helps ensure 64-bit compatibility.
	// However this needs to be off by default to avoid causing compatibility problems,
	// with Steam detours and other systems. It should also be disabled when PageHeap
	// is on because for some reason the combination turns into 4 GiB of working set, which
	// can easily cause problems.
	if ( CommandLineHasToken( pszCommandLine, "-RESERVELOWMEM" ) && !s_bPageHeapEnabled )
		ReserveBottomMemory();

	// Uninitialized data, including pointers, is often set to 0xFFEEFFEE.
	// If we reserve that block of memory then we can turn these pointer
	// dereferences into crashes a little bit earlier and more reliably.
	// We don't really care whether this allocation succeeds, but it's
	// worth trying. Note that we do this in all cases -- whether we are using
	// -processheap or not.
	// dimhotepus: Now on x64/ARM64 too, where it catches 0x00000000FFEEFFEE (a pointer that
	// went through a DWORD, or a fill-patterned 32-bit field used as a pointer) - the
	// full-width 0xFFEEFFEEFFEEFFEE faults by itself. Also covers the Windows heap/CRT
	// patterns and the next 64 KiB for member offsets. See mem_helpers.h.
	ReserveFillPatternAddresses();

	// Enable application termination (breakpoint) on heap corruption. This is
	// better than trying to patch it up and continue, both from a security and
	// a bug-finding point of view. Do this always on Windows since the heap is
	// used by video drivers and other in-proc components.
#if defined(PLATFORM_WINDOWS_PC)
	HeapSetInformation( NULL, HeapEnableTerminationOnCorruption, NULL, 0 );
#endif

	bool bZeroMemory = false;
	bool bProcessHeap = false;
	// Should we force using the process heap? This is handy for gathering memory
	// statistics with ETW/xperf. When using App Verifier -processheap is automatically
	// turned on.
	if ( CommandLineHasToken( pszCommandLine, "-PROCESSHEAP" ) )
	{
		bProcessHeap = true;
		bZeroMemory = CommandLineHasToken( pszCommandLine, "-PROCESSHEAPZEROMEM" );
	}

	// Unless specifically disabled, turn on -processheap if pageheap or ETWHeap tracing
	// are enabled.
	if ( !CommandLineHasToken( pszCommandLine, "-NOPROCESSHEAP" ) && ( s_bPageHeapEnabled || bETWHeapEnabled ) )
		bProcessHeap = true;

	if ( bProcessHeap )
	{
		// Now all allocations will go through the system heap.
		EnableHeapMemAlloc( bZeroMemory );
	}

	return bProcessHeap;
}

class CInitGlobalMemAllocPtr
{
public:
	CInitGlobalMemAllocPtr()
	{
		const char *pStr = Plat_GetCommandLineA();
		if ( pStr )
		{
			CheckWindowsAllocSettings( pStr );
		}

#if defined(FORCE_PROCESS_HEAP)
		// This may cause EnableHeapMemAlloc to be called twice, but that's okay.
		EnableHeapMemAlloc( false );
#endif
	}
};
CInitGlobalMemAllocPtr sg_InitGlobalMemAllocPtr;
#endif

#ifdef MEM_SBH_ENABLED
//-----------------------------------------------------------------------------
// Small block heap (multi-pool)
//-----------------------------------------------------------------------------

#ifndef NO_SBH
#ifdef ALLOW_NOSBH
static bool g_UsingSBH = true;
#define UsingSBH() g_UsingSBH
#else
#define UsingSBH() true
#endif
#else
#define UsingSBH() false
#endif

//-----------------------------------------------------------------------------
// dimhotepus: Address space primitives. Windows: VirtualAlloc/VirtualFree as before.
// POSIX: mmap/mprotect, so the SBH code builds (and can be enabled) everywhere.
//-----------------------------------------------------------------------------
#if defined( _WIN32 )
static byte *SbhReserve( size_t nBytes )
{
	return static_cast<byte *>( VirtualAlloc( nullptr, nBytes, MEM_RESERVE, PAGE_NOACCESS ) );
}

static bool SbhCommit( byte *p, size_t nBytes )
{
	return VirtualAlloc( p, nBytes, MEM_COMMIT, PAGE_READWRITE ) != nullptr;
}

static void SbhDecommit( byte *p, size_t nBytes )
{
	VirtualFree( p, nBytes, MEM_DECOMMIT );
}

static constexpr size_t SbhPageSize()
{
	return SBH_PAGE_SIZE;
}
#else
static byte *SbhReserve( size_t nBytes )
{
	void *p = mmap( nullptr, nBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0 );
	return p == MAP_FAILED ? nullptr : static_cast<byte *>( p );
}

static bool SbhCommit( byte *p, size_t nBytes )
{
	return mprotect( p, nBytes, PROT_READ | PROT_WRITE ) == 0;
}

static void SbhDecommit( byte *p, size_t nBytes )
{
	// Replacing the range with a fresh inaccessible mapping drops the pages on both Linux
	// and macOS (MADV_DONTNEED does not release memory on macOS).
	mmap( p, nBytes, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0 );
}

// Pages are 16 KiB on Apple Silicon and can be 64 KiB on Linux arm64; decommit
// boundaries must be multiples of the real page size.
static size_t SbhPageSize()
{
	static const size_t s_nPageSize = []
	{
		const long nPage = sysconf( _SC_PAGESIZE );
		return nPage > SBH_PAGE_SIZE ? static_cast<size_t>( nPage ) : static_cast<size_t>( SBH_PAGE_SIZE );
	}();
	return s_nPageSize;
}
#endif

static_assert( COMMIT_SIZE >= MAX_SBH_BLOCK, "One commit must always fit a block" );
static_assert( MAX_POOL_REGION % COMMIT_SIZE == 0, "Commits must tile the pool region" );

[[nodiscard]] static inline byte *AlignPointerUp( byte *p, size_t nAlign )
{
	const auto addr = reinterpret_cast<std::uintptr_t>( p );
	return p + ( ( ( addr + nAlign - 1 ) & ~static_cast<std::uintptr_t>( nAlign - 1 ) ) - addr );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

void CSmallBlockPool::Init( unsigned nBlockSize, byte *pBase, unsigned initialCommit )
{
	if ( !( nBlockSize % MIN_SBH_ALIGN == 0 && nBlockSize >= MIN_SBH_BLOCK && nBlockSize >= sizeof(TSLNodeBase_t) ) )
		DebuggerBreak();

	m_nBlockSize = nBlockSize;
	m_pBase = pBase;
	m_pNextAlloc.store( pBase, std::memory_order_relaxed );
	m_pCommitLimit.store( pBase, std::memory_order_relaxed );
	m_pAllocLimit = m_pBase + MAX_POOL_REGION;

	if ( initialCommit )
	{
		initialCommit = AlignValue( initialCommit, SBH_PAGE_SIZE );
		if ( !SbhCommit( pBase, initialCommit ) )
		{
			Assert( 0 );
			return;
		}
		m_pCommitLimit.store( pBase + initialCommit, std::memory_order_relaxed );
	}
}

uintp CSmallBlockPool::GetBlockSize() const
{
	return m_nBlockSize;
}

bool CSmallBlockPool::IsOwner( void *p ) const
{
	return ( p >= m_pBase && p < m_pAllocLimit );
}

// dimhotepus: Thread-safety design.
// * Freed blocks are reused through the lock-free free list (the hot path, unchanged).
// * Carving a never-used block (bump pointer) and committing pages now happen under
//   m_CommitMutex. They used to be lock-free, which let Compact() rewind the bump
//   pointer and decommit pages while another thread was carving a block from them:
//   the same block handed out twice, or a live block on a decommitted page. Compact()
//   runs from CompactHeap(), i.e. exactly when memory is short and threads are busy.
//   A block is carved only once in its lifetime, so the extra lock is cheap.
void *CSmallBlockPool::Alloc()
{
	if ( void *pResult = m_FreeList.Pop() )
		return pResult;

	AUTO_LOCK( m_CommitMutex );

	byte *pNextAlloc = m_pNextAlloc.load( std::memory_order_relaxed );
	byte *pCommitLimit = m_pCommitLimit.load( std::memory_order_relaxed );

	if ( static_cast<size_t>( pCommitLimit - pNextAlloc ) < m_nBlockSize )
	{
		// Sizes, not pointers, are compared so nothing is formed past the region.
		if ( static_cast<size_t>( m_pAllocLimit - pCommitLimit ) < COMMIT_SIZE )
			return nullptr;	// Pool region exhausted.

		if ( !SbhCommit( pCommitLimit, COMMIT_SIZE ) )
		{
			Assert( 0 );
			return nullptr;
		}

		pCommitLimit += COMMIT_SIZE;
		m_pCommitLimit.store( pCommitLimit, std::memory_order_release );
	}

	m_pNextAlloc.store( pNextAlloc + m_nBlockSize, std::memory_order_release );
	return pNextAlloc;
}

void CSmallBlockPool::Free( void *p )
{	
	Assert( IsOwner( p ) );

	m_FreeList.Push( p );
}

// Count the free blocks.  
// dimhotepus: Statistics only. CTSListBase::Count() is a uint16 depth and wraps at 65536,
// so this (and CountAllocatedBlocks) is approximate for huge free lists. Nothing that
// affects allocation relies on it any more (Compact drains the list instead).
int CSmallBlockPool::CountFreeBlocks() const
{
	return m_FreeList.Count();
}

// Size of committed memory managed by this heap:
uintp CSmallBlockPool::GetCommittedSize() const
{
	uintp totalSize = (uintp)m_pCommitLimit.load( std::memory_order_acquire ) - (uintp)m_pBase;
	Assert( 0 != m_nBlockSize );

	return totalSize;
}

// Return the total blocks memory is committed for in the heap
uintp CSmallBlockPool::CountCommittedBlocks() const
{		 
	return GetCommittedSize() / GetBlockSize();
}

// Count the number of allocated blocks in the heap:
uintp CSmallBlockPool::CountAllocatedBlocks() const
{
	// Statistics only: a snapshot, may be slightly off while other threads allocate.
	byte *pNextAlloc = m_pNextAlloc.load( std::memory_order_acquire );
	const uintp nCarved = static_cast<uintp>( pNextAlloc - m_pBase ) / GetBlockSize();
	const uintp nFree = static_cast<uintp>( (std::max)( CountFreeBlocks(), 0 ) );
	return nCarved > nFree ? nCarved - nFree : 0;
}

intp CSmallBlockPool::Compact()
{
	// dimhotepus: Exclusive with block carving/committing (see Alloc) and with another
	// Compact() of the same pool.
	AUTO_LOCK( m_CommitMutex );

	byte *const pOldNextAlloc = m_pNextAlloc.load( std::memory_order_relaxed );
	const size_t nCarved = static_cast<size_t>( pOldNextAlloc - m_pBase ) / m_nBlockSize;
	if ( !nCarved )
		return 0;

	// dimhotepus: Drain the free list completely instead of popping CountFreeBlocks()
	// blocks. CTSListBase keeps its depth in a uint16, so Count() wraps at 65536: a pool
	// with 65536 or 262144 free blocks reported 0 and was never compacted. (Other threads
	// popping concurrently also made the old count stale.)
	// Free blocks are marked in a bitmap over the carved range: 1 bit per block, at most
	// 32 KiB (64 KiB on 32-bit) per pool, versus up to 2 MiB for the old pointer array.
	// It also replaces the O(n log n) sort with an O(n) scan.
	const size_t nWords = ( nCarved + 63 ) / 64;
	auto *pFreeBits = static_cast<uint64 *>( calloc( nWords, sizeof( uint64 ) ) ); // can't use new because will reenter
	if ( !pFreeBits )
	{
		return 0;
	}

	// The popped blocks are linked through their own Next field; no extra memory.
	TSLNodeBase_t *pDrained = nullptr;
	while ( TSLNodeBase_t *pBlock = m_FreeList.Pop() )
	{
		const size_t iBlock = static_cast<size_t>( reinterpret_cast<byte *>( pBlock ) - m_pBase ) / m_nBlockSize;
		Assert( iBlock < nCarved );
		pFreeBits[iBlock / 64] |= uint64{ 1 } << ( iBlock % 64 );

		pBlock->Next = pDrained;
		pDrained = pBlock;
	}

	// Rewind the bump pointer over the free blocks at the top of the carved range.
	// Every block in [newTop, oldTop) is free and drained by us, and nobody can carve
	// meanwhile, so nothing live is affected.
	size_t nTop = nCarved;
	while ( nTop > 0 && ( pFreeBits[( nTop - 1 ) / 64] & ( uint64{ 1 } << ( ( nTop - 1 ) % 64 ) ) ) )
	{
		--nTop;
	}
	free( pFreeBits );

	byte *const pNextAlloc = m_pBase + nTop * m_nBlockSize;

	// Blocks below the new top go back to the free list. This walk must come BEFORE the
	// decommit below: it reads Next from every drained block, including those above the
	// new top whose pages are about to be released. (Next is read before Push rewrites it.)
	while ( pDrained )
	{
		TSLNodeBase_t *pNext = pDrained->Next;
		if ( reinterpret_cast<byte *>( pDrained ) < pNextAlloc )
		{
			m_FreeList.Push( pDrained );
		}
		pDrained = pNext;
	}

	intp nBytesFreed = 0;
	if ( pNextAlloc != pOldNextAlloc )
	{
		m_pNextAlloc.store( pNextAlloc, std::memory_order_release );

		byte *pNewCommitLimit = AlignPointerUp( pNextAlloc, SbhPageSize() );
		byte *pCommitLimit = m_pCommitLimit.load( std::memory_order_relaxed );
		if ( pNewCommitLimit < pCommitLimit )
		{
			nBytesFreed = pCommitLimit - pNewCommitLimit;
			m_pCommitLimit.store( pNewCommitLimit, std::memory_order_release );
			SbhDecommit( pNewCommitLimit, static_cast<size_t>( nBytesFreed ) );
		}
	}

	return nBytesFreed;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
#define GetInitialCommitForPool( i ) 0

CSmallBlockHeap::CSmallBlockHeap()
{
	// Make sure that we return 64-bit addresses in 64-bit builds.
	ReserveBottomMemory();

	if ( !UsingSBH() )
	{
		return;
	}

	byte *pBase = SbhReserve( static_cast<size_t>( NUM_POOLS ) * MAX_POOL_REGION );
	if ( !pBase )
	{
		// dimhotepus: Address space exhausted (e.g. 32-bit). This used to continue with
		// m_pBase == nullptr: IsOwner() then claimed every pointer below 168 MiB, so CRT
		// blocks in low memory were "freed" into the pools - heap corruption. Now the
		// small block heap is simply disabled.
		return;
	}

	m_pBase = pBase;
	m_pLimit = m_pBase + static_cast<size_t>( NUM_POOLS ) * MAX_POOL_REGION;

	// Build a lookup table used to find the correct pool based on size
	constexpr int MAX_TABLE = MAX_SBH_BLOCK >> 2;
	int i = 0;
	unsigned nBytesElement = 0;
	byte *pCurBase = m_pBase;
	CSmallBlockPool *pCurPool = nullptr;
	int iCurPool = 0;

	// Fills table entries [i, iEnd) with pools growing by nStep bytes; a new pool
	// starts every nStep / 4 entries (each entry covers 4 bytes of request size).
	const auto addPools = [&]( int iEnd, unsigned nStep )
	{
		const int nEntriesPerPool = static_cast<int>( nStep >> 2 );
		for ( ; i < iEnd && i < MAX_TABLE; i++ )
		{
			if ( ( i + 1 ) % nEntriesPerPool == 1 || nEntriesPerPool == 1 )
			{
				nBytesElement += nStep;
				pCurPool = &m_Pools[iCurPool];
				pCurPool->Init( nBytesElement, pCurBase, GetInitialCommitForPool(iCurPool) );
				iCurPool++;
				pCurBase += MAX_POOL_REGION;
			}
			m_PoolLookup[i] = pCurPool;
		}
	};

#if defined( PLATFORM_64BITS )
	// Blocks sized 0 - 256 are in pools in increments of 16
	// dimhotepus: All 64-bit targets (was _M_X64 only, ARM64 got 8-byte granularity).
	addPools( 64, 16 );
#else
	// Blocks sized 0 - 128 are in pools in increments of 8
	addPools( 32, 8 );
	// Blocks sized 129 - 256 are in pools in increments of 16
	addPools( 64, 16 );
#endif
	// Blocks sized 257 - 512 are in pools in increments of 32
	addPools( 128, 32 );
	// Blocks sized 513 - 768 are in pools in increments of 64
	addPools( 192, 64 );
	// Blocks sized 769 - 1024 are in pools in increments of 128
	addPools( 256, 128 );
	// Blocks sized 1025 - 2048 are in pools in increments of 256
	addPools( MAX_TABLE, 256 );

	Assert( iCurPool == NUM_POOLS );
}

bool CSmallBlockHeap::ShouldUse( size_t nBytes ) const
{
	return ( UsingSBH() && m_pBase && nBytes <= MAX_SBH_BLOCK );
}

bool CSmallBlockHeap::IsOwner( void * p ) const
{
	return ( UsingSBH() && m_pBase && p >= m_pBase && p < m_pLimit );
}

// Returns nullptr when the pool is exhausted; the caller then falls back to the CRT.
// dimhotepus: The CRT fallback used to live here too, so on CRT failure the OOM
// handling ran twice (here and again in CStdMemAlloc::Alloc).
void *CSmallBlockHeap::Alloc( size_t nBytes )
{
	if ( nBytes == 0)
	{
		nBytes = 1;
	}
	Assert( ShouldUse( nBytes ) );
	CSmallBlockPool *pPool = FindPool( nBytes );
	
	void *p = pPool->Alloc();
	if ( p )
	{
		return p;
	}

	if ( s_StdMemAlloc.CallAllocFailHandler( nBytes ) >= nBytes )
	{
		p = pPool->Alloc();
	}

	return p;
}

void *CSmallBlockHeap::Realloc( void *p, size_t nBytes )
{
	if ( nBytes == 0)
	{
		nBytes = 1;
	}

	CSmallBlockPool *pOldPool = FindPool( p );
	CSmallBlockPool *pNewPool = ( ShouldUse( nBytes ) ) ? FindPool( nBytes ) : nullptr;

	if ( pOldPool == pNewPool )
	{
		return p;
	}

	void *pNewBlock = nullptr;

	if ( pNewPool )
	{
		pNewBlock = pNewPool->Alloc();

		if ( !pNewBlock )
		{
			if ( s_StdMemAlloc.CallAllocFailHandler( nBytes ) >= nBytes )
			{
				pNewBlock = pNewPool->Alloc();
			}
		}
	}

	if ( !pNewBlock )
	{
		pNewBlock = malloc( nBytes );
		if ( !pNewBlock )
		{
			s_StdMemAlloc.SetCRTAllocFailed( nBytes );
			// dimhotepus: Like realloc(), keep the original block on failure. It used to be
			// freed unconditionally: data loss, and a dangling pointer for the caller.
			return nullptr;
		}
	}

	const size_t nBytesCopy = (std::min)( nBytes, static_cast<size_t>( pOldPool->GetBlockSize() ) );
	memcpy( pNewBlock, p, nBytesCopy );

	pOldPool->Free( p );

	return pNewBlock;
}

void CSmallBlockHeap::Free( void *p )
{
	CSmallBlockPool *pPool = FindPool( p );
	pPool->Free( p );
}

uintp CSmallBlockHeap::GetSize( void *p ) const
{
	const CSmallBlockPool *pPool = FindPool( p );
	return pPool->GetBlockSize();
}

void CSmallBlockHeap::DumpStats( FILE *pFile )
{
	if ( !m_pBase )
	{
		if ( pFile )
			fprintf( pFile, "Small block heap is disabled.\n" );
		else
			Msg( "Small block heap is disabled.\n" );
		return;
	}

	bool bSpew = true;

	if ( pFile )
	{
		for ( int i = 0; i < NUM_POOLS; i++ )
		{
			// output for vxconsole parsing
			fprintf( pFile, "Pool %i: Size: %zu Allocated: %zu Free: %i Committed: %zu CommittedSize: %zu\n", 
				i, 
				static_cast<size_t>( m_Pools[i].GetBlockSize() ), 
				static_cast<size_t>( m_Pools[i].CountAllocatedBlocks() ), 
				m_Pools[i].CountFreeBlocks(),
				static_cast<size_t>( m_Pools[i].CountCommittedBlocks() ), 
				static_cast<size_t>( m_Pools[i].GetCommittedSize() ) );
		}
		bSpew = false;
	}

	if ( bSpew )
	{
		size_t bytesCommitted = 0, bytesAllocated = 0;

		for ( int i = 0; i < NUM_POOLS; i++ )
		{
			Msg( "Pool %i: (size: %zu) blocks: allocated:%zu free:%i committed:%zu (committed size:%zu KiB)\n",
				i,
				static_cast<size_t>( m_Pools[i].GetBlockSize() ),
				static_cast<size_t>( m_Pools[i].CountAllocatedBlocks() ),
				m_Pools[i].CountFreeBlocks(),
				static_cast<size_t>( m_Pools[i].CountCommittedBlocks() ),
				static_cast<size_t>( m_Pools[i].GetCommittedSize() / 1024 ) );

			bytesCommitted += m_Pools[i].GetCommittedSize();
			bytesAllocated += ( m_Pools[i].CountAllocatedBlocks() * m_Pools[i].GetBlockSize() );
		}

		Msg( "Totals: Committed:%zu KiB Allocated:%zu KiB\n", bytesCommitted / 1024, bytesAllocated / 1024 );
	}
}

intp CSmallBlockHeap::Compact()
{
	if ( !m_pBase )
		return 0;

	intp nBytesFreed = 0;
	for( intp i = 0; i < NUM_POOLS; i++ )
	{
		nBytesFreed += m_Pools[i].Compact();
	}
	return nBytesFreed;
}

CSmallBlockPool *CSmallBlockHeap::FindPool( size_t nBytes ) const
{
	return m_PoolLookup[(nBytes - 1) >> 2];
}

CSmallBlockPool *CSmallBlockHeap::FindPool( void *p )
{
	size_t i = static_cast<size_t>( (byte *)p - m_pBase ) / MAX_POOL_REGION;
	return &m_Pools[i];
}

const CSmallBlockPool *CSmallBlockHeap::FindPool( void *p ) const
{
	size_t i = static_cast<size_t>( (byte *)p - m_pBase ) / MAX_POOL_REGION;
	return &m_Pools[i];
}

#endif // MEM_SBH_ENABLED

//-----------------------------------------------------------------------------
// Release versions
//-----------------------------------------------------------------------------

void *CStdMemAlloc::Alloc( size_t nSize )
{
	PROFILE_ALLOC(Malloc);
	
	void *pMem;

#ifdef MEM_SBH_ENABLED
	if ( m_SmallBlockHeap.ShouldUse( nSize ) )
	{
		pMem = m_SmallBlockHeap.Alloc( nSize );

		// dimhotepus: Fallback to common allocator if pool has no empty blocks.
		if ( pMem )
		{
			ApplyMemoryInitializations( pMem, nSize );
			return pMem;
		}
	}
#endif

	pMem = malloc( nSize );
	if ( !pMem )
	{
		// dimhotepus: Try to free some space and allocate again.
		CompactHeap();

		pMem = malloc( nSize );
		if ( !pMem )
		{
			SetCRTAllocFailed( nSize );
			return nullptr;
		}
	}

	ApplyMemoryInitializations( pMem, nSize );
	return pMem;
}

void *CStdMemAlloc::Realloc( void *pMem, size_t nSize )
{
	if ( !pMem )
	{
		return Alloc( nSize );
	}

	PROFILE_ALLOC(Realloc);

#ifdef MEM_SBH_ENABLED
	if ( m_SmallBlockHeap.IsOwner( pMem ) )
	{
		return m_SmallBlockHeap.Realloc( pMem, nSize );
	}
#endif

	// dimhotepus: realloc( p, 0 ) frees p and returns nullptr on both the UCRT and glibc.
	// That nullptr was taken for a failure: CompactHeap() and then realloc() on the
	// already freed block (use-after-free / double free), plus a bogus OOM report.
	// Keep a minimal block instead, consistent with the small block heap path.
	if ( nSize == 0 )
	{
		nSize = 1;
	}

	void *pRet = realloc( pMem, nSize );
	if ( !pRet )
	{
		// dimhotepus: Try to free some space and allocate again.
		CompactHeap();

		pRet = realloc( pMem, nSize );
		if ( !pRet )
		{
			SetCRTAllocFailed( nSize );
		}
	}
	return pRet;
}

void CStdMemAlloc::Free( void *pMem )
{
	if ( !pMem )
	{
		return;
	}

	PROFILE_ALLOC(Free);

#ifdef MEM_SBH_ENABLED
	if ( m_SmallBlockHeap.IsOwner( pMem ) )
	{
		m_SmallBlockHeap.Free( pMem );
		return;
	}
#endif

	free( pMem );
}

void *CStdMemAlloc::Expand_NoLongerSupported( void * /*pMem*/, size_t /*nSize*/ )
{
	return NULL;
}


//-----------------------------------------------------------------------------
// Debug versions
//-----------------------------------------------------------------------------
void *CStdMemAlloc::Alloc( size_t nSize, const char * /*pFileName*/, int /*nLine*/ )
{
	return CStdMemAlloc::Alloc( nSize );
}

void *CStdMemAlloc::Realloc( void *pMem, size_t nSize, const char * /*pFileName*/, int /*nLine*/ )
{
	return CStdMemAlloc::Realloc( pMem, nSize );
	}

void  CStdMemAlloc::Free( void *pMem, const char * /*pFileName*/, int /*nLine*/ )
{
	CStdMemAlloc::Free( pMem );
}

void *CStdMemAlloc::Expand_NoLongerSupported( void *, size_t, const char *, int )
{
	return NULL;
}

//-----------------------------------------------------------------------------
// Returns size of a particular allocation
// dimhotepus: Same contract on every platform: nullptr -> bytes in use by the heap
// (POSIX used to return malloc_usable_size( nullptr ) == 0).
//-----------------------------------------------------------------------------
size_t CStdMemAlloc::GetSize( void *pMem )
{
	if ( !pMem )
		return CalcHeapUsed();

#ifdef MEM_SBH_ENABLED
	if ( m_SmallBlockHeap.IsOwner( pMem ) )
	{
		return m_SmallBlockHeap.GetSize( pMem );
	}
#endif

#if defined( _WIN32 )
	return _msize( pMem );
#elif defined( __APPLE__ )
	return malloc_size( pMem );
#else
	return malloc_usable_size( pMem );
#endif
}


//-----------------------------------------------------------------------------
// Force file + line information for an allocation
//-----------------------------------------------------------------------------
void CStdMemAlloc::PushAllocDbgInfo( const char *, int )
{
}

void CStdMemAlloc::PopAllocDbgInfo()
{
}

//-----------------------------------------------------------------------------
// FIXME: Remove when we make our own heap! Crt stuff we're currently using
//-----------------------------------------------------------------------------
long CStdMemAlloc::CrtSetBreakAlloc( long )
{
	return 0;
}

int CStdMemAlloc::CrtSetReportMode( int, int )
{
	return 0;
}

int CStdMemAlloc::CrtIsValidHeapPointer( const void * )
{
	return 1;
}

int CStdMemAlloc::CrtIsValidPointer( const void *, unsigned int, int )
{
	return 1;
}

int CStdMemAlloc::CrtCheckMemory( void )
{
	return 1;
}

int CStdMemAlloc::CrtSetDbgFlag( int )
{
	return 0;
}

void CStdMemAlloc::CrtMemCheckpoint( _CrtMemState * )
{
}

// FIXME: Remove when we have our own allocator
void* CStdMemAlloc::CrtSetReportFile( int, void* )
{
	return 0;
}

void* CStdMemAlloc::CrtSetReportHook( void* )
{
	return 0;
}

int CStdMemAlloc::CrtDbgReport( int, const char *,
	int, const char *, const char * )
{
	return 0;
}

int CStdMemAlloc::heapchk()
{
#ifdef _WIN32
	return _HEAPOK;
#else
	return 1;
#endif
}

void CStdMemAlloc::DumpStats() 
{ 
	DumpStatsFileBase( "memstats" );
}

void CStdMemAlloc::DumpStatsFileBase( char const *pchFileBase )
{
	// dimhotepus: On every platform (was Windows-only); snprintf always terminates.
	char filename[ 512 ];
	snprintf( filename, std::size( filename ), "%s.txt", pchFileBase );
	FILE *pFile = fopen( filename, "w" );
	if (pFile)
	{
		RunCodeAtScopeExit( fclose( pFile ) );

		fprintf( pFile, "Heap used: %zu bytes\n", CalcHeapUsed() );
#ifdef MEM_SBH_ENABLED
		fprintf( pFile, "\nSBH:\n" );
		m_SmallBlockHeap.DumpStats(pFile);	// Dump statistics to small block heap
#endif
	}
	else
	{
		Warning( "Unable to open '%s' to dump small block heap stats.", filename );
	}
}

void CStdMemAlloc::GlobalMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory )
{
	if ( !pUsedMemory || !pFreeMemory )
		return;

	// no data
	*pFreeMemory = 0;
	*pUsedMemory = 0;
}

void CStdMemAlloc::CompactHeap()
{
#if !defined( NO_SBH ) && defined( MEM_SBH_ENABLED )
	intp nBytesRecovered = m_SmallBlockHeap.Compact();
	Msg( "Compact heap freed %zd  bytes from small block heap.\n", nBytesRecovered );
#endif

#if defined( _WIN32 )

	// dimhotepus: Cleanup caches and decommit if possible.
	// If HeapSetInformation is called with HeapHandle set to NULL, then all heaps
	// in the process with a low-fragmentation heap (LFH) will have their caches
	// optimized, and the memory will be decommitted if possible.
	//
	// See
	// https://docs.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapsetinformation
	HEAP_OPTIMIZE_RESOURCES_INFORMATION information{
		HEAP_OPTIMIZE_RESOURCES_CURRENT_VERSION, 0U};
	const BOOL ok = ::HeapSetInformation(
		nullptr, HeapOptimizeResources, &information, sizeof(information));

	// dimhotepus: Last attempt to allow app allocate smth later.
	const size_t largestFreeBytes = HeapCompact( ::GetProcessHeap(), 0 );
	
	Msg( "Compacted heap. Largest free block is %zu bytes, optimize heap %s.\n",
		largestFreeBytes, ok ? "OK" : "FAIL" );
#elif defined( __APPLE__ )
	// dimhotepus: POSIX used to do nothing here. Return free pages of all zones to the OS.
	const size_t nReleased = malloc_zone_pressure_relief( nullptr, 0 );
	Msg( "Compacted heap. Released %zu bytes.\n", nReleased );
#elif defined( __GLIBC__ )
	// Trim free memory at the top of every arena back to the OS.
	const int bReleased = malloc_trim( 0 );
	Msg( "Compacted heap. Memory %s released.\n", bReleased ? "was" : "was not" );
#endif
}

MemAllocFailHandler_t CStdMemAlloc::SetAllocFailHandler( MemAllocFailHandler_t pfnMemAllocFailHandler )
{
	// dimhotepus: nullptr would crash the next CallAllocFailHandler(); use the default.
	return m_pfnFailHandler.exchange( pfnMemAllocFailHandler ? pfnMemAllocFailHandler : DefaultFailHandler,
		std::memory_order_acq_rel );
}

size_t CStdMemAlloc::DefaultFailHandler( size_t )
{
	return 0;
}

#if defined( _MEMTEST )
void CStdMemAlloc::SetStatsExtraInfo( const char *, const char * )
{
}
#endif

void CStdMemAlloc::SetCRTAllocFailed( size_t nSize )
{
	m_sMemoryAllocFailed.store( nSize, std::memory_order_relaxed );

	MemAllocOOMError( nSize );
}

size_t CStdMemAlloc::MemoryAllocFailed()
{
	return m_sMemoryAllocFailed.load( std::memory_order_relaxed );
}

#endif

void ReserveBottomMemory()
{
	// If we are running a 64-bit build then reserve all addresses below the
	// 4 GiB line to push as many pointers as possible above the line.
#ifdef PLATFORM_WINDOWS_PC64
	// Avoid the cost of calling this multiple times.
	// dimhotepus: Atomic, it is called from several static constructors and at runtime.
	static std::atomic<bool> s_initialized{false};
	if ( s_initialized.exchange( true, std::memory_order_acq_rel ) )
		return;

	// If AppVerifier is enabled then memory reservations get turned into committed
	// memory in the working set. This means that ReserveBottomMemory() can end
	// up adding almost 4 GiB to the working set, which is a significant problem if
	// you run many processes in parallel. Therefore, if vfbasics.dll (part of AppVerifier)
	// is loaded, don't do the reservation.
	HMODULE vfBasicsDLL = GetModuleHandleA( "vfbasics.dll" );
	if ( vfBasicsDLL )
		return;

	// Start by reserving large blocks of memory. When those reservations
	// have exhausted the bottom 4 GiB then halve the size and try again.
	// The granularity for reserving address space is 64 KiB so if we wanted
	// to reserve every single page we would need to continue down to 64 KiB.
	// However stopping at 1 MiB is sufficient because it prevents the Windows
	// heap (and dlmalloc and the small block heap) from grabbing address space
	// from the bottom 4 GiB, while still allowing Steam to allocate a few pages
	// for setting up detours.
	constexpr size_t LOW_MEM_LINE = 0x100000000LL;
	size_t totalReservation = 0;
	size_t numVAllocs = 0;
	size_t numHeapAllocs = 0;
	for ( size_t blockSize = 256 * 1024 * 1024; blockSize >= 1024 * 1024; blockSize /= 2 )
	{
		for (;;)
		{
			void* p = VirtualAlloc( nullptr, blockSize, MEM_RESERVE, PAGE_NOACCESS );
			if ( !p )
				break;

			if ( reinterpret_cast<size_t>( p ) >= LOW_MEM_LINE )
			{
				// We don't need this memory, so release it completely.
				VirtualFree( p, 0, MEM_RELEASE );
				break;
			}

			totalReservation += blockSize;
			++numVAllocs;
		}
	}

	// Now repeat the same process but making heap allocations, to use up the
	// already committed heap blocks that are below the 4 GiB line. Now we start
	// with 64-KiB allocations and proceed down to 16-byte allocations.
	HANDLE heap = GetProcessHeap();
	for ( size_t blockSize = 64 * 1024; blockSize >= 16; blockSize /= 2 )
	{
		for (;;)
		{
			void* p = HeapAlloc( heap, 0, blockSize );
			if ( !p )
				break;

			if ( reinterpret_cast<size_t>( p ) >= LOW_MEM_LINE )
			{
				// We don't need this memory, so release it completely.
				HeapFree( heap, 0, p );
				break;
			}

			totalReservation += blockSize;
			++numHeapAllocs;
		}
	}

	// Print diagnostics showing how many allocations we had to make in order to
	// reserve all of low memory. In one test run it took 55 virtual allocs and
	// 85 heap allocs. Note that since the process may have multiple heaps (each
	// CRT seems to have its own) there is likely to be a few MiB of address space
	// that was previously reserved and is available to be handed out by some allocators.
	char buffer[1000];
	snprintf(
		buffer,
		std::size(buffer),
		"[tier0] Reserved %1.3f MiB (%zu vallocs, %zu heap allocs) to keep allocations out of low-memory in x64.\n",
		totalReservation / (1024 * 1024.0),
		numVAllocs,
		numHeapAllocs );
	// Can't use Msg here because it isn't necessarily initialized yet.
	Plat_DebugString( buffer );
#endif
}

#endif // STEAM