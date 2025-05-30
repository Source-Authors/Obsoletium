//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef POINT_BONUSMAPS_ACCESSOR_H
#define POINT_BONUSMAPS_ACCESSOR_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"

void BonusMapChallengeUpdate( const char *pchFileName, const char *pchMapName, const char *pchChallengeName, int iBest );

void BonusMapChallengeNames( OUT_Z_CAP(fileSize) char *pchFileName, intp fileSize,
	OUT_Z_CAP(mapSize) char *pchMapName, intp mapSize,
	OUT_Z_CAP(challengeSize) char *pchChallengeName, intp challengeSize );
template<intp fileSize, intp mapSize, intp challengeSize>
void BonusMapChallengeNames( OUT_Z_ARRAY char (&pchFileName)[fileSize],
	OUT_Z_ARRAY char (&pchMapName)[mapSize], 
	OUT_Z_ARRAY char (&pchChallengeName)[challengeSize] )
{
	BonusMapChallengeNames( pchFileName, fileSize,
		pchMapName, mapSize,
		pchChallengeName, challengeSize );
}

void BonusMapChallengeObjectives( int &iBronze, int &iSilver, int &iGold );


#endif		// POINT_BONUSMAPS_ACCESSOR_H
