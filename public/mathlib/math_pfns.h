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

// MIT License
// 
// Copyright (c) 2015-2024 SSE2NEON Contributors
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Compute the square root of packed single-precision (32-bit) floating-point
// elements in a, and store the results in dst.
// Due to ARMv7-A NEON's lack of a precise square root intrinsic, we implement
// square root by multiplying input in with its reciprocal square root before
// using the Newton-Raphson method to approximate the results.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sqrt_ps
inline DirectX::XMVECTOR XM_CALLCONV SSE_Sqrt_PS( DirectX::XMVECTOR in )
{
#if defined(_XM_ARM_NEON_INTRINSICS_)
#if (defined(__aarch64__) || defined(_M_ARM64)) && !SSE2NEON_PRECISE_SQRT
  return vreinterpretq_m128_f32(vsqrtq_f32(vreinterpretq_f32_m128(in)));
#else
  float32x4_t recip = vrsqrteq_f32(vreinterpretq_f32_m128(in));

  // Test for vrsqrteq_f32(0) -> positive infinity case.
  // Change to zero, so that s * 1/sqrt(s) result is zero too.
  const uint32x4_t pos_inf = vdupq_n_u32(0x7F800000);
  const uint32x4_t div_by_zero =
	  vceqq_u32(pos_inf, vreinterpretq_u32_f32(recip));
  recip = vreinterpretq_f32_u32(
	  vandq_u32(vmvnq_u32(div_by_zero), vreinterpretq_u32_f32(recip)));

  recip = vmulq_f32(
	  vrsqrtsq_f32(vmulq_f32(recip, recip), vreinterpretq_f32_m128(in)), recip);
  // Additional Netwon-Raphson iteration for accuracy
  recip = vmulq_f32(
	  vrsqrtsq_f32(vmulq_f32(recip, recip), vreinterpretq_f32_m128(in)), recip);

  // sqrt(s) = s * 1/sqrt(s)
  return vreinterpretq_m128_f32(vmulq_f32(vreinterpretq_f32_m128(in), recip));
#endif
#elif defined(_XM_SSE_INTRINSICS_)
	return _mm_sqrt_ps(in);
#else
	return
	{
		sqrtf(in.m128_f32[0]),
		sqrtf(in.m128_f32[1]),
		sqrtf(in.m128_f32[2]),
		sqrtf(in.m128_f32[3])
	};
#endif
}

// Compute the square root of the lower single-precision (32-bit) floating-point
// element in a, store the result in the lower element of dst, and copy the
// upper 3 packed elements from a to the upper elements of dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_sqrt_ss
inline DirectX::XMVECTOR XM_CALLCONV SSE_Sqrt_SS( DirectX::XMVECTOR in )
{
#if defined(_XM_ARM_NEON_INTRINSICS_)
	float32_t value = vgetq_lane_f32(vreinterpretq_f32_m128(SSE_Sqrt_PS(in)), 0);
	return vreinterpretq_m128_f32(
		vsetq_lane_f32(value, vreinterpretq_f32_m128(in), 0));
#elif defined(_XM_SSE_INTRINSICS_)
	return _mm_sqrt_ss(in);
#else
	return
	{
		sqrtf( in.m128_f32[0] ),
		in.m128_f32[1],
		in.m128_f32[2],
		in.m128_f32[3]
	};
#endif
}

// Compute the approximate reciprocal square root of packed single-precision
// (32-bit) floating-point elements in a, and store the results in dst. The
// maximum relative error for this approximation is less than 1.5*2^-12.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_rsqrt_ps
inline DirectX::XMVECTOR XM_CALLCONV SSE_RSqrt_PS( DirectX::XMVECTOR in )
{
#if defined(_XM_ARM_NEON_INTRINSICS_)
	float32x4_t out = vrsqrteq_f32(vreinterpretq_f32_m128(in));
#if SSE2NEON_PRECISE_SQRT
	// Additional Netwon-Raphson iteration for accuracy
	out = vmulq_f32(
		out, vrsqrtsq_f32(vmulq_f32(vreinterpretq_f32_m128(in), out), out));
	out = vmulq_f32(
		out, vrsqrtsq_f32(vmulq_f32(vreinterpretq_f32_m128(in), out), out));
#endif
	return vreinterpretq_m128_f32(out);
#elif defined(_XM_SSE_INTRINSICS_)
	return _mm_rsqrt_ps(in);
#else
	return
	{
		1.0f / ::sqrtf( in.m128_f32[0] ),
		1.0f / ::sqrtf( in.m128_f32[1] ),
		1.0f / ::sqrtf( in.m128_f32[2] ),
		1.0f / ::sqrtf( in.m128_f32[3] )
	};
#endif
}

// Compute the approximate reciprocal square root of the lower single-precision
// (32-bit) floating-point element in a, store the result in the lower element
// of dst, and copy the upper 3 packed elements from a to the upper elements of
// dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_rsqrt_ss
inline DirectX::XMVECTOR XM_CALLCONV SSE_RSqrt_SS( DirectX::XMVECTOR in )
{
#if defined(_XM_ARM_NEON_INTRINSICS_)
	return vsetq_lane_f32(vgetq_lane_f32(SSE_RSqrt_PS(in), 0), in, 0);
#elif defined(_XM_SSE_INTRINSICS_)
	return _mm_rsqrt_ss(in);
#else
	return
	{
		1.0f / ::sqrtf( in.m128_f32[0] ),
		in.m128_f32[1],
		in.m128_f32[2],
		in.m128_f32[3]
	};
#endif
}

inline float SSE_Sqrt(float x)
{
	return DirectX::XMVectorGetX( SSE_Sqrt_SS( DirectX::XMLoadFloat( &x ) ) );
}

inline const DirectX::XMVECTOR  f3  = DirectX::XMVectorSet( 3.0f, 0.0f, 0.0f, 0.0f );  // 3 as SSE value
inline const DirectX::XMVECTOR  f05 = DirectX::XMVectorSet( 0.5f, 0.0f, 0.0f, 0.0f );  // 0.5 as SSE value

// Intel / Kipps SSE RSqrt.  Significantly faster than above.
inline float SSE_RSqrtAccurate(float a)
{
	DirectX::XMVECTOR xx = DirectX::XMLoadFloat( &a );
	DirectX::XMVECTOR xr = SSE_RSqrt_SS( xx );

	DirectX::XMVECTOR xt = DirectX::XMVectorMultiply( xr, xr );
	xt = DirectX::XMVectorMultiply( xt, xx );
	xt = DirectX::XMVectorSubtract( f3, xt );
	xt = DirectX::XMVectorMultiply( xt, f05 );
	xr = DirectX::XMVectorMultiply( xr, xt );

	return DirectX::XMVectorGetX( xr );
}

// Simple SSE rsqrt.  Usually accurate to around 6 (relative) decimal places 
// or so, so ok for closed transforms.  (ie, computing lighting normals)
inline float SSE_RSqrtFast(float x)
{
	return DirectX::XMVectorGetX( SSE_RSqrt_SS( DirectX::XMLoadFloat( &x ) ) );
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
