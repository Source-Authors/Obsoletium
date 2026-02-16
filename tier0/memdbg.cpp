// Copyright Valve Corporation, All rights reserved.
//
// Memory allocation!

#include "stdafx.h"

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include "mem_helpers.h"

#if defined(_WIN32)
#include "winlite.h"
#include <crtdbg.h>
#elif defined(OSX)
#include <malloc/malloc.h>
#include <mach/mach.h>
#include <stdlib.h>
#endif

#include <atomic>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <climits>
#include "tier0/threadtools.h"

#if ( !defined(_DEBUG) && defined(USE_MEM_DEBUG) )
#pragma message ("USE_MEM_DEBUG is enabled in a release build. Don't commit this!")
#endif
#if (defined(_DEBUG) || defined(USE_MEM_DEBUG))

#if defined(_WIN32) && ( !defined(_X360) && !defined(_WIN64) )
// #define USE_STACK_WALK
// or:
// #define USE_STACK_WALK_DETAILED
#endif

//-----------------------------------------------------------------------------

#define DebugAlloc	malloc
#define DebugFree	free

#ifdef WIN32
static const int g_DefaultHeapFlags = _CrtSetDbgFlag( _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_ALLOC_MEM_DF );
#endif

#if defined( _MEMTEST )
static char s_szStatsMapName[32];
static char s_szStatsComment[256];
#endif

//-----------------------------------------------------------------------------

#if defined( USE_STACK_WALK ) || defined( USE_STACK_WALK_DETAILED )
#include <dbghelp.h>

#pragma comment(lib, "Dbghelp.lib" )

#pragma auto_inline(off)
__declspec(naked) DWORD GetEIP()
{
	__asm 
	{
		mov eax, [ebp + 4]
		ret
	}
}

int WalkStack( void **ppAddresses, int nMaxAddresses, int nSkip = 0 )
{
	HANDLE hProcess = GetCurrentProcess();
	HANDLE hThread = GetCurrentThread();

	STACKFRAME64 frame;

	BitwiseClear(frame);
	DWORD valEsp, valEbp;
	__asm
	{
		mov [valEsp], esp;
		mov [valEbp], ebp
	}
	frame.AddrPC.Offset    = GetEIP();
	frame.AddrStack.Offset = valEsp;
	frame.AddrFrame.Offset = valEbp;
	frame.AddrPC.Mode      = AddrModeFlat;
	frame.AddrStack.Mode   = AddrModeFlat;
	frame.AddrFrame.Mode   = AddrModeFlat;

	// Walk the stack.
	int nWalked = 0;
	nSkip++;
	while ( nMaxAddresses - nWalked > 0 ) 
	{
		if ( !StackWalk64(IMAGE_FILE_MACHINE_I386, hProcess, hThread, &frame, NULL, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL ) ) 
		{
			break;
		}

		if ( nSkip == 0 )
		{
			if (frame.AddrFrame.Offset == 0) 
			{
				// End of stack.
				break;
			}

			*ppAddresses++ = (void *)frame.AddrPC.Offset;
			nWalked++;
			
			if (frame.AddrPC.Offset == frame.AddrReturn.Offset)
			{
				// Catching a stack loop
				break;
			}
		}
		else
		{
			nSkip--;
		}
	}

	if ( nMaxAddresses )
	{
		memset( ppAddresses, 0, ( nMaxAddresses - nWalked ) * sizeof(*ppAddresses) );
	}

	return nWalked;
}

template<size_t size>
bool GetModuleFromAddress( void *address, char (&pResult)[size] )
{
	IMAGEHLP_MODULE   moduleInfo;

	moduleInfo.SizeOfStruct = sizeof(moduleInfo);

	if ( SymGetModuleInfo( GetCurrentProcess(), (DWORD)address, &moduleInfo ) )
	{
		strncpy( pResult, moduleInfo.ModuleName, std::size(pResult) - 1 );
		pResult[ std::size(pResult) - 1 ] = '\0';
		return true;
	}

	return false;
}

template<size_t size>
bool GetCallerModule( char (&pDest)[size] )
{
	static bool bInit;
	if ( !bInit )
	{
		PSTR psUserSearchPath = NULL;
		// dimhotepus: x86-64 port.
		psUserSearchPath = "u:\\data\\game\\" PLATFORM_BIN_DIR "\\;u:\\data\\game\\episodic\\" PLATFORM_BIN_DIR "\\;u:\\data\\game\\hl2\\" PLATFORM_BIN_DIR "\\;\\\\perforce\\symbols";
		SymInitialize( GetCurrentProcess(), psUserSearchPath, true );
		bInit = true;
	}
	void *pCaller;
	WalkStack( &pCaller, 1, 2 );

	return ( pCaller != 0 && GetModuleFromAddress( pCaller, pDest ) );
}


#if defined( USE_STACK_WALK_DETAILED )

//
// Note: StackDescribe function is non-reentrant:
//		Reason:   Stack description is stored in a static buffer.
//		Solution: Passing caller-allocated buffers would allow the
//		function to become reentrant, however the current only client (FindOrCreateFilename)
//		is synchronized with a heap mutex, after retrieving stack description the
//		heap memory will be allocated to copy the text.
//

char * StackDescribe( void **ppAddresses, int nMaxAddresses )
{
	static char s_chStackDescription[ 32 * 1024 ];
	static char s_chSymbolBuffer[ sizeof( IMAGEHLP_SYMBOL64 ) + 1024 ];
	
	IMAGEHLP_SYMBOL64 &hlpSymbol = * ( IMAGEHLP_SYMBOL64 * ) s_chSymbolBuffer;
	hlpSymbol.SizeOfStruct = sizeof( IMAGEHLP_SYMBOL64 );
	hlpSymbol.MaxNameLength = 1024;
	DWORD64 hlpSymbolOffset = 0;

	IMAGEHLP_LINE64 hlpLine;
	hlpLine.SizeOfStruct = sizeof( IMAGEHLP_LINE64 );
	DWORD hlpLineOffset = 0;

	s_chStackDescription[ 0 ] = 0;
	char *pchBuffer = s_chStackDescription;

	for ( int k = 0; k < nMaxAddresses; ++ k )
	{
		if ( !ppAddresses[k] )
			break;

		pchBuffer += strlen( pchBuffer );
		if ( SymGetLineFromAddr64( GetCurrentProcess(), ( DWORD64 ) ppAddresses[k], &hlpLineOffset, &hlpLine ) )
		{
			char const *pchFileName = hlpLine.FileName ? hlpLine.FileName + strlen( hlpLine.FileName ) : NULL;
			for ( size_t numSlashesAllowed = 2; pchFileName > hlpLine.FileName; -- pchFileName )
			{
				if ( *pchFileName == '\\' )
				{
					if ( numSlashesAllowed -- )
						continue;
					else
						break;
				}
			}
			sprintf( pchBuffer, hlpLineOffset ? "%s:%d+0x%I32X" : "%s:%d", pchFileName, hlpLine.LineNumber, hlpLineOffset );
		}
		else if ( SymGetSymFromAddr64( GetCurrentProcess(), ( DWORD64 ) ppAddresses[k], &hlpSymbolOffset, &hlpSymbol ) )
		{
			sprintf( pchBuffer, ( hlpSymbolOffset > 0 && !( hlpSymbolOffset >> 63 ) ) ? "%s+0x%I64X" : "%s", hlpSymbol.Name, hlpSymbolOffset );
		}
		else
		{
			sprintf( pchBuffer, "#0x%08p", ppAddresses[k] );
		}

		pchBuffer += strlen( pchBuffer );
		sprintf( pchBuffer, "<--" );
	}
	*pchBuffer  = 0;

	return s_chStackDescription;
}

#endif // #if defined( USE_STACK_WALK_DETAILED )

#else

inline int WalkStack( void **ppAddresses, int nMaxAddresses, [[maybe_unused]] int nSkip = 0 )
{
	memset( ppAddresses, 0, nMaxAddresses * sizeof(*ppAddresses) );
	return 0;
}
#define GetModuleFromAddress( address, pResult ) ( ( *pResult = 0 ), 0)
#define GetCallerModule( pDest ) false
#endif


//-----------------------------------------------------------------------------

// The size of the no-man's land used in unaligned and aligned allocations:
static constexpr inline size_t no_mans_land_size = 4;

// NOTE: This exactly mirrors the dbg header in the MSDEV crt
// eventually when we write our own allocator, we can kill this
// See struct _CrtMemBlockHeader at debug_heap.cpp
struct CrtDbgMemHeader_t
{
    CrtDbgMemHeader_t* m_pBlockHeaderNext;
    CrtDbgMemHeader_t* m_pBlockHeaderPrev;
	const char *m_pFileName;
	int			m_nLineNumber;

    int			m_BlockUse;
    size_t		m_DataSize;

    long		m_RequestNumber;
    unsigned char m_Gap[no_mans_land_size];

    // Followed by:
    // unsigned char    m_data[_data_size];
    // unsigned char    m_AnotherGap[no_mans_land_size];
};

struct DbgMemHeader_t
#if !defined( _DEBUG ) || defined( POSIX )
	: CrtDbgMemHeader_t
#endif
{
	size_t nLogicalSize;
	byte reserved[16 - sizeof(size_t)];	// MS allocator always returns mem aligned on 16 bytes, which some of our code depends on
};

//-----------------------------------------------------------------------------

#if defined( _DEBUG ) && !defined( POSIX )
#define GetCrtDbgMemHeader( pMem ) ((CrtDbgMemHeader_t*)((DbgMemHeader_t*)pMem - 1) - 1)
#elif defined( OSX )
DbgMemHeader_t *GetCrtDbgMemHeader( void *pMem );
#else
#define GetCrtDbgMemHeader( pMem ) ((DbgMemHeader_t*)pMem - 1)
#endif

#ifdef OSX
DbgMemHeader_t *GetCrtDbgMemHeader( void *pMem )
{
	size_t msize = malloc_size( pMem );
	return (DbgMemHeader_t *)( (char *)pMem + msize - sizeof(DbgMemHeader_t) );
}
#endif

static inline void *InternalMalloc( size_t nSize, const char *pFileName, int nLine )
{
#ifdef OSX
	void *pAllocedMem = malloc_zone_malloc( malloc_default_zone(), nSize + sizeof(DbgMemHeader_t) );
	if (!pAllocedMem)
	{
		return NULL;
	}
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( pAllocedMem );

	pInternalMem->m_pFileName = pFileName;
	pInternalMem->m_nLineNumber = nLine;
	pInternalMem->nLogicalSize = nSize;
	*((int*)pInternalMem->m_Reserved) = 0xf00df00d;

	return pAllocedMem;
#else // LINUX || WIN32
	DbgMemHeader_t *pInternalMem;
#if defined( POSIX ) || !defined( _DEBUG )
	pInternalMem = (DbgMemHeader_t *)malloc( nSize + sizeof(DbgMemHeader_t) );
	if (!pInternalMem)
	{
		return NULL;
	}
	pInternalMem->m_pFileName = pFileName;
	pInternalMem->m_nLineNumber = nLine;
	*((int*)pInternalMem->m_Reserved) = 0xf00df00d;
#else
	pInternalMem = (DbgMemHeader_t *)_malloc_dbg( nSize + sizeof(DbgMemHeader_t), _NORMAL_BLOCK, pFileName, nLine );
#endif // defined( POSIX ) || !defined( _DEBUG )

	if ( pInternalMem )
	{
		pInternalMem->nLogicalSize = nSize;
		return pInternalMem + 1;
	}

	return nullptr;
#endif // LINUX || WIN32
}

static inline void *InternalRealloc( void *pMem, size_t nNewSize, const char *pFileName, int nLine )
{
	if ( !pMem )
		return InternalMalloc( nNewSize, pFileName, nLine );

#ifdef OSX
	void *pNewAllocedMem = NULL;

	pNewAllocedMem = (void *)malloc_zone_realloc( malloc_default_zone(), pMem, nNewSize + sizeof(DbgMemHeader_t) );
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( pNewAllocedMem );

	pInternalMem->m_pFileName = pFileName;
	pInternalMem->m_nLineNumber = nLine;
	pInternalMem->nLogicalSize = static_cast<unsigned int>( nNewSize );
	*((int*)pInternalMem->m_Reserved) = 0xf00df00d;

	return pNewAllocedMem;
#else // LINUX || WIN32
	DbgMemHeader_t *pInternalMem = (DbgMemHeader_t *)pMem - 1;
#if defined( POSIX ) || !defined( _DEBUG )
	pInternalMem = (DbgMemHeader_t *)realloc( pInternalMem, nNewSize + sizeof(DbgMemHeader_t) );
	pInternalMem->m_pFileName = pFileName;
	pInternalMem->m_nLineNumber = nLine;
#else
	pInternalMem = (DbgMemHeader_t *)_realloc_dbg( pInternalMem, nNewSize + sizeof(DbgMemHeader_t), _NORMAL_BLOCK, pFileName, nLine );
#endif

	if ( pInternalMem )
	{
		pInternalMem->nLogicalSize = nNewSize;
		return pInternalMem + 1;
	}

	return nullptr;
#endif // LINUX || WIN32
}

static inline void InternalFree( void *pMem )
{
	if ( !pMem )
		return;

	DbgMemHeader_t *pInternalMem = (DbgMemHeader_t *)pMem - 1;
#if !defined( _DEBUG ) || defined( POSIX )
#ifdef OSX
	malloc_zone_free( malloc_default_zone(), pMem );
#else
	free( pInternalMem );
#endif
#else
	_free_dbg( pInternalMem, _NORMAL_BLOCK );
#endif
}

static inline size_t InternalMSize( void *pMem )
{
	//$ TODO. For Linux, we could use 'int size = malloc_usable_size( pMem )'...
#if defined(POSIX)
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( pMem );
	return pInternalMem->nLogicalSize;
#elif !defined(_DEBUG)
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( pMem );
	return _msize( pInternalMem ) - sizeof(DbgMemHeader_t);
#else
	DbgMemHeader_t *pInternalMem = (DbgMemHeader_t *)pMem - 1;
	return _msize_dbg( pInternalMem, _NORMAL_BLOCK ) - sizeof(DbgMemHeader_t);
#endif	
}

static inline size_t InternalLogicalSize( void *pMem )
{
#if defined(POSIX)
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( pMem );
#elif !defined(_DEBUG)
	DbgMemHeader_t *pInternalMem = (DbgMemHeader_t *)pMem - 1;
#else
	DbgMemHeader_t *pInternalMem = (DbgMemHeader_t *)pMem - 1;
#endif
	return pInternalMem->nLogicalSize;
}

#ifndef _DEBUG
#define _CrtDbgReport( nRptType, szFile, nLine, szModule, pMsg ) 0
#endif


//-----------------------------------------------------------------------------


// Custom allocator protects this module from recursing on operator new
template <class T>
class CNoRecurseAllocator
{
public:
	// type definitions
	using value_type = T;
	using pointer = T *;
	using const_pointer = const T *;
	using reference = T &;
	using const_reference = const T &;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	CNoRecurseAllocator() = default;
	CNoRecurseAllocator(const CNoRecurseAllocator&) {}
	template <class U> CNoRecurseAllocator(const CNoRecurseAllocator<U>&) {}
	~CNoRecurseAllocator() = default;

	// rebind allocator to type U
	template <class U > struct rebind { using other = CNoRecurseAllocator<U>; };

	// return address of values
	pointer address (reference value) const { return &value; }

	[[nodiscard]] const_pointer address (const_reference value) const { return &value;}
	[[nodiscard]] size_type max_size() const { return INT_MAX; }

	pointer allocate(size_type num, const void* = nullptr)  { return (pointer)DebugAlloc(num * sizeof(T)); }
	void deallocate (pointer p, size_type num) { DebugFree(p); }
	void construct(pointer p, const T& value) {	new((void*)p)T(value); }
	void destroy (pointer p) { p->~T(); }
};

template <class T1, class T2>
bool operator==(const CNoRecurseAllocator<T1>&, const CNoRecurseAllocator<T2>&)
{
	return true;
}

template <class T1, class T2>
bool operator!=(const CNoRecurseAllocator<T1>&, const CNoRecurseAllocator<T2>&)
{
	return false;
}

//-----------------------------------------------------------------------------

#pragma warning( disable:4074 ) // warning C4074: initializers put in compiler reserved initialization area
#pragma init_seg( compiler )

//-----------------------------------------------------------------------------
// NOTE! This should never be called directly from leaf code
// Just use new,delete,malloc,free etc. They will call into this eventually
//-----------------------------------------------------------------------------
class CDbgMemAlloc final : public IMemAlloc
{
public:
	CDbgMemAlloc();
	~CDbgMemAlloc();

	// Release versions
	void *Alloc( size_t nSize ) override;
	void *Realloc( void *pMem, size_t nSize ) override;
	void  Free( void *pMem ) override;
	void *Expand_NoLongerSupported( void *pMem, size_t nSize ) override;

	// Debug versions
	void *Alloc( size_t nSize, const char *pFileName, int nLine ) override;
	void *Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine ) override;
	void  Free( void *pMem, const char *pFileName, int nLine ) override;
	void *Expand_NoLongerSupported( void *pMem, size_t nSize, const char *pFileName, int nLine ) override;

	// Returns size of a particular allocation
	size_t GetSize( void *pMem ) override;

	// Force file + line information for an allocation
	void PushAllocDbgInfo( const char *pFileName, int nLine ) override;
	void PopAllocDbgInfo() override;

	long CrtSetBreakAlloc( long lNewBreakAlloc ) override;
	int CrtSetReportMode( int nReportType, int nReportMode ) override;
	int CrtIsValidHeapPointer( const void *pMem ) override;
	int CrtIsValidPointer( const void *pMem, unsigned int size, int access ) override;
	int CrtCheckMemory( ) override;
	int CrtSetDbgFlag( int nNewFlag ) override;
	void CrtMemCheckpoint( _CrtMemState *pState ) override;

	// handles storing allocation info for coroutines
	uint32 GetDebugInfoSize() override;
	void SaveDebugInfo( void *pvDebugInfo ) override;
	void RestoreDebugInfo( const void *pvDebugInfo ) override;
	void InitDebugInfo( void *pvDebugInfo, const char *pchRootFileName, int nLine ) override;

	// FIXME: Remove when we have our own allocator
	void* CrtSetReportFile( int nRptType, void* hFile ) override;
	void* CrtSetReportHook( void* pfnNewHook ) override;
	int CrtDbgReport( int nRptType, const char * szFile,
			int nLine, const char * szModule, const char * szFormat ) override;

	int heapchk() override;

	bool IsDebugHeap() override { return true; }

	int GetVersion() override { return MEMALLOC_VERSION; }

	void CompactHeap() override
	{
#if defined( _DEBUG )
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

		const size_t largestFreeBytes = ::HeapCompact( ::GetProcessHeap(), 0 );

		Msg( "Compacted heap. Largest free block is %zu bytes, optimize heap %s.\n",
			largestFreeBytes, ok ? "OK" : "FAIL" );
#endif
	}

	MemAllocFailHandler_t SetAllocFailHandler( MemAllocFailHandler_t ) override { return nullptr; } // debug heap doesn't attempt retries

#if defined( _MEMTEST )
	void SetStatsExtraInfo( const char *pMapName, const char *pComment )
	{
		strncpy( s_szStatsMapName, pMapName, std::size( s_szStatsMapName ) );
		s_szStatsMapName[std::size( s_szStatsMapName ) - 1] = '\0';

		strncpy( s_szStatsComment, pComment, std::size( s_szStatsComment ) );
		s_szStatsComment[std::size( s_szStatsComment ) - 1] = '\0';
	}
#endif

	size_t MemoryAllocFailed() override;
	void SetCRTAllocFailed( size_t nMemSize );

	enum
	{
		BYTE_COUNT_16 = 0,
		BYTE_COUNT_32,
		BYTE_COUNT_128,
		BYTE_COUNT_1024,
		BYTE_COUNT_GREATER,

		NUM_BYTE_COUNT_BUCKETS
	};

	void Shutdown();

private:
	struct MemInfo_t
	{
		MemInfo_t()
		{
			memset( this, 0, sizeof(*this) );
		}

		// Size in bytes
		size_t m_nCurrentSize;
		size_t m_nPeakSize;
		size_t m_nTotalSize;
		size_t m_nOverheadSize;
		size_t m_nPeakOverheadSize;

		// Count in terms of # of allocations
		size_t m_nCurrentCount;
		size_t m_nPeakCount;
		size_t m_nTotalCount;

		// Count in terms of # of allocations of a particular size
		size_t m_pCount[NUM_BYTE_COUNT_BUCKETS];

		// Time spent allocating + deallocating	(microseconds)
		int64 m_nTime;
	};

	struct MemInfoKey_t
	{
		MemInfoKey_t( const char *pFileName, int line ) : m_pFileName(pFileName), m_nLine(line) {}

		bool operator==( const MemInfoKey_t &key ) const
		{
			// dimhotepus: File names are from __FILE__ or from module name, so unique pointers.
			return m_nLine == key.m_nLine && m_pFileName == key.m_pFileName;
		}

		const char *m_pFileName;
		int			m_nLine;
	};

	struct MemInfoKeyHasher 
	{
		std::size_t operator()( const MemInfoKey_t &key ) const
		{
			std::size_t h1 = std::hash<const char*>{}( key.m_pFileName );
			return h1 ^ (static_cast<size_t>(key.m_nLine) << 1);
		}
	};

	struct MemInfoKeySorter
	{
		bool operator()( const std::pair<MemInfoKey_t, MemInfo_t> &lhs,
			const std::pair<MemInfoKey_t, MemInfo_t> &rhs )
		{
			// dimhotepus: stricmp -> strcmp. Alphabetical order is enough, and stricmp is too slow.
			int iret = strcmp( lhs.first.m_pFileName, rhs.first.m_pFileName );
			if ( iret < 0 )
				return true;

			if ( iret > 0 )
				return false;

			return lhs.first.m_nLine < rhs.first.m_nLine;
		}
	};

	// NOTE: Deliberately using STL here because the UTL stuff
	// is a client of this library; want to avoid circular dependency

	// Maps file name to info
	// dimhotepus: Speed up debug allocation tracking.
	using StatMap_t = std::unordered_map<MemInfoKey_t, MemInfo_t, MemInfoKeyHasher, std::equal_to<>, CNoRecurseAllocator<std::pair<const MemInfoKey_t, MemInfo_t>>>;
	using Filenames_t = std::unordered_set<const char *, std::hash<const char *>, std::equal_to<const char *>, CNoRecurseAllocator<const char *>>;

	// Heap reporting method
	using HeapReportFunc_t = void (*)(const char *, ...);

private:
	// Returns the actual debug info
	void GetActualDbgInfo( const char *&pFileName, int &nLine ) override;

	void Initialize();

	// Finds the file in our map
	MemInfo_t &FindOrCreateEntry( const char *pFileName, int line );
	const char *FindOrCreateFilename( const char *pFileName );

	// Updates stats
	void RegisterAllocation( const char *pFileName, int nLine, size_t nLogicalSize, size_t nActualSize, unsigned nTime ) override;
	void RegisterDeallocation( const char *pFileName, int nLine, size_t nLogicalSize, size_t nActualSize, unsigned nTime ) override;

	void RegisterAllocation( MemInfo_t &info, size_t nLogicalSize, size_t nActualSize, unsigned nTime );
	void RegisterDeallocation( MemInfo_t &info, size_t nLogicalSize, size_t nActualSize, unsigned nTime );

	// Gets the allocation file name
	const char *GetAllocatonFileName( void *pMem );
	int GetAllocatonLineNumber( void *pMem );

	// FIXME: specify a spew output func for dumping stats
	// Stat output
	void DumpMemInfo( const char *pAllocationName, int line, const MemInfo_t &info ) const;
	void DumpFileStats() const;
	void DumpStats() override;
	void DumpStatsFileBase( char const *pchFileBase ) override;
	void DumpBlockStats( void *p ) override;
	void GlobalMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory ) override;

private:
	StatMap_t *m_pStatMap{nullptr};
	MemInfo_t m_GlobalInfo;
	CFastTimer m_Timer;
	bool		m_bInitialized;
	Filenames_t *m_pFilenames{nullptr};

	HeapReportFunc_t m_OutputFunc;

	static size_t s_pCountSizes[NUM_BYTE_COUNT_BUCKETS];
	static const char *s_pCountHeader[NUM_BYTE_COUNT_BUCKETS];

	size_t				m_sMemoryAllocFailed{0};
};

static constexpr inline char g_pszUnknown[]{"unknown"};

//-----------------------------------------------------------------------------

static constexpr inline int DBG_INFO_STACK_DEPTH = 32;

struct DbgInfoStack_t
{
	const char *m_pFileName;
	int m_nLine;
};

thread_local DbgInfoStack_t* g_DbgInfoStack CONSTRUCT_EARLY;
thread_local int				g_nDbgInfoStackDepth CONSTRUCT_EARLY;

//-----------------------------------------------------------------------------
// Singleton...
//-----------------------------------------------------------------------------
static CDbgMemAlloc s_DbgMemAlloc CONSTRUCT_EARLY;

#ifndef TIER0_VALIDATE_HEAP
IMemAlloc *g_pMemAlloc = &s_DbgMemAlloc;
#else
IMemAlloc *g_pActualAlloc = &s_DbgMemAlloc;
#endif

//-----------------------------------------------------------------------------

CThreadMutex g_DbgMemMutex CONSTRUCT_EARLY;

#define HEAP_LOCK() AUTO_LOCK( g_DbgMemMutex )


//-----------------------------------------------------------------------------
// Byte count buckets
//-----------------------------------------------------------------------------
size_t CDbgMemAlloc::s_pCountSizes[CDbgMemAlloc::NUM_BYTE_COUNT_BUCKETS] = 
{
	16, 32, 128, 1024, SIZE_MAX
};

const char *CDbgMemAlloc::s_pCountHeader[CDbgMemAlloc::NUM_BYTE_COUNT_BUCKETS] = 
{
	"<=16 byte allocations", 
	"17-32 byte allocations",
	"33-128 byte allocations", 
	"129-1024 byte allocations",
	">1024 byte allocations"
};

//-----------------------------------------------------------------------------
// Standard output
//-----------------------------------------------------------------------------
static FILE* s_DbgFile;

static void DefaultHeapReportFunc( char const *pFormat, ... )
{
	va_list args;
	va_start( args, pFormat ); //-V2018 //-V2019
	vfprintf( s_DbgFile, pFormat, args );
	va_end( args );
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CDbgMemAlloc::CDbgMemAlloc()  
{
	// Make sure that we return 64-bit addresses in 64-bit builds.
	ReserveBottomMemory();

	m_OutputFunc = DefaultHeapReportFunc;
	m_bInitialized = false;

	if ( !IsDebug() )
	{
		Plat_DebugString( "USE_MEM_DEBUG is enabled in a release build. Don't commit this!\n" );
	}
}

CDbgMemAlloc::~CDbgMemAlloc()
{
	Shutdown();
}


void CDbgMemAlloc::Initialize()
{
	if ( !m_bInitialized )
	{
		m_pFilenames = new Filenames_t;
		// dimhotepus: Allocate reasonable amount.
		m_pFilenames->reserve( 2048 + 1024 );
		m_pStatMap = new StatMap_t;
		// dimhotepus: Allocate reasonable amount.
		m_pStatMap->reserve( 2048 + 1024 );
		m_bInitialized = true;

		// Enable application termination (breakpoint) on heap corruption. This is
		// better than trying to patch it up and continue, both from a security and
		// a bug-finding point of view. Do this always on Windows since the heap is
		// used by video drivers and other in-proc components.
#if defined(PLATFORM_WINDOWS_PC)
		HeapSetInformation( nullptr, HeapEnableTerminationOnCorruption, nullptr, 0 );
#endif
	}
}


//-----------------------------------------------------------------------------
// Release versions
//-----------------------------------------------------------------------------
void CDbgMemAlloc::Shutdown()
{
	if ( m_bInitialized )
	{
		delete m_pFilenames;
		m_pFilenames = nullptr;

		delete m_pStatMap;
		m_pStatMap = nullptr;
	}

	m_bInitialized = false;
}


#ifdef WIN32
extern "C" BOOL APIENTRY MemDbgDllMain( [[maybe_unused]] HMODULE hDll, DWORD dwReason, void* )
{
	// Check if we are shutting down
	if ( dwReason == DLL_PROCESS_DETACH )
	{
		// CDbgMemAlloc is a global object and destructs after the _Lockit object in the CRT runtime,
		//  so we can't actually operate on the STL object in a normal destructor here as its support libraries have been turned off already
		s_DbgMemAlloc.Shutdown();
	}

	return TRUE;
}
#endif


//-----------------------------------------------------------------------------
// Release versions
//-----------------------------------------------------------------------------

void *CDbgMemAlloc::Alloc( size_t nSize )
{
/*
	// NOTE: Uncomment this to find unknown allocations
	const char *pFileName = g_pszUnknown;
	int nLine;
	GetActualDbgInfo( pFileName, nLine );
	if (pFileName == g_pszUnknown)
	{
		int x = 3;
	}
*/
	
	if ( char szModule[MAX_PATH]; GetCallerModule(szModule) )
	{
		return Alloc( nSize, szModule, 0 );
	}

	return Alloc( nSize, g_pszUnknown, 0 );
}

void *CDbgMemAlloc::Realloc( void *pMem, size_t nSize )
{
/*
	// NOTE: Uncomment this to find unknown allocations
	const char *pFileName = g_pszUnknown;
	int nLine;
	GetActualDbgInfo( pFileName, nLine );
	if (pFileName == g_pszUnknown)
	{
		int x = 3;
	}
*/
	// FIXME: Should these gather stats?
	if ( char szModule[MAX_PATH]; GetCallerModule( szModule ) )
	{
		return Realloc( pMem, nSize, szModule, 0 );
	}

	return Realloc( pMem, nSize, g_pszUnknown, 0 );
}

void CDbgMemAlloc::Free( void *pMem )
{
	// FIXME: Should these gather stats?
	Free( pMem, g_pszUnknown, 0 );
}

void *CDbgMemAlloc::Expand_NoLongerSupported( void *, size_t )
{
	return nullptr;
}

void SetupDebugInfoStack(DbgInfoStack_t *&stack, int &stack_depth)
{
	stack = (DbgInfoStack_t *)DebugAlloc( sizeof(DbgInfoStack_t) * DBG_INFO_STACK_DEPTH );
	stack_depth = -1;
}

//-----------------------------------------------------------------------------
// Force file + line information for an allocation
//-----------------------------------------------------------------------------
void CDbgMemAlloc::PushAllocDbgInfo( const char *pFileName, int nLine )
{
	if ( g_DbgInfoStack == nullptr )
	{
		SetupDebugInfoStack( g_DbgInfoStack, g_nDbgInfoStackDepth );
	}

	const int newStackDepth{++g_nDbgInfoStackDepth};
	Assert( newStackDepth < DBG_INFO_STACK_DEPTH );

	DbgInfoStack_t &info{g_DbgInfoStack[newStackDepth]};

	info.m_pFileName = FindOrCreateFilename( pFileName );
	info.m_nLine = nLine;
}

void CDbgMemAlloc::PopAllocDbgInfo()
{
	if ( g_DbgInfoStack == nullptr )
	{
		SetupDebugInfoStack( g_DbgInfoStack, g_nDbgInfoStackDepth );
	}

	[[maybe_unused]] const int newStackDepth{--g_nDbgInfoStackDepth};
	Assert( newStackDepth >= -1 );
}


//-----------------------------------------------------------------------------
// handles storing allocation info for coroutines
//-----------------------------------------------------------------------------
uint32 CDbgMemAlloc::GetDebugInfoSize()
{
	return sizeof( DbgInfoStack_t ) * DBG_INFO_STACK_DEPTH + sizeof( int32 );
}

void CDbgMemAlloc::SaveDebugInfo( void *pvDebugInfo )
{
	if ( g_DbgInfoStack == nullptr )
	{
		SetupDebugInfoStack( g_DbgInfoStack, g_nDbgInfoStackDepth );
	}

	auto *pnStackDepth = (int32*) pvDebugInfo;
	*pnStackDepth = g_nDbgInfoStackDepth;
	memcpy( pnStackDepth+1, &g_DbgInfoStack[0], sizeof( DbgInfoStack_t ) * DBG_INFO_STACK_DEPTH );
}

void CDbgMemAlloc::RestoreDebugInfo( const void *pvDebugInfo )
{
	if ( g_DbgInfoStack == nullptr )
	{
		SetupDebugInfoStack( g_DbgInfoStack, g_nDbgInfoStackDepth );
	}

	const auto *pnStackDepth = (const int32*) pvDebugInfo;
	g_nDbgInfoStackDepth = *pnStackDepth;
	memcpy( &g_DbgInfoStack[0], pnStackDepth+1, sizeof( DbgInfoStack_t ) * DBG_INFO_STACK_DEPTH );

}

void CDbgMemAlloc::InitDebugInfo( void *pvDebugInfo, const char *pchRootFileName, int nLine )
{
	auto *pnStackDepth = (int32*) pvDebugInfo;

	if( pchRootFileName )
	{
		*pnStackDepth = 0;

		auto *pStackRoot = (DbgInfoStack_t *)(pnStackDepth + 1);
		pStackRoot->m_pFileName = FindOrCreateFilename( pchRootFileName );
		pStackRoot->m_nLine = nLine;
	}
	else
	{
		*pnStackDepth = -1;
	}

}


//-----------------------------------------------------------------------------
// Returns the actual debug info
//-----------------------------------------------------------------------------
void CDbgMemAlloc::GetActualDbgInfo( const char *&pFileName, int &nLine )
{
#if defined( USE_STACK_WALK_DETAILED )
	return;
#endif

	if ( g_DbgInfoStack == nullptr )
	{
		SetupDebugInfoStack( g_DbgInfoStack, g_nDbgInfoStackDepth );
	}

	const DbgInfoStack_t &top{g_DbgInfoStack[0]};

	if (g_nDbgInfoStackDepth >= 0 && top.m_pFileName)
	{
		pFileName = top.m_pFileName;
		nLine = top.m_nLine;
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CDbgMemAlloc::FindOrCreateFilename( const char *pFileName )
{
	Initialize();

	if ( !pFileName )
	{
		pFileName = g_pszUnknown;
	}

	// If we created it for the first time, actually *allocate* the filename memory
	HEAP_LOCK();
	// This is necessary for shutdown conditions: the file name is stored
	// in some piece of memory in a DLL; if that DLL becomes unloaded,
	// we'll have a pointer to crap memory

#if defined( USE_STACK_WALK_DETAILED )
{
	// Walk the stack to determine what's causing the allocation
	void *arrStackAddresses[ 10 ] = { 0 };
	int numStackAddrRetrieved = WalkStack( arrStackAddresses, 10, 0 );
	char *szStack = StackDescribe( arrStackAddresses, numStackAddrRetrieved );
	if ( szStack && *szStack )
	{
		pFileName = szStack;		// Use the stack description for the allocation
	}
}
#endif // #if defined( USE_STACK_WALK_DETAILED )

	// dimhotepus: Insert if not found.
	return *m_pFilenames->emplace(pFileName).first;
}

//-----------------------------------------------------------------------------
// Finds the file in our map
//-----------------------------------------------------------------------------
CDbgMemAlloc::MemInfo_t &CDbgMemAlloc::FindOrCreateEntry( const char *pFileName, int line )
{
	Initialize();
	// Oh how I love crazy STL. retval.first == the StatMapIter_t in the std::pair
	// retval.first->second == the MemInfo_t that's part of the StatMapIter_t 
	// dimhotepus: Insert if not found.
	return m_pStatMap
		->try_emplace( MemInfoKey_t( pFileName, line ), MemInfo_t() )
		.first->second;
}


//-----------------------------------------------------------------------------
// Updates stats
//-----------------------------------------------------------------------------
void CDbgMemAlloc::RegisterAllocation( const char *pFileName, int nLine, size_t nLogicalSize, size_t nActualSize, unsigned nTime )
{
	HEAP_LOCK();
	RegisterAllocation( m_GlobalInfo, nLogicalSize, nActualSize, nTime );
	MemInfo_t &info{FindOrCreateEntry( pFileName, nLine )};
	RegisterAllocation( info, nLogicalSize, nActualSize, nTime );
}

void CDbgMemAlloc::RegisterDeallocation( const char *pFileName, int nLine, size_t nLogicalSize, size_t nActualSize, unsigned nTime )
{
	HEAP_LOCK();
	RegisterDeallocation( m_GlobalInfo, nLogicalSize, nActualSize, nTime );
	MemInfo_t &info{FindOrCreateEntry( pFileName, nLine )};
	RegisterDeallocation( info, nLogicalSize, nActualSize, nTime );
}

void CDbgMemAlloc::RegisterAllocation( MemInfo_t &info, size_t nLogicalSize, size_t nActualSize, unsigned nTime )
{
#ifndef __SANITIZE_ADDRESS__
	++info.m_nCurrentCount;
	++info.m_nTotalCount;
	if (info.m_nCurrentCount > info.m_nPeakCount)
	{
		info.m_nPeakCount = info.m_nCurrentCount;
	}

	info.m_nCurrentSize += nLogicalSize;
	info.m_nTotalSize += nLogicalSize;
	if (info.m_nCurrentSize > info.m_nPeakSize) //-V1051
	{
		info.m_nPeakSize = info.m_nCurrentSize;
	}

	for (int i = 0; i < NUM_BYTE_COUNT_BUCKETS; ++i)
	{
		if (nLogicalSize <= s_pCountSizes[i])
		{
			++info.m_pCount[i];
			break;
		}
	}

	Assert( info.m_nPeakCount >= info.m_nCurrentCount );
	Assert( info.m_nPeakSize >= info.m_nCurrentSize );

	info.m_nOverheadSize += (nActualSize - nLogicalSize);
	if (info.m_nOverheadSize > info.m_nPeakOverheadSize)
	{
		info.m_nPeakOverheadSize = info.m_nOverheadSize;
	}

	info.m_nTime += nTime;
#endif
}

void CDbgMemAlloc::RegisterDeallocation( MemInfo_t &info, size_t nLogicalSize, size_t nActualSize, unsigned nTime )
{
#ifndef __SANITIZE_ADDRESS__
	// Check for decrementing these counters below zero. The checks
	// must be done here because these unsigned counters will wrap-around and
	// still be positive.
	Assert( info.m_nCurrentCount != 0 );

	// It is technically legal for code to request allocations of zero bytes, and there are a number of places in our code
	// that do.
	Assert( info.m_nCurrentSize >= nLogicalSize );
	--info.m_nCurrentCount;
	info.m_nCurrentSize -= nLogicalSize;

	for (int i = 0; i < NUM_BYTE_COUNT_BUCKETS; ++i)
	{
		if (nLogicalSize <= s_pCountSizes[i])
		{
			--info.m_pCount[i];
			break;
		}
	}

	Assert( info.m_nPeakCount >= info.m_nCurrentCount );
	Assert( info.m_nPeakSize >= info.m_nCurrentSize );

	info.m_nOverheadSize -= (nActualSize - nLogicalSize);

	info.m_nTime += nTime;
#endif
}


//-----------------------------------------------------------------------------
// Gets the allocation file name
//-----------------------------------------------------------------------------

const char *CDbgMemAlloc::GetAllocatonFileName( void *pMem )
{
	if (!pMem)
		return "";

#ifndef __SANITIZE_ADDRESS__
	if ( CrtDbgMemHeader_t *pHeader = GetCrtDbgMemHeader( pMem ); pHeader->m_pFileName )
		return pHeader->m_pFileName;

	return g_pszUnknown;
#else
	return g_pszUnknown;
#endif
}

//-----------------------------------------------------------------------------
// Gets the allocation file name
//-----------------------------------------------------------------------------
int CDbgMemAlloc::GetAllocatonLineNumber( void *pMem )
{
	if ( !pMem )
		return 0;

#ifndef __SANITIZE_ADDRESS__
	CrtDbgMemHeader_t *pHeader = GetCrtDbgMemHeader( pMem );
	return pHeader->m_nLineNumber;
#else
	return 0;
#endif
}

//-----------------------------------------------------------------------------
// Debug versions of the main allocation methods
//-----------------------------------------------------------------------------
void *CDbgMemAlloc::Alloc( size_t nSize, const char *pFileName, int nLine )
{
	HEAP_LOCK();

	if ( !m_bInitialized )
		return InternalMalloc( nSize, pFileName, nLine );

	if ( pFileName != g_pszUnknown )
		pFileName = FindOrCreateFilename( pFileName );

	GetActualDbgInfo( pFileName, nLine );

	/*
	if ( strcmp( pFileName, "class CUtlVector<int,class CUtlMemory<int> >" ) == 0)
	{
		GetActualDbgInfo( pFileName, nLine );
	}
	*/

	m_Timer.Start();
	void *pMem = InternalMalloc( nSize, pFileName, nLine );
	if ( !pMem )
	{
		// dimhotepus: Try to free some space and allocate again.
		CompactHeap();
		
		pMem = InternalMalloc( nSize, pFileName, nLine );
	}
	m_Timer.End();

	if ( pMem )
	{
		ApplyMemoryInitializations( pMem, nSize );
		RegisterAllocation( pFileName, nLine, nSize, InternalMSize( pMem ), m_Timer.GetDuration().GetMicroseconds() );
	}
	else
	{
		SetCRTAllocFailed( nSize );
	}
	return pMem;
}

void *CDbgMemAlloc::Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine )
{
	HEAP_LOCK();

	pFileName = FindOrCreateFilename( pFileName );

	if ( !m_bInitialized )
		return InternalRealloc( pMem, nSize, pFileName, nLine );

	if ( pMem != nullptr )
	{
		RegisterDeallocation( GetAllocatonFileName( pMem ), GetAllocatonLineNumber( pMem ), InternalLogicalSize( pMem ), InternalMSize( pMem ), 0 );
	}

	GetActualDbgInfo( pFileName, nLine );

	m_Timer.Start();
	pMem = InternalRealloc( pMem, nSize, pFileName, nLine );
	if ( !pMem )
	{
		// dimhotepus: Try to free some space and allocate again.
		CompactHeap();

		pMem = InternalRealloc( pMem, nSize, pFileName, nLine );
	}
	m_Timer.End();

	if ( pMem )
	{
		RegisterAllocation( pFileName, nLine, nSize, InternalMSize( pMem ), m_Timer.GetDuration().GetMicroseconds() );
	}
	else
	{
		SetCRTAllocFailed( nSize );
	}
	return pMem;
}

void  CDbgMemAlloc::Free( void *pMem, const char * /*pFileName*/, int )
{
	if ( !pMem )
		return;

	HEAP_LOCK();

	if ( !m_bInitialized )
	{
		InternalFree( pMem );
		return;
	}

	size_t nOldLogicalSize = InternalLogicalSize( pMem );
	size_t nOldSize = InternalMSize( pMem );
	const char *pOldFileName = GetAllocatonFileName( pMem );
	int oldLine = GetAllocatonLineNumber( pMem );

	m_Timer.Start();
	InternalFree( pMem );
 	m_Timer.End();

	RegisterDeallocation( pOldFileName, oldLine, nOldLogicalSize, nOldSize, m_Timer.GetDuration().GetMicroseconds() );
}

void *CDbgMemAlloc::Expand_NoLongerSupported( void *, size_t, const char *, int )
{
	return nullptr;
}


//-----------------------------------------------------------------------------
// Returns size of a particular allocation
//-----------------------------------------------------------------------------
size_t CDbgMemAlloc::GetSize( void *pMem )
{
	HEAP_LOCK();

	if ( !pMem )
		return CalcHeapUsed();

	return InternalMSize( pMem );
}


//-----------------------------------------------------------------------------
// FIXME: Remove when we make our own heap! Crt stuff we're currently using
//-----------------------------------------------------------------------------
long CDbgMemAlloc::CrtSetBreakAlloc( long lNewBreakAlloc )
{
#ifdef POSIX
	return 0;
#else
	return _CrtSetBreakAlloc( lNewBreakAlloc );
#endif
}

int CDbgMemAlloc::CrtSetReportMode( int nReportType, int nReportMode )
{
#ifdef POSIX
	return 0;
#else
	return _CrtSetReportMode( nReportType, nReportMode );
#endif
}

int CDbgMemAlloc::CrtIsValidHeapPointer( const void *pMem )
{
#ifdef POSIX
	return 0;
#else
	return _CrtIsValidHeapPointer( pMem );
#endif
}

int CDbgMemAlloc::CrtIsValidPointer( const void *pMem, unsigned int size, int access )
{
#ifdef POSIX
	return 0;
#else
	return _CrtIsValidPointer( pMem, size, access );
#endif
}

#define DBGMEM_CHECKMEMORY 1

int CDbgMemAlloc::CrtCheckMemory( )
{
#if !defined( DBGMEM_CHECKMEMORY ) || defined( POSIX )
	return 1;
#else
	if ( !_CrtCheckMemory())
	{
		Msg( "Memory check failed!\n" );
		return 0;
	}
	return 1;
#endif
}

int CDbgMemAlloc::CrtSetDbgFlag( int nNewFlag )
{
#ifdef POSIX
	return 0;
#else
	return _CrtSetDbgFlag( nNewFlag );
#endif
}

void CDbgMemAlloc::CrtMemCheckpoint( _CrtMemState *pState )
{
#ifndef POSIX
	_CrtMemCheckpoint( pState );
#endif
}

// FIXME: Remove when we have our own allocator
void* CDbgMemAlloc::CrtSetReportFile( int nRptType, void* hFile )
{
#ifdef POSIX
	return 0;
#else
	return (void*)_CrtSetReportFile( nRptType, (_HFILE)hFile );
#endif
}

void* CDbgMemAlloc::CrtSetReportHook( void* pfnNewHook )
{
#ifdef POSIX
	return 0;
#else
	return (void*)_CrtSetReportHook( (_CRT_REPORT_HOOK)pfnNewHook );
#endif
}

int CDbgMemAlloc::CrtDbgReport( int nRptType, const char * szFile,
		int nLine, const char * szModule, const char * pMsg )
{
#ifdef POSIX
	return 0;
#else
	return _CrtDbgReport( nRptType, szFile, nLine, szModule, pMsg );
#endif
}

int CDbgMemAlloc::heapchk()
{
#ifdef POSIX
	return 0;
#else
	return _HEAPOK;
#endif
}

void CDbgMemAlloc::DumpBlockStats( void *p )
{
	if ( auto *pBlock = (DbgMemHeader_t *)p - 1; !CrtIsValidHeapPointer( pBlock ) )
	{
		Msg( "0x%p is not valid heap pointer\n", p );
		return;
	}

	const char *pFileName = GetAllocatonFileName( p );
	int line = GetAllocatonLineNumber( p );

	Msg( "0x%p allocated by %s line %d, %llu bytes\n", p, pFileName, line, (uint64)GetSize( p ) );
}

//-----------------------------------------------------------------------------
// Stat output
//-----------------------------------------------------------------------------
void CDbgMemAlloc::DumpMemInfo( const char *pAllocationName, int line, const MemInfo_t &info ) const
{
	m_OutputFunc("%s, line %i\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%d\t%d\t%d\t%d",
		pAllocationName,
		line,
		info.m_nCurrentSize / 1024.0f,
		info.m_nPeakSize / 1024.0f,
		info.m_nTotalSize / 1024.0f,
		info.m_nOverheadSize / 1024.0f,
		info.m_nPeakOverheadSize / 1024.0f,
		(int)(info.m_nTime / 1000),
		info.m_nCurrentCount,
		info.m_nPeakCount,
		info.m_nTotalCount
		);

	for (const auto &b : info.m_pCount)
	{
		m_OutputFunc( "\t%d", b );
	}

	m_OutputFunc("\n");
}


//-----------------------------------------------------------------------------
// Stat output
//-----------------------------------------------------------------------------
void CDbgMemAlloc::DumpFileStats() const
{
	if ( !m_pStatMap )
		return;

	// dimhotepus: Need to preserve sorted order as it was before.
	std::vector<std::pair<MemInfoKey_t, MemInfo_t>> elems( m_pStatMap->begin(), m_pStatMap->end() );
	std::sort( elems.begin(), elems.end(), MemInfoKeySorter{} );

	for ( const auto &e : elems )
	{
		DumpMemInfo( e.first.m_pFileName, e.first.m_nLine, e.second );
	}
}

void CDbgMemAlloc::DumpStatsFileBase( char const *pchFileBase )
{
	HEAP_LOCK();

	char szFileName[MAX_PATH];
	static unsigned s_FileCount = 0;
	if (m_OutputFunc == DefaultHeapReportFunc)
	{
		_snprintf( szFileName, std::size( szFileName ), "%s%u.txt", pchFileBase, s_FileCount );
		szFileName[ std::size(szFileName) - 1 ] = '\0';

		++s_FileCount;

		s_DbgFile = fopen(szFileName, "wt");
		if (!s_DbgFile)
			return;
	}

	m_OutputFunc("Allocation type\tCurrent Size(k)\tPeak Size(k)\tTotal Allocations(k)\tOverhead Size(k)\tPeak Overhead Size(k)\tTime(ms)\tCurrent Count\tPeak Count\tTotal Count");

	for (const auto &b : s_pCountHeader)
	{
		m_OutputFunc( "\t%s", b );
	}

	m_OutputFunc("\n");

	DumpMemInfo( "Totals", 0, m_GlobalInfo );

	DumpFileStats();

	if (m_OutputFunc == DefaultHeapReportFunc)
	{
		fclose(s_DbgFile);
	}
}

void CDbgMemAlloc::GlobalMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory )
{
	if ( !pUsedMemory || !pFreeMemory )
		return;

	// no data
	*pFreeMemory = 0;
	*pUsedMemory = 0;
}

//-----------------------------------------------------------------------------
// Stat output
//-----------------------------------------------------------------------------
void CDbgMemAlloc::DumpStats()
{
	DumpStatsFileBase( "memstats" );
}

void CDbgMemAlloc::SetCRTAllocFailed( size_t nSize )
{
	m_sMemoryAllocFailed = nSize;

	MemAllocOOMError( nSize );
}

size_t CDbgMemAlloc::MemoryAllocFailed()
{
	return m_sMemoryAllocFailed;
}



#if defined( LINUX ) && !defined( NO_HOOK_MALLOC )
//
// Under linux we can ask GLIBC to override malloc for us
//   Base on code from Ryan, https://github.com/icculus/mallocmonitor/blob/fc7c207fb18f61977ba4e46a995f1b9f349246b1/monitor_client/malloc_hook_glibc.c
//
//
static void *glibc_malloc_hook = NULL;
static void *glibc_realloc_hook = NULL;
static void *glibc_memalign_hook = NULL;
static void *glibc_free_hook = NULL;

/* convenience functions for setting the hooks... */
static inline void save_glibc_hooks(void);
static inline void set_glibc_hooks(void);
static inline void set_override_hooks(void);

CThreadMutex g_HookMutex;
/*
 * Our overriding hooks...they call through to the original C runtime
 *  implementations and report to the monitoring daemon.
 */

static void *override_malloc_hook(size_t s, const void *caller)
{
    void *retval;
    AUTO_LOCK( g_HookMutex );
    set_glibc_hooks();  /* put glibc back in control. */
    retval = InternalMalloc( s, NULL, 0 );
    save_glibc_hooks();  /* update in case glibc changed them. */

    set_override_hooks(); /* only restore hooks if daemon is listening */

    return(retval);
} /* override_malloc_hook */


static void *override_realloc_hook(void *ptr, size_t s, const void *caller)
{
    void *retval;
    AUTO_LOCK( g_HookMutex );

    set_glibc_hooks();  /* put glibc back in control. */
    retval = InternalRealloc(ptr, s, NULL, 0);  /* call glibc version. */
    save_glibc_hooks();  /* update in case glibc changed them. */

    set_override_hooks(); /* only restore hooks if daemon is listening */

    return(retval);
} /* override_realloc_hook */


static void *override_memalign_hook(size_t a, size_t s, const void *caller)
{
    void *retval;
    AUTO_LOCK( g_HookMutex );

    set_glibc_hooks();  /* put glibc back in control. */
    retval = memalign(a, s);  /* call glibc version. */
    save_glibc_hooks();  /* update in case glibc changed them. */

    set_override_hooks(); /* only restore hooks if daemon is listening */

    return(retval);
} /* override_memalign_hook */


static void override_free_hook(void *ptr, const void *caller)
{
    AUTO_LOCK( g_HookMutex );

    set_glibc_hooks();  /* put glibc back in control. */
    InternalFree(ptr);  /* call glibc version. */
    save_glibc_hooks();  /* update in case glibc changed them. */

    set_override_hooks(); /* only restore hooks if daemon is listening */
} /* override_free_hook */



/*
 * Convenience functions for swapping the hooks around...
 */

/*
 * Save a copy of the original allocation hooks, so we can call into them
 *  from our overriding functions. It's possible that glibc might change
 *  these hooks under various conditions (so the manual's examples seem
 *  to suggest), so we update them whenever we finish calling into the
 *  the originals.
 */
static inline void save_glibc_hooks(void)
{
    glibc_malloc_hook = (void *)__malloc_hook;
    glibc_realloc_hook = (void *)__realloc_hook;
    glibc_memalign_hook = (void *)__memalign_hook;
    glibc_free_hook = (void *)__free_hook;
} /* save_glibc_hooks */

/*
 * Restore the hooks to the glibc versions. This is needed since, say,
 *  their realloc() might call malloc() or free() under the hood, etc, so
 *  it's safer to let them have complete control over the subsystem, which
 *  also makes our logging saner, too.
 */
static inline void set_glibc_hooks(void)
{
    __malloc_hook = (void* (*)(size_t, const void*))glibc_malloc_hook;
    __realloc_hook = (void* (*)(void*, size_t, const void*))glibc_realloc_hook;
    __memalign_hook = (void* (*)(size_t, size_t, const void*))glibc_memalign_hook;
    __free_hook = (void (*)(void*, const void*))glibc_free_hook;
} /* set_glibc_hooks */


/*
 * Put our hooks back in place. This should be done after the original
 *  glibc version has been called and we've finished any logging (which
 *  may call glibc functions, too). This sets us up for the next calls from
 *  the application.
 */
static inline void set_override_hooks(void)
{
    __malloc_hook = override_malloc_hook;
    __realloc_hook = override_realloc_hook;
    __memalign_hook = override_memalign_hook;
    __free_hook = override_free_hook;
} /* set_override_hooks */



/*
 * The Hook Of All Hooks...how we get in there in the first place.
 */

/*
 * glibc will call this when the malloc subsystem is initializing, giving
 *  us a chance to install hooks that override the functions.
 */
static void __attribute__((constructor)) override_init_hook(void)
{
    AUTO_LOCK( g_HookMutex );

    /* install our hooks. Will connect to daemon on first malloc, etc. */
    save_glibc_hooks();
    set_override_hooks();
} /* override_init_hook */


/*
 * __malloc_initialize_hook is apparently a "weak variable", so you can
 *  define and assign it here even though it's in glibc, too. This lets
 *  us hook into malloc as soon as the runtime initializes, and before
 *  main() is called. Basically, this whole trick depends on this.
 */
void (*__MALLOC_HOOK_VOLATILE __malloc_initialize_hook)(void) __attribute__((visibility("default")))= override_init_hook;

#endif // LINUX


#if defined( OSX ) && !defined( NO_HOOK_MALLOC )
//
// pointers to the osx versions of these functions
static void *osx_malloc_hook = NULL;
static void *osx_realloc_hook = NULL;
static void *osx_free_hook = NULL;

// convenience functions for setting the hooks... 
static inline void save_osx_hooks(void);
static inline void set_osx_hooks(void);
static inline void set_override_hooks(void);

CThreadMutex g_HookMutex;
//
// Our overriding hooks...they call through to the original C runtime
//  implementations and report to the monitoring daemon.
//

static void *override_malloc_hook(struct _malloc_zone_t *zone, size_t s)
{
    void *retval;
    set_osx_hooks(); 
    retval = InternalMalloc( s, NULL, 0 );
    set_override_hooks(); 
	
    return(retval);
} 


static void *override_realloc_hook(struct _malloc_zone_t *zone, void *ptr, size_t s)
{
    void *retval;
	
    set_osx_hooks();  
    retval = InternalRealloc(ptr, s, NULL, 0);	
    set_override_hooks(); 
	
    return(retval);
} 


static void override_free_hook(struct _malloc_zone_t *zone, void *ptr)
{
	// sometime they pass in a null pointer from higher level calls, just ignore it
	if ( !ptr )
		return;
	
    set_osx_hooks(); 
	
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( ptr );
	if ( *((int*)pInternalMem->m_Reserved) == 0xf00df00d )
	{
		InternalFree( ptr );
	}
    
    set_override_hooks(); 
} 

static inline void unprotect_malloc_zone( malloc_zone_t *malloc_zone )
{
	// Starting in OS X 10.7 the default zone defaults to read-only, version 8.
	// The version check may not be necessary, but we know it was RW before that.
	if ( malloc_zone->version >= 8 )
	{
		vm_protect( mach_task_self(), (uintptr_t)malloc_zone, sizeof( malloc_zone_t ), 0, VM_PROT_READ | VM_PROT_WRITE );
	}
}

static inline void protect_malloc_zone( malloc_zone_t *malloc_zone )
{
	if ( malloc_zone->version >= 8 )
	{
		vm_protect( mach_task_self(), (uintptr_t)malloc_zone, sizeof( malloc_zone_t ), 0, VM_PROT_READ );
	}
}

//
//  Save a copy of the original allocation hooks, so we can call into them
//   from our overriding functions. It's possible that osx might change
//   these hooks under various conditions (so the manual's examples seem
//   to suggest), so we update them whenever we finish calling into the
//   the originals.
//
static inline void save_osx_hooks(void)
{
	malloc_zone_t *malloc_zone = malloc_default_zone();

	osx_malloc_hook = (void *)malloc_zone->malloc;
	osx_realloc_hook = (void *)malloc_zone->realloc;
	osx_free_hook = (void *)malloc_zone->free;

	// These are func's we could optionally override right now on OSX but don't need to
	// osx_size_hook = (void *)malloc_zone->size;
	// osx_calloc_hook = (void *)malloc_zone->calloc;
	// osx_valloc_hook = (void *)malloc_zone->valloc;
	// osx_destroy_hook = (void *)malloc_zone->destroy;
}

//
//  Restore the hooks to the osx versions. This is needed since, say,
//   their realloc() might call malloc() or free() under the hood, etc, so
//   it's safer to let them have complete control over the subsystem, which
//   also makes our logging saner, too.
//
static inline void set_osx_hooks(void)
{
	malloc_zone_t *malloc_zone = malloc_default_zone();

	unprotect_malloc_zone( malloc_zone );
	malloc_zone->malloc = (void* (*)(_malloc_zone_t*, size_t))osx_malloc_hook;
	malloc_zone->realloc = (void* (*)(_malloc_zone_t*, void*, size_t))osx_realloc_hook;
	malloc_zone->free = (void (*)(_malloc_zone_t*, void*))osx_free_hook;
	protect_malloc_zone( malloc_zone );

	// These are func's we could optionally override right now on OSX but don't need to

	//malloc_zone->size = (size_t (*)(_malloc_zone_t*, const void *))osx_size_hook;
	//malloc_zone->calloc = (void* (*)(_malloc_zone_t*, size_t, size_t))osx_calloc_hook;
	//malloc_zone->valloc = (void* (*)(_malloc_zone_t*, size_t))osx_valloc_hook;
	//malloc_zone->destroy = (void (*)(_malloc_zone_t*))osx_destroy_hook;
}


/*
 * Put our hooks back in place. This should be done after the original
 *  osx version has been called and we've finished any logging (which
 *  may call osx functions, too). This sets us up for the next calls from
 *  the application.
 */
static inline void set_override_hooks(void)
{
	malloc_zone_t *malloc_zone = malloc_default_zone();
	AssertMsg( malloc_zone, "No malloc zone returned by malloc_default_zone" );

	unprotect_malloc_zone( malloc_zone );
	malloc_zone->malloc = override_malloc_hook;
	malloc_zone->realloc = override_realloc_hook;
	malloc_zone->free = override_free_hook;
	protect_malloc_zone( malloc_zone );

	// These are func's we could optionally override right now on OSX but don't need to
	//malloc_zone->size = override_size_hook;
	//malloc_zone->calloc = override_calloc_hook;
	// malloc_zone->valloc = override_valloc_hook;
	//malloc_zone->destroy = override_destroy_hook;
}


//
// The Hook Of All Hooks...how we get in there in the first place.
//
// osx will call this when the malloc subsystem is initializing, giving
// us a chance to install hooks that override the functions.
//

void __attribute__ ((constructor)) mem_init(void)
{
    AUTO_LOCK( g_HookMutex );
	save_osx_hooks();
    set_override_hooks();
}

void *operator new( size_t nSize, int nBlockUse, const char *pFileName, int nLine )
{
	set_osx_hooks(); 
	void *pMem = g_pMemAlloc->Alloc(nSize, pFileName, nLine);
	set_override_hooks(); 
	return pMem;
}

void *operator new[] ( size_t nSize, int nBlockUse, const char *pFileName, int nLine )
{
	set_osx_hooks(); 
	void *pMem = g_pMemAlloc->Alloc(nSize, pFileName, nLine);
	set_override_hooks(); 
	return pMem;
}


#endif // defined( OSX ) && !defined( NO_HOOK_MALLOC )


#endif // (defined(_DEBUG) || defined(USE_MEM_DEBUG))

#endif // !STEAM && !NO_MALLOC_OVERRIDE
