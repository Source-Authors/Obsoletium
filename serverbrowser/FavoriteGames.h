//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef FAVORITEGAMES_H
#define FAVORITEGAMES_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Favorite games list
//-----------------------------------------------------------------------------
class CFavoriteGames final : public CBaseGamesPage
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CFavoriteGames, CBaseGamesPage );

public:
	CFavoriteGames(vgui::Panel *parent);
	~CFavoriteGames();

	// favorites list, loads/saves into keyvalues
	void LoadFavoritesList();
	
	// IGameList handlers
	// returns true if the game list supports the specified ui elements
	bool SupportsItem(InterfaceItem_e item) override;

	// called when the current refresh list is complete
	void RefreshComplete( HServerListRequest hReq, EMatchMakingServerResponse response ) override;

	// passed from main server browser window instead of messages
	void OnConnectToGame();
	void OnDisconnectFromGame( void );

	void SetRefreshOnReload() { m_bRefreshOnListReload = true; }

private:
	// context menu message handlers
	MESSAGE_FUNC_INT( OnOpenContextMenu, "OpenContextMenu", itemID );
	MESSAGE_FUNC( OnRemoveFromFavorites, "RemoveFromFavorites" );
	MESSAGE_FUNC( OnAddServerByName, "AddServerByName" );

	void OnAddCurrentServer( void );

	void OnCommand(const char *command) override;

	bool m_bRefreshOnListReload;
};


#endif // FAVORITEGAMES_H
