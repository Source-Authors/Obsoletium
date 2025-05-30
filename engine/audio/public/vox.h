//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef VOX_H
#define VOX_H
#pragma once

#include "tier0/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sfxcache_t;
struct channel_t;

class CUtlSymbol;

extern void				VOX_Init( void );
extern void 			VOX_Shutdown( void );
extern void				VOX_ReadSentenceFile( const char *psentenceFileName );
extern int				VOX_SentenceCount( void );
extern void				VOX_LoadSound( channel_t *pchan, const char *psz );
// UNDONE: Improve the interface of this call, it returns sentence data AND the sentence index
extern char				*VOX_LookupString( const char *pSentenceName, int *psentencenum, bool *pbEmitCaption = NULL, CUtlSymbol *pCaptionSymbol = NULL, float * pflDuration = NULL );
extern void				VOX_PrecacheSentenceGroup( class IEngineSound *pSoundSystem, const char *pGroupName, const char *pPathOverride = NULL );
extern const char		*VOX_SentenceNameFromIndex( int sentencenum );
extern float			VOX_SentenceLength( int sentence_num );
extern const char		*VOX_GroupNameFromIndex( int groupIndex );
extern int				VOX_GroupIndexFromName( const char *pGroupName );
extern int				VOX_GroupPick( intp isentenceg, OUT_Z_CAP(strLen) char *szfound, intp strLen );
extern int				VOX_GroupPickSequential( intp isentenceg, OUT_Z_CAP(szfoundLen) char *szfound, intp szfoundLen, int ipick, int freset );

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
template<intp strLen>
inline int VOX_GroupPick( intp isentenceg, OUT_Z_ARRAY char (&szfound)[strLen] )
{
	return VOX_GroupPick( isentenceg, szfound, strLen );
}

template <intp szfoundLen>
int VOX_GroupPickSequential( intp isentenceg, OUT_Z_ARRAY char (&szfound)[szfoundLen], int ipick, int freset )
{
	return VOX_GroupPickSequential( isentenceg, szfound, szfoundLen, ipick, freset );
}

#endif

#endif // VOX_H
