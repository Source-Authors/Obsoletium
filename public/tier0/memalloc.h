// Copyright Valve Corporation, All rights reserved.
//
// Purpose: This header should never be used directly from leaf code!!!
// Instead, just add the file memoverride.cpp into your project and all this
// will automagically be used

#ifndef SE_PUBLIC_TIER0_MEMALLOC_H_
#define SE_PUBLIC_TIER0_MEMALLOC_H_

// These memory debugging switches aren't relevant under Linux builds since memoverride.cpp
// isn't built into Linux projects
#ifndef POSIX
// Define this in release to get memory tracking even in release builds
//#define USE_MEM_DEBUG 1
#endif

#if defined( _MEMTEST )
#ifdef _WIN32
#define USE_MEM_DEBUG 1
#endif
#endif

// Undefine this if using a compiler lacking threadsafe RTTI (like vc6)
#define MEM_DEBUG_CLASSNAME 1

#include <cstddef>
#include <cstdint>   // std::uintptr_t
#include <cstdlib>   // malloc, free, posix_memalign
#include <cstring>   // memcpy
#if defined( OSX )
#include <malloc/malloc.h>
#elif defined( POSIX )
#include <malloc.h>  // malloc_usable_size
#endif

#include "mem.h"

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

struct _CrtMemState;

constexpr inline int MEMALLOC_VERSION{1};

using MemAllocFailHandler_t = size_t (*)(size_t);

//-----------------------------------------------------------------------------
// NOTE! This should never be called directly from leaf code
// Just use new,delete,malloc,free etc. They will call into this eventually
//-----------------------------------------------------------------------------
abstract_class IMemAlloc
{
public:
	// Release versions
	virtual void *Alloc( size_t nSize ) = 0;
	virtual void *Realloc( void *pMem, size_t nSize ) = 0;
	virtual void Free( void *pMem ) = 0;
    virtual void *Expand_NoLongerSupported( void *pMem, size_t nSize ) = 0;

	// Debug versions
    virtual void *Alloc( size_t nSize, const char *pFileName, int nLine ) = 0;
    virtual void *Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine ) = 0;
    virtual void  Free( void *pMem, const char *pFileName, int nLine ) = 0;
    virtual void *Expand_NoLongerSupported( void *pMem, size_t nSize, const char *pFileName, int nLine ) = 0;

	// Returns size of a particular allocation
	virtual size_t GetSize( void *pMem ) = 0;

    // Force file + line information for an allocation
    virtual void PushAllocDbgInfo( const char *pFileName, int nLine ) = 0;
    virtual void PopAllocDbgInfo() = 0;

	// FIXME: Remove when we have our own allocator
	// these methods of the Crt debug code is used in our codebase currently
	virtual long CrtSetBreakAlloc( long lNewBreakAlloc ) = 0;
	virtual	int CrtSetReportMode( int nReportType, int nReportMode ) = 0;
	virtual int CrtIsValidHeapPointer( const void *pMem ) = 0;
	virtual int CrtIsValidPointer( const void *pMem, unsigned int size, int access ) = 0;
	virtual int CrtCheckMemory( ) = 0;
	virtual int CrtSetDbgFlag( int nNewFlag ) = 0;
	virtual void CrtMemCheckpoint( _CrtMemState *pState ) = 0;

	// FIXME: Make a better stats interface
	virtual void DumpStats() = 0;
	virtual void DumpStatsFileBase( char const *pchFileBase ) = 0;

	// FIXME: Remove when we have our own allocator
	virtual void* CrtSetReportFile( int nRptType, void* hFile ) = 0;
	virtual void* CrtSetReportHook( void* pfnNewHook ) = 0;
	virtual int CrtDbgReport( int nRptType, const char * szFile,
			int nLine, const char * szModule, const char * pMsg ) = 0;

	virtual int heapchk() = 0;

	virtual bool IsDebugHeap() = 0;

	virtual void GetActualDbgInfo( const char *&pFileName, int &nLine ) = 0;
	virtual void RegisterAllocation( const char *pFileName, int nLine, size_t nLogicalSize, size_t nActualSize, unsigned nTime ) = 0;
	virtual void RegisterDeallocation( const char *pFileName, int nLine, size_t nLogicalSize, size_t nActualSize, unsigned nTime ) = 0;

	virtual int GetVersion() = 0;

	virtual void CompactHeap() = 0;

	// Function called when malloc fails or memory limits hit to attempt to free up memory (can come in any thread)
	virtual MemAllocFailHandler_t SetAllocFailHandler( MemAllocFailHandler_t pfnMemAllocFailHandler ) = 0;

	virtual void DumpBlockStats( void * ) = 0;

#if defined( _MEMTEST )	
	virtual void SetStatsExtraInfo( const char *pMapName, const char *pComment ) = 0;
#endif

	// Returns 0 if no failure, otherwise the size_t of the last requested chunk
	//  "I'm sure this is completely thread safe!" Brian Deen 7/19/2012.
	virtual size_t MemoryAllocFailed() = 0;

	// handles storing allocation info for coroutines
	virtual uint32 GetDebugInfoSize() = 0;
	virtual void SaveDebugInfo( void *pvDebugInfo ) = 0;
	virtual void RestoreDebugInfo( const void *pvDebugInfo ) = 0;	
	virtual void InitDebugInfo( void *pvDebugInfo, const char *pchRootFileName, int nLine ) = 0;

	// Replacement for ::GlobalMemoryStatus which accounts for unused memory in our system
	virtual void GlobalMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory ) = 0;
};

//-----------------------------------------------------------------------------
// Singleton interface
//-----------------------------------------------------------------------------
MEM_INTERFACE IMemAlloc *g_pMemAlloc;

//-----------------------------------------------------------------------------

#ifdef MEMALLOC_REGIONS
#ifndef MEMALLOC_REGION
#define MEMALLOC_REGION 0
#endif
inline void *MemAlloc_Alloc( size_t nSize )
{ 
	return g_pMemAlloc->RegionAlloc( MEMALLOC_REGION, nSize );
}

inline void *MemAlloc_Alloc( size_t nSize, const char *pFileName, int nLine )
{ 
	return g_pMemAlloc->RegionAlloc( MEMALLOC_REGION, nSize, pFileName, nLine );
}
#else
#undef MEMALLOC_REGION
inline void *MemAlloc_Alloc( size_t nSize )
{ 
	return g_pMemAlloc->Alloc( nSize );
}

inline void *MemAlloc_Alloc( size_t nSize, const char *pFileName, int nLine )
{ 
	return g_pMemAlloc->Alloc( nSize, pFileName, nLine );
}
#endif
inline void MemAlloc_Free( void *ptr )
{
	g_pMemAlloc->Free( ptr );
}
inline void MemAlloc_Free( void *ptr, const char *pFileName, int nLine )
{
	g_pMemAlloc->Free( ptr, pFileName, nLine );
}

//-----------------------------------------------------------------------------

constexpr inline bool ValueIsPowerOfTwo( size_t value )			// don't clash with mathlib definition
{
	return IsPowerOfTwo( value );
}

// dimhotepus: Shared implementation of the over-aligned allocation helpers below
// (they used to be four copy-pasted bodies). Block layout is unchanged:
//
//   pBase                                   pResult (aligned)
//   v                                       v
//   [ padding ............ | void *pBase ][ user data ... ]
//
// Pointers go through std::uintptr_t (not size_t) and the base pointer is stored with
// memcpy, so this is well-defined on every data model (ILP32, LLP64, LP64).
namespace memalloc_detail
{
	// Effective alignment minus one; never below pointer alignment so the slot fits.
	[[nodiscard]] constexpr size_t AlignMask( size_t align ) noexcept
	{
		return ( align > sizeof( void * ) ? align : sizeof( void * ) ) - 1;
	}

	[[nodiscard]] inline void *AlignInBlock( void *pAlloc, size_t alignMask ) noexcept
	{
		auto *pBytes = static_cast<unsigned char *>( pAlloc );
		const std::uintptr_t base{ reinterpret_cast<std::uintptr_t>( pBytes ) };
		const std::uintptr_t aligned{ ( base + sizeof( void * ) + alignMask ) & ~static_cast<std::uintptr_t>( alignMask ) };
		// Offset from pBytes keeps pointer provenance (no int -> pointer round trip).
		unsigned char *pResult{ pBytes + ( aligned - base ) };
		memcpy( pResult - sizeof( void * ), &pAlloc, sizeof( void * ) );
		return pResult;
	}

	[[nodiscard]] inline void *BlockFromAligned( void *pMemBlock ) noexcept
	{
		// The slot sits right below the (at least pointer-aligned) user pointer.
		auto *pBytes = static_cast<unsigned char *>( pMemBlock );
		const std::uintptr_t misalign{ reinterpret_cast<std::uintptr_t>( pBytes ) & ( sizeof( void * ) - 1 ) };
		void *pAlloc;
		memcpy( &pAlloc, pBytes - misalign - sizeof( void * ), sizeof( void * ) );
		return pAlloc;
	}

	template <typename AllocFn>
	[[nodiscard]] inline void *AllocAligned( size_t size, size_t align, AllocFn &&alloc )
	{
		if ( !ValueIsPowerOfTwo( align ) )
			return nullptr;

		const size_t alignMask{ AlignMask( align ) };
		const size_t overhead{ sizeof( void * ) + alignMask };
		// dimhotepus: Huge sizes used to wrap around to a tiny block -> heap overflow.
		if ( size > static_cast<size_t>( -1 ) - overhead )
			return nullptr;

		void *pAlloc{ alloc( size + overhead ) };
		return pAlloc ? AlignInBlock( pAlloc, alignMask ) : nullptr;
	}
}  // namespace memalloc_detail

inline void *MemAlloc_AllocAligned( size_t size, size_t align )
{
	return memalloc_detail::AllocAligned( size, align,
		[]( size_t n ) { return g_pMemAlloc->Alloc( n ); } );
}

inline void *MemAlloc_AllocAligned( size_t size, size_t align, const char *pszFile, int nLine )
{
	return memalloc_detail::AllocAligned( size, align,
		[pszFile, nLine]( size_t n ) { return g_pMemAlloc->Alloc( n, pszFile, nLine ); } );
}

inline void *MemAlloc_AllocAlignedUnattributed( size_t size, size_t align )
{
	return memalloc_detail::AllocAligned( size, align,
		[]( size_t n ) { return MemAlloc_Alloc( n ); } );
}

inline void *MemAlloc_AllocAlignedFileLine( size_t size, size_t align, const char *pszFile, int nLine )
{
	return memalloc_detail::AllocAligned( size, align,
		[pszFile, nLine]( size_t n ) { return MemAlloc_Alloc( n, pszFile, nLine ); } );
}

inline void *MemAlloc_ReallocAligned( void *ptr, size_t size, size_t align )
{
	if ( !IsPowerOfTwo( align ) )
		return nullptr;

	if ( !ptr )
		return MemAlloc_AllocAligned( size, align );

	// Don't change alignment between allocation + reallocation.
	if ( ( reinterpret_cast<std::uintptr_t>( ptr ) & memalloc_detail::AlignMask( align ) ) != 0 )
		return nullptr;

	// Figure out the actual allocation point
	void *pAlloc{ memalloc_detail::BlockFromAligned( ptr ) };

	// See if we have enough space
	const size_t nOffset{ static_cast<size_t>( static_cast<unsigned char *>( ptr ) - static_cast<unsigned char *>( pAlloc ) ) };
	const size_t nOldSize{ g_pMemAlloc->GetSize( pAlloc ) };
	// dimhotepus: Compare without computing size + nOffset, which can overflow.
	if ( size <= nOldSize - nOffset )
		return ptr;

	void *pResult{ MemAlloc_AllocAligned( size, align ) };
	// dimhotepus: Like realloc(), leave the original block valid and owned by the
	// caller when the new allocation fails (it used to be freed -> data loss).
	if ( !pResult )
		return nullptr;

	memcpy( pResult, ptr, nOldSize - nOffset );
	g_pMemAlloc->Free( pAlloc );
	return pResult;
}

inline void MemAlloc_FreeAligned( void *pMemBlock )
{
	if ( pMemBlock == nullptr )
		return;

	g_pMemAlloc->Free( memalloc_detail::BlockFromAligned( pMemBlock ) );
}

inline void MemAlloc_FreeAligned( void *pMemBlock, const char *pFileName, int nLine )
{
	if ( pMemBlock == nullptr )
		return;

	g_pMemAlloc->Free( memalloc_detail::BlockFromAligned( pMemBlock ), pFileName, nLine );
}

inline size_t MemAlloc_GetSizeAligned( void *pMemBlock )
{
	if ( pMemBlock == nullptr )
		return 0;

	void *pAlloc{ memalloc_detail::BlockFromAligned( pMemBlock ) };
	return g_pMemAlloc->GetSize( pAlloc ) -
		static_cast<size_t>( static_cast<unsigned char *>( pMemBlock ) - static_cast<unsigned char *>( pAlloc ) );
}

//-----------------------------------------------------------------------------

#if (!defined(NDEBUG) || defined(USE_MEM_DEBUG))
#define MEM_ALLOC_CREDIT_JOIN_AGAIN( a, b )							a ## b
#define MEM_ALLOC_CREDIT_JOIN( a, b )								MEM_ALLOC_CREDIT_JOIN_AGAIN( a, b )
#define MEM_ALLOC_CREDIT_(tag)										CMemAllocAttributeAlloction MEM_ALLOC_CREDIT_JOIN( memAllocAttributeAlloction, __LINE__ )( tag, __LINE__ )
#define MemAlloc_PushAllocDbgInfo( pszFile, line ) g_pMemAlloc->PushAllocDbgInfo( pszFile, line )
#define MemAlloc_PopAllocDbgInfo() g_pMemAlloc->PopAllocDbgInfo()
#define MemAlloc_RegisterAllocation( pFileName, nLine, nLogicalSize, nActualSize, nTime ) g_pMemAlloc->RegisterAllocation( pFileName, nLine, nLogicalSize, nActualSize, nTime )
#define MemAlloc_RegisterDeallocation( pFileName, nLine, nLogicalSize, nActualSize, nTime ) g_pMemAlloc->RegisterDeallocation( pFileName, nLine, nLogicalSize, nActualSize, nTime )
#else
#define MEM_ALLOC_CREDIT_(tag)	((void)0)
#define MemAlloc_PushAllocDbgInfo( pszFile, line ) ((void)0)
#define MemAlloc_PopAllocDbgInfo() ((void)0)
#define MemAlloc_RegisterAllocation( pFileName, nLine, nLogicalSize, nActualSize, nTime ) ((void)0)
#define MemAlloc_RegisterDeallocation( pFileName, nLine, nLogicalSize, nActualSize, nTime ) ((void)0)
#endif

#define MemAlloc_DumpStats() g_pMemAlloc->DumpStats()
#define MemAlloc_CompactHeap() g_pMemAlloc->CompactHeap()
#define MemAlloc_CompactIncremental() g_pMemAlloc->CompactIncremental()
#define MemAlloc_DumpStatsFileBase( _filename ) g_pMemAlloc->DumpStatsFileBase( _filename )
#define MemAlloc_CrtCheckMemory() g_pMemAlloc->CrtCheckMemory()
#define MemAlloc_GlobalMemoryStatus( _usedMemory, _freeMemory ) g_pMemAlloc->GlobalMemoryStatus( _usedMemory, _freeMemory )
#define MemAlloc_MemoryAllocFailed() g_pMemAlloc->MemoryAllocFailed()

#define MemAlloc_GetDebugInfoSize() g_pMemAlloc->GetDebugInfoSize()
#define MemAlloc_SaveDebugInfo( pvDebugInfo ) g_pMemAlloc->SaveDebugInfo( pvDebugInfo )
#define MemAlloc_RestoreDebugInfo( pvDebugInfo ) g_pMemAlloc->RestoreDebugInfo( pvDebugInfo )
#define MemAlloc_InitDebugInfo( pvDebugInfo, pchRootFileName, nLine ) g_pMemAlloc->InitDebugInfo( pvDebugInfo, pchRootFileName, nLine )
#define MemAlloc_GetSize( x ) g_pMemAlloc->GetSize( x )
//-----------------------------------------------------------------------------

class CMemAllocAttributeAlloction
{
public:
	CMemAllocAttributeAlloction( [[maybe_unused]] const char *pszFile, [[maybe_unused]] int line ) 
	{
		MemAlloc_PushAllocDbgInfo( pszFile, line );
	}
	
	~CMemAllocAttributeAlloction()
	{
		MemAlloc_PopAllocDbgInfo();
	}
};

#define MEM_ALLOC_CREDIT()	MEM_ALLOC_CREDIT_(__FILE__)

//-----------------------------------------------------------------------------

// dimhotepus: Class/function credits on every platform (used to be Windows-only), and
// only when RTTI is actually enabled (MSVC: _CPPRTTI, GCC/Clang: __GXX_RTTI).
#if ( defined( _CPPRTTI ) || defined( __GXX_RTTI ) ) && defined( MEM_DEBUG_CLASSNAME )
	#define SE_MEM_ALLOC_HAS_RTTI_CLASSNAME 1
#endif

#if !defined(NDEBUG) || defined(USE_MEM_DEBUG)

	#include <typeinfo>

	// MEM_DEBUG_CLASSNAME is opt-in.
	// Note: type_info::name() returns a string with static storage duration, as the
	// debug info stack requires. It is thread-safe on MSVC (UCRT caches the undecorated
	// name with interlocked operations) and on Itanium ABI (GCC/Clang: a constant).
	#if defined( SE_MEM_ALLOC_HAS_RTTI_CLASSNAME )
		#define MEM_ALLOC_CREDIT_CLASS()	MEM_ALLOC_CREDIT_( typeid(*this).name() )
		#define MEM_ALLOC_CLASSNAME(type) (typeid((type*)(0)).name())
	#else
		#define MEM_ALLOC_CREDIT_CLASS()	MEM_ALLOC_CREDIT_( __FILE__ )
		#define MEM_ALLOC_CLASSNAME(type) (__FILE__)
	#endif

	// MEM_ALLOC_CREDIT_FUNCTION is used when no this pointer is available ( inside 'new' overloads, for example )
	// dimhotepus: __FUNCTION__ is supported by MSVC, GCC and Clang. The non-MSVC branch
	// used to expand to a bare (__FILE__) expression, i.e. no credit at all.
	#define MEM_ALLOC_CREDIT_FUNCTION()		MEM_ALLOC_CREDIT_( __FUNCTION__ )
#else
	#define MEM_ALLOC_CREDIT_CLASS()
	#define MEM_ALLOC_CLASSNAME(type) nullptr
	#define MEM_ALLOC_CREDIT_FUNCTION() 
#endif

//-----------------------------------------------------------------------------

#if (!defined(NDEBUG) || defined(USE_MEM_DEBUG))
#include <mutex>

struct MemAllocFileLine_t
{
	const char *pszFile;
	int line;
};

// dimhotepus: Name copy lives in tier0's heap so it outlives the module that defines the
// tracking (stats keep the pointer after a DLL unload). Null-safe on allocation failure.
inline const char *MemAlloc_CopyTrackingName( const char *pszName )
{
	const size_t nSize{ strlen( pszName ) + 1 };
	auto *pszCopy = static_cast<char *>( g_pMemAlloc->Alloc( nSize, "intentional leak", 0 ) );
	if ( !pszCopy )
		return pszName;

	memcpy( pszCopy, pszName, nSize );
	return pszCopy;
}

// dimhotepus: The map is shared by every thread that reports external allocations,
// so it is now guarded by a per-tag mutex. Macros are do { } while ( false ) so they
// behave as one statement (they still need the trailing ';', as in release builds).
#define MEMALLOC_DEFINE_EXTERNAL_TRACKING( tag ) \
	static CUtlMap<void *, MemAllocFileLine_t, int> g_##tag##Allocs( DefLessFunc( void *) ); \
	static std::mutex g_##tag##AllocsMutex; \
	static const char *g_psz##tag##Alloc = MemAlloc_CopyTrackingName( #tag "Alloc" );

#define MemAlloc_RegisterExternalAllocation( tag, p, size ) \
	do \
	{ \
		if ( p ) \
		{ \
			MemAllocFileLine_t fileLine = { g_psz##tag##Alloc, 0 }; \
			g_pMemAlloc->GetActualDbgInfo( fileLine.pszFile, fileLine.line ); \
			if ( fileLine.pszFile != g_psz##tag##Alloc ) \
			{ \
				std::lock_guard<std::mutex> lock_##tag##Allocs( g_##tag##AllocsMutex ); \
				g_##tag##Allocs.Insert( p, fileLine ); \
			} \
			\
			MemAlloc_RegisterAllocation( fileLine.pszFile, fileLine.line, size, size, 0 ); \
		} \
	} while ( false )

#define MemAlloc_RegisterExternalDeallocation( tag, p, size ) \
	do \
	{ \
		if ( p ) \
		{ \
			MemAllocFileLine_t fileLine = { g_psz##tag##Alloc, 0 }; \
			{ \
				std::lock_guard<std::mutex> lock_##tag##Allocs( g_##tag##AllocsMutex ); \
				const auto iRecordedFileLine = g_##tag##Allocs.Find( p ); \
				if ( iRecordedFileLine != g_##tag##Allocs.InvalidIndex() ) \
				{ \
					fileLine = g_##tag##Allocs[iRecordedFileLine]; \
					g_##tag##Allocs.RemoveAt( iRecordedFileLine ); \
				} \
			} \
			\
			MemAlloc_RegisterDeallocation( fileLine.pszFile, fileLine.line, size, size, 0 ); \
		} \
	} while ( false )

#else

#define MEMALLOC_DEFINE_EXTERNAL_TRACKING( tag )
#define MemAlloc_RegisterExternalAllocation( tag, p, size ) ((void)0)
#define MemAlloc_RegisterExternalDeallocation( tag, p, size ) ((void)0)

#endif

//-----------------------------------------------------------------------------

#elif defined( POSIX )

inline void MemAlloc_CheckAlloc( void *ptr, size_t nSize )
{
	if ( !ptr )
		MemAllocOOMError( nSize );
}

// dimhotepus: One aligned allocation path for Linux and macOS. posix_memalign is POSIX
// (macOS 10.6+, glibc, musl) and, unlike the old macOS memalign() shim that just called
// malloc(), honors alignments above 16 (AVX, cache lines). Alignment is raised to the
// POSIX minimum (sizeof(void*)) and rounded up to a power of two, as glibc memalign does.
inline void *MemAlloc_PosixAlignedAlloc( size_t alignment, size_t size )
{
	size_t align{ alignment < sizeof( void * ) ? sizeof( void * ) : alignment };
	if ( ( align & ( align - 1 ) ) != 0 )
	{
		size_t pow2{ sizeof( void * ) };
		while ( pow2 < align && pow2 <= ( static_cast<size_t>( -1 ) >> 1 ) )
			pow2 <<= 1;
		if ( pow2 < align )
			return nullptr;
		align = pow2;
	}

	void *ptr{ nullptr };
	return posix_memalign( &ptr, align, size ) == 0 ? ptr : nullptr;
}

#if defined( OSX )
// Kept for source compatibility; now really aligns.
inline void *memalign( size_t alignment, size_t size ) { void *pTmp = MemAlloc_PosixAlignedAlloc( alignment, size ); MemAlloc_CheckAlloc( pTmp, size ); return pTmp; }
#endif

inline void *_aligned_malloc( size_t nSize, size_t align )																{ void *ptr = MemAlloc_PosixAlignedAlloc( align, nSize ); MemAlloc_CheckAlloc( ptr, nSize ); return ptr;  }
inline void _aligned_free( void *ptr )																					{ free( ptr ); }

inline void *MemAlloc_Alloc( size_t nSize, [[maybe_unused]] const char *pFileName = nullptr, [[maybe_unused]] int nLine = 0 )							{ void *ptr = malloc( nSize ); MemAlloc_CheckAlloc( ptr, nSize ); return ptr; }
inline void MemAlloc_Free( void *ptr, [[maybe_unused]] const char *pFileName = nullptr, [[maybe_unused]] int nLine = 0 )									{ free( ptr ); }

inline void *MemAlloc_AllocAligned( size_t size, size_t align, [[maybe_unused]] const char *pszFile = nullptr, [[maybe_unused]] int nLine = 0  )	        { void *ptr = MemAlloc_PosixAlignedAlloc( align, size ); MemAlloc_CheckAlloc( ptr, size ); return ptr; }
inline void *MemAlloc_AllocAlignedFileLine( size_t size, size_t align, [[maybe_unused]] const char *pszFile = nullptr, [[maybe_unused]] int nLine = 0 )	{ void *ptr = MemAlloc_PosixAlignedAlloc( align, size ); MemAlloc_CheckAlloc( ptr, size ); return ptr; }
inline void MemAlloc_FreeAligned( void *pMemBlock, [[maybe_unused]] const char *pszFile = nullptr, [[maybe_unused]] int nLine = 0 ) 						{ free( pMemBlock ); }

#if defined( OSX )
inline size_t _msize( void *ptr )																						{ return ptr ? malloc_size( ptr ) : 0; }
#else
inline size_t _msize( void *ptr )																						{ return ptr ? malloc_usable_size( ptr ) : 0; }
#endif

inline void *MemAlloc_ReallocAligned( void *ptr, size_t size, size_t align )
{
	void *ptr_new_aligned = MemAlloc_PosixAlignedAlloc( align, size );

	if( ptr_new_aligned )
	{
		// dimhotepus: memcpy from nullptr is UB even for 0 bytes.
		if ( ptr )
		{
			size_t old_size = _msize( ptr );
			size_t copy_size = ( size < old_size ) ? size : old_size;

			memcpy( ptr_new_aligned, ptr, copy_size );
			free( ptr );
		}
	}

	MemAlloc_CheckAlloc( ptr_new_aligned, size ); 
	return ptr_new_aligned;
}
#else
#define MemAlloc_GetDebugInfoSize() g_pMemAlloc->GetDebugInfoSize()
#define MemAlloc_SaveDebugInfo( pvDebugInfo ) g_pMemAlloc->SaveDebugInfo( pvDebugInfo )
#define MemAlloc_RestoreDebugInfo( pvDebugInfo ) g_pMemAlloc->RestoreDebugInfo( pvDebugInfo )
#define MemAlloc_InitDebugInfo( pvDebugInfo, pchRootFileName, nLine ) g_pMemAlloc->InitDebugInfo( pvDebugInfo, pchRootFileName, nLine )

#endif // !STEAM && !NO_MALLOC_OVERRIDE

//-----------------------------------------------------------------------------

#if !defined(STEAM) && defined(NO_MALLOC_OVERRIDE)

#define MEM_ALLOC_CREDIT_(tag)	((void)0)
#define MEM_ALLOC_CREDIT()	MEM_ALLOC_CREDIT_(__FILE__)
#define MEM_ALLOC_CREDIT_FUNCTION()
#define MEM_ALLOC_CREDIT_CLASS()
#define MEM_ALLOC_CLASSNAME(type) nullptr

#define MemAlloc_PushAllocDbgInfo( pszFile, line )
#define MemAlloc_PopAllocDbgInfo()
#define MemAlloc_RegisterAllocation( pFileName, nLine, nLogicalSize, nActualSize, nTime ) ((void)0)
#define MemAlloc_RegisterDeallocation( pFileName, nLine, nLogicalSize, nActualSize, nTime ) ((void)0)
#define MemAlloc_DumpStats() ((void)0)
#define MemAlloc_CompactHeap() ((void)0)
#define MemAlloc_CompactIncremental() ((void)0)
#define MemAlloc_DumpStatsFileBase( _filename ) ((void)0)
inline bool MemAlloc_CrtCheckMemory() { return true; }
inline void MemAlloc_GlobalMemoryStatus( size_t *pusedMemory, size_t *pfreeMemory )
{
	*pusedMemory = 0;
	*pfreeMemory = 0;
}
#define MemAlloc_MemoryAllocFailed() 0

#define MemAlloc_GetDebugInfoSize() 0
#define MemAlloc_SaveDebugInfo( pvDebugInfo ) ((void)0)
#define MemAlloc_RestoreDebugInfo( pvDebugInfo ) ((void)0)
#define MemAlloc_InitDebugInfo( pvDebugInfo, pchRootFileName, nLine ) ((void)0)


#define MEMALLOC_DEFINE_EXTERNAL_TRACKING( tag )
#define MemAlloc_RegisterExternalAllocation( tag, p, size ) ((void)0)
#define MemAlloc_RegisterExternalDeallocation( tag, p, size ) ((void)0)

#endif // !STEAM && NO_MALLOC_OVERRIDE

//-----------------------------------------------------------------------------



// linux memory tracking via hooks.
#if defined( POSIX ) && !defined( NO_HOOK_MALLOC )
PLATFORM_INTERFACE void MemoryLogMessage( char const *s );						// throw a message into the memory log
PLATFORM_INTERFACE void EnableMemoryLogging( bool bOnOff );
PLATFORM_INTERFACE void DumpMemoryLog( int nThresh );
PLATFORM_INTERFACE void DumpMemorySummary( void );
PLATFORM_INTERFACE void SetMemoryMark( void );
PLATFORM_INTERFACE void DumpChangedMemory( int nThresh );

#else
FORCEINLINE void MemoryLogMessage( char const * )
{
}

FORCEINLINE void EnableMemoryLogging( bool )
{
}
FORCEINLINE void DumpMemoryLog( int )
{
}
FORCEINLINE void DumpMemorySummary()
{
}
FORCEINLINE void SetMemoryMark()
{
}
FORCEINLINE void DumpChangedMemory( int )
{
}

#endif

#ifdef POSIX
// ApproximateProcessMemoryUsage returns the approximate memory footprint of this process.
PLATFORM_INTERFACE size_t ApproximateProcessMemoryUsage();
#else
FORCEINLINE size_t ApproximateProcessMemoryUsage()
{
	return 0;
}

#endif

struct aligned_tmp_t
{
	// empty base class
};

/*
This class used to be required if you wanted an object to be allocated with a specific
alignment. ALIGN16 and ALIGN16_POST are not actually sufficient for this because they
guarantee that the globals, statics, locals, and function parameters are appropriately
aligned they do not affect memory allocation alignment.
However this class is usually not needed because as of 2012 our policy is that our
allocator should take care of this automatically. Any object whose size is a multiple
of 16 will be 16-byte aligned. Existing uses of this class were not changed because
the cost/benefit did not justify it.
*/
// template here to allow adding alignment at levels of hierarchy that aren't the base
template<size_t bytesAlignment = 16, typename T = aligned_tmp_t>
class CAlignedNewDelete : public T
{

public:
	/*
	Note that this class does not overload operator new[] and delete[] which means that
	classes that depend on this for alignment may end up misaligned if an array is
	allocated.  This problem is now mostly theoretical because this class is mostly
	obsolete.
	*/
	void *operator new( size_t nSize )
	{
		return MemAlloc_AllocAligned( nSize, bytesAlignment );
	}

	void* operator new( size_t nSize, int, const char *pFileName, int nLine )
	{
		return MemAlloc_AllocAlignedFileLine( nSize, bytesAlignment, pFileName, nLine );
	}

	void operator delete( void *pData )
	{
		MemAlloc_FreeAligned( pData );
	}

	void operator delete( void* pData, int, const char *pFileName, int nLine )
	{
		MemAlloc_FreeAligned( pData, pFileName, nLine );
	}

	// These ain't gonna work
	void* operator new[]( size_t nSize )
	{
		return MemAlloc_AllocAligned( nSize, bytesAlignment );
	}

	void operator delete[]( void* pData )
	{
		MemAlloc_FreeAligned( pData );
	}
};


#endif  // !SE_PUBLIC_TIER0_MEMALLOC_H_