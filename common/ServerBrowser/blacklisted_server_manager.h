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
#include "tier1/netadr.h"
#include "tier1/utlvector.h"

constexpr inline char BLACKLIST_DEFAULT_SAVE_FILE[]{"cfg/server_blacklist.txt"};

class gameserveritem_t;

struct blacklisted_server_t 
{
	// dimhotepus: int -> unsigned.
	unsigned m_nServerID;
	char m_szServerName[64];
	// dimhotepus: uint32 -> time_t
	time_t m_ulTimeBlacklistedAt;
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
	// dimhotepus: int port -> uint16 port.
	blacklisted_server_t *AddServer( const char *serverName, uint32 serverIP, uint16 serverPort );
	// dimhotepus: uint32 -> time_t
	blacklisted_server_t *AddServer( const char *serverName, const char *netAddressString, time_t timestamp );

	void RemoveServer( unsigned iServerID );		// remove server with matching 'server id' from list

	void SaveToFile( const char *filename );
	int LoadServersFromFile( const char *pszFilename, bool bResetTimes );		// returns count of appended servers, zero for failure

	blacklisted_server_t *GetServer( unsigned iServerID );		// return server with matching 'server id'
	[[nodiscard]] intp GetServerCount() const;

	[[nodiscard]] const CUtlVector< blacklisted_server_t > &GetServerVector() const;

	[[nodiscard]] bool IsServerBlacklisted( const gameserveritem_t &server ) const;
	// dimhotepus: int port -> uint16 port.
	bool IsServerBlacklisted( uint32 serverIP, uint16 serverPort, const char *serverName ) const;

	bool CanServerBeBlacklisted( gameserveritem_t &server ) const;
	// dimhotepus: int port -> uint16 port.
	bool CanServerBeBlacklisted( uint32 serverIP, uint16 serverPort, const char *serverName ) const;

private:
	CUtlVector< blacklisted_server_t >	m_Blacklist;
	// dimhotepus: int -> unsigned
	unsigned m_iNextServerID;		// for vgui use
};

inline intp CBlacklistedServerManager::GetServerCount() const
{
	return m_Blacklist.Count();
}




#endif // BLACKLISTEDSERVERMANAGER_H
