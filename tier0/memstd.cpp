// Copyright Valve Corporation, All rights reserved.
//
// Memory allocation!

#include "stdafx.h"

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

#if defined( _WIN32 )
#define VA_COMMIT_FLAGS MEM_COMMIT
#define VA_RESERVE_FLAGS MEM_RESERVE
#endif

#include <algorithm>

#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include "tier0/threadtools.h"
#include "mem_helpers.h"
#include "memstd.h"

// Force on redirecting all allocations to the process heap on Win64,
// which currently means the GC.  This is to make AppVerifier more effective
// at catching memory stomps.
#if defined( _WIN64 )

	#define FORCE_PROCESS_HEAP

#elif defined( _WIN32 )
// Define this to force using the OS Heap* functions for allocations. This is useful
// in conjunction with AppVerifier/PageHeap in order to find memory problems, and
// also allows ETW/xperf tracing to be used to record allocations.
// Normally the command-line option -processheap can be used instead.
//#define FORCE_PROCESS_HEAP

#define ALLOW_PROCESS_HEAP
#endif

#if (!defined(_DEBUG) && !defined(USE_MEM_DEBUG))
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

#if (!defined(_DEBUG) && !defined(USE_MEM_DEBUG))

// Support for CHeapMemAlloc for easy switching to using the process heap.
#ifdef ALLOW_PROCESS_HEAP

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
			m_nOutstandingBytes.fetch_add( nSize, std::memory_order::memory_order_relaxed );
			m_nOutstandingPageHeapBytes.fetch_add( RoundUpToPage( nSize ), std::memory_order::memory_order_relaxed );
			m_nOutstandingAllocations.fetch_add( 1, std::memory_order::memory_order_relaxed );
			m_nLifetimeAllocations.fetch_add( 1, std::memory_order::memory_order_relaxed );
		}
		else if ( nSize )
		{
			// Having PageHeap enabled leads to lots of allocation failures. These
			// then lead to crashes. In order to avoid confusion about the cause of
			// these crashes, halt immediately on allocation failures.
			__debugbreak();
			m_nAllocFailures.fetch_add( 1, std::memory_order::memory_order_relaxed );
		}

		return pMem;
	}

	void *Realloc( void *pMem, size_t nSize ) override
	{
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
					m_nOutstandingBytes.fetch_add( nSize - nOldSize, std::memory_order::memory_order_relaxed );
					m_nOutstandingPageHeapBytes.fetch_add( RoundUpToPage( nSize ) - RoundUpToPage( nOldSize ), std::memory_order::memory_order_relaxed );
				}
				else
				{
					m_nOutstandingBytes.fetch_sub( nOldSize - nSize, std::memory_order::memory_order_relaxed );
					m_nOutstandingPageHeapBytes.fetch_sub( RoundUpToPage( nOldSize ) - RoundUpToPage( nSize ), std::memory_order::memory_order_relaxed );
				}

				// Outstanding allocation count isn't affected by Realloc, but
				// lifetime allocation count is.
				m_nLifetimeAllocations.fetch_add( 1, std::memory_order::memory_order_relaxed );
			}
			else
			{
				// Having PageHeap enabled leads to lots of allocation failures. These
				// then lead to crashes. In order to avoid confusion about the cause of
				// these crashes, halt immediately on allocation failures.
				__debugbreak();
				m_nAllocFailures.fetch_add( 1, std::memory_order::memory_order_relaxed );
			}

			return pNewMem;
		}

		// Call the regular alloc function.
		return Alloc( nSize );
	}

	void Free( void *pMem ) override
	{
		if ( pMem )
		{
			const size_t nOldSize = HeapSize( m_heap, 0, pMem );
			m_nOutstandingBytes.fetch_sub( nOldSize, std::memory_order::memory_order_relaxed );
			m_nOutstandingPageHeapBytes.fetch_sub( RoundUpToPage( nOldSize ), std::memory_order::memory_order_relaxed );
			m_nOutstandingAllocations.fetch_sub( 1, std::memory_order::memory_order_relaxed );
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
			return m_nOutstandingBytes;
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
		Msg( "%zu MiB allocated.\n", m_nOutstandingBytes / MiB );
		// Print memory after rounding up to pages.
		Msg( "%zu MiB assuming maximum PageHeap overhead.\n", m_nOutstandingPageHeapBytes / MiB );
		// Print memory after adding in reserved page after every allocation.  Do 64-bit calculations
		// because the pageHeap required memory can easily go over 4 GiB.
		const size_t pageHeapBytes = m_nOutstandingPageHeapBytes + m_nOutstandingAllocations * 4096;
		Msg( "%zu MiB address space used assuming maximum PageHeap overhead.\n", pageHeapBytes / MiB );
		Msg( "%zu outstanding allocations (%zd delta).\n",
			m_nOutstandingAllocations.load(std::memory_order::memory_order_relaxed),
			(intp)m_nOutstandingAllocations.load(std::memory_order::memory_order_relaxed) - (intp)m_nOldOutstandingAllocations );
		Msg( "%zu lifetime allocations (%u delta).\n",
			m_nLifetimeAllocations.load(std::memory_order::memory_order_relaxed),
			(intp)m_nLifetimeAllocations.load(std::memory_order::memory_order_relaxed) - (intp)m_nOldLifetimeAllocations );
		Msg( "%zu allocation failures.\n", m_nAllocFailures.load(std::memory_order::memory_order_relaxed) );

		// Update the numbers on outstanding and lifetime allocation counts so
		// that we can print out deltas.
		m_nOldOutstandingAllocations = m_nOutstandingAllocations.load(std::memory_order::memory_order_relaxed);
		m_nOldLifetimeAllocations = m_nLifetimeAllocations.load(std::memory_order::memory_order_relaxed);
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
	HANDLE m_heap;
	uint32 m_HeapFlags;

	// Total outstanding bytes allocated.
	std::atomic_size_t m_nOutstandingBytes;

	// Total outstanding committed bytes assuming that all allocations are
	// put on individual 4-KiB pages (true when using full PageHeap from
	// App Verifier).
	std::atomic_size_t m_nOutstandingPageHeapBytes;

	// Total outstanding allocations. With PageHeap enabled each allocation
	// requires an extra 4-KiB page of address space.
	std::atomic_size_t m_nOutstandingAllocations;
	std::size_t m_nOldOutstandingAllocations;

	// Total allocations without subtracting freed memory.
	std::atomic_size_t m_nLifetimeAllocations;
	std::size_t m_nOldLifetimeAllocations;

	// Total number of allocation failures.
	std::atomic_size_t m_nAllocFailures;
};

#endif //ALLOW_PROCESS_HEAP

//-----------------------------------------------------------------------------
// Singletons...
//-----------------------------------------------------------------------------
#pragma warning( disable:4074 ) // warning C4074: initializers put in compiler reserved initialization area
#pragma init_seg( compiler )

static CStdMemAlloc s_StdMemAlloc CONSTRUCT_EARLY;

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
	static bool s_initCalled = false;

	if ( !s_initCalled )
	{
		s_HeapMemAlloc.Init( bZeroMemory );
		g_pMemAlloc = &s_HeapMemAlloc;
		s_initCalled = true;
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
	HMODULE exeHandle = GetModuleHandle( nullptr );
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
				_snprintf( regPathName, std::size( regPathName ),
							"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\%s",
							pFilePart );
				regPathName[ std::size( regPathName ) - 1 ] = '\0';

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
static bool CheckWindowsAllocSettings( const char* upperCommandLine )
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
	if ( strstr( upperCommandLine, "-RESERVELOWMEM" ) && !s_bPageHeapEnabled )
		ReserveBottomMemory();

	// Uninitialized data, including pointers, is often set to 0xFFEEFFEE.
	// If we reserve that block of memory then we can turn these pointer
	// dereferences into crashes a little bit earlier and more reliably.
	// We don't really care whether this allocation succeeds, but it's
	// worth trying. Note that we do this in all cases -- whether we are using
	// -processheap or not.
#ifdef PLATFORM_64BITS
	// dimhotepus: x86-64 0xFFEEFFEE->0xFFEEFFEEFFEEFFEE
	VirtualAlloc( (void*)0xFFEEFFEEFFEEFFEE, 1, MEM_RESERVE, PAGE_NOACCESS );
#else
	VirtualAlloc( (void*)0xFFEEFFEE, 1, MEM_RESERVE, PAGE_NOACCESS );
#endif

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
	if ( strstr( upperCommandLine, "-PROCESSHEAP" ) )
	{
		bProcessHeap = true;
		bZeroMemory = !!strstr( upperCommandLine, "-PROCESSHEAPZEROMEM" );
	}

	// Unless specifically disabled, turn on -processheap if pageheap or ETWHeap tracing
	// are enabled.
	if ( !strstr( upperCommandLine, "-NOPROCESSHEAP" ) && ( s_bPageHeapEnabled || bETWHeapEnabled ) )
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
			char tempStr[512];
			strncpy( tempStr, pStr, std::size( tempStr ) );
			tempStr[ std::size( tempStr ) - 1 ] = '\0';
			_strupr( tempStr );

			CheckWindowsAllocSettings( tempStr );
		}

#if defined(FORCE_PROCESS_HEAP)
		// This may cause EnableHeapMemAlloc to be called twice, but that's okay.
		EnableHeapMemAlloc( false );
#endif
	}
};
CInitGlobalMemAllocPtr sg_InitGlobalMemAllocPtr;
#endif

#ifdef _WIN32
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
// 
//-----------------------------------------------------------------------------

void CSmallBlockPool::Init( unsigned nBlockSize, byte *pBase, unsigned initialCommit )
{
	if ( !( nBlockSize % MIN_SBH_ALIGN == 0 && nBlockSize >= MIN_SBH_BLOCK && nBlockSize >= sizeof(TSLNodeBase_t) ) )
		DebuggerBreak();

	m_nBlockSize = nBlockSize;
	m_pCommitLimit = m_pNextAlloc = m_pBase = pBase;
	m_pAllocLimit = m_pBase + MAX_POOL_REGION;

	if ( initialCommit )
	{
		initialCommit = AlignValue( initialCommit, SBH_PAGE_SIZE );
		if ( !VirtualAlloc( m_pCommitLimit, initialCommit, VA_COMMIT_FLAGS, PAGE_READWRITE ) )
		{
			Assert( 0 );
			return;
		}
		m_pCommitLimit += initialCommit;
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

void *CSmallBlockPool::Alloc()
{
	void *pResult = m_FreeList.Pop();
	if ( !pResult )
	{
		int nBlockSize = m_nBlockSize;
		byte *pCommitLimit;
		byte *pNextAlloc;

		for (;;)
		{
			pCommitLimit = m_pCommitLimit;
			pNextAlloc = m_pNextAlloc;
			if ( pNextAlloc + nBlockSize <= pCommitLimit )
			{
				if ( m_pNextAlloc.AssignIf( pNextAlloc, pNextAlloc + m_nBlockSize ) )
				{
					pResult = pNextAlloc;
					break;
				}
			}
			else
			{
				AUTO_LOCK( m_CommitMutex );
				if ( pCommitLimit == m_pCommitLimit )
				{
					if ( pCommitLimit + COMMIT_SIZE <= m_pAllocLimit )
					{
						if ( !VirtualAlloc( pCommitLimit, COMMIT_SIZE, VA_COMMIT_FLAGS, PAGE_READWRITE ) )
						{
							Assert( 0 );
							return NULL;
						}

						m_pCommitLimit = pCommitLimit + COMMIT_SIZE;
					}
					else
					{
						return NULL;
					}
				}
			}
		}
	}
	return pResult;
}

void CSmallBlockPool::Free( void *p )
{	
	Assert( IsOwner( p ) );

	m_FreeList.Push( p );
}

// Count the free blocks.  
int CSmallBlockPool::CountFreeBlocks() const
{
	return m_FreeList.Count();
}

// Size of committed memory managed by this heap:
uintp CSmallBlockPool::GetCommittedSize() const
{
	uintp totalSize = (uintp)m_pCommitLimit - (uintp)m_pBase;
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
	return CountCommittedBlocks( ) - ( CountFreeBlocks( ) + ( m_pCommitLimit - (byte *)m_pNextAlloc ) / GetBlockSize() );
}

intp CSmallBlockPool::Compact()
{
	intp nBytesFreed = 0;
	if ( m_FreeList.Count() )
	{
		const int nFree = CountFreeBlocks();
		FreeBlock_t **pSortArray = (FreeBlock_t **)malloc( nFree * sizeof(FreeBlock_t *) ); // can't use new because will reenter

		if ( !pSortArray )
		{
			return 0;
		}

		int i = 0;
		while ( i < nFree )
		{
			pSortArray[i++] = m_FreeList.Pop();
		}

		std::sort( pSortArray, pSortArray + nFree );

		byte *pOldNextAlloc = m_pNextAlloc;

		for ( i = nFree - 1; i >= 0; i-- )
		{
			if ( (byte *)pSortArray[i] == static_cast<byte*>( m_pNextAlloc ) - m_nBlockSize )
			{
				pSortArray[i] = NULL;
				m_pNextAlloc -= m_nBlockSize;
			}
			else
			{
				break;
			}
		}

		if ( pOldNextAlloc != m_pNextAlloc )
		{
			byte *pNewCommitLimit = AlignValue( (byte *)m_pNextAlloc, SBH_PAGE_SIZE );
			if ( pNewCommitLimit < m_pCommitLimit )
			{
				nBytesFreed = m_pCommitLimit - pNewCommitLimit;
				VirtualFree( pNewCommitLimit, nBytesFreed, MEM_DECOMMIT );
				m_pCommitLimit = pNewCommitLimit;
			}
		}

		if ( pSortArray[0] )
		{
			for ( i = 0; i < nFree ; i++ )
			{
				if ( !pSortArray[i] )
				{
					break;
				}

				m_FreeList.Push( pSortArray[i] );
			}
		}

		free( pSortArray );
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

	m_pBase = (byte *)VirtualAlloc( NULL, NUM_POOLS * MAX_POOL_REGION, VA_RESERVE_FLAGS, PAGE_NOACCESS );
	m_pLimit = m_pBase + NUM_POOLS * MAX_POOL_REGION;

	// Build a lookup table used to find the correct pool based on size
	constexpr int MAX_TABLE = MAX_SBH_BLOCK >> 2;
	int i = 0;
	int nBytesElement = 0;
	byte *pCurBase = m_pBase;
	CSmallBlockPool *pCurPool = NULL;
	int iCurPool = 0;

#if _M_X64
	// Blocks sized 0 - 256 are in pools in increments of 16
	for ( ; i < 64 && i < MAX_TABLE; i++ )
	{
		if ( (i + 1) % 4 == 1)
		{
			nBytesElement += 16;
			pCurPool = &m_Pools[iCurPool];
			pCurPool->Init( nBytesElement, pCurBase, GetInitialCommitForPool(iCurPool) );
			iCurPool++;
			m_PoolLookup[i] = pCurPool;
			pCurBase += MAX_POOL_REGION;
		}
		else
		{
			m_PoolLookup[i] = pCurPool;
		}
	}
#else
	// Blocks sized 0 - 128 are in pools in increments of 8
	for ( ; i < 32; i++ )
	{
		if ( (i + 1) % 2 == 1)
		{
			nBytesElement += 8;
			pCurPool = &m_Pools[iCurPool];
			pCurPool->Init( nBytesElement, pCurBase, GetInitialCommitForPool(iCurPool) );
			iCurPool++;
			m_PoolLookup[i] = pCurPool;
			pCurBase += MAX_POOL_REGION;
		}
		else
		{
			m_PoolLookup[i] = pCurPool;
		}
	}

	// Blocks sized 129 - 256 are in pools in increments of 16
	for ( ; i < 64; i++ )
	{
		if ( (i + 1) % 4 == 1)
		{
			nBytesElement += 16;
			pCurPool = &m_Pools[iCurPool];
			pCurPool->Init( nBytesElement, pCurBase, GetInitialCommitForPool(iCurPool) );
			iCurPool++;
			m_PoolLookup[i] = pCurPool;
			pCurBase += MAX_POOL_REGION;
		}
		else
		{
			m_PoolLookup[i] = pCurPool;
		}
	}
#endif

	// Blocks sized 257 - 512 are in pools in increments of 32
	for ( ; i < 128; i++ )
	{
		if ( (i + 1) % 8 == 1)
		{
			nBytesElement += 32;
			pCurPool = &m_Pools[iCurPool];
			pCurPool->Init( nBytesElement, pCurBase, GetInitialCommitForPool(iCurPool) );
			iCurPool++;
			m_PoolLookup[i] = pCurPool;
			pCurBase += MAX_POOL_REGION;
		}
		else
		{
			m_PoolLookup[i] = pCurPool;
		}
	}

	// Blocks sized 513 - 768 are in pools in increments of 64
	for ( ; i < 192; i++ )
	{
		if ( (i + 1) % 16 == 1)
		{
			nBytesElement += 64;
			pCurPool = &m_Pools[iCurPool];
			pCurPool->Init( nBytesElement, pCurBase, GetInitialCommitForPool(iCurPool) );
			iCurPool++;
			m_PoolLookup[i] = pCurPool;
			pCurBase += MAX_POOL_REGION;
		}
		else
		{
			m_PoolLookup[i] = pCurPool;
		}
	}

	// Blocks sized 769 - 1024 are in pools in increments of 128
	for ( ; i < 256; i++ )
	{
		if ( (i + 1) % 32 == 1)
		{
			nBytesElement += 128;
			pCurPool = &m_Pools[iCurPool];
			pCurPool->Init( nBytesElement, pCurBase, GetInitialCommitForPool(iCurPool) );
			iCurPool++;
			m_PoolLookup[i] = pCurPool;
			pCurBase += MAX_POOL_REGION;
		}
		else
		{
			m_PoolLookup[i] = pCurPool;
		}
	}

	// Blocks sized 1025 - 2048 are in pools in increments of 256
	for ( ; i < MAX_TABLE; i++ )
	{
		if ( (i + 1) % 64 == 1)
		{
			nBytesElement += 256;
			pCurPool = &m_Pools[iCurPool];
			pCurPool->Init( nBytesElement, pCurBase, GetInitialCommitForPool(iCurPool) );
			iCurPool++;
			m_PoolLookup[i] = pCurPool;
			pCurBase += MAX_POOL_REGION;
		}
		else
		{
			m_PoolLookup[i] = pCurPool;
		}
	}

	Assert( iCurPool == NUM_POOLS );
}

bool CSmallBlockHeap::ShouldUse( size_t nBytes ) const
{
	return ( UsingSBH() && nBytes <= MAX_SBH_BLOCK );
}

bool CSmallBlockHeap::IsOwner( void * p ) const
{
	return ( UsingSBH() && p >= m_pBase && p < m_pLimit );
}

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
		if ( p )
		{
			return p;
		}
	}

	void *pRet = malloc( nBytes );
	if ( !pRet )
	{
		s_StdMemAlloc.SetCRTAllocFailed( nBytes );
	}
	return pRet;
}

void *CSmallBlockHeap::Realloc( void *p, size_t nBytes )
{
	if ( nBytes == 0)
	{
		nBytes = 1;
	}

	CSmallBlockPool *pOldPool = FindPool( p );
	CSmallBlockPool *pNewPool = ( ShouldUse( nBytes ) ) ? FindPool( nBytes ) : NULL;

	if ( pOldPool == pNewPool )
	{
		return p;
	}

	void *pNewBlock = NULL;

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
		}
	}

	if ( pNewBlock )
	{
		size_t nBytesCopy = min( nBytes, pOldPool->GetBlockSize() );
		memcpy( pNewBlock, p, nBytesCopy );
	} 

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
	bool bSpew = true;

	if ( pFile )
	{
		for ( int i = 0; i < NUM_POOLS; i++ )
		{
			// output for vxconsole parsing
			fprintf( pFile, "Pool %i: Size: %zu Allocated: %zu Free: %i Committed: %zu CommittedSize: %zu\n", 
				i, 
				m_Pools[i].GetBlockSize(), 
				m_Pools[i].CountAllocatedBlocks(), 
				m_Pools[i].CountFreeBlocks(),
				m_Pools[i].CountCommittedBlocks(), 
				m_Pools[i].GetCommittedSize() );
		}
		bSpew = false;
	}

	if ( bSpew )
	{
		uintp bytesCommitted = 0, bytesAllocated = 0;

		for ( int i = 0; i < NUM_POOLS; i++ )
		{
			Msg( "Pool %i: (size: %zu) blocks: allocated:%zu free:%i committed:%zu (committed size:%zu KiB)\n",
				i,
				m_Pools[i].GetBlockSize(),
				m_Pools[i].CountAllocatedBlocks(),
				m_Pools[i].CountFreeBlocks(),
				m_Pools[i].CountCommittedBlocks(),
				m_Pools[i].GetCommittedSize() / 1024);

			bytesCommitted += m_Pools[i].GetCommittedSize();
			bytesAllocated += ( m_Pools[i].CountAllocatedBlocks() * m_Pools[i].GetBlockSize() );
		}

		Msg( "Totals: Committed:%zu KiB Allocated:%zu KiB\n", bytesCommitted / 1024, bytesAllocated / 1024 );
	}
}

intp CSmallBlockHeap::Compact()
{
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
	size_t i = ((byte *)p - m_pBase) / MAX_POOL_REGION;
	return &m_Pools[i];
}

const CSmallBlockPool *CSmallBlockHeap::FindPool( void *p ) const
{
	size_t i = ((byte *)p - m_pBase) / MAX_POOL_REGION;
	return &m_Pools[i];
}

#endif

//-----------------------------------------------------------------------------
// Release versions
//-----------------------------------------------------------------------------

void *CStdMemAlloc::Alloc( size_t nSize )
{
	PROFILE_ALLOC(Malloc);
	
	void *pMem;

#ifdef _WIN32
	if ( m_SmallBlockHeap.ShouldUse( nSize ) )
	{
		pMem = m_SmallBlockHeap.Alloc( nSize );
		ApplyMemoryInitializations( pMem, nSize );

		// dimhotepus: Fallback to common allocator if pool has no empty blocks.
		if ( pMem )
			return pMem;
	}
#endif

	pMem = malloc( nSize );
	ApplyMemoryInitializations( pMem, nSize );
	if ( !pMem )
	{
		// dimhotepus: Try to free some space and allocate again.
		CompactHeap();

		pMem = malloc( nSize );
		ApplyMemoryInitializations( pMem, nSize );
		if ( !pMem )
		{
			SetCRTAllocFailed( nSize );
		}
	}

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

void *CStdMemAlloc::Expand_NoLongerSupported( void *pMem, size_t nSize )
{
	return NULL;
}


//-----------------------------------------------------------------------------
// Debug versions
//-----------------------------------------------------------------------------
void *CStdMemAlloc::Alloc( size_t nSize, const char *pFileName, int nLine )
{
	return CStdMemAlloc::Alloc( nSize );
}

void *CStdMemAlloc::Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine )
{
	return CStdMemAlloc::Realloc( pMem, nSize );
	}

void  CStdMemAlloc::Free( void *pMem, const char *pFileName, int nLine )
{
	CStdMemAlloc::Free( pMem );
}

void *CStdMemAlloc::Expand_NoLongerSupported( void *pMem, size_t nSize, const char *pFileName, int nLine )
{
	return NULL;
}

#if defined (LINUX)
#include <malloc.h>
#elif defined (OSX)
#define malloc_usable_size( ptr ) malloc_size( ptr )
extern "C" {
	extern size_t malloc_size( const void *ptr );
}
#endif

//-----------------------------------------------------------------------------
// Returns size of a particular allocation
//-----------------------------------------------------------------------------
size_t CStdMemAlloc::GetSize( void *pMem )
{
#ifdef MEM_SBH_ENABLED
	if ( !pMem )
		return CalcHeapUsed();
	else
	{
		if ( m_SmallBlockHeap.IsOwner( pMem ) )
		{
			return m_SmallBlockHeap.GetSize( pMem );
		}
		return _msize( pMem );
	}
#else
	return malloc_usable_size( pMem );
#endif
}


//-----------------------------------------------------------------------------
// Force file + line information for an allocation
//-----------------------------------------------------------------------------
void CStdMemAlloc::PushAllocDbgInfo( const char *pFileName, int nLine )
{
}

void CStdMemAlloc::PopAllocDbgInfo()
{
}

//-----------------------------------------------------------------------------
// FIXME: Remove when we make our own heap! Crt stuff we're currently using
//-----------------------------------------------------------------------------
long CStdMemAlloc::CrtSetBreakAlloc( long lNewBreakAlloc )
{
	return 0;
}

int CStdMemAlloc::CrtSetReportMode( int nReportType, int nReportMode )
{
	return 0;
}

int CStdMemAlloc::CrtIsValidHeapPointer( const void *pMem )
{
	return 1;
}

int CStdMemAlloc::CrtIsValidPointer( const void *pMem, unsigned int size, int access )
{
	return 1;
}

int CStdMemAlloc::CrtCheckMemory( void )
{
	return 1;
}

int CStdMemAlloc::CrtSetDbgFlag( int nNewFlag )
{
	return 0;
}

void CStdMemAlloc::CrtMemCheckpoint( _CrtMemState *pState )
{
}

// FIXME: Remove when we have our own allocator
void* CStdMemAlloc::CrtSetReportFile( int nRptType, void* hFile )
{
	return 0;
}

void* CStdMemAlloc::CrtSetReportHook( void* pfnNewHook )
{
	return 0;
}

int CStdMemAlloc::CrtDbgReport( int nRptType, const char * szFile,
	int nLine, const char * szModule, const char * pMsg )
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
#ifdef _WIN32
	char filename[ 512 ];
	_snprintf( filename, std::size( filename ) - 1, "%s.txt", pchFileBase );
	filename[ std::size( filename ) - 1 ] = '\0';
	FILE *pFile = fopen( filename, "wt" );
	if (pFile)
	{
		fprintf( pFile, "\nSBH:\n" );
		m_SmallBlockHeap.DumpStats(pFile);	// Dump statistics to small block heap
		fclose( pFile );
	}
	else
	{
		Warning( "Unable to open '%s' to dump small block heap stats.", filename );
	}
#endif
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
#if !defined( NO_SBH ) && defined( _WIN32 )
	intp nBytesRecovered = m_SmallBlockHeap.Compact();
	Msg( "Compact heap freed %zd  bytes from small block heap.\n", nBytesRecovered );
	
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
#endif
}

MemAllocFailHandler_t CStdMemAlloc::SetAllocFailHandler( MemAllocFailHandler_t pfnMemAllocFailHandler )
{
	return std::exchange(m_pfnFailHandler, pfnMemAllocFailHandler);
}

size_t CStdMemAlloc::DefaultFailHandler( size_t nBytes )
{
	return 0;
}

#if defined( _MEMTEST )
void CStdMemAlloc::void SetStatsExtraInfo( const char *pMapName, const char *pComment )
{
}
#endif

void CStdMemAlloc::SetCRTAllocFailed( size_t nSize )
{
	m_sMemoryAllocFailed = nSize;

	MemAllocOOMError( nSize );
}

size_t CStdMemAlloc::MemoryAllocFailed()
{
	return m_sMemoryAllocFailed;
}

#endif

void ReserveBottomMemory()
{
	// If we are running a 64-bit build then reserve all addresses below the
	// 4 GiB line to push as many pointers as possible above the line.
#ifdef PLATFORM_WINDOWS_PC64
	// Avoid the cost of calling this multiple times.
	static bool s_initialized = false;
	if ( s_initialized )
		return;

	s_initialized = true;

	// If AppVerifier is enabled then memory reservations get turned into committed
	// memory in the working set. This means that ReserveBottomMemory() can end
	// up adding almost 4 GiB to the working set, which is a significant problem if
	// you run many processes in parallel. Therefore, if vfbasics.dll (part of AppVerifier)
	// is loaded, don't do the reservation.
	HMODULE vfBasicsDLL = GetModuleHandle( "vfbasics.dll" );
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
	const size_t LOW_MEM_LINE = 0x100000000LL;
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

			if ( (size_t)p >= LOW_MEM_LINE )
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

			if ( (size_t)p >= LOW_MEM_LINE )
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
	//char buffer[1000];
	//sprintf_s( buffer, "Reserved %1.3f MiB (%d vallocs, %d heap allocs) to keep allocations out of low-memory.\n",
	//			totalReservation / (1024 * 1024.0), (int)numVAllocs, (int)numHeapAllocs );
	// Can't use Msg here because it isn't necessarily initialized yet.
	//OutputDebugString( buffer );
#endif
}

#endif // STEAM
