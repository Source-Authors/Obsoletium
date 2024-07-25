//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: - defines SIMD "structure of arrays" classes and functions.
//
//===========================================================================//
#ifndef SSEMATH_H
#define SSEMATH_H

#include <DirectXMath.h>
#include <xmmintrin.h>

#include <mathlib/vector.h>
#include <mathlib/mathlib.h>

#define USE_STDC_FOR_SIMD 0

#if (USE_STDC_FOR_SIMD == 0)
#define _SSE1 1
#endif

// I thought about defining a class/union for the SIMD packed floats instead of using fltx4,
// but decided against it because (a) the nature of SIMD code which includes comparisons is to blur
// the relationship between packed floats and packed integer types and (b) not sure that the
// compiler would handle generating good code for the intrinsics.

#if USE_STDC_FOR_SIMD

typedef union
{
	float  m128_f32[4];
	uint32 m128_u32[4];
} fltx4;

typedef fltx4 i32x4;
typedef fltx4 u32x4;

#else

typedef DirectX::XMVECTOR fltx4;
typedef DirectX::XMVECTOR i32x4;
typedef DirectX::XMVECTOR u32x4;

#endif

// The FLTX4 type is a fltx4 used as a parameter to a function.
// On the 360, the best way to do this is pass-by-copy on the registers.
// On the PC, the best way is to pass by const reference. 
using FLTX4 = const fltx4 &;

// A 16-byte aligned int32 datastructure
// (for use when writing out fltx4's as SIGNED
// ints).
// dimhotepus: Fix aligned alloc.
struct alignas(16) intx4 : public CAlignedNewDelete<16> 
{
	int32 m_i32[4];

	inline int & operator[](int which)
	{
		Assert( which >= 0 && which < 4 );
		return m_i32[which];
	}

	inline const int & operator[](int which) const
	{
		Assert( which >= 0 && which < 4 );
		return m_i32[which];
	}

	inline int32 *Base()
	{
		return m_i32;
	}

	inline const int32 *Base() const
	{
		return m_i32;
	}

	inline bool operator==(const intx4 &other) const
	{
		return !DirectX::XMVector4NotEqualInt
		(
			DirectX::XMLoadInt4A( reinterpret_cast<uint32_t const*>( Base() ) ),
			DirectX::XMLoadInt4A( reinterpret_cast<uint32_t const*>( other.Base() ) )
		);
	}
};

FORCEINLINE void TestVPUFlags() {}

// useful constants in SIMD packed float format:
// (note: some of these aren't stored on the 360, 
// but are manufactured directly in one or two 
// instructions, saving a load and possible L2
// miss.)
extern const fltx4 Four_Zeros;									// 0 0 0 0
extern const fltx4 Four_Ones;									// 1 1 1 1
extern const fltx4 Four_Twos;									// 2 2 2 2
extern const fltx4 Four_Threes;									// 3 3 3 3
extern const fltx4 Four_Fours;									// guess.
extern const fltx4 Four_Point225s;								// .225 .225 .225 .225
extern const fltx4 Four_PointFives;								// .5 .5 .5 .5
extern const fltx4 Four_Epsilons;								// FLT_EPSILON FLT_EPSILON FLT_EPSILON FLT_EPSILON
extern const fltx4 Four_2ToThe21s;								// (1<<21)..
extern const fltx4 Four_2ToThe22s;								// (1<<22)..
extern const fltx4 Four_2ToThe23s;								// (1<<23)..
extern const fltx4 Four_2ToThe24s;								// (1<<24)..
extern const fltx4 Four_Origin;									// 0 0 0 1 (origin point, like vr0 on the PS2)
extern const fltx4 Four_NegativeOnes;							// -1 -1 -1 -1 
extern const fltx4 Four_FLT_MAX;								// FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX
extern const fltx4 Four_Negative_FLT_MAX;						// -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX
extern const fltx4 g_SIMD_0123;									// 0 1 2 3 as float

// external aligned integer constants
alignas(16) extern const uint32 g_SIMD_clear_signmask[];		// 0x7fffffff x 4
alignas(16) extern const uint32 g_SIMD_signmask[];				// 0x80000000 x 4
alignas(16) extern const uint32 g_SIMD_lsbmask[];				// 0xfffffffe x 4
alignas(16) extern const uint32 g_SIMD_clear_wmask[];			// -1 -1 -1 0
alignas(16) extern const uint32 g_SIMD_ComponentMask[4][4];		// [0xFFFFFFFF 0 0 0], [0 0xFFFFFFFF 0 0], [0 0 0xFFFFFFFF 0], [0 0 0 0xFFFFFFFF]
alignas(16) extern const uint32 g_SIMD_AllOnesMask[];			// ~0,~0,~0,~0
alignas(16) extern const uint32 g_SIMD_Low16BitsMask[];			// 0xffff x 4

// this mask is used for skipping the tail of things. If you have N elements in an array, and wish
// to mask out the tail, g_SIMD_SkipTailMask[N & 3] what you want to use for the last iteration.
alignas(16) extern const uint32 g_SIMD_SkipTailMask[4][4];

// Define prefetch macros.
// The characteristics of cache and prefetch are completely 
// different between the different platforms, so you DO NOT
// want to just define one macro that maps to every platform
// intrinsic under the hood -- you need to prefetch at different
// intervals between x86 and PPC, for example, and that is
// a higher level code change. 
// On the other hand, I'm tired of typing #ifdef _X360
// all over the place, so this is just a nop on Intel, PS3.
#define PREFETCH360(x,y) // nothing

#if USE_STDC_FOR_SIMD

//---------------------------------------------------------------------
// Standard C (fallback/Linux) implementation (only there for compat - slow)
//---------------------------------------------------------------------

FORCEINLINE float SubFloat( const fltx4 & a, int idx )
{
	return a.m128_f32[ idx ];
}

FORCEINLINE float & SubFloat( fltx4 & a, int idx )
{
	return a.m128_f32[idx];
}

FORCEINLINE uint32 SubInt( const fltx4 & a, int idx )
{
	return a.m128_u32[idx];
}

FORCEINLINE uint32 & SubInt( fltx4 & a, int idx )
{
	return a.m128_u32[idx];
}

FORCEINLINE fltx4 LoadZeroSIMD( void )
{
	return Four_Zeros;
}

FORCEINLINE fltx4 LoadOneSIMD( void )
{
	return Four_Ones;
}

FORCEINLINE fltx4 SplatXSIMD( const fltx4 & a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = SubFloat( a, 0 );
	SubFloat( retVal, 1 ) = SubFloat( a, 0 );
	SubFloat( retVal, 2 ) = SubFloat( a, 0 );
	SubFloat( retVal, 3 ) = SubFloat( a, 0 );
	return retVal;
}

FORCEINLINE fltx4 SplatYSIMD( fltx4 a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = SubFloat( a, 1 );
	SubFloat( retVal, 1 ) = SubFloat( a, 1 );
	SubFloat( retVal, 2 ) = SubFloat( a, 1 );
	SubFloat( retVal, 3 ) = SubFloat( a, 1 );
	return retVal;
}

FORCEINLINE fltx4 SplatZSIMD( fltx4 a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = SubFloat( a, 2 );
	SubFloat( retVal, 1 ) = SubFloat( a, 2 );
	SubFloat( retVal, 2 ) = SubFloat( a, 2 );
	SubFloat( retVal, 3 ) = SubFloat( a, 2 );
	return retVal;
}

FORCEINLINE fltx4 SplatWSIMD( fltx4 a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = SubFloat( a, 3 );
	SubFloat( retVal, 1 ) = SubFloat( a, 3 );
	SubFloat( retVal, 2 ) = SubFloat( a, 3 );
	SubFloat( retVal, 3 ) = SubFloat( a, 3 );
	return retVal;
}

FORCEINLINE fltx4 SetXSIMD( const fltx4& a, const fltx4& x )
{
	fltx4 result = a;
	SubFloat( result, 0 ) = SubFloat( x, 0 );
	return result;
}

FORCEINLINE fltx4 SetYSIMD( const fltx4& a, const fltx4& y )
{
	fltx4 result = a;
	SubFloat( result, 1 ) = SubFloat( y, 1 );
	return result;
}

FORCEINLINE fltx4 SetZSIMD( const fltx4& a, const fltx4& z )
{
	fltx4 result = a;
	SubFloat( result, 2 ) = SubFloat( z, 2 );
	return result;
}

FORCEINLINE fltx4 SetWSIMD( const fltx4& a, const fltx4& w )
{
	fltx4 result = a;
	SubFloat( result, 3 ) = SubFloat( w, 3 );
	return result;
}

FORCEINLINE fltx4 SetComponentSIMD( const fltx4& a, int nComponent, float flValue )
{
	fltx4 result = a;
	SubFloat( result, nComponent ) = flValue;
	return result;
}

// a b c d -> b c d a
FORCEINLINE fltx4 RotateLeft( const fltx4 & a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = SubFloat( a, 1 );
	SubFloat( retVal, 1 ) = SubFloat( a, 2 );
	SubFloat( retVal, 2 ) = SubFloat( a, 3 );
	SubFloat( retVal, 3 ) = SubFloat( a, 0 );
	return retVal;
}

// a b c d -> c d a b
FORCEINLINE fltx4 RotateLeft2( const fltx4 & a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = SubFloat( a, 2 );
	SubFloat( retVal, 1 ) = SubFloat( a, 3 );
	SubFloat( retVal, 2 ) = SubFloat( a, 0 );
	SubFloat( retVal, 3 ) = SubFloat( a, 1 );
	return retVal;
}

#define BINOP(op) 														\
	fltx4 retVal;                                          				\
	SubFloat( retVal, 0 ) = ( SubFloat( a, 0 ) op SubFloat( b, 0 ) );	\
	SubFloat( retVal, 1 ) = ( SubFloat( a, 1 ) op SubFloat( b, 1 ) );	\
	SubFloat( retVal, 2 ) = ( SubFloat( a, 2 ) op SubFloat( b, 2 ) );	\
	SubFloat( retVal, 3 ) = ( SubFloat( a, 3 ) op SubFloat( b, 3 ) );	\
    return retVal;

#define IBINOP(op) 														\
	fltx4 retVal;														\
	SubInt( retVal, 0 ) = ( SubInt( a, 0 ) op SubInt ( b, 0 ) );		\
	SubInt( retVal, 1 ) = ( SubInt( a, 1 ) op SubInt ( b, 1 ) );		\
	SubInt( retVal, 2 ) = ( SubInt( a, 2 ) op SubInt ( b, 2 ) );		\
	SubInt( retVal, 3 ) = ( SubInt( a, 3 ) op SubInt ( b, 3 ) );		\
    return retVal;

FORCEINLINE fltx4 AddSIMD( const fltx4 & a, const fltx4 & b )
{
	BINOP(+);
}

FORCEINLINE fltx4 SubSIMD( const fltx4 & a, const fltx4 & b )				// a-b
{
	BINOP(-);
};

FORCEINLINE fltx4 MulSIMD( const fltx4 & a, const fltx4 & b )				// a*b
{
	BINOP(*);
}

FORCEINLINE fltx4 DivSIMD( const fltx4 & a, const fltx4 & b )				// a/b
{
	BINOP(/);
}


FORCEINLINE fltx4 MaddSIMD( const fltx4 & a, const fltx4 & b, const fltx4 & c )				// a*b + c
{
	return AddSIMD( MulSIMD(a,b), c );
}

FORCEINLINE fltx4 MsubSIMD( const fltx4 & a, const fltx4 & b, const fltx4 & c )				// c - a*b
{
	return SubSIMD( c, MulSIMD(a,b) );
};


FORCEINLINE fltx4 SinSIMD( const fltx4 &radians )
{
	fltx4 result;
	SubFloat( result, 0 ) = sin( SubFloat( radians, 0 ) );
	SubFloat( result, 1 ) = sin( SubFloat( radians, 1 ) );
	SubFloat( result, 2 ) = sin( SubFloat( radians, 2 ) );
	SubFloat( result, 3 ) = sin( SubFloat( radians, 3 ) );
	return result;
}

FORCEINLINE fltx4 CosSIMD( const fltx4 &radians )
{
	fltx4 result;
	SubFloat( result, 0 ) = cos( SubFloat( radians, 0 ) );
	SubFloat( result, 1 ) = cos( SubFloat( radians, 1 ) );
	SubFloat( result, 2 ) = cos( SubFloat( radians, 2 ) );
	SubFloat( result, 3 ) = cos( SubFloat( radians, 3 ) );
	return result;
}

FORCEINLINE void SinCos3SIMD( fltx4 &sine, fltx4 &cosine, const fltx4 &radians )
{
	SinCos( SubFloat( radians, 0 ), &SubFloat( sine, 0 ), &SubFloat( cosine, 0 ) );
	SinCos( SubFloat( radians, 1 ), &SubFloat( sine, 1 ), &SubFloat( cosine, 1 ) );
	SinCos( SubFloat( radians, 2 ), &SubFloat( sine, 2 ), &SubFloat( cosine, 2 ) );
}

FORCEINLINE void SinCosSIMD( fltx4 &sine, fltx4 &cosine, const fltx4 &radians )
{
	SinCos( SubFloat( radians, 0 ), &SubFloat( sine, 0 ), &SubFloat( cosine, 0 ) );
	SinCos( SubFloat( radians, 1 ), &SubFloat( sine, 1 ), &SubFloat( cosine, 1 ) );
	SinCos( SubFloat( radians, 2 ), &SubFloat( sine, 2 ), &SubFloat( cosine, 2 ) );
	SinCos( SubFloat( radians, 3 ), &SubFloat( sine, 3 ), &SubFloat( cosine, 3 ) );
}

FORCEINLINE fltx4 ArcSinSIMD( const fltx4 &sine )
{
	fltx4 result;
	SubFloat( result, 0 ) = asin( SubFloat( sine, 0 ) );
	SubFloat( result, 1 ) = asin( SubFloat( sine, 1 ) );
	SubFloat( result, 2 ) = asin( SubFloat( sine, 2 ) );
	SubFloat( result, 3 ) = asin( SubFloat( sine, 3 ) );
	return result;
}

FORCEINLINE fltx4 ArcCosSIMD( const fltx4 &cs )
{
	fltx4 result;
	SubFloat( result, 0 ) = acos( SubFloat( cs, 0 ) );
	SubFloat( result, 1 ) = acos( SubFloat( cs, 1 ) );
	SubFloat( result, 2 ) = acos( SubFloat( cs, 2 ) );
	SubFloat( result, 3 ) = acos( SubFloat( cs, 3 ) );
	return result;
}

// tan^1(a/b) .. ie, pass sin in as a and cos in as b
FORCEINLINE fltx4 ArcTan2SIMD( const fltx4 &a, const fltx4 &b )
{
	fltx4 result;
	SubFloat( result, 0 ) = atan2( SubFloat( a, 0 ), SubFloat( b, 0 ) );
	SubFloat( result, 1 ) = atan2( SubFloat( a, 1 ), SubFloat( b, 1 ) );
	SubFloat( result, 2 ) = atan2( SubFloat( a, 2 ), SubFloat( b, 2 ) );
	SubFloat( result, 3 ) = atan2( SubFloat( a, 3 ), SubFloat( b, 3 ) );
	return result;
}

// arctan(a)
FORCEINLINE fltx4 ArcTanSIMD( const fltx4 &a )
{
	fltx4 result;
	SubFloat( result, 0 ) = atan( SubFloat( a, 0 ) );
	SubFloat( result, 1 ) = atan( SubFloat( a, 1 ) );
	SubFloat( result, 2 ) = atan( SubFloat( a, 2 ) );
	SubFloat( result, 3 ) = atan( SubFloat( a, 3 ) );
	return result;
}

FORCEINLINE fltx4 MaxSIMD( const fltx4 & a, const fltx4 & b )				// max(a,b)
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = max( SubFloat( a, 0 ), SubFloat( b, 0 ) );
	SubFloat( retVal, 1 ) = max( SubFloat( a, 1 ), SubFloat( b, 1 ) );
	SubFloat( retVal, 2 ) = max( SubFloat( a, 2 ), SubFloat( b, 2 ) );
	SubFloat( retVal, 3 ) = max( SubFloat( a, 3 ), SubFloat( b, 3 ) );
	return retVal;
}

FORCEINLINE fltx4 MinSIMD( const fltx4 & a, const fltx4 & b )				// min(a,b)
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = min( SubFloat( a, 0 ), SubFloat( b, 0 ) );
	SubFloat( retVal, 1 ) = min( SubFloat( a, 1 ), SubFloat( b, 1 ) );
	SubFloat( retVal, 2 ) = min( SubFloat( a, 2 ), SubFloat( b, 2 ) );
	SubFloat( retVal, 3 ) = min( SubFloat( a, 3 ), SubFloat( b, 3 ) );
	return retVal;
}

FORCEINLINE fltx4 AndSIMD( const fltx4 & a, const fltx4 & b )				// a & b
{
	IBINOP(&);
}

FORCEINLINE fltx4 AndNotSIMD( const fltx4 & a, const fltx4 & b )			// ~a & b
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ~SubInt( a, 0 ) & SubInt( b, 0 );
	SubInt( retVal, 1 ) = ~SubInt( a, 1 ) & SubInt( b, 1 );
	SubInt( retVal, 2 ) = ~SubInt( a, 2 ) & SubInt( b, 2 );
	SubInt( retVal, 3 ) = ~SubInt( a, 3 ) & SubInt( b, 3 );
	return retVal;
}

FORCEINLINE fltx4 XorSIMD( const fltx4 & a, const fltx4 & b )				// a ^ b
{
	IBINOP(^);
}

FORCEINLINE fltx4 OrSIMD( const fltx4 & a, const fltx4 & b )				// a | b
{
	IBINOP(|);
}

FORCEINLINE fltx4 NegSIMD(const fltx4 &a) // negate: -a
{
	fltx4 retval;
	SubFloat( retval, 0 ) = -SubFloat( a, 0 );
	SubFloat( retval, 1 ) = -SubFloat( a, 1 );
	SubFloat( retval, 2 ) = -SubFloat( a, 2 );
	SubFloat( retval, 3 ) = -SubFloat( a, 3 );

	return retval;
}

FORCEINLINE bool IsAllZeros( const fltx4 & a )								// all floats of a zero?
{
	return	( SubFloat( a, 0 ) == 0.0 ) &&
		( SubFloat( a, 1 ) == 0.0 ) &&
		( SubFloat( a, 2 ) == 0.0 ) &&
		( SubFloat( a, 3 ) == 0.0 ) ;
}


// for branching when a.xyzw > b.xyzw
FORCEINLINE bool IsAllGreaterThan( const fltx4 &a, const fltx4 &b )
{
	return	SubFloat(a,0) > SubFloat(b,0) &&
		SubFloat(a,1) > SubFloat(b,1) &&
		SubFloat(a,2) > SubFloat(b,2) &&
		SubFloat(a,3) > SubFloat(b,3);
}

// for branching when a.xyzw >= b.xyzw
FORCEINLINE bool IsAllGreaterThanOrEq( const fltx4 &a, const fltx4 &b )
{
	return	SubFloat(a,0) >= SubFloat(b,0) &&
		SubFloat(a,1) >= SubFloat(b,1) &&
		SubFloat(a,2) >= SubFloat(b,2) &&
		SubFloat(a,3) >= SubFloat(b,3);
}

// For branching if all a.xyzw == b.xyzw
FORCEINLINE bool IsAllEqual( const fltx4 & a, const fltx4 & b )
{
	return	SubFloat(a,0) == SubFloat(b,0) &&
		SubFloat(a,1) == SubFloat(b,1) &&
		SubFloat(a,2) == SubFloat(b,2) &&
		SubFloat(a,3) == SubFloat(b,3);
}

FORCEINLINE int TestSignSIMD( const fltx4 & a )								// mask of which floats have the high bit set
{
	int nRet = 0;

	nRet |= ( SubInt( a, 0 ) & 0x80000000 ) >> 31; // sign(x) -> bit 0
	nRet |= ( SubInt( a, 1 ) & 0x80000000 ) >> 30; // sign(y) -> bit 1
	nRet |= ( SubInt( a, 2 ) & 0x80000000 ) >> 29; // sign(z) -> bit 2
	nRet |= ( SubInt( a, 3 ) & 0x80000000 ) >> 28; // sign(w) -> bit 3

	return nRet;
}

FORCEINLINE bool IsAnyNegative( const fltx4 & a )							// (a.x < 0) || (a.y < 0) || (a.z < 0) || (a.w < 0)
{
	return (0 != TestSignSIMD( a ));
}

FORCEINLINE fltx4 CmpEqSIMD( const fltx4 & a, const fltx4 & b )				// (a==b) ? ~0:0
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ( SubFloat( a, 0 ) == SubFloat( b, 0 )) ? ~0 : 0;
	SubInt( retVal, 1 ) = ( SubFloat( a, 1 ) == SubFloat( b, 1 )) ? ~0 : 0;
	SubInt( retVal, 2 ) = ( SubFloat( a, 2 ) == SubFloat( b, 2 )) ? ~0 : 0;
	SubInt( retVal, 3 ) = ( SubFloat( a, 3 ) == SubFloat( b, 3 )) ? ~0 : 0;
	return retVal;
}

FORCEINLINE fltx4 CmpGtSIMD( const fltx4 & a, const fltx4 & b )				// (a>b) ? ~0:0
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ( SubFloat( a, 0 ) > SubFloat( b, 0 )) ? ~0 : 0;
	SubInt( retVal, 1 ) = ( SubFloat( a, 1 ) > SubFloat( b, 1 )) ? ~0 : 0;
	SubInt( retVal, 2 ) = ( SubFloat( a, 2 ) > SubFloat( b, 2 )) ? ~0 : 0;
	SubInt( retVal, 3 ) = ( SubFloat( a, 3 ) > SubFloat( b, 3 )) ? ~0 : 0;
	return retVal;
}

FORCEINLINE fltx4 CmpGeSIMD( const fltx4 & a, const fltx4 & b )				// (a>=b) ? ~0:0
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ( SubFloat( a, 0 ) >= SubFloat( b, 0 )) ? ~0 : 0;
	SubInt( retVal, 1 ) = ( SubFloat( a, 1 ) >= SubFloat( b, 1 )) ? ~0 : 0;
	SubInt( retVal, 2 ) = ( SubFloat( a, 2 ) >= SubFloat( b, 2 )) ? ~0 : 0;
	SubInt( retVal, 3 ) = ( SubFloat( a, 3 ) >= SubFloat( b, 3 )) ? ~0 : 0;
	return retVal;
}

FORCEINLINE fltx4 CmpLtSIMD( const fltx4 & a, const fltx4 & b )				// (a<b) ? ~0:0
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ( SubFloat( a, 0 ) < SubFloat( b, 0 )) ? ~0 : 0;
	SubInt( retVal, 1 ) = ( SubFloat( a, 1 ) < SubFloat( b, 1 )) ? ~0 : 0;
	SubInt( retVal, 2 ) = ( SubFloat( a, 2 ) < SubFloat( b, 2 )) ? ~0 : 0;
	SubInt( retVal, 3 ) = ( SubFloat( a, 3 ) < SubFloat( b, 3 )) ? ~0 : 0;
	return retVal;
}

FORCEINLINE fltx4 CmpLeSIMD( const fltx4 & a, const fltx4 & b )				// (a<=b) ? ~0:0
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ( SubFloat( a, 0 ) <= SubFloat( b, 0 )) ? ~0 : 0;
	SubInt( retVal, 1 ) = ( SubFloat( a, 1 ) <= SubFloat( b, 1 )) ? ~0 : 0;
	SubInt( retVal, 2 ) = ( SubFloat( a, 2 ) <= SubFloat( b, 2 )) ? ~0 : 0;
	SubInt( retVal, 3 ) = ( SubFloat( a, 3 ) <= SubFloat( b, 3 )) ? ~0 : 0;
	return retVal;
}

FORCEINLINE fltx4 CmpInBoundsSIMD( const fltx4 & a, const fltx4 & b )		// (a <= b && a >= -b) ? ~0 : 0
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ( SubFloat( a, 0 ) <= SubFloat( b, 0 ) && SubFloat( a, 0 ) >= -SubFloat( b, 0 ) ) ? ~0 : 0;
	SubInt( retVal, 1 ) = ( SubFloat( a, 1 ) <= SubFloat( b, 1 ) && SubFloat( a, 1 ) >= -SubFloat( b, 1 ) ) ? ~0 : 0;
	SubInt( retVal, 2 ) = ( SubFloat( a, 2 ) <= SubFloat( b, 2 ) && SubFloat( a, 2 ) >= -SubFloat( b, 2 ) ) ? ~0 : 0;
	SubInt( retVal, 3 ) = ( SubFloat( a, 3 ) <= SubFloat( b, 3 ) && SubFloat( a, 3 ) >= -SubFloat( b, 3 ) ) ? ~0 : 0;
	return retVal;
}


FORCEINLINE fltx4 MaskedAssign( const fltx4 & ReplacementMask, const fltx4 & NewValue, const fltx4 & OldValue )
{
	return OrSIMD(
		AndSIMD( ReplacementMask, NewValue ),
		AndNotSIMD( ReplacementMask, OldValue ) );
}

FORCEINLINE fltx4 ReplicateX4( float flValue )					//  a,a,a,a
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = flValue;
	SubFloat( retVal, 1 ) = flValue;
	SubFloat( retVal, 2 ) = flValue;
	SubFloat( retVal, 3 ) = flValue;
	return retVal;
}

/// replicate a single 32 bit integer value to all 4 components of an m128
FORCEINLINE fltx4 ReplicateIX4( int nValue )
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = nValue;
	SubInt( retVal, 1 ) = nValue;
	SubInt( retVal, 2 ) = nValue;
	SubInt( retVal, 3 ) = nValue;
	return retVal;

}

// Round towards positive infinity
FORCEINLINE fltx4 CeilSIMD( const fltx4 &a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = ceil( SubFloat( a, 0 ) );
	SubFloat( retVal, 1 ) = ceil( SubFloat( a, 1 ) );
	SubFloat( retVal, 2 ) = ceil( SubFloat( a, 2 ) );
	SubFloat( retVal, 3 ) = ceil( SubFloat( a, 3 ) );
	return retVal;

}

// Round towards negative infinity
FORCEINLINE fltx4 FloorSIMD( const fltx4 &a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = floor( SubFloat( a, 0 ) );
	SubFloat( retVal, 1 ) = floor( SubFloat( a, 1 ) );
	SubFloat( retVal, 2 ) = floor( SubFloat( a, 2 ) );
	SubFloat( retVal, 3 ) = floor( SubFloat( a, 3 ) );
	return retVal;

}

FORCEINLINE fltx4 SqrtEstSIMD( const fltx4 & a )				// sqrt(a), more or less
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = sqrt( SubFloat( a, 0 ) );
	SubFloat( retVal, 1 ) = sqrt( SubFloat( a, 1 ) );
	SubFloat( retVal, 2 ) = sqrt( SubFloat( a, 2 ) );
	SubFloat( retVal, 3 ) = sqrt( SubFloat( a, 3 ) );
	return retVal;
}

FORCEINLINE fltx4 SqrtSIMD( const fltx4 & a )					// sqrt(a)
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = sqrt( SubFloat( a, 0 ) );
	SubFloat( retVal, 1 ) = sqrt( SubFloat( a, 1 ) );
	SubFloat( retVal, 2 ) = sqrt( SubFloat( a, 2 ) );
	SubFloat( retVal, 3 ) = sqrt( SubFloat( a, 3 ) );
	return retVal;
}

FORCEINLINE fltx4 ReciprocalSqrtEstSIMD( const fltx4 & a )		// 1/sqrt(a), more or less
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / sqrt( SubFloat( a, 0 ) );
	SubFloat( retVal, 1 ) = 1.0 / sqrt( SubFloat( a, 1 ) );
	SubFloat( retVal, 2 ) = 1.0 / sqrt( SubFloat( a, 2 ) );
	SubFloat( retVal, 3 ) = 1.0 / sqrt( SubFloat( a, 3 ) );
	return retVal;
}

FORCEINLINE fltx4 ReciprocalSqrtEstSaturateSIMD( const fltx4 & a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / sqrt( SubFloat( a, 0 ) != 0.0f ? SubFloat( a, 0 ) : FLT_EPSILON );
	SubFloat( retVal, 1 ) = 1.0 / sqrt( SubFloat( a, 1 ) != 0.0f ? SubFloat( a, 1 ) : FLT_EPSILON );
	SubFloat( retVal, 2 ) = 1.0 / sqrt( SubFloat( a, 2 ) != 0.0f ? SubFloat( a, 2 ) : FLT_EPSILON );
	SubFloat( retVal, 3 ) = 1.0 / sqrt( SubFloat( a, 3 ) != 0.0f ? SubFloat( a, 3 ) : FLT_EPSILON );
	return retVal;
}

FORCEINLINE fltx4 ReciprocalSqrtSIMD( const fltx4 & a )			// 1/sqrt(a)
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / sqrt( SubFloat( a, 0 ) );
	SubFloat( retVal, 1 ) = 1.0 / sqrt( SubFloat( a, 1 ) );
	SubFloat( retVal, 2 ) = 1.0 / sqrt( SubFloat( a, 2 ) );
	SubFloat( retVal, 3 ) = 1.0 / sqrt( SubFloat( a, 3 ) );
	return retVal;
}

FORCEINLINE fltx4 ReciprocalEstSIMD( const fltx4 & a )			// 1/a, more or less
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / SubFloat( a, 0 );
	SubFloat( retVal, 1 ) = 1.0 / SubFloat( a, 1 );
	SubFloat( retVal, 2 ) = 1.0 / SubFloat( a, 2 );
	SubFloat( retVal, 3 ) = 1.0 / SubFloat( a, 3 );
	return retVal;
}

FORCEINLINE fltx4 ReciprocalSIMD( const fltx4 & a )				// 1/a
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / SubFloat( a, 0 );
	SubFloat( retVal, 1 ) = 1.0 / SubFloat( a, 1 );
	SubFloat( retVal, 2 ) = 1.0 / SubFloat( a, 2 );
	SubFloat( retVal, 3 ) = 1.0 / SubFloat( a, 3 );
	return retVal;
}

/// 1/x for all 4 values.
/// 1/0 will result in a big but NOT infinite result
FORCEINLINE fltx4 ReciprocalEstSaturateSIMD( const fltx4 & a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / (SubFloat( a, 0 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 0 ));
	SubFloat( retVal, 1 ) = 1.0 / (SubFloat( a, 1 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 1 ));
	SubFloat( retVal, 2 ) = 1.0 / (SubFloat( a, 2 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 2 ));
	SubFloat( retVal, 3 ) = 1.0 / (SubFloat( a, 3 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 3 ));
	return retVal;
}

FORCEINLINE fltx4 ReciprocalSaturateSIMD( const fltx4 & a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / (SubFloat( a, 0 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 0 ));
	SubFloat( retVal, 1 ) = 1.0 / (SubFloat( a, 1 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 1 ));
	SubFloat( retVal, 2 ) = 1.0 / (SubFloat( a, 2 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 2 ));
	SubFloat( retVal, 3 ) = 1.0 / (SubFloat( a, 3 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 3 ));
	return retVal;
}

// 2^x for all values (the antilog)
FORCEINLINE fltx4 ExpSIMD( const fltx4 &toPower )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = powf( 2, SubFloat(toPower, 0) );
	SubFloat( retVal, 1 ) = powf( 2, SubFloat(toPower, 1) );
	SubFloat( retVal, 2 ) = powf( 2, SubFloat(toPower, 2) );
	SubFloat( retVal, 3 ) = powf( 2, SubFloat(toPower, 3) );

	return retVal;
}

FORCEINLINE fltx4 Dot3SIMD( const fltx4 &a, const fltx4 &b )
{
	float flDot = SubFloat( a, 0 ) * SubFloat( b, 0 ) +
		SubFloat( a, 1 ) * SubFloat( b, 1 ) + 
		SubFloat( a, 2 ) * SubFloat( b, 2 );
	return ReplicateX4( flDot );
}

FORCEINLINE fltx4 Dot4SIMD( const fltx4 &a, const fltx4 &b )
{
	float flDot = SubFloat( a, 0 ) * SubFloat( b, 0 ) +
		SubFloat( a, 1 ) * SubFloat( b, 1 ) + 
		SubFloat( a, 2 ) * SubFloat( b, 2 ) +
		SubFloat( a, 3 ) * SubFloat( b, 3 );
	return ReplicateX4( flDot );
}

// Clamps the components of a vector to a specified minimum and maximum range.
FORCEINLINE fltx4 ClampVectorSIMD( FLTX4 in, FLTX4 min, FLTX4 max)
{
	return MaxSIMD( min, MinSIMD( max, in ) );
}

// Squelch the w component of a vector to +0.0.
// Most efficient when you say a = SetWToZeroSIMD(a) (avoids a copy)
FORCEINLINE fltx4 SetWToZeroSIMD( const fltx4 & a )
{
	fltx4 retval;
	retval = a;
	SubFloat( retval, 0 ) = 0;
	return retval;
}

FORCEINLINE fltx4 LoadUnalignedSIMD( const void *pSIMD )
{
	return *( reinterpret_cast< const fltx4 *> ( pSIMD ) );
}

FORCEINLINE fltx4 LoadUnaligned3SIMD( const void *pSIMD )
{
	return *( reinterpret_cast< const fltx4 *> ( pSIMD ) );
}

FORCEINLINE fltx4 LoadAlignedSIMD( const void *pSIMD )
{
	return *( reinterpret_cast< const fltx4 *> ( pSIMD ) );
}

// for the transitional class -- load a 3-by VectorAligned and squash its w component
FORCEINLINE fltx4 LoadAlignedSIMD( const VectorAligned & pSIMD )
{
	fltx4 retval = LoadAlignedSIMD(pSIMD.Base());
	// squelch w
	SubInt( retval, 3 ) = 0;
	return retval;
}

FORCEINLINE void StoreAlignedSIMD( float *pSIMD, const fltx4 & a )
{
	*( reinterpret_cast< fltx4 *> ( pSIMD ) ) = a;
}

FORCEINLINE void StoreUnalignedSIMD( float *pSIMD, const fltx4 & a )
{
	*( reinterpret_cast< fltx4 *> ( pSIMD ) ) = a;
}

FORCEINLINE void StoreUnaligned3SIMD( float *pSIMD, const fltx4 & a )
{
	*pSIMD     = SubFloat(a, 0);
	*(pSIMD+1) = SubFloat(a, 1);
	*(pSIMD+2) = SubFloat(a, 2);
}

// strongly typed -- syntactic castor oil used for typechecking as we transition to SIMD
FORCEINLINE void StoreAligned3SIMD( VectorAligned * RESTRICT pSIMD, const fltx4 & a )
{
	StoreAlignedSIMD(pSIMD->Base(),a);
}

FORCEINLINE void TransposeSIMD( fltx4 & x, fltx4 & y, fltx4 & z, fltx4 & w )
{
#define SWAP_FLOATS( _a_, _ia_, _b_, _ib_ ) { float tmp = SubFloat( _a_, _ia_ ); SubFloat( _a_, _ia_ ) = SubFloat( _b_, _ib_ ); SubFloat( _b_, _ib_ ) = tmp; }
	SWAP_FLOATS( x, 1, y, 0 );
	SWAP_FLOATS( x, 2, z, 0 );
	SWAP_FLOATS( x, 3, w, 0 );
	SWAP_FLOATS( y, 2, z, 1 );
	SWAP_FLOATS( y, 3, w, 1 );
	SWAP_FLOATS( z, 3, w, 2 );
}

// find the lowest component of a.x, a.y, a.z,
// and replicate it to the whole return value.
FORCEINLINE fltx4 FindLowestSIMD3( const fltx4 & a )
{
	float lowest = min( min( SubFloat(a, 0), SubFloat(a, 1) ), SubFloat(a, 2));
	return ReplicateX4(lowest);
}

// find the highest component of a.x, a.y, a.z,
// and replicate it to the whole return value.
FORCEINLINE fltx4 FindHighestSIMD3( const fltx4 & a )
{
	float highest = max( max( SubFloat(a, 0), SubFloat(a, 1) ), SubFloat(a, 2));
	return ReplicateX4(highest);
}

// Fixed-point conversion and save as SIGNED INTS.
// pDest->x = Int (vSrc.x)
// note: some architectures have means of doing 
// fixed point conversion when the fix depth is
// specified as an immediate.. but there is no way 
// to guarantee an immediate as a parameter to function
// like this.
FORCEINLINE void ConvertStoreAsIntsSIMD(intx4 * RESTRICT pDest, const fltx4 &vSrc)
{
	(*pDest)[0] = SubFloat(vSrc, 0);
	(*pDest)[1] = SubFloat(vSrc, 1);
	(*pDest)[2] = SubFloat(vSrc, 2);
	(*pDest)[3] = SubFloat(vSrc, 3);
}

// ------------------------------------
// INTEGER SIMD OPERATIONS.
// ------------------------------------
// splat all components of a vector to a signed immediate int number.
FORCEINLINE fltx4 IntSetImmediateSIMD( int nValue )
{
	fltx4 retval;
	SubInt( retval, 0 ) = SubInt( retval, 1 ) = SubInt( retval, 2 ) = SubInt( retval, 3) = nValue;
	return retval;
}

// Load 4 aligned words into a SIMD register
FORCEINLINE i32x4 LoadAlignedIntSIMD(const void * RESTRICT pSIMD)
{
	return *( reinterpret_cast< const i32x4 *> ( pSIMD ) );
}

// Load 4 unaligned words into a SIMD register
FORCEINLINE i32x4 LoadUnalignedIntSIMD( const void * RESTRICT pSIMD)
{
	return *( reinterpret_cast< const i32x4 *> ( pSIMD ) );
}

// save into four words, 16-byte aligned
FORCEINLINE void StoreAlignedIntSIMD( int32 *pSIMD, const fltx4 & a )
{
	*( reinterpret_cast< i32x4 *> ( pSIMD ) ) = a;
}

FORCEINLINE void StoreAlignedIntSIMD( intx4 &pSIMD, const fltx4 & a )
{
	*( reinterpret_cast< i32x4 *> ( pSIMD.Base() ) ) = a;
}

FORCEINLINE void StoreUnalignedIntSIMD( int32 *pSIMD, const fltx4 & a )
{
	*( reinterpret_cast< i32x4 *> ( pSIMD ) ) = a;
}

// Take a fltx4 containing fixed-point uints and 
// return them as single precision floats. No
// fixed point conversion is done.
FORCEINLINE fltx4 UnsignedIntConvertToFltSIMD( const u32x4 &vSrcA )
{
	Assert(0);			/* pc has no such operation */
	fltx4 retval;
	SubFloat( retval, 0 ) = ( (float) SubInt( retval, 0 ) );
	SubFloat( retval, 1 ) = ( (float) SubInt( retval, 1 ) );
	SubFloat( retval, 2 ) = ( (float) SubInt( retval, 2 ) );
	SubFloat( retval, 3 ) = ( (float) SubInt( retval, 3 ) );
	return retval;
}


#if 0				/* pc has no such op */
// Take a fltx4 containing fixed-point sints and 
// return them as single precision floats. No 
// fixed point conversion is done.
FORCEINLINE fltx4 SignedIntConvertToFltSIMD( const i32x4 &vSrcA )
{
	fltx4 retval;
	SubFloat( retval, 0 ) = ( (float) (reinterpret_cast<int32 *>(&vSrcA.m128_s32[0])) );
	SubFloat( retval, 1 ) = ( (float) (reinterpret_cast<int32 *>(&vSrcA.m128_s32[1])) );
	SubFloat( retval, 2 ) = ( (float) (reinterpret_cast<int32 *>(&vSrcA.m128_s32[2])) );
	SubFloat( retval, 3 ) = ( (float) (reinterpret_cast<int32 *>(&vSrcA.m128_s32[3])) );
	return retval;
}


/*
  works on fltx4's as if they are four uints.
  the first parameter contains the words to be shifted,
  the second contains the amount to shift by AS INTS

  for i = 0 to 3
  shift = vSrcB_i*32:(i*32)+4
  vReturned_i*32:(i*32)+31 = vSrcA_i*32:(i*32)+31 << shift
*/
FORCEINLINE i32x4 IntShiftLeftWordSIMD(const i32x4 &vSrcA, const i32x4 &vSrcB)
{
	i32x4 retval;
	SubInt(retval, 0) = SubInt(vSrcA, 0) << SubInt(vSrcB, 0);
	SubInt(retval, 1) = SubInt(vSrcA, 1) << SubInt(vSrcB, 1);
	SubInt(retval, 2) = SubInt(vSrcA, 2) << SubInt(vSrcB, 2);
	SubInt(retval, 3) = SubInt(vSrcA, 3) << SubInt(vSrcB, 3);


	return retval;
}
#endif

#else

//---------------------------------------------------------------------
// Intel/SSE implementation
//---------------------------------------------------------------------

#ifdef _XM_SSE_INTRINSICS_

FORCEINLINE void XM_CALLCONV StoreAlignedSIMD( float * RESTRICT pSIMD, DirectX::FXMVECTOR a )
{
	_mm_store_ps( pSIMD, a );
}

FORCEINLINE void XM_CALLCONV StoreUnalignedSIMD( float * RESTRICT pSIMD, DirectX::FXMVECTOR a )
{
	_mm_storeu_ps( pSIMD, a );
}

#endif  // _XM_SSE_INTRINSICS_


[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV RotateLeft( DirectX::FXMVECTOR a );

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV RotateLeft2( DirectX::FXMVECTOR a );

#ifdef _XM_SSE_INTRINSICS_

FORCEINLINE void XM_CALLCONV StoreUnaligned3SIMD( float *pSIMD, DirectX::FXMVECTOR a )
{
	_mm_store_ss(pSIMD, a);
	_mm_store_ss(pSIMD+1, RotateLeft(a));
	_mm_store_ss(pSIMD+2, RotateLeft2(a));
}

#endif  // _XM_SSE_INTRINSICS_

// strongly typed -- syntactic castor oil used for typechecking as we transition to SIMD
FORCEINLINE void XM_CALLCONV StoreAligned3SIMD( VectorAligned * RESTRICT pSIMD, DirectX::FXMVECTOR a )
{
	DirectX::XMStoreFloat4A( pSIMD->XmBase(), a );
}

#ifdef _XM_SSE_INTRINSICS_

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV LoadAlignedSIMD( const void *pSIMD )
{
	return _mm_load_ps( reinterpret_cast< const float *> ( pSIMD ) );
}

#endif  // _XM_SSE_INTRINSICS_

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV AndSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )				// a & b
{
	return DirectX::XMVectorAndInt( a, b );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV AndNotSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )			// ~a & b
{
	// Yep, b & ~a.
	return DirectX::XMVectorAndCInt( b, a );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV XorSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )				// a ^ b
{
	return DirectX::XMVectorXorInt( a, b );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV OrSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )				// a | b
{
	return DirectX::XMVectorOrInt( a, b );
}

// Squelch the w component of a vector to +0.0.
// Most efficient when you say a = SetWToZeroSIMD(a) (avoids a copy)
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV SetWToZeroSIMD( DirectX::FXMVECTOR a )
{
	return AndSIMD( a, DirectX::XMLoadInt4A( g_SIMD_clear_wmask ) );
}

// for the transitional class -- load a 3-by VectorAligned and squash its w component
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV LoadAlignedSIMD( const VectorAligned & pSIMD )
{
	return SetWToZeroSIMD( DirectX::XMLoadFloat4A( pSIMD.XmBase() ) );
}

#ifdef _XM_SSE_INTRINSICS_

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV LoadUnalignedSIMD( const void *pSIMD )
{
	return _mm_loadu_ps( reinterpret_cast<const float *>( pSIMD ) );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV LoadUnaligned3SIMD( const void *pSIMD )
{
	return _mm_loadu_ps( reinterpret_cast<const float *>( pSIMD ) );
}

#endif  // _XM_SSE_INTRINSICS_

/// replicate a single 32 bit integer value to all 4 components of an m128
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV ReplicateIX4(uint32_t i) {
	return DirectX::XMVectorReplicateInt( i );
}


[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV ReplicateX4(float flValue) {
	return DirectX::XMVectorReplicate( flValue );
}

// Performance loss. Use with care.
[[nodiscard]]
FORCEINLINE float XM_CALLCONV SubFloat( const fltx4& a, size_t idx )
{
	return DirectX::XMVectorGetByIndex( a, idx );
}

// Performance loss. Use with care.
[[nodiscard]]
FORCEINLINE float& XM_CALLCONV SubFloat( fltx4 & a, size_t idx )
{
	Assert( idx < 4 );

	return (reinterpret_cast<float *>(&a))[idx];
}

[[nodiscard]]
FORCEINLINE uint32 XM_CALLCONV SubFloatConvertToInt( DirectX::FXMVECTOR a, size_t idx )
{
	return (uint32)SubFloat(a,idx);
}

// Performance loss. Use with care.
[[nodiscard]]
FORCEINLINE uint32 XM_CALLCONV SubInt( const fltx4& a, size_t idx )
{
	return DirectX::XMVectorGetIntByIndex( a, idx );
}

// Performance loss. Use with care.
[[nodiscard]]
FORCEINLINE uint32& XM_CALLCONV SubInt( fltx4 & a, size_t idx )
{
	Assert( idx < 4 );

	return (reinterpret_cast<uint32 *>(&a))[idx];
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV LoadZeroSIMD()
{
	return Four_Zeros;
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV LoadOneSIMD()
{
	return Four_Ones;
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV MaskedAssign( DirectX::FXMVECTOR ReplacementMask, DirectX::FXMVECTOR NewValue, DirectX::FXMVECTOR OldValue )
{
	return OrSIMD(
		AndSIMD( ReplacementMask, NewValue ),
		AndNotSIMD( ReplacementMask, OldValue ) );
}

// dimhteopus: Comment unused macro.
// remember, the SSE numbers its words 3 2 1 0
// The way we want to specify shuffles is backwards from the default
// MM_SHUFFLE_REV is in array index order (default is reversed)
// define MM_SHUFFLE_REV(a,b,c,d) _MM_SHUFFLE(d,c,b,a)

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV SplatXSIMD( DirectX::FXMVECTOR a )
{
	return DirectX::XMVectorSplatX( a );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV SplatYSIMD( DirectX::FXMVECTOR a )
{
	return DirectX::XMVectorSplatY( a );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV SplatZSIMD( DirectX::FXMVECTOR a )
{
	return DirectX::XMVectorSplatZ( a );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV SplatWSIMD( DirectX::FXMVECTOR a )
{
	return DirectX::XMVectorSplatW( a );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV SetXSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR x )
{
	fltx4 result = MaskedAssign( DirectX::XMLoadInt4A( g_SIMD_ComponentMask[0] ), x, a );
	return result;
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV SetYSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR y )
{
	fltx4 result = MaskedAssign( DirectX::XMLoadInt4A( g_SIMD_ComponentMask[1] ), y, a );
	return result;
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV SetZSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR z )
{
	fltx4 result = MaskedAssign( DirectX::XMLoadInt4A( g_SIMD_ComponentMask[2] ), z, a );
	return result;
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV SetWSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR w )
{
	fltx4 result = MaskedAssign( DirectX::XMLoadInt4A( g_SIMD_ComponentMask[3] ), w, a );
	return result;
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV SetComponentSIMD( DirectX::FXMVECTOR a, int nComponent, float flValue )
{
	fltx4 val = ReplicateX4( flValue );
	fltx4 result = MaskedAssign( DirectX::XMLoadInt4A( g_SIMD_ComponentMask[nComponent] ), val, a );
	return result;
}

// a b c d -> b c d a
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV RotateLeft( DirectX::FXMVECTOR a )
{
	return DirectX::XMVectorSwizzle<
		DirectX::XM_SWIZZLE_Y,
		DirectX::XM_SWIZZLE_Z,
		DirectX::XM_SWIZZLE_W,
		DirectX::XM_SWIZZLE_X>( a );
}

// a b c d -> c d a b
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV RotateLeft2( DirectX::FXMVECTOR a )
{
	return DirectX::XMVectorSwizzle<
		DirectX::XM_SWIZZLE_Z,
		DirectX::XM_SWIZZLE_W,
		DirectX::XM_SWIZZLE_X,
		DirectX::XM_SWIZZLE_Y>( a );
}

// a b c d -> d a b c
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV RotateRight( DirectX::FXMVECTOR a )
{
	return DirectX::XMVectorSwizzle<
		DirectX::XM_SWIZZLE_W,
		DirectX::XM_SWIZZLE_X,
		DirectX::XM_SWIZZLE_Y,
		DirectX::XM_SWIZZLE_Z>( a );
}

// a b c d -> c d a b
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV RotateRight2( DirectX::FXMVECTOR a )
{
	return DirectX::XMVectorSwizzle<
		DirectX::XM_SWIZZLE_Z,
		DirectX::XM_SWIZZLE_W,
		DirectX::XM_SWIZZLE_X,
		DirectX::XM_SWIZZLE_Y>( a );
}


[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV AddSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )  // a+b
{
	return DirectX::XMVectorAdd( a, b );
};

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV SubSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )				// a-b
{
	return DirectX::XMVectorSubtract( a, b );
};

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV MulSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )				// a*b
{
	return DirectX::XMVectorMultiply( a, b );
};

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV DivSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )				// a/b
{
	return DirectX::XMVectorDivide( a, b );
};

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV MaddSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b, DirectX::FXMVECTOR c )				// a*b + c
{
	return DirectX::XMVectorMultiplyAdd( a, b, c );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV MsubSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b, DirectX::FXMVECTOR c )				// c - a*b
{
	return DirectX::XMVectorNegativeMultiplySubtract( a, b, c );
};

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV Dot3SIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )
{
	return DirectX::XMVector3Dot( a, b );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV Dot4SIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )
{
	return DirectX::XMVector4Dot( a, b );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV SinSIMD( DirectX::FXMVECTOR radians )
{
	return DirectX::XMVectorSin( radians );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV CosSIMD( DirectX::FXMVECTOR radians )
{
	return DirectX::XMVectorCos( radians );
}

FORCEINLINE void XM_CALLCONV SinCos3SIMD( fltx4 &sine, fltx4 &cosine, DirectX::FXMVECTOR radians )
{
	// Doing 1 additional op.
	DirectX::XMVectorSinCos( &sine, &cosine, radians );
}

FORCEINLINE void XM_CALLCONV SinCosSIMD( fltx4 &sine, fltx4 &cosine, DirectX::FXMVECTOR radians )
{
	DirectX::XMVectorSinCos( &sine, &cosine, radians );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV ArcSinSIMD( DirectX::FXMVECTOR sine )
{
	return DirectX::XMVectorASin( sine );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV ArcCosSIMD( DirectX::FXMVECTOR cs )
{
	return DirectX::XMVectorACos( cs );
}

// tan^1(a/b) .. ie, pass sin in as a and cos in as b
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV ArcTan2SIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )
{
	return DirectX::XMVectorATan2( a, b );
}

// arctan(a)
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV ArcTanSIMD( DirectX::FXMVECTOR a )
{
	return DirectX::XMVectorATan( a );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV NegSIMD( DirectX::FXMVECTOR a ) // negate: -a
{
	return DirectX::XMVectorNegate( a );
}

[[nodiscard]]
FORCEINLINE int XM_CALLCONV TestSignSIMD( DirectX::FXMVECTOR a )								// mask of which floats have the high bit set
{
	return _mm_movemask_ps( a );
}

[[nodiscard]]
FORCEINLINE bool XM_CALLCONV IsAnyNegative( DirectX::FXMVECTOR a )							// (a.x < 0) || (a.y < 0) || (a.z < 0) || (a.w < 0)
{
	return (0 != TestSignSIMD( a ));
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV CmpEqSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )				// (a==b) ? ~0:0
{
	return DirectX::XMVectorEqual( a, b );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV CmpGtSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )				// (a>b) ? ~0:0
{
	return DirectX::XMVectorGreater( a, b );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV CmpGeSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )				// (a>=b) ? ~0:0
{
	return DirectX::XMVectorGreaterOrEqual( a, b );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV CmpLtSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )				// (a<b) ? ~0:0
{
	return DirectX::XMVectorLess( a, b );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV CmpLeSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )				// (a<=b) ? ~0:0
{
	return DirectX::XMVectorLessOrEqual( a, b );
}

// for branching when a.xyzw > b.xyzw
[[nodiscard]]
FORCEINLINE bool XM_CALLCONV IsAllGreaterThan( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )
{
	return	TestSignSIMD( CmpLeSIMD( a, b ) ) == 0;
}

// for branching when a.xyzw >= b.xyzw
[[nodiscard]]
FORCEINLINE bool XM_CALLCONV IsAllGreaterThanOrEq( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )
{
	return	TestSignSIMD( CmpLtSIMD( a, b ) ) == 0;
}

// For branching if all a.xyzw == b.xyzw
[[nodiscard]]
FORCEINLINE bool XM_CALLCONV IsAllEqual( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )
{
	return	TestSignSIMD( CmpEqSIMD( a, b ) ) == 0xf;
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV CmpInBoundsSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )		// (a <= b && a >= -b) ? ~0 : 0
{
	return AndSIMD( CmpLeSIMD(a,b), CmpGeSIMD(a, NegSIMD(b)) );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV MinSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )				// min(a,b)
{
	return DirectX::XMVectorMin( a, b );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV MaxSIMD( DirectX::FXMVECTOR a, DirectX::FXMVECTOR b )				// max(a,b)
{
	return DirectX::XMVectorMax( a, b );
}


[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV CeilSIMD( DirectX::FXMVECTOR a )
{
	return DirectX::XMVectorCeiling( a );
}

// Round towards negative infinity
// This is the implementation that was here before; it assumes
// you are in round-to-floor mode, which I guess is usually the
// case for us vis-a-vis SSE. It's totally unnecessary on 
// VMX, which has a native floor op.
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV FloorSIMD( DirectX::FXMVECTOR val )
{
	return DirectX::XMVectorFloor( val );
}



[[nodiscard]]
inline bool XM_CALLCONV IsAllZeros( DirectX::FXMVECTOR var )
{
	return TestSignSIMD( CmpEqSIMD( var, Four_Zeros ) ) == 0xF;
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV SqrtEstSIMD( DirectX::FXMVECTOR a )					// sqrt(a), more or less
{
	return DirectX::XMVectorSqrtEst( a );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV SqrtSIMD( DirectX::FXMVECTOR a )  // sqrt(a) //-V524
{
	return DirectX::XMVectorSqrt( a );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV ReciprocalSqrtEstSIMD( DirectX::FXMVECTOR a )			// 1/sqrt(a), more or less
{
	return DirectX::XMVectorReciprocalSqrtEst( a );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV ReciprocalSqrtEstSaturateSIMD( DirectX::FXMVECTOR a )
{
	fltx4 zero_mask = CmpEqSIMD( a, Four_Zeros );
	fltx4 ret = OrSIMD( a, AndSIMD( Four_Epsilons, zero_mask ) );
	ret = ReciprocalSqrtEstSIMD( ret );
	return ret;
}

/// uses newton iteration for higher precision results than ReciprocalSqrtEstSIMD
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV ReciprocalSqrtSIMD( DirectX::FXMVECTOR a )  // 1/sqrt(a)
{
	fltx4 guess = ReciprocalSqrtEstSIMD( a );
	// newton iteration for 1/sqrt(a) : y(n+1) = 1/2 (y(n)*(3-a*y(n)^2));
	guess = MulSIMD( guess, MsubSIMD( a, MulSIMD( guess, guess ), Four_Threes ));
	guess = MulSIMD( Four_PointFives, guess);
	return guess;
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV ReciprocalEstSIMD( DirectX::FXMVECTOR a )				// 1/a, more or less
{
	return DirectX::XMVectorReciprocalEst( a );
}

/// 1/x for all 4 values, more or less
/// 1/0 will result in a big but NOT infinite result
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV ReciprocalEstSaturateSIMD( DirectX::FXMVECTOR a )
{
	fltx4 zero_mask = CmpEqSIMD( a, Four_Zeros );
	fltx4 ret = OrSIMD( a, AndSIMD( Four_Epsilons, zero_mask ) );
	ret = ReciprocalEstSIMD( ret );
	return ret;
}

/// 1/x for all 4 values. uses reciprocal approximation instruction plus newton iteration.
/// No error checking!
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV ReciprocalSIMD( DirectX::FXMVECTOR a )  // 1/a
{
	fltx4 ret = ReciprocalEstSIMD( a );
	// newton iteration is: Y(n+1) = 2*Y(n)-a*Y(n)^2
	ret = SubSIMD( AddSIMD( ret, ret ), MulSIMD( a, MulSIMD( ret, ret ) ) );
	return ret;
}

/// 1/x for all 4 values.
/// 1/0 will result in a big but NOT infinite result
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV ReciprocalSaturateSIMD( DirectX::FXMVECTOR a )
{
	fltx4 zero_mask = CmpEqSIMD( a, Four_Zeros );
	fltx4 ret = OrSIMD( a, AndSIMD( Four_Epsilons, zero_mask ) );
	ret = ReciprocalSIMD( ret );
	return ret;
}

// 2^x for all values (the antilog)
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV ExpSIMD( DirectX::FXMVECTOR toPower )
{
	return DirectX::XMVectorExp2( toPower );
}

// Clamps the components of a vector to a specified minimum and maximum range.
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV ClampVectorSIMD( DirectX::FXMVECTOR in, DirectX::FXMVECTOR min, DirectX::FXMVECTOR max)
{
	return MaxSIMD( min, MinSIMD( max, in ) );
}

FORCEINLINE void XM_CALLCONV TransposeSIMD( fltx4 & x, fltx4 & y, fltx4 & z, fltx4 & w)
{
	_MM_TRANSPOSE4_PS( x, y, z, w );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV FindLowestSIMD3( DirectX::FXMVECTOR a )
{
	// a is [x,y,z,G] (where G is garbage)
	// rotate left by one 
	fltx4 compareOne = RotateLeft( a );
	// compareOne is [y,z,G,x]
	fltx4 retval = MinSIMD( a, compareOne );
	// retVal is [min(x,y), ... ]
	compareOne = RotateLeft2( a );
	// compareOne is [z, G, x, y]
	retval = MinSIMD( retval, compareOne );
	// retVal = [ min(min(x,y),z)..]
	// splat the x component out to the whole vector and return
	return SplatXSIMD( retval );
	
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV FindHighestSIMD3( DirectX::FXMVECTOR a )
{
	// a is [x,y,z,G] (where G is garbage)
	// rotate left by one 
	fltx4 compareOne = RotateLeft( a );
	// compareOne is [y,z,G,x]
	fltx4 retval = MaxSIMD( a, compareOne );
	// retVal is [max(x,y), ... ]
	compareOne = RotateLeft2( a );
	// compareOne is [z, G, x, y]
	retval = MaxSIMD( retval, compareOne );
	// retVal = [ max(max(x,y),z)..]
	// splat the x component out to the whole vector and return
	return SplatXSIMD( retval );
	
}

// ------------------------------------
// INTEGER SIMD OPERATIONS.
// ------------------------------------


#if 0				/* pc does not have these ops */
// splat all components of a vector to a signed immediate int number.
FORCEINLINE fltx4 IntSetImmediateSIMD(int to)
{
	//CHRISG: SSE2 has this, but not SSE1. What to do?
	fltx4 retval;
	SubInt( retval, 0 ) = to;
	SubInt( retval, 1 ) = to;
	SubInt( retval, 2 ) = to;
	SubInt( retval, 3 ) = to;
	return retval;
}
#endif

#ifdef _XM_SSE_INTRINSICS_

// Load 4 aligned words into a SIMD register
[[nodiscard]]
FORCEINLINE i32x4 XM_CALLCONV LoadAlignedIntSIMD( const void * RESTRICT pSIMD )
{
	return _mm_load_ps( reinterpret_cast<const float *>(pSIMD) );
}

// Load 4 unaligned words into a SIMD register
[[nodiscard]]
FORCEINLINE i32x4 XM_CALLCONV LoadUnalignedIntSIMD( const void * RESTRICT pSIMD )
{
	return _mm_loadu_ps( reinterpret_cast<const float *>(pSIMD) );
}

// save into four words, 16-byte aligned
FORCEINLINE void XM_CALLCONV StoreAlignedIntSIMD( int32 * RESTRICT pSIMD, DirectX::FXMVECTOR a )
{
	_mm_store_ps( reinterpret_cast<float *>(pSIMD), a );
}

FORCEINLINE void XM_CALLCONV StoreAlignedIntSIMD( intx4 &pSIMD, DirectX::FXMVECTOR a )
{
	_mm_store_ps( reinterpret_cast<float *>(pSIMD.Base()), a );
}

FORCEINLINE void XM_CALLCONV StoreUnalignedIntSIMD( int32 * RESTRICT pSIMD, DirectX::FXMVECTOR a )
{
	_mm_storeu_ps( reinterpret_cast<float *>(pSIMD), a );
}

#endif  // _XM_SSE_INTRINSICS_

// CHRISG: the conversion functions all seem to operate on m64's only...
// how do we make them work here?

// Take a fltx4 containing fixed-point uints and 
// return them as single precision floats. No
// fixed point conversion is done.
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV UnsignedIntConvertToFltSIMD( const u32x4 vSrcA )
{
	fltx4 retval;
	// dimhotepus: Fix source, was assigned self to self.
	SubFloat( retval, 0 ) = ( (float) SubInt( vSrcA, 0 ) );
	SubFloat( retval, 1 ) = ( (float) SubInt( vSrcA, 1 ) );
	SubFloat( retval, 2 ) = ( (float) SubInt( vSrcA, 2 ) );
	SubFloat( retval, 3 ) = ( (float) SubInt( vSrcA, 3 ) );
	return retval;
}


// Take a fltx4 containing fixed-point sints and 
// return them as single precision floats. No 
// fixed point conversion is done.
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV SignedIntConvertToFltSIMD( const i32x4 vSrcA )
{
	fltx4 retval;
	SubFloat( retval, 0 ) = ( (float) (reinterpret_cast<const int32 *>(&vSrcA)[0]));
	SubFloat( retval, 1 ) = ( (float) (reinterpret_cast<const int32 *>(&vSrcA)[1]));
	SubFloat( retval, 2 ) = ( (float) (reinterpret_cast<const int32 *>(&vSrcA)[2]));
	SubFloat( retval, 3 ) = ( (float) (reinterpret_cast<const int32 *>(&vSrcA)[3]));
	return retval;
}

/*
  works on fltx4's as if they are four uints.
  the first parameter contains the words to be shifted,
  the second contains the amount to shift by AS INTS

  for i = 0 to 3
  shift = vSrcB_i*32:(i*32)+4
  vReturned_i*32:(i*32)+31 = vSrcA_i*32:(i*32)+31 << shift
*/
[[nodiscard]]
FORCEINLINE i32x4 XM_CALLCONV IntShiftLeftWordSIMD(const i32x4 vSrcA, const i32x4 vSrcB)
{
	i32x4 retval;
	SubInt(retval, 0) = SubInt(vSrcA, 0) << SubInt(vSrcB, 0);
	SubInt(retval, 1) = SubInt(vSrcA, 1) << SubInt(vSrcB, 1);
	SubInt(retval, 2) = SubInt(vSrcA, 2) << SubInt(vSrcB, 2);
	SubInt(retval, 3) = SubInt(vSrcA, 3) << SubInt(vSrcB, 3);
	return retval;
}


// Fixed-point conversion and save as SIGNED INTS.
// pDest->x = Int (vSrc.x)
// NOTE: Some architectures have means of doing fixed point conversion
// when the fix depth is specified as an immediate.. but there is no
// way to guarantee an immediate as a parameter to function like this.
FORCEINLINE void XM_CALLCONV ConvertStoreAsIntsSIMD(intx4 * RESTRICT pDest, DirectX::FXMVECTOR vSrc)
{
#if defined( COMPILER_MSVC64 ) || !defined(_XM_SSE_INTRINSICS_)

	(*pDest)[0] = (int)SubFloat( vSrc, 0 );
	(*pDest)[1] = (int)SubFloat( vSrc, 1 );
	(*pDest)[2] = (int)SubFloat( vSrc, 2 );
	(*pDest)[3] = (int)SubFloat( vSrc, 3 );

#else
	__m64 bottom = _mm_cvttps_pi32( vSrc );
	__m64 top    = _mm_cvttps_pi32( _mm_movehl_ps(vSrc,vSrc) );

	memcpy( &(*pDest)[0], &bottom, sizeof(bottom) );
	memcpy( &(*pDest)[2], &top, sizeof(top) );

	_mm_empty();
#endif
}

#endif  // !USE_STDC_FOR_SIMD


/// class FourVectors stores 4 independent vectors for use in SIMD processing. These vectors are
/// stored in the format x x x x y y y y z z z z so that they can be efficiently SIMD-accelerated.
// dimhotepus: Fix aligned alloc.
class alignas(16) FourVectors : public CAlignedNewDelete<16>
{
public:
	fltx4 x, y, z;

	FORCEINLINE void XM_CALLCONV DuplicateVector(Vector const &v)			//< set all 4 vectors to the same vector value
	{
		x=ReplicateX4(v.x);
		y=ReplicateX4(v.y);
		z=ReplicateX4(v.z);
	}

	[[nodiscard]]
	FORCEINLINE fltx4 const & XM_CALLCONV operator[](int idx) const
	{
		return *((&x)+idx);
	}
	
	[[nodiscard]]
	FORCEINLINE fltx4 & XM_CALLCONV operator[](int idx)
	{
		return *((&x)+idx);
	}

	FORCEINLINE void operator+=(FourVectors const &b)			//< add 4 vectors to another 4 vectors
	{
		x=AddSIMD(x,b.x);
		y=AddSIMD(y,b.y);
		z=AddSIMD(z,b.z);
	}

	FORCEINLINE void operator-=(FourVectors const &b)			//< subtract 4 vectors from another 4
	{
		x=SubSIMD(x,b.x);
		y=SubSIMD(y,b.y);
		z=SubSIMD(z,b.z);
	}

	FORCEINLINE void operator*=(FourVectors const &b)			//< scale all four vectors per component scale
	{
		x=MulSIMD(x,b.x);
		y=MulSIMD(y,b.y);
		z=MulSIMD(z,b.z);
	}

	FORCEINLINE void XM_CALLCONV operator*=(DirectX::FXMVECTOR scale)  //< scale 
	{
		x=MulSIMD(x,scale);
		y=MulSIMD(y,scale);
		z=MulSIMD(z,scale);
	}

	FORCEINLINE void XM_CALLCONV operator*=(float scale)					//< uniformly scale all 4 vectors
	{
		fltx4 scalepacked = ReplicateX4(scale);
		*this *= scalepacked;
	}
	
	[[nodiscard]]
	FORCEINLINE fltx4 XM_CALLCONV operator*(FourVectors const &b) const		//< 4 dot products
	{
		fltx4 dot=MulSIMD(x,b.x);
		dot=MaddSIMD(y,b.y,dot);
		dot=MaddSIMD(z,b.z,dot);
		return dot;
	}
	
	[[nodiscard]]
	FORCEINLINE fltx4 XM_CALLCONV operator*(Vector const &b) const			//< dot product all 4 vectors with 1 vector
	{
		fltx4 dot=MulSIMD(x,ReplicateX4(b.x));
		dot=MaddSIMD(y,ReplicateX4(b.y), dot);
		dot=MaddSIMD(z,ReplicateX4(b.z), dot);
		return dot;
	}

	FORCEINLINE void VProduct(FourVectors const &b)				//< component by component mul //-V524
	{
		x=MulSIMD(x,b.x);
		y=MulSIMD(y,b.y);
		z=MulSIMD(z,b.z);
	}
	FORCEINLINE void MakeReciprocal(void)						//< (x,y,z)=(1/x,1/y,1/z)
	{
		x=ReciprocalSIMD(x);
		y=ReciprocalSIMD(y);
		z=ReciprocalSIMD(z);
	}

	FORCEINLINE void MakeReciprocalSaturate(void)				//< (x,y,z)=(1/x,1/y,1/z), 1/0=1.0e23
	{
		x=ReciprocalSaturateSIMD(x);
		y=ReciprocalSaturateSIMD(y);
		z=ReciprocalSaturateSIMD(z);
	}

	// Assume the given matrix is a rotation, and rotate these vectors by it.
	// If you have a long list of FourVectors structures that you all want 
	// to rotate by the same matrix, use FourVectors::RotateManyBy() instead.
	inline void RotateBy(const matrix3x4_t& matrix);

	/// You can use this to rotate a long array of FourVectors all by the same
	/// matrix. The first parameter is the head of the array. The second is the
	/// number of vectors to rotate. The third is the matrix.
	static void RotateManyBy(FourVectors * RESTRICT pVectors, unsigned int numVectors, const matrix3x4_t& rotationMatrix );

	/// Assume the vectors are points, and transform them in place by the matrix.
	inline void TransformBy(const matrix3x4_t& matrix);

	/// You can use this to Transform a long array of FourVectors all by the same
	/// matrix. The first parameter is the head of the array. The second is the
	/// number of vectors to rotate. The third is the matrix. The fourth is the 
	/// output buffer, which must not overlap the pVectors buffer. This is not 
	/// an in-place transformation.
	static void TransformManyBy(FourVectors * RESTRICT pVectors, unsigned int numVectors, const matrix3x4_t& rotationMatrix, FourVectors * RESTRICT pOut );

	/// You can use this to Transform a long array of FourVectors all by the same
	/// matrix. The first parameter is the head of the array. The second is the
	/// number of vectors to rotate. The third is the matrix. The fourth is the 
	/// output buffer, which must not overlap the pVectors buffer. 
	/// This is an in-place transformation.
	static void TransformManyBy(FourVectors * RESTRICT pVectors, unsigned int numVectors, const matrix3x4_t& rotationMatrix );

	// X(),Y(),Z() - get at the desired component of the i'th (0..3) vector.
	[[nodiscard]]
	FORCEINLINE const float & XM_CALLCONV X(int idx) const
	{
		// NOTE: if the output goes into a register, this causes a Load-Hit-Store stall (don't mix fpu/vpu math!)
		return SubFloat( (fltx4 &)x, idx );
	}
	
	[[nodiscard]]
	FORCEINLINE const float & XM_CALLCONV Y(int idx) const
	{
		return SubFloat( (fltx4 &)y, idx );
	}
	
	[[nodiscard]]
	FORCEINLINE const float & XM_CALLCONV Z(int idx) const
	{
		return SubFloat( (fltx4 &)z, idx );
	}
	
	[[nodiscard]]
	FORCEINLINE float & XM_CALLCONV X(int idx)
	{
		return SubFloat( x, idx );
	}
	
	[[nodiscard]]
	FORCEINLINE float & XM_CALLCONV Y(int idx)
	{
		return SubFloat( y, idx );
	}
	
	[[nodiscard]]
	FORCEINLINE float & XM_CALLCONV Z(int idx)
	{
		return SubFloat( z, idx );
	}
	
	[[nodiscard]]
	FORCEINLINE Vector XM_CALLCONV Vec(int idx) const						//< unpack one of the vectors
	{
		return Vector( X(idx), Y(idx), Z(idx) );
	}
	
	FourVectors() = default;

	FourVectors( FourVectors const &src )
	{
		x=src.x;
		y=src.y;
		z=src.z;
	}

	FORCEINLINE void operator=( FourVectors const &src )
	{
		x=src.x;
		y=src.y;
		z=src.z;
	}

	/// LoadAndSwizzle - load 4 Vectors into a FourVectors, performing transpose op
	FORCEINLINE void LoadAndSwizzle(Vector const &a, Vector const &b, Vector const &c, Vector const &d)
	{
		x		= DirectX::XMLoadFloat3( a.XmBase() );
		y		= DirectX::XMLoadFloat3( b.XmBase() );
		z		= DirectX::XMLoadFloat3( c.XmBase() );
		fltx4 w = DirectX::XMLoadFloat3( d.XmBase() );
		// now, matrix is:
		// x y z ?
		// x y z ?
		// x y z ?
		// x y z ?
		TransposeSIMD(x, y, z, w);
	}

	/// LoadAndSwizzleAligned - load 4 Vectors into a FourVectors, performing transpose op.
	/// all 4 vectors must be 128 bit boundary
	// dimhotepus: Too unsafe. Use other methods.
	FORCEINLINE void LoadAndSwizzleAligned(const float *RESTRICT a, const float *RESTRICT b, const float *RESTRICT c, const float *RESTRICT d) = delete;
	//{
	//	x		= LoadAlignedSIMD( a );
	//	y		= LoadAlignedSIMD( b );
	//	z		= LoadAlignedSIMD( c );
	//	fltx4 w = LoadAlignedSIMD( d );
	//	// now, matrix is:
	//	// x y z ?
	//	// x y z ?
	//	// x y z ?
	//	// x y z ?
	//	TransposeSIMD( x, y, z, w );
	//}

	FORCEINLINE void LoadAndSwizzleAligned(Vector const &a, Vector const &b, Vector const &c, Vector const &d)
	{
		x		= DirectX::XMLoadFloat3( a.XmBase() );
		y		= DirectX::XMLoadFloat3( b.XmBase() );
		z		= DirectX::XMLoadFloat3( c.XmBase() );
		fltx4 w = DirectX::XMLoadFloat3( d.XmBase() );
		// now, matrix is:
		// x y z ?
		// x y z ?
		// x y z ?
		// x y z ?
		TransposeSIMD( x, y, z, w );
	}

	/// return the squared length of all 4 vectors
	[[nodiscard]]
	FORCEINLINE fltx4 XM_CALLCONV length2(void) const
	{
		return (*this)*(*this);
	}

	/// return the approximate length of all 4 vectors. uses the sqrt approximation instruction
	[[nodiscard]]
	FORCEINLINE fltx4 XM_CALLCONV length(void) const
	{
		return SqrtEstSIMD(length2());
	}

	/// normalize all 4 vectors in place. not mega-accurate (uses reciprocal approximation instruction)
	FORCEINLINE void VectorNormalizeFast(void)
	{
		fltx4 mag_sq=(*this)*(*this);						// length^2
		// dimhotepus: Ensure no zero division.
		(*this) *= ReciprocalSqrtEstSIMD( AddSIMD( Four_Epsilons, mag_sq ) );			// *(1.0/sqrt(length^2))
	}

	/// normalize all 4 vectors in place.
	FORCEINLINE void VectorNormalize(void)
	{
		fltx4 mag_sq=(*this)*(*this);						// length^2
		// dimhotepus: Ensure no zero division.
		(*this) *= ReciprocalSqrtSIMD( AddSIMD( Four_Epsilons, mag_sq ) );				// *(1.0/sqrt(length^2))
	}

	/// construct a FourVectors from 4 separate Vectors
	FORCEINLINE FourVectors(Vector const &a, Vector const &b, Vector const &c, Vector const &d)
	{
		LoadAndSwizzle(a,b,c,d);
	}

	/// construct a FourVectors from 4 separate Vectors
	FORCEINLINE FourVectors(VectorAligned const &a, VectorAligned const &b, VectorAligned const &c, VectorAligned const &d)
	{
		x		= DirectX::XMLoadFloat4A( a.XmBase() );
		y		= DirectX::XMLoadFloat4A( b.XmBase() );
		z		= DirectX::XMLoadFloat4A( c.XmBase() );
		fltx4 w = DirectX::XMLoadFloat4A( d.XmBase() );
		// now, matrix is:
		// x y z ?
		// x y z ?
		// x y z ?
		// x y z ?
		TransposeSIMD( x, y, z, w );
	}

	[[nodiscard]]
	FORCEINLINE fltx4 XM_CALLCONV DistToSqr( FourVectors const &pnt )
	{
		fltx4 fl4dX = SubSIMD( pnt.x, x );
		fltx4 fl4dY = SubSIMD( pnt.y, y );
		fltx4 fl4dZ = SubSIMD( pnt.z, z );
		return MaddSIMD( fl4dX, fl4dX, MaddSIMD( fl4dY, fl4dY, MulSIMD( fl4dZ, fl4dZ ) ) );

	}

	[[nodiscard]]
	FORCEINLINE fltx4 XM_CALLCONV TValueOfClosestPointOnLine( FourVectors const &p0, FourVectors const &p1 ) const
	{
		FourVectors lineDelta = p1;
		lineDelta -= p0;
		fltx4 OOlineDirDotlineDir = ReciprocalSIMD( p1 * p1 );
		FourVectors v4OurPnt = *this;
		v4OurPnt -= p0;
		return MulSIMD( OOlineDirDotlineDir, v4OurPnt * lineDelta );
	}

	[[nodiscard]]
	FORCEINLINE fltx4 XM_CALLCONV DistSqrToLineSegment( FourVectors const &p0, FourVectors const &p1 ) const
	{
		FourVectors lineDelta = p1;
		FourVectors v4OurPnt = *this;
		v4OurPnt -= p0;
		lineDelta -= p0;

		fltx4 OOlineDirDotlineDir = ReciprocalSIMD( lineDelta * lineDelta );

		fltx4 fl4T = MulSIMD( OOlineDirDotlineDir, v4OurPnt * lineDelta );

		fl4T = MinSIMD( fl4T, Four_Ones );
		fl4T = MaxSIMD( fl4T, Four_Zeros );
		lineDelta *= fl4T;
		return v4OurPnt.DistToSqr( lineDelta );
	}

};

/// form 4 cross products
[[nodiscard]]
inline FourVectors XM_CALLCONV operator ^(const FourVectors &a, const FourVectors &b)
{
	FourVectors ret;
	ret.x=MsubSIMD(a.z,b.y,MulSIMD(a.y,b.z));
	ret.y=MsubSIMD(a.x,b.z,MulSIMD(a.z,b.x));
	ret.z=MsubSIMD(a.y,b.x,MulSIMD(a.x,b.y));
	return ret;
}

/// component-by-componentwise MAX operator
[[nodiscard]]
inline FourVectors XM_CALLCONV maximum(const FourVectors &a, const FourVectors &b)
{
	FourVectors ret;
	ret.x=MaxSIMD(a.x,b.x);
	ret.y=MaxSIMD(a.y,b.y);
	ret.z=MaxSIMD(a.z,b.z);
	return ret;
}

/// component-by-componentwise MIN operator
[[nodiscard]]
inline FourVectors XM_CALLCONV minimum(const FourVectors &a, const FourVectors &b)
{
	FourVectors ret;
	ret.x=MinSIMD(a.x,b.x);
	ret.y=MinSIMD(a.y,b.y);
	ret.z=MinSIMD(a.z,b.z);
	return ret;
}

/// calculate reflection vector. incident and normal dir assumed normalized
[[nodiscard]]
FORCEINLINE FourVectors XM_CALLCONV VectorReflect( const FourVectors &incident, const FourVectors &normal )
{
	FourVectors ret = incident;
	fltx4 iDotNx2 = incident * normal;
	iDotNx2 = AddSIMD( iDotNx2, iDotNx2 );
	FourVectors nPart = normal;
	nPart *= iDotNx2;
	ret -= nPart;											// i-2(n*i)n
	return ret;
}

/// calculate slide vector. removes all components of a vector which are perpendicular to a normal vector.
[[nodiscard]]
FORCEINLINE FourVectors XM_CALLCONV VectorSlide( const FourVectors &incident, const FourVectors &normal )
{
	FourVectors ret = incident;
	fltx4 iDotN = incident * normal;
	FourVectors nPart = normal;
	nPart *= iDotN;
	ret -= nPart;											// i-(n*i)n
	return ret;
}


// Assume the given matrix is a rotation, and rotate these vectors by it.
// If you have a long list of FourVectors structures that you all want 
// to rotate by the same matrix, use FourVectors::RotateManyBy() instead.
void FourVectors::RotateBy(const matrix3x4_t& matrix)
{
	// Splat out each of the entries in the matrix to a fltx4. Do this
	// in the order that we will need them, to hide latency. I'm
	// avoiding making an array of them, so that they'll remain in 
	// registers.
	fltx4 matSplat00, matSplat01, matSplat02,
		matSplat10, matSplat11, matSplat12,
		matSplat20, matSplat21, matSplat22;

 {
	// Load the matrix into local vectors. Sadly, matrix3x4_ts are 
	// often unaligned. The w components will be the tranpose row of
	// the matrix, but we don't really care about that.
	fltx4 matCol0 = DirectX::XMLoadFloat4( matrix.XmBase() );
	fltx4 matCol1 = DirectX::XMLoadFloat4( matrix.XmBase() + 1 );
	fltx4 matCol2 = DirectX::XMLoadFloat4( matrix.XmBase() + 2 );
	
	matSplat00 = SplatXSIMD( matCol0 );
	matSplat01 = SplatYSIMD( matCol0 );
	matSplat02 = SplatZSIMD( matCol0 );

	matSplat10 = SplatXSIMD( matCol1 );
	matSplat11 = SplatYSIMD( matCol1 );
	matSplat12 = SplatZSIMD( matCol1 );
	
	matSplat20 = SplatXSIMD( matCol2 );
	matSplat21 = SplatYSIMD( matCol2 );
	matSplat22 = SplatZSIMD( matCol2 );
 }

	// Trust in the compiler to schedule these operations correctly:
	fltx4 outX, outY, outZ;
	outX = MaddSIMD( z, matSplat02, MaddSIMD( x, matSplat00, MulSIMD( y, matSplat01 ) ) );
	outY = MaddSIMD( z, matSplat12, MaddSIMD( x, matSplat10, MulSIMD( y, matSplat11 ) ) );
	outZ = MaddSIMD( z, matSplat22, MaddSIMD( x, matSplat20, MulSIMD( y, matSplat21 ) ) );
	
	x = outX;
	y = outY;
	z = outZ;
}

// Assume the given matrix is a rotation, and rotate these vectors by it.
// If you have a long list of FourVectors structures that you all want 
// to rotate by the same matrix, use FourVectors::RotateManyBy() instead.
void FourVectors::TransformBy(const matrix3x4_t& matrix)
{
	// Splat out each of the entries in the matrix to a fltx4. Do this
	// in the order that we will need them, to hide latency. I'm
	// avoiding making an array of them, so that they'll remain in 
	// registers.
	fltx4 matSplat00, matSplat01, matSplat02,
		matSplat10, matSplat11, matSplat12,
		matSplat20, matSplat21, matSplat22;

 {
	// Load the matrix into local vectors. Sadly, matrix3x4_ts are 
	// often unaligned. The w components will be the tranpose row of
	// the matrix, but we don't really care about that.
	fltx4 matCol0 = DirectX::XMLoadFloat4( matrix.XmBase() );
	fltx4 matCol1 = DirectX::XMLoadFloat4( matrix.XmBase() + 1 );
	fltx4 matCol2 = DirectX::XMLoadFloat4( matrix.XmBase() + 2 );
	
	matSplat00 = SplatXSIMD( matCol0 );
	matSplat01 = SplatYSIMD( matCol0 );
	matSplat02 = SplatZSIMD( matCol0 );
	
	matSplat10 = SplatXSIMD( matCol1 );
	matSplat11 = SplatYSIMD( matCol1 );
	matSplat12 = SplatZSIMD( matCol1 );
	
	matSplat20 = SplatXSIMD( matCol2 );
	matSplat21 = SplatYSIMD( matCol2 );
	matSplat22 = SplatZSIMD( matCol2 );
 }
	
	// Trust in the compiler to schedule these operations correctly:
	fltx4 outX, outY, outZ;
	
	outX = MaddSIMD( z, matSplat02, MaddSIMD( x, matSplat00, MulSIMD( y, matSplat01 ) ) );
	outY = MaddSIMD( z, matSplat12, MaddSIMD( x, matSplat10, MulSIMD( y, matSplat11 ) ) );
	outZ = MaddSIMD( z, matSplat22, MaddSIMD( x, matSplat20, MulSIMD( y, matSplat21 ) ) );
	
	x = AddSIMD( outX, ReplicateX4( matrix[0][3] ));
	y = AddSIMD( outY, ReplicateX4( matrix[1][3] ));
	z = AddSIMD( outZ, ReplicateX4( matrix[2][3] ));
}



/// quick, low quality perlin-style noise() function suitable for real time use.
/// return value is -1..1. Only reliable around +/- 1 million or so.
[[nodiscard]]
fltx4 XM_CALLCONV NoiseSIMD( DirectX::FXMVECTOR x, DirectX::FXMVECTOR y, DirectX::FXMVECTOR z );
[[nodiscard]]
fltx4 XM_CALLCONV NoiseSIMD( FourVectors const &v );

// vector valued noise direction
FourVectors DNoiseSIMD( FourVectors const &v );

// vector value "curl" noise function. see http://hyperphysics.phy-astr.gsu.edu/hbase/curl.html
FourVectors CurlNoiseSIMD( FourVectors const &v );


/// calculate the absolute value of a packed single
[[nodiscard]]
inline fltx4 XM_CALLCONV fabs( DirectX::FXMVECTOR x )
{
	return DirectX::XMVectorAbs( x );
}

/// negate all four components of a SIMD packed single
[[nodiscard]]
inline fltx4 XM_CALLCONV fnegate( DirectX::FXMVECTOR x )
{
	return DirectX::XMVectorNegate( x );
}


[[nodiscard]]
fltx4 XM_CALLCONV Pow_FixedPoint_Exponent_SIMD( DirectX::FXMVECTOR x, int exponent);

// PowSIMD - raise a SIMD register to a power.  This is analogous to the C pow() function, with some
// restictions: fractional exponents are only handled with 2 bits of precision. Basically,
// fractions of 0,.25,.5, and .75 are handled. PowSIMD(x,.30) will be the same as PowSIMD(x,.25).
// negative and fractional powers are handled by the SIMD reciprocal and square root approximation
// instructions and so are not especially accurate ----Note that this routine does not raise
// numeric exceptions because it uses SIMD--- This routine is O(log2(exponent)).
[[nodiscard]]
inline fltx4 XM_CALLCONV PowSIMD( DirectX::FXMVECTOR x, float exponent )
{
	return Pow_FixedPoint_Exponent_SIMD(x, (int)(4.0f * exponent));
}



// random number generation - generate 4 random numbers quickly.

void SeedRandSIMD(uint32 seed);								// seed the random # generator

[[nodiscard]]
fltx4 XM_CALLCONV RandSIMD( int nContext = 0 );							// return 4 numbers in the 0..1 range

// for multithreaded, you need to use these and use the argument form of RandSIMD:
[[nodiscard]] int GetSIMDRandContext();
void ReleaseSIMDRandContext( int nContext );

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV RandSignedSIMD()  // -1..1
{
	return SubSIMD( MulSIMD( Four_Twos, RandSIMD() ), Four_Ones );
}


// SIMD versions of mathlib simplespline functions
// hermite basis function for smooth interpolation
// Similar to Gain() above, but very cheap to call
// value should be between 0 & 1 inclusive
[[nodiscard]]
inline fltx4 XM_CALLCONV SimpleSpline(DirectX::FXMVECTOR value) {
	// Arranged to avoid a data dependency between these two MULs:
	fltx4 valueDoubled = MulSIMD( value, Four_Twos );
	fltx4 valueSquared = MulSIMD( value, value );

	// Nice little ease-in, ease-out spline-like curve
	return MsubSIMD(
		valueDoubled, valueSquared,
		MulSIMD( Four_Threes, valueSquared ) );
}

// remaps a value in [startInterval, startInterval+rangeInterval] from linear to
// spline using SimpleSpline
[[nodiscard]]
inline fltx4 XM_CALLCONV SimpleSplineRemapValWithDeltas( DirectX::FXMVECTOR val,
											 DirectX::FXMVECTOR A, [[maybe_unused]] DirectX::FXMVECTOR BMinusA,
											 DirectX::GXMVECTOR OneOverBMinusA, DirectX::HXMVECTOR C, 
											 DirectX::HXMVECTOR DMinusC )
{
// 	if ( A == B )
// 		return val >= B ? D : C;
	fltx4 cVal = MulSIMD( SubSIMD( val, A), OneOverBMinusA );
	return MaddSIMD( DMinusC, SimpleSpline( cVal ), C );
}

[[nodiscard]]
inline fltx4 XM_CALLCONV SimpleSplineRemapValWithDeltasClamped( DirectX::FXMVECTOR val,
													DirectX::FXMVECTOR A, [[maybe_unused]] DirectX::FXMVECTOR BMinusA,
													DirectX::GXMVECTOR OneOverBMinusA, DirectX::HXMVECTOR C, 
													DirectX::HXMVECTOR DMinusC )
{
// 	if ( A == B )
// 		return val >= B ? D : C;
	fltx4 cVal = MulSIMD( SubSIMD( val, A), OneOverBMinusA );
	cVal = MinSIMD( Four_Ones, MaxSIMD( Four_Zeros, cVal ) );
	return MaddSIMD( DMinusC, SimpleSpline( cVal ), C );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV FracSIMD( DirectX::FXMVECTOR val )
{
	fltx4 fl4Abs = fabs( val );
	fltx4 ival = SubSIMD( AddSIMD( fl4Abs, Four_2ToThe23s ), Four_2ToThe23s );
	ival = MaskedAssign( CmpGtSIMD( ival, fl4Abs ), SubSIMD( ival, Four_Ones ), ival );
	return XorSIMD( SubSIMD( fl4Abs, ival ), XorSIMD( val, fl4Abs ) );			// restore sign bits
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV Mod2SIMD( DirectX::FXMVECTOR val )
{
	fltx4 fl4Abs = fabs( val );
	fltx4 ival = SubSIMD( AndSIMD( DirectX::XMLoadInt4A( g_SIMD_lsbmask ), AddSIMD( fl4Abs, Four_2ToThe23s ) ), Four_2ToThe23s );
	ival = MaskedAssign( CmpGtSIMD( ival, fl4Abs ), SubSIMD( ival, Four_Twos ), ival );
	return XorSIMD( SubSIMD( fl4Abs, ival ), XorSIMD( val, fl4Abs ) );			// restore sign bits
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV Mod2SIMDPositiveInput( DirectX::FXMVECTOR val )
{
	fltx4 ival = SubSIMD( AndSIMD( DirectX::XMLoadInt4A( g_SIMD_lsbmask ), AddSIMD( val, Four_2ToThe23s ) ), Four_2ToThe23s );
	ival = MaskedAssign( CmpGtSIMD( ival, val ), SubSIMD( ival, Four_Twos ), ival );
	return SubSIMD( val, ival );
}


// approximate sin of an angle, with -1..1 representing the whole sin wave period instead of -pi..pi.
// no range reduction is done - for values outside of 0..1 you won't like the results
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV _SinEst01SIMD( DirectX::FXMVECTOR val )
{
	// really rough approximation - x*(4-x*4) - a parabola. s(0) = 0, s(.5) = 1, s(1)=0, smooth in-between.
	// sufficient for simple oscillation.
	return MulSIMD( val, MsubSIMD( val, Four_Fours, Four_Fours ) );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV _Sin01SIMD( DirectX::FXMVECTOR val )
{
	// not a bad approximation : parabola always over-estimates. Squared parabola always
	// underestimates. So lets blend between them:  goodsin = badsin + .225*( badsin^2-badsin)
	fltx4 fl4BadEst = MulSIMD( val, MsubSIMD( val, Four_Fours, Four_Fours ) );
	return MaddSIMD( Four_Point225s, SubSIMD( MulSIMD( fl4BadEst, fl4BadEst ), fl4BadEst ), fl4BadEst );
}

// full range useable implementations
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV SinEst01SIMD( DirectX::FXMVECTOR val )
{
	fltx4 fl4Abs = fabs( val );
	fltx4 fl4Reduced2 = Mod2SIMDPositiveInput( fl4Abs );
	fltx4 fl4OddMask = CmpGeSIMD( fl4Reduced2, Four_Ones );
	fltx4 fl4val = SubSIMD( fl4Reduced2, AndSIMD( Four_Ones, fl4OddMask ) );
	fltx4 fl4Sin = _SinEst01SIMD( fl4val );
	fl4Sin = XorSIMD( fl4Sin, AndSIMD( DirectX::XMLoadInt4A( g_SIMD_signmask ), XorSIMD( val, fl4OddMask ) ) );
	return fl4Sin;

}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV Sin01SIMD( DirectX::FXMVECTOR val )
{
	fltx4 fl4Abs = fabs( val );
	fltx4 fl4Reduced2 = Mod2SIMDPositiveInput( fl4Abs );
	fltx4 fl4OddMask = CmpGeSIMD( fl4Reduced2, Four_Ones );
	fltx4 fl4val = SubSIMD( fl4Reduced2, AndSIMD( Four_Ones, fl4OddMask ) );
	fltx4 fl4Sin = _Sin01SIMD( fl4val );
	fl4Sin = XorSIMD( fl4Sin, AndSIMD( DirectX::XMLoadInt4A( g_SIMD_signmask ), XorSIMD( val, fl4OddMask ) ) );
	return fl4Sin;

}

// Schlick style Bias approximation see graphics gems 4 : bias(t,a)= t/( (1/a-2)*(1-t)+1)
[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV PreCalcBiasParameter( DirectX::FXMVECTOR bias_parameter )
{
	// convert perlin-style-bias parameter to the value right for the approximation
	return SubSIMD( ReciprocalSIMD( bias_parameter ), Four_Twos );
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV BiasSIMD( DirectX::FXMVECTOR val, DirectX::FXMVECTOR precalc_param )
{
	// similar to bias function except pass precalced bias value from calling PreCalcBiasParameter.

	//!!speed!! use reciprocal est?
	//!!speed!! could save one op by precalcing _2_ values
	return DivSIMD( val, MaddSIMD( precalc_param, SubSIMD( Four_Ones, val ), Four_Ones ) );
}

//-----------------------------------------------------------------------------
// Box/plane test 
// NOTE: The w component of emins + emaxs must be 1 for this to work
//-----------------------------------------------------------------------------
[[nodiscard]]
FORCEINLINE int XM_CALLCONV BoxOnPlaneSideSIMD( DirectX::FXMVECTOR emins, DirectX::FXMVECTOR emaxs, const cplane_t *p, float tolerance = 0.f )
{
	fltx4 corners[2];
	fltx4 normal = DirectX::XMLoadFloat3( p->normal.XmBase() );
	fltx4 dist = ReplicateX4( -p->dist );
	normal = SetWSIMD( normal, dist );
	fltx4 t4 = ReplicateX4( tolerance );
	fltx4 negt4 = ReplicateX4( -tolerance );
	fltx4 cmp = CmpGeSIMD( normal, Four_Zeros );
	corners[0] = MaskedAssign( cmp, emaxs, emins );
	corners[1] = MaskedAssign( cmp, emins, emaxs );
	fltx4 dot1 = Dot4SIMD( normal, corners[0] );
	fltx4 dot2 = Dot4SIMD( normal, corners[1] );
	cmp = CmpGeSIMD( dot1, t4 );
	fltx4 cmp2 = CmpGtSIMD( negt4, dot2 );
	fltx4 result = MaskedAssign( cmp, Four_Ones, Four_Zeros );
	fltx4 result2 = MaskedAssign( cmp2, Four_Twos, Four_Zeros );
	result = AddSIMD( result, result2 );
	intx4 sides;
	ConvertStoreAsIntsSIMD( &sides, result );
	return sides[0];
}

[[nodiscard]]
FORCEINLINE fltx4 XM_CALLCONV InvRSquared( DirectX::FXMVECTOR v )
{
	DirectX::XMVECTOR len = DirectX::XMVectorReciprocalSqrt
	(
		// Ensure no division on zero.
		DirectX::XMVectorAdd
		(
			DirectX::g_XMEpsilon,
			DirectX::XMVector3LengthSq( v )
		)
	);
	return DirectX::XMVector3Dot( len, len );
}

[[nodiscard]]
FORCEINLINE vec_t XM_CALLCONV InvRSquared( Vector v )
{
	return DirectX::XMVectorGetX( InvRSquared( DirectX::XMLoadFloat3( v.XmBase() ) ) );
}

[[nodiscard]]
FORCEINLINE vec_t XM_CALLCONV InvRSquared( VectorAligned v )
{
	return DirectX::XMVectorGetX( InvRSquared( DirectX::XMLoadFloat4A( v.XmBase() ) ) );
}

inline void XM_CALLCONV VectorNegate( DirectX::XMFLOAT4 *v )
{
	Assert(v);
	DirectX::XMStoreFloat4( v, DirectX::XMVectorNegate( DirectX::XMLoadFloat4( v ) ) );
}

#endif // _ssemath_h
