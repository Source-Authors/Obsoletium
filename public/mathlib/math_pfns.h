//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef _MATH_PFNS_H_
#define _MATH_PFNS_H_

#include <DirectXMath.h>
#include <intrin.h>

namespace details
{

inline float SSE_Sqrt(float x)
{
	float root;
	_mm_store_ss( &root, _mm_sqrt_ss( _mm_load_ss( &x ) ) );
	return root;
}

inline const __m128  f3  = _mm_set_ss(3.0f);  // 3 as SSE value
inline const __m128  f05 = _mm_set_ss(0.5f);  // 0.5 as SSE value

// Intel / Kipps SSE RSqrt.  Significantly faster than above.
inline float SSE_RSqrtAccurate(float a)
{
	__m128 xx = _mm_load_ss( &a );
    __m128 xr = _mm_rsqrt_ss( xx );

    __m128 xt = _mm_mul_ss( xr, xr );
    xt = _mm_mul_ss( xt, xx );
    xt = _mm_sub_ss( f3, xt );
    xt = _mm_mul_ss( xt, f05 );
    xr = _mm_mul_ss( xr, xt );

	float rroot;
    _mm_store_ss( &rroot, xr );
    return rroot;
}

// Simple SSE rsqrt.  Usually accurate to around 6 (relative) decimal places 
// or so, so ok for closed transforms.  (ie, computing lighting normals)
inline float SSE_RSqrtFast(float x)
{
	float rroot;
	_mm_store_ss( &rroot, _mm_rsqrt_ss( _mm_load_ss(&x) ) );
	return rroot;
}

}  // namespace details

// The following are not declared as macros because they are often used in limiting situations,
// and sometimes the compiler simply refuses to inline them for some reason
#define FastSqrt(x)			details::SSE_Sqrt(x)
#define	FastRSqrt(x)		details::SSE_RSqrtAccurate(x)
#define FastRSqrtFast(x)    details::SSE_RSqrtFast(x)
#define FastSinCos(x, s, c) DirectX::XMScalarSinCos(s, c, x)
#define FastCos(x)			DirectX::XMScalarCos(x)

#endif // _MATH_PFNS_H_
