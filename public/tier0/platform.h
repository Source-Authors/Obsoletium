// Copyright Valve Corporation, All rights reserved.

#ifndef SE_PUBLIC_TIER0_PLATFORM_H_
#define SE_PUBLIC_TIER0_PLATFORM_H_

#define __STDC_LIMIT_MACROS
#include <cstdint>

#include "wchartypes.h"
#include "basetypes.h"
#include "valve_off.h"

#ifndef PLAT_COMPILE_TIME_ASSERT
#define PLAT_COMPILE_TIME_ASSERT( pred )	static_assert(pred);
#endif

// feature enables
#define NEW_SOFTWARE_LIGHTING

#ifdef POSIX
// need this for _alloca
#include <alloca.h>
#include <unistd.h>
#include <signal.h>
#include <ctime>
#endif

#include <malloc.h>
#include <new>

#include <cstring>  // std::memset
#include <cstddef>  // std::size

#if defined(__GCC__) && (defined(__i386__) || defined(__x86_64__))
#include <x86intrin.h>  // __rdtsc, __rdtscp
#endif

#include "valve_minmax_on.h"	// GCC 4.2.2 headers screw up our min/max defs.

#ifdef _RETAIL
#define IsRetail() true
#else
#define IsRetail() false
#endif

#ifdef _DEBUG
#define IsRelease() false
#define IsDebug() true
#else
#define IsRelease() true
#define IsDebug() false
#endif

// Deprecating, infavor of IsX360() which will revert to IsXbox()
// after confidence of xbox 1 code flush
#define IsXbox()	false

#ifdef _WIN32
	#define IsLinux() false
	#define IsOSX() false
	#define IsPosix() false
	#define PLATFORM_WINDOWS 1 // Windows PC or Xbox 360
		#define IsWindows() true
		#define IsPC() true
		#define IsConsole() false
		#define IsX360() false
		//#define IsPS3() false
		#define IS_WINDOWS_PC
		#define PLATFORM_WINDOWS_PC 1 // Windows PC
		#ifdef _WIN64
			//#define IsPlatformWindowsPC64() true
			//#define IsPlatformWindowsPC32() false
			#define PLATFORM_WINDOWS_PC64 1
		#else
			//#define IsPlatformWindowsPC64() false
			//#define IsPlatformWindowsPC32() true
			#define PLATFORM_WINDOWS_PC32 1
		#endif
	// Adding IsPlatformOpenGL() to help fix a bunch of code that was using IsPosix() to infer if the DX->GL translation layer was being used.
	#if defined( DX_TO_GL_ABSTRACTION )
		#define IsPlatformOpenGL() true
	#else
		#define IsPlatformOpenGL() false
	#endif
#elif defined(POSIX)
	#define IsPC() true
	#define IsWindows() false
	#define IsConsole() false
	#define IsX360() false
	//#define IsPS3() false
	#if defined( LINUX )
		#define IsLinux() true
	#else
		#define IsLinux() false
	#endif
	
	#if defined( OSX )
		#define IsOSX() true
	#else
		#define IsOSX() false
	#endif
	
	#define IsPosix() true
	#define IsPlatformOpenGL() true
#else
	#error "Please define your platform"
#endif

// dimhotepus: TF2 backport.
#if PLATFORM_WINDOWS_PC

# if PLATFORM_64BITS
#  define PLATFORM_DIR "\\x64"
# else
#  define PLATFORM_DIR ""
# endif

//#elif PLATFORM_LINUX
#elif LINUX

# if PLATFORM_64BITS
#  define PLATFORM_DIR "/linux64"
# else
#  define PLATFORM_DIR ""
# endif

//#elif PLATFORM_OSX
#elif OSX

#if PLATFORM_ARM
#  define PLATFORM_DIR "/osxarm64"
#else
# if PLATFORM_64BITS
#  define PLATFORM_DIR "/osx64"
# else
#  define PLATFORM_DIR ""
# endif
#endif

#else
# error "Define a platform dir for me!"
#endif

#define PLATFORM_BIN_DIR "bin" PLATFORM_DIR

#if !defined( _WIN32 )
using HWND = void *;
#endif // !_WIN32

// Avoid redefinition warnings if a previous header defines this.
#undef OVERRIDE
// Use this to specify that a function is an override of a
// virtual function. This lets the compiler catch cases where
// you meant to override a virtual function but you accidentally
// changed the function signature and created an overloaded
// function. Usage in function declarations is like this: int
// GetData() const OVERRIDE;
#define OVERRIDE override

//-----------------------------------------------------------------------------
// Set up platform type defines.
//-----------------------------------------------------------------------------
#define IsPC()			true
#define IsGameConsole()	false

#ifdef PLATFORM_64BITS
	#define IsPlatform64Bits()	true
#else
	#define IsPlatform64Bits()	false
#endif

// From steam/steamtypes.h
// RTime32
// We use this 32 bit time representing real world time.
// It offers 1 second resolution beginning on January 1, 1970 (Unix time)
using RTime32 = uint32;

using float32 = float;
using float64 = double;

// for when we don't care about how many bits we use
using uint = unsigned int;

// This can be used to ensure the size of pointers to members when declaring
// a pointer type for a class that has only been forward declared
#ifdef _MSC_VER
#define SINGLE_INHERITANCE __single_inheritance
#define MULTIPLE_INHERITANCE __multiple_inheritance
#else
#define SINGLE_INHERITANCE
#define MULTIPLE_INHERITANCE
#endif

// This indicates that a function never returns, which helps with
// generating accurate compiler warnings
#define NORETURN				[[noreturn]]

// MSVC CRT uses 0x7fff while gcc uses MAX_INT, leading to mismatches between platforms
// As a result, we pick the least common denominator here.  This should be used anywhere
// you might typically want to use RAND_MAX
#define VALVE_RAND_MAX 0x7fff

// portability / compiler settings
#if defined(_WIN32) && !defined(WINDED)

#if defined(_M_IX86)
#define __i386__	1
#endif

#elif POSIX
#if defined( OSX ) && defined( CARBON_WORKAROUND )
#define DWORD unsigned int
#else
typedef unsigned int DWORD;
#endif
typedef unsigned short WORD;
typedef void * HINSTANCE;
#define MAX_PATH PATH_MAX
#define __cdecl
#define __stdcall
#define __declspec

#endif // defined(_WIN32) && !defined(WINDED)

#define MAX_FILEPATH 512 

// Defines MAX_PATH
#ifndef MAX_PATH
#define MAX_PATH  260
#endif

#ifdef _WIN32
#define MAX_UNICODE_PATH 32767
#else
#define MAX_UNICODE_PATH MAX_PATH
#endif

#define MAX_UNICODE_PATH_IN_UTF8 (MAX_UNICODE_PATH*4)

#if !defined( offsetof )
	#ifdef __GNUC__
		#define offsetof( type, var ) __builtin_offsetof( type, var )
	#else
		#define offsetof(s,m)	(size_t)&(((s *)0)->m)
	#endif
#endif // !defined( offsetof )

// dimhotepus: Deprecated, use AlignValue
#define ALIGN_VALUE( val, alignment ) AlignValue(val, alignment) //  need macro for constant expression

// Used to step into the debugger
#if defined( _WIN32 )
#define DebuggerBreak()  __debugbreak()
#else
	// On OSX, SIGTRAP doesn't really stop the thread cold when debugging.
	// So if being debugged, use INT3 which is precise.
#ifdef OSX
#define DebuggerBreak()  if ( Plat_IsInDebugSession() ) { __asm ( "int $3" ); } else { raise(SIGTRAP); }
#else
#define DebuggerBreak()  raise(SIGTRAP)
#endif
#endif
#define	DebuggerBreakIfDebugging() if ( !Plat_IsInDebugSession() ) ; else DebuggerBreak()

#define	DebuggerBreakIfDebugging_StagingOnly()

// Allows you to specify code that should only execute if we are in a staging build. Otherwise the code noops.
#define STAGING_ONLY_EXEC( _exec ) do { } while (0)

// C functions for external declarations that call the appropriate C++ methods
#ifndef EXPORT
	#ifdef _WIN32
		#define EXPORT	_declspec(dllexport)
	#else
		#define EXPORT	/* */
	#endif
#endif

#if defined __i386__ && !defined __linux__
	#define id386	1
#else
	#define id386	0
#endif  // __i386__

// decls for aligning data
#ifdef _WIN32
        #define DECL_ALIGN(x) __declspec(align(x))

#elif GNUC
	#define DECL_ALIGN(x) __attribute__((aligned(x)))
#else
        #define DECL_ALIGN(x) /* */
#endif

#ifdef _MSC_VER
// MSVC has the align at the start of the struct
#define ALIGN4 DECL_ALIGN(4)
#define ALIGN8 DECL_ALIGN(8)
#define ALIGN16 DECL_ALIGN(16)
#define ALIGN32 DECL_ALIGN(32)
#define ALIGN128 DECL_ALIGN(128)

#define ALIGN4_POST
#define ALIGN8_POST
#define ALIGN16_POST
#define ALIGN32_POST
#define ALIGN128_POST
#elif defined( GNUC )
// gnuc has the align decoration at the end
#define ALIGN4
#define ALIGN8 
#define ALIGN16
#define ALIGN32
#define ALIGN128

#define ALIGN4_POST DECL_ALIGN(4)
#define ALIGN8_POST DECL_ALIGN(8)
#define ALIGN16_POST DECL_ALIGN(16)
#define ALIGN32_POST DECL_ALIGN(32)
#define ALIGN128_POST DECL_ALIGN(128)
#else
#error "Please define your platform"
#endif

// Pull in the /analyze code annotations.
#include "annotations.h"

//-----------------------------------------------------------------------------
// Convert int<-->pointer, avoiding 32/64-bit compiler warnings:
//-----------------------------------------------------------------------------
template<typename T>
constexpr inline std::enable_if_t<std::is_integral_v<T>, void*> INT_TO_POINTER(T i) noexcept {
	if constexpr (std::is_unsigned_v<T>) {
		return reinterpret_cast<void*>(static_cast<uintp>(i));
	}
	return reinterpret_cast<void*>(static_cast<intp>(i));
}
template <typename R, typename T>
constexpr inline std::enable_if_t<std::is_pointer_v<T>, R> POINTER_TO_INT(T p) noexcept {
	if constexpr (std::is_unsigned_v<R>) {
		return static_cast<uint>(reinterpret_cast<uintp>(p));
	}
	return static_cast<int>(reinterpret_cast<intp>(p));
}


//-----------------------------------------------------------------------------
// Stack-based allocation related helpers
//-----------------------------------------------------------------------------
#if defined( GNUC )
	// dimhotepus: Align at 16 bytes boundary for performance and SIMD.
	#define stackalloc( _size )		AlignValue( alloca( AlignValue( _size, 16 ) ), 16 )
#ifdef _LINUX
	#define mallocsize( _p )	( malloc_usable_size( _p ) )
#elif defined(OSX)
	#define mallocsize( _p )	( malloc_size( _p ) )
#else
#error "Please define your platform"
#endif
#elif defined ( _WIN32 )
	// dimhotepus: Align at 16 bytes boundary for performance and SIMD. 
	#define stackalloc( _size )		AlignValue( _alloca( AlignValue( _size, 16 ) ), 16 )
	#define mallocsize( _p )		( _msize( _p ) )
#endif

// dimhotepus: Add type-safe interface.
#define stackallocT( type_, _size )		static_cast<type_*>( stackalloc( sizeof(type_) * (_size) ) )

#define  stackfree( _p )			0

// Linux had a few areas where it didn't construct objects in the same order that Windows does.
// So when CVProfile::CVProfile() would access g_pMemAlloc, it would crash because the allocator wasn't initalized yet.
#ifdef POSIX
	#define CONSTRUCT_EARLY __attribute__((init_priority(101)))
#else
	#define CONSTRUCT_EARLY
	#endif

#if defined(_MSC_VER)
	#define SELECTANY __declspec(selectany)
	#define RESTRICT __restrict
	#define RESTRICT_FUNC __declspec(restrict)
	#define FMTFUNCTION( a, b )

	// dimhotepus: Allow compiler optimize allocated pointer access.
	#define ALLOC_CALL RESTRICT_FUNC __declspec(allocator)
	#define FREE_CALL
#elif defined(GNUC)
	#define SELECTANY __attribute__((weak))
	#if defined(LINUX) && !defined(DEDICATED)
		#define RESTRICT
	#else
		#define RESTRICT __restrict
	#endif
	#define RESTRICT_FUNC
	// squirrel.h does a #define printf DevMsg which leads to warnings when we try
	// to use printf as the prototype format function. Using __printf__ instead.
	#define FMTFUNCTION( fmtargnumber, firstvarargnumber ) __attribute__ (( format( __printf__, fmtargnumber, firstvarargnumber )))

	// dimhotepus: Allow compiler optimize allocated pointer access.
	#define ALLOC_CALL
	#define FREE_CALL
#else
	#define SELECTANY static
	#define RESTRICT
	#define RESTRICT_FUNC
	#define FMTFUNCTION( a, b )

	// dimhotepus: Allow compiler optimize allocated pointer access.
	#define ALLOC_CALL
	#define FREE_CALL
#endif

#if defined( _WIN32 )

	// Used for dll exporting and importing
	#define DLL_EXPORT				extern "C" __declspec( dllexport )
	#define DLL_IMPORT				extern "C" __declspec( dllimport )

	// Can't use extern "C" when DLL exporting a class
	#define DLL_CLASS_EXPORT		__declspec( dllexport )
	#define DLL_CLASS_IMPORT		__declspec( dllimport )

	// Can't use extern "C" when DLL exporting a global
	#define DLL_GLOBAL_EXPORT		extern __declspec( dllexport )
	#define DLL_GLOBAL_IMPORT		extern __declspec( dllimport )

	#define DLL_LOCAL

#elif defined GNUC
// Used for dll exporting and importing
#define  DLL_EXPORT   extern "C" __attribute__ ((visibility("default")))
#define  DLL_IMPORT   extern "C"

// Can't use extern "C" when DLL exporting a class
#define  DLL_CLASS_EXPORT __attribute__ ((visibility("default")))
#define  DLL_CLASS_IMPORT

// Can't use extern "C" when DLL exporting a global
#define  DLL_GLOBAL_EXPORT   extern __attribute ((visibility("default")))
#define  DLL_GLOBAL_IMPORT   extern

#define  DLL_LOCAL __attribute__ ((visibility("hidden")))

#else
#error "Please define your platform"
#endif

// Used for standard calling conventions
#if defined( _WIN32 )
	#define  STDCALL				__stdcall
	#define  FASTCALL				__fastcall
	#define  FORCEINLINE			__forceinline
	// GCC 3.4.1 has a bug in supporting forced inline of templated functions
	// this macro lets us not force inlining in that case
	#define  FORCEINLINE_TEMPLATE		__forceinline
#else
	#define FORCEINLINE inline __attribute__((always_inline))
	// dimhotepus: TF2 backport. Drop workaround for old GCC.
	#define FORCEINLINE_TEMPLATE		FORCEINLINE
#endif

// dimhotepus: TF2 backport.
#if ( defined(__SANITIZE_ADDRESS__) && __SANITIZE_ADDRESS__ )
	#define NO_ASAN __attribute__((no_sanitize("address")))
	#define NO_ASAN_FORCEINLINE NO_ASAN inline
#else
	#define NO_ASAN
	#define NO_ASAN_FORCEINLINE FORCEINLINE
#endif

// Force a function call site -not- to inlined. (useful for profiling)
#define DONT_INLINE(a) (((int)(a)+1)?(a):(a))

// Pass hints to the compiler to prevent it from generating unnessecary / stupid code
// in certain situations.  Several compilers other than MSVC also have an equivilent
// construct.
//
// Essentially the 'Hint' is that the condition specified is assumed to be true at
// that point in the compilation.  If '0' is passed, then the compiler assumes that
// any subsequent code in the same 'basic block' is unreachable, and thus usually
// removed.
#ifdef _MSC_VER
	#define HINT(THE_HINT)	__assume((THE_HINT))
#else
	#define HINT(THE_HINT)	0
#endif

// Marks the codepath from here until the next branch entry point as unreachable,
// and asserts if any attempt is made to execute it.
#define UNREACHABLE() { Assert(0); unreachable(); }

// In cases where no default is present or appropriate, this causes MSVC to generate
// as little code as possible, and throw an assertion in debug.
#define NO_DEFAULT default: { Assert(0); unreachable(); }

#if defined( LINUX ) && ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
  // based on some Jonathan Wakely macros on the net...
  #define GCC_DIAG_STR(s) #s
  #define GCC_DIAG_JOINSTR(x,y) GCC_DIAG_STR(x ## y)
  #define GCC_DIAG_DO_PRAGMA(x) _Pragma (#x)
  #define GCC_DIAG_PRAGMA(x)	GCC_DIAG_DO_PRAGMA(GCC diagnostic x)

  #define GCC_DIAG_PUSH_OFF(x)	GCC_DIAG_PRAGMA(push) GCC_DIAG_PRAGMA(ignored GCC_DIAG_JOINSTR(-W,x))
  #define GCC_DIAG_POP()		GCC_DIAG_PRAGMA(pop)
#else
  #define GCC_DIAG_PUSH_OFF(x)
  #define GCC_DIAG_POP()
#endif

#ifdef POSIX
#define _stricmp stricmp
#define strcmpi stricmp
#define stricmp strcasecmp
#define _vsnprintf vsnprintf
#define _alloca alloca
#ifdef _snprintf
#undef _snprintf
#endif
#define _snprintf snprintf
#define GetProcAddress dlsym
#define _chdir chdir
#define _strnicmp strnicmp
#define strnicmp strncasecmp
#define _getcwd getcwd
#define _snwprintf swprintf
#define swprintf_s swprintf
#define wcsicmp _wcsicmp
#define _wcsicmp wcscmp
#define _finite finite
#define _tempnam tempnam
#define _unlink unlink
#define _access access
#define _mkdir(dir) mkdir( dir, S_IRWXU | S_IRWXG | S_IRWXO )
#define _wtoi(arg) wcstol(arg, NULL, 10)
#define _wtoi64(arg) wcstoll(arg, NULL, 10)

typedef uint32 HMODULE;
typedef void *HANDLE;
#endif

//-----------------------------------------------------------------------------
// fsel
//-----------------------------------------------------------------------------
static FORCEINLINE float fsel(float fComparand, float fValGE, float fLT)
{
	return fComparand >= 0 ? fValGE : fLT;
}
static FORCEINLINE double fsel(double fComparand, double fValGE, double fLT)
{
	return fComparand >= 0 ? fValGE : fLT;
}

//-----------------------------------------------------------------------------
// FP exception handling
//-----------------------------------------------------------------------------
//#define CHECK_FLOAT_EXCEPTIONS		1

#if !defined( _X360 )
#if defined( _MSC_VER )

	#if defined( PLATFORM_WINDOWS_PC64 )
		inline void SetupFPUControlWord()
		{
		}
	#else
		inline void SetupFPUControlWordForceExceptions()
		{
			// use local to get and store control word
			uint16 tmpCtrlW;
			__asm
			{
				fnclex						/* clear all current exceptions */
				fnstcw word ptr [tmpCtrlW]	/* get current control word */
				and [tmpCtrlW], 0FCC0h		/* Keep infinity control + rounding control */
				or [tmpCtrlW], 0230h		/* set to 53-bit, mask only inexact, underflow */
				fldcw word ptr [tmpCtrlW]	/* put new control word in FPU */
			}
		}

		#ifdef CHECK_FLOAT_EXCEPTIONS

			inline void SetupFPUControlWord()
			{
				SetupFPUControlWordForceExceptions();
			}

		#else

			inline void SetupFPUControlWord()
			{
				// use local to get and store control word
				uint16 tmpCtrlW;
				__asm
				{
					fnstcw word ptr [tmpCtrlW]	/* get current control word */
					and [tmpCtrlW], 0FCC0h		/* Keep infinity control + rounding control */
					or [tmpCtrlW], 023Fh		/* set to 53-bit, mask only inexact, underflow */
					fldcw word ptr [tmpCtrlW]	/* put new control word in FPU */
				}
			}

		#endif
	#endif

#else

	inline void SetupFPUControlWord()
	{
		__volatile unsigned short int __cw;
		__asm __volatile ("fnstcw %0" : "=m" (__cw));
		__cw = __cw & 0x0FCC0;	// keep infinity control, keep rounding mode
		__cw = __cw | 0x023F;	// set 53-bit, no exceptions
		__asm __volatile ("fldcw %0" : : "m" (__cw));
	}

#endif // _MSC_VER

#else

	#ifdef _DEBUG
		FORCEINLINE bool IsFPUControlWordSet()
		{
			float f = 0.996f;
			union
			{
				double flResult;
				int pResult[2];
			};
			flResult = __fctiw( f );
			return ( pResult[1] == 1 );
		}
	#endif

	inline void SetupFPUControlWord()
	{
		// Set round-to-nearest in FPSCR
		// (cannot assemble, must use op-code form)
		__emit( 0xFF80010C );	// mtfsfi  7,0

		// Favour compatibility over speed (make sure the VPU set to Java-compliant mode)
		// NOTE: the VPU *always* uses round-to-nearest
			__vector4  a = { 0.0f, 0.0f, 0.0f, 0.0f };
			a;				//	Avoid compiler warning
			__asm
		{
			mtvscr a;	// Clear the Vector Status & Control Register to zero
		}
	}

#endif // _X360

//-----------------------------------------------------------------------------
// Purpose: Standard functions for handling endian-ness
//-----------------------------------------------------------------------------

//-------------------------------------
// Basic swaps
//-------------------------------------

#ifdef _MSC_VER

#include <intrin.h>
#define bswap_16(x) _byteswap_ushort(x)
#define bswap_32(x) _byteswap_ulong(x)
#define bswap_64(x) _byteswap_uint64(x)

#elif defined(__APPLE__)

// Mac OS X / Darwin features
#include <libkern/OSByteOrder.h>
#define bswap_16(x) OSSwapInt16(x)
#define bswap_32(x) OSSwapInt32(x)
#define bswap_64(x) OSSwapInt64(x)

#elif defined(__sun) || defined(sun)

#include <sys/byteorder.h>
#define bswap_16(x) BSWAP_16(x)
#define bswap_32(x) BSWAP_32(x)
#define bswap_64(x) BSWAP_64(x)

#elif defined(__FreeBSD__)

#include <sys/endian.h>
#define bswap_16(x) bswap16(x)
#define bswap_32(x) bswap32(x)
#define bswap_64(x) bswap64(x)

#elif defined(__OpenBSD__)

#include <sys/types.h>
#define bswap_16(x) swap16(x)
#define bswap_32(x) swap32(x)
#define bswap_64(x) swap64(x)

#elif defined(__NetBSD__)

#include <sys/types.h>
#include <machine/bswap.h>
#if defined(__BSWAP_RENAME) && !defined(__bswap_32)
#define bswap_16(x) bswap16(x)
#define bswap_32(x) bswap32(x)
#define bswap_64(x) bswap64(x)
#endif

#else

#include <byteswap.h>

#endif

template <typename T>
inline std::enable_if_t<std::is_scalar_v<T> && sizeof(T) == 2 && alignof(T) == alignof(unsigned short), T> WordSwapC( T w )
{
   return static_cast<T>( bswap_16( w ) );
}

template <typename T>
inline std::enable_if_t<std::is_scalar_v<T> && sizeof(T) == 4 && alignof(T) == alignof(unsigned int), T> DWordSwapC( T dw )
{
   return static_cast<T>( bswap_32( dw ) );
}

template <typename T>
inline std::enable_if_t<std::is_scalar_v<T> && sizeof(T) == 8 && alignof(T) == alignof(unsigned long long), T> QWordSwapC( T qdw )
{
   return static_cast<T>( bswap_64( qdw ) );
}

//-------------------------------------
// Fast swaps
//-------------------------------------
// dimhotepus: Use C instead of ASM as it fallback to intrinsic.

#define WordSwap  WordSwapC
#define DWordSwap DWordSwapC
#define QWordSwap QWordSwapC

//-------------------------------------
// The typically used methods.
//-------------------------------------

#if defined(__i386__) && !defined(VALVE_LITTLE_ENDIAN)
#define VALVE_LITTLE_ENDIAN 1
#endif

#if defined( _SGI_SOURCE )
#define	VALVE_BIG_ENDIAN 1
#endif

// If a swapped float passes through the fpu, the bytes may get changed.
// Prevent this by swapping floats as DWORDs.
#define SafeSwapFloat( pOut, pIn )	(*((uint*)pOut) = DWordSwap( *((const uint*)pIn) ))

#if defined(VALVE_LITTLE_ENDIAN)

#define BigShort( val )				WordSwap( val )
#define BigWord( val )				WordSwap( val )
#define BigLong( val )				DWordSwap( val )
#define BigDWord( val )				DWordSwap( val )
#define LittleShort( val )			( val )
#define LittleWord( val )			( val )
#define LittleLong( val )			( val )
#define LittleDWord( val )			( val )
#define LittleQWord( val )			( val )
#define SwapShort( val )			BigShort( val )
#define SwapWord( val )				BigWord( val )
#define SwapLong( val )				BigLong( val )
#define SwapDWord( val )			BigDWord( val )

// Pass floats by pointer for swapping to avoid truncation in the fpu
#define BigFloat( pOut, pIn )		SafeSwapFloat( pOut, pIn )
#define LittleFloat( pOut, pIn )	( *pOut = *pIn )
#define SwapFloat( pOut, pIn )		BigFloat( pOut, pIn )

#elif defined(VALVE_BIG_ENDIAN)

#define BigShort( val )				( val )
#define BigWord( val )				( val )
#define BigLong( val )				( val )
#define BigDWord( val )				( val )
#define LittleShort( val )			WordSwap( val )
#define LittleWord( val )			WordSwap( val )
#define LittleLong( val )			DWordSwap( val )
#define LittleDWord( val )			DWordSwap( val )
#define LittleQWord( val )			QWordSwap( val )
#define SwapShort( val )			LittleShort( val )
#define SwapWord( val )				LittleWord( val )
#define SwapLong( val )				LittleLong( val )
#define SwapDWord( val )			LittleDWord( val )

// Pass floats by pointer for swapping to avoid truncation in the fpu
#define BigFloat( pOut, pIn )		( *pOut = *pIn )
#define LittleFloat( pOut, pIn )	SafeSwapFloat( pOut, pIn )
#define SwapFloat( pOut, pIn )		LittleFloat( pOut, pIn )

#else

template <bool isLittleEndian = endian::native == endian::little>
inline int16 BigShort( int16 val )
{
	if constexpr (isLittleEndian)
	{
		return WordSwap( val );
	}
	else
	{
		return val;
	}
}
template <bool isLittleEndian = endian::native == endian::little>
inline uint16 BigWord( uint16 val )
{
	if constexpr (isLittleEndian)
	{
		return WordSwap( val );
	}
	else
	{
		return val;
	}
}
template <bool isLittleEndian = endian::native == endian::little>
inline long BigLong( long val )
{
	if constexpr (isLittleEndian)
	{
		// dimhotepus: Honor size ex for LP64.
		if constexpr (sizeof(val) <= 4) //-V112
			return DWordSwap( val );
		else
			return QWordSwap( val );
	}
	else
	{
		return val;
	}
}
template <bool isLittleEndian = endian::native == endian::little>
inline uint32 BigDWord( uint32 val )
{
	if constexpr (isLittleEndian)
	{
		return DWordSwap( val );
	}
	else
	{
		return val;
	}
}
template <bool isLittleEndian = endian::native == endian::little>
inline int16 LittleShort( int16 val )
{
	if constexpr (isLittleEndian)
	{
		return val;
	}
	else
	{
		return WordSwap( val );
	}
}
template <bool isLittleEndian = endian::native == endian::little>
inline uint16 LittleWord( uint16 val )
{
	if constexpr (isLittleEndian)
	{
		return val;
	}
	else
	{
		return WordSwap( val );
	}
}
template <bool isLittleEndian = endian::native == endian::little>
inline long LittleLong( long val )
{
	if constexpr (isLittleEndian)
		return val;
	// dimhotepus: Honor size ex for LP64.
	else if constexpr (sizeof(val) <= 4) //-V112
		return DWordSwap( val );
	else
		return QWordSwap( val );
}
template <bool isLittleEndian = endian::native == endian::little>
inline uint32 LittleDWord( uint32 val )
{
	if constexpr (isLittleEndian)
	{
		return val;
	}
	else
	{
		return DWordSwap(val);
	}
}
template <bool isLittleEndian = endian::native == endian::little>
inline uint64 LittleQWord( uint64 val )
{
	if constexpr (isLittleEndian)
	{
		return val;
	}
	else
	{
		return QWordSwap( val );
	}
}
inline int16 SwapShort( int16 val )					{ return WordSwap( val ); }
inline uint16 SwapWord( uint16 val )				{ return WordSwap( val ); }
inline long SwapLong( long val )					{ return DWordSwap( val ); }
inline uint32 SwapDWord( uint32 val )				{ return DWordSwap( val ); }

// Pass floats by pointer for swapping to avoid truncation in the fpu
template <bool isLittleEndian = endian::native == endian::little>
inline void BigFloat( float *pOut, const float *pIn )
{
	if constexpr (isLittleEndian)
	{
		SafeSwapFloat( pOut, pIn );
	}
	else
	{
		*pOut = *pIn;
	}
}
template <bool isLittleEndian = endian::native == endian::little>
inline void LittleFloat( float *pOut, const float *pIn )
{
	if constexpr (isLittleEndian)
	{
		*pOut = *pIn;
	}
	else
	{
		SafeSwapFloat( pOut, pIn );
	}
}
inline void SwapFloat( float *pOut, const float *pIn )		{ SafeSwapFloat( pOut, pIn ); }

#endif

FORCEINLINE unsigned int LoadLittleDWord( const unsigned int *base, size_t dwordIndex )
{
	return LittleDWord( base[dwordIndex] );
}

FORCEINLINE void StoreLittleDWord( unsigned int *base, size_t dwordIndex, unsigned int dword )
{
	base[dwordIndex] = LittleDWord(dword);
}

//-----------------------------------------------------------------------------
// Portability casting
//-----------------------------------------------------------------------------
// dimhotepus: TF2 & CS:GO backport.
template <typename Tdst, typename Tsrc>
[[nodiscard]] inline
#ifndef DEBUG
    constexpr
#endif
    Tdst
    size_cast(Tsrc val) noexcept {
  static_assert(
      sizeof(Tdst) <= sizeof(uint64) && sizeof(Tsrc) <= sizeof(uint64),
      "Okay in my defense there weren't any types larger than 64-bits when "
      "this code was written.");

  Tdst dst = (Tdst)val;

  if constexpr (sizeof(Tdst) < sizeof(Tsrc)) {
    // If this fails, the source value didn't actually fit in the destination
    // value--you'll need to change the return type's size to match the source
    // type in the calling code.
    if (val != (Tsrc)dst) {
      // Can't use assert here, and if this happens when running on a machine
      // internally we should crash in preference to missing the problem ( so
      // not DebuggerBreakIfDebugging() ).
      DebuggerBreak();
    }
  }

  return dst;
}

//-----------------------------------------------------------------------------
// DLL export for platform utilities
//-----------------------------------------------------------------------------
#ifndef STATIC_TIER0

#ifdef TIER0_DLL_EXPORT
#define PLATFORM_INTERFACE	DLL_EXPORT
#define PLATFORM_OVERLOAD	DLL_GLOBAL_EXPORT
#define PLATFORM_CLASS		DLL_CLASS_EXPORT
#else
#define PLATFORM_INTERFACE	DLL_IMPORT
#define PLATFORM_OVERLOAD	DLL_GLOBAL_IMPORT
#define PLATFORM_CLASS		DLL_CLASS_IMPORT
#endif

#else	// BUILD_AS_DLL

#define PLATFORM_INTERFACE	extern
#define PLATFORM_OVERLOAD
#define PLATFORM_CLASS

#endif	// BUILD_AS_DLL


// When in benchmark mode, the timer returns a simple incremented value each time you call it.
//
// It should not be changed after startup unless you really know what you're doing. The only place
// that should do this is the benchmark code itself so it can output a legit duration.
PLATFORM_INTERFACE void				Plat_SetBenchmarkMode( bool bBenchmarkMode );	
PLATFORM_INTERFACE bool				Plat_IsInBenchmarkMode();


PLATFORM_INTERFACE double			Plat_FloatTime();		// Returns time in seconds since the module was loaded.
PLATFORM_INTERFACE uint32			Plat_MSTime();			// Time in milliseconds.
PLATFORM_INTERFACE uint64			Plat_USTime();			// Time in microseconds.
PLATFORM_INTERFACE char *			Plat_ctime( const time_t *timep, char *buf, size_t bufsize );
PLATFORM_INTERFACE void				Plat_GetModuleFilename( char *pOut, int nMaxBytes );

PLATFORM_INTERFACE void				Plat_ExitProcess( int nCode );

//called to exit the process due to a fatal error. This allows for the application to handle providing a hook as well which can be called
//before exiting
PLATFORM_INTERFACE void				Plat_ExitProcessWithError( int nCode, bool bGenerateMinidump = false );

//sets the callback that will be triggered by Plat_ExitProcessWithError. NULL is valid. The return value true indicates that
//the exit has been handled and no further processing should be performed. False will cause a minidump to be generated, and the process
//to be terminated
using ExitProcessWithErrorCBFn = bool (*)(int);
PLATFORM_INTERFACE void				Plat_SetExitProcessWithErrorCB( ExitProcessWithErrorCBFn pfnCB );

PLATFORM_INTERFACE struct tm *		Plat_gmtime( const time_t *timep, struct tm *result );
PLATFORM_INTERFACE time_t			Plat_timegm( struct tm *timeptr );
PLATFORM_INTERFACE struct tm *		Plat_localtime( const time_t *timep, struct tm *result );

#if defined( _WIN32 )
	extern "C" unsigned __int64 __rdtsc();
	extern "C" unsigned __int64 __rdtscp(unsigned *aux);

// dimhotepus: Clang does not have __rdtscp intrinsic for now.
#if defined(_MSC_VER) && !defined(__clang__) 
	#pragma intrinsic(__rdtscp)
#endif  // _MSC_VER) && !defined(__clang__) 

	#pragma intrinsic(__rdtsc)
#endif

FORCEINLINE uint64_t Plat_Rdtsc()
{
	// See https://www.felixcloutier.com/x86/rdtsc
	// 
	// The RDTSC instruction is not a serializing instruction.  It does not
	// necessarily wait until all previous instructions have been executed
	// before reading the counter.  Similarly, subsequent instructions may begin
	// execution before the read operation is performed.  The following items
	// may guide software seeking to order executions of RDTSC:
	// * If software requires RDTSC to be executed only after all previous
	// instructions have executed and all previous loads are globally visible,
	// it can execute LFENCE immediately before RDTSC.
	// * If software requires RDTSC to be executed only after all previous
	// instructions have executed and all previous loads and stores are globally
	// visible, it can execute the sequence MFENCE;LFENCE immediately before
	// RDTSC.
	// * If software requires RDTSC to be executed prior to execution of any
	// subsequent instruction (including any memory accesses), it can execute
	// the sequence LFENCE immediately after RDTSC.
	//
	// We do not mfence before as for timing only ordering matters, not finished
	// memory stores are ok.
	// Ensure no reordering aka acquire barrier.
	_mm_lfence();
	const uint64_t tsc{__rdtsc()};
	// Ensure no reordering aka acquire barrier.
	_mm_lfence();
	return tsc;
}

FORCEINLINE uint64_t Plat_Rdtscp(uint32_t &coreId)
{
	// See https://www.felixcloutier.com/x86/rdtscp
	// 
	// The RDTSCP instruction is not a serializing instruction, but it does wait
	// until all previous instructions have executed and all previous loads are
	// globally visible.  But it does not wait for previous stores to be
	// globally visible, and subsequent instructions may begin execution before
	// the read operation is performed.  The following items may guide software
	// seeking to order executions of RDTSCP:
	// * If software requires RDTSCP to be executed only after all previous
	// stores are globally visible, it can execute MFENCE immediately before
	// RDTSCP.
	// * If software requires RDTSCP to be executed prior to execution of any
	// subsequent instruction (including any memory accesses), it can execute
	// LFENCE immediately after RDTSCP.
	//
	// We do not mfence before as for timing only ordering matters, not finished
	// memory stores are ok. 
	const uint64_t tsc{__rdtscp(&coreId)};
	// Ensure no reordering aka acquire barrier.
	_mm_lfence();
	return tsc;
}

inline uint64_t Plat_MeasureRtscpOverhead()
{
	uint32_t coreIdStart, coreIdEnd;
	uint64_t overheads[64];
	for (size_t i{0}; i < std::size(overheads);)
	{
		uint64_t start = Plat_Rdtscp(coreIdStart);
		uint64_t end = Plat_Rdtscp(coreIdEnd);
		// Usually tsc synced over cores (constant_tsc), but due to UEFI / CPU
		// bugs all is possible.  So expect clock is monotonic only on a single
		// core.
		if (coreIdStart == coreIdEnd)
		{
			 overheads[i++] = end - start;
		}
	}

	// median.
	std::nth_element(std::begin(overheads),
		std::begin(overheads) + std::size(overheads) / 2,
		std::end(overheads));
	uint64_t median = overheads[std::size(overheads) / 2];

	return median;
}

// b/w compatibility
#define Sys_FloatTime Plat_FloatTime

// Protect against bad auto operator=
#define DISALLOW_OPERATOR_EQUAL( _classname )			\
	public:											\
		_classname &operator=( const _classname & ) = delete;

// Define a reasonable operator=
#define IMPLEMENT_OPERATOR_EQUAL( _classname )			\
	public:												\
		_classname &operator=( const _classname &src )	\
		{												\
			BitwiseCopy( this, &src, sizeof(_classname) );	\
			return *this;								\
		}

// Processor Information:
struct CPUInformation
{
	int	 m_Size;		// Size of this structure, for forward compatability.

	bool m_bRDTSC : 1,	// Is RDTSC supported?
		 m_bCMOV  : 1,  // Is CMOV supported?
		 m_bFCMOV : 1,  // Is FCMOV supported?
		 m_bSSE	  : 1,	// Is SSE supported?
		 m_bSSE2  : 1,	// Is SSE2 Supported?
		 m_b3DNow : 1,	// Is 3DNow! Supported?
		 m_bMMX   : 1,	// Is MMX supported?
		 m_bHT	  : 1;	// Is HyperThreading supported?

	uint8 m_nLogicalProcessors;		// Number op logical processors.
	uint8 m_nPhysicalProcessors;	// Number of physical processors
	
	bool m_bSSE3 : 1,
		 m_bSSSE3 : 1,
		 m_bSSE4a : 1,
		 m_bSSE41 : 1,
		 m_bSSE42 : 1;	

	int64 m_Speed;						// In cycles per second.

	const char* m_szProcessorID;				// Processor vendor Identification.

	uint32 m_nModel;
	uint32 m_nFeatures[3];

	const char *m_szProcessorBrand;

	CPUInformation() { memset(this, 0, sizeof(*this)); }
};

// Have to return a pointer, not a reference, because references are not compatible with the
// extern "C" implied by PLATFORM_INTERFACE.
PLATFORM_INTERFACE const CPUInformation* GetCPUInformation();

#define MEMORY_INFORMATION_VERSION 0

struct MemoryInformation
{
	int m_nStructVersion;

	// Total physical RAM in MiBs.
	uint m_nPhysicalRamMbTotal;
	// Available physical RAM in MiBs.
	uint m_nPhysicalRamMbAvailable;
	
	// Total virtual RAM in MiBs.
	uint m_nVirtualRamMbTotal;
	// Available virtual RAM in MiBs.
	uint m_nVirtualRamMbAvailable;

	inline MemoryInformation()
	{
		memset( this, 0, sizeof( *this ) );
		m_nStructVersion = MEMORY_INFORMATION_VERSION;
	}
};

// Returns true if the passed in MemoryInformation structure was filled out, otherwise false.
PLATFORM_INTERFACE bool GetMemoryInformation( MemoryInformation *pOutMemoryInfo );

PLATFORM_INTERFACE float GetCPUUsage();

PLATFORM_INTERFACE void GetCurrentDate( int *pDay, int *pMonth, int *pYear );

// ---------------------------------------------------------------------------------- //
// Performance Monitoring Events - L2 stats etc...
// ---------------------------------------------------------------------------------- //
PLATFORM_INTERFACE void InitPME();
PLATFORM_INTERFACE void ShutdownPME();

//-----------------------------------------------------------------------------
// Thread related functions
//-----------------------------------------------------------------------------

// Sets a hardware data breakpoint on the given address. Currently Win32-only.
// Specify 1, 2, or 4 bytes for nWatchBytes; pass 0 to unregister the address.
PLATFORM_INTERFACE void	Plat_SetHardwareDataBreakpoint( const void *pAddress, int nWatchBytes, bool bBreakOnRead );

// Apply current hardware data breakpoints to a newly created thread.
PLATFORM_INTERFACE void	Plat_ApplyHardwareDataBreakpointsToNewThread( unsigned long dwThreadID );

//-----------------------------------------------------------------------------
// Process related functions
//-----------------------------------------------------------------------------
PLATFORM_INTERFACE const tchar *Plat_GetCommandLine();
#ifndef _WIN32
// helper function for OS's that don't have a ::GetCommandLine() call
PLATFORM_INTERFACE void Plat_SetCommandLine( const char *cmdLine );
#endif
PLATFORM_INTERFACE const char *Plat_GetCommandLineA();

//-----------------------------------------------------------------------------
// Security related functions
//-----------------------------------------------------------------------------
// Ensure that the hardware key's drivers have been installed.
PLATFORM_INTERFACE bool Plat_VerifyHardwareKeyDriver();

// Ok, so this isn't a very secure way to verify the hardware key for now.  It
// is primarially depending on the fact that all the binaries have been wrapped
// with the secure wrapper provided by the hardware keys vendor.
PLATFORM_INTERFACE bool Plat_VerifyHardwareKey();

// The same as above, but notifies user with a message box when the key isn't in
// and gives him an opportunity to correct the situation.
PLATFORM_INTERFACE bool Plat_VerifyHardwareKeyPrompt();

// Can be called in real time, doesn't perform the verify every frame.  Mainly just
// here to allow the game to drop out quickly when the key is removed, rather than
// allowing the wrapper to pop up it's own blocking dialog, which the engine doesn't
// like much.
PLATFORM_INTERFACE bool Plat_FastVerifyHardwareKey();

//-----------------------------------------------------------------------------
// Just logs file and line to simple.log
//-----------------------------------------------------------------------------
PLATFORM_INTERFACE void* Plat_SimpleLog( const tchar* file, int line );

#define Plat_FastMemset memset
#define Plat_FastMemcpy memcpy

//-----------------------------------------------------------------------------
// Returns true if debugger attached, false otherwise
//-----------------------------------------------------------------------------
#if defined(_WIN32) || defined(LINUX) || defined(OSX)
PLATFORM_INTERFACE bool Plat_IsInDebugSession();
PLATFORM_INTERFACE void Plat_DebugString( const char * );
#else
inline bool Plat_IsInDebugSession( bool bForceRecheck = false ) { return false; }
#define Plat_DebugString(s) ((void)0)
#endif

// dimhotepus: Check user is admin or priviledged one.
PLATFORM_INTERFACE bool Plat_IsUserAnAdmin();

#ifdef _WIN32
enum class SystemBackdropType
{
	Auto,        // [Default] Let DWM automatically decide the system-drawn backdrop for this window.
	None,        // Do not draw any system backdrop.
	MainWindow,  // Draw the backdrop material effect corresponding to a long-lived window.
	TransientWindow,  // Draw the backdrop material effect corresponding to a transient window.
	TabbedWindow,  // Draw the backdrop material effect corresponding to a window with a tabbed title bar.
};

// dimhotepus: Apply system Mica materials and Dark mode (if enabled) to window.
// See https://learn.microsoft.com/en-us/windows/apps/design/style/mica
PLATFORM_INTERFACE bool Plat_ApplySystemTitleBarTheme( void *window, SystemBackdropType backDropType );
#endif

//-----------------------------------------------------------------------------
// Returns true if running on a 64 bit (windows) OS
//-----------------------------------------------------------------------------
PLATFORM_INTERFACE bool Is64BitOS();


//-----------------------------------------------------------------------------
// XBOX Components valid in PC compilation space
//-----------------------------------------------------------------------------

#define XBOX_DVD_SECTORSIZE			2048
#define XBOX_DVD_ECC_SIZE			32768 // driver reads in quantum ECC blocks
#define XBOX_HDD_SECTORSIZE			512

// Custom windows messages for Xbox input
#define WM_XREMOTECOMMAND					(WM_USER + 100)
#define WM_XCONTROLLER_KEY					(WM_USER + 101)
#define WM_SYS_UI							(WM_USER + 102)
#define WM_SYS_SIGNINCHANGED				(WM_USER + 103)
#define WM_SYS_STORAGEDEVICESCHANGED		(WM_USER + 104)
#define WM_SYS_PROFILESETTINGCHANGED		(WM_USER + 105)
#define WM_SYS_MUTELISTCHANGED				(WM_USER + 106)
#define WM_SYS_INPUTDEVICESCHANGED			(WM_USER + 107)
#define WM_SYS_INPUTDEVICECONFIGCHANGED		(WM_USER + 108)
#define WM_LIVE_CONNECTIONCHANGED			(WM_USER + 109)
#define WM_LIVE_INVITE_ACCEPTED				(WM_USER + 110)
#define WM_LIVE_LINK_STATE_CHANGED			(WM_USER + 111)
#define WM_LIVE_CONTENT_INSTALLED			(WM_USER + 112)
#define WM_LIVE_MEMBERSHIP_PURCHASED		(WM_USER + 113)
#define WM_LIVE_VOICECHAT_AWAY				(WM_USER + 114)
#define WM_LIVE_PRESENCE_CHANGED			(WM_USER + 115)
#define WM_FRIENDS_PRESENCE_CHANGED			(WM_USER + 116)
#define WM_FRIENDS_FRIEND_ADDED				(WM_USER + 117)
#define WM_FRIENDS_FRIEND_REMOVED			(WM_USER + 118)
#define WM_CUSTOM_GAMEBANNERPRESSED			(WM_USER + 119)
#define WM_CUSTOM_ACTIONPRESSED				(WM_USER + 120)
#define WM_XMP_STATECHANGED					(WM_USER + 121)
#define WM_XMP_PLAYBACKBEHAVIORCHANGED		(WM_USER + 122)
#define WM_XMP_PLAYBACKCONTROLLERCHANGED	(WM_USER + 123)

inline const char *GetPlatformExt( )
{
	return IsX360() ? ".360" : "";
}

// flat view, 6 hw threads
#define XBOX_PROCESSOR_0			( 1<<0 )
#define XBOX_PROCESSOR_1			( 1<<1 )
#define XBOX_PROCESSOR_2			( 1<<2 )
#define XBOX_PROCESSOR_3			( 1<<3 )
#define XBOX_PROCESSOR_4			( 1<<4 )
#define XBOX_PROCESSOR_5			( 1<<5 )

// core view, 3 cores with 2 hw threads each
#define XBOX_CORE_0_HWTHREAD_0		XBOX_PROCESSOR_0
#define XBOX_CORE_0_HWTHREAD_1		XBOX_PROCESSOR_1
#define XBOX_CORE_1_HWTHREAD_0		XBOX_PROCESSOR_2
#define XBOX_CORE_1_HWTHREAD_1		XBOX_PROCESSOR_3
#define XBOX_CORE_2_HWTHREAD_0		XBOX_PROCESSOR_4
#define XBOX_CORE_2_HWTHREAD_1		XBOX_PROCESSOR_5

//-----------------------------------------------------------------------------
// Include additional dependant header components.
//-----------------------------------------------------------------------------
#include "fasttimer.h"

//-----------------------------------------------------------------------------
// Methods to invoke the constructor, copy constructor, and destructor
//-----------------------------------------------------------------------------
template <class T>
inline T* Construct( T* pMemory )
{
	HINT( pMemory != nullptr );
	return reinterpret_cast<T*>(::new( pMemory ) T); //-V572
}

template <class T, typename ARG1>
inline T* Construct( T* pMemory, ARG1 a1 )
{
	HINT( pMemory != nullptr );
	return reinterpret_cast<T*>(::new( pMemory ) T( a1 )); //-V572
}

template <class T, typename ARG1, typename ARG2>
inline T* Construct( T* pMemory, ARG1 a1, ARG2 a2 )
{
	HINT( pMemory != nullptr );
	return reinterpret_cast<T*>(::new( pMemory ) T( a1, a2 )); //-V572
}

template <class T, typename ARG1, typename ARG2, typename ARG3>
inline T* Construct( T* pMemory, ARG1 a1, ARG2 a2, ARG3 a3 )
{
	HINT( pMemory != nullptr );
	return reinterpret_cast<T*>(::new( pMemory ) T( a1, a2, a3 )); //-V572
}

template <class T, typename ARG1, typename ARG2, typename ARG3, typename ARG4>
inline T* Construct( T* pMemory, ARG1 a1, ARG2 a2, ARG3 a3, ARG4 a4 )
{
	HINT( pMemory != nullptr );
	return reinterpret_cast<T*>(::new( pMemory ) T( a1, a2, a3, a4 )); //-V572
}

template <class T, typename ARG1, typename ARG2, typename ARG3, typename ARG4, typename ARG5>
inline T* Construct( T* pMemory, ARG1 a1, ARG2 a2, ARG3 a3, ARG4 a4, ARG5 a5 )
{
	HINT( pMemory != nullptr );
	return reinterpret_cast<T*>(::new( pMemory ) T( a1, a2, a3, a4, a5 )); //-V572
}

template <typename T, typename... Args>
inline void Construct( T *memory, Args &&... args )
{
	HINT( pMemory != nullptr );
	::new (memory) T{std::forward<Args>(args)...};
}

template <class T, class P>
[[deprecated("Use Construct")]] inline void ConstructOneArg( T* pMemory, P const& arg)
{
	::new( pMemory ) T(arg);
}

template <class T, class P1, class P2 >
[[deprecated("Use Construct")]] inline void ConstructTwoArg( T* pMemory, P1 const& arg1, P2 const& arg2)
{
	::new( pMemory ) T(arg1, arg2);
}

template <class T, class P1, class P2, class P3 >
[[deprecated("Use Construct")]] inline void ConstructThreeArg( T* pMemory, P1 const& arg1, P2 const& arg2, P3 const& arg3)
{
	::new( pMemory ) T(arg1, arg2, arg3);
}

template <class T>
inline T* CopyConstruct( T* pMemory, T const& src )
{
	HINT( pMemory != nullptr );
	return ::new( pMemory ) T{src};
}

template <class T>
inline void CopyConstruct( T* pMemory, T&& src )
{
	HINT( pMemory != nullptr );
  ::new (pMemory) T{std::forward<T>(src)};
}

template <class T, typename... Args>
inline void CopyConstruct( T* pMemory, Args&&... src )
{
	HINT( pMemory != nullptr );
	::new( pMemory ) T{std::forward<Args...>( src )...};
}

template <class T>
inline T* MoveConstruct( T* pMemory, T&& src )
{
	HINT( pMemory != nullptr );
	return ::new( pMemory ) T{std::forward<T>( src )};
}

template <class T>
inline void Destruct( T* pMemory )
{
	HINT( pMemory != nullptr );

	pMemory->~T();

#ifdef _DEBUG
	memset( reinterpret_cast<void*>( pMemory ), 0xDD, sizeof(T) );
#endif
}

// The above will error when binding to a type of: foo(*)[] -- there is no provision in c++ for knowing how many objects
// to destruct without preserving the count and calling the necessary destructors.
template <class T, size_t N>
inline void Destruct( T (*pMemory)[N] )
{
	for ( size_t i = 0; i < N; i++ )
	{
		(pMemory[i])->~T();
	}

#ifdef _DEBUG
	memset( reinterpret_cast<void*>( pMemory ), 0xDD, sizeof(*pMemory) );
#endif
}


// dimhotepus: TF2 backport.
// misyl: Shamelessly nicked from Source 2 =)
//
//--------------------------------------------------------------------------------------------------
// RunCodeAtScopeExit
//
// Example:
//	int *x = new int;
//	RunCodeAtScopeExit( delete x )
//--------------------------------------------------------------------------------------------------
template <typename LambdaType>
class CScopeGuardLambdaImpl
{
public:
	explicit CScopeGuardLambdaImpl( LambdaType&& lambda ) : m_lambda( std::move( lambda ) ) { }
	~CScopeGuardLambdaImpl() { m_lambda(); }
private:
	LambdaType m_lambda;
};

//--------------------------------------------------------------------------------------------------
template <typename LambdaType>
CScopeGuardLambdaImpl< LambdaType > MakeScopeGuardLambda( LambdaType&& lambda )
{
	return CScopeGuardLambdaImpl< LambdaType >( std::move( lambda ) );
}

//--------------------------------------------------------------------------------------------------
#define RunLambdaAtScopeExit2( VarName, ... )		[[maybe_unused]] const auto VarName( MakeScopeGuardLambda( __VA_ARGS__ ) );
#define RunLambdaAtScopeExit( ... )					RunLambdaAtScopeExit2( UNIQUE_ID, __VA_ARGS__ )
#define RunCodeAtScopeExit( ... )					RunLambdaAtScopeExit( [&]() { __VA_ARGS__ ; } )


//
// GET_OUTER()
//
// A platform-independent way for a contained class to get a pointer to its
// owner. If you know a class is exclusively used in the context of some
// "outer" class, this is a much more space efficient way to get at the outer
// class than having the inner class store a pointer to it.
//
//	class COuter
//	{
//		class CInner // Note: this does not need to be a nested class to work
//		{
//			void PrintAddressOfOuter()
//			{
//				printf( "Outer is at 0x%x\n", GET_OUTER( COuter, m_Inner ) );
//			}
//		};
//
//		CInner m_Inner;
//		friend class CInner;
//	};

#define GET_OUTER( OuterType, OuterMember ) \
   ( ( OuterType * ) ( (uint8 *)this - offsetof( OuterType, OuterMember ) ) )


/*	TEMPLATE_FUNCTION_TABLE()

    (Note added to platform.h so platforms that correctly support templated
	 functions can handle portions as templated functions rather than wrapped
	 functions)

	Helps automate the process of creating an array of function
	templates that are all specialized by a single integer.
	This sort of thing is often useful in optimization work.

	For example, using TEMPLATE_FUNCTION_TABLE, this:

	TEMPLATE_FUNCTION_TABLE(int, Function, ( int blah, int blah ), 10)
	{
		return argument * argument;
	}

	is equivilent to the following:

	(NOTE: the function has to be wrapped in a class due to code
	generation bugs involved with directly specializing a function
	based on a constant.)

	template<int argument>
	class FunctionWrapper
	{
	public:
		int Function( int blah, int blah )
		{
			return argument*argument;
		}
	}

	using FunctionType = int (*)( int blah, int blah );

	class FunctionName
	{
	public:
		enum { count = 10 };
		FunctionType functions[10];
	};

	FunctionType FunctionName::functions[] =
	{
		FunctionWrapper<0>::Function,
		FunctionWrapper<1>::Function,
		FunctionWrapper<2>::Function,
		FunctionWrapper<3>::Function,
		FunctionWrapper<4>::Function,
		FunctionWrapper<5>::Function,
		FunctionWrapper<6>::Function,
		FunctionWrapper<7>::Function,
		FunctionWrapper<8>::Function,
		FunctionWrapper<9>::Function
	};
*/

PLATFORM_INTERFACE bool vtune( bool resume );


#define TEMPLATE_FUNCTION_TABLE(RETURN_TYPE, NAME, ARGS, COUNT)			\
																		\
using __Type_##NAME = RETURN_TYPE (FASTCALL *) ARGS;					\
																		\
template<const int nArgument>											\
struct __Function_##NAME												\
{																		\
	static RETURN_TYPE FASTCALL Run ARGS;								\
};																		\
																		\
template <const int i>													\
struct __MetaLooper_##NAME : __MetaLooper_##NAME<i-1>					\
{																		\
	__Type_##NAME func;													\
	inline __MetaLooper_##NAME() { func = __Function_##NAME<i>::Run; }	\
};																		\
																		\
template<>																\
struct __MetaLooper_##NAME<0>											\
{																		\
	__Type_##NAME func;													\
	inline __MetaLooper_##NAME() { func = __Function_##NAME<0>::Run; }	\
};																		\
																		\
class NAME																\
{																		\
private:																\
    static const __MetaLooper_##NAME<COUNT> m;							\
public:																	\
	enum { count = COUNT };												\
	static const __Type_##NAME* functions;								\
};																		\
const __MetaLooper_##NAME<COUNT> NAME::m;								\
const __Type_##NAME* NAME::functions = (__Type_##NAME*)&m;				\
template<const int nArgument>											\
RETURN_TYPE FASTCALL __Function_##NAME<nArgument>::Run ARGS

// dimhotepus: Deprecated, use inline code.
#define LOOP_INTERCHANGE(BOOLEAN, CODE)\
	if( (BOOLEAN) )\
	{\
		CODE;\
	} else\
	{\
		CODE;\
	}

// Watchdog timer support. Call Plat_BeginWatchdogTimer( nn ) to kick the timer off.  if you don't call
// Plat_EndWatchdogTimer within nn seconds, the program will kick off an exception.  This is for making
// sure that hung dedicated servers abort (and restart) instead of staying hung. Calling
// Plat_EndWatchdogTimer more than once or when there is no active watchdog is fine. Only does anything
// under linux right now. It should be possible to implement this functionality in windows via a
// thread, if desired.
PLATFORM_INTERFACE void Plat_BeginWatchdogTimer( int nSecs );
PLATFORM_INTERFACE void Plat_EndWatchdogTimer();
PLATFORM_INTERFACE int Plat_GetWatchdogTime();

using Plat_WatchDogHandlerFunction_t = void (*)();
PLATFORM_INTERFACE void Plat_SetWatchdogHandlerFunction( Plat_WatchDogHandlerFunction_t function );


//-----------------------------------------------------------------------------

#include "valve_on.h"

#if defined(TIER0_DLL_EXPORT)
extern "C" int V_tier0_stricmp(const char *s1, const char *s2 );
#undef stricmp
#undef strcmpi
#define stricmp(s1,s2) V_tier0_stricmp( s1, s2 )
#define strcmpi(s1,s2) V_tier0_stricmp( s1, s2 )
#endif


#endif  // !SE_PUBLIC_TIER0_PLATFORM_H_
