//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "bitmap/imageformat.h"

// dimhotepus: <cmath> is not needed anymore (integer 565 expansion), add headers actually used.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#include "tier0/basetypes.h"
#include "tier0/dbg.h"
#include "tier0/platform.h"
#include "tier0/wchartypes.h"
#include "mathlib/vector.h"
#include "mathlib/compressed_vector.h"
// dimhotepus: nvtc and ATI_Compress are x86 / x86-64 only binaries, so disable them on arm64 (Windows, Linux, macOS).
#if defined( _M_ARM64 ) || defined( __aarch64__ )
#ifndef NO_NVTC
#define NO_NVTC
#endif
#ifndef NO_ATI_COMPRESS
#define NO_ATI_COMPRESS
#endif
#endif

// dimhotepus: Exclude nvtc as proprietary.
#ifndef NO_NVTC
#include "nvtc.h"
#endif

// dimhotepus: Needed by ATI_Compress.h only.
#if defined( POSIX ) && !defined( NO_ATI_COMPRESS )
typedef int32 *DWORD_PTR;
#endif

// dimhotepus: Exclude ATI compress as proprietary.
#ifndef NO_ATI_COMPRESS
#include "ATI_Compress.h"
#endif
#include "bitmap/float_bm.h"

MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()
MSVC_DISABLE_WARNING(4244)
#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"
MSVC_END_WARNING_OVERRIDE_SCOPE()

// Should be last include
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Various important function types for each color format
//-----------------------------------------------------------------------------

using UserFormatToRGBA8888Func_t = void (*)(const uint8 *, uint8 *, int);
using RGBA8888ToUserFormatFunc_t = void (*)(const uint8 *, uint8 *, int);


namespace ImageLoader
{

// dimhotepus: Pixel buffers have arbitrary strides, so they are not guaranteed to be aligned.  Casting to
// unsigned short* / float* and dereferencing is UB and can trap on strict alignment targets.  Use memcpy,
// compilers turn it into a single load / store.
static inline unsigned LoadU16( const uint8 *p )
{
	unsigned short v;
	memcpy( &v, p, sizeof( v ) );
	return v;
}

static inline void StoreU16( uint8 *p, unsigned v )
{
	const auto s = static_cast<unsigned short>( v );
	memcpy( p, &s, sizeof( s ) );
}

static inline void StoreF32( uint8 *p, float v )
{
	memcpy( p, &v, sizeof( v ) );
}

// Color Conversion functions
// dimhotepus: CS:GO backport.
static void RGBA8888ToRGBA32323232F( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToRGB323232F( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToR32F( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToABGR8888( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToRGB888( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToBGR888( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToI8( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToIA88( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToA8( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToRGB888_BLUESCREEN( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToBGR888_BLUESCREEN( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToARGB8888( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToBGRA8888( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToBGRX8888( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToBGR565( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToBGRX5551( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToBGRA5551( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToBGRA4444( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToUV88( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToUVWQ8888( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToUVLX8888( const uint8 *src, uint8 *dst, int numPixels );
//static void RGBA8888ToRGBA16161616F( const uint8 *src, uint8 *dst, int numPixels );

static void ABGR8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void RGB888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void BGR888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void I8ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void IA88ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void A8ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void RGB888_BLUESCREENToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void BGR888_BLUESCREENToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void ARGB8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void BGRA8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void BGRX8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void BGR565ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void BGRX5551ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void BGRA5551ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void BGRA4444ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void UV88ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void UVWQ8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void UVLX8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA16161616ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
//static void RGBA16161616FToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );


static UserFormatToRGBA8888Func_t GetUserFormatToRGBA8888Func_t( ImageFormat srcImageFormat )
{
	switch( srcImageFormat )
	{
	case IMAGE_FORMAT_RGBA8888:
		return RGBA8888ToRGBA8888;
	case IMAGE_FORMAT_ABGR8888:
		return ABGR8888ToRGBA8888;
	case IMAGE_FORMAT_RGB888:
		return RGB888ToRGBA8888;
	case IMAGE_FORMAT_BGR888:
		return BGR888ToRGBA8888;
	case IMAGE_FORMAT_RGB565:
		return nullptr; //-V1037
	case IMAGE_FORMAT_I8:
		return I8ToRGBA8888;
	case IMAGE_FORMAT_IA88:
		return IA88ToRGBA8888;
	case IMAGE_FORMAT_A8:
		return A8ToRGBA8888;
	case IMAGE_FORMAT_RGB888_BLUESCREEN:
		return RGB888_BLUESCREENToRGBA8888;
	case IMAGE_FORMAT_BGR888_BLUESCREEN:
		return BGR888_BLUESCREENToRGBA8888;
	case IMAGE_FORMAT_ARGB8888:
		return ARGB8888ToRGBA8888;
	case IMAGE_FORMAT_BGRA8888:
		return BGRA8888ToRGBA8888;
	case IMAGE_FORMAT_BGRX8888:
		return BGRX8888ToRGBA8888;
	case IMAGE_FORMAT_BGR565:
		return BGR565ToRGBA8888;
	case IMAGE_FORMAT_BGRX5551:
		return BGRX5551ToRGBA8888;
	case IMAGE_FORMAT_BGRA5551:
		return BGRA5551ToRGBA8888;
	case IMAGE_FORMAT_BGRA4444:
		return BGRA4444ToRGBA8888;
	case IMAGE_FORMAT_UV88:
		return UV88ToRGBA8888;
	case IMAGE_FORMAT_UVWQ8888:
		return UVWQ8888ToRGBA8888;
	case IMAGE_FORMAT_UVLX8888:
		return UVLX8888ToRGBA8888;
	case IMAGE_FORMAT_RGBA16161616:
		return RGBA16161616ToRGBA8888;
	case IMAGE_FORMAT_RGBA16161616F:
		return nullptr;

	default:
		return nullptr;
	}
}

static RGBA8888ToUserFormatFunc_t GetRGBA8888ToUserFormatFunc_t( ImageFormat dstImageFormat )
{
	switch( dstImageFormat )
	{
		// dimhotepus: CS:GO backport.
	case IMAGE_FORMAT_RGBA32323232F:
		return RGBA8888ToRGBA32323232F;
		// dimhotepus: CS:GO backport.
	case IMAGE_FORMAT_RGB323232F:
		return RGBA8888ToRGB323232F;
		// dimhotepus: CS:GO backport.
	case IMAGE_FORMAT_R32F:
		return RGBA8888ToR32F;
	case IMAGE_FORMAT_RGBA8888:
		return RGBA8888ToRGBA8888;
	case IMAGE_FORMAT_ABGR8888:
		return RGBA8888ToABGR8888;
	case IMAGE_FORMAT_RGB888:
		return RGBA8888ToRGB888;
	case IMAGE_FORMAT_BGR888:
		return RGBA8888ToBGR888;
	case IMAGE_FORMAT_RGB565:
		return nullptr; //-V1037
	case IMAGE_FORMAT_I8:
		return RGBA8888ToI8;
	case IMAGE_FORMAT_IA88:
		return RGBA8888ToIA88;
	case IMAGE_FORMAT_A8:
		return RGBA8888ToA8;
	case IMAGE_FORMAT_RGB888_BLUESCREEN:
		return RGBA8888ToRGB888_BLUESCREEN;
	case IMAGE_FORMAT_BGR888_BLUESCREEN:
		return RGBA8888ToBGR888_BLUESCREEN;
	case IMAGE_FORMAT_ARGB8888:
		return RGBA8888ToARGB8888;
	case IMAGE_FORMAT_BGRA8888:
		return RGBA8888ToBGRA8888;
	case IMAGE_FORMAT_BGRX8888:
		return RGBA8888ToBGRX8888;
	case IMAGE_FORMAT_BGR565:
		return RGBA8888ToBGR565;
	case IMAGE_FORMAT_BGRX5551:
		return RGBA8888ToBGRX5551;
	case IMAGE_FORMAT_BGRA5551:
		return RGBA8888ToBGRA5551;
	case IMAGE_FORMAT_BGRA4444:
		return RGBA8888ToBGRA4444;
	case IMAGE_FORMAT_UV88:
		return RGBA8888ToUV88;
	case IMAGE_FORMAT_UVWQ8888:
		return RGBA8888ToUVWQ8888;
	case IMAGE_FORMAT_UVLX8888:
		return RGBA8888ToUVLX8888;
	case IMAGE_FORMAT_RGBA16161616F:
		return nullptr;

	default:
		return nullptr;
	}
}



#pragma pack(1)

struct DXTColBlock
{
  word col0;
  word col1;

	// no bit fields - use bytes
	byte row[4];
};

struct DXTAlphaBlock3BitLinear
{
	byte alpha0;
	byte alpha1;

	byte stuff[6];
};

#pragma pack()

// dimhotepus: Expand 5 / 6 bit channel to 8 bits with rounding.  Integer math gives exactly the same result as
// round( v * 255.0f / 31.0f ) / round( v * 255.0f / 63.0f ) (255 * v / 31 and 255 * v / 63 never have a .5
// fraction), but it does not depend on <cmath> and does not type-pun the color through bit field structs
// (strict aliasing violation).
static inline uint8 Expand5To8( unsigned v ) { return static_cast<uint8>( ( v * 255U + 15U ) / 31U ); }
static inline uint8 Expand6To8( unsigned v ) { return static_cast<uint8>( ( v * 255U + 31U ) / 63U ); }

// dimhotepus: bForceFourColors - DXT3 / DXT5 color blocks are always decoded in four color mode
// (color0 <= color1 does not select three color + transparent mode there, it is DXT1 only feature).
static inline void GetColorBlockColorsBGRA8888( const DXTColBlock *pBlock, BGRA8888_t *col_0, 
											    BGRA8888_t *col_1, BGRA8888_t *col_2, 
												BGRA8888_t *col_3, word & wrd, bool bForceFourColors = false )
{
	// input data is assumed to be x86 order
	// swap to target platform for proper dxt decoding
	word color0 = LittleShort( pBlock->col0 );
	word color1 = LittleShort( pBlock->col1 );

	// convert to full precision correctly.
	col_0->a = 0xff;
	col_0->r = Expand5To8( color0 >> 11 );
	col_0->g = Expand6To8( ( color0 >> 5 ) & 0x3FU );
	col_0->b = Expand5To8( color0 & 0x1FU );

	col_1->a = 0xff;
	col_1->r = Expand5To8( color1 >> 11 );
	col_1->g = Expand6To8( ( color1 >> 5 ) & 0x3FU );
	col_1->b = Expand5To8( color1 & 0x1FU );

	if ( bForceFourColors || color0 > color1 )
	{
		// Four-color block: derive the other two colors.    
		// 00 = color_0, 01 = color_1, 10 = color_2, 11 = color_3
		// These two bit codes correspond to the 2-bit fields 
		// stored in the 64-bit block.

		wrd = ((word)col_0->r * 2 + (word)col_1->r )/3;
											// no +1 for rounding
											// as bits have been shifted to 888
		col_2->r = (byte)wrd;

		wrd = ((word)col_0->g * 2 + (word)col_1->g )/3;
		col_2->g = (byte)wrd;

		wrd = ((word)col_0->b * 2 + (word)col_1->b )/3;
		col_2->b = (byte)wrd;
		col_2->a = 0xff;

		wrd = ((word)col_0->r + (word)col_1->r *2 )/3;
		col_3->r = (byte)wrd;

		wrd = ((word)col_0->g + (word)col_1->g *2 )/3;
		col_3->g = (byte)wrd;

		wrd = ((word)col_0->b + (word)col_1->b *2 )/3;
		col_3->b = (byte)wrd;
		col_3->a = 0xff;
	}
	else
	{
		// Three-color block: derive the other color.
		// 00 = color_0,  01 = color_1,  10 = color_2,  
		// 11 = transparent.
		// These two bit codes correspond to the 2-bit fields 
		// stored in the 64-bit block. 

		// explicit for each component, unlike some refrasts...????
		
		wrd = ((word)col_0->r + (word)col_1->r )/2;
		col_2->r = (byte)wrd;
		wrd = ((word)col_0->g + (word)col_1->g )/2;
		col_2->g = (byte)wrd;
		wrd = ((word)col_0->b + (word)col_1->b )/2;
		col_2->b = (byte)wrd;
		col_2->a = 0xff;

		col_3->r = 0x00;		// random color to indicate alpha
		col_3->g = 0xff;
		col_3->b = 0xff;
		col_3->a = 0x00;
	}
}

template <class CDestPixel> 
static inline void DecodeColorBlock( CDestPixel *pOutputImage, const DXTColBlock *pColorBlock, int width,
					                 BGRA8888_t *col_0, BGRA8888_t *col_1, 
					                 BGRA8888_t *col_2, BGRA8888_t *col_3 )
{
	// width is width of image in pixels
	dword bits;
	int r,n;

	// bit masks = 00000011, 00001100, 00110000, 11000000
	constexpr dword masks[] = { 3 << 0, 3 << 2, 3 << 4, 3 << 6 };
	constexpr int   shift[] = { 0, 2, 4, 6 };

	// r steps through lines in y
	for ( r=0; r < 4; r++, pOutputImage += width-4 )	// no width*4 as dword ptr inc will *4
	{
		// width * 4 bytes per pixel per line
		// each j dxtc row is 4 lines of pixels

		// n steps through pixels
		for ( n=0; n < 4; n++ )
		{
			bits = pColorBlock->row[r] & masks[n];
			bits >>= shift[n];

			switch( bits )
			{
			case 0:
				*pOutputImage = *col_0;
				pOutputImage++;		// increment to next output pixel
				break;
			case 1:
				*pOutputImage = *col_1;
				pOutputImage++;
				break;
			case 2:
				*pOutputImage = *col_2;
				pOutputImage++;
				break;
			case 3:
				*pOutputImage = *col_3;
				pOutputImage++;
				break;
			default:
				Assert( 0 );
				pOutputImage++;
				break;
			}
		}
	}
}

// dimhotepus: Alpha is 1 / 4 bit in BGRA5551 / BGRA4444, so alpha byte must be reduced, not truncated
// (assigning 255 to 1 bit field gave 1, but 254 gave 0).
template <class CDestPixel>
static inline void SetPixelAlpha( CDestPixel *pPixel, uint8 a )
{
	pPixel->a = a;
}

static inline void SetPixelAlpha( BGRA5551_t *pPixel, uint8 a )
{
	pPixel->a = static_cast<unsigned>( a >> 7 );
}

static inline void SetPixelAlpha( BGRA4444_t *pPixel, uint8 a )
{
	pPixel->a = static_cast<unsigned>( a >> 4 );
}

template <class CDestPixel> 
static inline void DecodeAlpha3BitLinear( CDestPixel *pImPos, const DXTAlphaBlock3BitLinear *pAlphaBlock, int width, int nChannelSelect = 3 )
{
	// dimhotepus: Tables were function level statics shared between all calls => data race when textures
	// are decoded on several threads.  Use locals.
	uint8 alphas[8];

	const unsigned alpha0 = pAlphaBlock->alpha0;
	const unsigned alpha1 = pAlphaBlock->alpha1;

	alphas[0] = static_cast<uint8>( alpha0 );
	alphas[1] = static_cast<uint8>( alpha1 );

	// 8-alpha or 6-alpha block?

	if( alpha0 > alpha1 )
	{
		// 8-alpha block:  derive the other 6 alphas.    
		// 000 = alpha_0, 001 = alpha_1, others are interpolated

		alphas[2] = static_cast<uint8>( ( 6 * alpha0 +     alpha1) / 7 );	// bit code 010
		alphas[3] = static_cast<uint8>( ( 5 * alpha0 + 2 * alpha1) / 7 );	// Bit code 011    
		alphas[4] = static_cast<uint8>( ( 4 * alpha0 + 3 * alpha1) / 7 );	// Bit code 100    
		alphas[5] = static_cast<uint8>( ( 3 * alpha0 + 4 * alpha1) / 7 );	// Bit code 101
		alphas[6] = static_cast<uint8>( ( 2 * alpha0 + 5 * alpha1) / 7 );	// Bit code 110    
		alphas[7] = static_cast<uint8>( (     alpha0 + 6 * alpha1) / 7 );	// Bit code 111
	}    
	else
	{
		// 6-alpha block:  derive the other alphas.    
		// 000 = alpha_0, 001 = alpha_1, others are interpolated

		alphas[2] = static_cast<uint8>( (4 * alpha0 +     alpha1) / 5 );	// Bit code 010
		alphas[3] = static_cast<uint8>( (3 * alpha0 + 2 * alpha1) / 5 );	// Bit code 011    
		alphas[4] = static_cast<uint8>( (2 * alpha0 + 3 * alpha1) / 5 );	// Bit code 100    
		alphas[5] = static_cast<uint8>( (    alpha0 + 4 * alpha1) / 5 );	// Bit code 101
		alphas[6] = 0;														// Bit code 110
		alphas[7] = 255;													// Bit code 111
	}

	// dimhotepus: 16 pixels * 3 bit indices = 48 bits stored in 6 bytes, little endian.  Assemble them
	// explicitly instead of memcpy to (partially uninitialized) dword, so no endianness / uninitialized reads.
	uint64 bits = 0;
	for ( int i = 0; i < 6; i++ )
	{
		bits |= static_cast<uint64>( pAlphaBlock->stuff[i] ) << ( 8 * i );
	}

	// decode the codes into alpha values and write out to the image bits
	for ( int row = 0; row < 4; row++, pImPos += width-4 )
	{
		for ( int pix = 0; pix < 4; pix++ )
		{
			const uint8 a = alphas[ ( bits >> ( 3 * ( row * 4 + pix ) ) ) & 7 ];

			switch ( nChannelSelect )
			{
				case 0:
					pImPos->r = a;
					pImPos->g = 0;	// Danger...stepping on the other color channels
					pImPos->b = 0;
					SetPixelAlpha( pImPos, 0 );
					break;
				case 1:
					pImPos->g = a;
					break;
				case 2:
					pImPos->b = a;
					break;
				default:
				case 3:
					SetPixelAlpha( pImPos, a );
					break;
			}

			pImPos++;
		}
	}
}

// dimhotepus: Decoders below work on whole 4x4 blocks.  Image which is not multiple of 4 in some dimension
// (before: only < 4 case was handled, 6x6 left right / bottom pixels not decoded in release) is decoded to padded
// temporary image and cropped.  Heap is used as stack allocation can be big for 1 x 16384 images.
template <class CDestPixel, class TDecodeBlocks>
static void DecodeBlockAligned( CDestPixel *dst, int width, int height, TDecodeBlocks &&decodeBlocks )
{
	if ( width <= 0 || height <= 0 )
		return;

	if ( !( width & 3 ) && !( height & 3 ) )
	{
		decodeBlocks( dst, width, height );
		return;
	}

	// round up to the nearest four
	const int alignedWidth = ( width + 3 ) & ~3;
	const int alignedHeight = ( height + 3 ) & ~3;

	std::vector<CDestPixel> padded( static_cast<size_t>( alignedWidth ) * static_cast<size_t>( alignedHeight ) );
	decodeBlocks( padded.data(), alignedWidth, alignedHeight );

	for ( int y = 0; y < height; y++ )
	{
		for ( int x = 0; x < width; x++ )
		{
			dst[ static_cast<size_t>( y ) * width + x ] = padded[ static_cast<size_t>( y ) * alignedWidth + x ];
		}
	}
}

template <class CDestPixel> 
static void ConvertFromDXT1( const uint8 *src, CDestPixel *dst, int width, int height )
{
	static_assert( sizeof( BGRA8888_t ) == 4 );
	static_assert( sizeof( RGBA8888_t ) == 4 );
	static_assert( sizeof( RGB888_t ) == 3 );
	static_assert( sizeof( BGR888_t ) == 3 );
	static_assert( sizeof( BGR565_t ) == 2 );
	static_assert( sizeof( BGRA5551_t ) == 2 );
	static_assert( sizeof( BGRA4444_t ) == 2 );

	DecodeBlockAligned( dst, width, height, [src]( CDestPixel *pDst, int w, int h )
	{
		const int xblocks = w >> 2;
		const int yblocks = h >> 2;

		BGRA8888_t col_0, col_1, col_2, col_3;
		word wrdDummy;

		for ( int j = 0; j < yblocks; j++ )
		{
			for ( int i = 0; i < xblocks; i++ )
			{
				// 8 bytes per block
				// dimhotepus: size_t math, as int offsets overflow on huge images.
				const auto *pBlock = reinterpret_cast<const DXTColBlock *>( src + ( static_cast<size_t>( j ) * xblocks + i ) * 8 );

				GetColorBlockColorsBGRA8888( pBlock, &col_0, &col_1, &col_2, &col_3, wrdDummy );

				// now decode the color block into the bitmap bits
				// inline func:
				CDestPixel *pDstScan = pDst + static_cast<size_t>( i ) * 4 + static_cast<size_t>( j ) * 4 * w;
				DecodeColorBlock<CDestPixel>( pDstScan, pBlock, w, &col_0, &col_1, &col_2, &col_3 );
			}
		}
	} );
}

template <class CDestPixel> 
static void ConvertFromDXT5( const uint8 *src, CDestPixel *dst, int width, int height )
{
	DecodeBlockAligned( dst, width, height, [src]( CDestPixel *pDst, int w, int h )
	{
		const int xblocks = w >> 2;
		const int yblocks = h >> 2;

		BGRA8888_t col_0, col_1, col_2, col_3;
		word wrd;

		for ( int j = 0; j < yblocks; j++ )
		{
			for ( int i = 0; i < xblocks; i++ )
			{
				// 16 bytes per block: 8 for alpha, 8 for color
				const uint8 *pBlockBytes = src + ( static_cast<size_t>( j ) * xblocks + i ) * 16;

				const auto *pAlphaBlock = reinterpret_cast<const DXTAlphaBlock3BitLinear *>( pBlockBytes );
				const auto *pBlock = reinterpret_cast<const DXTColBlock *>( pBlockBytes + 8 );

				// dimhotepus: DXT5 color is always in four color mode.
				GetColorBlockColorsBGRA8888( pBlock, &col_0, &col_1, &col_2, &col_3, wrd, true );

				CDestPixel *pDstScan = pDst + static_cast<size_t>( i ) * 4 + static_cast<size_t>( j ) * 4 * w;

				// Decode the color block into the bitmap bits
				// inline func:
				DecodeColorBlock<CDestPixel>( pDstScan, pBlock, w, &col_0, &col_1, &col_2, &col_3 );

				// Overwrite the previous alpha bits with the alpha block
				//  info
				DecodeAlpha3BitLinear( pDstScan, pAlphaBlock, w );
			}
		}
	} );
}

template <class CDestPixel> 
static void ConvertFromDXT5IgnoreAlpha( const uint8 *src, CDestPixel *dst, int width, int height )
{
	DecodeBlockAligned( dst, width, height, [src]( CDestPixel *pDst, int w, int h )
	{
		const int xblocks = w >> 2;
		const int yblocks = h >> 2;

		BGRA8888_t col_0, col_1, col_2, col_3;
		word wrd;

		for ( int j = 0; j < yblocks; j++ )
		{
			for ( int i = 0; i < xblocks; i++ )
			{
				// 16 bytes per block: 8 for alpha (skipped), 8 for color
				const uint8 *pBlockBytes = src + ( static_cast<size_t>( j ) * xblocks + i ) * 16;
				const auto *pBlock = reinterpret_cast<const DXTColBlock *>( pBlockBytes + 8 );

				// dimhotepus: DXT5 color is always in four color mode.
				GetColorBlockColorsBGRA8888( pBlock, &col_0, &col_1, &col_2, &col_3, wrd, true );

				CDestPixel *pDstScan = pDst + static_cast<size_t>( i ) * 4 + static_cast<size_t>( j ) * 4 * w;

				// Decode the color block into the bitmap bits
				// inline func:
				DecodeColorBlock<CDestPixel>( pDstScan, pBlock, w, &col_0, &col_1, &col_2, &col_3 );
			}
		}
	} );
}


template <class CDestPixel> 
static void ConvertFromATIxN( const uint8 *src, CDestPixel *dst, int width, int height, bool bATI2N )
{
	DecodeBlockAligned( dst, width, height, [src, bATI2N]( CDestPixel *pDst, int w, int h )
	{
		const int xblocks = w >> 2;
		const int yblocks = h >> 2;

		// 8 bytes per channel block, ATI2N has 2 blocks: 1 for x, 1 for y
		const size_t nBytesPerBlock = bATI2N ? 16 : 8;

		for ( int j = 0; j < yblocks; j++ )
		{
			for ( int i = 0; i < xblocks; i++ )
			{
				const uint8 *pBlockBytes = src + ( static_cast<size_t>( j ) * xblocks + i ) * nBytesPerBlock;

				CDestPixel *pDstScan = pDst + static_cast<size_t>( i ) * 4 + static_cast<size_t>( j ) * 4 * w;
				DecodeAlpha3BitLinear( pDstScan, reinterpret_cast<const DXTAlphaBlock3BitLinear *>( pBlockBytes ), w, 0 );

				if ( bATI2N )
				{
					DecodeAlpha3BitLinear( pDstScan, reinterpret_cast<const DXTAlphaBlock3BitLinear *>( pBlockBytes + 8 ), w, 1 );
				}
			}
		}
	} );
}

// dimhotepus: Exclude nvtc as proprietary.
#if !defined(NO_NVTC)
static dword GetDXTCEncodeType( ImageFormat imageFormat )
{
	switch ( imageFormat )
	{
	case IMAGE_FORMAT_DXT1:
		return S3TC_ENCODE_RGB_FULL;
	case IMAGE_FORMAT_DXT1_ONEBITALPHA:
		return S3TC_ENCODE_RGB_FULL | S3TC_ENCODE_RGB_ALPHA_COMPARE;
	case IMAGE_FORMAT_DXT3:
		return S3TC_ENCODE_RGB_FULL | S3TC_ENCODE_ALPHA_EXPLICIT;
	case IMAGE_FORMAT_DXT5:
		return S3TC_ENCODE_RGB_FULL | S3TC_ENCODE_ALPHA_INTERPOLATED;
	default:
		return 0;
	}
}
#endif

// Convert RGBA input to ATI1N or ATI2N format
bool ConvertToATIxN(  [[maybe_unused]] const uint8 *src, [[maybe_unused]]ImageFormat srcImageFormat,
					  [[maybe_unused]] uint8 *dst, [[maybe_unused]] ImageFormat dstImageFormat,
					  [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] int srcStride, [[maybe_unused]] int dstStride )
{
// dimhotepus: Exclude ATI compress as proprietary.
#if !defined(NO_ATI_COMPRESS) && !defined(POSIX)

	// from rgb(a) to ATIxN
	if( srcStride != 0 || dstStride != 0 )
		return false;

	// If we're not the right format for the ATI compressor, we bail
	if ( srcImageFormat != IMAGE_FORMAT_ARGB8888 )
		return false;

	// dimhotepus: Nothing to compress / bad size.
	if ( width <= 0 || height <= 0 )
		return false;

	// Define source image parameters and copy the bits into buffer
	// dimhotepus: Zero init, as the struct has more fields (block sizes) than we set.
	ATI_TC_Texture srcTexture = {};
	srcTexture.dwSize = sizeof( srcTexture );
	srcTexture.dwWidth = width;
	srcTexture.dwHeight = height;
	srcTexture.dwPitch = srcStride;
	srcTexture.format = ATI_TC_FORMAT_ARGB_8888;
	srcTexture.dwDataSize = ATI_TC_CalculateBufferSize( &srcTexture );
	srcTexture.pData = (ATI_TC_BYTE*) malloc( srcTexture.dwDataSize );
	// dimhotepus: Don't crash on out of memory.
	if ( !srcTexture.pData )
		return false;
	memcpy( srcTexture.pData, src, srcTexture.dwDataSize );

	ATI_TC_Texture destTexture = {};
	destTexture.dwSize = sizeof( destTexture );
	destTexture.dwWidth = width;
	destTexture.dwHeight = height;
	destTexture.dwPitch = 0;
	destTexture.format = dstImageFormat == IMAGE_FORMAT_ATI2N ? ATI_TC_FORMAT_ATI2N : ATI_TC_FORMAT_ATI1N;	// Assume it can only be one of these two...
	destTexture.dwDataSize = ATI_TC_CalculateBufferSize( &destTexture );
	destTexture.pData = (ATI_TC_BYTE*) dst;

	ATI_TC_ERROR errATI = ATI_TC_ConvertTexture( &srcTexture, &destTexture, nullptr, nullptr, nullptr, nullptr );		// Convert it!
	
	free( srcTexture.pData );

	if ( errATI != ATI_TC_OK )
		return false;

	return true;
#else
	Assert( 0 );
	return false;
#endif
}


bool ConvertToDXTLegacy(  [[maybe_unused]] const uint8 *src, [[maybe_unused]] ImageFormat srcImageFormat,
 						  [[maybe_unused]] uint8 *dst, [[maybe_unused]] ImageFormat dstImageFormat, 
					      [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] int srcStride, [[maybe_unused]] int dstStride )
{
// dimhotepus: Exclude nvtc as proprietary.
#if !defined(NO_NVTC) && !defined( _X360 ) && !defined( POSIX )
	// from rgb(a) to dxtN
	if( srcStride != 0 || dstStride != 0 )
		return false;

	DDSURFACEDESC descIn = {};
	DDSURFACEDESC descOut = {};
	float weight[3] = {0.3086f, 0.6094f, 0.0820f};
	dword dwEncodeType = GetDXTCEncodeType( dstImageFormat );
	
	// Setup descIn
	descIn.dwSize = sizeof(descIn);
	descIn.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_LPSURFACE | 
		/*DDSD_PITCH | */ DDSD_PIXELFORMAT;
	descIn.dwWidth = width;
	descIn.dwHeight = height;
	descIn.lPitch = width * ImageLoader::SizeInBytes( srcImageFormat );
	descIn.lpSurface = ( LPVOID *) src;
	descIn.ddpfPixelFormat.dwSize = sizeof( DDPIXELFORMAT );
	switch ( srcImageFormat )
	{
	case IMAGE_FORMAT_RGBA8888:
		descIn.ddpfPixelFormat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
		descIn.ddpfPixelFormat.dwRGBBitCount = 32;
		descIn.ddpfPixelFormat.dwRBitMask = 0x0000ff;
		descIn.ddpfPixelFormat.dwGBitMask = 0x00ff00;
		descIn.ddpfPixelFormat.dwBBitMask = 0xff0000;
		// must set this anyway or S3TC will lock up!!!
		descIn.ddpfPixelFormat.dwRGBAlphaBitMask = 0xff000000;
		break;
	case IMAGE_FORMAT_BGRA8888:
		descIn.ddpfPixelFormat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
		descIn.ddpfPixelFormat.dwRGBBitCount = 32;
		descIn.ddpfPixelFormat.dwRBitMask = 0xFF0000;
		descIn.ddpfPixelFormat.dwGBitMask = 0x00ff00;
		descIn.ddpfPixelFormat.dwBBitMask = 0x0000FF;
		// must set this anyway or S3TC will lock up!!!
		descIn.ddpfPixelFormat.dwRGBAlphaBitMask = 0xff000000;
		break;
	case IMAGE_FORMAT_BGRX8888:
		descIn.ddpfPixelFormat.dwFlags = DDPF_RGB;
		descIn.ddpfPixelFormat.dwRGBBitCount = 32;
		descIn.ddpfPixelFormat.dwRBitMask = 0xFF0000;
		descIn.ddpfPixelFormat.dwGBitMask = 0x00ff00;
		descIn.ddpfPixelFormat.dwBBitMask = 0x0000FF;
		// must set this anyway or S3TC will lock up!!!
		descIn.ddpfPixelFormat.dwRGBAlphaBitMask = 0xff000000;
		break;
	case IMAGE_FORMAT_RGB888:
		descIn.ddpfPixelFormat.dwFlags = DDPF_RGB;
		descIn.ddpfPixelFormat.dwRGBBitCount = 24;
		descIn.ddpfPixelFormat.dwRBitMask = 0x0000ff;
		descIn.ddpfPixelFormat.dwGBitMask = 0x00ff00;
		descIn.ddpfPixelFormat.dwBBitMask = 0xff0000;
		descIn.ddpfPixelFormat.dwRGBAlphaBitMask = 0xff000000;
		break;
	default:
		return false;
	}
	
	// Setup descOut
	descOut.dwSize = sizeof( descOut );
	
	// Encode the texture
	S3TCencode( &descIn, nullptr, &descOut, dst, dwEncodeType, weight );
	return true;
#else
	Assert( 0 );
	return false;
#endif
}

template < typename SrcPixel_t >
void CompressSTB( uint8 *pDstBytes, ImageFormat dstFmt, const uint8 *pSrcBytes, int nWidth, int nHeight )
{
	const bool cbWriteAlpha = ( dstFmt == IMAGE_FORMAT_DXT5 );
	const uint32 cDstStride = ( dstFmt == IMAGE_FORMAT_DXT1 ) ? 8 : 16;

	const uint32 cPixX = (uint32) nWidth;
	const uint32 cPixY = (uint32) nHeight;
	// dimhotepus: size_t, as uint32 pitch * row overflows for images > 4 GB.
	const size_t cSrcPitch = static_cast<size_t>( cPixX ) * sizeof( SrcPixel_t );
	const uint32 cLastX = cPixX - 1;
	const uint32 cLastY = cPixY - 1;

	// STB always takes blocks as 4x4 of RGBA8888_t
	RGBA8888_t srcBlock[16] = { {},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{} };
	// dimhotepus: Source is read only.
	const SrcPixel_t* pSrcs[4] = {};

	for ( uint32 y = 0; y < cPixY; y += 4 ) 
	{
		// This handles clamping for cPixY % 4 != 0
		pSrcs[ 0 ] = ( const SrcPixel_t* ) ( pSrcBytes + cSrcPitch * Min( y + 0, cLastY ) );
		pSrcs[ 1 ] = ( const SrcPixel_t* ) ( pSrcBytes + cSrcPitch * Min( y + 1, cLastY ) );
		pSrcs[ 2 ] = ( const SrcPixel_t* ) ( pSrcBytes + cSrcPitch * Min( y + 2, cLastY ) );
		pSrcs[ 3 ] = ( const SrcPixel_t* ) ( pSrcBytes + cSrcPitch * Min( y + 3, cLastY ) );

		for ( uint32 x = 0; x < cPixX; x += 4 ) 
		{
			for ( uint32 i = 0; i < 4; ++i )
			{
				uint32 offsetX = Min( x + i, cLastX );
				srcBlock[ 0 + i ] =  pSrcs[ 0 ][ offsetX ];
				srcBlock[ 4 + i ] =  pSrcs[ 1 ][ offsetX ];
				srcBlock[ 8 + i ] =  pSrcs[ 2 ][ offsetX ];
				srcBlock[ 12 + i ] = pSrcs[ 3 ][ offsetX ];
			}

			stb_compress_dxt_block( pDstBytes, ( const uint8* ) srcBlock, cbWriteAlpha, STB_DXT_NORMAL );
			pDstBytes += cDstStride;
		}
	}
}

inline ImageFormat GetTrueImageFormat( ImageFormat fmt )
{
	switch ( fmt )
	{
	case IMAGE_FORMAT_DXT1_RUNTIME:
		return IMAGE_FORMAT_DXT1;
	case IMAGE_FORMAT_DXT5_RUNTIME:
		return IMAGE_FORMAT_DXT5;
	default: /* expected */
		break;
	}

	return fmt;
}

bool ConvertToDXTRuntime( const uint8 *src, ImageFormat srcImageFormat,
						  uint8 *dst, ImageFormat dstImageFormat,
						  int width, int height, int srcStride, int dstStride )
{
	if ( srcStride != 0 || dstStride != 0 )
		return false;

	// dimhotepus: Negative size converted to uint32 is huge => out of bounds.
	if ( width <= 0 || height <= 0 )
		return false;

	dstImageFormat = GetTrueImageFormat( dstImageFormat );

	switch ( srcImageFormat )
	{
		case IMAGE_FORMAT_RGBA8888: CompressSTB<RGBA8888_t>( dst, dstImageFormat, src, width, height ); return true;
		case IMAGE_FORMAT_RGB888:   CompressSTB<RGB888_t>  ( dst, dstImageFormat, src, width, height ); return true;
		case IMAGE_FORMAT_BGRA8888: CompressSTB<BGRA8888_t>( dst, dstImageFormat, src, width, height ); return true;
		case IMAGE_FORMAT_BGRX8888: CompressSTB<BGRX8888_t>( dst, dstImageFormat, src, width, height ); return true;
		default:
			AssertMsg( false, "Unexpected format here, wtf." );
			break;
	}

	return false;
}

bool ConvertToDXT( const uint8 *src, ImageFormat srcImageFormat,
 				   uint8 *dst, ImageFormat dstImageFormat, 
				   int width, int height, int srcStride, int dstStride )
{
	// The STB compressor (the new compressor) is faster and higher quality in most cases, and has less error overall
	// than the S3TC compressor. So use it by default, unless we're working with a format that STB doesn't support.
	bool bUseNewCompressor = dstImageFormat != IMAGE_FORMAT_DXT1_ONEBITALPHA 
		                  && dstImageFormat != IMAGE_FORMAT_DXT3;

//	bool bUseNewCompressor = dstImageFormat == IMAGE_FORMAT_DXT1_RUNTIME 
//		                  || dstImageFormat == IMAGE_FORMAT_DXT5_RUNTIME;

	if ( bUseNewCompressor )
		return ConvertToDXTRuntime( src, srcImageFormat, dst, dstImageFormat, width, height, srcStride, dstStride );

	return ConvertToDXTLegacy( src, srcImageFormat, dst, dstImageFormat, width, height, srcStride, dstStride );
}

// HDRFIXME: This assumes that the 16-bit integer values are 4.12 fixed-point.
void ConvertImageFormat_RGBA16161616_To_RGB323232F( unsigned short *pSrcImage, float *pDstImage, int width, int height )
{
	// dimhotepus: size_t pixel count, int width * height * 4 overflows.
	if ( width <= 0 || height <= 0 )
		return;

	const size_t numPixels = static_cast<size_t>( width ) * height;
	constexpr float scale = 1.0f / static_cast<float>( 1 << 12 );

	for ( size_t i = 0; i < numPixels; i++ )
	{
		const unsigned short *pSrcScan = pSrcImage + i * 4;
		float *pDstScan = pDstImage + i * 3;

		pDstScan[0] = static_cast<float>( pSrcScan[0] ) * scale;
		pDstScan[1] = static_cast<float>( pSrcScan[1] ) * scale;
		pDstScan[2] = static_cast<float>( pSrcScan[2] ) * scale;
	}
}

// HDRFIXME: This assumes that the 16-bit integer values are 4.12 fixed-point.
void ConvertImageFormat_RGB323232F_To_RGBA16161616( float *pSrcImage, unsigned short *pDstImage, int width, int height )
{
	if ( width <= 0 || height <= 0 )
		return;

	const size_t numPixels = static_cast<size_t>( width ) * height;
	constexpr float scale = static_cast<float>( 1 << 12 );

	for ( size_t i = 0; i < numPixels; i++ )
	{
		const float *pSrcScan = pSrcImage + i * 3;
		unsigned short *pDstScan = pDstImage + i * 4;

		// dimhotepus: Negative float to unsigned short conversion is UB, clamp to [0, 65535].
		pDstScan[0] = static_cast<unsigned short>( max( 0.0f, min( 65535.0f, pSrcScan[0] * scale ) ) );
		pDstScan[1] = static_cast<unsigned short>( max( 0.0f, min( 65535.0f, pSrcScan[1] * scale ) ) );
		pDstScan[2] = static_cast<unsigned short>( max( 0.0f, min( 65535.0f, pSrcScan[2] * scale ) ) );
		pDstScan[3] = 65535;
	}
}

void ConvertImageFormat_RGBA16161616F_To_RGB323232F( float16 *pSrcImage, float *pDstImage, int width, int height )
{
	if ( width <= 0 || height <= 0 )
		return;

	const size_t numPixels = static_cast<size_t>( width ) * height;

	for ( size_t i = 0; i < numPixels; i++ )
	{
		const float16 *pSrcScan = pSrcImage + i * 4;
		float *pDstScan = pDstImage + i * 3;

		pDstScan[0] = pSrcScan[0].GetFloat();
		pDstScan[1] = pSrcScan[1].GetFloat();
		pDstScan[2] = pSrcScan[2].GetFloat();
	}
}

void ConvertImageFormat_RGBA16161616F_To_RGBA323232F( float16 *pSrcImage, float *pDstImage, int width, int height , size_t src_stride)
{
	if ( width <= 0 || height <= 0 )
		return;

	// dimhotepus: Stride 0 means tightly packed (ConvertImageFormat passes default 0 stride).  It was used as is,
	// so every row was read from the first source row.
	if ( src_stride == 0 )
	{
		src_stride = static_cast<size_t>( width ) * 4 * sizeof( float16 );
	}

	const size_t s_stride = src_stride / sizeof( float16 );
	for ( int y = 0; y < height; y++ )
	{
		float16 const *pSrcScan = pSrcImage;
		float *pDstScan = pDstImage;
		for ( int x = 0; x < width; x++ )
		{
			pDstScan[0] = pSrcScan[0].GetFloat();
			pDstScan[1] = pSrcScan[1].GetFloat();
			pDstScan[2] = pSrcScan[2].GetFloat();
			pDstScan[3] = pSrcScan[3].GetFloat();
			pDstScan += 4;
			pSrcScan += 4;
		}
		pSrcImage += s_stride;
		pDstImage += static_cast<size_t>( 4 ) * width;
	}
}


void ConvertImageFormat_RGB323232F_To_RGBA16161616F( float *pSrcImage, float16 *pDstImage, int width, int height )
{
	if ( width <= 0 || height <= 0 )
		return;

	const size_t numPixels = static_cast<size_t>( width ) * height;

	for ( size_t i = 0; i < numPixels; i++ )
	{
		const float *pSrcScan = pSrcImage + i * 3;
		float16 *pDstScan = pDstImage + i * 4;

		pDstScan[0].SetFloat( pSrcScan[0] );
		pDstScan[1].SetFloat( pSrcScan[1] );
		pDstScan[2].SetFloat( pSrcScan[2] );
		// dimhotepus: Alpha was never written (uninitialized memory).  Opaque.
		pDstScan[3].SetFloat( 1.0f );
	}
}

void ConvertImageFormat_RGB323232F_To_RGBA8888( float *pSrcImage, uint8 *dst, int width, int height )
{
	if ( width <= 0 || height <= 0 )
		return;

	FloatBitMap_t flbm;
	flbm.AllocateRGB( width, height );

	// Set the pixels
	for ( int y = 0; y < height; ++ y )
	{
		for ( int x = 0; x < width; ++ x )
		{
			float *pf = &pSrcImage[ 3 * ( static_cast<size_t>( y ) * width + x ) ];
			PixRGBAF fpix;
			fpix.Red = pf[0];
			fpix.Green = pf[1];
			fpix.Blue = pf[2];
			fpix.Alpha = 0.f;
			
			flbm.WritePixelRGBAF( x, y, fpix );
		}
	}
	// memcpy( flbm.RGBAData, pSrcImage, width * height * 4 );
	
	flbm.CompressTo8Bits( 8.0 );

	// Now, get the pixels
	for ( int y = 0; y < height; ++ y )
	{
		for ( int x = 0; x < width; ++ x )
		{
			PixRGBAF fpix = flbm.PixelRGBAF( x, y );
			PixRGBA8 pix8 = PixRGBAF_to_8( fpix );

			uint8 *pch = &dst[ 4 * ( static_cast<size_t>( y ) * width + x ) ];

			pch[0] = pix8.Red;
			pch[1] = pix8.Green;
			pch[2] = pix8.Blue;
			pch[3] = pix8.Alpha;
		}
	}
}

void ConvertImageFormat_RGB323232F_To_BGRA8888( float *pSrcImage, uint8 *dst, int width, int height )
{
	if ( width <= 0 || height <= 0 )
		return;

	FloatBitMap_t flbm;
	flbm.AllocateRGB( width, height );

	// Set the pixels
	for ( int y = 0; y < height; ++ y )
	{
		for ( int x = 0; x < width; ++ x )
		{
			float *pf = &pSrcImage[ 3 * ( static_cast<size_t>( y ) * width + x ) ];
			PixRGBAF fpix;
			fpix.Red = pf[0];
			fpix.Green = pf[1];
			fpix.Blue = pf[2];
			fpix.Alpha = 0.f;

			flbm.WritePixelRGBAF( x, y, fpix );
		}
	}
	// memcpy( flbm.RGBAData, pSrcImage, width * height * 4 );

	flbm.CompressTo8Bits( 8.0 );

	// Now, get the pixels
	for ( int y = 0; y < height; ++ y )
	{
		for ( int x = 0; x < width; ++ x )
		{
			PixRGBAF fpix = flbm.PixelRGBAF( x, y );
			PixRGBA8 pix8 = PixRGBAF_to_8( fpix );

			uint8 *pch = &dst[ 4 * ( static_cast<size_t>( y ) * width + x ) ];

			pch[0] = pix8.Blue;
			pch[1] = pix8.Green;
			pch[2] = pix8.Red;
			pch[3] = pix8.Alpha;
		}
	}
}

// HDRFIXME: This assumes that the 16-bit integer values are 4.12 fixed-point.
void ConvertImageFormat_RGBA16161616_To_RGBA16161616F( unsigned short *pSrcImage, float *pDstImage, int width, int height )
{
	if ( width <= 0 || height <= 0 )
		return;

	const size_t numValues = static_cast<size_t>( width ) * height * 4;
	auto *pDstScan = ( float16 * )pDstImage;

	// dimhotepus: Well, correct scale is 1 / 2^12 so it can convert to and from int
	// dimhotepus: But we have HDR overbright multiplier 16 which adjust value on shader level.
	constexpr float scale = 1.0f / static_cast<float>( 1 << 16 );
	for ( size_t i = 0; i < numValues; i++ )
	{
		pDstScan[i].SetFloat( pSrcImage[i] * scale );
	}
}

void ConvertImageFormat_RGBA16161616F_To_RGBA16161616( float16 *pSrcImage, unsigned short *pDstImage, int width, int height )
{
	if ( width <= 0 || height <= 0 )
		return;

	const size_t numValues = static_cast<size_t>( width ) * height * 4;
	constexpr float kScale{ 1 << 12 };
	for ( size_t i = 0; i < numValues; i++ )
	{
		float val = pSrcImage[i].GetFloat();
		val *= kScale;
		val = max( val, 0.f );
		val = min( val, 65535.0f );
		pDstImage[i] = ( unsigned short )val;
	}
}

bool ConvertImageFormat( const uint8 *src, ImageFormat srcImageFormat,
 					     uint8 *dst, ImageFormat dstImageFormat, 
						 int width, int height, int srcStride, int dstStride )
{
	// dimhotepus: DXT1_RUNTIME / DXT5_RUNTIME store plain DXT1 / DXT5 blocks.  Decoders below know only DXT1 / DXT5
	// sources, so decode runtime formats as them (before conversion silently failed).
	if ( !IsCompressed( dstImageFormat ) )
	{
		srcImageFormat = GetTrueImageFormat( srcImageFormat );
	}

	// HDRFIXME: WE NEED A BIGGER INTERMEDIATE FORMAT!!!!!
	if ( srcImageFormat == IMAGE_FORMAT_RGBA16161616 )
	{
		if ( dstImageFormat == IMAGE_FORMAT_RGB323232F )
		{
			Assert( srcStride == 0 && dstStride == 0 );
			ConvertImageFormat_RGBA16161616_To_RGB323232F( ( unsigned short * )src, ( float * )dst, width, height );
			return true;
		}
		if ( dstImageFormat == IMAGE_FORMAT_RGBA16161616F )
		{
			Assert( srcStride == 0 && dstStride == 0 );
			ConvertImageFormat_RGBA16161616_To_RGBA16161616F( ( unsigned short * )src, ( float * )dst, width, height );
			return true;
		}
	}
	else if ( srcImageFormat == IMAGE_FORMAT_RGBA16161616F )
	{	
		if ( dstImageFormat == IMAGE_FORMAT_RGB323232F )
		{
			Assert( srcStride == 0 && dstStride == 0 );
			ConvertImageFormat_RGBA16161616F_To_RGB323232F( ( float16 * )src, ( float * )dst, width, height );
			return true;
		}
		if ( dstImageFormat == IMAGE_FORMAT_RGBA32323232F )
		{
			Assert( dstStride == 0 );
			ConvertImageFormat_RGBA16161616F_To_RGBA323232F( ( float16 * )src, ( float * )dst, width, height, srcStride );
			return true;
		}
		if ( dstImageFormat == IMAGE_FORMAT_RGBA16161616 )
		{
			Assert( srcStride == 0 && dstStride == 0 );
			ConvertImageFormat_RGBA16161616F_To_RGBA16161616( ( float16 * )src, ( unsigned short * )dst, width, height );
			return true;
		}
	}
	else if ( srcImageFormat == IMAGE_FORMAT_RGB323232F )
	{
		if ( dstImageFormat == IMAGE_FORMAT_RGBA16161616 )
		{
			Assert( srcStride == 0 && dstStride == 0 );
			ConvertImageFormat_RGB323232F_To_RGBA16161616( ( float * )src, ( unsigned short * )dst, width, height );
			return true;
		}
		if ( dstImageFormat == IMAGE_FORMAT_RGBA16161616F )
		{
			Assert( srcStride == 0 && dstStride == 0 );
			ConvertImageFormat_RGB323232F_To_RGBA16161616F( ( float * )src, ( float16 * )dst, width, height );
			return true;
		}
		if ( dstImageFormat == IMAGE_FORMAT_RGBA8888 )
		{
			Assert( srcStride == 0 );
			ConvertImageFormat_RGB323232F_To_RGBA8888( ( float * )src, dst, width, height );
			return true;
		}
		if ( dstImageFormat == IMAGE_FORMAT_BGRA8888 )
		{
			Assert( srcStride == 0 );
			ConvertImageFormat_RGB323232F_To_BGRA8888( ( float * )src, dst, width, height );
			return true;
		}
	}
	
	// Fast path for just copying a compressed texture
	// dimhotepus: Any compressed format (DXT1_ONEBITALPHA was missed: nothing was copied, as SizeInBytes is 0 for it).
	if ( ( IsCompressed( dstImageFormat ) && srcImageFormat == dstImageFormat ) ||
		 ( dstImageFormat == IMAGE_FORMAT_DXT5 && srcImageFormat == IMAGE_FORMAT_DXT5_RUNTIME ) ||
		 ( dstImageFormat == IMAGE_FORMAT_DXT1 && srcImageFormat == IMAGE_FORMAT_DXT1_RUNTIME ) ||
		 ( dstImageFormat == IMAGE_FORMAT_DXT5_RUNTIME && srcImageFormat == IMAGE_FORMAT_DXT5 ) ||
		 ( dstImageFormat == IMAGE_FORMAT_DXT1_RUNTIME && srcImageFormat == IMAGE_FORMAT_DXT1 ) )
	{
		// Fast path for compressed textures . . stride doesn't make as much sense.
//		Assert( srcStride == 0 && dstStride == 0 );

		intp memRequired = GetMemRequired( width, height, 1, srcImageFormat, false );
		// dimhotepus: Copy to itself is UB.
		if ( src != dst )
		{
			memcpy( dst, src, static_cast<size_t>( memRequired ) );
		}
		return true;
	}
	else if ( ( srcImageFormat == IMAGE_FORMAT_RGBA8888 ||		
			   srcImageFormat == IMAGE_FORMAT_RGB888    ||														// RGBA source
			   srcImageFormat == IMAGE_FORMAT_BGRA8888  ||														//
			   srcImageFormat == IMAGE_FORMAT_BGRX8888 ) &&	   													// and
			 ( dstImageFormat == IMAGE_FORMAT_DXT1  ||															//
			   dstImageFormat == IMAGE_FORMAT_DXT3  ||	   														// DXT compressed dest
			   dstImageFormat == IMAGE_FORMAT_DXT5  ||
			   dstImageFormat == IMAGE_FORMAT_DXT1_RUNTIME ||
			   dstImageFormat == IMAGE_FORMAT_DXT5_RUNTIME ) )
	{
		return ConvertToDXT( src, srcImageFormat, dst, dstImageFormat, width, height, srcStride, dstStride );
	}
	else if ( ( srcImageFormat == IMAGE_FORMAT_ARGB8888 ) &&									 				// RGBA source and
			  ( dstImageFormat == IMAGE_FORMAT_ATI1N    || dstImageFormat == IMAGE_FORMAT_ATI2N ) )				// ATI compressed dest
	{
		return ConvertToATIxN( src, srcImageFormat, dst, dstImageFormat, width, height, srcStride, dstStride );
	}
	else if ( ( dstImageFormat == IMAGE_FORMAT_RGBA8888 ||
			   dstImageFormat == IMAGE_FORMAT_BGRX8888 ||
			   dstImageFormat == IMAGE_FORMAT_BGRA8888 ||
			   dstImageFormat == IMAGE_FORMAT_BGRA4444 ||
			   dstImageFormat == IMAGE_FORMAT_BGRA5551 ||
			   dstImageFormat == IMAGE_FORMAT_BGRX5551 ||
			   dstImageFormat == IMAGE_FORMAT_BGR565 ||
			   dstImageFormat == IMAGE_FORMAT_BGR888 ||
			   dstImageFormat == IMAGE_FORMAT_RGB888 ) &&
			 ( srcImageFormat == IMAGE_FORMAT_DXT1  ||
			   srcImageFormat == IMAGE_FORMAT_DXT3  ||
			   srcImageFormat == IMAGE_FORMAT_DXT5  ||
			   srcImageFormat == IMAGE_FORMAT_ATI1N ||
			   srcImageFormat == IMAGE_FORMAT_ATI2N ) )
	{
		// from dxtN to rgb(a)
		if ( srcStride != 0 || dstStride != 0 )
		{
			return false;
		}
		if ( srcImageFormat == IMAGE_FORMAT_DXT1 )
		{
			if ( dstImageFormat == IMAGE_FORMAT_RGBA8888 )
			{
				ConvertFromDXT1( src, ( RGBA8888_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_BGRA8888 ||
			    dstImageFormat == IMAGE_FORMAT_BGRX8888 )
			{
				ConvertFromDXT1( src, ( BGRA8888_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_RGB888 )
			{
				ConvertFromDXT1( src, ( RGB888_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_BGR888 )
			{
				ConvertFromDXT1( src, ( BGR888_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_BGR565 )
			{
				ConvertFromDXT1( src, ( BGR565_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_BGRA5551 ||
				dstImageFormat == IMAGE_FORMAT_BGRX5551 )
			{
				ConvertFromDXT1( src, ( BGRA5551_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_BGRA4444 ) //-V547
			{
				ConvertFromDXT1( src, ( BGRA4444_t * )dst, width, height );
				return true;
			}
		}
		else if ( srcImageFormat == IMAGE_FORMAT_ATI2N )
		{
			if ( dstImageFormat == IMAGE_FORMAT_BGRA8888 )
			{
				ConvertFromATIxN( src, ( BGRA8888_t * )dst, width, height, true );
				return true;
			}
		}
		else if ( srcImageFormat == IMAGE_FORMAT_ATI1N )
		{
			if ( dstImageFormat == IMAGE_FORMAT_BGRA8888 )
			{
				ConvertFromATIxN( src, ( BGRA8888_t * )dst, width, height, false );
				return true;
			}
		}
		else if ( srcImageFormat == IMAGE_FORMAT_DXT5 )
		{
			if ( dstImageFormat == IMAGE_FORMAT_RGBA8888 )
			{
				ConvertFromDXT5( src, ( RGBA8888_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_BGRA8888 ||
			    dstImageFormat == IMAGE_FORMAT_BGRX8888 )
			{
				ConvertFromDXT5( src, ( BGRA8888_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_RGB888 )
			{
				ConvertFromDXT5IgnoreAlpha( src, ( RGB888_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_BGR888 )
			{
				ConvertFromDXT5IgnoreAlpha( src, ( BGR888_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_BGR565 )
			{
				ConvertFromDXT5IgnoreAlpha( src, ( BGR565_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_BGRA5551 ||
				dstImageFormat == IMAGE_FORMAT_BGRX5551 )
			{
				ConvertFromDXT5( src, ( BGRA5551_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_BGRA4444 ) //-V547
			{
				ConvertFromDXT5( src, ( BGRA4444_t * )dst, width, height );
				return true;
			}
		}
		return false;
	}
	else if ( dstImageFormat == IMAGE_FORMAT_DXT1  ||
			  dstImageFormat == IMAGE_FORMAT_DXT3  ||
			  dstImageFormat == IMAGE_FORMAT_DXT5  ||
			  dstImageFormat == IMAGE_FORMAT_ATI1N ||
			  dstImageFormat == IMAGE_FORMAT_ATI2N ||
			  srcImageFormat == IMAGE_FORMAT_DXT1  ||
			  srcImageFormat == IMAGE_FORMAT_DXT3  ||
			  srcImageFormat == IMAGE_FORMAT_DXT5  ||
			  srcImageFormat == IMAGE_FORMAT_ATI1N ||
			  srcImageFormat == IMAGE_FORMAT_ATI2N )
	{
		// DxtN to DxtN
		Assert( IsPC() );
		return false;
	}
	else
	{
		// uncompressed textures
		int line;
		int srcPixelSize = SizeInBytes(srcImageFormat);
		int dstPixelSize = SizeInBytes(dstImageFormat);

		// dimhotepus: Compressed (not handled above) and unknown formats have no pixel size, can't convert them
		// (before: same format copied 0 bytes and reported success).
		if ( srcPixelSize <= 0 || dstPixelSize <= 0 )
		{
			return false;
		}
		if ( width < 0 || height < 0 )
		{
			return false;
		}
		if ( width == 0 || height == 0 )
		{
			return true;
		}
		
		if ( srcStride == 0 )
		{
			srcStride = srcPixelSize * width;
		}
		if ( dstStride == 0 )
		{
			dstStride = dstPixelSize * width;
		}
		
		// Fast path...
		if( ( srcImageFormat == dstImageFormat ) || 
			((srcImageFormat == IMAGE_FORMAT_BGRA8888) && (dstImageFormat == IMAGE_FORMAT_BGRX8888)) )
		{
			if ( ( IsX360() ||
				// dimhotepus: CS:GO backport. IMAGE_FORMAT_RGBA32323232F -> IMAGE_FORMAT_RGBA32323232F
				   srcImageFormat == IMAGE_FORMAT_RGBA32323232F ||
				// dimhotepus: CS:GO backport. IMAGE_FORMAT_RGBA8888 -> IMAGE_FORMAT_RGBA8888
				   srcImageFormat == IMAGE_FORMAT_RGBA8888 ||
				// dimhotepus: CS:GO backport. IMAGE_FORMAT_BGRA8888 -> IMAGE_FORMAT_BGRX8888
				   srcImageFormat == IMAGE_FORMAT_BGRA8888 ) &&
				 srcStride == dstStride && width * srcPixelSize == srcStride )
			{
				// fastest path
				// dimhotepus: size_t math + memmove, as src can be the same as dst.
				memmove( dst, src, static_cast<size_t>( height ) * srcStride );
				return true;
			}

			for ( line = 0; line < height; ++line )
			{
				memmove( dst, src, static_cast<size_t>( width ) * srcPixelSize );
				dst += dstStride;
				src += srcStride;
			}
			return true;
		}
		
		// format conversion
		// dimhotepus: Heap instead of stack allocation, width can be big.
		std::vector<uint8> lineBufStorage( static_cast<size_t>( width ) * 4 );
		uint8 *lineBufRGBA8888 = lineBufStorage.data();
		
		UserFormatToRGBA8888Func_t userFormatToRGBA8888Func;
		RGBA8888ToUserFormatFunc_t RGBA8888ToUserFormatFunc;
		
		userFormatToRGBA8888Func = GetUserFormatToRGBA8888Func_t( srcImageFormat );
		RGBA8888ToUserFormatFunc = GetRGBA8888ToUserFormatFunc_t( dstImageFormat );
		
		if ( !userFormatToRGBA8888Func || !RGBA8888ToUserFormatFunc )
		{
			return false;
		}

		for ( line = 0; line < height; line++ )
		{
			userFormatToRGBA8888Func( src + static_cast<ptrdiff_t>( line ) * srcStride, lineBufRGBA8888, width );
			RGBA8888ToUserFormatFunc( lineBufRGBA8888, dst + static_cast<ptrdiff_t>( line ) * dstStride, width );
		}
		return true;
	}
}



//-----------------------------------------------------------------------------
// Color conversion routines
//-----------------------------------------------------------------------------
void ConvertIA88ImageToNormalMapRGBA8888( const uint8 *src, int width, 
										  int height, uint8 *dst,
										  float bumpScale )
{
	float heightScale = ( 1.0f / 255.0f ) * bumpScale;
	float c, cx, cy;
	float maxDim = ( width > height ) ? (float)width : (float)height;
	float ooMaxDim = 1.0f / maxDim;

	int s, t;
	for( t = 0; t < height; t++ )
	{
		uint8 *dstPixel = &dst[t * width * 4];
		for( s = 0; s < width; s++ )
		{
			c = src[( t * width + s ) * 2];
			cx = src[( t * width + ((s+1)%width) ) * 2];
			cy = src[( ((t+1)%height) * width + s ) * 2];

			/*
			// \Z (out of screen)
			//  \
			//   \
			//    \
			//     \-----------  X
			//     |
			//     |
			//     |
			//     |
			//     |
			//     Y
			*/
			
			Vector xVect, yVect, normal;
			xVect[0] = ooMaxDim;
			xVect[1] = 0.0f;
			xVect[2] = (cx - c) * heightScale;

			yVect[0] = 0.0f;
			yVect[1] = ooMaxDim;
			yVect[2] = (cy - c) * heightScale;

			CrossProduct( xVect, yVect, normal );
			VectorNormalize( normal );

			/* Repack the normalized vector into an RGB unsigned byte
			vector in the normal map image. */
			dstPixel[0] = ( uint8 )( 128 + 127*normal[0] );
			dstPixel[1] = ( uint8 )( 128 + 127*normal[1] );
			dstPixel[2] = ( uint8 )( 128 + 127*normal[2] );
			dstPixel[3] = src[( ( t * width + s ) * 2 ) + 1];
			dstPixel += 4;
		}
	}
}

// dimhotepus: Destination is signed bytes.  Plain char is signed on x86 / Windows and unsigned on arm64 Linux, so write
// the two's complement bytes explicitly through uint8 (same bits as before), and size_t for the pixel count.
void ConvertNormalMapRGBA8888ToDUDVMapUVWQ8888( const uint8 *src, int width, int height,
									                     uint8 *dst )
{
	if ( width <= 0 || height <= 0 )
		return;

	const uint8 *lastPixel = src + static_cast<size_t>( width ) * height * 4;

	for( ; src < lastPixel; src += 4, dst += 4 )
	{
		dst[0] = static_cast<uint8>( ( ( int )src[0] ) - 127 );
		dst[1] = static_cast<uint8>( ( ( int )src[1] ) - 127 );
		dst[2] = static_cast<uint8>( ( ( int )src[2] ) - 127 );
		dst[3] = static_cast<uint8>( ( ( int )src[3] ) - 127 );
	}
}

void ConvertNormalMapRGBA8888ToDUDVMapUVLX8888( const uint8 *src, int width, int height,
									                     uint8 *dst )
{
	if ( width <= 0 || height <= 0 )
		return;

	const uint8 *lastPixel = src + static_cast<size_t>( width ) * height * 4;

	for( ; src < lastPixel; src += 4, dst += 4 )
	{
		dst[0] = static_cast<uint8>( ( ( int )src[0] ) - 127 );
		dst[1] = static_cast<uint8>( ( ( int )src[1] ) - 127 );
		dst[2] = src[3];
		dst[3] = 0xFF;
	}
}

void ConvertNormalMapRGBA8888ToDUDVMapUV88( const uint8 *src, int width, int height,
									                     uint8 *dst )
{
	if ( width <= 0 || height <= 0 )
		return;

	const uint8 *lastPixel = src + static_cast<size_t>( width ) * height * 4;

	for( ; src < lastPixel; src += 4, dst += 2 )
	{
		dst[0] = static_cast<uint8>( ( ( int )src[0] ) - 127 );
		dst[1] = static_cast<uint8>( ( ( int )src[1] ) - 127 );
	}
}

void NormalizeNormalMapRGBA8888( uint8 *src, int numTexels )
{
	// dimhotepus: size_t, int numTexels * 4 overflows.
	uint8 *lastPixel = src + static_cast<size_t>( numTexels ) * 4;

	for( uint8 *pixel = src; pixel < lastPixel; pixel += 4 )
	{
		Vector tmpVect
		{
			( ( float )pixel[0] - 128.0f ) * ( 1.0f / 127.0f ),
			( ( float )pixel[1] - 128.0f ) * ( 1.0f / 127.0f ),
			( ( float )pixel[2] - 128.0f ) * ( 1.0f / 127.0f )
		};		

		VectorNormalize( tmpVect );

		pixel[0] = ( uint8 )( 128 + 127 * tmpVect[0] );
		pixel[1] = ( uint8 )( 128 + 127 * tmpVect[1] );
		pixel[2] = ( uint8 )( 128 + 127 * tmpVect[2] );
	}
}

//-----------------------------------------------------------------------------
// Image rotation
//-----------------------------------------------------------------------------
bool RotateImageLeft( const uint8 *src, uint8 *dst, 
					  int widthHeight, ImageFormat imageFormat )
{
	// dimhotepus: size_t math for offsets (int overflows for big images).
#define SRC(x,y) src[(static_cast<size_t>(x)+static_cast<size_t>(y)*static_cast<size_t>(widthHeight))*sizeInBytes]
#define DST(x,y) dst[(static_cast<size_t>(x)+static_cast<size_t>(y)*static_cast<size_t>(widthHeight))*sizeInBytes]
	if( IsCompressed( imageFormat ) )
	{
		return false;
	}

	const int nSizeInBytes = SizeInBytes( imageFormat );
	Assert( nSizeInBytes <= 16 && nSizeInBytes > 0 );
	// dimhotepus: tmp is 16 bytes, so check in release too (was assert only => stack overflow).
	if ( nSizeInBytes <= 0 || nSizeInBytes > 16 || widthHeight < 0 )
	{
		return false;
	}
	const size_t sizeInBytes = static_cast<size_t>( nSizeInBytes );

	uint8 tmp[4][16];
	const int halfWidthHeight = widthHeight >> 1;
	// dimhotepus: Odd sizes have middle row / column, which are not in the [0, half) x [0, half) quadrant, so
	// they were never rotated / copied.  Walk [0, half) x [0, ceil(half)) for every 4 pixel cycle exactly once.
	const int halfWidthHeightCeil = halfWidthHeight + ( widthHeight & 1 );

	for( int y = 0; y < halfWidthHeight; y++ )
	{
		for( int x = 0; x < halfWidthHeightCeil; x++ )
		{
			memcpy( tmp[0], &SRC( x, y ), sizeInBytes );
			memcpy( tmp[1], &SRC( y, widthHeight-x-1 ), sizeInBytes );
			memcpy( tmp[2], &SRC( widthHeight-x-1, widthHeight-y-1 ), sizeInBytes );
			memcpy( tmp[3], &SRC( widthHeight-y-1, x ), sizeInBytes );
			memcpy( &DST( x, y ),                             tmp[3], sizeInBytes );
			memcpy( &DST( y, widthHeight-x-1 ),               tmp[0], sizeInBytes );
			memcpy( &DST( widthHeight-x-1, widthHeight-y-1 ), tmp[1], sizeInBytes );
			memcpy( &DST( widthHeight-y-1, x ),               tmp[2], sizeInBytes );
		}
	}

	// dimhotepus: Center pixel stays in place, but must be copied if not rotating in place.
	if ( ( widthHeight & 1 ) && src != dst )
	{
		memcpy( &DST( halfWidthHeight, halfWidthHeight ), &SRC( halfWidthHeight, halfWidthHeight ), sizeInBytes );
	}
#undef SRC
#undef DST
	return true;
}

bool RotateImage180( const uint8 *src, uint8 *dst, 
					  int widthHeight, ImageFormat imageFormat )
{
	// OPTIMIZE: do this transformation directly.
	if( RotateImageLeft( src, dst, widthHeight, imageFormat ) )
	{
		return RotateImageLeft( dst, dst, widthHeight, imageFormat );
	}
	return false;
}

bool FlipImageVertically( void *pSrc, void *pDst, int nWidth, int nHeight, ImageFormat imageFormat, int nDstStride )
{
	if( IsCompressed( imageFormat ) )
		return false;

	const int nSizeInBytes = SizeInBytes( imageFormat );
	// dimhotepus: Unknown format has no pixel size.
	if ( nSizeInBytes <= 0 )
		return false;
	// Nothing to flip.
	if ( nWidth <= 0 || nHeight <= 0 )
		return true;

	// dimhotepus: size_t / ptrdiff_t math, as int row offsets overflow on big images.
	const size_t nRowBytes = static_cast<size_t>( nSizeInBytes ) * nWidth;
	const ptrdiff_t nSrcStride = static_cast<ptrdiff_t>( nRowBytes );
	const ptrdiff_t nDstRowStride = nDstStride == 0 ? nSrcStride : nDstStride;

	auto *pSrcRow = (uint8*)pSrc;
	uint8 *pDstRow = (uint8*)pDst + ( static_cast<ptrdiff_t>( nHeight - 1 ) * nDstRowStride );
	if ( pSrc == pDst )
	{
		// dimhotepus: Swapping rows in place needs the same stride.
		Assert( nSrcStride == nDstRowStride );

		// dimhotepus: Heap instead of stack allocation, row can be big.
		std::vector<uint8> temp( nRowBytes );
		const int nHalfHeight = nHeight >> 1;
		for ( int i = 0; i < nHalfHeight; i++ )
		{
			memcpy( temp.data(), pSrcRow, nRowBytes );
			memcpy( pSrcRow, pDstRow, nRowBytes );
			memcpy( pDstRow, temp.data(), nRowBytes );

			pSrcRow += nSrcStride;
			pDstRow -= nDstRowStride;
		}
	}
	else
	{
		for ( int i = 0; i < nHeight; i++ )
		{
			memcpy( pDstRow, pSrcRow, nRowBytes );
			pSrcRow += nSrcStride;
			pDstRow -= nDstRowStride;
		}
	}

	return true;
}

bool FlipImageHorizontally( void *pSrc, void *pDst, int nWidth, int nHeight, ImageFormat imageFormat, int nDstStride )
{
	if( IsCompressed( imageFormat ) )
		return false;

	uint8 tmp[16];
	const int nSizeInBytes = SizeInBytes( imageFormat );
	Assert( nSizeInBytes <= 16 && nSizeInBytes > 0 );
	// dimhotepus: tmp is 16 bytes, so check in release too (was assert only).
	if ( nSizeInBytes <= 0 || nSizeInBytes > 16 )
		return false;
	// Nothing to flip.
	if ( nWidth <= 0 || nHeight <= 0 )
		return true;

	// dimhotepus: size_t / ptrdiff_t math, as int offsets overflow on big images.
	const size_t nRowBytes = static_cast<size_t>( nSizeInBytes ) * nWidth;
	const ptrdiff_t nSrcStride = static_cast<ptrdiff_t>( nRowBytes );
	const ptrdiff_t nDstRowStride = nDstStride == 0 ? nSrcStride : nDstStride;

	auto *pSrcRow = (uint8*)pSrc;
	auto *pDstRow = (uint8*)pDst;
	if ( pSrc == pDst )
	{
		// dimhotepus: Swapping pixels in place needs the same stride.
		Assert( nSrcStride == nDstRowStride );

		const int nHalfWidth = nWidth >> 1;
		for( int y = 0; y < nHeight; y++ )
		{
			uint8 *pSrcPixel = pSrcRow;
			uint8 *pDstPixel = pDstRow + nRowBytes - nSizeInBytes;
			for( int x = 0; x < nHalfWidth; x++ )
			{
				memcpy( tmp, pSrcPixel, nSizeInBytes );
				memcpy( pSrcPixel, pDstPixel, nSizeInBytes );
				memcpy( pDstPixel, tmp, nSizeInBytes );

				pSrcPixel += nSizeInBytes;
				pDstPixel -= nSizeInBytes;
			}

			pSrcRow += nSrcStride;
			pDstRow += nDstRowStride;
		}
	}
	else
	{
		for( int y = 0; y < nHeight; y++ )
		{
			uint8 *pSrcPixel = pSrcRow;
			uint8 *pDstPixel = pDstRow + nRowBytes - nSizeInBytes;
			for( int x = 0; x < nWidth; x++ )
			{
				memcpy( pDstPixel, pSrcPixel, nSizeInBytes );
				pSrcPixel += nSizeInBytes;
				pDstPixel -= nSizeInBytes;
			}

			pSrcRow += nSrcStride;
			pDstRow += nDstRowStride;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Image rotation
//-----------------------------------------------------------------------------
bool SwapAxes( uint8 *src, int widthHeight, ImageFormat imageFormat )
{
	// dimhotepus: size_t math for offsets (int overflows for big images).
#define SRC(x,y) src[(static_cast<size_t>(x)+static_cast<size_t>(y)*static_cast<size_t>(widthHeight))*sizeInBytes]
	if( IsCompressed( imageFormat ) )
	{
		return false;
	}

	const int nSizeInBytes = SizeInBytes( imageFormat );
	// dimhotepus: Pixel can be up to 16 bytes (RGBA32323232F), tmp was 4 bytes => stack overflow in release
	// for RGBA16161616 and float formats.
	Assert( nSizeInBytes <= 16 && nSizeInBytes > 0 );
	if ( nSizeInBytes <= 0 || nSizeInBytes > 16 || widthHeight < 0 )
	{
		return false;
	}
	const size_t sizeInBytes = static_cast<size_t>( nSizeInBytes );

	uint8 tmp[16];

	for( int y = 0; y < widthHeight; y++ )
	{
		for( int x = 0; x < y; x++ )
		{
			memcpy( tmp, &SRC( x, y ), sizeInBytes );
			memcpy( &SRC( x, y ), &SRC( y, x ), sizeInBytes );
			memcpy( &SRC( y, x ), tmp, sizeInBytes );
		}
	}
#undef SRC
	return true;
}

// dimhotepus: CS:GO backport.
void RGBA8888ToRGBA32323232F( const uint8 *src, uint8 *dst, int numPixels )
{
	constexpr float flOO255 = 1.0f / 255.0f;
	const uint8 *endSrc = src + static_cast<size_t>( numPixels ) * 4;
	for ( ; src < endSrc; src += 4, dst += 4 * sizeof( float ) )
	{
		StoreF32( dst + 0 * sizeof( float ), src[0] * flOO255 );
		StoreF32( dst + 1 * sizeof( float ), src[1] * flOO255 );
		StoreF32( dst + 2 * sizeof( float ), src[2] * flOO255 );
		StoreF32( dst + 3 * sizeof( float ), src[3] * flOO255 );
	}
}

// dimhotepus: CS:GO backport.
void RGBA8888ToRGB323232F( const uint8 *src, uint8 *dst, int numPixels )
{
	constexpr float flOO255 = 1.0f / 255.0f;
	const uint8 *endSrc = src + static_cast<size_t>( numPixels ) * 4;
	for ( ; src < endSrc; src += 4, dst += 3 * sizeof( float ) )
	{
		StoreF32( dst + 0 * sizeof( float ), src[0] * flOO255 );
		StoreF32( dst + 1 * sizeof( float ), src[1] * flOO255 );
		StoreF32( dst + 2 * sizeof( float ), src[2] * flOO255 );
	}
}

// dimhotepus: CS:GO backport.
void RGBA8888ToR32F( const uint8 *src, uint8 *dst, int numPixels )
{
	constexpr float flOO255 = 1.0f / 255.0f;
	const uint8 *endSrc = src + static_cast<size_t>( numPixels ) * 4;
	for ( ; src < endSrc; src += 4, dst += sizeof( float ) )
	{
		StoreF32( dst, src[0] * flOO255 );
	}
}

void RGBA8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	// dimhotepus: memcpy to itself is UB.
	if ( dst != src )
	{
		memcpy( dst, src, static_cast<size_t>( 4 ) * numPixels );
	}
}

void RGBA8888ToABGR8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 4 )
	{
		dst[0] = src[3];
		dst[1] = src[2];
		dst[2] = src[1];
		dst[3] = src[0];
	}
}

void RGBA8888ToRGB888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 3 )
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
	}
}

void RGBA8888ToBGR888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 3 )
	{
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
	}
}

void RGBA8888ToI8( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 1 )
	{
		dst[0] = ( uint8 )( 0.299f * src[0] + 0.587f * src[1] + 0.114f * src[2] );
	}
}

void RGBA8888ToIA88( const uint8 *src, uint8 *dst, int numPixels )
{
	// fixme: need to find the proper rgb weighting
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 2 )
	{
		dst[0] = ( uint8 )( 0.299f * src[0] + 0.587f * src[1] + 0.114f * src[2] );
		dst[1] = src[3];
	}
}

void RGBA8888ToA8( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 1 )
	{
		dst[0] = src[3];
	}
}

void RGBA8888ToRGB888_BLUESCREEN( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 3 )
	{
		if( src[3] == 0 )
		{
			dst[0] = 0;
			dst[1] = 0;
			dst[2] = 255;
		}
		else
		{
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
		}
	}
}

void RGBA8888ToBGR888_BLUESCREEN( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 3 )
	{
		if( src[3] == 0 )
		{
			dst[2] = 0;
			dst[1] = 0;
			dst[0] = 255;
		}
		else
		{
			dst[2] = src[0];
			dst[1] = src[1];
			dst[0] = src[2];
		}
	}
}

void RGBA8888ToARGB8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 4 )
	{
		dst[0] = src[3];
		dst[1] = src[0];
		dst[2] = src[1];
		dst[3] = src[2];
	}
}

void RGBA8888ToBGRA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 4 )
	{
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
		dst[3] = src[3];
	}
}

void RGBA8888ToBGRX8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 4 )
	{
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
		// dimhotepus: X is unused, but was left uninitialized.  Make output deterministic.
		dst[3] = 255;
	}
}

void RGBA8888ToBGR565( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + static_cast<size_t>( numPixels ) * 4;
	for ( ; src < endSrc; src += 4, dst += 2 )
	{
		StoreU16( dst, ((src[0] >> 3) << 11) |
					   ((src[1] >> 2) << 5) |
						(src[2] >> 3) );
	}
}

void RGBA8888ToBGRX5551( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + static_cast<size_t>( numPixels ) * 4;
	for ( ; src < endSrc; src += 4, dst += 2 )
	{
		StoreU16( dst, ((src[0] >> 3) << 10) |
					   ((src[1] >> 3) << 5) |
						(src[2] >> 3) );
	}
}

void RGBA8888ToBGRA5551( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + static_cast<size_t>( numPixels ) * 4;
	for ( ; src < endSrc; src += 4, dst += 2 )
	{
		StoreU16( dst, ((src[0] >> 3) << 10) |
					   ((src[1] >> 3) << 5) |
						(src[2] >> 3) |
						((src[3] >> 7) << 15) );
	}
}

void RGBA8888ToBGRA4444( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + static_cast<size_t>( numPixels ) * 4;
	for ( ; src < endSrc; src += 4, dst += 2 )
	{
		StoreU16( dst, ((src[0] >> 4) << 8) |
					   ((src[1] >> 4) << 4) |
						(src[2] >> 4) |
					   ((src[3] >> 4) << 12) );
	}
}

void RGBA8888ToUV88( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 2 )
	{
		dst[0] = src[0];
		dst[1] = src[1];
	}
}

void RGBA8888ToUVWQ8888( const uint8 *src, uint8 *dst, int numPixels )
{
	RGBA8888ToRGBA8888( src, dst, numPixels );
}

void RGBA8888ToUVLX8888( const uint8 *src, uint8 *dst, int numPixels )
{
	RGBA8888ToRGBA8888( src, dst, numPixels );
}

void ABGR8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 4 )
	{
		dst[0] = src[3];
		dst[1] = src[2];
		dst[2] = src[1];
		dst[3] = src[0];
	}
}

void RGB888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 3;
	for ( ; src < endSrc; src += 3, dst += 4 )
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst[3] = 255;
	}
}

void BGR888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 3;
	for ( ; src < endSrc; src += 3, dst += 4 )
	{
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
		dst[3] = 255;
	}
}

void I8ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels;
	for ( ; src < endSrc; src += 1, dst += 4 )
	{
		dst[0] = src[0];
		dst[1] = src[0];
		dst[2] = src[0];
		dst[3] = 255;
	}
}

void IA88ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 2;
	for ( ; src < endSrc; src += 2, dst += 4 )
	{
		dst[0] = src[0];
		dst[1] = src[0];
		dst[2] = src[0];
		dst[3] = src[1];
	}
}

void A8ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels;
	for ( ; src < endSrc; src += 1, dst += 4 )
	{
		dst[0] = src[0];
		dst[1] = src[0];
		dst[2] = src[0];
		dst[3] = src[0];
	}
}

void RGB888_BLUESCREENToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 3;
	for ( ; src < endSrc; src += 3, dst += 4 )
	{
		if( src[0] == 0 && src[1] == 0 && src[2] == 255 )
		{
			dst[0] = 0;
			dst[1] = 0;
			dst[2] = 0;
			dst[3] = 0;
		}
		else
		{
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = 255;
		}
	}
}

void BGR888_BLUESCREENToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 3;
	for ( ; src < endSrc; src += 3, dst += 4 )
	{
		if( src[2] == 0 && src[1] == 0 && src[0] == 255 )
		{
			dst[0] = 0;
			dst[1] = 0;
			dst[2] = 0;
			dst[3] = 0;
		}
		else
		{
			dst[2] = src[0];
			dst[1] = src[1];
			dst[0] = src[2];
			dst[3] = 255;
		}
	}
}

void ARGB8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 4 )
	{
		dst[0] = src[1];
		dst[1] = src[2];
		dst[2] = src[3];
		dst[3] = src[0];
	}
}

void BGRA8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 4 )
	{
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
		dst[3] = src[3];
	}
}

void BGRX8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 4 )
	{
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
		dst[3] = 255;
	}
}

void BGR565ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *pEndSrc = src + static_cast<size_t>( numPixels ) * 2;
	for ( ; src < pEndSrc; src += 2, dst += 4 )
	{
		const unsigned pixel = LoadU16( src );
		unsigned blue = pixel & 0x1FU;
		unsigned green = (pixel >> 5) & 0x3FU;
		unsigned red = (pixel >> 11) & 0x1FU;

		// Expand to 8 bits
		dst[0] = size_cast<uint8>((red << 3) | (red >> 2));
		dst[1] = size_cast<uint8>((green << 2) | (green >> 4));
		dst[2] = size_cast<uint8>((blue << 3) | (blue >> 2));
		dst[3] = 255U;
	}
}

void BGRX5551ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *pEndSrc = src + static_cast<size_t>( numPixels ) * 2;
	for ( ; src < pEndSrc; src += 2, dst += 4 )
	{
		const unsigned pixel = LoadU16( src );
		unsigned blue = pixel & 0x1FU;
		unsigned green = (pixel >> 5) & 0x1FU;
		unsigned red = (pixel >> 10) & 0x1FU;

		// Expand to 8 bits
		dst[0] = size_cast<uint8>((red << 3) | (red >> 2));
		dst[1] = size_cast<uint8>((green << 3) | (green >> 2));
		dst[2] = size_cast<uint8>((blue << 3) | (blue >> 2));
		dst[3] = 255U;
	}
}

void BGRA5551ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *pEndSrc = src + static_cast<size_t>( numPixels ) * 2;
	for ( ; src < pEndSrc; src += 2, dst += 4 )
	{
		const unsigned pixel = LoadU16( src );
		unsigned blue = pixel & 0x1FU;
		unsigned green = (pixel >> 5) & 0x1FU;
		unsigned red = (pixel >> 10) & 0x1FU;
		unsigned alpha = pixel & ( 1U << 15 );

		// Expand to 8 bits
		dst[0] = size_cast<uint8>((red << 3) | (red >> 2));
		dst[1] = size_cast<uint8>((green << 3) | (green >> 2));
		dst[2] = size_cast<uint8>((blue << 3) | (blue >> 2));
		// garymcthack
		if( alpha )
		{
			dst[3] = 255U;
		}
		else
		{
			dst[3] = 0;
		}
	}
}

void BGRA4444ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *pEndSrc = src + static_cast<size_t>( numPixels ) * 2;
	for ( ; src < pEndSrc; src += 2, dst += 4 )
	{
		const unsigned pixel = LoadU16( src );
		unsigned blue = pixel & 0xFU;
		unsigned green = (pixel >> 4) & 0xFU;
		unsigned red = (pixel >> 8) & 0xFU;
		unsigned alpha = (pixel >> 12) & 0xFU;

		// Expand to 8 bits
		// dimhotepus: Was (red << 4) | (red >> 4) (see old FIXME), so 0xF gave 0xF0 not 0xFF.  Replicate the nibble.
		dst[0] = size_cast<uint8>((red << 4) | red);
		dst[1] = size_cast<uint8>((green << 4) | green);
		dst[2] = size_cast<uint8>((blue << 4) | blue);
		dst[3] = size_cast<uint8>((alpha << 4) | alpha);
	}
}

void UV88ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8* pEndSrc = src + numPixels * 2;
	for ( ; src < pEndSrc; src += 2, dst += 4 )
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = 0;
		dst[3] = 0;
	}
}

void UVWQ8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	RGBA8888ToRGBA8888( src, dst, numPixels );
}

void UVLX8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	RGBA8888ToRGBA8888( src, dst, numPixels );
}

// HDRFIXME: This assumes that the 16-bit integer values are 4.12 fixed-point.
void RGBA16161616ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *pEndSrc = src + static_cast<size_t>( numPixels ) * 8;
	for ( ; src < pEndSrc; src += 8, dst += 4 )
	{
		// dimhotepus: Read through memcpy, source is not guaranteed to be 2 bytes aligned.
		unsigned short px[4];
		memcpy( px, src, sizeof( px ) );

		dst[0] = size_cast<uint8>(min( static_cast<unsigned short>(255U), static_cast<unsigned short>(px[0] >> 4) ));
		dst[1] = size_cast<uint8>(min( static_cast<unsigned short>(255U), static_cast<unsigned short>(px[1] >> 4) ));
		dst[2] = size_cast<uint8>(min( static_cast<unsigned short>(255U), static_cast<unsigned short>(px[2] >> 4) ));
		dst[3] = size_cast<uint8>(min( static_cast<unsigned short>(255U), static_cast<unsigned short>(px[3] >> 8) ));
	}
}

}  // namespace ImageLoader
