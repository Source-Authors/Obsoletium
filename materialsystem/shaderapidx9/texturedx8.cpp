//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#include "texturedx8.h"

#include "shaderapidx8_global.h"
#include "colorformatdx8.h"
#include "recording.h"

#include "shaderapi/ishaderapi.h"
#include "shaderapi/ishaderutil.h"
#include "materialsystem/imaterialsystem.h"

#include "tier0/icommandline.h"
#include "tier0/vprof.h"
#include "tier1/utlvector.h"
#include "tier1/utlbuffer.h"
#include "tier1/callqueue.h"

#include "vtf/vtf.h"
#include "filesystem.h"
#include "windows/com_error_category.h"

#include <atomic>

#include "tier0/memdbgon.h"

static std::atomic_uint s_TextureCount = 0;

//-----------------------------------------------------------------------------
// Stats...
//-----------------------------------------------------------------------------

unsigned TextureCount()
{
	return s_TextureCount.load(std::memory_order::memory_order_relaxed);
}

static bool IsVolumeTexture( IDirect3DBaseTexture* pBaseTexture )
{
	return pBaseTexture && pBaseTexture->GetType() == D3DRTYPE_VOLUMETEXTURE;
}

static HRESULT GetLevelDesc( IDirect3DBaseTexture* pBaseTexture, UINT level, D3DSURFACE_DESC* pDesc )
{
	MEM_ALLOC_D3D_CREDIT();

	if ( !pBaseTexture )
	{
		return E_POINTER;
	}

	HRESULT hr;
	switch( pBaseTexture->GetType() )
	{
	case D3DRTYPE_TEXTURE:
		hr = ( ( IDirect3DTexture9 * )pBaseTexture )->GetLevelDesc( level, pDesc );
		break;
	case D3DRTYPE_CUBETEXTURE:
		hr = ( ( IDirect3DCubeTexture9 * )pBaseTexture )->GetLevelDesc( level, pDesc );
		break;
	default:
		AssertMsg( false, "Unexpected texture type 0x%x.", pBaseTexture->GetType() );
		return E_NOTIMPL;
	}

	if ( FAILED(hr) )
	{
		Assert(false);
		Warning( __FUNCTION__ ": IDirect3DBaseTexture(type = 0x%x)::GetLevelDesc(level = 0x%x) failed w/e %s.\n",
			pBaseTexture->GetType(),
			level,
			se::win::com::com_error_category().message(hr).c_str() );
	}

	return hr;
}

static HRESULT GetSurfaceFromTexture( IDirect3DBaseTexture* pBaseTexture, UINT level, 
									  D3DCUBEMAP_FACES cubeFaceID, IDirect3DSurface** ppSurfLevel )
{
	MEM_ALLOC_D3D_CREDIT();

	if ( !pBaseTexture )
	{
		return E_POINTER;
	}

	HRESULT hr;

	switch( pBaseTexture->GetType() )
	{
	case D3DRTYPE_TEXTURE:
		hr = ( ( IDirect3DTexture9 * )pBaseTexture )->GetSurfaceLevel( level, ppSurfLevel );
		break;
	case D3DRTYPE_CUBETEXTURE:
		hr = ( ( IDirect3DCubeTexture9 * )pBaseTexture )->GetCubeMapSurface( cubeFaceID, level, ppSurfLevel );
		break;
	default:
		AssertMsg( false, "Unexpected texture type 0x%x.", pBaseTexture->GetType() );
		return E_NOTIMPL;
	}
	return hr;
}

//-----------------------------------------------------------------------------
// Gets the image format of a texture
//-----------------------------------------------------------------------------
static ImageFormat GetImageFormat( IDirect3DBaseTexture* pTexture )
{
	MEM_ALLOC_D3D_CREDIT();

	if ( pTexture )
	{
		HRESULT hr;
		if ( !IsVolumeTexture( pTexture ) )
		{
			D3DSURFACE_DESC desc;
			hr = GetLevelDesc( pTexture, 0, &desc );
			if ( SUCCEEDED( hr ) )
				return ImageLoader::D3DFormatToImageFormat( desc.Format );
		}
		else
		{
			D3DVOLUME_DESC desc;
			auto *pVolumeTexture = static_cast<IDirect3DVolumeTexture9*>( pTexture );
			hr = pVolumeTexture->GetLevelDesc( 0, &desc );
			if ( SUCCEEDED( hr ) )
				return ImageLoader::D3DFormatToImageFormat( desc.Format );

			Assert(false);
			Warning( __FUNCTION__ ": IDirect3DVolumeTexture9::GetLevelDesc(level = 0x%x) failed w/e %s.\n",
				0,
				se::win::com::com_error_category().message(hr).c_str() );
		}
	}

	// Bogus baby!
	return IMAGE_FORMAT_UNKNOWN;
}


//-----------------------------------------------------------------------------
// Allocates the D3DTexture
//-----------------------------------------------------------------------------
IDirect3DBaseTexture* CreateD3DTexture( int width, int height, int nDepth, 
	ImageFormat dstFormat, int numLevels, int nCreationFlags, char *debugLabel )		// OK to skip the last param
{
	if ( nDepth <= 0 )
	{
		nDepth = 1;
	}

	bool isCubeMap = ( nCreationFlags & TEXTURE_CREATE_CUBEMAP ) != 0;
	bool bIsRenderTarget = ( nCreationFlags & TEXTURE_CREATE_RENDERTARGET ) != 0;
	bool bManaged = ( nCreationFlags & TEXTURE_CREATE_MANAGED ) != 0;
	bool bSysmem = ( nCreationFlags & TEXTURE_CREATE_SYSMEM ) != 0;
	bool bIsDepthBuffer = ( nCreationFlags & TEXTURE_CREATE_DEPTHBUFFER ) != 0;
	bool isDynamic = ( nCreationFlags & TEXTURE_CREATE_DYNAMIC ) != 0;
	bool bAutoMipMap = ( nCreationFlags & TEXTURE_CREATE_AUTOMIPMAP ) != 0;
	bool bVertexTexture = ( nCreationFlags & TEXTURE_CREATE_VERTEXTEXTURE ) != 0;
	bool bAllowNonFilterable = ( nCreationFlags & TEXTURE_CREATE_UNFILTERABLE_OK ) != 0;
	bool bVolumeTexture = ( nDepth > 1 );
	[[maybe_unused]] bool bSRGB = (nCreationFlags & TEXTURE_CREATE_SRGB) != 0;			// for Posix/GL only

	// NOTE: This function shouldn't be used for creating depth buffers!
	Assert( !bIsDepthBuffer );

	D3DPOOL pool = bManaged ? D3DPOOL_MANAGED : D3DPOOL_DEFAULT;
	if ( bSysmem )
		pool = D3DPOOL_SYSTEMMEM;
	
	const D3DFORMAT d3dFormat = ImageLoader::ImageFormatToD3DFormat( FindNearestSupportedFormat( dstFormat, bVertexTexture, bIsRenderTarget, bAllowNonFilterable ) );
	if ( d3dFormat == D3DFMT_UNKNOWN )
	{
		Assert( false );
		Warning( __FUNCTION__ ": ImageLoader::ImageFormatToD3DFormat failed to find D3D image format for image format 0x%x.\n", dstFormat );
		return 0;
	}

	HRESULT hr = S_OK;
	DWORD usage = 0;

	if ( bIsRenderTarget )
	{
		usage |= D3DUSAGE_RENDERTARGET;
	}
	if ( isDynamic )
	{
		usage |= D3DUSAGE_DYNAMIC;
	}
	if ( bAutoMipMap )
	{
		usage |= D3DUSAGE_AUTOGENMIPMAP;
	}

#ifdef DX_TO_GL_ABSTRACTION
	if ( bSRGB )
	{
		// does not exist in real DX9... just for GL to know that this is an SRGB tex
		usage |= D3DUSAGE_TEXTURE_SRGB;
	}
#endif
	
	IDirect3DBaseTexture9* pBaseTexture = NULL;
	if ( isCubeMap )
	{
		IDirect3DCubeTexture9* pD3DCubeTexture = NULL;
		hr = Dx9Device()->CreateCubeTexture( 
				width,
				numLevels,
				usage,
				d3dFormat,
				pool, 
				&pD3DCubeTexture,
				NULL
#ifdef DX_TO_GL_ABSTRACTION
				, debugLabel					// tex create funcs take extra arg for debug name on GL
#endif
				);
		if ( SUCCEEDED( hr ) )
		{
			pBaseTexture = pD3DCubeTexture;
		}
		else
		{
#ifdef ENABLE_NULLREF_DEVICE_SUPPORT
			if ( CommandLine()->FindParm( "-nulldevice" ) )
			{
				Warning( "ShaderAPIDX8::CreateD3DTexture: Null device used. Texture not created.\n" );
				return nullptr;
			}
#endif
			Warning( __FUNCTION__ ": IDirect3DDevice9Ex::CreateCubeTexture(width = %d, levels = %d, usage = 0x%x, format = 0x%x, pool = 0x%x) failed w/e %s.\n",
				width,
				numLevels,
				usage,
				d3dFormat,
				pool,
				se::win::com::com_error_category().message(hr).c_str() );
			return nullptr;
		}
	}
	else if ( bVolumeTexture )
	{
		IDirect3DVolumeTexture9* pD3DVolumeTexture = NULL;
		hr = Dx9Device()->CreateVolumeTexture( 
				width, 
				height, 
				nDepth,
				numLevels, 
				usage, 
				d3dFormat, 
				pool, 
				&pD3DVolumeTexture,
				NULL
#if defined( DX_TO_GL_ABSTRACTION )			
				, debugLabel					// tex create funcs take extra arg for debug name on GL
#endif
				  );
		if ( SUCCEEDED( hr ) )
		{
			pBaseTexture = pD3DVolumeTexture;
		}
		else
		{
#ifdef ENABLE_NULLREF_DEVICE_SUPPORT
			if ( CommandLine()->FindParm( "-nulldevice" ) )
			{
				Warning( "ShaderAPIDX8::CreateD3DTexture: Null device used. Texture not created.\n" );
				return nullptr;
			}
#endif
			Warning( __FUNCTION__ ": IDirect3DDevice9Ex::CreateVolumeTexture(width = %d, height = %d, depth = %d, levels = %d, usage = 0x%x, format = 0x%x, pool = 0x%x) failed w/e %s.\n",
				width,
				height,
				nDepth,
				numLevels,
				usage,
				d3dFormat,
				pool,
				se::win::com::com_error_category().message(hr).c_str() );
			return nullptr;
		}
	}
	else
	{
		// Override usage and managed params if using special hardware shadow depth map formats...
		if ( ( d3dFormat == NVFMT_RAWZ ) || ( d3dFormat == NVFMT_INTZ   ) || 
		     ( d3dFormat == D3DFMT_D16 ) || ( d3dFormat == D3DFMT_D24S8 ) || 
			 ( d3dFormat == ATIFMT_D16 ) || ( d3dFormat == ATIFMT_D24S8 ) )
		{
			// Not putting D3DUSAGE_RENDERTARGET here causes D3D debug spew later, but putting the flag causes this create to fail...
			usage = D3DUSAGE_DEPTHSTENCIL;
			bManaged = false;
		}

		// Override managed param if using special null texture format
		if ( d3dFormat == NVFMT_NULL )
		{
			bManaged = false;
		}
		
		IDirect3DTexture9* pD3DTexture = NULL;
		hr = Dx9Device()->CreateTexture(
				width,
				height,
				numLevels,
				usage,
				d3dFormat,
				pool,
				&pD3DTexture,
				NULL
#if defined( DX_TO_GL_ABSTRACTION )			
				, debugLabel					// tex create funcs take extra arg for debug name on GL
#endif
				 );
		if ( SUCCEEDED( hr ) )
		{
			pBaseTexture = pD3DTexture;
		}
		else
		{
#ifdef ENABLE_NULLREF_DEVICE_SUPPORT
			if ( CommandLine()->FindParm( "-nulldevice" ) )
			{
				Warning( "ShaderAPIDX8::CreateD3DTexture: Null device used. Texture not created.\n" );
				return nullptr;
			}
#endif
			Warning( __FUNCTION__ ": IDirect3DDevice9Ex::CreateTexture(width = %d, height = %d, levels = %d, usage = 0x%x, format = 0x%x, pool = 0x%x) failed w/e %s.\n",
				width,
				height,
				numLevels,
				usage,
				d3dFormat,
				pool,
				se::win::com::com_error_category().message(hr).c_str() );
			return nullptr;
		}
	}

#ifdef MEASURE_DRIVER_ALLOCATIONS
	int nMipCount = numLevels;
	if ( !nMipCount )
	{
		while ( width > 1 || height > 1 )
		{
			width >>= 1;
			height >>= 1;
			++nMipCount;
		}
	}

	int nMemUsed = nMipCount * 1.1f * 1024;
	if ( isCubeMap )
	{
		nMemUsed *= 6;
	}

	VPROF_INCREMENT_GROUP_COUNTER( "texture count", COUNTER_GROUP_NO_RESET, 1 );
	VPROF_INCREMENT_GROUP_COUNTER( "texture driver mem", COUNTER_GROUP_NO_RESET, nMemUsed );
	VPROF_INCREMENT_GROUP_COUNTER( "total driver mem", COUNTER_GROUP_NO_RESET, nMemUsed );
#endif

	s_TextureCount.fetch_add(1, std::memory_order::memory_order_relaxed);

	return pBaseTexture;
}


//-----------------------------------------------------------------------------
// Texture destruction
//-----------------------------------------------------------------------------
static void ReleaseD3DTexture( IDirect3DBaseTexture* pD3DTex )
{
	ULONG ref = pD3DTex->Release();
	Assert( ref == 0 );
}

void DestroyD3DTexture( IDirect3DBaseTexture* pD3DTex )
{
	if ( pD3DTex )
	{
#ifdef MEASURE_DRIVER_ALLOCATIONS
		D3DRESOURCETYPE type = pD3DTex->GetType();
		int nMipCount = pD3DTex->GetLevelCount();
		if ( type == D3DRTYPE_CUBETEXTURE )
		{
			nMipCount *= 6;
		}
		int nMemUsed = nMipCount * 1.1f * 1024;
		VPROF_INCREMENT_GROUP_COUNTER( "texture count", COUNTER_GROUP_NO_RESET, -1 );
		VPROF_INCREMENT_GROUP_COUNTER( "texture driver mem", COUNTER_GROUP_NO_RESET, -nMemUsed );
		VPROF_INCREMENT_GROUP_COUNTER( "total driver mem", COUNTER_GROUP_NO_RESET, -nMemUsed );
#endif

		CMatRenderContextPtr pRenderContext( materials );
		if ( ICallQueue *pCallQueue = pRenderContext->GetCallQueue(); pCallQueue )
		{
			pCallQueue->QueueCall( ReleaseD3DTexture, pD3DTex );
		}
		else
		{
			ReleaseD3DTexture( pD3DTex );
		}

		s_TextureCount.fetch_sub(1, std::memory_order::memory_order_relaxed);
	}
}


//-----------------------------------------------------------------------------
// Lock, unlock a texture...
//-----------------------------------------------------------------------------

static RECT s_LockedSrcRect;
static D3DLOCKED_RECT s_LockedRect;
#ifdef DBGFLAG_ASSERT
static bool s_bInLock = false;
#endif

bool LockTexture( ShaderAPITextureHandle_t bindId, int copy, IDirect3DBaseTexture* pTexture, int level, 
	D3DCUBEMAP_FACES cubeFaceID, int xOffset, int yOffset, int width, int height, bool bDiscard,
	CPixelWriter& writer )
{
	Assert( !s_bInLock );
	
	se::win::com::com_ptr<IDirect3DSurface> pSurf;
	HRESULT hr = GetSurfaceFromTexture( pTexture, level, cubeFaceID, &pSurf );
	if ( FAILED( hr ) )
		return false;

	s_LockedSrcRect.left = xOffset;
	s_LockedSrcRect.right = xOffset + width;
	s_LockedSrcRect.top = yOffset;
	s_LockedSrcRect.bottom = yOffset + height;

	DWORD flags = D3DLOCK_NOSYSLOCK;
	flags |= bDiscard ? D3DLOCK_DISCARD : 0;
	RECORD_COMMAND( DX8_LOCK_TEXTURE, 6 );
	RECORD_INT( bindId );
	RECORD_INT( copy );
	RECORD_INT( level );
	RECORD_INT( cubeFaceID );
	RECORD_STRUCT( &s_LockedSrcRect, sizeof(s_LockedSrcRect) );
	RECORD_INT( flags );

	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "D3DLockTexture" );

	hr = pSurf->LockRect( &s_LockedRect, &s_LockedSrcRect, flags );
	if ( FAILED( hr ) )
		return false;

	writer.SetPixelMemory( GetImageFormat(pTexture), s_LockedRect.pBits, s_LockedRect.Pitch );

#ifdef DBGFLAG_ASSERT
	s_bInLock = true;
#endif
	return true;
}

void UnlockTexture( ShaderAPITextureHandle_t bindId, int copy, IDirect3DBaseTexture* pTexture, int level, 
	D3DCUBEMAP_FACES cubeFaceID )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	Assert( s_bInLock );
	
	se::win::com::com_ptr<IDirect3DSurface> pSurf;
	HRESULT hr = GetSurfaceFromTexture( pTexture, level, cubeFaceID, &pSurf );
	if (FAILED(hr))
		return;

#ifdef RECORD_TEXTURES 
	int width = s_LockedSrcRect.right - s_LockedSrcRect.left;
	int height = s_LockedSrcRect.bottom - s_LockedSrcRect.top;
	int imageFormatSize = ImageLoader::SizeInBytes( GetImageFormat( pTexture ) );
	Assert( imageFormatSize != 0 );
	int validDataBytesPerRow = imageFormatSize * width;
	int storeSize = validDataBytesPerRow * height;
	static CUtlVector< unsigned char > tmpMem;
	if( tmpMem.Size() < storeSize )
	{
		tmpMem.AddMultipleToTail( storeSize - tmpMem.Size() );
	}
	unsigned char *pDst = tmpMem.Base();
	unsigned char *pSrc = ( unsigned char * )s_LockedRect.pBits;
	RECORD_COMMAND( DX8_SET_TEXTURE_DATA, 3 );
	RECORD_INT( validDataBytesPerRow );
	RECORD_INT( height );
	int i;
	for( i = 0; i < height; i++ )
	{
		memcpy( pDst, pSrc, validDataBytesPerRow );
		pDst += validDataBytesPerRow;
		pSrc += s_LockedRect.Pitch;
	}
	RECORD_STRUCT( tmpMem.Base(), storeSize );
#endif // RECORD_TEXTURES 
	
	RECORD_COMMAND( DX8_UNLOCK_TEXTURE, 4 );
	RECORD_INT( bindId );
	RECORD_INT( copy );
	RECORD_INT( level );
	RECORD_INT( cubeFaceID );

	hr = pSurf->UnlockRect();

#ifdef DBGFLAG_ASSERT
	s_bInLock = false;
#endif
}

//-----------------------------------------------------------------------------
// Compute texture size based on compression
//-----------------------------------------------------------------------------

constexpr inline int DeterminePowerOfTwo( int val )
{
	int pow = 0;
	while ((val & 0x1) == 0x0)
	{
		val >>= 1;
		++pow;
	}

	return pow;
}


//-----------------------------------------------------------------------------
// Blit in bits
//-----------------------------------------------------------------------------
// NOTE: IF YOU CHANGE THIS, CHANGE THE VERSION IN PLAYBACK.CPP!!!!
// OPTIMIZE??: could lock the texture directly instead of the surface in dx9.
static void BlitSurfaceBits( TextureLoadInfo_t &info, int xOffset, int yOffset, int srcStride )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	// Get the level of the texture we want to write into
	se::win::com::com_ptr<IDirect3DSurface> pTextureLevel;
	HRESULT hr = GetSurfaceFromTexture( info.m_pTexture, info.m_nLevel, info.m_CubeFaceID, &pTextureLevel );
	if ( FAILED( hr ) )
		return;

	RECT			srcRect;
	D3DLOCKED_RECT	lockedRect;

	srcRect.left   = xOffset;
	srcRect.right  = xOffset + info.m_nWidth;
	srcRect.top    = yOffset;
	srcRect.bottom = yOffset + info.m_nHeight;

#if defined( SHADERAPIDX9 ) && !defined( DX_TO_GL_ABSTRACTION )
	if ( !info.m_bTextureIsLockable )
	{
		// Copy from system memory to video memory using D3D9Device->UpdateSurface
		bool bSuccess = false;

		D3DSURFACE_DESC desc;
		Verify( pTextureLevel->GetDesc( &desc ) == S_OK );
		ImageFormat dstFormat = ImageLoader::D3DFormatToImageFormat( desc.Format );
		D3DFORMAT dstFormatD3D = ImageLoader::ImageFormatToD3DFormat( dstFormat );

		se::win::com::com_ptr<IDirect3DSurface> pSrcSurface;
		bool bCopyBitsToSrcSurface = true;

#if defined(IS_WINDOWS_PC) && defined(SHADERAPIDX9)
		// D3D9Ex fast path: create a texture wrapping our own system memory buffer
		// if the source and destination formats are exactly the same and the stride
		// is tightly packed. no locking/blitting required.
		// NOTE: the fast path does not work on sub-4x4 DXT compressed textures.
		if ( 
			( info.m_SrcFormat == dstFormat || ( info.m_SrcFormat == IMAGE_FORMAT_DXT1_ONEBITALPHA && dstFormat == IMAGE_FORMAT_DXT1 ) ) &&
			( !ImageLoader::IsCompressed( dstFormat ) || (info.m_nWidth >= 4 || info.m_nHeight >= 4) ) )
		{
			if ( srcStride == 0 || srcStride == info.m_nWidth * ImageLoader::SizeInBytes( info.m_SrcFormat ) )
			{
				se::win::com::com_ptr<IDirect3DTexture9> pTempTex;
				if ( Dx9Device()->CreateTexture( info.m_nWidth, info.m_nHeight, 1, 0, dstFormatD3D, D3DPOOL_SYSTEMMEM, &pTempTex, (HANDLE*) &info.m_pSrcData ) == S_OK )
				{
					if ( pTempTex->GetSurfaceLevel( 0, &pSrcSurface ) == S_OK )
					{
						bCopyBitsToSrcSurface = false;
					}
				}
			}
		}
#endif

		// If possible to create a texture of this size, create a temporary texture in
		// system memory and then use the UpdateSurface method to copy between textures.
		if ( !pSrcSurface && ( g_pHardwareConfig->Caps().m_SupportsNonPow2Textures ||
							( IsPowerOfTwo( info.m_nWidth ) && IsPowerOfTwo( info.m_nHeight ) ) ) )
		{
			int tempW = info.m_nWidth, tempH = info.m_nHeight, mip = 0;
			if ( info.m_nLevel > 0 && ( ( tempW | tempH ) & 3 ) && ImageLoader::IsCompressed( dstFormat ) )
			{
				// Loading lower mip levels of DXT compressed textures is sort of tricky
				// because we can't create textures that aren't multiples of 4, and we can't
				// pass subrectangles of DXT textures into UpdateSurface. Create a temporary
				// texture which is 1 or 2 mip levels larger and then lock the appropriate
				// mip level to grab its correctly-dimensioned surface. -henryg 11/18/2011
				mip = ( info.m_nLevel > 1 && ( ( tempW | tempH ) & 1 ) ) ? 2 : 1;
				tempW <<= mip;
				tempH <<= mip;
			}

			se::win::com::com_ptr<IDirect3DTexture9> pTempTex;
			if ( Dx9Device()->CreateTexture( tempW, tempH, mip+1, 0, dstFormatD3D, D3DPOOL_SYSTEMMEM, &pTempTex, NULL ) == S_OK )
			{
				if ( pTempTex->GetSurfaceLevel( mip, &pSrcSurface ) == S_OK )
				{
					bCopyBitsToSrcSurface = true;
				}
			}
		}

		// Create an offscreen surface if the texture path wasn't an option.
		if ( !pSrcSurface )
		{
			if ( Dx9Device()->CreateOffscreenPlainSurface( info.m_nWidth, info.m_nHeight, dstFormatD3D, D3DPOOL_SYSTEMMEM, &pSrcSurface, NULL ) == S_OK )
			{
				bCopyBitsToSrcSurface = true;
			}
		}

		// Lock and fill the surface
		if ( bCopyBitsToSrcSurface && pSrcSurface )
		{
			if ( pSrcSurface->LockRect( &lockedRect, NULL, D3DLOCK_NOSYSLOCK ) == S_OK )
			{
				unsigned char *pImage = (unsigned char *)lockedRect.pBits;
				ShaderUtil()->ConvertImageFormat( info.m_pSrcData, info.m_SrcFormat,
					pImage, dstFormat, info.m_nWidth, info.m_nHeight, srcStride, lockedRect.Pitch );
				pSrcSurface->UnlockRect();
			}
			else
			{
				// Lock failed.
				pSrcSurface.Release();
			}
		}
	
		// Perform the UpdateSurface call that blits between system and video memory
		if ( pSrcSurface )
		{
			POINT pt = { xOffset, yOffset };
			bSuccess = ( Dx9Device()->UpdateSurface( pSrcSurface, NULL, pTextureLevel, &pt ) == S_OK );
		}
		
		if ( !bSuccess )
		{
			Warning( "CShaderAPIDX8::BlitTextureBits: couldn't lock texture rect or use UpdateSurface\n" );
		}

		return;
	}
#endif

	Assert( info.m_bTextureIsLockable );

#ifndef RECORD_TEXTURES
	RECORD_COMMAND( DX8_LOCK_TEXTURE, 6 );
	RECORD_INT( info.m_TextureHandle );
	RECORD_INT( info.m_nCopy );
	RECORD_INT( info.m_nLevel );
	RECORD_INT( info.m_CubeFaceID );
	RECORD_STRUCT( &srcRect, sizeof(srcRect) );
	RECORD_INT( D3DLOCK_NOSYSLOCK );
#endif

	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - D3DLockRect", __FUNCTION__ );

		// lock the region (could be the full surface or less)
		if ( FAILED( pTextureLevel->LockRect( &lockedRect, &srcRect, D3DLOCK_NOSYSLOCK ) ) )
		{
			Warning( "CShaderAPIDX8::BlitTextureBits: couldn't lock texture rect\n" );
			return;
		}
	}

	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - ConvertImageFormat", __FUNCTION__ );

		// garymcthack : need to make a recording command for this.
		ImageFormat dstFormat = GetImageFormat( info.m_pTexture );
		unsigned char *pImage = (unsigned char *)lockedRect.pBits;
		ShaderUtil()->ConvertImageFormat( info.m_pSrcData, info.m_SrcFormat,
							pImage, dstFormat, info.m_nWidth, info.m_nHeight, srcStride, lockedRect.Pitch );
	}

#ifndef RECORD_TEXTURES
	RECORD_COMMAND( DX8_UNLOCK_TEXTURE, 4 );
	RECORD_INT( info.m_TextureHandle );
	RECORD_INT( info.m_nCopy );
	RECORD_INT( info.m_nLevel );
	RECORD_INT( info.m_CubeFaceID );
#endif

	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - UnlockRect", __FUNCTION__ );

		if ( FAILED( pTextureLevel->UnlockRect() ) ) 
		{
			Warning( "CShaderAPIDX8::BlitTextureBits: couldn't unlock texture rect\n" );
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Blit in bits
//-----------------------------------------------------------------------------
static void BlitVolumeBits( TextureLoadInfo_t &info, int xOffset, int yOffset, int srcStride )
{
	D3DBOX srcBox;
	D3DLOCKED_BOX lockedBox;
	srcBox.Left = xOffset;
	srcBox.Right = xOffset + info.m_nWidth;
	srcBox.Top = yOffset;
	srcBox.Bottom = yOffset + info.m_nHeight;
	srcBox.Front = info.m_nZOffset;
	srcBox.Back = info.m_nZOffset + 1;

#ifndef RECORD_TEXTURES
	RECORD_COMMAND( DX8_LOCK_TEXTURE, 6 );
	RECORD_INT( info.m_TextureHandle );
	RECORD_INT( info.m_nCopy );
	RECORD_INT( info.m_nLevel );
	RECORD_INT( info.m_CubeFaceID );
	RECORD_STRUCT( &srcRect, sizeof(srcRect) );
	RECORD_INT( D3DLOCK_NOSYSLOCK );
#endif

	IDirect3DVolumeTexture *pVolumeTexture = static_cast<IDirect3DVolumeTexture*>( info.m_pTexture );
	if ( FAILED( pVolumeTexture->LockBox( info.m_nLevel, &lockedBox, &srcBox, D3DLOCK_NOSYSLOCK ) ) )
	{
		Warning( "BlitVolumeBits: couldn't lock volume texture rect\n" );
		return;
	}

	// garymcthack : need to make a recording command for this.
	ImageFormat dstFormat = GetImageFormat( info.m_pTexture );
	unsigned char *pImage = (unsigned char *)lockedBox.pBits;
	ShaderUtil()->ConvertImageFormat( info.m_pSrcData, info.m_SrcFormat,
						pImage, dstFormat, info.m_nWidth, info.m_nHeight, srcStride, lockedBox.RowPitch );

#ifndef RECORD_TEXTURES
	RECORD_COMMAND( DX8_UNLOCK_TEXTURE, 4 );
	RECORD_INT( info.m_TextureHandle );
	RECORD_INT( info.m_nCopy );
	RECORD_INT( info.m_nLevel );
	RECORD_INT( info.m_CubeFaceID );
#endif

	if ( FAILED( pVolumeTexture->UnlockBox( info.m_nLevel ) ) ) 
	{
		Warning( "BlitVolumeBits: couldn't unlock volume texture rect\n" );
		return;
	}
}

// FIXME: How do I blit from D3DPOOL_SYSTEMMEM to D3DPOOL_MANAGED?  I used to use CopyRects for this.  UpdateSurface doesn't work because it can't blit to anything besides D3DPOOL_DEFAULT.
// We use this only in the case where we need to create a < 4x4 miplevel for a compressed texture.  We end up creating a 4x4 system memory texture, and blitting it into the proper miplevel.
// 6) LockRects should be used for copying between SYSTEMMEM and
// MANAGED.  For such a small copy, you'd avoid a significant 
// amount of overhead from the old CopyRects code.  Ideally, you 
// should just lock the bottom of MANAGED and generate your 
// sub-4x4 data there.
	
// NOTE: IF YOU CHANGE THIS, CHANGE THE VERSION IN PLAYBACK.CPP!!!!
static void BlitTextureBits( TextureLoadInfo_t &info, int xOffset, int yOffset, int srcStride )
{
#ifdef RECORD_TEXTURES
	RECORD_COMMAND( DX8_BLIT_TEXTURE_BITS, 14 );
	RECORD_INT( info.m_TextureHandle );
	RECORD_INT( info.m_nCopy );
	RECORD_INT( info.m_nLevel );
	RECORD_INT( info.m_CubeFaceID );
	RECORD_INT( xOffset );
	RECORD_INT( yOffset );
	RECORD_INT( info.m_nZOffset );
	RECORD_INT( info.m_nWidth );
	RECORD_INT( info.m_nHeight );
	RECORD_INT( info.m_SrcFormat );
	RECORD_INT( srcStride );
	RECORD_INT( GetImageFormat( info.m_pTexture ) );
	// strides are in bytes.
	intp srcDataSize;
	if ( srcStride == 0 )
	{
		srcDataSize = ImageLoader::GetMemRequired( info.m_nWidth, info.m_nHeight, 1, info.m_SrcFormat, false );
	}
	else
	{
		srcDataSize = srcStride * info.m_nHeight;
	}
	RECORD_INT( srcDataSize );
	RECORD_STRUCT( info.m_pSrcData, srcDataSize );
#endif // RECORD_TEXTURES
	
	if ( !IsVolumeTexture( info.m_pTexture ) )
	{
		Assert( info.m_nZOffset == 0 );
		BlitSurfaceBits( info, xOffset, yOffset, srcStride );
	}
	else
	{
		BlitVolumeBits( info, xOffset, yOffset, srcStride );
	}
}

//-----------------------------------------------------------------------------
// Texture image upload
//-----------------------------------------------------------------------------
void LoadTexture( TextureLoadInfo_t &info )
{
	MEM_ALLOC_D3D_CREDIT();

	Assert( info.m_pSrcData );
	Assert( info.m_pTexture );

#ifdef _DEBUG
	ImageFormat format = GetImageFormat( info.m_pTexture );
	Assert( (format != -1) && (format == FindNearestSupportedFormat( format, false, false, false )) );
#endif

	// Copy in the bits...
	BlitTextureBits( info, 0, 0, 0 );
}

void LoadVolumeTextureFromVTF( TextureLoadInfo_t &info, IVTFTexture* pVTF, int iVTFFrame )
{
	if ( !info.m_pTexture || info.m_pTexture->GetType() != D3DRTYPE_VOLUMETEXTURE )
	{
		Assert( 0 );
		return;
	}

	IDirect3DVolumeTexture9 *pVolTex = static_cast<IDirect3DVolumeTexture*>( info.m_pTexture );

	D3DVOLUME_DESC desc;
	if ( pVolTex->GetLevelDesc( 0, &desc ) != S_OK )
	{
		Warning( "LoadVolumeTextureFromVTF: couldn't get texture level description\n" );
		return;
	}

	int iMipCount = pVolTex->GetLevelCount();
	if ( pVTF->Depth() != (int)desc.Depth || pVTF->Width() != (int)desc.Width || pVTF->Height() != (int)desc.Height || pVTF->MipCount() < iMipCount )
	{
		Warning( "LoadVolumeTextureFromVTF: VTF dimensions do not match texture\n" );
		return;
	}

	TextureLoadInfo_t sliceInfo = info;

#if !defined( DX_TO_GL_ABSTRACTION )
	se::win::com::com_ptr<IDirect3DVolumeTexture9> pStagingTexture;
	if ( !info.m_bTextureIsLockable )
	{
		if ( Dx9Device()->CreateVolumeTexture( desc.Width, desc.Height, desc.Depth, iMipCount, 0, desc.Format, D3DPOOL_SYSTEMMEM, &pStagingTexture, NULL ) != S_OK )
		{
			Warning( "LoadVolumeTextureFromVTF: failed to create temporary staging texture\n" );
			return;
		}
		sliceInfo.m_pTexture = static_cast<IDirect3DBaseTexture*>( pStagingTexture );
		sliceInfo.m_bTextureIsLockable = true;
	}
#endif

	for ( int iMip = 0; iMip < iMipCount; ++iMip )
	{
		int w, h, d;
		pVTF->ComputeMipLevelDimensions( iMip, &w, &h, &d );
		sliceInfo.m_nLevel = iMip;
		sliceInfo.m_nWidth = w;
		sliceInfo.m_nHeight = h;
		for ( int iSlice = 0; iSlice < d; ++iSlice )
		{
			sliceInfo.m_nZOffset = iSlice;
			sliceInfo.m_pSrcData = pVTF->ImageData( iVTFFrame, 0, iMip, 0, 0, iSlice );
			BlitTextureBits( sliceInfo, 0, 0, 0 );
		}
	}

#if !defined( DX_TO_GL_ABSTRACTION )
	if ( pStagingTexture )
	{
		if ( Dx9Device()->UpdateTexture( pStagingTexture, pVolTex ) != S_OK )
		{
			Warning( "LoadVolumeTextureFromVTF: volume UpdateTexture failed\n" );
		}
	}
#endif
}

void LoadCubeTextureFromVTF( TextureLoadInfo_t &info, IVTFTexture* pVTF, int iVTFFrame )
{
	if ( !info.m_pTexture || info.m_pTexture->GetType() != D3DRTYPE_CUBETEXTURE )
	{
		Assert( 0 );
		return;
	}

	IDirect3DCubeTexture9 *pCubeTex = static_cast<IDirect3DCubeTexture9*>( info.m_pTexture );

	D3DSURFACE_DESC desc;
	if ( pCubeTex->GetLevelDesc( 0, &desc ) != S_OK )
	{
		Warning( "LoadCubeTextureFromVTF: couldn't get texture level description\n" );
		return;
	}

	int iMipCount = pCubeTex->GetLevelCount();
	if ( pVTF->Depth() != 1 || pVTF->Width() != (int)desc.Width || pVTF->Height() != (int)desc.Height || pVTF->FaceCount() < 6 || pVTF->MipCount() < iMipCount )
	{
		Warning( "LoadCubeTextureFromVTF: VTF dimensions do not match texture\n" );
		return;
	}

	TextureLoadInfo_t faceInfo = info;

#if !defined( DX_TO_GL_ABSTRACTION )
	se::win::com::com_ptr<IDirect3DCubeTexture9> pStagingTexture;
	if ( !info.m_bTextureIsLockable )
	{
		if ( Dx9Device()->CreateCubeTexture( desc.Width, iMipCount, 0, desc.Format, D3DPOOL_SYSTEMMEM, &pStagingTexture, NULL ) != S_OK )
		{
			Warning( "LoadCubeTextureFromVTF: failed to create temporary staging texture\n" );
			return;
		}

		faceInfo.m_pTexture = static_cast<IDirect3DBaseTexture*>( pStagingTexture );
		faceInfo.m_bTextureIsLockable = true;
	}
#endif

	for ( int iMip = 0; iMip < iMipCount; ++iMip )
	{
		int w, h, d;
		pVTF->ComputeMipLevelDimensions( iMip, &w, &h, &d );
		faceInfo.m_nLevel = iMip;
		faceInfo.m_nWidth = w;
		faceInfo.m_nHeight = h;
		for ( int iFace = 0; iFace < 6; ++iFace )
		{
			faceInfo.m_CubeFaceID = (D3DCUBEMAP_FACES) iFace;
			faceInfo.m_pSrcData = pVTF->ImageData( iVTFFrame, iFace, iMip );
			BlitTextureBits( faceInfo, 0, 0, 0 );
		}
	}

#if !defined( DX_TO_GL_ABSTRACTION )
	if ( pStagingTexture )
	{
		if ( Dx9Device()->UpdateTexture( pStagingTexture, pCubeTex ) != S_OK )
		{
			Warning( "LoadCubeTextureFromVTF: cube UpdateTexture failed\n" );
		}
	}
#endif
}

void LoadTextureFromVTF( TextureLoadInfo_t &info, IVTFTexture* pVTF, int iVTFFrame )
{
	TM_ZONE_DEFAULT( TELEMETRY_LEVEL0 );

	if ( !info.m_pTexture || info.m_pTexture->GetType() != D3DRTYPE_TEXTURE )
	{
		Assert( 0 );
		return;
	}

	IDirect3DTexture9 *pTex = static_cast<IDirect3DTexture9*>( info.m_pTexture );

	D3DSURFACE_DESC desc;
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - GetLevelDesc", __FUNCTION__ );
		if ( pTex->GetLevelDesc( 0, &desc ) != S_OK )
		{
			Warning( "LoadTextureFromVTF: couldn't get texture level description\n" );
			return;
		}
	}

	int iMipCount = pTex->GetLevelCount();
	if ( pVTF->Depth() != 1 || pVTF->Width() != (int)desc.Width || pVTF->Height() != (int)desc.Height || pVTF->MipCount() < iMipCount || pVTF->FaceCount() <= (int)info.m_CubeFaceID )
	{
		Warning( "LoadTextureFromVTF: VTF dimensions do not match texture\n" );
		return;
	}

	// Info may have a cube face ID if we are falling back to 2D sphere map support
	TextureLoadInfo_t mipInfo = info;
	int iVTFFaceNum = info.m_CubeFaceID;
	mipInfo.m_CubeFaceID = D3DCUBEMAP_FACE_POSITIVE_X;

#if !defined( DX_TO_GL_ABSTRACTION )
	// If blitting more than one mip level of an unlockable texture, create a temporary
	// texture for all mip levels only call UpdateTexture once. For textures with
	// only a single mip level, fall back on the support in BlitSurfaceBits. -henryg
	se::win::com::com_ptr<IDirect3DTexture9> pStagingTexture;
	if ( !info.m_bTextureIsLockable && iMipCount > 1 )
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - CreateSysmemTexture", __FUNCTION__ );
		
		if ( Dx9Device()->CreateTexture( desc.Width, desc.Height, iMipCount, 0, desc.Format, D3DPOOL_SYSTEMMEM, &pStagingTexture, NULL ) != S_OK )
		{
			Warning( "LoadTextureFromVTF: failed to create temporary staging texture\n" );
			return;
		}

		mipInfo.m_pTexture = static_cast<IDirect3DBaseTexture*>( pStagingTexture );
		mipInfo.m_bTextureIsLockable = true;
	}
#endif

	// Get the clamped resolutions from the VTF, then apply any clamping we've done from the higher level code.
	// (For example, we chop off the bottom of the mipmap pyramid at 32x32--that is reflected in iMipCount, so 
	// honor that here).
	int finest = 0, coarsest = 0;
	pVTF->GetMipmapRange( &finest, &coarsest );
	finest = Min( finest, iMipCount - 1 );
	coarsest = Min( coarsest, iMipCount - 1 );
	Assert( finest <= coarsest && coarsest < iMipCount );

	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - BlitTextureBits", __FUNCTION__ );
		for ( int iMip = finest; iMip <= coarsest; ++iMip )
		{
			int w, h, d;
			pVTF->ComputeMipLevelDimensions( iMip, &w, &h, &d );
			mipInfo.m_nLevel = iMip;
			mipInfo.m_nWidth = w;
			mipInfo.m_nHeight = h;
			mipInfo.m_pSrcData = pVTF->ImageData( iVTFFrame, iVTFFaceNum, iMip );

			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - BlitTextureBits - %d", __FUNCTION__, iMip );

			BlitTextureBits( mipInfo, 0, 0, 0 );
		}
	}

#if !defined( DX_TO_GL_ABSTRACTION )
	if ( pStagingTexture )
	{
		if ( ( coarsest - finest + 1 ) == iMipCount )
		{
			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - UpdateTexture", __FUNCTION__ );
			if ( Dx9Device()->UpdateTexture( pStagingTexture, pTex ) != S_OK )
			{
				Warning( "LoadTextureFromVTF: UpdateTexture failed\n" );
			}
		}
		else
		{
			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - UpdateSurface", __FUNCTION__ );

			for ( int mip = finest; mip <= coarsest; ++mip )
			{
				tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - UpdateSurface - %d", __FUNCTION__, mip );

				se::win::com::com_ptr<IDirect3DSurface9> pSrcSurf;
				if ( pStagingTexture->GetSurfaceLevel( mip, &pSrcSurf ) != S_OK )
					Warning( "LoadTextureFromVTF: couldn't get surface level %d for system surface\n", mip );
				
				se::win::com::com_ptr<IDirect3DSurface9> pDstSurf;
				if ( pTex->GetSurfaceLevel( mip, &pDstSurf ) != S_OK )
					Warning( "LoadTextureFromVTF: couldn't get surface level %d for dest surface\n", mip );
				{
					tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - UpdateSurface - Call ", __FUNCTION__, mip );
					if ( !pSrcSurf || !pDstSurf || Dx9Device()->UpdateSurface( pSrcSurf, NULL, pDstSurf, NULL ) != S_OK ) 
						Warning( "LoadTextureFromVTF: surface update failed.\n" );
				}
			}
		}

		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - Cleanup", __FUNCTION__ );
	}
#endif
}


//-----------------------------------------------------------------------------
// Upload to a sub-piece of a texture
//-----------------------------------------------------------------------------
void LoadSubTexture( TextureLoadInfo_t &info, int xOffset, int yOffset, int srcStride )
{
	Assert( info.m_pSrcData );
	Assert( info.m_pTexture );

#ifdef _DEBUG
	ImageFormat format = GetImageFormat( info.m_pTexture );
	Assert( (format == FindNearestSupportedFormat(format, false, false, false )) && (format != -1) );
#endif

	// Copy in the bits...
	BlitTextureBits( info, xOffset, yOffset, srcStride );
}
