//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "s3tc_decode.h"

#include <cstring>

#include "bitmap/imageformat.h"
#include "tier0/basetypes.h"
#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct S3TCBlock_DXT1
{
	unsigned short m_Ref1;		// The two colors that this block blends betwixt.
	unsigned short m_Ref2;
	unsigned int m_PixelBits;
};


struct S3TCBlock_DXT5
{
	unsigned char m_AlphaRef[2];
	unsigned char m_AlphaBits[6];

	unsigned short m_Ref1;		// The two colors that this block blends betwixt.
	unsigned short m_Ref2;
	unsigned int m_PixelBits;
};

// dimhotepus: Blocks are a disk / GPU format, make sure no compiler on any platform adds padding.
static_assert( sizeof( S3TCBlock_DXT1 ) == 8, "DXT1 block must be 8 bytes" );
static_assert( sizeof( S3TCBlock_DXT5 ) == 16, "DXT5 block must be 16 bytes" );
static_assert( sizeof( S3TCBlock_DXT5 ) <= MAX_S3TC_BLOCK_BYTES, "Update MAX_S3TC_BLOCK_BYTES" );


// ------------------------------------------------------------------------------------------ //
// S3TCBlock
// ------------------------------------------------------------------------------------------ //

// dimhotepus: Was a template with signed char reads; use unsigned char so result does not depend on
// platform 'char' signedness (signed on x86, unsigned on arm64 Linux / Windows ARM64 differs by ABI).
[[nodiscard]] static unsigned ReadBitInt( const char *pBits, unsigned iBaseBit, unsigned nBits )
{
	const unsigned char *pBytes = reinterpret_cast<const unsigned char *>( pBits );

	unsigned ret = 0;
	for ( unsigned i = 0; i < nBits; i++ )
	{
		const unsigned iBit = iBaseBit + i;
		const unsigned val = ( ( pBytes[iBit >> 3] >> ( iBit & 7 ) ) & 1u ) << i;

		ret |= val;
	}
	return ret;
}

static void WriteBitInt( char *pBits, int iBaseBit, int nBits, int val )
{
	unsigned char *pBytes = reinterpret_cast<unsigned char *>( pBits );

	for ( int i=0; i < nBits; i++ )
	{
		const int iBit = iBaseBit + i;
		const unsigned char mask = static_cast<unsigned char>( 1u << ( iBit & 7 ) );

		pBytes[iBit>>3] = static_cast<unsigned char>( pBytes[iBit>>3] & ~mask );
		if ( (val >> i) & 1 )
			pBytes[iBit>>3] = static_cast<unsigned char>( pBytes[iBit>>3] | mask );
	}
}

[[nodiscard]] static int S3TC_BytesPerBlock( ImageFormat format )
{
	if ( format == IMAGE_FORMAT_DXT1 || format == IMAGE_FORMAT_ATI1N )
	{
		return 8;
	}
	else
	{
		Assert( format == IMAGE_FORMAT_DXT5 || format == IMAGE_FORMAT_ATI2N );
		return 16;
	}
}


/*

// We're not using this, but I'll keep it around for reference.
void S3TC_BuildPalette( ImageFormat format, const char *pS3Block, S3RGBA palette[4] )
{
	if ( format == IMAGE_FORMAT_DXT1 )
	{
		const S3TCBlock_DXT1 *pBlock = reinterpret_cast<const S3TCBlock_DXT1 *>( pS3Block );

		palette[0] = S3TC_RGBAFrom565( pBlock->m_Ref1, 255 );

		if ( pBlock->m_Ref1 <= pBlock->m_Ref2 )
		{
			// Opaque and transparent texels are defined. The lookup is 3 colors. 11 means
			// a black, transparent pixel.
			palette[1] = S3TC_RGBAFrom565( pBlock->m_Ref2, 255 );
			palette[2] = S3TC_RGBABlend( palette[0], palette[1], 1, 1, 2 );	
			palette[3].r = palette[3].g = palette[3].b = palette[3].a = 0;
		}
		else
		{
			// Only opaque texels are defined. The lookup is 4 colors.
			palette[1] = S3TC_RGBAFrom565( pBlock->m_Ref2, 255 );
			palette[2] = S3TC_RGBABlend( palette[0], palette[1], 2, 1, 3 );
			palette[3] = S3TC_RGBABlend( palette[0], palette[1], 1, 2, 3 );
		}
	}
	else
	{
		Assert( format == IMAGE_FORMAT_DXT5 );
	}
}

*/


S3PaletteIndex S3TC_GetPixelPaletteIndex( ImageFormat format, const char *pS3Block, int x, int y )
{
	Assert( x >= 0 && x < 4 ); //-V112
	Assert( y >= 0 && y < 4 ); //-V112
	int iQuadPixel = y*4 + x;
	S3PaletteIndex ret = { 0, 0 };

	if ( format == IMAGE_FORMAT_DXT1 )
	{
		const S3TCBlock_DXT1 *pBlock = reinterpret_cast<const S3TCBlock_DXT1 *>( pS3Block );
		ret.m_ColorIndex = static_cast<unsigned char>( (pBlock->m_PixelBits >> (iQuadPixel << 1)) & 3 );
		ret.m_AlphaIndex = 0;
	}
	else
	{
		Assert( format == IMAGE_FORMAT_DXT5 );

		const S3TCBlock_DXT5 *pBlock = reinterpret_cast<const S3TCBlock_DXT5 *>( pS3Block );
		
		// dimhotepus: Was '*((int64*)pBlock->m_AlphaBits)'. That cast dropped const, did a misaligned 8 byte
		// load (m_AlphaBits sits at offset 2), violated strict aliasing and read 2 bytes past the 6 alpha bytes.
		// Read only the 3 bits we need, bit by bit (little-endian bit order, as in the file format).
		ret.m_ColorIndex = static_cast<unsigned char>( (pBlock->m_PixelBits >> (iQuadPixel << 1)) & 3 );
		ret.m_AlphaIndex = static_cast<unsigned char>( ReadBitInt( reinterpret_cast<const char *>( pBlock->m_AlphaBits ),
			static_cast<unsigned>( iQuadPixel * 3 ), 3 ) );
	}

	return ret;
}


void S3TC_SetPixelPaletteIndex( ImageFormat format, char *pS3Block, int x, int y, S3PaletteIndex iPaletteIndex )
{
	Assert( x >= 0 && x < 4 ); //-V112
	Assert( y >= 0 && y < 4 ); //-V112
	Assert( iPaletteIndex.m_ColorIndex < 4 );
	Assert( iPaletteIndex.m_AlphaIndex < 8 );

	int iQuadPixel = y*4 + x;
	int iColorBit = iQuadPixel * 2;
	
	if ( format == IMAGE_FORMAT_DXT1 )
	{
		S3TCBlock_DXT1 *pBlock = reinterpret_cast<S3TCBlock_DXT1 *>( pS3Block );

		// dimhotepus: '3 << 30' overflows signed int (UB), use unsigned.
		pBlock->m_PixelBits &= ~( 3u << iColorBit );
		pBlock->m_PixelBits |= (unsigned int)iPaletteIndex.m_ColorIndex << iColorBit;
	}
	else
	{
		Assert( format == IMAGE_FORMAT_DXT5 );
		 
		S3TCBlock_DXT5 *pBlock = reinterpret_cast<S3TCBlock_DXT5 *>( pS3Block );

		// Copy the color portion in.
		pBlock->m_PixelBits &= ~( 3u << iColorBit );
		pBlock->m_PixelBits |= (unsigned int)iPaletteIndex.m_ColorIndex << iColorBit;
		 
		 // Copy the alpha portion in.
		WriteBitInt( (char*)pBlock->m_AlphaBits, iQuadPixel*3, 3, iPaletteIndex.m_AlphaIndex );
	}
}


const char* S3TC_GetBlock( 
	const void *pCompressed, 
	ImageFormat format, 
	int nBlocksWidth,
	int xBlock,
	int yBlock )
{
	// dimhotepus: Do math in intp, ((yBlock * nBlocksWidth) + xBlock) * nBytesPerBlock overflowed int
	// for large textures (16384 blocks wide DXT5 is already 4 GiB).
	const intp nBytesPerBlock = S3TC_BytesPerBlock( format );
	const intp iBlock = static_cast<intp>( yBlock ) * nBlocksWidth + xBlock;

	return static_cast<const char*>( pCompressed ) + iBlock * nBytesPerBlock;
}


char* S3TC_GetBlock( 
	void *pCompressed, 
	ImageFormat format, 
	int nBlocksWidth,
	int xBlock,
	int yBlock )
{
	return const_cast<char*>( S3TC_GetBlock( static_cast<const void*>( pCompressed ), format, nBlocksWidth, xBlock, yBlock ) );
}


// dimhotepus: Was [[maybe_unused]] everywhere plus ~50 lines of code under '#if 0' which depended on nvtc / DirectDraw
// structures (DDSURFACEDESC, S3TCencode) which are not available. Removed dead code, kept the failure.
static void GenerateRepresentativePalette(
	[[maybe_unused]] ImageFormat format,
	[[maybe_unused]] S3RGBA **pOriginals,	// Original RGBA colors in the texture. This allows it to avoid doubly compressing.
	[[maybe_unused]] int nBlocks,
	[[maybe_unused]] int lPitch,	// (in BYTES)
	[[maybe_unused]] char mergedBlocks[16*MAX_S3TC_BLOCK_BYTES]
	)
{
	Error( "GenerateRepresentativePalette: not implemented" );
}

void S3TC_MergeBlocks( 
	char **blocks,
	S3RGBA **pOriginals,	// Original RGBA colors in the texture. This allows it to avoid doubly compressing.
	int nBlocks,
	int lPitch,	// (in BYTES)
	ImageFormat format
	)
{
	// dimhotepus: Merged buffer holds at most 3 blocks wide x 4 rows, bit reads below would overflow it otherwise.
	Assert( nBlocks == 2 || nBlocks == 3 );
	if ( nBlocks < 1 || nBlocks > 3 )
		return;

	// Figure out a good palette to represent all of these blocks.
	// dimhotepus: Was 'alignas(S3TCBlock_DXT1*)' (align of a pointer, not of the block) and left uninitialized
	// if Error() returns. Align to the block and zero it.
	alignas(S3TCBlock_DXT1) char mergedBlocks[16 * MAX_S3TC_BLOCK_BYTES] = {};
	GenerateRepresentativePalette( format, pOriginals, nBlocks, lPitch, mergedBlocks );

	// Build a remap table to remap block 2's colors to block 1's colors.
	if ( format == IMAGE_FORMAT_DXT1 )
	{
		// Grab the palette indices that s3tc.lib made for us.
		const char *pBase = (const char*)mergedBlocks;
		pBase += 4;

		unsigned short mergedRef1, mergedRef2;
		std::memcpy( &mergedRef1, mergedBlocks, sizeof( mergedRef1 ) );
		std::memcpy( &mergedRef2, mergedBlocks + sizeof( mergedRef1 ), sizeof( mergedRef2 ) );

		for ( int iBlock=0; iBlock < nBlocks; iBlock++ )
		{
			S3TCBlock_DXT1 *pBlock = ((S3TCBlock_DXT1*)blocks[iBlock]);
			
			// Remap all of the block's pixels.
			for ( int x=0; x < 4; x++ )
			{
				for ( int y=0; y < 4; y++ )
				{
					const unsigned iBaseBit = static_cast<unsigned>( (y*nBlocks*4 + x + iBlock*4) * 2 ); //-V112
					
					S3PaletteIndex index = {0, 0};
					index.m_ColorIndex = static_cast<unsigned char>( ReadBitInt( pBase, iBaseBit, 2 ) );
					
					S3TC_SetPixelPaletteIndex( format, (char*)pBlock, x, y, index );
				}
			}

			// Copy block 1's palette to block 2.
			pBlock->m_Ref1 = mergedRef1;
			pBlock->m_Ref2 = mergedRef2;
		}
	}
	else
	{
		Assert( format == IMAGE_FORMAT_DXT5 );

		// Skip past the alpha palette.
		const char *pAlphaPalette = mergedBlocks;
		const char *pAlphaBits = mergedBlocks + 2;
		
		// Skip past the alpha pixel bits and past the color palette.
		// dimhotepus: Was 'alignas(unsigned short*) const char *pColorPalette', alignas on a pointer variable
		// does not align the pointee. Load the 16 bit values with memcpy, no alignment / aliasing assumptions.
		const char *pColorPalette = pAlphaBits + 6*nBlocks;
		const char *pColorBits = pColorPalette + 4;

		unsigned short mergedRef1, mergedRef2;
		std::memcpy( &mergedRef1, pColorPalette, sizeof( mergedRef1 ) );
		std::memcpy( &mergedRef2, pColorPalette + sizeof( mergedRef1 ), sizeof( mergedRef2 ) );

		for ( int iBlock=0; iBlock < nBlocks; iBlock++ )
		{
			S3TCBlock_DXT5 *pBlock = ((S3TCBlock_DXT5*)blocks[iBlock]);
			
			// Remap all of the block's pixels.
			for ( int x=0; x < 4; x++ )
			{
				for ( int y=0; y < 4; y++ )
				{
					const unsigned iBasePixel = static_cast<unsigned>( y*nBlocks*4 + x + iBlock*4 ); //-V112
					
					S3PaletteIndex index;
					index.m_AlphaIndex = static_cast<unsigned char>( ReadBitInt( pAlphaBits, iBasePixel * 3, 3 ) );
					index.m_ColorIndex = static_cast<unsigned char>( ReadBitInt( pColorBits, iBasePixel * 2, 2 ) );
					
					S3TC_SetPixelPaletteIndex( format, (char*)pBlock, x, y, index );
				}
			}

			// Copy block 1's palette to block 2.
			pBlock->m_AlphaRef[0] = static_cast<unsigned char>( pAlphaPalette[0] );
			pBlock->m_AlphaRef[1] = static_cast<unsigned char>( pAlphaPalette[1] );
			pBlock->m_Ref1 = mergedRef1;
			pBlock->m_Ref2 = mergedRef2;
		}
	}
}


S3PaletteIndex S3TC_GetPaletteIndex( 
	unsigned char *pFaceData,
	ImageFormat format,
	int imageWidth,
	int x,
	int y )
{
	// dimhotepus: Round blocks up, (imageWidth>>2) is 0 for 1x1 and 2x2 mips and wrong for widths not multiple of 4.
	char *pBlock = S3TC_GetBlock( pFaceData, format, (imageWidth+3)>>2, x>>2, y>>2 );
	return S3TC_GetPixelPaletteIndex( format, pBlock, x&3, y&3 );
}


void S3TC_SetPaletteIndex( 
	unsigned char *pFaceData,
	ImageFormat format,
	int imageWidth,
	int x,
	int y,
	S3PaletteIndex paletteIndex )
{
	// dimhotepus: Round blocks up, see S3TC_GetPaletteIndex.
	char *pBlock = S3TC_GetBlock( pFaceData, format, (imageWidth+3)>>2, x>>2, y>>2 );
	S3TC_SetPixelPaletteIndex( format, pBlock, x&3, y&3, paletteIndex );
}