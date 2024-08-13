//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef INTERNETGAMES_H
#define INTERNETGAMES_H
#ifdef _WIN32
#pragma once
#endif

#include "BaseGamesPage.h"

//-----------------------------------------------------------------------------
// Purpose: Internet games list
//-----------------------------------------------------------------------------
class CInternetGames : public CBaseGamesPage
{

	DECLARE_CLASS_SIMPLE_OVERRIDE( CInternetGames, CBaseGamesPage );

public:
	CInternetGames( vgui::Panel *parent, const char *panelName = "InternetGames", EPageType eType = eInternetServer );
	~CInternetGames();

	// property page handlers
	void OnPageShow() override;

	// returns true if the game list supports the specified ui elements
	bool SupportsItem(IGameList::InterfaceItem_e item) override;

	// gets a new server list
	MESSAGE_FUNC_OVERRIDE( GetNewServerList, "GetNewServerList" );

	// serverlist refresh responses
	void ServerResponded( HServerListRequest hReq, int iServer ) override;
	void ServerFailedToRespond( HServerListRequest hReq, int iServer ) override;
	void RefreshComplete( HServerListRequest hReq, EMatchMakingServerResponse response ) override;
	MESSAGE_FUNC_INT_OVERRIDE( OnRefreshServer, "RefreshServer", serverID );

	int GetRegionCodeToFilter() override;
	bool CheckTagFilter( gameserveritem_t &server ) override;

protected:
	// vgui overrides
	void PerformLayout() override;
	void OnTick() override;

	virtual const char *GetStringNoUnfilteredServers() { return "#ServerBrowser_NoInternetGames"; }
	virtual const char *GetStringNoUnfilteredServersOnMaster() { return "#ServerBrowser_MasterServerHasNoServersListed"; }
	virtual const char *GetStringNoServersResponded() { return "#ServerBrowser_NoInternetGamesResponded"; }

private:
	// Called once per frame to see if sorting needs to occur again
	void CheckRedoSort();
	// Called once per frame to check re-send request to master server
	void CheckRetryRequest( ESteamServerType serverType );
	// opens context menu (user right clicked on a server)
	MESSAGE_FUNC_INT( OnOpenContextMenu, "OpenContextMenu", itemID );

	struct regions_s
	{
		CUtlSymbol name;
		unsigned char code;
	};

	CUtlVector<struct regions_s> m_Regions;	// list of the different regions you can query for

	float				m_fLastSort;	// Time of last re-sort
	bool				m_bDirty;	// Has the list been modified, thereby needing re-sort
	bool				m_bRequireUpdate;	// checks whether we need an update upon opening
	
	// error cases for if no servers are listed
	bool				m_bAnyServersRetrievedFromMaster;
	bool				m_bAnyServersRespondedToQuery;
	bool				m_bNoServersListedOnMaster;

	bool m_bOfflineMode;
};

#endif // INTERNETGAMES_H
