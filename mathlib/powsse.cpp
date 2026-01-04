//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "mathlib/ssemath.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


fltx4 XM_CALLCONV Pow_FixedPoint_Exponent_SIMD( DirectX::FXMVECTOR x, int exponent )
{
	fltx4 rslt=Four_Ones;									// x^0=1.0
	int xp=abs(exponent);
	if (xp & 3)												// fraction present?
	{
		fltx4 sq_rt=SqrtEstSIMD(x);
		if (xp & 1)											// .25?
			rslt=SqrtEstSIMD(sq_rt);						// x^.25
		if (xp & 2)
			rslt=MulSIMD(rslt,sq_rt);
	}
	xp>>=2;													// strip fraction
	fltx4 curpower=x;										// curpower iterates through  x,x^2,x^4,x^8,x^16...

	while(true)
	{
		if (xp & 1)
			rslt=MulSIMD(rslt,curpower);
		xp>>=1;
		if (xp)
			curpower=MulSIMD(curpower,curpower);
		else
			break;
	}
	if (exponent<0)
		return ReciprocalEstSaturateSIMD(rslt);				// pow(x,-b)=1/pow(x,b)
	else
		return rslt;
}




/*
 * (c) Ian Stephenson
 *
 * ian@dctsystems.co.uk
 *
 * Fast pow() reference implementation
 */


static constexpr float shift23=(1<<23);
static constexpr float OOshift23=1.0/(1<<23);

float XM_CALLCONV FastLog2(float i)
{
	static_assert(std::numeric_limits<float>::is_iec559);

	constexpr float LogBodge=0.346607f;

	// dimhotepus: Fix UB on type punning.
	float x;
	static_assert(sizeof(x) == sizeof(i));
	memcpy(&x, &i, sizeof(i));

	x *= OOshift23;
	x -= 127;

	float y = x - floorf(x);
	y = (y - y * y) * LogBodge;

	return x + y;
}
float XM_CALLCONV FastPow2(float i)
{
	static_assert(std::numeric_limits<float>::is_iec559);

	constexpr float PowBodge=0.33971f;

	float y = i - floorf(i);
	y = (y-y*y) * PowBodge;

	float x = i + 127 - y;
	x *= shift23;
	*(int*)&x = (int)x;

	return x;
}
float XM_CALLCONV FastPow(float a, float b)
{
	static_assert(std::numeric_limits<float>::is_iec559);

	if (a <= OOshift23)
	{
		return 0.0f;
	}
	return FastPow2(b*FastLog2(a));
}
float XM_CALLCONV FastPow10( float i )
{
	return FastPow2( i * 3.321928f );
}

