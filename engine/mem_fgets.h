//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( MEM_FGETS_H )
#define MEM_FGETS_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/annotations.h"
#include "tier0/basetypes.h"

char *memfgets( IN_CAP(fileSize) unsigned char *pMemFile, intp fileSize, intp *pFilePos, OUT_Z_CAP(bufferSize) char *pBuffer, intp bufferSize );

template<intp size>
char *memfgets( IN_CAP(fileSize) unsigned char *pMemFile, intp fileSize, intp *pFilePos, OUT_Z_ARRAY char (&pBuffer)[size] )
{
	return memfgets( pMemFile, fileSize, pFilePos, pBuffer, size );
}


#endif // MEM_FGETS_H