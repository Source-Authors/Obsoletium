//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "map_utils.h"
#include "mathlib/vector.h"
#include "bspfile.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void SetupLightNormalFromProps( const QAngle &angles, float angle, float pitch, Vector &output )
{
	if (angle == ANGLE_UP)
	{
		output[0] = output[1] = 0;
		output[2] = 1;
	}
	else if (angle == ANGLE_DOWN)
	{
		output[0] = output[1] = 0;
		output[2] = -1;
	}
	else
	{
		// if we don't have a specific "angle" use the "angles" YAW
		if ( !angle )
		{
			angle = angles[YAW];
		}
		
		output[2] = 0;

		DirectX::XMScalarSinCos(&output[1], &output[0], DEG2RAD(angle));
	}
	
	if ( !pitch )
	{
		// if we don't have a specific "pitch" use the "angles" PITCH
		pitch = angles[PITCH];
	}
	
	float fSin, fCos;
	DirectX::XMScalarSinCos(&fSin, &fCos, DEG2RAD(pitch));

	output[0] *= fCos;
	output[1] *= fCos;
	output[2] = fSin;
}

