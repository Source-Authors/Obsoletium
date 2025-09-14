//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef BUMPVECTS_H
#define BUMPVECTS_H

#include "vector.h"

constexpr inline float OO_SQRT_2{0.70710676908493042f};
constexpr inline float OO_SQRT_3{0.57735025882720947f};
constexpr inline float OO_SQRT_6{0.40824821591377258f};
// sqrt( 2 / 3 )
constexpr inline float OO_SQRT_2_OVER_3{0.81649661064147949f};

constexpr inline int NUM_BUMP_VECTS{3};

const TableVector g_localBumpBasis[NUM_BUMP_VECTS] = 
{
	{	OO_SQRT_2_OVER_3, 0.0f, OO_SQRT_3 },
	{  -OO_SQRT_6, OO_SQRT_2, OO_SQRT_3 },
	{  -OO_SQRT_6, -OO_SQRT_2, OO_SQRT_3 }
};

void XM_CALLCONV GetBumpNormals( Vector sVect, Vector tVect, const Vector& flatNormal, 
					 const Vector& phongNormal, Vector (&bumpNormals)[NUM_BUMP_VECTS] );

#endif // BUMPVECTS_H
