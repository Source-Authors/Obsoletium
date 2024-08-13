//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef FRIENDSGAMES_H
#define FRIENDSGAMES_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Favorite games list
//-----------------------------------------------------------------------------
class CFriendsGames : public CBaseGamesPage
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CFriendsGames, CBaseGamesPage );

public:
	CFriendsGames(vgui::Panel *parent);
	~CFriendsGames();

	// IGameList handlers
	// returns true if the game list supports the specified ui elements
	bool SupportsItem(InterfaceItem_e item) override;

	// called when the current refresh list is complete
	void RefreshComplete( HServerListRequest hReq, EMatchMakingServerResponse response ) override;

private:
	// context menu message handlers
	MESSAGE_FUNC_INT( OnOpenContextMenu, "OpenContextMenu", itemID );

	int m_iServerRefreshCount;	// number of servers refreshed
};

#endif // FRIENDSGAMES_H
