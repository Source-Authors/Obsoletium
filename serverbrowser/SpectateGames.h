//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef SPECTATEGAMES_H
#define SPECTATEGAMES_H
#ifdef _WIN32
#pragma once
#endif

#include "InternetGames.h"

//-----------------------------------------------------------------------------
// Purpose: Spectator games list
//-----------------------------------------------------------------------------
class CSpectateGames final : public CInternetGames
{
public:
	CSpectateGames(vgui::Panel *parent);

	// property page handlers
	void OnPageShow() override;

	bool CheckTagFilter( gameserveritem_t &server ) override;

protected:
	// filters by spectator games
	void GetNewServerList() override;

private:
	typedef CInternetGames BaseClass;
};


#endif // SPECTATEGAMES_H
