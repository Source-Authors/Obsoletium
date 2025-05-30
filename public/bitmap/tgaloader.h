//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#ifndef TGALOADER_H
#define TGALOADER_H

#ifdef _WIN32
#pragma once
#endif

#include "bitmap/imageformat.h"
#include "tier1/utlmemory.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CUtlBuffer;


namespace TGALoader
{

[[nodiscard]] int TGAHeaderSize();

[[nodiscard]] bool GetInfo( const char *fileName, int *width, int *height, ImageFormat *imageFormat, float *sourceGamma );
[[nodiscard]] bool GetInfo( CUtlBuffer &buf, int *width, int *height, ImageFormat *imageFormat, float *sourceGamma );

[[nodiscard]] bool Load( unsigned char *imageData, const char *fileName, int width, int height, 
		   ImageFormat imageFormat, float targetGamma, bool mipmap );
[[nodiscard]] bool Load( unsigned char *imageData, CUtlBuffer &buf, int width, int height, 
			ImageFormat imageFormat, float targetGamma, bool mipmap );

[[nodiscard]] bool LoadRGBA8888( const char *pFileName, CUtlMemory<unsigned char> &outputData, int &outWidth, int &outHeight );
[[nodiscard]] bool LoadRGBA8888( CUtlBuffer &buf, CUtlMemory<unsigned char> &outputData, int &outWidth, int &outHeight );

} // end namespace TGALoader

#endif // TGALOADER_H
