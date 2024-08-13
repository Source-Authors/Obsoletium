//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#include "pch_materialsystem.h"

#define MATSYS_INTERNAL

#include "cmatlightmaps.h"

#include "colorspace.h"
#include "IHardwareConfigInternal.h"

#include "cmaterialsystem.h"

// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"
#include "bitmap/float_bm.h"

static ConVar mat_lightmap_pfms( "mat_lightmap_pfms", "0", FCVAR_MATERIAL_SYSTEM_THREAD, "Outputs .pfm files containing lightmap data for each lightmap page when a level exits." ); // Write PFM files for each lightmap page in the game directory when exiting a level 

#define USE_32BIT_LIGHTMAPS_ON_360 //uncomment to use 32bit lightmaps, be sure to keep this in sync with the same #define in stdshaders/lightmappedgeneric_ps2_3_x.h

//-----------------------------------------------------------------------------

inline IMaterialInternal* CMatLightmaps::GetCurrentMaterialInternal() const
{
	return GetMaterialSystem()->GetRenderContextInternal()->GetCurrentMaterialInternal();
}

inline void CMatLightmaps::SetCurrentMaterialInternal(IMaterialInternal* pCurrentMaterial)
{
	return GetMaterialSystem()->GetRenderContextInternal()->SetCurrentMaterialInternal( pCurrentMaterial );
}

inline IMaterialInternal *CMatLightmaps::GetMaterialInternal( MaterialHandle_t idx ) const
{
	return GetMaterialSystem()->GetMaterialInternal( idx );
}

inline const IMatRenderContextInternal *CMatLightmaps::GetRenderContextInternal() const
{
	return GetMaterialSystem()->GetRenderContextInternal();
}

inline IMatRenderContextInternal *CMatLightmaps::GetRenderContextInternal()
{
	return GetMaterialSystem()->GetRenderContextInternal();
}

inline const CMaterialDict *CMatLightmaps::GetMaterialDict() const
{
	return GetMaterialSystem()->GetMaterialDict();
}

inline CMaterialDict *CMatLightmaps::GetMaterialDict()
{
	return GetMaterialSystem()->GetMaterialDict();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CMatLightmaps::CMatLightmaps() : m_LightmapPixelWriter()
{
	m_numSortIDs = 0;
	m_currentWhiteLightmapMaterial = NULL;
	m_pLightmapPages = NULL;
	m_NumLightmapPages = 0;
	m_nUpdatingLightmapsStackDepth = 0;
	m_firstDynamicLightmap = 0;
	m_nLockedLightmap = -1;
	m_dynamic.Init();
	m_pLightmapDataPtrArray = NULL;
	m_eLightmapsState = STATE_DEFAULT;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMatLightmaps::Shutdown( )
{
	// Clean up all lightmaps
	CleanupLightmaps();
}

//-----------------------------------------------------------------------------
// Assign enumeration IDs to all materials
//-----------------------------------------------------------------------------
void CMatLightmaps::EnumerateMaterials( void )
{
	// iterate in sorted order
	int id = 0;
	for (MaterialHandle_t i = GetMaterialDict()->FirstMaterial(); i != GetMaterialDict()->InvalidMaterial(); i = GetMaterialDict()->NextMaterial(i) )
	{
		GetMaterialInternal(i)->SetEnumerationID( id );
		++id;
	}
}


//-----------------------------------------------------------------------------
// Gets the maximum lightmap page size...
//-----------------------------------------------------------------------------
int CMatLightmaps::GetMaxLightmapPageWidth() const
{
	// FIXME: It's unclear which we want here.
	// It doesn't drastically increase primitives per DrawIndexedPrimitive
	// call at the moment to increase it, so let's not for now.
	
	// If we're using dynamic textures though, we want bigger that's for sure.
	// The tradeoff here is how much memory we waste if we don't fill the lightmap

	// We need to go to 512x256 textures because that's the only way bumped
	// lighting on displacements can work given the 128x128 allowance..
	int nWidth = 512;
	if ( nWidth > HardwareConfig()->MaxTextureWidth() )
		nWidth = HardwareConfig()->MaxTextureWidth();

	return nWidth;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CMatLightmaps::GetMaxLightmapPageHeight() const
{
	int nHeight = 256;

	if ( nHeight > HardwareConfig()->MaxTextureHeight() )
		nHeight = HardwareConfig()->MaxTextureHeight();

	return nHeight;
}


//-----------------------------------------------------------------------------
// Returns the lightmap page size
//-----------------------------------------------------------------------------
void CMatLightmaps::GetLightmapPageSize( int lightmapPageID, int *pWidth, int *pHeight ) const
{
	switch( lightmapPageID )
	{
	default:
 		Assert( lightmapPageID >= 0 && lightmapPageID < GetNumLightmapPages() );
		*pWidth = m_pLightmapPages[lightmapPageID].m_Width;
		*pHeight = m_pLightmapPages[lightmapPageID].m_Height;
		break;

	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED:
		*pWidth = *pHeight = 1;
		AssertOnce( !"Can't use CMatLightmaps to get properties of MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED" );
		break;
	
	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE:
	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP:
		*pWidth = *pHeight = 1;
		break;
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CMatLightmaps::GetLightmapWidth( int lightmapPageID ) const
{
	switch( lightmapPageID )
	{
	default:
 		Assert( lightmapPageID >= 0 && lightmapPageID < GetNumLightmapPages() );
		return m_pLightmapPages[lightmapPageID].m_Width;

	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED:
		AssertOnce( !"Can't use CMatLightmaps to get properties of MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED" );
		return 1;
	
	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE:
	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP:
		return 1;
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CMatLightmaps::GetLightmapHeight( int lightmapPageID ) const
{
	switch( lightmapPageID )
	{
	default:
 		Assert( lightmapPageID >= 0 && lightmapPageID < GetNumLightmapPages() );
		return m_pLightmapPages[lightmapPageID].m_Height;

	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED:
		AssertOnce( !"Can't use CMatLightmaps to get properties of MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED" );
		return 1;
	
	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE:
	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP:
		return 1;
	}
}


//-----------------------------------------------------------------------------
// Clean up lightmap pages.
//-----------------------------------------------------------------------------
void CMatLightmaps::CleanupLightmaps()
{
   if ( mat_lightmap_pfms.GetBool())
   {
      // Write PFM files containing lightmap data for this page
      for (int lightmap = 0; lightmap < GetNumLightmapPages(); lightmap++)
      {
         if ((NULL != m_pLightmapDataPtrArray) && (NULL != m_pLightmapDataPtrArray[lightmap]))
         {
            char szPFMFileName[MAX_PATH];

            sprintf(szPFMFileName, "Lightmap-Page-%d.pfm", lightmap);
            m_pLightmapDataPtrArray[lightmap]->WritePFM(szPFMFileName);
         }
      }
   }

   // Remove the lightmap data bitmap representations
   if (m_pLightmapDataPtrArray)
   {
      int i;
      for( i = 0; i < GetNumLightmapPages(); i++ )
      {
         delete m_pLightmapDataPtrArray[i];
      }

      delete [] m_pLightmapDataPtrArray;
      m_pLightmapDataPtrArray = NULL;
   }
   
   // delete old lightmap pages
	if( m_pLightmapPages )
	{
		int i;
		for( i = 0; i < GetNumLightmapPages(); i++ )
		{
			g_pShaderAPI->DeleteTexture( m_LightmapPageTextureHandles[i] );
		}
		delete [] m_pLightmapPages;
		m_pLightmapPages = 0;
	}

	m_NumLightmapPages = 0;
}

//-----------------------------------------------------------------------------
// Resets the lightmap page info for each material
//-----------------------------------------------------------------------------
void CMatLightmaps::ResetMaterialLightmapPageInfo( void )
{
	for (MaterialHandle_t i = GetMaterialDict()->FirstMaterial(); i != GetMaterialDict()->InvalidMaterial(); i = GetMaterialDict()->NextMaterial(i) )
	{
		IMaterialInternal *pMaterial = GetMaterialInternal(i);
		pMaterial->SetMinLightmapPageID( 9999 );
		pMaterial->SetMaxLightmapPageID( -9999 );
		pMaterial->SetNeedsWhiteLightmap( false );
	}
}

//-----------------------------------------------------------------------------
// This is called before any lightmap allocations take place
//-----------------------------------------------------------------------------
void CMatLightmaps::BeginLightmapAllocation()
{	
	// delete old lightmap pages
	CleanupLightmaps();

	m_ImagePackers.RemoveAll();
	intp i = m_ImagePackers.AddToTail();
	m_ImagePackers[i].Reset( 0, GetMaxLightmapPageWidth(), GetMaxLightmapPageHeight() );

	SetCurrentMaterialInternal(0);
	m_currentWhiteLightmapMaterial = 0;
	m_numSortIDs = 0;

	// need to set the min and max sorting id number for each material to 
	// a default value that basically means that it hasn't been used yet.
	ResetMaterialLightmapPageInfo();

	EnumerateMaterials();
}


//-----------------------------------------------------------------------------
// Allocates space in the lightmaps; must be called after BeginLightmapAllocation
//-----------------------------------------------------------------------------
int CMatLightmaps::AllocateLightmap( int width, int height, 
		                               int offsetIntoLightmapPage[2],
									   IMaterial *iMaterial )
{
	IMaterialInternal *pMaterial = static_cast<IMaterialInternal *>( iMaterial );
	if ( !pMaterial )
	{
		Warning( "Programming error: CMatRenderContext::AllocateLightmap: NULL material\n" );
		return m_numSortIDs;
	}
	pMaterial = pMaterial->GetRealTimeVersion(); //always work with the real time versions of materials internally
	
	// material change
	intp i;
	intp nPackCount = m_ImagePackers.Count();
	if ( GetCurrentMaterialInternal() != pMaterial )
	{
		// If this happens, then we need to close out all image packers other than
		// the last one so as to produce as few sort IDs as possible
		for ( i = nPackCount - 1; --i >= 0; )
		{
			// NOTE: We *must* use the order preserving one here so the remaining one
			// is the last lightmap
			m_ImagePackers.Remove( i );
			--nPackCount;
		}

		// If it's not the first material, increment the sort id
		if (GetCurrentMaterialInternal())
		{
			m_ImagePackers[0].IncrementSortId( );
			++m_numSortIDs;
		}

		SetCurrentMaterialInternal(pMaterial);

		// This assertion guarantees we don't see the same material twice in this loop.
		Assert( pMaterial->GetMinLightmapPageID( ) > pMaterial->GetMaxLightmapPageID() );
		Assert( GetCurrentMaterialInternal() );

		// NOTE: We may not use this lightmap page, but we might
		// we won't know for sure until the next material is passed in.
		// So, for now, we're going to forcibly add the current lightmap
		// page to this material so the sort IDs work out correctly.
		GetCurrentMaterialInternal()->SetMinLightmapPageID( GetNumLightmapPages() );
		GetCurrentMaterialInternal()->SetMaxLightmapPageID( GetNumLightmapPages() );
	}

	// Try to add it to any of the current images...
	bool bAdded = false;
	for ( i = 0; i < nPackCount; ++i )
	{
		bAdded = m_ImagePackers[i].AddBlock( width, height, &offsetIntoLightmapPage[0], &offsetIntoLightmapPage[1] );
		if ( bAdded )
			break;
	}

	if ( !bAdded )
	{
		++m_numSortIDs;
		i = m_ImagePackers.AddToTail();
		m_ImagePackers[i].Reset( m_numSortIDs, GetMaxLightmapPageWidth(), GetMaxLightmapPageHeight() );
		++m_NumLightmapPages;
		if ( !m_ImagePackers[i].AddBlock( width, height, &offsetIntoLightmapPage[0], &offsetIntoLightmapPage[1] ) )
		{
			Error( "MaterialSystem_Interface_t::AllocateLightmap: lightmap (%dx%d) too big to fit in page (%dx%d)\n", 
				width, height, GetMaxLightmapPageWidth(), GetMaxLightmapPageHeight() );
		}

		// Add this lightmap to the material...
		GetCurrentMaterialInternal()->SetMaxLightmapPageID( GetNumLightmapPages() );
	}

	return m_ImagePackers[i].GetSortId();
}

// UNDONE: This needs testing, but it appears as though creating these textures managed
// results in huge stalls whenever they are locked for modify.
// That makes sense given the d3d docs, but these have been flagged as managed for quite some time.
#define DYNAMIC_TEXTURES_NO_BACKING 1

void CMatLightmaps::EndLightmapAllocation()
{
	// count the last page that we were on.if it wasn't 
	// and count the last sortID that we were on
	m_NumLightmapPages++; 
	m_numSortIDs++;

	m_firstDynamicLightmap = m_NumLightmapPages;
	// UNDONE: Until we start using the separate dynamic lighting textures don't allocate them
	// NOTE: Enable this if we want to stop locking the base lightmaps and instead only lock update
	// these completely dynamic pages
//	m_NumLightmapPages += COUNT_DYNAMIC_LIGHTMAP_PAGES;
	m_dynamic.Init();

	// Compute the dimensions of the last lightmap 
	int lastLightmapPageWidth, lastLightmapPageHeight;
	int nLastIdx = m_ImagePackers.Count();
	m_ImagePackers[nLastIdx - 1].GetMinimumDimensions( &lastLightmapPageWidth, &lastLightmapPageHeight );
	m_ImagePackers.Purge();

	m_pLightmapPages = new LightmapPageInfo_t[GetNumLightmapPages()];
	Assert( m_pLightmapPages );

   if ( mat_lightmap_pfms.GetBool())
   {
      // This array will be used to write PFM files full of lightmap data
      m_pLightmapDataPtrArray = new FloatBitMap_t*[GetNumLightmapPages()];
   }

	int i;
	m_LightmapPageTextureHandles.EnsureCapacity( GetNumLightmapPages() );
	for ( i = 0; i < GetNumLightmapPages(); i++ )
	{
		// Compute lightmap dimensions
		bool lastStaticLightmap = ( i == (m_firstDynamicLightmap-1));
		m_pLightmapPages[i].m_Width = (unsigned short)(lastStaticLightmap ? lastLightmapPageWidth : GetMaxLightmapPageWidth());
		m_pLightmapPages[i].m_Height = (unsigned short)(lastStaticLightmap ? lastLightmapPageHeight : GetMaxLightmapPageHeight());
		m_pLightmapPages[i].m_Flags = 0;

		AllocateLightmapTexture( i );

        if ( mat_lightmap_pfms.GetBool())
        {
           // Initialize the pointers to lightmap data
           m_pLightmapDataPtrArray[i] = NULL;
        }
	}
}

//-----------------------------------------------------------------------------
// Allocate lightmap textures
//-----------------------------------------------------------------------------
void CMatLightmaps::AllocateLightmapTexture( int lightmap )
{
	bool bUseDynamicTextures = HardwareConfig()->PreferDynamicTextures();

	int flags = bUseDynamicTextures ? TEXTURE_CREATE_DYNAMIC : TEXTURE_CREATE_MANAGED;

	m_LightmapPageTextureHandles.EnsureCount( lightmap + 1 );

	char debugName[256];
	Q_snprintf( debugName, sizeof( debugName ), "[lightmap %d]", lightmap );
	
	ImageFormat imageFormat;
	switch ( HardwareConfig()->GetHDRType() )
	{
	default:
		Assert( 0 );
		[[fallthrough]];

	case HDR_TYPE_NONE:
		imageFormat = IMAGE_FORMAT_RGBA8888;
		flags |= TEXTURE_CREATE_SRGB;
		break;

	case HDR_TYPE_INTEGER:
		imageFormat = IMAGE_FORMAT_RGBA16161616;
		break;

	case HDR_TYPE_FLOAT:
		imageFormat = IMAGE_FORMAT_RGBA16161616F;
		break;
	}

	switch ( m_eLightmapsState )
	{
	case STATE_DEFAULT:
		// Allow allocations in default state
		{
			m_LightmapPageTextureHandles[lightmap] = g_pShaderAPI->CreateTexture( 
				GetLightmapWidth(lightmap), GetLightmapHeight(lightmap), 1,
				imageFormat, 
				1, 1, flags, debugName, TEXTURE_GROUP_LIGHTMAP );	// don't mipmap lightmaps

			// Load up the texture data
			g_pShaderAPI->ModifyTexture( m_LightmapPageTextureHandles[lightmap] );
			g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR );
			g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );

			if ( !bUseDynamicTextures )
			{
				g_pShaderAPI->TexSetPriority( 1 );
			}

			// Blat out the lightmap bits
			InitLightmapBits( lightmap );
		}
		break;

	case STATE_RELEASED:
		// Not assigned m_LightmapPageTextureHandles[lightmap];
		DevMsg( "AllocateLightmapTexture(%d) in released lightmap state (STATE_RELEASED), delayed till \"Restore\".\n", lightmap );
		return;

	default:
		// Not assigned m_LightmapPageTextureHandles[lightmap];
		Warning( "AllocateLightmapTexture(%d) in unknown lightmap state (%d), skipped.\n", lightmap, m_eLightmapsState );
		AssertMsg( false, "AllocateLightmapTexture(?) in unknown lightmap state (?)" );
		return;
	}
}


int	CMatLightmaps::AllocateWhiteLightmap( IMaterial *iMaterial )
{
	IMaterialInternal *pMaterial = static_cast<IMaterialInternal *>( iMaterial );
	if( !pMaterial )
	{
		Warning( "Programming error: CMatRenderContext::AllocateWhiteLightmap: NULL material\n" );
		return m_numSortIDs;
	}
	pMaterial = pMaterial->GetRealTimeVersion(); //always work with the real time versions of materials internally

	if ( !m_currentWhiteLightmapMaterial || ( m_currentWhiteLightmapMaterial != pMaterial ) )
	{
		if ( !GetCurrentMaterialInternal() && !m_currentWhiteLightmapMaterial )
		{
			// don't increment if this is the very first material (ie. no lightmaps
			// allocated with AllocateLightmap
			// Assert( 0 );
		}
		else
		{
			// material change
			m_numSortIDs++;
#if 0
			char buf[128];
			Q_snprintf( buf, sizeof( buf ), "AllocateWhiteLightmap: m_numSortIDs = %d %s\n", m_numSortIDs, pMaterial->GetName() );
			Plat_DebugString( buf );
#endif
		}
//		Warning( "%d material: \"%s\" lightmapPageID: -1\n", m_numSortIDs, pMaterial->GetName() );
		m_currentWhiteLightmapMaterial = pMaterial;
		pMaterial->SetNeedsWhiteLightmap( true );
	}

	return m_numSortIDs;
}

//-----------------------------------------------------------------------------
// Releases/restores lightmap pages
//-----------------------------------------------------------------------------
void CMatLightmaps::ReleaseLightmapPages()
{
	switch ( m_eLightmapsState )
	{
	case STATE_DEFAULT:
		// Allow release in default state only
		break;
	
	default:
		Warning( "ReleaseLightmapPages is expected in STATE_DEFAULT, current state = %d, discarded.\n", m_eLightmapsState );
		AssertMsg( false, "ReleaseLightmapPages is expected in STATE_DEFAULT" );
		return;
	}

	for( int i = 0; i < GetNumLightmapPages(); i++ )
	{
		g_pShaderAPI->DeleteTexture( m_LightmapPageTextureHandles[i] );
	}
	
	// We are now in released state
	m_eLightmapsState = STATE_RELEASED;
}

void CMatLightmaps::RestoreLightmapPages()
{
	switch ( m_eLightmapsState )
	{
	case STATE_RELEASED:
		// Allow restore in released state only
		break;

	default:
		Warning( "RestoreLightmapPages is expected in STATE_RELEASED, current state = %d, discarded.\n", m_eLightmapsState );
		AssertMsg( false, "RestoreLightmapPages is expected in STATE_RELEASED" );
		return;
	}

	// Switch to default state to allow allocations
	m_eLightmapsState = STATE_DEFAULT;

	for( int i = 0; i < GetNumLightmapPages(); i++ )
	{
		AllocateLightmapTexture( i );
	}
}


//-----------------------------------------------------------------------------
// This initializes the lightmap bits
//-----------------------------------------------------------------------------
void CMatLightmaps::InitLightmapBits( int lightmap )
{
	VPROF_( "CMatLightmaps::InitLightmapBits", 1, VPROF_BUDGETGROUP_DLIGHT_RENDERING, false, 0 );
	int width = GetLightmapWidth(lightmap);
	int height = GetLightmapHeight(lightmap);

	CPixelWriter writer;

	g_pShaderAPI->ModifyTexture( m_LightmapPageTextureHandles[lightmap] );
	if ( !g_pShaderAPI->TexLock( 0, 0, 0, 0, width, height, writer ) )
		return;

	// Debug mode, make em green checkerboard
	if ( writer.IsUsingFloatFormat() )
	{
		for ( int j = 0; j < height; ++j )
		{
			writer.Seek( 0, j );
			for ( int k = 0; k < width; ++k )
			{
#ifndef _DEBUG
				writer.WritePixel( 1.0f, 1.0f, 1.0f );
#else // _DEBUG
				if( ( j + k ) & 1 )
				{
					writer.WritePixelF( 0.0f, 1.0f, 0.0f );
				}
				else
				{
					writer.WritePixelF( 0.0f, 0.0f, 0.0f );
				}
#endif // _DEBUG
			}
		}
	}
	else
	{
		for ( int j = 0; j < height; ++j )
		{
			writer.Seek( 0, j );
			for ( int k = 0; k < width; ++k )
			{
#ifndef _DEBUG
				// note: make this white to find multisample centroid sampling problems.
				//				writer.WritePixel( 255, 255, 255 );
				writer.WritePixel( 0, 0, 0 );
#else // _DEBUG
				if ( ( j + k ) & 1 )
				{
					writer.WritePixel( 0, 255, 0 );
				}
				else
				{
					writer.WritePixel( 0, 0, 0 );
				}
#endif // _DEBUG
			}
		}
	}

	g_pShaderAPI->TexUnlock();
}

bool CMatLightmaps::LockLightmap( int lightmap )
{
//	Warning( "locking lightmap page: %d\n", lightmap );
	VPROF_INCREMENT_COUNTER( "lightmap fullpage texlock", 1 );
	if( m_nLockedLightmap != -1 )
	{
		g_pShaderAPI->TexUnlock();
	}
	g_pShaderAPI->ModifyTexture( m_LightmapPageTextureHandles[lightmap] );
	int pageWidth  = m_pLightmapPages[lightmap].m_Width;
	int pageHeight = m_pLightmapPages[lightmap].m_Height;
	if (!g_pShaderAPI->TexLock( 0, 0, 0, 0,	pageWidth, pageHeight, m_LightmapPixelWriter ))
	{
		Assert( 0 );
		return false;
	}
	m_nLockedLightmap = lightmap;
	return true;
}

// write bumped lightmap update to LDR 8-bit lightmap
void CMatLightmaps::BumpedLightmapBitsToPixelWriter_LDR( float* pFloatImage, float *pFloatImageBump1, float *pFloatImageBump2, 
	float *pFloatImageBump3, int pLightmapSize[2], int pOffsetIntoLightmapPage[2], FloatBitMap_t *pfmOut )
{
	const int nLightmapSize0 = pLightmapSize[0];
	const int nLightmap0WriterSizeBytes = nLightmapSize0 * m_LightmapPixelWriter.GetPixelSize();
	const int nRewindToNextPixel = -( ( nLightmap0WriterSizeBytes * 3 ) - m_LightmapPixelWriter.GetPixelSize() );

	for( int t = 0; t < pLightmapSize[1]; t++ )
	{
		int srcTexelOffset = ( sizeof( Vector4D ) / sizeof( float ) ) * ( 0 + t * nLightmapSize0 );
		m_LightmapPixelWriter.Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );

		for( int s = 0; s < nLightmapSize0; 
			s++, m_LightmapPixelWriter.SkipBytes(nRewindToNextPixel),srcTexelOffset += (sizeof(Vector4D)/sizeof(float)))
		{
			unsigned char color[4][3];

			ColorSpace::LinearToBumpedLightmap( &pFloatImage[srcTexelOffset],
				&pFloatImageBump1[srcTexelOffset], &pFloatImageBump2[srcTexelOffset],
				&pFloatImageBump3[srcTexelOffset],
				color[0], color[1], color[2], color[3] );

			unsigned char alpha =  RoundFloatToByte( pFloatImage[srcTexelOffset+3] * 255.0f );
			m_LightmapPixelWriter.WritePixelNoAdvance( color[0][0], color[0][1], color[0][2], alpha );

			m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
			m_LightmapPixelWriter.WritePixelNoAdvance( color[1][0], color[1][1], color[1][2], alpha );

			m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
			m_LightmapPixelWriter.WritePixelNoAdvance( color[2][0], color[2][1], color[2][2], alpha );

			m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
			m_LightmapPixelWriter.WritePixelNoAdvance( color[3][0], color[3][1], color[3][2], alpha );
		}
	}
	if ( pfmOut )
	{
		for( int t = 0; t < pLightmapSize[1]; t++ )
		{
			int srcTexelOffset = ( sizeof( Vector4D ) / sizeof( float ) ) * ( 0 + t * nLightmapSize0 );
			for( int s = 0;  s < nLightmapSize0; s++,srcTexelOffset += (sizeof(Vector4D)/sizeof(float)))
			{
				unsigned char color[4][3];

				ColorSpace::LinearToBumpedLightmap( &pFloatImage[srcTexelOffset],
					&pFloatImageBump1[srcTexelOffset], &pFloatImageBump2[srcTexelOffset],
					&pFloatImageBump3[srcTexelOffset],
					color[0], color[1], color[2], color[3] );

				unsigned char alpha =  RoundFloatToByte( pFloatImage[srcTexelOffset+3] * 255.0f );
				// Write data to the bitmapped represenations so that PFM files can be written
				PixRGBAF pixelData;
				pixelData.Red = color[0][0];                  
				pixelData.Green = color[0][1];                  
				pixelData.Blue = color[0][2];
				pixelData.Alpha = alpha;
				pfmOut->WritePixelRGBAF( pOffsetIntoLightmapPage[0] + s, pOffsetIntoLightmapPage[1] + t, pixelData);
			}
		}

	}
}

// write bumped lightmap update to HDR float lightmap
void CMatLightmaps::BumpedLightmapBitsToPixelWriter_HDRF( float* pFloatImage, float *pFloatImageBump1, float *pFloatImageBump2, 
												 float *pFloatImageBump3, int pLightmapSize[2], int pOffsetIntoLightmapPage[2], FloatBitMap_t *pfmOut )
{
	Assert( !pfmOut );		// unsupported in this mode

	const int nLightmapSize0 = pLightmapSize[0];
	const int nLightmap0WriterSizeBytes = nLightmapSize0 * m_LightmapPixelWriter.GetPixelSize();
	const int nRewindToNextPixel = -( ( nLightmap0WriterSizeBytes * 3 ) - m_LightmapPixelWriter.GetPixelSize() );

	for( int t = 0; t < pLightmapSize[1]; t++ )
	{
		int srcTexelOffset = ( sizeof( Vector4D ) / sizeof( float ) ) * ( 0 + t * nLightmapSize0 );
		m_LightmapPixelWriter.Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );

		for( int s = 0; 
			s < nLightmapSize0; 
			s++, m_LightmapPixelWriter.SkipBytes(nRewindToNextPixel),srcTexelOffset += (sizeof(Vector4D)/sizeof(float)))
		{
			m_LightmapPixelWriter.WritePixelNoAdvanceF( pFloatImage[srcTexelOffset], pFloatImage[srcTexelOffset+1],
				pFloatImage[srcTexelOffset+2], pFloatImage[srcTexelOffset+3] );

			m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
			m_LightmapPixelWriter.WritePixelNoAdvanceF( pFloatImageBump1[srcTexelOffset], pFloatImageBump1[srcTexelOffset+1],
				pFloatImageBump1[srcTexelOffset+2], pFloatImage[srcTexelOffset+3] ); //-V778

			m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
			m_LightmapPixelWriter.WritePixelNoAdvanceF( pFloatImageBump2[srcTexelOffset], pFloatImageBump2[srcTexelOffset+1],
				pFloatImageBump2[srcTexelOffset+2], pFloatImage[srcTexelOffset+3] );

			m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
			m_LightmapPixelWriter.WritePixelNoAdvanceF( pFloatImageBump3[srcTexelOffset], pFloatImageBump3[srcTexelOffset+1],
				pFloatImageBump3[srcTexelOffset+2], pFloatImage[srcTexelOffset+3] );
		}
	}
}

// write bumped lightmap update to HDR integer lightmap
void CMatLightmaps::BumpedLightmapBitsToPixelWriter_HDRI( float* RESTRICT pFloatImage, float * RESTRICT pFloatImageBump1, float * RESTRICT pFloatImageBump2, 
												 float * RESTRICT pFloatImageBump3, int pLightmapSize[2], int pOffsetIntoLightmapPage[2], FloatBitMap_t *pfmOut )
{
	const int nLightmapSize0 = pLightmapSize[0];
	const int nLightmap0WriterSizeBytes = nLightmapSize0 * m_LightmapPixelWriter.GetPixelSize();
	const int nRewindToNextPixel = -( ( nLightmap0WriterSizeBytes * 3 ) - m_LightmapPixelWriter.GetPixelSize() );

	if( m_LightmapPixelWriter.IsUsingFloatFormat() )
	{
		{
			for( int t = 0; t < pLightmapSize[1]; t++ )
			{
				int srcTexelOffset = ( sizeof( Vector4D ) / sizeof( float ) ) * ( 0 + t * nLightmapSize0 );
				m_LightmapPixelWriter.Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );

				for( int s = 0; 
					s < nLightmapSize0; 
					s++, m_LightmapPixelWriter.SkipBytes(nRewindToNextPixel),srcTexelOffset += (sizeof(Vector4D)/sizeof(float)))
				{
					unsigned short color[4][4];

					ColorSpace::LinearToBumpedLightmap( &pFloatImage[srcTexelOffset],
						&pFloatImageBump1[srcTexelOffset], &pFloatImageBump2[srcTexelOffset],
						&pFloatImageBump3[srcTexelOffset],
						color[0], color[1], color[2], color[3] );
					float alpha = pFloatImage[srcTexelOffset+3];
					Assert( alpha >= 0.0f && alpha <= 1.0f );
					color[0][3] = color[1][3] = color[2][3] = color[3][3] = alpha;

					float toFloat = ( 1.0f / ( float )( 1 << 16 ) );

					m_LightmapPixelWriter.WritePixelNoAdvanceF( toFloat * color[0][0], toFloat * color[0][1], toFloat * color[0][2], toFloat * color[0][3] );

					m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
					m_LightmapPixelWriter.WritePixelNoAdvanceF( toFloat * color[1][0], toFloat * color[1][1], toFloat * color[1][2], toFloat * color[1][3] );

					m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
					m_LightmapPixelWriter.WritePixelNoAdvanceF( toFloat * color[2][0], toFloat * color[2][1], toFloat * color[2][2], toFloat * color[2][3] );

					m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
					m_LightmapPixelWriter.WritePixelNoAdvanceF( toFloat * color[3][0], toFloat * color[3][1], toFloat * color[3][2], toFloat * color[3][3] );
				}
			}
		}
	}
	else
	{
		for( int t = 0; t < pLightmapSize[1]; t++ )
		{
			int srcTexelOffset = ( sizeof( Vector4D ) / sizeof( float ) ) * ( 0 + t * nLightmapSize0 );
			m_LightmapPixelWriter.Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );

			for( int s = 0; 
				s < nLightmapSize0; 
				s++, m_LightmapPixelWriter.SkipBytes(nRewindToNextPixel),srcTexelOffset += (sizeof(Vector4D)/sizeof(float)))
			{					
				unsigned short color[4][4];

				ColorSpace::LinearToBumpedLightmap( &pFloatImage[srcTexelOffset],
					&pFloatImageBump1[srcTexelOffset], &pFloatImageBump2[srcTexelOffset],
					&pFloatImageBump3[srcTexelOffset],
					color[0], color[1], color[2], color[3] );
				unsigned short alpha = ColorSpace::LinearToUnsignedShort( pFloatImage[srcTexelOffset+3], 16 );
				color[0][3] = color[1][3] = color[2][3] = color[3][3] = alpha;

				m_LightmapPixelWriter.WritePixelNoAdvance( color[0][0], color[0][1], color[0][2], color[0][3] );

				m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
				m_LightmapPixelWriter.WritePixelNoAdvance( color[1][0], color[1][1], color[1][2], color[1][3] );

				m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
				m_LightmapPixelWriter.WritePixelNoAdvance( color[2][0], color[2][1], color[2][2], color[2][3] );

				m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
				m_LightmapPixelWriter.WritePixelNoAdvance( color[3][0], color[3][1], color[3][2], color[3][3] );

				// Write data to the bitmapped represenations so that PFM files can be written
				if ( pfmOut )
				{
					PixRGBAF pixelData;
					pixelData.Red = color[0][0];                  
					pixelData.Green = color[0][1];                  
					pixelData.Blue = color[0][2];
					pixelData.Alpha = alpha;
					pfmOut->WritePixelRGBAF(pOffsetIntoLightmapPage[0] + s, pOffsetIntoLightmapPage[1] + t, pixelData);
				}
			}
		}
	}
}


void CMatLightmaps::LightmapBitsToPixelWriter_LDR( float* pFloatImage, int pLightmapSize[2], int pOffsetIntoLightmapPage[2], FloatBitMap_t *pfmOut )
{
	// non-HDR lightmap processing
	float *pSrc = pFloatImage;
	for( int t = 0; t < pLightmapSize[1]; ++t )
	{
		m_LightmapPixelWriter.Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );
		for( int s = 0; s < pLightmapSize[0]; ++s, pSrc += (sizeof(Vector4D)/sizeof(*pSrc)) )
		{
			unsigned char color[4];
			ColorSpace::LinearToLightmap( color, pSrc );
			color[3] =  RoundFloatToByte( pSrc[3] * 255.0f );
			m_LightmapPixelWriter.WritePixel( color[0], color[1], color[2], color[3] );

			if ( pfmOut )
			{
				// Write data to the bitmapped represenations so that PFM files can be written
				PixRGBAF pixelData;
				pixelData.Red = color[0];                  
				pixelData.Green = color[1];                  
				pixelData.Blue = color[2];
				pixelData.Alpha = color[3];
				pfmOut->WritePixelRGBAF( pOffsetIntoLightmapPage[0] + s, pOffsetIntoLightmapPage[1] + t, pixelData );
			}
		}
	}
}


void CMatLightmaps::LightmapBitsToPixelWriter_HDRF( float* pFloatImage, int pLightmapSize[2], int pOffsetIntoLightmapPage[2], FloatBitMap_t *pfmOut )
{
	// float HDR lightmap processing
	float *pSrc = pFloatImage;
	for ( int t = 0; t < pLightmapSize[1]; ++t )
	{
		m_LightmapPixelWriter.Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );
		for ( int s = 0; s < pLightmapSize[0]; ++s, pSrc += (sizeof(Vector4D)/sizeof(*pSrc)) )
		{
			m_LightmapPixelWriter.WritePixelF( pSrc[0], pSrc[1], pSrc[2], pSrc[3] );
		}
	}
}

// numbers come in on the domain [0..16]
void CMatLightmaps::LightmapBitsToPixelWriter_HDRI( float* RESTRICT pFloatImage, int pLightmapSize[2], int pOffsetIntoLightmapPage[2], FloatBitMap_t * RESTRICT pfmOut )
{
	// PC code (and old, pre-SIMD xbox version -- unshippably slow)
	if ( m_LightmapPixelWriter.IsUsingFloatFormat() )
	{
		// integer HDR lightmap processing
		float *pSrc = pFloatImage;
		for ( int t = 0; t < pLightmapSize[1]; ++t )
		{
			m_LightmapPixelWriter.Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );
			for ( int s = 0; s < pLightmapSize[0]; ++s, pSrc += (sizeof(Vector4D)/sizeof(*pSrc)) )
			{
				int r, g, b, a;

				r = ColorSpace::LinearFloatToCorrectedShort( pSrc[0] );
				g = ColorSpace::LinearFloatToCorrectedShort( pSrc[1] );
				b = ColorSpace::LinearFloatToCorrectedShort( pSrc[2] );
				a = ColorSpace::LinearToUnsignedShort( pSrc[3], 16 );

				float toFloat = ( 1.0f / ( float )( 1 << 16 ) );

				Assert( pSrc[3] >= 0.0f && pSrc[3] <= 1.0f );
				m_LightmapPixelWriter.WritePixelF( r * toFloat, g * toFloat, b * toFloat, pSrc[3] );
			}
		}
	}
	else
	{
		// integer HDR lightmap processing
		float *pSrc = pFloatImage;
		for ( int t = 0; t < pLightmapSize[1]; ++t )
		{
			m_LightmapPixelWriter.Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );
			for ( int s = 0; s < pLightmapSize[0]; ++s, pSrc += (sizeof(Vector4D)/sizeof(*pSrc)) )
			{
				int r, g, b, a;

				r = ColorSpace::LinearFloatToCorrectedShort( pSrc[0] );
				g = ColorSpace::LinearFloatToCorrectedShort( pSrc[1] );
				b = ColorSpace::LinearFloatToCorrectedShort( pSrc[2] );
				a = ColorSpace::LinearToUnsignedShort( pSrc[3], 16 );

				m_LightmapPixelWriter.WritePixel( r, g, b, a );

				if ( pfmOut )
				{
					// Write data to the bitmapped represenations so that PFM files can be written
					PixRGBAF pixelData;
					pixelData.Red = pSrc[0];                  
					pixelData.Green = pSrc[1];                  
					pixelData.Blue = pSrc[2];
					pixelData.Alpha = pSrc[3];
					pfmOut->WritePixelRGBAF( pOffsetIntoLightmapPage[0] + s, pOffsetIntoLightmapPage[1] + t, pixelData );
				}				
			}
		}
	}
}

void CMatLightmaps::BeginUpdateLightmaps( void )
{
	CMatCallQueue *pCallQueue = GetMaterialSystem()->GetRenderContextInternal()->GetCallQueueInternal();
	if ( pCallQueue )
	{
		pCallQueue->QueueCall( this, &CMatLightmaps::BeginUpdateLightmaps );
		return;
	}

	m_nUpdatingLightmapsStackDepth++;
}

void CMatLightmaps::EndUpdateLightmaps( void )
{
	CMatCallQueue *pCallQueue = GetMaterialSystem()->GetRenderContextInternal()->GetCallQueueInternal();
	if ( pCallQueue )
	{
		pCallQueue->QueueCall( this, &CMatLightmaps::EndUpdateLightmaps );
		return;
	}

	m_nUpdatingLightmapsStackDepth--;
	Assert( m_nUpdatingLightmapsStackDepth >= 0 );
	if( m_nUpdatingLightmapsStackDepth <= 0 && m_nLockedLightmap != -1 )
	{
		g_pShaderAPI->TexUnlock();
		m_nLockedLightmap = -1;
	}
}

int CMatLightmaps::AllocateDynamicLightmap( int lightmapSize[2], int *pOutOffsetIntoPage, int frameID )
{
	// check frameID, fail if current
	for ( int i = 0; i < COUNT_DYNAMIC_LIGHTMAP_PAGES; i++ ) //-V1008
	{
		int dynamicIndex = (m_dynamic.currentDynamicIndex + i) % COUNT_DYNAMIC_LIGHTMAP_PAGES; //-V1063
		int lightmapPageIndex = m_firstDynamicLightmap + dynamicIndex;
		if ( m_dynamic.lightmapLockFrame[dynamicIndex] != frameID )
		{
			m_dynamic.lightmapLockFrame[dynamicIndex] = frameID;
			m_dynamic.imagePackers[dynamicIndex].Reset( 0, m_pLightmapPages[lightmapPageIndex].m_Width, m_pLightmapPages[lightmapPageIndex].m_Height );
		}

		if ( m_dynamic.imagePackers[dynamicIndex].AddBlock( lightmapSize[0], lightmapSize[1], &pOutOffsetIntoPage[0], &pOutOffsetIntoPage[1] ) )
		{
			return lightmapPageIndex;
		}
	}
	
	return -1;
}

//-----------------------------------------------------------------------------
// Updates the lightmap
//-----------------------------------------------------------------------------
void CMatLightmaps::UpdateLightmap( int lightmapPageID, int lightmapSize[2],
									  int offsetIntoLightmapPage[2], 
									  float *pFloatImage, float *pFloatImageBump1,
									  float *pFloatImageBump2, float *pFloatImageBump3 )
{
	VPROF( "CMatRenderContext::UpdateLightmap" );

	bool hasBump = false;
	int uSize = 1;
	FloatBitMap_t *pfmOut = NULL;
	if ( pFloatImageBump1 && pFloatImageBump2 && pFloatImageBump3 )
	{
		hasBump = true;
		uSize = 4;
	}

	if ( lightmapPageID >= GetNumLightmapPages() || lightmapPageID < 0 )
	{
		Error( "MaterialSystem_Interface_t::UpdateLightmap lightmapPageID=%d out of range\n", lightmapPageID );
		return;
	}
	bool bDynamic = IsDynamicLightmap(lightmapPageID);

	if ( bDynamic )
	{
		int dynamicIndex = lightmapPageID-m_firstDynamicLightmap;
		Assert(dynamicIndex < COUNT_DYNAMIC_LIGHTMAP_PAGES);
		m_dynamic.currentDynamicIndex = (dynamicIndex + 1) % COUNT_DYNAMIC_LIGHTMAP_PAGES; //-V1063
	}

	if ( mat_lightmap_pfms.GetBool())
	{
		// Allocate and initialize lightmap data that will be written to a PFM file
		if (NULL == m_pLightmapDataPtrArray[lightmapPageID])
		{
			m_pLightmapDataPtrArray[lightmapPageID] = new FloatBitMap_t(m_pLightmapPages[lightmapPageID].m_Width, m_pLightmapPages[lightmapPageID].m_Height);
			m_pLightmapDataPtrArray[lightmapPageID]->Clear(0, 0, 0, 1);
		}
		pfmOut = m_pLightmapDataPtrArray[lightmapPageID];
	}

	// NOTE: Change how the lock is taking place if you ever change how bumped
	// lightmaps are put into the page. Right now, we assume that they're all
	// added to the right of the original lightmap.
	bool bLockSubRect;
	{
		VPROF_( "Locking lightmaps", 2, VPROF_BUDGETGROUP_DLIGHT_RENDERING, false, 0 ); // vprof scope

		bLockSubRect = m_nUpdatingLightmapsStackDepth <= 0 && !bDynamic;
		if( bLockSubRect )
		{
			VPROF_INCREMENT_COUNTER( "lightmap subrect texlock", 1 );
			g_pShaderAPI->ModifyTexture( m_LightmapPageTextureHandles[lightmapPageID] );
			if (!g_pShaderAPI->TexLock( 0, 0, offsetIntoLightmapPage[0], offsetIntoLightmapPage[1],
				lightmapSize[0] * uSize, lightmapSize[1], m_LightmapPixelWriter ))
			{
				return;
			}
		}
		else if( lightmapPageID != m_nLockedLightmap )
		{
			if ( !LockLightmap( lightmapPageID ) )
			{
				ExecuteNTimes( 10, Warning( "Failed to lock lightmap\n" ) );
				return;
			}
		}
	}

	int subRectOffset[2] = {0,0};

	{
		// account for the part spent in math:
		VPROF_( "LightmapBitsToPixelWriter", 2, VPROF_BUDGETGROUP_DLIGHT_RENDERING, false, 0 );
		if ( hasBump )
		{
			switch( HardwareConfig()->GetHDRType() )
			{
			case HDR_TYPE_NONE:
				BumpedLightmapBitsToPixelWriter_LDR( pFloatImage, pFloatImageBump1, pFloatImageBump2, pFloatImageBump3, 
					lightmapSize, bLockSubRect ? subRectOffset : offsetIntoLightmapPage, pfmOut );
				break;
			case HDR_TYPE_INTEGER:
				BumpedLightmapBitsToPixelWriter_HDRI( pFloatImage, pFloatImageBump1, pFloatImageBump2, pFloatImageBump3, 
					lightmapSize, bLockSubRect ? subRectOffset : offsetIntoLightmapPage, pfmOut );
				break;
			case HDR_TYPE_FLOAT:
				BumpedLightmapBitsToPixelWriter_HDRF( pFloatImage, pFloatImageBump1, pFloatImageBump2, pFloatImageBump3, 
					lightmapSize, bLockSubRect ? subRectOffset : offsetIntoLightmapPage, pfmOut );
				break;
			}
		}
		else
		{
			switch ( HardwareConfig()->GetHDRType() )
			{
			case HDR_TYPE_NONE:
				LightmapBitsToPixelWriter_LDR( pFloatImage, lightmapSize, bLockSubRect ? subRectOffset : offsetIntoLightmapPage, pfmOut );
				break;

			case HDR_TYPE_INTEGER:
				LightmapBitsToPixelWriter_HDRI( pFloatImage, lightmapSize, bLockSubRect ? subRectOffset : offsetIntoLightmapPage, pfmOut );
				break;

			case HDR_TYPE_FLOAT:
				LightmapBitsToPixelWriter_HDRF( pFloatImage, lightmapSize, bLockSubRect ? subRectOffset : offsetIntoLightmapPage, pfmOut );
				break;

			default:
				Assert( 0 );
				break;
			}
		}
	}

	if( bLockSubRect )
	{
		VPROF_( "Unlocking Lightmaps", 2, VPROF_BUDGETGROUP_DLIGHT_RENDERING, false, 0 );
		g_pShaderAPI->TexUnlock();
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int	CMatLightmaps::GetNumSortIDs( void )
{
	return m_numSortIDs;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMatLightmaps::ComputeSortInfo( MaterialSystem_SortInfo_t* pInfo, int& sortId, bool alpha )
{
	int lightmapPageID;

	for ( MaterialHandle_t i = GetMaterialDict()->FirstMaterial(); i != GetMaterialDict()->InvalidMaterial(); i = GetMaterialDict()->NextMaterial(i) )
	{
		IMaterialInternal* pMaterial = GetMaterialInternal(i);

		if ( pMaterial->GetMinLightmapPageID() > pMaterial->GetMaxLightmapPageID() )
		{
			continue;
		}
		
		//	const IMaterialVar *pTransVar = pMaterial->GetMaterialProperty( MATERIAL_PROPERTY_OPACITY );
		//	if( ( !alpha && ( pTransVar->GetIntValue() == MATERIAL_TRANSLUCENT ) ) ||
		//		( alpha && !( pTransVar->GetIntValue() == MATERIAL_TRANSLUCENT ) ) )
		//	{
		//		return true;
		//	}

	
//		Warning( "sort stuff: %s %s\n", material->GetName(), bAlpha ? "alpha" : "not alpha" );
		
		// fill in the lightmapped materials
		for ( lightmapPageID = pMaterial->GetMinLightmapPageID(); 
			 lightmapPageID <= pMaterial->GetMaxLightmapPageID(); ++lightmapPageID )
		{
			pInfo[sortId].material = pMaterial->GetQueueFriendlyVersion();
			pInfo[sortId].lightmapPageID = lightmapPageID;
#if 0
			char buf[128];
			Q_snprintf( buf, sizeof( buf ), "ComputeSortInfo: %s lightmapPageID: %d sortID: %d\n", pMaterial->GetName(), lightmapPageID, sortId );
			Plat_DebugString( buf );
#endif
			++sortId;
		}
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMatLightmaps::ComputeWhiteLightmappedSortInfo( MaterialSystem_SortInfo_t* pInfo, int& sortId, bool alpha )
{
	for (MaterialHandle_t i = GetMaterialDict()->FirstMaterial(); i != GetMaterialDict()->InvalidMaterial(); i = GetMaterialDict()->NextMaterial(i) )
	{
		IMaterialInternal* pMaterial = GetMaterialInternal(i);

		// fill in the lightmapped materials that are actually used by this level
		if( pMaterial->GetNeedsWhiteLightmap() && 
			( pMaterial->GetReferenceCount() > 0 ) )
		{
			// const IMaterialVar *pTransVar = pMaterial->GetMaterialProperty( MATERIAL_PROPERTY_OPACITY );
			//		if( ( !alpha && ( pTransVar->GetIntValue() == MATERIAL_TRANSLUCENT ) ) ||
			//			( alpha && !( pTransVar->GetIntValue() == MATERIAL_TRANSLUCENT ) ) )
			//		{
			//			return true;
			//		}

			pInfo[sortId].material = pMaterial->GetQueueFriendlyVersion();
			if( pMaterial->GetPropertyFlag( MATERIAL_PROPERTY_NEEDS_BUMPED_LIGHTMAPS ) )
			{
				pInfo[sortId].lightmapPageID = MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP;
			}
			else
			{
				pInfo[sortId].lightmapPageID = MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE;
			}

			sortId++;
		}
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMatLightmaps::GetSortInfo( MaterialSystem_SortInfo_t *pSortInfoArray )
{
	// sort non-alpha blended materials first
	int sortId = 0;
	ComputeSortInfo( pSortInfoArray, sortId, false );
	ComputeWhiteLightmappedSortInfo( pSortInfoArray, sortId, false );
	Assert( m_numSortIDs == sortId );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMatLightmaps::EnableLightmapFiltering( bool enabled )
{
	int i;
	for( i = 0; i < GetNumLightmapPages(); i++ )
	{
		g_pShaderAPI->ModifyTexture( m_LightmapPageTextureHandles[i] );
		if( enabled )
		{
			g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR );
			g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );
		}
		else
		{
			g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_NEAREST );
			g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_NEAREST );
		}
	}
}


