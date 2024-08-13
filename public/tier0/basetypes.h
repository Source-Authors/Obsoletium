// Copyright Valve Corporation, All rights reserved.

#ifndef TIER0_BASETYPES_H_
#define TIER0_BASETYPES_H_

#include "tier0/commonmacros.h"
#include "tier0/wchartypes.h"

#include <cstdint>
#include <limits>  // std::numeric_limits

#include "tier0/valve_off.h"

// This is a trick to get the DLL extension off the -D option on the command line.
#define DLLExtTokenPaste(x) #x
#define DLLExtTokenPaste2(x) DLLExtTokenPaste(x)
#define DLL_EXT_STRING DLLExtTokenPaste2( _DLL_EXT )

#include "tier0/protected_things.h"

// There's a different version of this file in the xbox codeline
// so the PC version built in the xbox branch includes things like 
// tickrate changes.
#include "tier0/xbox_codeline_defines.h"

#ifdef IN_XBOX_CODELINE
#define XBOX_CODELINE_ONLY()
#else
#define XBOX_CODELINE_ONLY() Error_Compiling_Code_Only_Valid_in_Xbox_Codeline
#endif

// cstdio
#ifndef NULL
#define NULL 0
#endif

#define ExecuteNTimes( nTimes, x )	   \
	{								   \
		static int executeCount__{0};  \
		if ( executeCount__ < nTimes ) \
		{							   \
			x;						   \
			++executeCount__;		   \
		}							   \
	}
#define ExecuteOnce( x )			ExecuteNTimes( 1, x )

template <typename T>
[[nodiscard]] constexpr inline T AlignValue( T val, uintptr_t alignment )
{
	return (T)( ( (uintptr_t)val + alignment - 1 ) & ~( alignment - 1 ) );
}


// Pad a number so it lies on an N byte boundary.
// So PAD_NUMBER(0,4) is 0 and PAD_NUMBER(1,4) is 4
template<typename T, typename Y>
[[nodiscard]] constexpr inline auto PAD_NUMBER( T number, Y boundary )
{
	return ((number + (boundary - 1)) / boundary) * boundary;
}

// In case this ever changes
#if !defined(M_PI) && !defined(HAVE_M_PI)
constexpr inline double M_PI{3.14159265358979323846};
#endif

#if !defined(M_PI_F) && !defined(HAVE_M_PI_F)
constexpr inline float M_PI_F{3.14159265358979323846f};
#endif

#include "tier0/valve_minmax_on.h"

#ifdef __cplusplus

// #define COMPILETIME_MAX and COMPILETIME_MIN for max/min in constant expressions
template <typename T, typename Y>
[[nodiscard]] constexpr inline auto COMPILETIME_MIN( const T& a, const Y& b )
{
	return a < b ? a : b;
}

template <typename T, typename Y>
[[nodiscard]] constexpr inline auto COMPILETIME_MAX( const T& a, const Y& b)
{
	return a > b ? a : b;
}

#ifndef MIN
template<typename T, typename Y>
[[nodiscard]] constexpr inline auto MIN( const T& a, const Y& b )
{
	return a < b ? a : b;
}
#endif

#ifndef MAX
template<typename T, typename Y>
[[nodiscard]] constexpr inline auto MAX( const T& a, const Y& b )
{
	return a > b ? a : b;
}
#endif

// This is the preferred clamp operator. Using the clamp macro can lead to
// unexpected side-effects or more expensive code. Even the clamp (all
// lower-case) function can generate more expensive code because of the
// mixed types involved.
template< typename T >
[[nodiscard]] constexpr inline T Clamp( T const &val, T const &minVal, T const &maxVal )
{
	if( val < minVal )
		return minVal;
	if( val > maxVal )
		return maxVal;
	
	return val;
}

// This is the preferred Min operator.
template< typename T >
[[nodiscard]] constexpr inline T Min( T const &val1, T const &val2 )
{
	return val1 < val2 ? val1 : val2;
}

// This is the preferred Max operator.
template< typename T >
[[nodiscard]] constexpr inline T Max( T const &val1, T const &val2 )
{
	return val1 > val2 ? val1 : val2;
}

#endif

#ifndef FALSE
#define FALSE 0
#define TRUE (!FALSE)
#endif


#ifndef DONT_DEFINE_BOOL // Needed for Cocoa stuff to compile.
using BOOL = int;
#endif

using qboolean = int;
using ULONG = unsigned long;
using byte = unsigned char;
using word = unsigned short;

#ifdef _WIN32
using dword = unsigned long;
#else
using dword = unsigned int;
#endif

#ifdef _WIN32
using ucs2 = wchar_t; // under windows wchar_t is ucs2
#else
using ucs2 = unsigned short;
#endif

enum ThreeState_t
{
	TRS_FALSE,
	TRS_TRUE,
	TRS_NONE,
};

using vec_t = float;

#if defined(__GNUC__)
#define fpmin __builtin_fminf
#define fpmax __builtin_fmaxf
#elif !defined(_X360)
#define fpmin min
#define fpmax max
#endif


//-----------------------------------------------------------------------------
// look for NANs, infinities, and underflows. 
// This assumes the ANSI/IEEE 754-1985 standard
//-----------------------------------------------------------------------------
// dimhotepus: Fix UB reintrepret casts.
//[[deprecated("UB in reinterpret cast. Use non-ref overload.")]] inline unsigned& FloatBits( vec_t& f )
//{
//	static_assert(alignof(vec_t) == alignof(unsigned));
//	static_assert(sizeof(f) == sizeof(unsigned));
//	return *reinterpret_cast<unsigned*>(&f);
//}
//
//[[deprecated("UB in reinterpret cast. Use non-ref overload.")]] inline unsigned const& FloatBits( vec_t const& f )
//{
//	static_assert(alignof(vec_t) == alignof(unsigned));
//	static_assert(sizeof(f) == sizeof(unsigned));
//	return *reinterpret_cast<unsigned const*>(&f);
//}

[[nodiscard]] inline unsigned FloatBits( vec_t f )
{
	unsigned r;
	static_assert(sizeof(r) == sizeof(f));
	memcpy( &r, &f, sizeof(unsigned) );
	return r;
}

[[nodiscard]] inline vec_t BitsToFloat( unsigned i )
{
	vec_t r;
	static_assert(sizeof(r) == sizeof(i));
	memcpy( &r, &i, sizeof(unsigned) );
	return r;
}

[[nodiscard]] inline bool IsNaN( vec_t f )
{
	static_assert(sizeof(f) == 4u);

	// NaN check.
	// std::isnan is slower on MSVC due to call to fpclassify.

	// & 0x7FFFFFFF to ignore sign bit.  The sign bit does not matter for NaN.
	// 0x7F800000 is the mask for NaN bits of the exponential field.

	// > 0x7F800000 because for NaN:
	// "The state/value of the remaining bits [...] are not defined by the standard
	// except that they must not be all zero."
	return (FloatBits(f) & 0x7FFFFFFFu) > 0x7F800000u;
}

[[nodiscard]] inline bool IsFinite( vec_t f )
{
	return !IsNaN(f);
}

[[nodiscard]] inline unsigned FloatAbsBits( vec_t f )
{
	return FloatBits(f) & 0x7FFFFFFFu; //-V112
}

// Given today's processors, I cannot think of any circumstance
// where bit tricks would be faster than fabs. henryg 8/16/2011
#ifdef _MSC_VER
#ifndef _In_
#define _In_
#endif
extern "C" _Check_return_ float fabsf(_In_ float);
#else
#include <cmath>
#endif

[[nodiscard]] inline float FloatMakeNegative( vec_t f )
{
	return -fabsf(f);
}

[[nodiscard]] inline float FloatMakePositive( vec_t f )
{
	return fabsf(f);
}

[[nodiscard]] inline float FloatNegate( vec_t f )
{
	return -f;
}


constexpr inline unsigned FLOAT32_NAN_BITS{0x7FC00000u};  // not a number!
constexpr inline float FLOAT32_NAN{std::numeric_limits<float>::quiet_NaN()};

constexpr inline vec_t VEC_T_NAN{std::numeric_limits<vec_t>::quiet_NaN()};

#ifndef NOTE_UNUSED
#define NOTE_UNUSED(x)	(void)(x)	// for pesky compiler / lint warnings
#endif

struct vrect_t
{
	int				x,y,width,height;
	vrect_t			*pnext;
};


//-----------------------------------------------------------------------------
// MaterialRect_t struct - used for DrawDebugText
//-----------------------------------------------------------------------------
struct Rect_t
{
    int x, y;
	int width, height;
};


//-----------------------------------------------------------------------------
// Interval, used by soundemittersystem + the game
//-----------------------------------------------------------------------------
struct interval_t
{
	float start;
	float range;
};


//-----------------------------------------------------------------------------
// Declares a type-safe handle type; you can't assign one handle to the next
//-----------------------------------------------------------------------------

// 32-bit pointer handles.

// Typesafe 8-bit and 16-bit handles.
template< class HandleType >
class CBaseIntHandle
{
public:
	
	[[nodiscard]] inline bool			operator==( const CBaseIntHandle &other ) const	{ return m_Handle == other.m_Handle; }
	[[nodiscard]] inline bool			operator!=( const CBaseIntHandle &other ) const	{ return m_Handle != other.m_Handle; }

	// Only the code that doles out these handles should use these functions.
	// Everyone else should treat them as a transparent type.
	[[nodiscard]] inline HandleType	GetHandleValue()					{ return m_Handle; }
	inline void			SetHandleValue( HandleType val )	{ m_Handle = val; }

	typedef HandleType	HANDLE_TYPE;

protected:

	HandleType	m_Handle;
};

template< class DummyType >
class CIntHandle16 : public CBaseIntHandle< unsigned short >
{
public:
	inline CIntHandle16() = default;

	[[nodiscard]] static inline	CIntHandle16<DummyType> MakeHandle( HANDLE_TYPE val )
	{
		return CIntHandle16<DummyType>( val );
	}

protected:
	inline CIntHandle16( HANDLE_TYPE val )
	{
		m_Handle = val;
	}
};


template< class DummyType >
class CIntHandle32 : public CBaseIntHandle< unsigned long >
{
public:
	inline			CIntHandle32() = default;

	[[nodiscard]] static inline	CIntHandle32<DummyType> MakeHandle( HANDLE_TYPE val )
	{
		return CIntHandle32<DummyType>( val );
	}

protected:
	inline CIntHandle32( HANDLE_TYPE val )
	{
		m_Handle = val;
	}
};


// NOTE: This macro is the same as windows uses; so don't change the guts of it
#define DECLARE_HANDLE_16BIT(name)	using name = CIntHandle16< struct name##__handle * >;
#define DECLARE_HANDLE_32BIT(name)	using name = CIntHandle32< struct name##__handle * >;

#define DECLARE_POINTER_HANDLE(name) struct name##__ { int unused; }; using name = struct name##__ *
#define FORWARD_DECLARE_HANDLE(name) using name = struct name##__ *

// @TODO: Find a better home for this
#if !defined(_STATIC_LINKED) && !defined(PUBLISH_DLL_SUBSYSTEM)
// for platforms built with dynamic linking, the dll interface does not need spoofing
#define PUBLISH_DLL_SUBSYSTEM()
#endif

#define UID_PREFIX generated_id_
#define UID_CAT1(a,c) a ## c
#define UID_CAT2(a,c) UID_CAT1(a,c)
#define EXPAND_CONCAT(a,c) UID_CAT1(a,c)
#ifdef _MSC_VER
#define UNIQUE_ID UID_CAT2(UID_PREFIX,__COUNTER__)
#else
#define UNIQUE_ID UID_CAT2(UID_PREFIX,__LINE__)
#endif

// this allows enumerations to be used as flags, and still remain type-safe!
#define DEFINE_ENUM_BITWISE_OPERATORS( Type ) \
	inline Type  operator|  ( Type  a, Type b ) { return static_cast<Type>( to_underlying( a ) | to_underlying( b ) ); } \
	inline Type  operator&  ( Type  a, Type b ) { return static_cast<Type>( to_underlying( a ) & to_underlying( b ) ); } \
	inline Type  operator^  ( Type  a, Type b ) { return static_cast<Type>( to_underlying( a ) ^ to_underlying( b ) ); } \
	inline Type  operator<< ( Type  a, int  b ) { return static_cast<Type>( to_underlying( a ) << b ); } \
	inline Type  operator>> ( Type  a, int  b ) { return static_cast<Type>( to_underlying( a ) >> b ); } \
	inline Type &operator|= ( Type &a, Type b ) { return a = a |  b; } \
	inline Type &operator&= ( Type &a, Type b ) { return a = a &  b; } \
	inline Type &operator^= ( Type &a, Type b ) { return a = a ^  b; } \
	inline Type &operator<<=( Type &a, int  b ) { return a = a << b; } \
	inline Type &operator>>=( Type &a, int  b ) { return a = a >> b; } \
	inline Type  operator~( Type a ) { return static_cast<Type>( ~to_underlying( a ) ); }

// defines increment/decrement operators for enums for easy iteration
#define DEFINE_ENUM_INCREMENT_OPERATORS( Type ) \
	inline Type &operator++( Type &a      ) { return a = static_cast<Type>( to_underlying( a ) + 1 ); } \
	inline Type &operator--( Type &a      ) { return a = static_cast<Type>( to_underlying( a ) - 1 ); } \
	inline Type  operator++( Type &a, int ) { Type t{a}; ++a; return t; } \
	inline Type  operator--( Type &a, int ) { Type t{a}; --a; return t; }

#include "tier0/valve_on.h"

#endif  // TIER0_BASETYPES_H_
