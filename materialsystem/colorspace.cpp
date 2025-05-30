//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "colorspace.h"

#include <cmath>
#include "mathlib/bumpvects.h"
#include "materialsystem_global.h"
#include "IHardwareConfigInternal.h"
#include "materialsystem/materialsystem_config.h"

// NOTE: This has to be the last file included
#include "tier0/memdbgon.h"

static float			textureToLinear[256];	// texture (0..255) to linear (0..1)
float					g_LinearToVertex[4096];	// linear (0..4) to screen corrected vertex space (0..1?)
static int				linearToLightmap[4096];	// linear (0..4) to screen corrected texture value (0..255)

void ColorSpace::SetGamma( float screenGamma, float texGamma, 
						   float overbright, bool allowCheats, bool linearFrameBuffer )
{
	int		i;
	float	g1, g3;
	float	g;
	float	brightness = 0.0f; // This used to be configurable. . hardcode to 0.0

	if( linearFrameBuffer )
	{
		screenGamma = 1.0f;
	}

	g = screenGamma;
	
	// clamp values to prevent cheating in multiplayer
	if( !allowCheats )
	{
		if (brightness > 2.0f)
			brightness = 2.0f;

		if (g < 1.8f)
			g = 1.8f;
	}

	if (g > 3.0f) 
		g = 3.0f;

	g = 1.0f / g;
	g1 = texGamma * g; 

	// pow( textureColor, g1 ) converts from on-disk texture space to framebuffer space
	
	if (brightness <= 0.0f) 
	{
		g3 = 0.125;
	}
	else if (brightness > 1.0f) 
	{
		g3 = 0.05f;
	}
	else 
	{
		g3 = 0.125f - (brightness * brightness) * 0.075f;
	}

	for (i=0 ; i<256 ; i++)
	{
		// convert from nonlinear texture space (0..255) to linear space (0..1)
		textureToLinear[i] =  powf( i / 255.0f, texGamma );
	}

	float f, overbrightFactor;
	
	// Can't do overbright without texcombine
	// UNDONE: Add GAMMA ramp to rectify this

	if ( !HardwareConfig() )
	{
		overbright = 1.0f;
	}
	if ( overbright == 2.0F )
	{
		overbrightFactor = 0.5F;
	}
	else if ( overbright == 4.0F )
	{
		overbrightFactor = 0.25F;
	}
	else
	{
		overbrightFactor = 1.0F;
	}
	
	for (i=0 ; i<4096 ; i++)
	{
		// convert from linear 0..4 (x1024) to screen corrected vertex space (0..1?)
		f = powf ( i/1024.0f, 1.0f / screenGamma );
		
		g_LinearToVertex[i] = f * overbrightFactor;
		if (g_LinearToVertex[i] > 1)
			g_LinearToVertex[i] = 1;
		
		linearToLightmap[i] = ( int )( f * 255 * overbrightFactor );
		if (linearToLightmap[i] > 255)
			linearToLightmap[i] = 255;
	}
}

// convert texture to linear 0..1 value
float ColorSpace::TextureToLinear( int c )
{
	if (c < 0)
		return 0;
	if (c > 255)
		return 1.0f;

	return textureToLinear[c];
}

float ColorSpace::TexLightToLinear( int c, int exponent )
{
//	return texLightToLinear[ c ];
	// optimize me
	return ( float )c * powf( 2.0f, exponent ) * ( 1.0f / 255.0f );
}

uint16 ColorSpace::LinearFloatToCorrectedShort( float in )
{
	return max( min( in * 4096.0F, 65535.0F ), 0.0f );
}
