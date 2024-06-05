//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "bitmap/bitmap.h"

#include <cstring>

#include "bitmap/imageformat.h"
#include "Color.h"
#include "tier0/dbg.h"
#include "tier0/wchartypes.h"  // byte

// Should be last include
#include "tier0/memdbgon.h"

bool Bitmap_t::IsValid() const
{
	if ( m_nWidth <= 0 || m_nHeight <= 0 || m_pBits == nullptr )
	{
		Assert( m_nWidth == 0 );
		Assert( m_nHeight == 0 );
		Assert( m_pBits == nullptr );
		return false;
	}
	return true;
}

void Bitmap_t::Clear()
{
	if ( m_pBits && m_bOwnsBuffer )
	{
		free( m_pBits );
	}
	Reset();
}

void Bitmap_t::Init( int xs, int ys, ImageFormat imageFormat, int nStride )
{

	// Check for bogus allocation sizes
	if (xs <= 0 || ys <= 0 )
	{
		Assert( xs == 0 );
		Assert( ys == 0 );
		Clear();
		return;
	}

	int nPixSize = ImageLoader::SizeInBytes( imageFormat );

	// Auto detect stride
	if ( nStride == 0 )
	{
		nStride = nPixSize * xs;
	}

	// Check for NOP
	if (
		m_pBits
		&& m_bOwnsBuffer
		&& m_nWidth == xs
		&& m_nHeight == ys
		&& nStride == m_nStride
		&& nPixSize == m_nPixelSize )
	{
		// We're already got a buffer of the right size.
		// The only thing that might be wrong is the pixel format.
		m_ImageFormat = imageFormat;
		return;
	}

	// Free up anything already allocated
	Clear();

	// Remember dimensions and pixel format
	m_nWidth = xs;
	m_nHeight = ys;
	m_ImageFormat = imageFormat;
	m_nPixelSize = nPixSize;
	m_nStride = nStride;

	// Allocate buffer.  Because this is a PC game,
	// failure is impossible....right?
	m_pBits = static_cast<byte *>(malloc( ys * m_nStride ));

	// Assume ownership
	m_bOwnsBuffer = true;
}

void Bitmap_t::SetBuffer( int nWidth, int nHeight, ImageFormat imageFormat, unsigned char *pBits, bool bAssumeOwnership, int nStride )
{
	Assert( pBits );
	Assert( nWidth > 0 );
	Assert( nHeight > 0 );

	// Free up anything already allocated
	Clear();

	// Remember dimensions and pixel format
	m_nWidth = nWidth;
	m_nHeight = nHeight;
	m_ImageFormat = imageFormat;
	m_nPixelSize = ImageLoader::SizeInBytes( imageFormat );
	if ( nStride == 0 )
	{
		m_nStride = m_nPixelSize * nWidth;
	}
	else
	{
		m_nStride = nStride;
	}

	// Set our buffer pointer
	m_pBits = pBits;

	// Assume ownership of the buffer, if requested
	m_bOwnsBuffer = bAssumeOwnership;

	// We should be good to go
	Assert( IsValid() );
}

Color Bitmap_t::GetColor( int x, int y ) const
{
	Assert( x >= 0 && x < m_nWidth );
	Assert( y >= 0 && y < m_nHeight );
	Assert( m_pBits );

	// Get pointer to pixel data
	byte *ptr = m_pBits + (y*m_nStride) + x* m_nPixelSize;

	// Check supported image formats
	switch ( m_ImageFormat )
	{
		case IMAGE_FORMAT_RGBA8888:
			return Color( ptr[0], ptr[1], ptr[2], ptr[3] );

		case IMAGE_FORMAT_ABGR8888:
			return Color( ptr[3], ptr[2], ptr[1], ptr[0] );

		default:
			AssertMsg( false, "Unsupport image format!");
			return Color( 255,0,255,255 );
	}
}

void Bitmap_t::SetColor( int x, int y, Color c )
{
	Assert( x >= 0 && x < m_nWidth );
	Assert( y >= 0 && y < m_nHeight );
	Assert( m_pBits );

	// Get pointer to pixel data
	byte *ptr = m_pBits + (y*m_nStride) + x* m_nPixelSize;

	// Check supported image formats
	switch ( m_ImageFormat )
	{
		case IMAGE_FORMAT_RGBA8888:
			ptr[0] = static_cast<byte>(c.r());
			ptr[1] = static_cast<byte>(c.g());
			ptr[2] = static_cast<byte>(c.b());
			ptr[3] = static_cast<byte>(c.a());
			break;

		case IMAGE_FORMAT_ABGR8888:
			ptr[0] = static_cast<byte>(c.a());
			ptr[1] = static_cast<byte>(c.b());
			ptr[2] = static_cast<byte>(c.g());
			ptr[3] = static_cast<byte>(c.r());
			break;

		default:
			AssertMsg( false, "Unsupport image format!" );
			break;
	}
}

void Bitmap_t::MakeLogicalCopyOf( Bitmap_t &src, bool bTransferBufferOwnership )
{
	// What does it mean to make a logical copy of an
	// invalid bitmap?  I'll tell you what it means: you have a bug.
	Assert( src.IsValid() );

	// Free up anything we already own
	Clear();

	// Copy all of the member variables so we are
	// a logical copy of the source bitmap
	m_nWidth = src.m_nWidth;
	m_nHeight = src.m_nHeight;
	m_nPixelSize = src.m_nPixelSize;
	m_nStride = src.m_nStride;
	m_ImageFormat = src.m_ImageFormat;
	m_pBits = src.m_pBits;
	Assert( !m_bOwnsBuffer );

	// Check for assuming ownership of the buffer
	if ( bTransferBufferOwnership )
	{
		if ( src.m_bOwnsBuffer )
		{
			m_bOwnsBuffer = true;
			src.m_bOwnsBuffer = false;
		}
		else
		{
			// They don't own the buffer?  Then who does?
			// Maybe nobody, and it would safe to assume
			// ownership.  But more than likely, this is a
			// bug.
			Assert( src.m_bOwnsBuffer );

			// And a leak is better than a double-free.
			// Don't assume ownership of the buffer.
		}
	}
}

void Bitmap_t::Crop( int x0, int y0, int nWidth, int nHeight, const Bitmap_t *pImgSource )
{
	// Check for cropping in place, then save off our data to a temp
	Bitmap_t temp;
	if ( pImgSource == this || !pImgSource )
	{
		temp.MakeLogicalCopyOf( *this, m_bOwnsBuffer );
		pImgSource = &temp;
	}

	// No source image?
	if ( !pImgSource->IsValid() )
	{
		Assert( pImgSource->IsValid() );
		return;
	}

	// Sanity check crop rectangle
	Assert( x0 >= 0 );
	Assert( y0 >= 0 );
	Assert( x0 + nWidth <= pImgSource->Width() );
	Assert( y0 + nHeight <= pImgSource->Height() );

	// Allocate buffer
	Init( nWidth, nHeight, pImgSource->Format() );

	// Something wrong?
	if ( !IsValid() )
	{
		Assert( IsValid() );
		return;
	}

	// Copy the data a row at a time
	int nRowSize = m_nWidth * m_nPixelSize;
	for ( int y = 0 ; y < m_nHeight ; ++y )
	{
		memcpy( GetPixel(0,y), pImgSource->GetPixel( x0, y + y0 ), nRowSize );
	}
}

void Bitmap_t::SetPixelData( const Bitmap_t &src, int nSrcX1, int nSrcY1, int nCopySizeX, int nCopySizeY, int nDestX1, int nDestY1 )
{
	// Safety
	if ( !src.IsValid() )
	{
		Assert( src.IsValid() );
		return;
	}
	if ( !IsValid() )
	{
		Assert( IsValid() );
		return;
	}

	// You need to specify a valid source rectangle, we cannot clip that for you
	if ( nSrcX1 < 0 || nSrcY1 < 0 || nSrcX1 + nCopySizeX > src.Width() || nSrcY1 + nCopySizeY > src.Height() )
	{
		Assert( nSrcX1 >= 0 );
		Assert( nSrcY1 >= 0 );
		Assert( nSrcX1 + nCopySizeX <= src.Width() );
		Assert( nSrcY1 + nCopySizeY <= src.Height() );
		return;
	}

	// But we can clip the rectangle if it extends outside the destination image in a perfectly
	// reasonable way
	if ( nDestX1 < 0 )
	{
		nCopySizeX += nDestX1;
		nDestX1 = 0;
	}
	if ( nDestX1 + nCopySizeX > Width() )
	{
		nCopySizeX = Width() - nDestX1;
	}
	if ( nDestY1 < 0 )
	{
		nCopySizeY += nDestY1;
		nDestY1 = 0;
	}
	if ( nDestY1 + nCopySizeY > Height() )
	{
		nCopySizeY = Height() - nDestY1;
	}
	if ( nCopySizeX <= 0 || nCopySizeY <= 0 )
	{
		return;
	}

	// Copy the pixel data
	for ( int y = 0 ; y < nCopySizeY ; ++y )
	{
		// Wow, this could be a lot faster in the common case
		// that the pixe formats are the same.  But...this code
		// is simple and works, and is NOT the root of all evil.
		for ( int x = 0 ; x < nCopySizeX ; ++x )
		{
			Color c = src.GetColor( nSrcX1 + x, nSrcY1 + y );
			SetColor( nDestX1 + x, nDestY1 + y, c );
		}
	}
}

void Bitmap_t::SetPixelData( const Bitmap_t &src, int nDestX1, int nDestY1 )
{
	SetPixelData( src, 0, 0, src.Width(), src.Height(), nDestX1, nDestY1 );
}

