//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef BLACKLISTEDSERVERMANAGER_H
#define BLACKLISTEDSERVERMANAGER_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "netadr.h"
#include "tier1/utlvector.h"

constexpr inline char BLACKLIST_DEFAULT_SAVE_FILE[]{"cfg/server_blacklist.txt"};

class gameserveritem_t;

struct blacklisted_server_t 
{
	int m_nServerID;
	char m_szServerName[64];
	uint32 m_ulTimeBlacklistedAt;
	netadr_t m_NetAdr;
};


//-----------------------------------------------------------------------------
// Purpose: Collection of blacklisted servers
//-----------------------------------------------------------------------------
class CBlacklistedServerManager final
{
public:
	CBlacklistedServerManager();

	void Reset();

	blacklisted_server_t *AddServer( gameserveritem_t &server );
	blacklisted_server_t *AddServer( const char *serverName, uint32 serverIP, int serverPort );
	blacklisted_server_t *AddServer( const char *serverName, const char *netAddressString, uint32 timestamp );

	void RemoveServer( intp iServerID );		// remove server with matching 'server id' from list

	void SaveToFile( const char *filename );
	int LoadServersFromFile( const char *pszFilename, bool bResetTimes );		// returns count of appended servers, zero for failure

	blacklisted_server_t *GetServer( intp iServerID );		// return server with matching 'server id'
	[[nodiscard]] intp GetServerCount() const;

	[[nodiscard]] const CUtlVector< blacklisted_server_t > &GetServerVector() const;

	[[nodiscard]] bool IsServerBlacklisted( const gameserveritem_t &server ) const;
	bool IsServerBlacklisted( uint32 serverIP, int serverPort, const char *serverName ) const;

	bool CanServerBeBlacklisted( gameserveritem_t &server ) const;
	bool CanServerBeBlacklisted( uint32 serverIP, int serverPort, const char *serverName ) const;

private:
	CUtlVector< blacklisted_server_t >	m_Blacklist;
	int m_iNextServerID;		// for vgui use
};

inline intp CBlacklistedServerManager::GetServerCount() const
{
	return m_Blacklist.Count();
}




#endif // BLACKLISTEDSERVERMANAGER_H
