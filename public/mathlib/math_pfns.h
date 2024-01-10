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
	
float SSE_Sqrt(float x)
{
	float root = 0.0f;
	_mm_store_ss( &root, _mm_sqrt_ss( _mm_load_ss( &x ) ) );
	return root;
}

}  // namespace details

// These globals are initialized by mathlib and redirected based on available fpu features
extern float (*pfRSqrt)(float x);
extern float (*pfRSqrtFast)(float x);


// The following are not declared as macros because they are often used in limiting situations,
// and sometimes the compiler simply refuses to inline them for some reason
#define FastSqrt(x)			details::SSE_Sqrt(x)
#define	FastRSqrt(x)		(*pfRSqrt)(x)
#define FastRSqrtFast(x)    (*pfRSqrtFast)(x)
#define FastSinCos(x, s, c) DirectX::XMScalarSinCos(s, c, x)
#define FastCos(x)			DirectX::XMScalarCos(x)

#endif // _MATH_PFNS_H_
