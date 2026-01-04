//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: - defines SIMD "structure of arrays" classes and functions.
//
//===========================================================================//
#ifndef SSEQUATMATH_H
#define SSEQUATMATH_H

#include "ssemath.h"

// If you compile assuming the presence of SSE2, the MSVC will abandon
// the traditional x87 FPU operations altogether and make everything use
// the SSE2 registers, which lessens this problem a little.

// dimhotepus: Enable SSE quaternions as we use SSE2+ for all code.
#define ALLOW_SIMD_QUATERNION_MATH 1

//---------------------------------------------------------------------
// Load/store quaternions
//---------------------------------------------------------------------

#if ALLOW_SIMD_QUATERNION_MATH

// Using STDC or SSE
[[nodiscard]] FORCEINLINE fltx4 XM_CALLCONV LoadAlignedSIMD( const QuaternionAligned & pSIMD )
{
	return DirectX::XMLoadFloat4A( pSIMD.XmBase() );
}

[[nodiscard]] FORCEINLINE fltx4 XM_CALLCONV LoadAlignedSIMD( const QuaternionAligned * RESTRICT pSIMD )
{
	return DirectX::XMLoadFloat4A( pSIMD->XmBase() );
}

FORCEINLINE void XM_CALLCONV StoreAlignedSIMD( QuaternionAligned * RESTRICT pSIMD, fltx4 a )
{
	DirectX::XMStoreFloat4A( pSIMD->XmBase(), a );
}

//---------------------------------------------------------------------
// Make sure quaternions are within 180 degrees of one another, if not, reverse q
//---------------------------------------------------------------------
[[nodiscard]] FORCEINLINE fltx4 XM_CALLCONV QuaternionAlignSIMD( fltx4 p, fltx4 q )
{
	// decide if one of the quaternions is backwards
	fltx4 a = SubSIMD( p, q );
	fltx4 b = AddSIMD( p, q );
	a = Dot4SIMD( a, a );
	b = Dot4SIMD( b, b );
	fltx4 cmp = CmpGtSIMD( a, b );
	fltx4 result = MaskedAssign( cmp, NegSIMD(q), q );
	return result;
}

//---------------------------------------------------------------------
// Normalize Quaternion
//---------------------------------------------------------------------
#if USE_STDC_FOR_SIMD

[[nodiscard]] FORCEINLINE fltx4 XM_CALLCONV QuaternionNormalizeSIMD( const fltx4 &q )
{
	fltx4 radius, result;
	radius = Dot4SIMD( q, q );

	if ( SubFloat( radius, 0 ) ) // > FLT_EPSILON && ((radius < 1.0f - 4*FLT_EPSILON) || (radius > 1.0f + 4*FLT_EPSILON))
	{
		float iradius = 1.0f / sqrtf( SubFloat( radius, 0 ) );
		result = ReplicateX4( iradius );
		result = MulSIMD( result, q );
		return result;
	}
	return q;
}

#else

[[nodiscard]] FORCEINLINE fltx4 XM_CALLCONV QuaternionNormalizeSIMD( fltx4 q )
{
	fltx4 radiusSq = DirectX::XMVector4Dot( q, q );
	fltx4 zeroMask = DirectX::XMVectorEqual( radiusSq, Four_Zeros ); // all ones iff radius = 0
	fltx4 result = DirectX::XMVectorMultiply( ReciprocalSqrtSIMD( radiusSq ), q );
	return MaskedAssign( zeroMask, q, result );	// if radius was 0, just return q
	// dimhotepus: DirectX behavior differs here. Returns infinity quaternion when radius is 0.
	// return DirectX::XMQuaternionNormalize( q );
}

#endif


//---------------------------------------------------------------------
// 0.0 returns p, 1.0 return q.
//---------------------------------------------------------------------
[[nodiscard]] FORCEINLINE fltx4 XM_CALLCONV QuaternionBlendNoAlignSIMD( fltx4 p, fltx4 q, float t )
{
	fltx4 sclq = DirectX::XMVectorReplicate( t ); //-V2002
	fltx4 sclp = DirectX::XMVectorSubtract( Four_Ones, sclq );
	fltx4 result = DirectX::XMVectorMultiply( sclp, p );
	result = DirectX::XMVectorMultiplyAdd( sclq, q, result );
	return QuaternionNormalizeSIMD( result );
}


//---------------------------------------------------------------------
// Blend Quaternions
//---------------------------------------------------------------------
[[nodiscard]] FORCEINLINE fltx4 XM_CALLCONV QuaternionBlendSIMD( fltx4 p, fltx4 q, float t )
{
	// decide if one of the quaternions is backwards
	fltx4 q2 = QuaternionAlignSIMD( p, q );
	fltx4 result = QuaternionBlendNoAlignSIMD( p, q2, t );
	return result;
}


//---------------------------------------------------------------------
// Multiply Quaternions
//---------------------------------------------------------------------

// SSE and STDC
[[nodiscard]] FORCEINLINE fltx4 XM_CALLCONV QuaternionMultSIMD( fltx4 p, fltx4 q )
{
	// decide if one of the quaternions is backwards
	fltx4 q1 = QuaternionAlignSIMD( p, q );

	// Yep, q1, then p.
	return DirectX::XMQuaternionMultiply( q1, p );
}

//---------------------------------------------------------------------
// Quaternion scale
//---------------------------------------------------------------------

// SSE and STDC
[[nodiscard]] FORCEINLINE fltx4 XM_CALLCONV QuaternionScaleSIMD( fltx4 p, float t )
{
	// FIXME: nick, this isn't overly sensitive to accuracy, and it may be faster to 
	// use the cos part (w) of the quaternion (sin(omega)*N,cos(omega)) to figure the new scale.
	//float sinom = sqrtf( SubFloat( p, 0 ) * SubFloat( p, 0 ) + SubFloat( p, 1 ) * SubFloat( p, 1 ) + SubFloat( p, 2 ) * SubFloat( p, 2 ) );
	//sinom = min( sinom, 1.f );

	//float sinsom = sinf( asinf( sinom ) * t );

	//t = sinsom / (sinom + FLT_EPSILON);

	//fltx4 q;
	//SubFloat( q, 0 ) = t * SubFloat( p, 0 );
	//SubFloat( q, 1 ) = t * SubFloat( p, 1 );
	//SubFloat( q, 2 ) = t * SubFloat( p, 2 );

	//// rescale rotation
	//float r = 1.0f - sinsom * sinsom;

	//// Assert( r >= 0 );
	//if (r < 0.0f) 
	//	r = 0.0f;
	//r = sqrtf( r );

	//// keep sign of rotation
	//SubFloat( q, 3 ) = fsel( SubFloat( p, 3 ), r, -r );
	//return q;

	// dimhotepus: More SSE below.

	// FIXME: nick, this isn't overly sensitive to accuracy, and it may be faster to 
	// use the cos part (w) of the quaternion (sin(omega)*N,cos(omega)) to figure the new scale.
	fltx4 sinom = DirectX::XMVectorMin( DirectX::XMVectorSqrt( DirectX::XMVector3Dot( p, p ) ), DirectX::g_XMOne );
	fltx4 sinsom = DirectX::XMVectorSin( DirectX::XMVectorScale( DirectX::XMVectorASin( sinom ), t ) );
	
	fltx4 tvec = DirectX::XMVectorDivide( sinsom, DirectX::XMVectorAdd( sinom, DirectX::g_XMEpsilon ) );
	fltx4 q = DirectX::XMVectorMultiply( tvec, p );
	
	// rescale rotation
	fltx4 rr = DirectX::XMVectorSubtract( DirectX::g_XMOne, DirectX::XMVectorMultiply( sinsom, sinsom ) );
	
	// Assert( rr >= 0 );
	rr = DirectX::XMVectorSqrt
	(
		MaskedAssign
		(
			DirectX::XMVectorLess( rr, DirectX::g_XMZero ),
			DirectX::g_XMZero,
			rr
		)
	);
	
	fltx4 sign = DirectX::XMVectorSwizzle
	<
		DirectX::XM_SWIZZLE_W,
		DirectX::XM_SWIZZLE_W,
		DirectX::XM_SWIZZLE_W,
		DirectX::XM_SWIZZLE_W
	>
	(
		MaskedAssign
		(
			DirectX::XMVectorGreaterOrEqual( p, DirectX::g_XMZero ),
			rr,
			DirectX::XMVectorNegate( rr )
		)
	);
	
	// keep sign of rotation
	q = DirectX::XMVectorPermute
	<
		DirectX::XM_PERMUTE_0X,
		DirectX::XM_PERMUTE_0Y,
		DirectX::XM_PERMUTE_0Z,
		DirectX::XM_PERMUTE_1X
	>( q, sign );
	
	return q;
}


//-----------------------------------------------------------------------------
// Quaternion sphereical linear interpolation
//-----------------------------------------------------------------------------

// SSE and STDC
[[nodiscard]] FORCEINLINE fltx4 XM_CALLCONV QuaternionSlerpNoAlignSIMD( fltx4 p, fltx4 q, float t )
{
	// dimhotepus: More SSE below:

	// 0.0 returns p, 1.0 return q.
	fltx4 result = DirectX::XMVector4Dot( p, q );
	float cosom = DirectX::XMVectorGetX( result ); //-V2002

	if ( cosom > -0.099999f )
	{
		if ( 0.099999f > cosom )
		{
			float omega = acosf( cosom );
			float sinom = sinf( omega );

			const float tom = t * omega;

			float sclp = sinf( omega - tom ) / sinom;
			float sclq = sinf( tom ) / sinom;

			result = DirectX::XMVectorAdd( DirectX::XMVectorScale( p, sclp ), DirectX::XMVectorScale( q, sclq ) );
		}
		else 
		{
			result = DirectX::XMVectorAdd( DirectX::XMVectorScale( p, 1.0f - t ), DirectX::XMVectorScale( q, t ) );
		}

		return result;
	}

	{
		SubFloat( result, 0 ) = -SubFloat( q, 1 );
		SubFloat( result, 1 ) =  SubFloat( q, 0 );
		SubFloat( result, 2 ) = -SubFloat( q, 3 );

		constexpr float halfPi = 0.5f * M_PI_F;
		const float tt = t * halfPi;

		float sclp = sinf( halfPi - tt );
		float sclq = sinf( tt );

		result = DirectX::XMVectorAdd( DirectX::XMVectorScale( p, sclp ), DirectX::XMVectorScale( result, sclq ) );
		result = DirectX::XMVectorPermute
		<
			DirectX::XM_PERMUTE_0X,
			DirectX::XM_PERMUTE_0Y,
			DirectX::XM_PERMUTE_0Z,
			DirectX::XM_PERMUTE_1Z
		>( result, q );
		
		return result;
	}
}


[[nodiscard]] FORCEINLINE fltx4 XM_CALLCONV QuaternionSlerpSIMD( fltx4 p, fltx4 q, float t )
{
	fltx4 q2 = QuaternionAlignSIMD( p, q );
	fltx4 result = QuaternionSlerpNoAlignSIMD( p, q2, t );
	return result;
}


#endif // ALLOW_SIMD_QUATERNION_MATH

#endif // SSEQUATMATH_H

