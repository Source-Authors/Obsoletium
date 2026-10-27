//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include <sys/types.h>
#include <sys/stat.h>

#include <cerrno>
#include <cstdlib>

#include "vtf/vtf.h"
#include "tier0/platform.h"
#include "tier1/utlbuffer.h"

int main( int argc, char **argv )
{
	if( argc != 5 )
	{
		fprintf( stderr, "Usage: vtfscreen blah.vtf 0 1 2\n" );
		return EINVAL;
	}

	const char *pSrcFilename = argv[1];
	int rOffset = atoi( argv[2] );
	int gOffset = atoi( argv[3] );
	int bOffset = atoi( argv[4] );
	CUtlBuffer srcData;

	_stat buf;
	if( _stat( pSrcFilename, &buf ) == -1 )
	{
		const int rc{errno};
		fprintf( stderr, "Couldn't stat %s: %s.\n", pSrcFilename, strerror(rc) );
		return rc;
	}

	intp srcFileSize = buf.st_size;
	srcData.EnsureCapacity( srcFileSize );

	FILE *fp = fopen( pSrcFilename, "rb" );
	if( !fp )
	{
		const int rc{errno};
		fprintf( stderr, "Couldn't open %s for reading: %s\n", pSrcFilename, strerror(rc) );
		return rc;
	}	

	intp nBytesRead = fread( srcData.Base(), 1, srcFileSize, fp );
	fclose( fp );

	srcData.SeekPut( CUtlBuffer::SEEK_HEAD, nBytesRead );

	IVTFTexture *pSrcTexture = CreateVTFTexture();
	RunCodeAtScopeExit( DestroyVTFTexture( pSrcTexture) );
	
	pSrcTexture->Unserialize( srcData );
	ImageFormat srcFormat = pSrcTexture->Format();
	pSrcTexture->ConvertImageFormat( IMAGE_FORMAT_DEFAULT, false );

	intp totalSize = pSrcTexture->ComputeTotalSize();

	unsigned char *pImageData = pSrcTexture->ImageData();
	for( intp i = 0; i < totalSize; i += 4 )
	{
#if 0
		// ATI BEFORE E32003
		int r = ( int )pImageData[i+0];
		int g = ( int )pImageData[i+1];
		int b = ( int )pImageData[i+2];

		int grey = r + g + b;
		grey = grey / 3;
		pImageData[i+0] = 0;
		pImageData[i+1] = grey;
		pImageData[i+2] = 0;
#endif
#if 0
		// NVIDIA - first e3 build
		pImageData[i+1] = 0;
		pImageData[i+2] = 0;
#endif
#if 0
		// ATI e3 build on . .get the green component and stomp all channels with it.
		pImageData[i+0] = pImageData[i+1];
		pImageData[i+2] = pImageData[i+1];
#endif
#if 0
		// Intel . .get the blue component and stomp all channels with it.
		pImageData[i+0] = pImageData[i+2];
		pImageData[i+1] = pImageData[i+2];
#endif
		// leave alpha alone
		int tmpR = pImageData[i+rOffset];
		int tmpG = pImageData[i+gOffset];
		int tmpB = pImageData[i+bOffset];
		pImageData[i+0] = tmpR;
		pImageData[i+1] = tmpG;
		pImageData[i+2] = tmpB;
	}

	pSrcTexture->ConvertImageFormat( srcFormat, false );

	CUtlBuffer dstData;
	pSrcTexture->Serialize( dstData );

	if ( fp = fopen( pSrcFilename, "wb" ); !fp )
	{
		const int rc{errno};
		fprintf( stderr, "Couldn't open %s for writing: %s\n", pSrcFilename, strerror(rc) );
		return rc;
	}

	RunCodeAtScopeExit( fclose( fp ) );

	fwrite( dstData.Base(), 1, dstData.TellPut(), fp );

	return 0;
}
