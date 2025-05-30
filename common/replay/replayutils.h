//========= Copyright Valve Corporation, All rights reserved. ============//
//
//----------------------------------------------------------------------------------------

#ifndef REPLAYUTILS_H
#define REPLAYUTILS_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlstring.h"

void Replay_GetFirstAvailableFilename( OUT_Z_CAP(nDstLen) char *pDst, intp nDstLen, const char *pIdealFilename, const char *pExt,
									   const char *pFilePath, intp nStartIndex );
template<intp destSize>
void Replay_GetFirstAvailableFilename( OUT_Z_ARRAY char (&pDst)[destSize], const char *pIdealFilename, const char *pExt,
									   const char *pFilePath, intp nStartIndex )
{
  Replay_GetFirstAvailableFilename( pDst, destSize, pIdealFilename, pExt, pFilePath, nStartIndex );
}

void Replay_ConstructReplayFilenameString( CUtlString &strOut, const char *pReplaySubDir, const char *pFilename, const char *pGameDir );

//----------------------------------------------------------------------------------------
// Util function, copied from src/engine/common.cpp
//----------------------------------------------------------------------------------------
char *Replay_va( PRINTF_FORMAT_STRING const char *format, ... );

//----------------------------------------------------------------------------------------
// Return the base dir, e.g. "c:\...\game\tf\replays\"
//----------------------------------------------------------------------------------------
const char *Replay_GetBaseDir();

//----------------------------------------------------------------------------------------
// Set the game directory (only to be called from ReplayLib_Init())
//----------------------------------------------------------------------------------------
void Replay_SetGameDir( const char *pGameDir );

//----------------------------------------------------------------------------------------
// Return the base dir, e.g. "c:\...\game\tf\replays\"
//----------------------------------------------------------------------------------------
const char *Replay_GetGameDir();

//----------------------------------------------------------------------------------------
// Get a name of the format "<map>: <current date & time>" - used for replays and takes.
//----------------------------------------------------------------------------------------
void Replay_GetAutoName( OUT_Z_BYTECAP(nDestSizeInBytes) wchar_t *pDest, int nDestSizeInBytes, const char *pMapName );

template<int destSize>
void Replay_GetAutoName( OUT_Z_ARRAY wchar_t (&pDest)[destSize], const char *pMapName )
{
	Replay_GetAutoName( pDest, destSize, pMapName );
}

#endif // REPLAY_H
