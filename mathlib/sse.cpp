//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SSE Math primitives.
//
//=====================================================================================//

#include <float.h>	// Needed for FLT_EPSILON
#include <memory.h>
#include <cmath>

#include <DirectXMath.h>

#include "tier0/basetypes.h"
#include "tier0/dbg.h"

#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "sse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef COMPILER_MSVC64
// Implement for 64-bit Windows if needed.

static const uint32 _sincos_masks[]	  = { (uint32)0x0,  (uint32)~0x0 };
static const uint32 _sincos_inv_masks[] = { (uint32)~0x0, (uint32)0x0 };

//-----------------------------------------------------------------------------
// Macros and constants required by some of the SSE assembly:
//-----------------------------------------------------------------------------

#ifdef _WIN32
	#define _PS_EXTERN_CONST(Name, Val) \
		const __declspec(align(16)) float _ps_##Name[4] = { Val, Val, Val, Val }

	#define _PS_EXTERN_CONST_TYPE(Name, Type, Val) \
		const __declspec(align(16)) Type _ps_##Name[4] = { Val, Val, Val, Val }; \

	#define _EPI32_CONST(Name, Val) \
		static const __declspec(align(16)) __int32 _epi32_##Name[4] = { Val, Val, Val, Val }

	#define _PS_CONST(Name, Val) \
		static const __declspec(align(16)) float _ps_##Name[4] = { Val, Val, Val, Val }
#elif POSIX
	#define _PS_EXTERN_CONST(Name, Val) \
		const float _ps_##Name[4] __attribute__((aligned(16))) = { Val, Val, Val, Val }

	#define _PS_EXTERN_CONST_TYPE(Name, Type, Val) \
		const Type _ps_##Name[4]  __attribute__((aligned(16))) = { Val, Val, Val, Val }; \

	#define _EPI32_CONST(Name, Val) \
		static const int32 _epi32_##Name[4]  __attribute__((aligned(16))) = { Val, Val, Val, Val }

	#define _PS_CONST(Name, Val) \
		static const float _ps_##Name[4]  __attribute__((aligned(16))) = { Val, Val, Val, Val }
#endif

_PS_EXTERN_CONST(am_0, 0.0f);
_PS_EXTERN_CONST(am_1, 1.0f);
_PS_EXTERN_CONST(am_m1, -1.0f);
_PS_EXTERN_CONST(am_0p5, 0.5f);
_PS_EXTERN_CONST(am_1p5, 1.5f);
_PS_EXTERN_CONST(am_pi, (float)M_PI);
_PS_EXTERN_CONST(am_pi_o_2, (float)(M_PI / 2.0));
_PS_EXTERN_CONST(am_2_o_pi, (float)(2.0 / M_PI));
_PS_EXTERN_CONST(am_pi_o_4, (float)(M_PI / 4.0));
_PS_EXTERN_CONST(am_4_o_pi, (float)(4.0 / M_PI));
_PS_EXTERN_CONST_TYPE(am_sign_mask, uint32, 0x80000000);
_PS_EXTERN_CONST_TYPE(am_inv_sign_mask, uint32, ~0x80000000);
_PS_EXTERN_CONST_TYPE(am_min_norm_pos,uint32, 0x00800000);
_PS_EXTERN_CONST_TYPE(am_mant_mask, uint32, 0x7f800000);
_PS_EXTERN_CONST_TYPE(am_inv_mant_mask, int32, ~0x7f800000);

_EPI32_CONST(1, 1);
_EPI32_CONST(2, 2);

_PS_CONST(sincos_p0, 0.15707963267948963959e1f);
_PS_CONST(sincos_p1, -0.64596409750621907082e0f);
_PS_CONST(sincos_p2, 0.7969262624561800806e-1f);
_PS_CONST(sincos_p3, -0.468175413106023168e-2f);

#ifdef PFN_VECTORMA
void  __cdecl _SSE_VectorMA( const float *start, float scale, const float *direction, float *dest );
#endif

//-----------------------------------------------------------------------------
// SSE implementations of optimized routines:
//-----------------------------------------------------------------------------
float _SSE_Sqrt(float x)
{
	float	root = 0.f;
#ifdef _WIN32
	__asm
	{
		sqrtss		xmm0, x
		movss		root, xmm0
	}
#elif POSIX
	_mm_store_ss( &root, _mm_sqrt_ss( _mm_load_ss( &x ) ) );
#endif
	return root;
}

#ifdef POSIX
const __m128  f3  = _mm_set_ss(3.0f);  // 3 as SSE value
const __m128  f05 = _mm_set_ss(0.5f);  // 0.5 as SSE value
#endif

// Intel / Kipps SSE RSqrt.  Significantly faster than above.
float _SSE_RSqrtAccurate(float a)
{

#ifdef _WIN32
	float x;
	float half = 0.5f;
	float three = 3.f;

	__asm
	{
		movss   xmm3, a;
		movss   xmm1, half;
		movss   xmm2, three;
		rsqrtss xmm0, xmm3;

		mulss   xmm3, xmm0;
		mulss   xmm1, xmm0;
		mulss   xmm3, xmm0;
		subss   xmm2, xmm3;
		mulss   xmm1, xmm2;

		movss   x,    xmm1;
	}

	return x;
#elif POSIX	
	__m128  xx = _mm_load_ss( &a );
    __m128  xr = _mm_rsqrt_ss( xx );
    __m128  xt;
	
    xt = _mm_mul_ss( xr, xr );
    xt = _mm_mul_ss( xt, xx );
    xt = _mm_sub_ss( f3, xt );
    xt = _mm_mul_ss( xt, f05 );
    xr = _mm_mul_ss( xr, xt );
	
    _mm_store_ss( &a, xr );
    return a;
#else
	#error "Not Implemented"
#endif

}

// Simple SSE rsqrt.  Usually accurate to around 6 (relative) decimal places 
// or so, so ok for closed transforms.  (ie, computing lighting normals)
float _SSE_RSqrtFast(float x)
{
	float rroot;
#ifdef _WIN32
	__asm
	{
		rsqrtss	xmm0, x
		movss	rroot, xmm0
	}
#elif POSIX
	__asm__ __volatile__( "rsqrtss %0, %1" : "=x" (rroot) : "x" (x) );
#else
#error "Please define your platform"
#endif

	return rroot;
}

float FASTCALL _SSE_VectorNormalize (Vector& vec)
{
	// NOTE: This is necessary to prevent an memory overwrite...
	// sice vec only has 3 floats, we can't "movaps" directly into it.
#ifdef _WIN32
	alignas(16) float result[4];
#elif POSIX
	 float result[4] __attribute__((aligned(16)));
#endif

	float *v = &vec[0];
#ifdef _WIN32
	float *r = &result[0];
#endif

	float	radius = 0.f;
	// Blah, get rid of these comparisons ... in reality, if you have all 3 as zero, it shouldn't 
	// be much of a performance win, considering you will very likely miss 3 branch predicts in a row.
	if ( v[0] || v[1] || v[2] )
	{
#ifdef _WIN32
	__asm
		{
			mov			eax, v
			mov			edx, r
#ifdef ALIGNED_VECTOR
			movaps		xmm4, [eax]			// r4 = vx, vy, vz, X
			movaps		xmm1, xmm4			// r1 = r4
#else
			movups		xmm4, [eax]			// r4 = vx, vy, vz, X
			movaps		xmm1, xmm4			// r1 = r4
#endif
			mulps		xmm1, xmm4			// r1 = vx * vx, vy * vy, vz * vz, X
			movhlps		xmm3, xmm1			// r3 = vz * vz, X, X, X
			movaps		xmm2, xmm1			// r2 = r1
			shufps		xmm2, xmm2, 1		// r2 = vy * vy, X, X, X
			addss		xmm1, xmm2			// r1 = (vx * vx) + (vy * vy), X, X, X
			addss		xmm1, xmm3			// r1 = (vx * vx) + (vy * vy) + (vz * vz), X, X, X
			sqrtss		xmm1, xmm1			// r1 = sqrt((vx * vx) + (vy * vy) + (vz * vz)), X, X, X
			movss		radius, xmm1		// radius = sqrt((vx * vx) + (vy * vy) + (vz * vz))
			rcpss		xmm1, xmm1			// r1 = 1/radius, X, X, X
			shufps		xmm1, xmm1, 0		// r1 = 1/radius, 1/radius, 1/radius, X
			mulps		xmm4, xmm1			// r4 = vx * 1/radius, vy * 1/radius, vz * 1/radius, X
			movaps		[edx], xmm4			// v = vx * 1/radius, vy * 1/radius, vz * 1/radius, X
		}
#elif POSIX
		__asm__ __volatile__(
#ifdef ALIGNED_VECTOR
            "movaps          %2, %%xmm4 \n\t"
            "movaps          %%xmm4, %%xmm1 \n\t"
#else
            "movups          %2, %%xmm4 \n\t"
            "movaps          %%xmm4, %%xmm1 \n\t"
#endif
            "mulps           %%xmm4, %%xmm1 \n\t"
            "movhlps         %%xmm1, %%xmm3 \n\t"
            "movaps          %%xmm1, %%xmm2 \n\t"
            "shufps          $1, %%xmm2, %%xmm2 \n\t"
            "addss           %%xmm2, %%xmm1 \n\t"
            "addss           %%xmm3, %%xmm1 \n\t"
            "sqrtss          %%xmm1, %%xmm1 \n\t"
            "movss           %%xmm1, %0 \n\t"
            "rcpss           %%xmm1, %%xmm1 \n\t"
            "shufps          $0, %%xmm1, %%xmm1 \n\t"
            "mulps           %%xmm1, %%xmm4 \n\t"
            "movaps          %%xmm4, %1 \n\t"
            : "=m" (radius), "=m" (result)
            : "m" (*v)
            : "xmm1", "xmm2", "xmm3", "xmm4"
 		);
#else
	#error "Not Implemented"
#endif
		vec.x = result[0];
		vec.y = result[1];
		vec.z = result[2];

	}

	return radius;
}

void FASTCALL _SSE_VectorNormalizeFast (Vector& vec)
{
	float ool = _SSE_RSqrtAccurate( FLT_EPSILON + vec.x * vec.x + vec.y * vec.y + vec.z * vec.z );

	vec.x *= ool;
	vec.y *= ool;
	vec.z *= ool;
}

float _SSE_InvRSquared(const float* v)
{
	float	inv_r2 = 1.f;
#ifdef _WIN32
	__asm { // Intel SSE only routine
		mov			eax, v
		movss		xmm5, inv_r2		// x5 = 1.0, 0, 0, 0
#ifdef ALIGNED_VECTOR
		movaps		xmm4, [eax]			// x4 = vx, vy, vz, X
#else
		movups		xmm4, [eax]			// x4 = vx, vy, vz, X
#endif
		movaps		xmm1, xmm4			// x1 = x4
		mulps		xmm1, xmm4			// x1 = vx * vx, vy * vy, vz * vz, X
		movhlps		xmm3, xmm1			// x3 = vz * vz, X, X, X
		movaps		xmm2, xmm1			// x2 = x1
		shufps		xmm2, xmm2, 1		// x2 = vy * vy, X, X, X
		addss		xmm1, xmm2			// x1 = (vx * vx) + (vy * vy), X, X, X
		addss		xmm1, xmm3			// x1 = (vx * vx) + (vy * vy) + (vz * vz), X, X, X
		maxss		xmm1, xmm5			// x1 = max( 1.0, x1 )
		rcpss		xmm0, xmm1			// x0 = 1 / max( 1.0, x1 )
		movss		inv_r2, xmm0		// inv_r2 = x0
	}
#elif POSIX
		__asm__ __volatile__(
		"movss			 %0, %%xmm5 \n\t"
#ifdef ALIGNED_VECTOR
		"movaps          %1, %%xmm4 \n\t"
#else
		"movups          %1, %%xmm4 \n\t"
#endif
        "movaps          %%xmm4, %%xmm1 \n\t"
        "mulps           %%xmm4, %%xmm1 \n\t"
		"movhlps         %%xmm1, %%xmm3 \n\t"
		"movaps          %%xmm1, %%xmm2 \n\t"
        "shufps          $1, %%xmm2, %%xmm2 \n\t"
        "addss           %%xmm2, %%xmm1 \n\t"
        "addss           %%xmm3, %%xmm1 \n\t"
		"maxss           %%xmm5, %%xmm1 \n\t"
        "rcpss           %%xmm1, %%xmm0 \n\t"
		"movss           %%xmm0, %0 \n\t" 
        : "+m" (inv_r2)
        : "m" (*v)
        : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
 		);
#else
	#error "Not Implemented"
#endif

	return inv_r2;
}


#ifdef POSIX
// #define _PS_CONST(Name, Val) static const ALIGN16 float _ps_##Name[4] ALIGN16_POST = { Val, Val, Val, Val }
#define _PS_CONST_TYPE(Name, Type, Val) static const ALIGN16 Type _ps_##Name[4] ALIGN16_POST = { Val, Val, Val, Val }

_PS_CONST_TYPE(sign_mask, int, (int)0x80000000);
_PS_CONST_TYPE(inv_sign_mask, int, ~0x80000000);


#define _PI32_CONST(Name, Val)  static const ALIGN16 int _pi32_##Name[4]  ALIGN16_POST = { Val, Val, Val, Val }

_PI32_CONST(1, 1);
_PI32_CONST(inv1, ~1);
_PI32_CONST(2, 2);
_PI32_CONST(4, 4);
_PI32_CONST(0x7f, 0x7f);
_PS_CONST(1  , 1.0f);
_PS_CONST(0p5, 0.5f);

_PS_CONST(minus_cephes_DP1, -0.78515625);
_PS_CONST(minus_cephes_DP2, -2.4187564849853515625e-4);
_PS_CONST(minus_cephes_DP3, -3.77489497744594108e-8);
_PS_CONST(sincof_p0, -1.9515295891E-4);
_PS_CONST(sincof_p1,  8.3321608736E-3);
_PS_CONST(sincof_p2, -1.6666654611E-1);
_PS_CONST(coscof_p0,  2.443315711809948E-005);
_PS_CONST(coscof_p1, -1.388731625493765E-003);
_PS_CONST(coscof_p2,  4.166664568298827E-002);
_PS_CONST(cephes_FOPI, 1.27323954473516); // 4 / M_PI

typedef union xmm_mm_union {
	__m128 xmm;
	__m64 mm[2];
} xmm_mm_union;

#define COPY_MM_TO_XMM(mm0_, mm1_, xmm_) { xmm_mm_union u; u.mm[0]=mm0_; u.mm[1]=mm1_; xmm_ = u.xmm; }

typedef __m128 v4sf;  // vector of 4 float (sse1)
typedef __m64 v2si;   // vector of 2 int (mmx)

#endif

//-----------------------------------------------------------------------------
// SSE2 implementations of optimized routines:
//-----------------------------------------------------------------------------

#ifdef _WIN32
void _declspec(naked) _SSE_VectorMA( const float *start, float scale, const float *direction, float *dest )
{
	// FIXME: This don't work!! It will overwrite memory in the write to dest
	Assert(0);

	__asm {  // Intel SSE only routine
		mov	eax, DWORD PTR [esp+0x04]	; *start, s0..s2
		mov ecx, DWORD PTR [esp+0x0c]	; *direction, d0..d2
		mov edx, DWORD PTR [esp+0x10]	; *dest
		movss	xmm2, [esp+0x08]		; x2 = scale, 0, 0, 0
#ifdef ALIGNED_VECTOR
		movaps	xmm3, [ecx]				; x3 = dir0,dir1,dir2,X
		pshufd	xmm2, xmm2, 0			; x2 = scale, scale, scale, scale
		movaps	xmm1, [eax]				; x1 = start1, start2, start3, X
		mulps	xmm3, xmm2				; x3 *= x2
		addps	xmm3, xmm1				; x3 += x1
		movaps	[edx], xmm3				; *dest = x3
#else
		movups	xmm3, [ecx]				; x3 = dir0,dir1,dir2,X
		pshufd	xmm2, xmm2, 0			; x2 = scale, scale, scale, scale
		movups	xmm1, [eax]				; x1 = start1, start2, start3, X
		mulps	xmm3, xmm2				; x3 *= x2
		addps	xmm3, xmm1				; x3 += x1
		movups	[edx], xmm3				; *dest = x3
#endif
	}
}
#endif

#ifdef _WIN32
#ifdef PFN_VECTORMA
void _declspec(naked) __cdecl _SSE_VectorMA( const Vector &start, float scale, const Vector &direction, Vector &dest )
{
	// FIXME: This don't work!! It will overwrite memory in the write to dest
	Assert(0);

	__asm 
	{  
		// Intel SSE only routine
		mov	eax, DWORD PTR [esp+0x04]	; *start, s0..s2
		mov ecx, DWORD PTR [esp+0x0c]	; *direction, d0..d2
		mov edx, DWORD PTR [esp+0x10]	; *dest
		movss	xmm2, [esp+0x08]		; x2 = scale, 0, 0, 0
#ifdef ALIGNED_VECTOR
		movaps	xmm3, [ecx]				; x3 = dir0,dir1,dir2,X
		pshufd	xmm2, xmm2, 0			; x2 = scale, scale, scale, scale
		movaps	xmm1, [eax]				; x1 = start1, start2, start3, X
		mulps	xmm3, xmm2				; x3 *= x2
		addps	xmm3, xmm1				; x3 += x1
		movaps	[edx], xmm3				; *dest = x3
#else
		movups	xmm3, [ecx]				; x3 = dir0,dir1,dir2,X
		pshufd	xmm2, xmm2, 0			; x2 = scale, scale, scale, scale
		movups	xmm1, [eax]				; x1 = start1, start2, start3, X
		mulps	xmm3, xmm2				; x3 *= x2
		addps	xmm3, xmm1				; x3 += x1
		movups	[edx], xmm3				; *dest = x3
#endif
	}
}
float (__cdecl *pfVectorMA)(Vector& v) = _VectorMA;
#endif
#endif

#endif // COMPILER_MSVC64 
