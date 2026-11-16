//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "bitmap/bitmap.h"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bitmap/imageformat.h"
#include "Color.h"
#include "tier0/dbg.h"

// Should be last include
#include "tier0/memdbgon.h"

namespace
{

// dimhotepus: Formats (see ImageFormatInfo table in imageformat.cpp) we can read / write as Color.
// All other formats are compressed, packed, float, depth or signed and are not convertible per pixel.
[[nodiscard]] bool IsColorAccessSupported( ImageFormat format )
{
	switch ( format )
	{
	case IMAGE_FORMAT_RGBA8888:
	case IMAGE_FORMAT_ABGR8888:
	case IMAGE_FORMAT_ARGB8888:
	case IMAGE_FORMAT_BGRA8888:
	case IMAGE_FORMAT_BGRX8888:
	case IMAGE_FORMAT_RGB888:
	case IMAGE_FORMAT_BGR888:
		return true;

	default:
		return false;
	}
}

}  // namespace

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

	// dimhotepus: Unknown / invalid / block compressed formats (DXT*, ATI*N) have 0 bytes per
	// pixel in the ImageFormatInfo table, so can't be stored in a linear bitmap.
	if ( nPixSize <= 0 )
	{
		AssertMsg( false, "Unsupported image format!" );
		Clear();
		return;
	}

	// dimhotepus: xs * nPixSize can overflow int.
	if ( xs > INT_MAX / nPixSize )
	{
		AssertMsg( false, "Bitmap row is too big!" );
		Clear();
		return;
	}

	// Auto detect stride
	if ( nStride == 0 )
	{
		nStride = nPixSize * xs;
	}
	// dimhotepus: Stride smaller than a row would make rows overlap / overflow buffer.
	else if ( nStride < nPixSize * xs )
	{
		AssertMsg( false, "Bitmap stride is smaller than row size!" );
		Clear();
		return;
	}

	// dimhotepus: ys * nStride overflowed int (and size_t on 32-bit).  Compute in size_t with check.
	if ( static_cast<size_t>(ys) > SIZE_MAX / static_cast<size_t>(nStride) )
	{
		AssertMsg( false, "Bitmap is too big!" );
		Clear();
		return;
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

	// dimhotepus: Allocate before touching state, so on failure we stay in valid empty state.
	// Original code assumed malloc never fails.
	unsigned char *pBits = static_cast<unsigned char *>( malloc( static_cast<size_t>(ys) * static_cast<size_t>(nStride) ) );
	if ( !pBits )
	{
		AssertMsg( false, "Out of memory allocating bitmap!" );
		return;
	}

	// Remember dimensions and pixel format
	m_nWidth = xs;
	m_nHeight = ys;
	m_ImageFormat = imageFormat;
	m_nPixelSize = nPixSize;
	m_nStride = nStride;
	m_pBits = pBits;

	// Assume ownership
	m_bOwnsBuffer = true;
}

void Bitmap_t::SetBuffer( int nWidth, int nHeight, ImageFormat imageFormat, unsigned char *pBits, bool bAssumeOwnership, int nStride )
{
	Assert( pBits );
	Assert( nWidth > 0 );
	Assert( nHeight > 0 );

	const int nPixelSize = ImageLoader::SizeInBytes( imageFormat );

	// dimhotepus: Validate in release builds too.  On bad input we leave the bitmap
	// empty.  We do not free pBits even if ownership was offered: a leak is better than a double-free.
	if ( !pBits || nWidth <= 0 || nHeight <= 0 || nPixelSize <= 0 || nWidth > INT_MAX / nPixelSize
		|| ( nStride != 0 && nStride < nPixelSize * nWidth ) )
	{
		Clear();
		return;
	}

	// dimhotepus: Setting the buffer we already hold would free it in Clear() and then use it.
	if ( pBits == m_pBits )
	{
		Reset();
	}
	else
	{
		// Free up anything already allocated
		Clear();
	}

	// Remember dimensions and pixel format
	m_nWidth = nWidth;
	m_nHeight = nHeight;
	m_ImageFormat = imageFormat;
	m_nPixelSize = nPixelSize;
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
	// dimhotepus: Use GetPixel, which does overflow-safe math.
	const unsigned char *ptr = GetPixel( x, y );

	// Check supported image formats
	switch ( m_ImageFormat )
	{
		case IMAGE_FORMAT_RGBA8888:
			return { ptr[0], ptr[1], ptr[2], ptr[3] };

		case IMAGE_FORMAT_ABGR8888:
			return { ptr[3], ptr[2], ptr[1], ptr[0] };

		// dimhotepus: More 8-bit formats, as in ImageFormatInfo table.
		case IMAGE_FORMAT_ARGB8888:
			return { ptr[1], ptr[2], ptr[3], ptr[0] };

		case IMAGE_FORMAT_BGRA8888:
			return { ptr[2], ptr[1], ptr[0], ptr[3] };

		// X means unused, so opaque.
		case IMAGE_FORMAT_BGRX8888:
			return { ptr[2], ptr[1], ptr[0], 255 };

		case IMAGE_FORMAT_RGB888:
			return { ptr[0], ptr[1], ptr[2], 255 };

		case IMAGE_FORMAT_BGR888:
			return { ptr[2], ptr[1], ptr[0], 255 };

		default:
			AssertMsg( false, "Unsupported image format!");
			return { 255,0,255,255 };
	}
}

void Bitmap_t::SetColor( int x, int y, Color c )
{
	Assert( x >= 0 && x < m_nWidth );
	Assert( y >= 0 && y < m_nHeight );
	Assert( m_pBits );

	// Get pointer to pixel data
	// dimhotepus: Use GetPixel, which does overflow-safe math.
	unsigned char *ptr = GetPixel( x, y );

	// Check supported image formats
	switch ( m_ImageFormat )
	{
		case IMAGE_FORMAT_RGBA8888:
			ptr[0] = c.r();
			ptr[1] = c.g();
			ptr[2] = c.b();
			ptr[3] = c.a();
			break;

		case IMAGE_FORMAT_ABGR8888:
			ptr[0] = c.a();
			ptr[1] = c.b();
			ptr[2] = c.g();
			ptr[3] = c.r();
			break;

		// dimhotepus: More 8-bit formats, as in ImageFormatInfo table.
		case IMAGE_FORMAT_ARGB8888:
			ptr[0] = c.a();
			ptr[1] = c.r();
			ptr[2] = c.g();
			ptr[3] = c.b();
			break;

		case IMAGE_FORMAT_BGRA8888:
			ptr[0] = c.b();
			ptr[1] = c.g();
			ptr[2] = c.r();
			ptr[3] = c.a();
			break;

		case IMAGE_FORMAT_BGRX8888:
			ptr[0] = c.b();
			ptr[1] = c.g();
			ptr[2] = c.r();
			ptr[3] = 255;
			break;

		case IMAGE_FORMAT_RGB888:
			ptr[0] = c.r();
			ptr[1] = c.g();
			ptr[2] = c.b();
			break;

		case IMAGE_FORMAT_BGR888:
			ptr[0] = c.b();
			ptr[1] = c.g();
			ptr[2] = c.r();
			break;

		default:
			AssertMsg( false, "Unsupported image format!" );
			break;
	}
}

void Bitmap_t::MakeLogicalCopyOf( Bitmap_t &src, bool bTransferBufferOwnership )
{
	// dimhotepus: Self copy would Clear() (free) the buffer we then copy from.  Nothing to do,
	// except ownership transfer to self is a no-op as well.
	if ( &src == this )
	{
		return;
	}

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
	const bool bInPlace = ( pImgSource == this || !pImgSource );
	if ( bInPlace )
	{
		pImgSource = this;
	}

	// No source image?
	if ( !pImgSource->IsValid() )
	{
		Assert( pImgSource->IsValid() );
		return;
	}

	// Sanity check crop rectangle
	// dimhotepus: Validate in release builds too (was assert only => out of bounds memcpy), and do it
	// before moving the buffer to temp.  Otherwise an early return in place would leave us
	// pointing to memory freed by temp dtor.  Also x0 + nWidth could overflow int, so subtract instead.
	Assert( x0 >= 0 );
	Assert( y0 >= 0 );
	Assert( nWidth > 0 );
	Assert( nHeight > 0 );
	Assert( nWidth <= pImgSource->Width() - x0 );
	Assert( nHeight <= pImgSource->Height() - y0 );
	if ( x0 < 0 || y0 < 0 || nWidth <= 0 || nHeight <= 0
		|| nWidth > pImgSource->Width() - x0
		|| nHeight > pImgSource->Height() - y0 )
	{
		return;
	}

	// Check for cropping in place, then save off our data to a temp
	Bitmap_t temp;
	if ( bInPlace )
	{
		temp.MakeLogicalCopyOf( *this, m_bOwnsBuffer );
		pImgSource = &temp;
	}

	// Allocate buffer
	Init( nWidth, nHeight, pImgSource->Format() );

	// Something wrong?
	if ( !IsValid() )
	{
		Assert( IsValid() );
		return;
	}

	// Copy the data a row at a time
	const size_t nRowSize = static_cast<size_t>(m_nWidth) * m_nPixelSize;
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
	// dimhotepus: Use subtraction to avoid int overflow in nSrcX1 + nCopySizeX.
	if ( nSrcX1 < 0 || nSrcY1 < 0 || nCopySizeX > src.Width() - nSrcX1 || nCopySizeY > src.Height() - nSrcY1 )
	{
		Assert( nSrcX1 >= 0 );
		Assert( nSrcY1 >= 0 );
		Assert( nCopySizeX <= src.Width() - nSrcX1 );
		Assert( nCopySizeY <= src.Height() - nSrcY1 );
		return;
	}

	// Nothing to copy
	if ( nCopySizeX <= 0 || nCopySizeY <= 0 )
	{
		return;
	}

	// dimhotepus: Rectangle completely outside destination.  Also protects from int overflow below.
	if ( nDestX1 <= -nCopySizeX || nDestX1 >= Width() || nDestY1 <= -nCopySizeY || nDestY1 >= Height() )
	{
		return;
	}

	// But we can clip the rectangle if it extends outside the destination image in a perfectly
	// reasonable way
	if ( nDestX1 < 0 )
	{
		// dimhotepus: Clipping on the left must also advance the source origin, otherwise
		// wrong part of the source was copied.
		nSrcX1 -= nDestX1;
		nCopySizeX += nDestX1;
		nDestX1 = 0;
	}
	if ( nCopySizeX > Width() - nDestX1 )
	{
		nCopySizeX = Width() - nDestX1;
	}
	if ( nDestY1 < 0 )
	{
		// dimhotepus: Same as X, advance source origin when clipping on the top.
		nSrcY1 -= nDestY1;
		nCopySizeY += nDestY1;
		nDestY1 = 0;
	}
	if ( nCopySizeY > Height() - nDestY1 )
	{
		nCopySizeY = Height() - nDestY1;
	}
	if ( nCopySizeX <= 0 || nCopySizeY <= 0 )
	{
		return;
	}

	// dimhotepus: Fast path for same pixel format, copy rows with memmove instead of per pixel
	// conversion.  memmove + row order handle overlapping rectangles when src is this.
	if ( m_ImageFormat == src.m_ImageFormat && m_nPixelSize == src.m_nPixelSize )
	{
		const size_t nRowSize = static_cast<size_t>(nCopySizeX) * m_nPixelSize;
		const bool bReverse = ( &src == this && nDestY1 > nSrcY1 );

		for ( int i = 0 ; i < nCopySizeY ; ++i )
		{
			const int y = bReverse ? ( nCopySizeY - 1 - i ) : i;
			memmove( GetPixel( nDestX1, nDestY1 + y ), src.GetPixel( nSrcX1, nSrcY1 + y ), nRowSize );
		}
		return;
	}

	// dimhotepus: Check once instead of asserting for every pixel, and don't write garbage in release.
	if ( !IsColorAccessSupported( src.m_ImageFormat ) || !IsColorAccessSupported( m_ImageFormat ) )
	{
		AssertMsg( false, "Can't convert between image formats %s and %s!",
			ImageLoader::GetName( src.m_ImageFormat ), ImageLoader::GetName( m_ImageFormat ) );
		return;
	}

	// Copy the pixel data
	for ( int y = 0 ; y < nCopySizeY ; ++y )
	{
		// Different pixel formats: convert through Color.
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