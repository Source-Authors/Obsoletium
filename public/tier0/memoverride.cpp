//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Insert this file into all projects using the memory system
// It will cause that project to use the shader memory allocator
//
// $NoKeywords: $
//=============================================================================//

// dimhotepus: ASAN doesn't support default alloc functions replacement.
#if !defined(__SANITIZE_ADDRESS__) && !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

#undef PROTECTED_THINGS_ENABLE   // allow use of _vsnprintf

#if defined( _WIN32 )
#include "winlite.h"
#endif

#ifdef _WIN32
// ARG: crtdbg is necessary for certain definitions below,
// but it also redefines malloc as a macro in release.
// To disable this, we gotta define _DEBUG before including it.. BLEAH!
#define _DEBUG 1
#include <crtdbg.h>
#ifdef NDEBUG
#undef _DEBUG
#endif
// Turn this back off in release mode.
#ifdef NDEBUG
#undef _DEBUG
#endif
#endif

#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include "tier0/threadtools.h"

#include <cstring>
#include <cstdio>

#include "memdbgoff.h"


#if defined(POSIX)
#define __cdecl
#endif

#if defined( _WIN32 )
const char *MakeModuleFileName()
{
	if ( g_pMemAlloc && g_pMemAlloc->IsDebugHeap() )
	{
		char *pszModuleName = (char *)HeapAlloc( GetProcessHeap(), 0, MAX_PATH ); // small leak, debug only
		if (pszModuleName)
		{
			MEMORY_BASIC_INFORMATION mbi;
			static int dummy;
			if ( VirtualQuery( &dummy, &mbi, sizeof(mbi) ) )
			{
				GetModuleFileName( static_cast<HMODULE>(mbi.AllocationBase), pszModuleName, MAX_PATH );
				char *pDot = strrchr( pszModuleName, '.' );
				if ( pDot )
				{
					char *pSlash = strrchr( pszModuleName, '\\' );
					if ( pSlash )
					{
						pszModuleName = pSlash + 1;
						*pDot = '\0';
					}
				}

				return pszModuleName;
			}
		}
	}
	return nullptr;
}

namespace {
// helper class to detect when static construction has been done by the
// CRT
class CStaticConstructionCheck {
 public:
  std::atomic_bool m_bConstructed = true;
};

static CStaticConstructionCheck s_CheckStaticsConstructed;

}  // namespace

const char *GetModuleFileName()
{
#if !defined(_MSC_VER) || ( _MSC_VER >= 1900 ) //  VC 2015 and above, with the UCRT, will crash if you use a static before it is constructed
	if ( !s_CheckStaticsConstructed.m_bConstructed )
		return nullptr;
#endif

	static const char *pszOwner = MakeModuleFileName();
	return pszOwner;
}


static void *AllocUnattributed( size_t nSize )
{
	const char *pszOwner = GetModuleFileName();

	if ( !pszOwner )
		return g_pMemAlloc->Alloc(nSize);
	else
		return g_pMemAlloc->Alloc(nSize, pszOwner, 0);
}

static void *ReallocUnattributed( void *pMem, size_t nSize )
{
	const char *pszOwner = GetModuleFileName();

	if ( !pszOwner )
		return g_pMemAlloc->Realloc(pMem, nSize);
	else
		return g_pMemAlloc->Realloc(pMem, nSize, pszOwner, 0);
}

#else
#define MakeModuleFileName() nullptr
inline void *AllocUnattributed( size_t nSize )
{
	return g_pMemAlloc->Alloc(nSize);
}

inline void *ReallocUnattributed( void *pMem, size_t nSize )
{
	return g_pMemAlloc->Realloc(pMem, nSize);
}
#endif

//-----------------------------------------------------------------------------
// Standard functions in the CRT that we're going to override to call our allocator
//-----------------------------------------------------------------------------
#if defined(_WIN32) && !defined(_STATIC_LINKED)
// this magic only works under win32
// under linux this malloc() overrides the libc malloc() and so we
// end up in a recursion (as g_pMemAlloc->Alloc() calls malloc)
#if _MSC_VER >= 1900
#define SUPPRESS_INVALID_PARAMETER_NO_INFO
#define ALLOC_CALL  __declspec(restrict) __declspec(allocator)
#define FREE_CALL 
#else
#define ALLOC_CALL
#define FREE_CALL
#endif

extern "C"
{

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(size)
ALLOC_CALL void *malloc
(
	_In_ size_t size
)
{
	return AllocUnattributed( size );
}

FREE_CALL void free
(
	_Pre_maybenull_ _Post_invalid_ void *pMem
)
{
	g_pMemAlloc->Free(pMem);
}

_Success_(return != 0) _Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(nSize)
ALLOC_CALL void *realloc
(
	_Pre_maybenull_ _Post_invalid_ void *pMem,
	_In_ size_t nSize
)
{
	return ReallocUnattributed( pMem, nSize );
}

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(nCount * nElementSize)
ALLOC_CALL void *calloc
(
	 _In_ size_t nCount,
	 _In_ size_t nElementSize
)
{
	void *pMem = AllocUnattributed( nElementSize * nCount );
	if ( pMem )
	{
		memset( pMem, 0, nElementSize * nCount );
	}
	return pMem;
}

} // end extern "C"

//-----------------------------------------------------------------------------
// Non-standard MSVC functions that we're going to override to call our allocator
//-----------------------------------------------------------------------------
extern "C"
{

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(nSize)
ALLOC_CALL void *_malloc_base
(
	_In_ size_t nSize
)
{
	return AllocUnattributed( nSize );
}

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(nCount * nSize)
ALLOC_CALL void *_calloc_base
(
	_In_ size_t nCount,
	_In_ size_t nSize
)
{
	void *pMem = AllocUnattributed( nSize*nCount );
	if ( pMem )
	{
		memset( pMem, 0, nSize*nCount );
	}
	return pMem;
}

_Success_(return != 0) _Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(nSize)
ALLOC_CALL void *_realloc_base //-V524
(
	_Pre_maybenull_ _Post_invalid_ void *pMem,
	_In_ size_t nSize
)
{
	return ReallocUnattributed( pMem, nSize );
}

_Success_(return != 0) _Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(nCount * nSize)
ALLOC_CALL void *_recalloc_base
(
	_Pre_maybenull_ _Post_invalid_ void *pMem,
	_In_ size_t nCount,
	_In_ size_t nSize
)
{
	void *pMemOut = ReallocUnattributed( pMem, nCount * nSize );
	if ( !pMem )
	{
		memset( pMemOut, 0, nCount * nSize );
	}
	return pMemOut;
}

void _free_base(
	_Pre_maybenull_ _Post_invalid_ void *pMem
)
{
	g_pMemAlloc->Free(pMem);
}

// crt
void * __cdecl _malloc_crt(size_t size) //-V524
{
	return AllocUnattributed( size );
}

void * __cdecl _calloc_crt(size_t count, size_t size)
{
	return _calloc_base( count, size );
}

void * __cdecl _realloc_crt(void *ptr, size_t size)
{
	return _realloc_base( ptr, size );
}

void * __cdecl _recalloc_crt(void *ptr, size_t count, size_t size)
{
	return _recalloc_base( ptr, count, size );
}

_Success_(return != 0) _Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(count * size)
ALLOC_CALL void * __cdecl _recalloc(
	_Pre_maybenull_ _Post_invalid_ void * memblock,
	_In_ size_t count,
	_In_ size_t size )
{
	return _recalloc_base( memblock, count, size );
}

_Check_return_
size_t _msize_base
(
	_Pre_notnull_ void *pMem
) noexcept
{
	return g_pMemAlloc->GetSize(pMem);
}

_Check_return_
size_t _msize(
	_Pre_notnull_ void *pMem
)
{
	return _msize_base(pMem);
}

size_t msize( void *pMem ) //-V524
{
	return _msize_base(pMem);
}

void *__cdecl _heap_alloc( size_t nSize )
{
	return AllocUnattributed( nSize );
}

void *__cdecl _nh_malloc( size_t nSize, int )
{
	return AllocUnattributed( nSize );
}

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(size)
void *__cdecl _expand
(
	_Pre_notnull_ void *,
	[[maybe_unused]] _In_ size_t size
)
{
	Assert( 0 );
	return nullptr;
}

unsigned int _amblksiz = 16; //BYTES_PER_PARA;

#if _MSC_VER >= 1400
HANDLE _crtheap = (HANDLE)1;	// PatM Can't be 0 or CRT pukes
int __active_heap = 1;
#endif //  _MSC_VER >= 1400

size_t __cdecl _get_sbh_threshold()
{
	return 0;
}

int __cdecl _set_sbh_threshold( size_t )
{
	return 0;
}

_Check_return_ int _heapchk()
{
	return g_pMemAlloc->heapchk();
}

_Check_return_ int _heapmin()
{
	return 1;
}

int __cdecl _heapadd( void *, size_t )
{
	return 0;
}

int __cdecl _heapset( unsigned int )
{
	return 0;
}

size_t __cdecl _heapused( size_t *, size_t * )
{
	return 0;
}

#ifdef _WIN32
int __cdecl _heapwalk( _Inout_ _HEAPINFO * )
{
	return 0;
}
#endif

} // end extern "C"


//-----------------------------------------------------------------------------
// Debugging functions that we're going to override to call our allocator
// NOTE: These have to be here for release + debug builds in case we
// link to a debug static lib!!!
//-----------------------------------------------------------------------------

extern "C"
{
	
void *malloc_db( size_t nSize, const char *pFileName, int nLine ) //-V524
{
	return g_pMemAlloc->Alloc(nSize, pFileName, nLine);
}

void free_db( void *pMem, const char *pFileName, int nLine )
{
	g_pMemAlloc->Free(pMem, pFileName, nLine);
}

void *realloc_db( void *pMem, size_t nSize, const char *pFileName, int nLine )
{
	return g_pMemAlloc->Realloc(pMem, nSize, pFileName, nLine);
}
	
} // end extern "C"

//-----------------------------------------------------------------------------
// These methods are standard MSVC heap initialization + shutdown methods
//-----------------------------------------------------------------------------
extern "C"
{
	int __cdecl _heap_init()
	{
		return g_pMemAlloc != nullptr;
	}

	void __cdecl _heap_term()
	{
	}
}
#endif


//-----------------------------------------------------------------------------
// Prevents us from using an inappropriate new or delete method,
// ensures they are here even when linking against debug or release static libs
//-----------------------------------------------------------------------------
#ifndef NO_MEMOVERRIDE_NEW_DELETE
#ifdef OSX
void *__cdecl operator new( size_t nSize ) throw (std::bad_alloc)
#else
[[nodiscard]] _Ret_notnull_ _Post_writable_byte_size_(nSize) __declspec(allocator)
void *__cdecl operator new( size_t nSize )
#endif
{
	return AllocUnattributed( nSize );
}

[[nodiscard]] _Ret_maybenull_ _Success_(return != NULL)
_Post_writable_byte_size_(size) __declspec(allocator)
void *__cdecl operator new( size_t size, ::std::nothrow_t const & ) noexcept
{
	return AllocUnattributed( size );
}

#ifdef OSX
void *__cdecl operator new[]( size_t nSize ) throw (std::bad_alloc)
#else
[[nodiscard]] _Ret_notnull_ _Post_writable_byte_size_(nSize) __declspec(allocator)
void *__cdecl operator new[]( size_t nSize )
#endif
{
	return AllocUnattributed( nSize );
}

[[nodiscard]] _Ret_maybenull_ _Success_(return != NULL) _Post_writable_byte_size_(nSize) __declspec(allocator)
void *__cdecl operator new[]( size_t nSize, std::nothrow_t const& ) noexcept
{
	return AllocUnattributed( nSize );
}

#ifdef __cpp_aligned_new

[[nodiscard]] _Ret_notnull_ _Post_writable_byte_size_(nSize) __declspec(allocator)
void* __cdecl operator new( std::size_t nSize, std::align_val_t align )
{
	return MemAlloc_AllocAligned( nSize, static_cast<size_t>(align) );
}

[[nodiscard]] _Ret_maybenull_ _Success_(return != NULL) _Post_writable_byte_size_(nSize) __declspec(allocator)
void* __cdecl operator new( std::size_t nSize, std::align_val_t align, std::nothrow_t const& ) noexcept
{
	return MemAlloc_AllocAligned( nSize, static_cast<size_t>(align) );
}

[[nodiscard]] _Ret_notnull_ _Post_writable_byte_size_(nSize) __declspec(allocator)
void* __cdecl operator new[]( std::size_t nSize, std::align_val_t align )
{
	return MemAlloc_AllocAligned( nSize, static_cast<size_t>(align) );
}

[[nodiscard]] _Ret_maybenull_ _Success_(return != NULL) _Post_writable_byte_size_(nSize) __declspec(allocator)
void* __cdecl operator new[]( std::size_t nSize, std::align_val_t align, std::nothrow_t const& ) noexcept
{
	return MemAlloc_AllocAligned( nSize, static_cast<size_t>(align) );
}

void __cdecl operator delete( void* pMem, std::align_val_t align ) noexcept
{
#ifdef _WIN32
	// dimhotepus: Windows allocator has 16 bytes alignment by default, so use default free.
	if ( static_cast<size_t>(align) == 16 )
	{
		g_pMemAlloc->Free( pMem );
		return;
	}
#endif

	MemAlloc_FreeAligned( pMem );
}

void __cdecl operator delete( void* pMem, std::align_val_t align, std::nothrow_t const& ) noexcept
{
#ifdef _WIN32
	// dimhotepus: Windows allocator has 16 bytes alignment by default, so use default free.
	if ( static_cast<size_t>(align) == 16 )
	{
		g_pMemAlloc->Free( pMem );
		return;
	}
#endif

	MemAlloc_FreeAligned( pMem );
}

void __cdecl operator delete( void *ptr, [[maybe_unused]] size_t size, std::align_val_t align) noexcept
{
#ifdef _WIN32
	// dimhotepus: Windows allocator has 16 bytes alignment by default, so use default free.
	if ( static_cast<size_t>(align) == 16 )
	{
		g_pMemAlloc->Free( ptr );
		return;
	}
#endif

	MemAlloc_FreeAligned(ptr);
}

void __cdecl operator delete[]( void* ptr, std::align_val_t align ) noexcept
{
#ifdef _WIN32
	// dimhotepus: Windows allocator has 16 bytes alignment by default, so use default free.
	if ( static_cast<size_t>(align) == 16 )
	{
		g_pMemAlloc->Free( ptr );
		return;
	}
#endif

	MemAlloc_FreeAligned(ptr);
}

void __cdecl operator delete[]( void* ptr, std::align_val_t align, std::nothrow_t const& ) noexcept
{
#ifdef _WIN32
	// dimhotepus: Windows allocator has 16 bytes alignment by default, so use default free.
	if ( static_cast<size_t>(align) == 16 )
	{
		g_pMemAlloc->Free( ptr );
		return;
	}
#endif

	MemAlloc_FreeAligned(ptr);
}

void __cdecl operator delete[]( void *ptr, [[maybe_unused]] size_t size, std::align_val_t align) noexcept
{
#ifdef _WIN32
	// dimhotepus: Windows allocator has 16 bytes alignment by default, so use default free.
	if ( static_cast<size_t>(align) == 16 )
	{
		g_pMemAlloc->Free( ptr );
		return;
	}
#endif

	MemAlloc_FreeAligned(ptr);
}

#endif

[[nodiscard]] _Check_return_ _Ret_notnull_ _Post_writable_byte_size_(nSize) __declspec(allocator)
void *__cdecl operator new
(
	_In_ size_t nSize,
	_In_ int ,
	_In_z_ const char *pFileName,
	_In_ int nLine
)
{
	return g_pMemAlloc->Alloc(nSize, pFileName, nLine);
}

[[nodiscard]] _Check_return_ _Ret_notnull_ _Post_writable_byte_size_(nSize) __declspec(allocator)
void *__cdecl operator new[]
(
	_In_ size_t nSize,
	_In_ int,
	_In_z_ const char *pFileName,
	_In_ int nLine
)
{
	return g_pMemAlloc->Alloc(nSize, pFileName, nLine);
}

#ifdef OSX
void __cdecl operator delete( void *pMem ) throw()
#else
void __cdecl operator delete( void *pMem ) noexcept
#endif
{
	g_pMemAlloc->Free( pMem );
}

void __cdecl operator delete( void *block, ::std::nothrow_t const & ) noexcept
{
	g_pMemAlloc->Free( block );
}

#ifdef OSX
void operator delete( void *pMem, std::size_t )
#else
void operator delete( void *pMem, std::size_t ) noexcept
#endif
{
	g_pMemAlloc->Free( pMem );
}

#ifdef OSX
void __cdecl operator delete[]( void *pMem ) throw()
#else
void __cdecl operator delete[]( void *pMem ) noexcept
#endif
{
	g_pMemAlloc->Free( pMem );
}

#endif


//-----------------------------------------------------------------------------
// Override some debugging allocation methods in MSVC
// NOTE: These have to be here for release + debug builds in case we
// link to a debug static lib!!!
//-----------------------------------------------------------------------------
#ifndef _STATIC_LINKED
#ifdef _WIN32

// This here just hides the internal file names, etc of allocations
// made in the c runtime library
constexpr char CRT_INTERNAL_FILE_NAME[]{"C-runtime internal"};

namespace {

class CAttibCRT {
 public:
  CAttibCRT(int nBlockUse) : m_nBlockUse(nBlockUse) {
    if (m_nBlockUse == _CRT_BLOCK) {
      g_pMemAlloc->PushAllocDbgInfo(CRT_INTERNAL_FILE_NAME, 0);
    }
  }

  ~CAttibCRT() {
    if (m_nBlockUse == _CRT_BLOCK) {
      g_pMemAlloc->PopAllocDbgInfo();
    }
  }

 private:
  int m_nBlockUse;
};

}  // namespace

#define AttribIfCrt() CAttibCRT _attrib(nBlockUse)
#elif defined(POSIX)
#define AttribIfCrt()
#endif // _WIN32


extern "C"
{

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(nSize)
_CRTALLOCATOR void *__cdecl _malloc_dbg
(
	_In_ size_t nSize,
	_In_ int nBlockUse,
	_In_opt_z_ const char *pFileName,
	_In_ int nLine
)
{
	AttribIfCrt();
	return g_pMemAlloc->Alloc(nSize, pFileName, nLine);
}

#if defined( _X360 )
void *__cdecl _calloc_dbg_impl( size_t nNum, size_t nSize, int nBlockUse, 
								const char * szFileName, int nLine, int * errno_tmp )
{
	return _calloc_dbg( nNum, nSize, nBlockUse, szFileName, nLine );
}
#endif

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(nNum * nSize)
_CRTALLOCATOR void *__cdecl _calloc_dbg
(
	_In_ size_t nNum,
	_In_ size_t nSize,
	_In_ int nBlockUse,
	_In_opt_z_ const char *pFileName,
	_In_ int nLine
)
{
	AttribIfCrt();
	void *pMem = g_pMemAlloc->Alloc(nSize * nNum, pFileName, nLine);
	if ( pMem )
	{
		memset( pMem, 0, nSize * nNum );
	}
	return pMem;
}

void *__cdecl _calloc_dbg_impl( size_t nNum, size_t nSize, int nBlockUse, 
	const char * szFileName, int nLine, int *  )
{
	return _calloc_dbg( nNum, nSize, nBlockUse, szFileName, nLine );
}

_Success_(return != 0) _Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(nNewSize)
_CRTALLOCATOR void *__cdecl _realloc_dbg
(
	_Pre_maybenull_ _Post_invalid_ void *pMem,
	_In_ size_t nNewSize,
	_In_ int nBlockUse,
	_In_opt_z_ const char *pFileName,
	_In_ int nLine
)
{
	AttribIfCrt();
	return g_pMemAlloc->Realloc(pMem, nNewSize, pFileName, nLine);
}

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(size)
_CRTALLOCATOR void *__cdecl _expand_dbg
(
	_Pre_notnull_ void *,
	[[maybe_unused]] _In_ size_t size,
	_In_ int ,
	_In_opt_z_ const char *,
	_In_ int
)
{
	Assert( 0 );
	return nullptr;
}

void __cdecl _free_dbg
(
	_Pre_maybenull_ _Post_invalid_ void *pMem,
	_In_ int nBlockUse
)
{
	AttribIfCrt();
	g_pMemAlloc->Free(pMem);
}

size_t __cdecl _msize_dbg( _Pre_notnull_ void *pMem, _In_ int )
{
#ifdef _WIN32
	return _msize(pMem);
#elif POSIX
	AssertMsg( false, "_msize_dbg unsupported" );
	return 0;
#endif
}


#ifdef _WIN32

#if defined(_DEBUG) && _MSC_VER >= 1300
// aligned base
ALLOC_CALL void *__cdecl _aligned_malloc_base( size_t size, size_t align )
{
	return MemAlloc_AllocAligned( size, align );
}

inline void *MemAlloc_Unalign( void *pMemBlock )
{
	unsigned *pAlloc = (unsigned *)pMemBlock;

	// pAlloc points to the pointer to starting of the memory block
	pAlloc = (unsigned *)(((size_t)pAlloc & ~(sizeof( void * ) - 1)) - sizeof( void * ));

	// pAlloc is the pointer to the start of memory block
	return *((unsigned **)pAlloc);
}

ALLOC_CALL void *__cdecl _aligned_realloc_base( void *ptr, size_t size, size_t align )
{
	if ( ptr && !size )
	{
		MemAlloc_FreeAligned( ptr );
		return nullptr;
	}

	void *pNew = MemAlloc_AllocAligned( size, align );
	if ( ptr )
	{
		void *ptrUnaligned = MemAlloc_Unalign( ptr );
		size_t oldSize = g_pMemAlloc->GetSize( ptrUnaligned );
		size_t oldOffset = (uintp)ptr - (uintp)ptrUnaligned;
		size_t copySize = oldSize - oldOffset;
		if ( copySize > size )
			copySize = size;
		memcpy( pNew, ptr, copySize );
		MemAlloc_FreeAligned( ptr );
	}
	return pNew;
}

ALLOC_CALL void *__cdecl _aligned_recalloc_base( void *, size_t , size_t  )
{
	Error( "Unsupported function\n" );
}

FREE_CALL void __cdecl _aligned_free_base( void *ptr )
{
	MemAlloc_FreeAligned( ptr );
}

// aligned
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(size)
ALLOC_CALL void * __cdecl _aligned_malloc
(
	_In_ size_t size,
	_In_ size_t align
)
{
	return _aligned_malloc_base(size, align);
}

_Check_return_ size_t __cdecl _aligned_msize
(
  _Pre_notnull_ void *block,
  _In_ size_t,
  _In_ size_t
) 
{
	return g_pMemAlloc->GetSize(block);
}

_Success_(return != 0) _Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(size)
ALLOC_CALL void *__cdecl _aligned_realloc
(
	_Pre_maybenull_ _Post_invalid_ void *memblock,
	_In_ size_t size,
	_In_ size_t align
)
{
    return _aligned_realloc_base(memblock, size, align);
}

_Success_(return != 0) _Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(count * size)
ALLOC_CALL void *__cdecl _aligned_recalloc
(
    _Pre_maybenull_ _Post_invalid_ void *memblock,
	_In_ size_t count,
    _In_ size_t size,
	_In_ size_t align
)
{
    return _aligned_recalloc_base(memblock, count * size, align);
}

FREE_CALL void __cdecl _aligned_free
(
    _Pre_maybenull_ _Post_invalid_ void *memblock
)
{
    _aligned_free_base(memblock);
}

// aligned offset base
ALLOC_CALL void * __cdecl _aligned_offset_malloc_base( size_t , size_t , size_t  )
{
	Assert( IsPC() || 0 );
	return nullptr;
}

ALLOC_CALL void * __cdecl _aligned_offset_realloc_base( void * , size_t , size_t , size_t )
{
	Assert( IsPC() || 0 );
	return nullptr;
}

ALLOC_CALL void * __cdecl _aligned_offset_recalloc_base( void * memblock, size_t size, size_t , size_t )
{
	Assert( IsPC() || 0 );
	void *pMem = ReallocUnattributed( memblock, size );
	if ( !memblock )
	{
		memset( pMem, 0, size );
	}
	return pMem;
}

// aligned offset
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(size)
ALLOC_CALL void *__cdecl _aligned_offset_malloc
(
	_In_ size_t size,
	_In_ size_t align,
	_In_ size_t offset
)
{
    return _aligned_offset_malloc_base( size, align, offset );
}

_Success_(return != 0) _Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(size)
ALLOC_CALL void *__cdecl _aligned_offset_realloc
(
	_Pre_maybenull_ _Post_invalid_ void *memblock,
	_In_ size_t size,
	_In_ size_t align,
	_In_ size_t offset
)
{
    return _aligned_offset_realloc_base( memblock, size, align, offset );
}

_Success_(return != 0) _Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(count * size)
ALLOC_CALL void * __cdecl _aligned_offset_recalloc
(
	_Pre_maybenull_ _Post_invalid_ void * memblock,
	_In_ size_t count,
	_In_ size_t size,
	_In_ size_t align,
	_In_ size_t offset
)
{
    return _aligned_offset_recalloc_base( memblock, count * size, align, offset );
}

#endif // _MSC_VER >= 1400

#endif

} // end extern "C"


//-----------------------------------------------------------------------------
// Override some the _CRT debugging allocation methods in MSVC
//-----------------------------------------------------------------------------
#ifdef _WIN32

extern "C"
{

// 64-bit port.
#define AFNAME(var) __p_##var
#define AFRET(var)  &var
#if defined(_crtDbgFlag)
#undef _crtDbgFlag
#endif
#if defined(_crtBreakAlloc)
#undef _crtBreakAlloc
#endif

int _crtDbgFlag = _CRTDBG_ALLOC_MEM_DF;
int* AFNAME(_crtDbgFlag)(void)
{
	return AFRET(_crtDbgFlag);
}

long _crtBreakAlloc;      /* Break on this allocation */
long* AFNAME(_crtBreakAlloc) (void)
{
	return AFRET(_crtBreakAlloc);
}

//-----------------------------------------------------------------------------
// Methods in dbgrpt.cpp 
//-----------------------------------------------------------------------------
#if _MSC_VER >= 1400

// Configure VS so that it will record crash dumps on pure-call violations
// and invalid parameter handlers.
// If you manage to call a pure-virtual function (easily done if you indirectly
// call a pure-virtual function from the base-class constructor or destructor)
// or if you invoke the invalid parameter handler (printf(nullptr); is one way)
// then no crash dump will be created.
// This crash redirects the handlers for these two events so that crash dumps
// are created.
//
// The ErrorHandlerRegistrar object must be in memoverride.cpp so that it will
// be placed in every DLL and EXE. This is required because each DLL and EXE
// gets its own copy of the C run-time and these overrides are set on a per-CRT
// basis.

#include <cstdlib>
#include "minidump.h"

// Disable compiler optimizations. If we don't do this then VC++ generates code
// that confuses the Visual Studio debugger and causes it to display completely
// random call stacks. That makes the minidumps excruciatingly hard to understand.
#pragma optimize("", off)

// Write a minidump file, unless running under the debugger in which case break
// into the debugger.
// The "int dummy" parameter is so that the callers can be unique so that the
// linker won't use its /opt:icf optimization to collapse them together. This
// makes reading the call stack easier.
void __cdecl WriteMiniDumpOrBreak( int , const char *pchName )
{
	if ( Plat_IsInDebugSession() )
	{
		__debugbreak();
		// Continue at your peril...
	}
	else
	{
		WriteMiniDump( pchName );
		// Call Plat_ExitProcess so we don't continue in a bad state. 
		TerminateProcess(GetCurrentProcess(), 1);
	}
}

void __cdecl VPureCall()
{
	WriteMiniDumpOrBreak( 0, "PureClass" );
}

// dimhotepus: wchar_t -> mbcs.
char *Wide2Multibyte( const wchar_t* wide )
{
	if ( wide != nullptr )
	{
		const size_t expected_count{ wcstombs( nullptr, wide, 0 ) };
		char *mb{ new char[expected_count] };
		const size_t actual_count{ wcstombs( mb, wide, expected_count ) };

		Assert(expected_count == actual_count);

		return mb;
	}

	return new char[4]{"NA"};
}

void VInvalidParameterHandler( const wchar_t* expression,
   const wchar_t* function, 
   const wchar_t* file, 
   unsigned int line, 
   [[maybe_unused]] uintptr_t pReserved)
{
	char *mb_expression{ Wide2Multibyte( expression ) };
	char *mb_function{ Wide2Multibyte( function ) };
	char *mb_file{ Wide2Multibyte( file ) };

	char buffer[1024];
	snprintf( buffer,
		std::size(buffer),
		"%s(%u)_%s_%s_InvalidParameterHandler",
		mb_file,
		line,
		mb_function,
		mb_expression );

	// dimhtepus: Better prefix for invalid parameter handler.
	WriteMiniDumpOrBreak( 1, buffer );

	delete[] mb_file;
	delete[] mb_function;
	delete[] mb_expression;
}

// Restore compiler optimizations.
#pragma optimize("", on)

namespace {

// Helper class for registering error callbacks. See above for details.
class ErrorHandlerRegistrar {
 public:
  ErrorHandlerRegistrar() noexcept
      : old_pure_{_set_purecall_handler(VPureCall)},
        old_invalid_{_set_invalid_parameter_handler(VInvalidParameterHandler)} {
  }
  // dimhotepus: Restore old handlers on exit.
  ~ErrorHandlerRegistrar() noexcept {
    _set_invalid_parameter_handler(old_invalid_);
    _set_purecall_handler(old_pure_);
  }

 private:
  _purecall_handler old_pure_;
  _invalid_parameter_handler old_invalid_;
} s_ErrorHandlerRegistration;

}  // namespace

#if defined( _DEBUG )
 
// wrapper which passes no debug info; not available in debug
#ifndef	SUPPRESS_INVALID_PARAMETER_NO_INFO
void __cdecl _invalid_parameter_noinfo()
{
    Assert(0);
}
#endif

#endif /* defined( _DEBUG ) */

#if defined( _DEBUG ) || defined( USE_MEM_DEBUG )

int __cdecl __crtMessageWindowW( int , const wchar_t * , const wchar_t * ,
								 const wchar_t * , const wchar_t *  )
{
	Assert(0);
	return 0;
}

int __cdecl _CrtDbgReportV( int , const wchar_t *, int , 
						    const wchar_t *, const wchar_t *, va_list  )
{
	Assert(0);
	return 0;
}

#endif  /* defined( _DEBUG ) || defined( USE_MEM_DEBUG ) */

_Success_(return != 0) _Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(count * size)
extern "C" __declspec(allocator) void * __cdecl _recalloc_dbg
(
	_Pre_maybenull_ _Post_invalid_ void * memblock,
	_In_                           size_t count,
	_In_                           size_t size,
	_In_                           int ,
	_In_opt_z_                     const char * szFileName,
	_In_                           int nLine
)
{
	return _aligned_offset_recalloc_dbg(memblock, count, size, 0, 0, szFileName, nLine);
}

#endif

} // end extern "C"
#endif // _WIN32

// Most files include this file, so when it's used it adds an extra .ValveDbg section,
// to help identify debug binaries.
#ifdef _WIN32
	#ifndef NDEBUG // _DEBUG
		#pragma data_seg("ValveDBG") 
		volatile const char* DBG = "*** DEBUG STUB ***";
	#endif
#endif

#endif

// Extras added prevent dbgheap.obj from being included - DAL
#ifdef _WIN32

extern "C"
{
void * __cdecl _heap_alloc_base(size_t)
{
	Assert(0);
	return nullptr;
}

void * __cdecl _heap_alloc_dbg( size_t nSize, int , const char * , int )
{
	return _heap_alloc(nSize);
}

void __cdecl _free_nolock( void * pUserData)
{
	// I don't think the second param is used in memoverride
	_free_dbg(pUserData, 0);
}

void __cdecl _free_dbg_nolock( void * pUserData, int )
{
	_free_dbg(pUserData, 0);
}

// crtdbg.h

_CRT_ALLOC_HOOK __cdecl _CrtGetAllocHook()
{
	Assert(0); 
	return nullptr;
}
_CRT_ALLOC_HOOK __cdecl _CrtSetAllocHook( _In_opt_ _CRT_ALLOC_HOOK  )
{
	DebuggerBreak();
	return nullptr;
}

_CRT_DUMP_CLIENT __cdecl _CrtGetDumpClient()
{
	Assert(0); 
	return nullptr;
}
_CRT_DUMP_CLIENT __cdecl _CrtSetDumpClient( _In_opt_ _CRT_DUMP_CLIENT )
{
	return nullptr;
}

int __cdecl _CrtCheckMemory()
{
	// FIXME: Remove this when we re-implement the heap
	return g_pMemAlloc->CrtCheckMemory( );
}

void __cdecl _CrtDoForAllClientObjects
(
	[[maybe_unused]] _In_ _CrtDoForAllClientObjectsCallback callback,
	[[maybe_unused]] _In_ void* context
)
{
	Assert(0);
}

int _CrtDumpMemoryLeaks()
{
	return 0;
}

int __cdecl _CrtIsMemoryBlock
(
	_In_opt_  const void *,
	_In_      unsigned int ,
	_Out_opt_ long *,
	_Out_opt_ char **,
	_Out_opt_ int *
)
{
	DebuggerBreak();
	return 1;
}

_Check_return_
int __cdecl _CrtIsValidHeapPointer( _In_opt_ const void *pMem )
{
	return g_pMemAlloc->CrtIsValidHeapPointer( pMem );
}

_Check_return_
int __cdecl _CrtIsValidPointer
(
	_In_opt_ const void *pMem,
	_In_ unsigned int size,
	_In_ int access
)
{
	return g_pMemAlloc->CrtIsValidPointer( pMem, size, access );
}

void __cdecl _CrtMemCheckpoint( _Out_ _CrtMemState *pState )
{
	// FIXME: Remove this when we re-implement the heap
	g_pMemAlloc->CrtMemCheckpoint( pState );
}

int __cdecl _CrtMemDifference
(
	_Out_ _CrtMemState *,
	_In_  const _CrtMemState * ,
	_In_  const _CrtMemState *
)
{
	DebuggerBreak();
	return FALSE;
}

void __cdecl _CrtMemDumpAllObjectsSince( _In_opt_ const _CrtMemState * )
{
	DebuggerBreak();
}

void __cdecl _CrtMemDumpStatistics( _In_ const _CrtMemState * )
{
	DebuggerBreak();
}

_Check_return_
int __cdecl _CrtReportBlockType( _In_opt_ const void * )
{
	return 0;
}

long __cdecl _CrtSetBreakAlloc( _In_ long lNewBreakAlloc )
{
	return g_pMemAlloc->CrtSetBreakAlloc( lNewBreakAlloc );
}

int __cdecl _CrtSetDbgFlag( _In_ int nNewFlag )
{
	return g_pMemAlloc->CrtSetDbgFlag( nNewFlag );
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// Debug Reporting
//
//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

int __cdecl _CrtDbgReport
(
	_In_       int nRptType,
	_In_opt_z_ const char * szFile,
	_In_       int nLine,
	_In_opt_z_ const char * szModule,
	_In_opt_z_ const char * szFormat,
	...
)
{
	static char output[1024];
	va_list args;
	if ( szFormat )
	{
		va_start( args, szFormat );
		_vsnprintf( output, sizeof( output )-1, szFormat, args );
		va_end( args );
	}
	else
	{
		output[0] = 0;
	}

	return g_pMemAlloc->CrtDbgReport( nRptType, szFile, nLine, szModule, output );
}

int __cdecl _CrtDbgReportW
(
	_In_       int ,
	_In_opt_z_ const wchar_t *,
	_In_       int ,
	_In_opt_z_ const wchar_t *,
	_In_opt_z_ const wchar_t *,
	...
)
{
	Assert(0);
	return 0;
}

int __cdecl _VCrtDbgReportA
(
	_In_       int ,
	_In_opt_   void* ,
	_In_opt_z_ const char * ,
	_In_       int ,
	_In_opt_z_ const char * ,
	_In_opt_z_ const char * ,
	va_list
)
{
	Assert(0);
	return 0;
}

int __cdecl _VCrtDbgReportW
(
	[[maybe_unused]] _In_       int            ReportType,
	[[maybe_unused]] _In_opt_   void*          ReturnAddress,
	[[maybe_unused]] _In_opt_z_ wchar_t const* FileName,
	[[maybe_unused]] _In_       int            LineNumber,
	[[maybe_unused]] _In_opt_z_ wchar_t const* ModuleName,
	[[maybe_unused]] _In_opt_z_ wchar_t const* Format,
	[[maybe_unused]] va_list                   ArgList
)
{
	Assert(0);
	return 0;
}

_HFILE __cdecl _CrtSetReportFile
(
	_In_     int nRptType,
	_In_opt_ _HFILE hFile
)
{
	return static_cast<_HFILE>( g_pMemAlloc->CrtSetReportFile( nRptType, hFile ) );
}

int __cdecl _CrtSetReportMode
(
	_In_ int nReportType,
	_In_ int nReportMode
)
{
	return g_pMemAlloc->CrtSetReportMode( nReportType, nReportMode );
}

long _crtAssertBusy = -1;

_CRT_REPORT_HOOK __cdecl _CrtGetReportHook()
{
	return nullptr;
}

_CRT_REPORT_HOOK __cdecl _CrtSetReportHook
(
	_In_opt_ _CRT_REPORT_HOOK pfnNewHook
)
{
	return reinterpret_cast<_CRT_REPORT_HOOK>( g_pMemAlloc->CrtSetReportHook( reinterpret_cast<void*>(pfnNewHook) ) );
}

int __cdecl _CrtSetReportHook2
(
	_In_ int,
	_In_opt_ _CRT_REPORT_HOOK pfnNewHook
)
{
	_CrtSetReportHook( pfnNewHook );
	return 0;
}

int __cdecl _CrtSetReportHookW2
(
	[[maybe_unused]] _In_     int               Mode,
    [[maybe_unused]] _In_opt_ _CRT_REPORT_HOOKW PFnNewHook
)
{
	Assert(0);
	return 0;
}

// Heap debug routines.

void __cdecl _aligned_free_dbg
(
  _Pre_maybenull_ _Post_invalid_ void * block
)
{
    _aligned_free(block);
}

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(size)
__declspec(allocator) void * __cdecl _aligned_malloc_dbg
(
  _In_ size_t size,
  _In_ size_t align,
  _In_opt_z_ const char * ,
  _In_ int
)
{
    return _aligned_malloc(size, align);
}

size_t __cdecl _aligned_msize_dbg
(
  _Pre_notnull_ void* block,
  _In_ size_t align,
  _In_ size_t offset
)
{
    return _aligned_msize(block, align, offset);
}

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(size)
__declspec(allocator) void *__cdecl _aligned_offset_malloc_dbg
(
  _In_ size_t size,
  _In_ size_t align,
  _In_ size_t offset,
  _In_opt_z_ const char *,
  _In_ int)
{
    return _aligned_offset_malloc(size, align, offset);
}

_Success_(return != 0) _Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(size)
__declspec(allocator) void * __cdecl _aligned_offset_realloc_dbg
(
  _Pre_maybenull_ _Post_invalid_ void * memblock,
  _In_ size_t size,
  _In_ size_t align,
  _In_ size_t offset,
  _In_opt_z_ const char * ,
  _In_ int
)
{
    return _aligned_offset_realloc(memblock, size, align, offset);
}

_Success_(return != 0) _Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(count * size)
__declspec(allocator) void* __cdecl _aligned_offset_recalloc_dbg
(
  _Pre_maybenull_ _Post_invalid_ void* block,
  _In_ size_t count,
  _In_ size_t size,
  _In_ size_t align,
  _In_ size_t offset,
  _In_opt_z_ char const*,
  _In_ int
)
{
    return _aligned_offset_recalloc(block, count, size, align, offset);
}

_Success_(return != 0) _Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(size)
__declspec(allocator) void * __cdecl _aligned_realloc_dbg
(
  _Pre_maybenull_ _Post_invalid_ void *memblock,
  _In_ size_t size,
  _In_ size_t align,
  _In_opt_z_ const char * ,
  _In_ int
)
{
    return _aligned_realloc(memblock, size, align);
}

_Success_(return != 0) _Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(count * size)
void *__cdecl _aligned_recalloc_dbg
(
  _Pre_maybenull_ _Post_invalid_ void *memblock,
  _In_ size_t count,
  _In_ size_t size,
  _In_ size_t align,
  [[maybe_unused]] _In_opt_z_ const char *f_name,
  [[maybe_unused]] _In_ int line_n
)
{
    return _aligned_recalloc(memblock, count, size, align);
}

} // end extern "C"

#endif // !STEAM && !NO_MALLOC_OVERRIDE

#endif // _WIN32
