//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#if defined( _WIN32 ) && !defined( DX_TO_GL_ABSTRACTION )
#include "winlite.h"
#include <d3d9types.h>
#endif

#include "bitmap/imageformat.h"
#include "tier0/platform.h"
#include "tier0/dbg.h"
// dimhotepus: Exclude nvtc as proprietary.
#ifndef NO_NVTC
#include "nvtc.h"
#endif

// Should be last include
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Various important function types for each color format
//-----------------------------------------------------------------------------
static const ImageFormatInfo_t g_ImageFormatInfo[] =
{
	{ "UNKNOWN",					0, 0, 0, 0, 0, false },			// IMAGE_FORMAT_UNKNOWN,
	{ "RGBA8888",					4, 8, 8, 8, 8, false },			// IMAGE_FORMAT_RGBA8888,
	{ "ABGR8888",					4, 8, 8, 8, 8, false },			// IMAGE_FORMAT_ABGR8888, 
	{ "RGB888",						3, 8, 8, 8, 0, false },			// IMAGE_FORMAT_RGB888,
	{ "BGR888",						3, 8, 8, 8, 0, false },			// IMAGE_FORMAT_BGR888,
	{ "RGB565",						2, 5, 6, 5, 0, false },			// IMAGE_FORMAT_RGB565, 
	{ "I8",							1, 0, 0, 0, 0, false },			// IMAGE_FORMAT_I8,
	{ "IA88",						2, 0, 0, 0, 8, false },			// IMAGE_FORMAT_IA88
	{ "P8",							1, 0, 0, 0, 0, false },			// IMAGE_FORMAT_P8
	{ "A8",							1, 0, 0, 0, 8, false },			// IMAGE_FORMAT_A8
	{ "RGB888_BLUESCREEN",			3, 8, 8, 8, 0, false },			// IMAGE_FORMAT_RGB888_BLUESCREEN
	{ "BGR888_BLUESCREEN",			3, 8, 8, 8, 0, false },			// IMAGE_FORMAT_BGR888_BLUESCREEN
	{ "ARGB8888",					4, 8, 8, 8, 8, false },			// IMAGE_FORMAT_ARGB8888
	{ "BGRA8888",					4, 8, 8, 8, 8, false },			// IMAGE_FORMAT_BGRA8888
	{ "DXT1",						0, 0, 0, 0, 0, true },			// IMAGE_FORMAT_DXT1
	{ "DXT3",						0, 0, 0, 0, 8, true },			// IMAGE_FORMAT_DXT3
	{ "DXT5",						0, 0, 0, 0, 8, true },			// IMAGE_FORMAT_DXT5
	{ "BGRX8888",					4, 8, 8, 8, 0, false },			// IMAGE_FORMAT_BGRX8888
	{ "BGR565",						2, 5, 6, 5, 0, false },			// IMAGE_FORMAT_BGR565
	{ "BGRX5551",					2, 5, 5, 5, 0, false },			// IMAGE_FORMAT_BGRX5551
	{ "BGRA4444",					2, 4, 4, 4, 4, false },			// IMAGE_FORMAT_BGRA4444
	{ "DXT1_ONEBITALPHA",			0, 0, 0, 0, 0, true },			// IMAGE_FORMAT_DXT1_ONEBITALPHA
	{ "BGRA5551",					2, 5, 5, 5, 1, false },			// IMAGE_FORMAT_BGRA5551
	{ "UV88",						2, 8, 8, 0, 0, false },			// IMAGE_FORMAT_UV88
	{ "UVWQ8888",					4, 8, 8, 8, 8, false },			// IMAGE_FORMAT_UVWQ8899
	{ "RGBA16161616F",				8, 16, 16, 16, 16, false },		// IMAGE_FORMAT_RGBA16161616F
	{ "RGBA16161616",				8, 16, 16, 16, 16, false },		// IMAGE_FORMAT_RGBA16161616
	{ "IMAGE_FORMAT_UVLX8888",	    4, 8, 8, 8, 8, false },			// IMAGE_FORMAT_UVLX8899
	{ "IMAGE_FORMAT_R32F",			4, 32, 0, 0, 0, false },		// IMAGE_FORMAT_R32F
	{ "IMAGE_FORMAT_RGB323232F",	12, 32, 32, 32, 0, false },		// IMAGE_FORMAT_RGB323232F
	{ "IMAGE_FORMAT_RGBA32323232F",	16, 32, 32, 32, 32, false },	// IMAGE_FORMAT_RGBA32323232F

	// Vendor-dependent depth formats used for shadow depth mapping
	{ "NV_DST16",					2, 16, 0, 0, 0, false },		// IMAGE_FORMAT_NV_DST16
	{ "NV_DST24",					4, 24, 0, 0, 0, false },		// IMAGE_FORMAT_NV_DST24
	{ "NV_INTZ",					4,  8, 8, 8, 8, false },		// IMAGE_FORMAT_NV_INTZ
	{ "NV_RAWZ",					4, 24, 0, 0, 0, false },		// IMAGE_FORMAT_NV_RAWZ
	{ "ATI_DST16",					2, 16, 0, 0, 0, false },		// IMAGE_FORMAT_ATI_DST16
	{ "ATI_DST24",					4, 24, 0, 0, 0, false },		// IMAGE_FORMAT_ATI_DST24
	{ "NV_NULL",					4,  8, 8, 8, 8, false },		// IMAGE_FORMAT_NV_NULL

	// Vendor-dependent compressed formats typically used for normal map compression
	{ "ATI1N",						0, 0, 0, 0, 0, true },			// IMAGE_FORMAT_ATI1N
	{ "ATI2N",						0, 0, 0, 0, 0, true },			// IMAGE_FORMAT_ATI2N

	{ "DXT1_RUNTIME",				0, 0, 0, 0, 0, true, },			// IMAGE_FORMAT_DXT1_RUNTIME
	{ "DXT5_RUNTIME",				0, 0, 0, 0, 8, true, },			// IMAGE_FORMAT_DXT5_RUNTIME
};


namespace ImageLoader
{

//-----------------------------------------------------------------------------
// Returns info about each image format
//-----------------------------------------------------------------------------
const ImageFormatInfo_t& ImageFormatInfo( ImageFormat fmt )
{
	static_assert( ( NUM_IMAGE_FORMATS + 1 ) == std::size( g_ImageFormatInfo ) );
	Assert( unsigned( fmt + 1 ) <= ( NUM_IMAGE_FORMATS ) );
	return g_ImageFormatInfo[ fmt + 1 ];
}

ptrdiff_t GetMemRequired( int width, int height, int depth, ImageFormat imageFormat, bool mipmap )
{
	if ( depth <= 0 )
	{
		depth = 1;
	}

	if ( !mipmap )
	{
		// Block compressed formats
		
		if ( IsCompressed( imageFormat ) )
		{
			Assert( ( width < 4 ) || !( width % 4 ) );
			Assert( ( height < 4 ) || !( height % 4 ) );
			Assert( ( depth < 4 ) || !( depth % 4 ) );
			if ( width < 4 && width > 0 )
			{
				width = 4;
			}
			if ( height < 4 && height > 0 )
			{
				height = 4;
			}
			if ( depth < 4 && depth > 1 )
			{
				depth = 4;
			}
			ptrdiff_t numBlocks = ( static_cast<ptrdiff_t>(width) * height ) >> 4;
			numBlocks *= depth;
			switch ( imageFormat )
			{
			case IMAGE_FORMAT_DXT1:
			case IMAGE_FORMAT_DXT1_RUNTIME:
			case IMAGE_FORMAT_ATI1N:
				return numBlocks * 8;

			case IMAGE_FORMAT_DXT3:
			case IMAGE_FORMAT_DXT5:
			case IMAGE_FORMAT_DXT5_RUNTIME:
			case IMAGE_FORMAT_ATI2N:
				return numBlocks * 16;
			}

			Assert( 0 );
			return 0;
		}

		return static_cast<ptrdiff_t>(width) * height * depth * SizeInBytes( imageFormat );
	}

	// Mipmap version
	ptrdiff_t memSize = 0;
	while ( 1 )
	{
		memSize += GetMemRequired( width, height, depth, imageFormat, false );
		if ( width == 1 && height == 1 && depth == 1 )
		{
			break;
		}
		width >>= 1;
		height >>= 1;
		depth >>= 1;
		if ( width < 1 )
		{
			width = 1;
		}
		if ( height < 1 )
		{
			height = 1;
		}
		if ( depth < 1 )
		{
			depth = 1;
		}
	}

	return memSize;
}

ptrdiff_t GetMipMapLevelByteOffset( int width, int height, ImageFormat imageFormat, int skipMipLevels )
{
	ptrdiff_t offset = 0;

	while( skipMipLevels > 0 )
	{
		offset += static_cast<ptrdiff_t>(width) * height * SizeInBytes(imageFormat);
		if( width == 1 && height == 1 )
		{
			break;
		}
		width >>= 1;
		height >>= 1;
		if( width < 1 )
		{
			width = 1;
		}
		if( height < 1 )
		{
			height = 1;
		}
		skipMipLevels--;
	}
	return offset;
}

void GetMipMapLevelDimensions( int *width, int *height, int skipMipLevels )
{
	while( skipMipLevels > 0 )
	{
		if( *width == 1 && *height == 1 )
		{
			break;
		}
		*width >>= 1;
		*height >>= 1;
		if( *width < 1 )
		{
			*width = 1;
		}
		if( *height < 1 )
		{
			*height = 1;
		}
		skipMipLevels--;
	}
}

int GetNumMipMapLevels( int width, int height, int depth )
{
	if ( depth <= 0 )
	{
		depth = 1;
	}

	if( width < 1 || height < 1 )
		return 0;

	int numMipLevels = 1;
	while( 1 )
	{
		if( width == 1 && height == 1 && depth == 1 )
			break;

		width >>= 1;
		height >>= 1;
		depth >>= 1;
		if( width < 1 )
		{
			width = 1;
		}
		if( height < 1 )
		{
			height = 1;
		}
		if( depth < 1 )
		{
			depth = 1;
		}
		numMipLevels++;
	}
	return numMipLevels;
}

#ifdef DX_TO_GL_ABSTRACTION
#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
	((DWORD)(byte)(ch0) | ((DWORD)(byte)(ch1) << 8) |   \
	((DWORD)(byte)(ch2) << 16) | ((DWORD)(byte)(ch3) << 24 ))
#endif //defined(MAKEFOURCC)
#endif	
//-----------------------------------------------------------------------------
// convert back and forth from D3D format to ImageFormat, regardless of
// whether it's supported or not
//-----------------------------------------------------------------------------
ImageFormat D3DFormatToImageFormat( D3DFORMAT format )
{
	switch ( format )
	{
	case D3DFMT_R8G8B8:
		return IMAGE_FORMAT_BGR888;
	case D3DFMT_A8R8G8B8:
		return IMAGE_FORMAT_BGRA8888;
	case D3DFMT_X8R8G8B8:
		return IMAGE_FORMAT_BGRX8888;
	case D3DFMT_R5G6B5:
		return IMAGE_FORMAT_BGR565;
	case D3DFMT_X1R5G5B5:
		return IMAGE_FORMAT_BGRX5551;
	case D3DFMT_A1R5G5B5:
		return IMAGE_FORMAT_BGRA5551;
	case D3DFMT_A4R4G4B4:
		return IMAGE_FORMAT_BGRA4444;
	case D3DFMT_L8:
		return IMAGE_FORMAT_I8;
	case D3DFMT_A8L8:
		return IMAGE_FORMAT_IA88;
	case D3DFMT_A8:
		return IMAGE_FORMAT_A8;
	case D3DFMT_DXT1:
		return IMAGE_FORMAT_DXT1;
	case D3DFMT_DXT3:
		return IMAGE_FORMAT_DXT3;
	case D3DFMT_DXT5:
		return IMAGE_FORMAT_DXT5;
	case D3DFMT_V8U8:
		return IMAGE_FORMAT_UV88;
	case D3DFMT_Q8W8V8U8:
		return IMAGE_FORMAT_UVWQ8888;
	case D3DFMT_X8L8V8U8:
		return IMAGE_FORMAT_UVLX8888;
	case D3DFMT_A16B16G16R16F:
		return IMAGE_FORMAT_RGBA16161616F;
	case D3DFMT_A16B16G16R16:
		return IMAGE_FORMAT_RGBA16161616;
	case D3DFMT_R32F:
		return IMAGE_FORMAT_R32F;
	case D3DFMT_A32B32G32R32F:
		return IMAGE_FORMAT_RGBA32323232F;
		
	MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()
	// dimhotepus: D3DFORMAT expected to contain vendor-specific.
	MSVC_DISABLE_WARNING(4063)
	// DST and FOURCC formats mapped back to ImageFormat (for vendor-dependent shadow depth textures)
	case (D3DFORMAT)(MAKEFOURCC('R','A','W','Z')):
		return IMAGE_FORMAT_NV_RAWZ;
	case (D3DFORMAT)(MAKEFOURCC('I','N','T','Z')):
		return IMAGE_FORMAT_NV_INTZ;
	case (D3DFORMAT)(MAKEFOURCC('N','U','L','L')):
		return IMAGE_FORMAT_NV_NULL;
	MSVC_END_WARNING_OVERRIDE_SCOPE()
	case D3DFMT_D16:
		return IMAGE_FORMAT_NV_DST16;
	case D3DFMT_D24S8:
		return IMAGE_FORMAT_NV_DST24;
	MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()
	// dimhotepus: D3DFORMAT expected to contain vendor-specific.
	MSVC_DISABLE_WARNING(4063)
	case (D3DFORMAT)(MAKEFOURCC('D','F','1','6')):
		return IMAGE_FORMAT_ATI_DST16;
	case (D3DFORMAT)(MAKEFOURCC('D','F','2','4')):
		return IMAGE_FORMAT_ATI_DST24;

	// ATIxN FOURCC formats mapped back to ImageFormat
	case (D3DFORMAT)(MAKEFOURCC('A','T','I','1')):
		return IMAGE_FORMAT_ATI1N;
	case (D3DFORMAT)(MAKEFOURCC('A','T','I','2')):
		return IMAGE_FORMAT_ATI2N;
	MSVC_END_WARNING_OVERRIDE_SCOPE()
	}

	Assert( 0 );

	return IMAGE_FORMAT_UNKNOWN;
}

D3DFORMAT ImageFormatToD3DFormat( ImageFormat format )
{
	// This doesn't care whether it's supported or not
	switch ( format )
	{
	case IMAGE_FORMAT_BGR888:
		return D3DFMT_R8G8B8;
	case IMAGE_FORMAT_BGRA8888:
		return D3DFMT_A8R8G8B8;
	case IMAGE_FORMAT_BGRX8888:
		return D3DFMT_X8R8G8B8;
	case IMAGE_FORMAT_BGR565:
		return D3DFMT_R5G6B5;
	case IMAGE_FORMAT_BGRX5551:
		return D3DFMT_X1R5G5B5;
	case IMAGE_FORMAT_BGRA5551:
		return D3DFMT_A1R5G5B5;
	case IMAGE_FORMAT_BGRA4444:
		return D3DFMT_A4R4G4B4;
	case IMAGE_FORMAT_I8:
		return D3DFMT_L8;
	case IMAGE_FORMAT_IA88:
		return D3DFMT_A8L8;
	case IMAGE_FORMAT_A8:
		return D3DFMT_A8;
	case IMAGE_FORMAT_DXT1:
	case IMAGE_FORMAT_DXT1_ONEBITALPHA:
		return D3DFMT_DXT1;
	case IMAGE_FORMAT_DXT3:
		return D3DFMT_DXT3;
	case IMAGE_FORMAT_DXT5:
		return D3DFMT_DXT5; //-V1037
	case IMAGE_FORMAT_UV88:
		return D3DFMT_V8U8;
	case IMAGE_FORMAT_UVWQ8888:
		return D3DFMT_Q8W8V8U8;
	case IMAGE_FORMAT_UVLX8888:
		return D3DFMT_X8L8V8U8;
	case IMAGE_FORMAT_RGBA16161616F:
		return D3DFMT_A16B16G16R16F;
	case IMAGE_FORMAT_RGBA16161616:
		return D3DFMT_A16B16G16R16;
	case IMAGE_FORMAT_R32F:
		return D3DFMT_R32F;
	case IMAGE_FORMAT_RGBA32323232F:
		return D3DFMT_A32B32G32R32F;

	// ImageFormat mapped to vendor-dependent FOURCC formats (for shadow depth textures)
	case IMAGE_FORMAT_NV_RAWZ:
		return (D3DFORMAT)(MAKEFOURCC('R','A','W','Z'));
	case IMAGE_FORMAT_NV_INTZ:
		return (D3DFORMAT)(MAKEFOURCC('I','N','T','Z'));
	case IMAGE_FORMAT_NV_NULL:
		return (D3DFORMAT)(MAKEFOURCC('N','U','L','L'));
	case IMAGE_FORMAT_NV_DST16:
		return D3DFMT_D16;
	case IMAGE_FORMAT_NV_DST24:
		return D3DFMT_D24S8;
	case IMAGE_FORMAT_ATI_DST16:
		return (D3DFORMAT)(MAKEFOURCC('D','F','1','6'));
	case IMAGE_FORMAT_ATI_DST24:
		return (D3DFORMAT)(MAKEFOURCC('D','F','2','4'));

	// ImageFormats mapped to ATIxN FOURCC
	case IMAGE_FORMAT_ATI1N:
		return (D3DFORMAT)(MAKEFOURCC('A','T','I','1'));
	case IMAGE_FORMAT_ATI2N:
		return (D3DFORMAT)(MAKEFOURCC('A','T','I','2'));

	case IMAGE_FORMAT_DXT1_RUNTIME:
		return D3DFMT_DXT1;
	case IMAGE_FORMAT_DXT5_RUNTIME:
		return D3DFMT_DXT5;
	}

	Assert( 0 );

	return D3DFMT_UNKNOWN;
}

} // ImageLoader namespace ends

