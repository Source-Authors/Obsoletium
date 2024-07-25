// Copyright Valve Corporation, All rights reserved.
//
// Purpose: StudioMDL byteswapping functions.

#ifndef STUDIOBYTESWAP_H
#define STUDIOBYTESWAP_H

#include "tier1/byteswap.h"

struct studiohdr_t;
class IPhysicsCollision;

namespace StudioByteSwap
{

using CompressFunc_t = bool (*)( const void *in, int in_size, void **out, int *out_size );

// void SetTargetBigEndian( bool bigEndian );
void	ActivateByteSwapping( bool bActivate );
void	SourceIsNative( bool bActivate );
void	SetVerbose( bool bVerbose );
void	SetCollisionInterface( IPhysicsCollision *pPhysicsCollision );

intp		ByteswapStudioFile( const char *pFilename, void *pOutBase, const void *pFileBase, intp fileSize, studiohdr_t *pHdr, CompressFunc_t pCompressFunc = NULL );
intp		ByteswapPHY( void *pOutBase, const void *pFileBase, intp fileSize );
intp		ByteswapANI( studiohdr_t* pHdr, void *pOutBase, const void *pFileBase, intp filesize );
intp		ByteswapVVD( void *pOutBase, const void *pFileBase, intp fileSize );
intp		ByteswapVTX( void *pOutBase, const void *pFileBase, intp fileSize );
intp		ByteswapMDL( void *pOutBase, const void *pFileBase, intp fileSize );

#define BYTESWAP_ALIGNMENT_PADDING		4096

}

#endif // STUDIOBYTESWAP_H
