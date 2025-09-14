//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SSE Math primitives.
//
//=====================================================================================//

#include <cfloat>	// Needed for FLT_EPSILON
#include <DirectXMath.h>

#include "tier0/platform.h"

#include "mathlib/vector.h"
#include "mathlib/math_pfns.h"
#include "sse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef COMPILER_MSVC64
// Implement for 64-bit Windows if needed.

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
_PS_EXTERN_CONST(am_pi, M_PI_F);
_PS_EXTERN_CONST(am_pi_o_2, M_PI_F / 2.0f);
_PS_EXTERN_CONST(am_2_o_pi, 2.0f / M_PI_F);
_PS_EXTERN_CONST(am_pi_o_4, M_PI_F / 4.0f);
_PS_EXTERN_CONST(am_4_o_pi, 4.0f / M_PI_F);
_PS_EXTERN_CONST_TYPE(am_sign_mask, uint32, 0x80000000); //-V112
_PS_EXTERN_CONST_TYPE(am_inv_sign_mask, uint32, ~0x80000000); //-V112
_PS_EXTERN_CONST_TYPE(am_min_norm_pos,uint32, 0x00800000);
_PS_EXTERN_CONST_TYPE(am_mant_mask, uint32, 0x7f800000);
_PS_EXTERN_CONST_TYPE(am_inv_mant_mask, int32, ~0x7f800000);

_EPI32_CONST(1, 1);
_EPI32_CONST(2, 2);

_PS_CONST(sincos_p0, 0.15707963267948963959e1f);
_PS_CONST(sincos_p1, -0.64596409750621907082e0f);
_PS_CONST(sincos_p2, 0.7969262624561800806e-1f);
_PS_CONST(sincos_p3, -0.468175413106023168e-2f);

#ifdef POSIX
#define _PS_CONST_TYPE(Name, Type, Val) static const alignas(16) Type _ps_##Name[4] = { Val, Val, Val, Val }

_PS_CONST_TYPE(sign_mask, int, (int)0x80000000);
_PS_CONST_TYPE(inv_sign_mask, int, ~0x80000000);


#define _PI32_CONST(Name, Val)  static const alignas(16) int _pi32_##Name[4] = { Val, Val, Val, Val }

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

#endif // COMPILER_MSVC64 
