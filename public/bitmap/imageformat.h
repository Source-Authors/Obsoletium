//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef IMAGEFORMAT_H
#define IMAGEFORMAT_H

#include <cstdio>
#include "tier0/basetypes.h"

enum NormalDecodeMode_t
{
	NORMAL_DECODE_NONE			= 0
};

// Forward declaration
#ifdef _WIN32
using D3DFORMAT = enum _D3DFORMAT;
enum DXGI_FORMAT;
#endif

//-----------------------------------------------------------------------------
// The various image format types
//-----------------------------------------------------------------------------

enum ImageFormat : int
{
	IMAGE_FORMAT_UNKNOWN  = -1,
	IMAGE_FORMAT_RGBA8888 = 0, 
	IMAGE_FORMAT_ABGR8888, 
	IMAGE_FORMAT_RGB888, 
	IMAGE_FORMAT_BGR888,
	IMAGE_FORMAT_RGB565, 
	IMAGE_FORMAT_I8,
	IMAGE_FORMAT_IA88,
	IMAGE_FORMAT_P8,
	IMAGE_FORMAT_A8,
	IMAGE_FORMAT_RGB888_BLUESCREEN,
	IMAGE_FORMAT_BGR888_BLUESCREEN,
	IMAGE_FORMAT_ARGB8888,
	IMAGE_FORMAT_BGRA8888,
	IMAGE_FORMAT_DXT1,
	IMAGE_FORMAT_DXT3,
	IMAGE_FORMAT_DXT5,
	IMAGE_FORMAT_BGRX8888,
	IMAGE_FORMAT_BGR565,
	IMAGE_FORMAT_BGRX5551,
	IMAGE_FORMAT_BGRA4444,
	IMAGE_FORMAT_DXT1_ONEBITALPHA,
	IMAGE_FORMAT_BGRA5551,
	IMAGE_FORMAT_UV88,
	IMAGE_FORMAT_UVWQ8888,
	IMAGE_FORMAT_RGBA16161616F,
	IMAGE_FORMAT_RGBA16161616,
	IMAGE_FORMAT_UVLX8888,
	IMAGE_FORMAT_R32F,			// Single-channel 32-bit floating point
	IMAGE_FORMAT_RGB323232F,
	IMAGE_FORMAT_RGBA32323232F,

	// Depth-stencil texture formats for shadow depth mapping
	IMAGE_FORMAT_NV_DST16,		// 
	IMAGE_FORMAT_NV_DST24,		//
	IMAGE_FORMAT_NV_INTZ,		// Vendor-specific depth-stencil texture
	IMAGE_FORMAT_NV_RAWZ,		// formats for shadow depth mapping 
	IMAGE_FORMAT_ATI_DST16,		// 
	IMAGE_FORMAT_ATI_DST24,		//
	IMAGE_FORMAT_NV_NULL,		// Dummy format which takes no video memory

	// Compressed normal map formats
	IMAGE_FORMAT_ATI2N,			// One-surface ATI2N / DXN format
	IMAGE_FORMAT_ATI1N,			// Two-surface ATI1N format

	IMAGE_FORMAT_DXT1_RUNTIME,
	IMAGE_FORMAT_DXT5_RUNTIME,

	NUM_IMAGE_FORMATS
};

#if defined( POSIX  ) || defined( DX_TO_GL_ABSTRACTION )
typedef enum _D3DFORMAT
	{
		D3DFMT_INDEX16,
		D3DFMT_D16,
		D3DFMT_D24S8,
		D3DFMT_A8R8G8B8,
		D3DFMT_A4R4G4B4,
		D3DFMT_X8R8G8B8,
		D3DFMT_R5G6R5,
		D3DFMT_X1R5G5B5,
		D3DFMT_A1R5G5B5,
		D3DFMT_L8,
		D3DFMT_A8L8,
		D3DFMT_A,
		D3DFMT_DXT1,
		D3DFMT_DXT3,
		D3DFMT_DXT5,
		D3DFMT_V8U8,
		D3DFMT_Q8W8V8U8,
		D3DFMT_X8L8V8U8,
		D3DFMT_A16B16G16R16F,
		D3DFMT_A16B16G16R16,
		D3DFMT_R32F,
		D3DFMT_A32B32G32R32F,
		D3DFMT_R8G8B8,
		D3DFMT_D24X4S4,
		D3DFMT_A8,
		D3DFMT_R5G6B5,
		D3DFMT_D15S1,
		D3DFMT_D24X8,
		D3DFMT_VERTEXDATA,
		D3DFMT_INDEX32,

		// adding fake D3D format names for the vendor specific ones (eases debugging/logging)
		
		// NV shadow depth tex
		D3DFMT_NV_INTZ		= 0x5a544e49,	// MAKEFOURCC('I','N','T','Z')
		D3DFMT_NV_RAWZ		= 0x5a574152,	// MAKEFOURCC('R','A','W','Z')

		// NV null tex
		D3DFMT_NV_NULL		= 0x4c4c554e,	// MAKEFOURCC('N','U','L','L')

		// ATI shadow depth tex
		D3DFMT_ATI_D16		= 0x36314644,	// MAKEFOURCC('D','F','1','6')
		D3DFMT_ATI_D24S8	= 0x34324644,	// MAKEFOURCC('D','F','2','4')

		// ATI 1N and 2N compressed tex
		D3DFMT_ATI_2N		= 0x32495441,	// MAKEFOURCC('A', 'T', 'I', '2')
		D3DFMT_ATI_1N		= 0x31495441,	// MAKEFOURCC('A', 'T', 'I', '1')
		
		D3DFMT_UNKNOWN
	} D3DFORMAT;
#endif

//-----------------------------------------------------------------------------
// Color structures
//-----------------------------------------------------------------------------
struct BGRA8888_t;
struct BGRX8888_t;
struct RGBA8888_t;
struct RGB888_t;
struct BGR888_t;
struct BGR565_t;
struct BGRA5551_t;
struct BGRA4444_t;
struct RGBX5551_t;

struct BGRA8888_t
{
	unsigned char b;		// change the order of names to change the 
	unsigned char g;		//  order of the output ARGB or BGRA, etc...
	unsigned char r;		//  Last one is MSB, 1st is LSB.
	unsigned char a;
};

struct BGRX8888_t
{
	unsigned char b;		// change the order of names to change the 
	unsigned char g;		//  order of the output ARGB or BGRA, etc...
	unsigned char r;		//  Last one is MSB, 1st is LSB.
	unsigned char x;
};

struct RGBA8888_t
{
	unsigned char r;		// change the order of names to change the 
	unsigned char g;		//  order of the output ARGB or BGRA, etc...
	unsigned char b;		//  Last one is MSB, 1st is LSB.
	unsigned char a;
	inline RGBA8888_t& operator=( BGRA8888_t in );
	inline RGBA8888_t& operator=( RGB888_t in );
	inline RGBA8888_t& operator=( BGRX8888_t in );
};

struct RGB888_t
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
	inline RGB888_t& operator=( BGRA8888_t in )
	{
		r = in.r;
		g = in.g;
		b = in.b;
		return *this;
	}
	[[nodiscard]] inline bool operator==( const RGB888_t& in ) const
	{
		return ( r == in.r ) && ( g == in.g ) && ( b == in.b );
	}
	[[nodiscard]] inline bool operator!=( const RGB888_t& in ) const
	{
		return ( r != in.r ) || ( g != in.g ) || ( b != in.b );
	}
};

struct BGR888_t
{
	unsigned char b;
	unsigned char g;
	unsigned char r;
	inline BGR888_t& operator=( BGRA8888_t in )
	{
		r = in.r;
		g = in.g;
		b = in.b;
		return *this;
	}
};

struct BGR565_t
{
	unsigned short b : 5;		// order of names changes
	unsigned short g : 6;		//  byte order of output to 32 bit
	unsigned short r : 5;
	inline BGR565_t& operator=( BGRA8888_t in )
	{
		r = in.r >> 3;
		g = in.g >> 2;
		b = in.b >> 3;
		return *this;
	}
	inline BGR565_t &Set( int red, int green, int blue )
	{
		r = red >> 3;
		g = green >> 2;
		b = blue >> 3;
		return *this;
	}
};

struct BGRA5551_t
{
	unsigned short b : 5;		// order of names changes
	unsigned short g : 5;		//  byte order of output to 32 bit
	unsigned short r : 5;
	unsigned short a : 1;
	inline BGRA5551_t& operator=( BGRA8888_t in )
	{
		r = in.r >> 3;
		g = in.g >> 3;
		b = in.b >> 3;
		a = in.a >> 7;
		return *this;
	}
};

struct BGRA4444_t
{
	unsigned short b : 4;		// order of names changes
	unsigned short g : 4;		//  byte order of output to 32 bit
	unsigned short r : 4;
	unsigned short a : 4;
	inline BGRA4444_t& operator=( BGRA8888_t in )
	{
		r = in.r >> 4;
		g = in.g >> 4;
		b = in.b >> 4;
		a = in.a >> 4;
		return *this;
	}
};

struct RGBX5551_t
{
	unsigned short r : 5;
	unsigned short g : 5;
	unsigned short b : 5;
	unsigned short x : 1;
	inline RGBX5551_t& operator=( BGRA8888_t in )
	{
		r = in.r >> 3;
		g = in.g >> 3;
		b = in.b >> 3;
		return *this;
	}
};


//-----------------------------------------------------------------------------
// Conversion assignments
//-----------------------------------------------------------------------------
RGBA8888_t& RGBA8888_t::operator=( BGRA8888_t in )
{
	r = in.r;
	g = in.g;
	b = in.b;
	a = in.a;
	return *this;
}

RGBA8888_t& RGBA8888_t::operator=( RGB888_t in )
{
	r = in.r;
	g = in.g;
	b = in.b;
	a = 0xFF;
	return *this;
}

RGBA8888_t& RGBA8888_t::operator=( BGRX8888_t in )
{
	r = in.r;
	g = in.g;
	b = in.b;
	a = 0xFF;
	return *this;
}

//-----------------------------------------------------------------------------
// some important constants
//-----------------------------------------------------------------------------
constexpr float ARTWORK_GAMMA{2.2f};
constexpr int IMAGE_MAX_DIM{2048};


//-----------------------------------------------------------------------------
// information about each image format
//-----------------------------------------------------------------------------
struct ImageFormatInfo_t
{
	const char* m_pName;
	int m_NumBytes;
	int m_NumRedBits;
	int m_NumGreeBits;
	int m_NumBlueBits;
	int m_NumAlphaBits;
	bool m_IsCompressed;
};


//-----------------------------------------------------------------------------
// Various methods related to pixelmaps and color formats
//-----------------------------------------------------------------------------
namespace ImageLoader
{

	[[nodiscard]] bool GetInfo( const char *fileName, int *width, int *height, ImageFormat *imageFormat, float *sourceGamma );
	[[nodiscard]] intp  GetMemRequired( int width, int height, int depth, ImageFormat imageFormat, bool mipmap );
	[[nodiscard]] intp  GetMipMapLevelByteOffset( int width, int height, ImageFormat imageFormat, int skipMipLevels );
	void GetMipMapLevelDimensions( int *width, int *height, int skipMipLevels );
	[[nodiscard]] int  GetNumMipMapLevels( int width, int height, int depth = 1 );
	[[nodiscard]] bool Load( unsigned char *imageData, const char *fileName, int width,
		int height, ImageFormat imageFormat, float targetGamma, bool mipmap );
	[[nodiscard]] bool Load( unsigned char *imageData, FILE *fp, int width, int height, 
		ImageFormat imageFormat, float targetGamma, bool mipmap );

	// convert from any image format to any other image format.
	// return false if the conversion cannot be performed.
	// Strides denote the number of bytes per each line, 
	// by default assumes width * # of bytes per pixel
	bool ConvertImageFormat( const unsigned char *src, ImageFormat srcImageFormat,
							 unsigned char *dst, ImageFormat dstImageFormat, 
							 int width, int height, int srcStride = 0, int dstStride = 0 );

	// must be used in conjunction with ConvertImageFormat() to pre-swap and post-swap
	void PreConvertSwapImageData( unsigned char *pImageData, intp nImageSize, ImageFormat imageFormat, int width = 0, int stride = 0 );
	void PostConvertSwapImageData( unsigned char *pImageData, intp nImageSize, ImageFormat imageFormat, int width = 0, int stride = 0 );
	void ByteSwapImageData( unsigned char *pImageData, intp nImageSize, ImageFormat imageFormat, int width = 0, int stride = 0 );
	[[nodiscard]] bool IsFormatValidForConversion( ImageFormat fmt );

	//-----------------------------------------------------------------------------
	// convert back and forth from D3D format to ImageFormat, regardless of
	// whether it's supported or not
	//-----------------------------------------------------------------------------
	[[nodiscard]] ImageFormat D3DFormatToImageFormat( D3DFORMAT format );
	[[nodiscard]] ImageFormat DxgiFormatToImageFormat( DXGI_FORMAT format );
	[[nodiscard]] D3DFORMAT ImageFormatToD3DFormat( ImageFormat format );
	[[nodiscard]] DXGI_FORMAT ImageFormatToDxgiFormat( ImageFormat format );

	// Flags for ResampleRGBA8888
	enum
	{
		RESAMPLE_NORMALMAP = 0x1,
		RESAMPLE_ALPHATEST = 0x2,
		RESAMPLE_NICE_FILTER = 0x4,
		RESAMPLE_CLAMPS = 0x8,
		RESAMPLE_CLAMPT = 0x10,
		RESAMPLE_CLAMPU = 0x20,
	};

	struct ResampleInfo_t
	{
		ResampleInfo_t()
			 
		{
			m_flColorScale[0] = 1.0f;
			m_flColorScale[1] = 1.0f;
			m_flColorScale[2] = 1.0f;
			m_flColorScale[3] = 1.0f;

			m_flColorGoal[0] = 0.0f;
			m_flColorGoal[1] = 0.0f;
			m_flColorGoal[2] = 0.0f;
			m_flColorGoal[3] = 0.0f;
		}

		unsigned char *m_pSrc{nullptr};
		unsigned char *m_pDest{nullptr};

		int m_nSrcWidth{-1};
		int m_nSrcHeight{-1};
		int m_nSrcDepth{1};
		
		int m_nDestWidth{-1};
		int m_nDestHeight{-1};
		int m_nDestDepth{1};
		
		float m_flSrcGamma{0.0F};
		float m_flDestGamma{0.0F};
		
		float m_flColorScale[4];	// Color scale factors RGBA
		float m_flColorGoal[4];		// Color goal values RGBA		DestColor = ColorGoal + scale * (SrcColor - ColorGoal)
		
		float m_flAlphaThreshhold{0.4f};
		float m_flAlphaHiFreqThreshhold{0.4f};
		
		int m_nFlags{0};
	};

	[[nodiscard]] bool ResampleRGBA8888( const ResampleInfo_t &info );
	[[nodiscard]] bool ResampleRGBA16161616( const ResampleInfo_t &info );
	[[nodiscard]] bool ResampleRGB323232F( const ResampleInfo_t &info );
	[[nodiscard]] bool ResampleRGBA32323232F( const ResampleInfo_t &info );

	void ConvertNormalMapRGBA8888ToDUDVMapUVLX8888( const unsigned char *src, int width, int height,
													unsigned char *dst_ );
	void ConvertNormalMapRGBA8888ToDUDVMapUVWQ8888( const unsigned char *src, int width, int height,
													unsigned char *dst_ );
	void ConvertNormalMapRGBA8888ToDUDVMapUV88( const unsigned char *src, int width, int height,
												unsigned char *dst_ );

	void ConvertIA88ImageToNormalMapRGBA8888( const unsigned char *src, int width, 
											  int height, unsigned char *dst,
											  float bumpScale );

	void NormalizeNormalMapRGBA8888( unsigned char *src, int numTexels );


	//-----------------------------------------------------------------------------
	// Gamma correction
	//-----------------------------------------------------------------------------
	void GammaCorrectRGBA8888( unsigned char *src, unsigned char* dst,
							   int width, int height, int depth, float srcGamma, float dstGamma );


	//-----------------------------------------------------------------------------
	// Makes a gamma table
	//-----------------------------------------------------------------------------
	// dimhotepus: Make internal.
	// void ConstructGammaTable( unsigned char* pTable, float srcGamma, float dstGamma );


	//-----------------------------------------------------------------------------
	// Gamma corrects using a previously constructed gamma table
	//-----------------------------------------------------------------------------
	void GammaCorrectRGBA8888( unsigned char* pSrc, unsigned char* pDst,
							   int width, int height, int depth, unsigned char* pGammaTable );


	//-----------------------------------------------------------------------------
	// Generates a number of mipmap levels
	//-----------------------------------------------------------------------------
	void GenerateMipmapLevels( unsigned char* pSrc, unsigned char* pDst, int width,
							   int height,	int depth, ImageFormat imageFormat, float srcGamma, float dstGamma, 
							   int numLevels = 0 );

	// Low quality mipmap generation, but way faster. 
	void GenerateMipmapLevelsLQ( unsigned char* pSrc, unsigned char* pDst, int width, int height, 
		                         ImageFormat imageFormat, int numLevels );

	//-----------------------------------------------------------------------------
	// operations on square images (src and dst can be the same)
	//-----------------------------------------------------------------------------
	[[nodiscard]] bool RotateImageLeft( const unsigned char *src, unsigned char *dst, 
		int widthHeight, ImageFormat imageFormat );
	[[nodiscard]] bool RotateImage180( const unsigned char *src, unsigned char *dst, 
		int widthHeight, ImageFormat imageFormat );
	[[nodiscard]] bool FlipImageVertically( void *pSrc, void *pDst, int nWidth,
		int nHeight, ImageFormat imageFormat, int nDstStride = 0 );
	[[nodiscard]] bool FlipImageHorizontally( void *pSrc, void *pDst, int nWidth,
		int nHeight, ImageFormat imageFormat, int nDstStride = 0 );
	[[nodiscard]] bool SwapAxes( unsigned char *src, 
		int widthHeight, ImageFormat imageFormat );


	//-----------------------------------------------------------------------------
	// Returns info about each image format
	//-----------------------------------------------------------------------------
	[[nodiscard]] ImageFormatInfo_t const& ImageFormatInfo( ImageFormat fmt );


	//-----------------------------------------------------------------------------
	// Gets the name of the image format
	//-----------------------------------------------------------------------------
	[[nodiscard]] inline char const* GetName( ImageFormat fmt )
	{
		return ImageFormatInfo(fmt).m_pName;
	}


	//-----------------------------------------------------------------------------
	// Gets the size of the image format in bytes
	//-----------------------------------------------------------------------------
	[[nodiscard]] inline int SizeInBytes( ImageFormat fmt )
	{
		return ImageFormatInfo(fmt).m_NumBytes;
	}

	//-----------------------------------------------------------------------------
	// Does the image format support transparency?
	//-----------------------------------------------------------------------------
	[[nodiscard]] inline bool IsTransparent( ImageFormat fmt )
	{
		return ImageFormatInfo(fmt).m_NumAlphaBits > 0;
	}


	//-----------------------------------------------------------------------------
	// Is the image format compressed?
	//-----------------------------------------------------------------------------
	[[nodiscard]] inline bool IsCompressed( ImageFormat fmt )
	{
		return ImageFormatInfo(fmt).m_IsCompressed;
	}

	//-----------------------------------------------------------------------------
	// Is any channel > 8 bits?
	//-----------------------------------------------------------------------------
	[[nodiscard]] inline bool HasChannelLargerThan8Bits( ImageFormat fmt )
	{
		ImageFormatInfo_t info = ImageFormatInfo(fmt);
		return ( info.m_NumRedBits > 8 || info.m_NumGreeBits > 8 || info.m_NumBlueBits > 8 || info.m_NumAlphaBits > 8 );
	}

	[[nodiscard]] inline bool IsRuntimeCompressed( ImageFormat fmt )
	{
		return ( fmt == IMAGE_FORMAT_DXT1_RUNTIME ) || ( fmt == IMAGE_FORMAT_DXT5_RUNTIME );
	}

} // end namespace ImageLoader


#endif // IMAGEFORMAT_H
