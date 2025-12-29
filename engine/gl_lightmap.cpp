//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//


#include "render_pch.h"
#include "gl_lightmap.h"
#include "view.h"
#include "gl_cvars.h"
#include "zone.h"
#include "gl_water.h"
#include "r_local.h"
#include "gl_model_private.h"
#include "mathlib/bumpvects.h"
#include "gl_matsysiface.h"
#include <float.h>
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imesh.h"
#include "tier0/dbg.h"
#include "tier0/vprof.h"
#include "tier1/callqueue.h"
#include "lightcache.h"
#include "cl_main.h"
#include "materialsystem/imaterial.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// globals
//-----------------------------------------------------------------------------

// Only enable this if you are testing lightstyle performance.
//#define UPDATE_LIGHTSTYLES_EVERY_FRAME

ALIGN128 Vector4D blocklights[NUM_BUMP_VECTS+1][ MAX_LIGHTMAP_DIM_INCLUDING_BORDER * MAX_LIGHTMAP_DIM_INCLUDING_BORDER ];

ConVar r_avglightmap( "r_avglightmap", "0", FCVAR_CHEAT | FCVAR_MATERIAL_SYSTEM_THREAD );
ConVar r_maxdlights( "r_maxdlights", "32" );
extern ConVar r_unloadlightmaps;
extern bool g_bHunkAllocLightmaps;

static int r_dlightvisible;
static int r_dlightvisiblethisframe;
static int s_nVisibleDLightCount;
static int s_nMaxVisibleDLightCount;


//-----------------------------------------------------------------------------
// Visible, not visible DLights
//-----------------------------------------------------------------------------
void R_MarkDLightVisible( int dlight )
{
	if ( (r_dlightvisible & ( 1 << dlight )) == 0 )
	{
		++s_nVisibleDLightCount;
		r_dlightvisible |= 1 << dlight;
	}
}

void R_MarkDLightNotVisible( int dlight )
{
	if ( r_dlightvisible & ( 1 << dlight ))
	{
		--s_nVisibleDLightCount;
		r_dlightvisible &= ~( 1 << dlight );
	}
}


//-----------------------------------------------------------------------------
// Must call these at the start + end of rendering each view
//-----------------------------------------------------------------------------
void R_DLightStartView()
{
	r_dlightvisiblethisframe = 0;
	s_nMaxVisibleDLightCount = r_maxdlights.GetInt();
}

void R_DLightEndView()
{
	if ( !g_bActiveDlights )
		return;
	for( int lnum=0 ; lnum<MAX_DLIGHTS; lnum++ )
	{
		if ( r_dlightvisiblethisframe & ( 1 << lnum ))
			continue;

		R_MarkDLightNotVisible( lnum );
	}
}


//-----------------------------------------------------------------------------
// Can we use another dynamic light, or is it just too expensive?
//-----------------------------------------------------------------------------
bool R_CanUseVisibleDLight( int dlight )
{
	r_dlightvisiblethisframe |= (1 << dlight);

	if ( r_dlightvisible & ( 1 << dlight ) )
		return true;

	if ( s_nVisibleDLightCount >= s_nMaxVisibleDLightCount )
		return false;

	R_MarkDLightVisible( dlight );
	return true;
}


//-----------------------------------------------------------------------------
// Adds a single dynamic light
//-----------------------------------------------------------------------------
static bool AddSingleDynamicLight( dlight_t& dl, SurfaceHandle_t surfID, const Vector &lightOrigin, float perpDistSq, float lightRadiusSq )
{
	// transform the light into brush local space
	Vector local;
	// Spotlight early outs...
	if (dl.m_OuterAngle)
	{
		if (dl.m_OuterAngle < 180.0f)
		{
			// Can't light anything from the rear...
			if (DotProduct(dl.m_Direction, MSurf_Plane( surfID ).normal) >= 0.0f)
				return false;
		}
	}

	// Transform the light center point into (u,v) space of the lightmap
	mtexinfo_t* tex = MSurf_TexInfo( surfID );
	local[0] = DotProduct (lightOrigin, tex->lightmapVecsLuxelsPerWorldUnits[0].AsVector3D()) + 
			   tex->lightmapVecsLuxelsPerWorldUnits[0][3];
	local[1] = DotProduct (lightOrigin, tex->lightmapVecsLuxelsPerWorldUnits[1].AsVector3D()) + 
			   tex->lightmapVecsLuxelsPerWorldUnits[1][3];

	// Now put the center points into the space of the lightmap rectangle
	// defined by the lightmapMins + lightmapExtents
	local[0] -= MSurf_LightmapMins( surfID )[0];
	local[1] -= MSurf_LightmapMins( surfID )[1];
	
	// Figure out the quadratic attenuation factor...
	Vector intensity;
	float lightStyleValue = LightStyleValue( dl.style );
	intensity[0] = TexLightToLinear( dl.color.r, dl.color.exponent ) * lightStyleValue;
	intensity[1] = TexLightToLinear( dl.color.g, dl.color.exponent ) * lightStyleValue;
	intensity[2] = TexLightToLinear( dl.color.b, dl.color.exponent ) * lightStyleValue;

	float minlight = fpmax( g_flMinLightingValue, dl.minlight );
	float ooQuadraticAttn = lightRadiusSq * minlight;
	float ooRadiusSq = 1.0f / lightRadiusSq;

	// Compute a color at each luxel
	// We want to know the square distance from luxel center to light
	// so we can compute an 1/r^2 falloff in light color
	int smax = MSurf_LightmapExtents( surfID )[0] + 1;
	int tmax = MSurf_LightmapExtents( surfID )[1] + 1;
	for (int t=0; t<tmax; ++t)
	{
		float td = (local[1] - t) * tex->worldUnitsPerLuxel;
		
		for (int s=0; s<smax; ++s)
		{
			float sd = (local[0] - s) * tex->worldUnitsPerLuxel;

			float inPlaneDistSq = sd * sd + td * td;
			float totalDistSq = inPlaneDistSq + perpDistSq;
			if (totalDistSq < lightRadiusSq)
			{
				// at least all floating point only happens when a luxel is lit.
				float scale = (totalDistSq != 0.0f) ? ooQuadraticAttn / totalDistSq : 1.0f;

				// Apply a little extra attenuation
				scale *= (1.0f - totalDistSq * ooRadiusSq);

				if (scale > 2.0f)
					scale = 2.0f;

				int idx = t*smax + s;

				// Compute the base lighting just as is done in the non-bump case...
				blocklights[0][idx][0] += scale * intensity[0];
				blocklights[0][idx][1] += scale * intensity[1];
				blocklights[0][idx][2] += scale * intensity[2];
			}
		}
	}
	return true;
}												

//-----------------------------------------------------------------------------
// Adds a dynamic light to the bumped lighting
//-----------------------------------------------------------------------------
static void AddSingleDynamicLightToBumpLighting( dlight_t& dl, SurfaceHandle_t surfID, 
	const Vector &lightOrigin, float perpDistSq, float lightRadiusSq, Vector (&pBumpBasis)[NUM_BUMP_VECTS], const Vector& luxelBasePosition )
{
	Vector local;
	// FIXME: For now, only elights can be spotlights
	// the lightmap computation could get expensive for spotlights...
	Assert( dl.m_OuterAngle == 0.0f );

	// Transform the light center point into (u,v) space of the lightmap
	mtexinfo_t *pTexInfo = MSurf_TexInfo( surfID );
	local[0] = DotProduct (lightOrigin, pTexInfo->lightmapVecsLuxelsPerWorldUnits[0].AsVector3D()) + 
			   pTexInfo->lightmapVecsLuxelsPerWorldUnits[0][3];
	local[1] = DotProduct (lightOrigin, pTexInfo->lightmapVecsLuxelsPerWorldUnits[1].AsVector3D()) + 
			   pTexInfo->lightmapVecsLuxelsPerWorldUnits[1][3];

	// Now put the center points into the space of the lightmap rectangle
	// defined by the lightmapMins + lightmapExtents
	local[0] -= MSurf_LightmapMins( surfID )[0];
	local[1] -= MSurf_LightmapMins( surfID )[1];

	// Figure out the quadratic attenuation factor...
	Vector intensity;
	float lightStyleValue = LightStyleValue( dl.style );
	intensity[0] = TexLightToLinear( dl.color.r, dl.color.exponent ) * lightStyleValue;
	intensity[1] = TexLightToLinear( dl.color.g, dl.color.exponent ) * lightStyleValue;
	intensity[2] = TexLightToLinear( dl.color.b, dl.color.exponent ) * lightStyleValue;

	float minlight = fpmax( g_flMinLightingValue, dl.minlight );
	float ooQuadraticAttn = lightRadiusSq * minlight;
	float ooRadiusSq = 1.0f / lightRadiusSq;

	// The algorithm here is necessary to make dynamic lights live in the
	// same world as the non-bumped dynamic lights. Therefore, we compute
	// the intensity of the flat lightmap the exact same way here as when
	// we've got a non-bumped surface.

	// Then, I compute an actual light direction vector per luxel (FIXME: !!expensive!!)
	// and compute what light would have to come in along that direction
	// in order to produce the same illumination on the flat lightmap. That's
	// computed by dividing the flat lightmap color by n dot l.
	Vector lightDirection(0, 0, 0), texelWorldPosition;
#if 1
	bool useLightDirection = (dl.m_OuterAngle != 0.0f) &&
		(fabs(dl.m_Direction.LengthSqr() - 1.0f) < 1e-3);
	if (useLightDirection)
		VectorMultiply( dl.m_Direction, -1.0f, lightDirection );
#endif

	// Since there's a scale factor used when going from world to luxel,
	// we gotta undo that scale factor when going from luxel to world
	float fixupFactor = pTexInfo->worldUnitsPerLuxel * pTexInfo->worldUnitsPerLuxel;

	// Compute a color at each luxel
	// We want to know the square distance from luxel center to light
	// so we can compute an 1/r^2 falloff in light color
	int smax = MSurf_LightmapExtents( surfID )[0] + 1;
	int tmax = MSurf_LightmapExtents( surfID )[1] + 1;
	for (int t=0; t<tmax; ++t)
	{
		float td = (local[1] - t) * pTexInfo->worldUnitsPerLuxel;
		
		// Move along the v direction
		VectorMA( luxelBasePosition, t * fixupFactor, pTexInfo->lightmapVecsLuxelsPerWorldUnits[1].AsVector3D(), 
			texelWorldPosition );

		for (int s=0; s<smax; ++s)
		{
			float sd = (local[0] - s) * pTexInfo->worldUnitsPerLuxel;

			float inPlaneDistSq = sd * sd + td * td;
			float totalDistSq = inPlaneDistSq + perpDistSq;

			if (totalDistSq < lightRadiusSq)
			{
				// at least all floating point only happens when a luxel is lit.
				float scale = (totalDistSq != 0.0f) ? ooQuadraticAttn / totalDistSq : 1.0f;

				// Apply a little extra attenuation
				scale *= (1.0f - totalDistSq * ooRadiusSq);

				if (scale > 2.0f)
					scale = 2.0f;

				int idx = t*smax + s;

				// Compute the base lighting just as is done in the non-bump case...
				VectorMA( blocklights[0][idx].AsVector3D(), scale, intensity, blocklights[0][idx].AsVector3D() );

#if 1
				if (!useLightDirection)
				{
					VectorSubtract( lightOrigin, texelWorldPosition, lightDirection );
					VectorNormalize( lightDirection );
				}
				
				float lDotN = DotProduct( lightDirection, MSurf_Plane( surfID ).normal );
				if (lDotN < 1e-3)
					lDotN = 1e-3;
				scale /= lDotN;

				int i;
				for( i = 1; i < NUM_BUMP_VECTS + 1; i++ )
				{
					float dot = DotProduct( lightDirection, pBumpBasis[i-1] );
					if( dot <= 0.0f )
						continue;
					
					VectorMA( blocklights[i][idx].AsVector3D(), dot * scale, intensity, 
						blocklights[i][idx].AsVector3D() );
				}
#else
				VectorMA( blocklights[1][idx].AsVector3D(), scale, intensity, blocklights[1][idx].AsVector3D() );
				VectorMA( blocklights[2][idx].AsVector3D(), scale, intensity, blocklights[2][idx].AsVector3D() );
				VectorMA( blocklights[3][idx].AsVector3D(), scale, intensity, blocklights[3][idx].AsVector3D() );
#endif
			}
		}

		// Move along u
		VectorMA( texelWorldPosition, fixupFactor, 
			pTexInfo->lightmapVecsLuxelsPerWorldUnits[0].AsVector3D(), texelWorldPosition );

	}
}

//-----------------------------------------------------------------------------
// Compute the bumpmap basis for this surface
//-----------------------------------------------------------------------------
static void R_ComputeSurfaceBasis( SurfaceHandle_t surfID, Vector (&pBumpNormals)[NUM_BUMP_VECTS], Vector &luxelBasePosition )
{
	// Get the bump basis vects in the space of the surface.
	Vector sVect, tVect;
	VectorCopy( MSurf_TexInfo( surfID )->lightmapVecsLuxelsPerWorldUnits[0].AsVector3D(), sVect );
	VectorNormalize( sVect );
	VectorCopy( MSurf_TexInfo( surfID )->lightmapVecsLuxelsPerWorldUnits[1].AsVector3D(), tVect );
	VectorNormalize( tVect );
	GetBumpNormals( sVect, tVect, MSurf_Plane( surfID ).normal, MSurf_Plane( surfID ).normal, pBumpNormals );

	// Compute the location of the first luxel in worldspace

	// Since there's a scale factor used when going from world to luxel,
	// we gotta undo that scale factor when going from luxel to world
	float fixupFactor = 
		MSurf_TexInfo( surfID )->worldUnitsPerLuxel * 
		MSurf_TexInfo( surfID )->worldUnitsPerLuxel;

	// The starting u of the surface is surf->lightmapMins[0];
	// since N * P + D = u, N * P = u - D, therefore we gotta move (u-D) along uvec
	VectorMultiply( MSurf_TexInfo( surfID )->lightmapVecsLuxelsPerWorldUnits[0].AsVector3D(),
		(MSurf_LightmapMins( surfID )[0] - MSurf_TexInfo( surfID )->lightmapVecsLuxelsPerWorldUnits[0][3]) * fixupFactor,
		luxelBasePosition );

	// Do the same thing for the v direction.
	VectorMA( luxelBasePosition, 
		(MSurf_LightmapMins( surfID )[1] - 
		MSurf_TexInfo( surfID )->lightmapVecsLuxelsPerWorldUnits[1][3]) * fixupFactor,
		MSurf_TexInfo( surfID )->lightmapVecsLuxelsPerWorldUnits[1].AsVector3D(),
		luxelBasePosition );

	// Move out in the direction of the plane normal...
	VectorMA( luxelBasePosition, MSurf_Plane( surfID ).dist, MSurf_Plane( surfID ).normal, luxelBasePosition ); 
}

//-----------------------------------------------------------------------------
// Purpose: Compute the mask of which dlights affect a surface
//			NOTE: Also has the side effect of updating the surface lighting dlight flags!
//-----------------------------------------------------------------------------
unsigned int R_ComputeDynamicLightMask( dlight_t *pLights, SurfaceHandle_t surfID, msurfacelighting_t *pLighting, const matrix3x4_t& entityToWorld )
{
	ASSERT_SURF_VALID( surfID );

	// Displacements do dynamic lights different
	if( SurfaceHasDispInfo( surfID ) )
	{
		return MSurf_DispInfo( surfID )->ComputeDynamicLightMask(pLights);
	}

	if ( !g_bActiveDlights )
		return 0;

	int lightMask = 0;
	for ( int lnum = 0, testBit = 1, mask = r_dlightactive; lnum < MAX_DLIGHTS; lnum++, mask >>= 1, testBit <<= 1 )
	{
		if ( mask & 1 )
		{
			// not lit by this light
			if ( !(pLighting->m_fDLightBits & testBit ) )
				continue;

			// This light doesn't affect the world
			if ( pLights[lnum].flags & (DLIGHT_NO_WORLD_ILLUMINATION|DLIGHT_DISPLACEMENT_MASK))
				continue;

			// This is used to ensure a maximum number of dlights in a frame
			if ( !R_CanUseVisibleDLight( lnum ) ) 
				continue;

			// Cull surface to light radius
			Vector lightOrigin;

			VectorITransform( pLights[lnum].origin, entityToWorld, lightOrigin );

			// NOTE: Dist can be negative because muzzle flashes can actually get behind walls
			// since the gun isn't checked for collision tests.
			float perpDistSq = DotProduct (lightOrigin, MSurf_Plane( surfID ).normal) - MSurf_Plane( surfID ).dist;
			if (perpDistSq < DLIGHT_BEHIND_PLANE_DIST)
			{
				// update the surfacelighting and remove this light's bit
				pLighting->m_fDLightBits &= ~testBit;
				continue;
			}

			perpDistSq *= perpDistSq;

			// If the perp distance > radius of light, blow it off
			float lightRadiusSq = pLights[lnum].GetRadiusSquared();
			if (lightRadiusSq <= perpDistSq)
			{
				// update the surfacelighting and remove this light's bit
				pLighting->m_fDLightBits &= ~testBit;
				continue;
			}

			lightMask |= testBit;
		}
	}

	return lightMask;
}


//-----------------------------------------------------------------------------
// Purpose: Modifies blocklights[][][] to include the state of the dlights 
//			affecting this surface.
//			NOTE: Can be threaded, should not reference or modify any global state 
//			other than blocklights.
//-----------------------------------------------------------------------------
void R_AddDynamicLights( dlight_t *pLights, SurfaceHandle_t surfID, const matrix3x4_t& entityToWorld, bool needsBumpmap, unsigned int lightMask )
{
	ASSERT_SURF_VALID( surfID );
	VPROF( "R_AddDynamicLights" );

	Vector bumpNormals[NUM_BUMP_VECTS];
	bool computedBumpBasis = false;
	Vector luxelBasePosition;

	// Displacements do dynamic lights different
	if( SurfaceHasDispInfo( surfID ) )
	{
		MSurf_DispInfo( surfID )->AddDynamicLights(pLights, lightMask);
		return;
	}

	// iterate all of the active dynamic lights.  Uses several iterators to keep 
	// the light mask (bit), light index, and active mask current
	for ( int lnum = 0, testBit = 1, mask = lightMask; lnum < MAX_DLIGHTS && mask != 0; lnum++, mask >>= 1, testBit <<= 1 )
	{
		// shift over the mask of active lights each iteration, if this one is active, apply it
		if ( mask & 1 )
		{
			// Cull surface to light radius
			Vector lightOrigin;

			VectorITransform( pLights[lnum].origin, entityToWorld, lightOrigin );

			// NOTE: Dist can be negative because muzzle flashes can actually get behind walls
			// since the gun isn't checked for collision tests.
			float perpDistSq = DotProduct (lightOrigin, MSurf_Plane( surfID ).normal) - MSurf_Plane( surfID ).dist;
			if (perpDistSq < DLIGHT_BEHIND_PLANE_DIST)
				continue;

			perpDistSq *= perpDistSq;

			// If the perp distance > radius of light, blow it off
			float lightRadiusSq = pLights[lnum].GetRadiusSquared();
			if (lightRadiusSq <= perpDistSq)
				continue;

			if (!needsBumpmap)
			{
				AddSingleDynamicLight( pLights[lnum], surfID, lightOrigin, perpDistSq, lightRadiusSq );
				continue;
			}

			// Here, I'm precomputing things needed by bumped lighting that
			// are the same for a surface...
			if (!computedBumpBasis)
			{
				R_ComputeSurfaceBasis( surfID, bumpNormals, luxelBasePosition );
				computedBumpBasis = true;
			}

			AddSingleDynamicLightToBumpLighting( pLights[lnum], surfID, lightOrigin, perpDistSq, lightRadiusSq, bumpNormals, luxelBasePosition );
		}
	}
}


// Fixed point (8.8) color/intensity ratios
#define I_RED		((int)(0.299*255))
#define I_GREEN		((int)(0.587*255))
#define I_BLUE		((int)(0.114*255))


//-----------------------------------------------------------------------------
// Sets all elements in a lightmap to a particular opaque greyscale value
//-----------------------------------------------------------------------------
static void InitLMSamples( Vector4D *pSamples, int nSamples, float value )
{
	for( int i=0; i < nSamples; i++ )
	{
		pSamples[i][0] = pSamples[i][1] = pSamples[i][2] = value;
		pSamples[i][3] = 1.0f;
	}
}


//-----------------------------------------------------------------------------
// Computes the lightmap size
//-----------------------------------------------------------------------------
static int ComputeLightmapSize( SurfaceHandle_t surfID )
{
	int smax = ( MSurf_LightmapExtents( surfID )[0] ) + 1;
	int tmax = ( MSurf_LightmapExtents( surfID )[1] ) + 1;
	int size = smax * tmax;

	int nMaxSize = MSurf_MaxLightmapSizeWithBorder( surfID );
	if (size > nMaxSize * nMaxSize)
	{
		ConMsg("Bad lightmap extents on material \"%s\"\n", 
			materialSortInfoArray[MSurf_MaterialSortID( surfID )].material->GetName());
		return 0;
	}
	
	return size;
}


//-----------------------------------------------------------------------------
// Compute the portion of the lightmap generated from lightstyles
//-----------------------------------------------------------------------------
static void AccumulateLightstyles( ColorRGBExp32* pLightmap, int lightmapSize, float scalar ) 
{
	Assert( pLightmap );
	for (int i=0; i<lightmapSize ; ++i)
	{
		blocklights[0][i][0] += scalar * TexLightToLinear( pLightmap[i].r, pLightmap[i].exponent );
		blocklights[0][i][1] += scalar * TexLightToLinear( pLightmap[i].g, pLightmap[i].exponent );
		blocklights[0][i][2] += scalar * TexLightToLinear( pLightmap[i].b, pLightmap[i].exponent );
	}
}

static void AccumulateLightstylesFlat( ColorRGBExp32* pLightmap, int lightmapSize, float scalar ) 
{
	Assert( pLightmap );
	for (int i=0; i<lightmapSize ; ++i)
	{
		blocklights[0][i][0] += scalar * TexLightToLinear( pLightmap->r, pLightmap->exponent );
		blocklights[0][i][1] += scalar * TexLightToLinear( pLightmap->g, pLightmap->exponent );
		blocklights[0][i][2] += scalar * TexLightToLinear( pLightmap->b, pLightmap->exponent );
	}
}


static void AccumulateBumpedLightstyles( ColorRGBExp32* pLightmap, int lightmapSize, float scalar ) 
{
	ColorRGBExp32 *pBumpedLightmaps[3];
	pBumpedLightmaps[0] = pLightmap + lightmapSize;
	pBumpedLightmaps[1] = pLightmap + 2 * lightmapSize;
	pBumpedLightmaps[2] = pLightmap + 3 * lightmapSize;

	// I chose to split up the loops this way because it was the best tradeoff
	// based on profiles between cache miss + loop overhead
	for (int i=0 ; i<lightmapSize ; ++i)
	{
		blocklights[0][i][0] += scalar * TexLightToLinear( pLightmap[i].r, pLightmap[i].exponent );
		blocklights[0][i][1] += scalar * TexLightToLinear( pLightmap[i].g, pLightmap[i].exponent );
		blocklights[0][i][2] += scalar * TexLightToLinear( pLightmap[i].b, pLightmap[i].exponent );
		Assert( blocklights[0][i][0] >= 0.0f );
		Assert( blocklights[0][i][1] >= 0.0f );
		Assert( blocklights[0][i][2] >= 0.0f );

		blocklights[1][i][0] += scalar * TexLightToLinear( pBumpedLightmaps[0][i].r, pBumpedLightmaps[0][i].exponent );
		blocklights[1][i][1] += scalar * TexLightToLinear( pBumpedLightmaps[0][i].g, pBumpedLightmaps[0][i].exponent );
		blocklights[1][i][2] += scalar * TexLightToLinear( pBumpedLightmaps[0][i].b, pBumpedLightmaps[0][i].exponent );
		Assert( blocklights[1][i][0] >= 0.0f );
		Assert( blocklights[1][i][1] >= 0.0f );
		Assert( blocklights[1][i][2] >= 0.0f );
	}
	for ( int i=0 ; i<lightmapSize ; ++i)
	{
		blocklights[2][i][0] += scalar * TexLightToLinear( pBumpedLightmaps[1][i].r, pBumpedLightmaps[1][i].exponent );
		blocklights[2][i][1] += scalar * TexLightToLinear( pBumpedLightmaps[1][i].g, pBumpedLightmaps[1][i].exponent );
		blocklights[2][i][2] += scalar * TexLightToLinear( pBumpedLightmaps[1][i].b, pBumpedLightmaps[1][i].exponent );
		Assert( blocklights[2][i][0] >= 0.0f );
		Assert( blocklights[2][i][1] >= 0.0f );
		Assert( blocklights[2][i][2] >= 0.0f );

		blocklights[3][i][0] += scalar * TexLightToLinear( pBumpedLightmaps[2][i].r, pBumpedLightmaps[2][i].exponent );
		blocklights[3][i][1] += scalar * TexLightToLinear( pBumpedLightmaps[2][i].g, pBumpedLightmaps[2][i].exponent );
		blocklights[3][i][2] += scalar * TexLightToLinear( pBumpedLightmaps[2][i].b, pBumpedLightmaps[2][i].exponent );
		Assert( blocklights[3][i][0] >= 0.0f );
		Assert( blocklights[3][i][1] >= 0.0f );
		Assert( blocklights[3][i][2] >= 0.0f );
	}
}

//-----------------------------------------------------------------------------
// Compute the portion of the lightmap generated from lightstyles
//-----------------------------------------------------------------------------
static void ComputeLightmapFromLightstyle( msurfacelighting_t *pLighting, bool computeLightmap, 
				bool computeBumpmap, int lightmapSize, bool hasBumpmapLightmapData )
{
	VPROF( "ComputeLightmapFromLightstyle" );

	ColorRGBExp32 *pLightmap = pLighting->m_pSamples;

	// Compute iteration range
	int minmap, maxmap;
#ifdef USE_CONVARS
	if( r_lightmap.GetInt() != -1 )
	{
		minmap = r_lightmap.GetInt();
		maxmap = minmap + 1;
	}
	else
#endif
	{
		minmap = 0; maxmap = MAXLIGHTMAPS;
	}

	for (int maps = minmap; maps < maxmap && pLighting->m_nStyles[maps] != 255; ++maps)
	{
		if( r_lightstyle.GetInt() != -1 && pLighting->m_nStyles[maps] != r_lightstyle.GetInt())
		{
			continue;
		}

		float fscalar = LightStyleValue( pLighting->m_nStyles[maps] );

		// hack - don't know why we are getting negative values here.
//		if (scalar > 0.0f && maps > 0 )
		if (fscalar > 0.0f)
		{
#ifdef _X360
			fltx4 scalar = ReplicateX4(fscalar); // we use SIMD versions of these functions on 360
#else
			const float &scalar = fscalar;
#endif

			if( computeBumpmap )
			{
				AccumulateBumpedLightstyles( pLightmap, lightmapSize, scalar );
			}
			else if( computeLightmap )
			{
				if (r_avglightmap.GetInt())
				{
					pLightmap = pLighting->AvgLightColor(maps);
					AccumulateLightstylesFlat( pLightmap, lightmapSize, scalar );
				}
				else
				{
					AccumulateLightstyles( pLightmap, lightmapSize, scalar );
				}
			}
		}

		// skip to next lightmap. If we store lightmap data, we need to jump forward 4
		pLightmap += hasBumpmapLightmapData ? lightmapSize * ( NUM_BUMP_VECTS + 1 ) : lightmapSize;
	}
}

// instrumentation to measure locks
/*
static CUtlVector<int> g_LightmapLocks;
static int g_Lastdlightframe = -1;
static int g_lastlock = -1;
static int g_unsorted = 0;
void MarkPage( int pageID )
{
	if ( g_Lastdlightframe != r_framecount )
	{
		int total = 0;
		int locks = 0;
		for ( int i = 0; i < g_LightmapLocks.Count(); i++ )
		{
			int count = g_LightmapLocks[i];
			if ( count )
			{
				total++;
				locks += count;
			}
			g_LightmapLocks[i] = 0;
		}
		g_Lastdlightframe = r_framecount;
		g_lastlock = -1;
		if ( locks )
		Msg("Total pages %d, locks %d, unsorted locks %d\n", total, locks, g_unsorted );
		g_unsorted = 0;
	}
	if ( pageID != g_lastlock )
	{
		g_lastlock = pageID;
		g_unsorted++;
	}
	g_LightmapLocks.EnsureCount(pageID+1);
	g_LightmapLocks[pageID]++;
}
*/
//-----------------------------------------------------------------------------
// Update the lightmaps...
//-----------------------------------------------------------------------------
static void UpdateLightmapTextures( SurfaceHandle_t surfID, bool needsBumpmap )
{
	ASSERT_SURF_VALID( surfID );

	if( materialSortInfoArray )
	{
		int lightmapSize[2];
		int offsetIntoLightmapPage[2];
		lightmapSize[0] = ( MSurf_LightmapExtents( surfID )[0] ) + 1;
		lightmapSize[1] = ( MSurf_LightmapExtents( surfID )[1] ) + 1;
		offsetIntoLightmapPage[0] = MSurf_OffsetIntoLightmapPage( surfID )[0];
		offsetIntoLightmapPage[1] = MSurf_OffsetIntoLightmapPage( surfID )[1];
		Assert( MSurf_MaterialSortID( surfID ) >= 0 && 
			MSurf_MaterialSortID( surfID ) < g_WorldStaticMeshes.Count() );
		// FIXME: Should differentiate between bumped and unbumped since the perf characteristics
		// are completely different?
//		MarkPage( materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID );

		if( needsBumpmap )
		{
			materials->UpdateLightmap( materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID,
				lightmapSize, offsetIntoLightmapPage, 
				&blocklights[0][0][0], &blocklights[1][0][0], &blocklights[2][0][0], &blocklights[3][0][0] );
		}
		else
		{
			materials->UpdateLightmap( materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID,
				lightmapSize, offsetIntoLightmapPage, 
				&blocklights[0][0][0], NULL, NULL, NULL );
		}
	}
}


unsigned int R_UpdateDlightState( dlight_t *pLights, SurfaceHandle_t surfID, const matrix3x4_t& entityToWorld, bool bOnlyUseLightStyles, bool bLightmap )
{
	unsigned int dlightMask = 0;
	// Mark the surface with the particular cached light values...
	msurfacelighting_t *pLighting = SurfaceLighting( surfID );

	// Retire dlights that are no longer active
	pLighting->m_fDLightBits &= r_dlightactive;
	pLighting->m_nLastComputedFrame = r_framecount;

	// Here, it's got the data it needs. So use it!
	if ( !bOnlyUseLightStyles )
	{
		// add all the dynamic lights
		if( bLightmap && ( pLighting->m_nDLightFrame == r_framecount ) )
		{
			dlightMask = R_ComputeDynamicLightMask( pLights, surfID, pLighting, entityToWorld );
		}

		if ( !dlightMask || !pLighting->m_fDLightBits )
		{
			pLighting->m_fDLightBits = 0;
			MSurf_Flags(surfID) &= ~SURFDRAW_HASDLIGHT;
		}
	}
	return dlightMask;
}

//-----------------------------------------------------------------------------
// Purpose: Build the blocklights array for a given surface and copy to dest
//			Combine and scale multiple lightmaps into the 8.8 format in blocklights
// Input  : *psurf - surface to rebuild
//			*dest - texture pointer to receive copy in lightmap texture format
//			stride - stride of *dest memory
//-----------------------------------------------------------------------------
void R_BuildLightMapGuts( dlight_t *pLights, SurfaceHandle_t surfID, const matrix3x4_t& entityToWorld, unsigned int dlightMask, bool needsBumpmap, bool needsLightmap )
{
	VPROF_("R_BuildLightMapGuts", 1, VPROF_BUDGETGROUP_DLIGHT_RENDERING, false, 0);
	int bumpID;

	// Lightmap data can be dumped to save memory - this precludes any dynamic lighting on the world
	Assert( !host_state.worldbrush->unloadedlightmaps );

	// Mark the surface with the particular cached light values...
	msurfacelighting_t *pLighting = SurfaceLighting( surfID );

	int size = ComputeLightmapSize( surfID );
	if (size == 0)
		return;

	bool hasBumpmap = SurfHasBumpedLightmaps( surfID );
	bool hasLightmap = SurfHasLightmap( surfID );

	// clear to no light
	if( needsLightmap )
	{
		// set to full bright if no light data
		InitLMSamples( blocklights[0], size, hasLightmap ? 0.0f : 1.0f );
	}

	if( needsBumpmap )
	{
		// set to full bright if no light data
		for( bumpID = 1; bumpID < NUM_BUMP_VECTS + 1; bumpID++ )
		{
			InitLMSamples( blocklights[bumpID], size, hasBumpmap ? 0.0f : 1.0f );
		}
	}

	// add all the lightmaps
	// Here, it's got the data it needs. So use it!
	if( ( hasLightmap && needsLightmap ) || ( hasBumpmap && needsBumpmap ) )
	{
		ComputeLightmapFromLightstyle( pLighting, ( hasLightmap && needsLightmap ),
			( hasBumpmap && needsBumpmap ), size, hasBumpmap );
	}
	else if( !hasBumpmap && needsBumpmap && hasLightmap )
	{
		// make something up for the bumped lights if you need them but don't have the data
		// if you have a lightmap, use that, otherwise fullbright
		ComputeLightmapFromLightstyle( pLighting, true, false, size, hasBumpmap );

		for( bumpID = 0; bumpID < ( hasBumpmap ? ( NUM_BUMP_VECTS + 1 ) : 1 ); bumpID++ )
		{
			for (int i=0 ; i<size ; i++)
			{
				blocklights[bumpID][i].AsVector3D() = blocklights[0][i].AsVector3D();
			}
		}
	}
	else if( needsBumpmap && !hasLightmap )
	{
		// set to full bright if no light data
		InitLMSamples( blocklights[1], size, 0.0f );
		InitLMSamples( blocklights[2], size, 0.0f );
		InitLMSamples( blocklights[3], size, 0.0f );
	}
	else if( !needsBumpmap && !needsLightmap )
	{
	}
	else if( needsLightmap && !hasLightmap )
	{
	}
	else
	{
		Assert( 0 );
	}

	// add all the dynamic lights
	if ( dlightMask && (needsLightmap || needsBumpmap) )
	{
		R_AddDynamicLights( pLights, surfID, entityToWorld, needsBumpmap, dlightMask );
	}

	// Update the texture state
	UpdateLightmapTextures( surfID, needsBumpmap );
}

void R_BuildLightMap( dlight_t *pLights, ICallQueue *pCallQueue, SurfaceHandle_t surfID, const matrix3x4_t &entityToWorld, bool bOnlyUseLightStyles )
{
	bool needsBumpmap = SurfNeedsBumpedLightmaps( surfID );
	bool needsLightmap = SurfNeedsLightmap( surfID );

	if( !needsBumpmap && !needsLightmap )
		return;
	
	if( materialSortInfoArray )
	{
		Assert( MSurf_MaterialSortID( surfID ) >= 0 && 
			    MSurf_MaterialSortID( surfID ) < g_WorldStaticMeshes.Count() );
		if (( materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID == MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE )	||
		   ( materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID == MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP ) )
		{
			return;
		}
	}

	bool bDlightsInLightmap = needsLightmap || needsBumpmap;
	unsigned int dlightMask = R_UpdateDlightState( pLights, surfID, entityToWorld, bOnlyUseLightStyles, bDlightsInLightmap );

	// update the state, but don't render any dlights if only lightstyles requested
	if ( bOnlyUseLightStyles )
		dlightMask = 0;

	if ( !pCallQueue )
	{
		R_BuildLightMapGuts( pLights, surfID, entityToWorld, dlightMask, needsBumpmap, needsLightmap );
	}
	else
	{
		pCallQueue->QueueCall( R_BuildLightMapGuts, pLights, surfID, RefToVal( entityToWorld ), dlightMask, needsBumpmap, needsLightmap );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Save off the average light values, and dump the rest of the lightmap data.
// Can be used to save memory, at the expense of dynamic lights and lightstyles.
//-----------------------------------------------------------------------------
void CacheAndUnloadLightmapData()
{
	Assert( !g_bHunkAllocLightmaps );
	if ( g_bHunkAllocLightmaps )
	{
		return;
	}

	worldbrushdata_t *pBrushData = host_state.worldbrush;
	msurfacelighting_t *pLighting = pBrushData->surfacelighting;
	int numSurfaces = pBrushData->numsurfaces;

	// This will allocate more data than necessary, but only 1-2K max
	byte *pDestBase = (byte*)malloc( numSurfaces * MAXLIGHTMAPS * sizeof( ColorRGBExp32 ) );
	byte *pDest = pDestBase;

	for ( int i = 0; i < numSurfaces; ++i, ++pLighting )
	{
		int nStyleCt = 0;
		for ( auto s : pLighting->m_nStyles )
		{
			if ( s != 255 )
				++nStyleCt;
		}

		const int nHdrBytes = nStyleCt * sizeof( ColorRGBExp32 );
		byte *pHdr = (byte*)pLighting->m_pSamples - nHdrBytes;

		// Copy just the 0-4 average color entries
		Q_memcpy( pDest, pHdr, nHdrBytes );

		// m_pSamples needs to point AFTER the average color data
		pDest += nHdrBytes;
		pLighting->m_pSamples = (ColorRGBExp32*)pDest;
	}

	// Update the lightdata pointer
	free( host_state.worldbrush->lightdata );
	host_state.worldbrush->lightdata = (ColorRGBExp32*)pDestBase;
	host_state.worldbrush->unloadedlightmaps = true;
}

//sorts the surfaces in place
static void SortSurfacesByLightmapID( SurfaceHandle_t *pToSort, int iSurfaceCount )
{
	SurfaceHandle_t *pSortTemp = stackallocT( SurfaceHandle_t, iSurfaceCount );
	
	//radix sort
	for( int radix = 0; radix < 4; ++radix )
	{
		//swap the inputs for the next pass
		std::swap(pToSort, pSortTemp);

		int iCounts[256] = { 0 };
		int iBitOffset = radix * 8;
		for( int i = 0; i < iSurfaceCount; ++i )
		{
			uint8 val = (materialSortInfoArray[MSurf_MaterialSortID( pSortTemp[i] )].lightmapPageID >> iBitOffset) & 0xFF;
			++iCounts[val];
		}

		int iOffsetTable[256];
		iOffsetTable[0] = 0;
		for( int i = 0; i < 255; ++i )
		{
			iOffsetTable[i + 1] = iOffsetTable[i] + iCounts[i];
		}

		for( int i = 0; i < iSurfaceCount; ++i )
		{
			uint8 val = (materialSortInfoArray[MSurf_MaterialSortID( pSortTemp[i] )].lightmapPageID >> iBitOffset) & 0xFF;
			int iWriteIndex = iOffsetTable[val];
			pToSort[iWriteIndex] = pSortTemp[i];
			++iOffsetTable[val];
		}
	}
}

void R_RedownloadAllLightmaps()
{
#ifdef _DEBUG
	static bool initializedBlockLights = false;
	if (!initializedBlockLights)
	{
		memset( &blocklights[0][0][0], 0, MAX_LIGHTMAP_DIM_INCLUDING_BORDER * MAX_LIGHTMAP_DIM_INCLUDING_BORDER * (NUM_BUMP_VECTS + 1) * sizeof( Vector ) );
		initializedBlockLights = true;
	}
#endif

	double st = Sys_FloatTime();

	bool bOnlyUseLightStyles = false;

	if( r_dynamic.GetInt() == 0 )
	{
		bOnlyUseLightStyles = true;
	}

	{
		// Can't build lightmaps if the source data has been dumped
		CMatRenderContextPtr pRenderContext( materials );
		ICallQueue *pCallQueue = pRenderContext->GetCallQueue();
		if ( !host_state.worldbrush->unloadedlightmaps )
		{
			int iSurfaceCount = host_state.worldbrush->numsurfaces;
		
			SurfaceHandle_t *pSortedSurfaces = stackallocT( SurfaceHandle_t, iSurfaceCount );
			for( int surfaceIndex = 0; surfaceIndex < iSurfaceCount; surfaceIndex++ )
			{
				SurfaceHandle_t surfID = SurfaceHandleFromIndex( surfaceIndex );
				pSortedSurfaces[surfaceIndex] = surfID;
			}

			SortSurfacesByLightmapID( pSortedSurfaces, iSurfaceCount ); //sorts in place, so now the array really is sorted

			if( pCallQueue )
				pCallQueue->QueueCall( materials, &IMaterialSystem::BeginUpdateLightmaps );
			else
				materials->BeginUpdateLightmaps();
		
			matrix3x4_t xform;
			SetIdentityMatrix(xform);
			for( int surfaceIndex = 0; surfaceIndex < iSurfaceCount; surfaceIndex++ )
			{
				SurfaceHandle_t surfID = pSortedSurfaces[surfaceIndex];

				ASSERT_SURF_VALID( surfID );
				R_BuildLightMap( &cl_dlights[0], pCallQueue, surfID, xform, bOnlyUseLightStyles );
			}

			if( pCallQueue )
				pCallQueue->QueueCall( materials, &IMaterialSystem::EndUpdateLightmaps );
			else
				materials->EndUpdateLightmaps();		

			if ( !g_bHunkAllocLightmaps && r_unloadlightmaps.GetInt() == 1 )
			{
				// Delete the lightmap data from memory
				if ( !pCallQueue )
				{
					CacheAndUnloadLightmapData();
				}
				else
				{
					pCallQueue->QueueCall( CacheAndUnloadLightmapData );
				}
			}
		}
	}

	double elapsed = ( Sys_FloatTime() - st ) * 1000.0;
	DevMsg( "R_RedownloadAllLightmaps took %.3f msec!\n", elapsed );

	g_RebuildLightmaps = false;
}

//-----------------------------------------------------------------------------
// Purpose: flag the lightmaps as needing to be rebuilt (gamma change)
//-----------------------------------------------------------------------------
bool g_RebuildLightmaps = false;

void GL_RebuildLightmaps( void )
{
	g_RebuildLightmaps = true;
}


//-----------------------------------------------------------------------------
// Purpose: Update the in-RAM texture for the given surface's lightmap
// Input  : *fa - surface pointer
//-----------------------------------------------------------------------------

#ifdef UPDATE_LIGHTSTYLES_EVERY_FRAME
ConVar mat_updatelightstyleseveryframe( "mat_updatelightstyleseveryframe", "0" );
#endif
void FASTCALL R_RenderDynamicLightmaps ( dlight_t *pLights, ICallQueue *pCallQueue, SurfaceHandle_t surfID, const matrix3x4_t &xform )
{
	VPROF_BUDGET( "R_RenderDynamicLightmaps", VPROF_BUDGETGROUP_DLIGHT_RENDERING );
	ASSERT_SURF_VALID( surfID );

	int fSurfFlags = MSurf_Flags( surfID );

	if( fSurfFlags & SURFDRAW_NOLIGHT )
		return;

	// check for lightmap modification
	bool bChanged = false;
	msurfacelighting_t *pLighting = SurfaceLighting( surfID );
	if( fSurfFlags & SURFDRAW_HASLIGHTSYTLES )
	{
#ifdef UPDATE_LIGHTSTYLES_EVERY_FRAME
		if( mat_updatelightstyleseveryframe.GetBool() && ( pLighting->m_nStyles[0] != 0 || pLighting->m_nStyles[1] != 255 ) )
		{
			bChanged = true;
		}
#endif
		for( int maps = 0; maps < MAXLIGHTMAPS && pLighting->m_nStyles[maps] != 255; maps++ )
		{
			if( d_lightstyleframe[pLighting->m_nStyles[maps]] > pLighting->m_nLastComputedFrame )
			{
				bChanged = true;
				break;
			}
		}
	}

	// was it dynamic this frame (pLighting->m_nDLightFrame == r_framecount) 
	// or dynamic previously (pLighting->m_fDLightBits)
	bool bDLightChanged = ( pLighting->m_nDLightFrame == r_framecount ) || pLighting->m_fDLightBits;
	bool bOnlyUseLightStyles = false;

	if( r_dynamic.GetInt() == 0 )
	{
		bOnlyUseLightStyles = true;
		bDLightChanged = false;
	}

	if ( bChanged || bDLightChanged )
	{
		R_BuildLightMap( pLights, pCallQueue, surfID, xform, bOnlyUseLightStyles );
	}
}
