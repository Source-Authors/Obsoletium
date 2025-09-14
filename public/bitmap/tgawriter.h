//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef TGAWRITER_H
#define TGAWRITER_H

#include "tier0/wchartypes.h"
#include "imageformat.h" //ImageFormat enum definition

class CUtlBuffer;


namespace TGAWriter
{
	[[nodiscard]] bool WriteToBuffer( unsigned char *pImageData, CUtlBuffer &buffer, int width, int height, 
						ImageFormat srcFormat, ImageFormat dstFormat );


	// write out a simple tga file from a memory buffer.
	[[nodiscard]] bool WriteTGAFile( const char *fileName, int width, int height, ImageFormat srcFormat, uint8 const *srcData, int nStride );

	// A pair of routines for writing to files without allocating any memory in the TGA writer
	// Useful for very large files such as posters, which are rendered as sub-rects anyway
	[[nodiscard]] bool WriteDummyFileNoAlloc( const char *fileName, int width, int height, ImageFormat dstFormat );
	[[nodiscard]] bool WriteRectNoAlloc( unsigned char *pImageData, const char *fileName, int nXOrigin, int nYOrigin, int width, int height, int nStride, ImageFormat srcFormat );
}  // namespace TGAWriter

#endif // TGAWRITER_H
