//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "blacklisted_server_manager.h"

#include "tier1/convar.h"
#include "tier1/KeyValues.h"
#include "filesystem.h"
#include "steam/steamclientpublic.h"
#include "steam/matchmakingtypes.h"


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBlacklistedServerManager::CBlacklistedServerManager()
{
	m_iNextServerID = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Reset the list of blacklisted servers to empty.
//-----------------------------------------------------------------------------
void CBlacklistedServerManager::Reset()
{
	m_Blacklist.RemoveAll();
	m_iNextServerID = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Appends all the servers inside the specified file to the blacklist.
// Returns count of appended servers, zero for failure.
//-----------------------------------------------------------------------------
int CBlacklistedServerManager::LoadServersFromFile( const char *pszFilename, bool bResetTimes )
{
	KeyValuesAD pKV( "serverblacklist" );
	if ( !pKV->LoadFromFile( g_pFullFileSystem, pszFilename, "MOD" ) )
		return 0;

	int count = 0;

	for ( KeyValues *pData = pKV->GetFirstSubKey(); pData != nullptr; pData = pData->GetNextKey() )
	{
		const char *pszName = pData->GetString( "name" );
		// dimhotepus: uint32 -> time_t
		time_t ulDate = pData->GetUint64( "date" );
		if ( bResetTimes )
		{
			time_t today;
			time( &today );
			ulDate = today;
		}

		const char *pszNetAddr = pData->GetString( "addr" );
		if ( pszNetAddr && pszNetAddr[0] && pszName && pszName[0] )
		{
			netadr_t netadr;
			if ( !netadr.SetFromString(pszNetAddr) )
			{
				Warning( "%s: %s is not a IPv4 address, skipping add to black list.\n", pszFilename, pszNetAddr );
				continue;
			}

			auto iIdx = m_Blacklist.AddToTail();

			m_Blacklist[iIdx].m_nServerID = m_iNextServerID++;
			V_strcpy_safe( m_Blacklist[iIdx].m_szServerName, pszName );
			m_Blacklist[iIdx].m_ulTimeBlacklistedAt = ulDate;
			m_Blacklist[iIdx].m_NetAdr = netadr;

			++count;
		}
	}

	return count;
}


//-----------------------------------------------------------------------------
// Purpose: save blacklist to disk
//-----------------------------------------------------------------------------
void CBlacklistedServerManager::SaveToFile( const char *pszFilename )
{
	KeyValuesAD pKV( "serverblacklist" );

	for ( auto &bl : m_Blacklist )
	{
		auto *pSubKey = new KeyValues( "server" );
		pSubKey->SetString( "name", bl.m_szServerName );
		// dimhotepus: uint32 -> time_t
		pSubKey->SetUint64( "date", bl.m_ulTimeBlacklistedAt );
		pSubKey->SetString( "addr", bl.m_NetAdr.ToString() );
		pKV->AddSubKey( pSubKey );
	}

	pKV->SaveToFile( g_pFullFileSystem, pszFilename, "MOD" );
}


//-----------------------------------------------------------------------------
// Purpose: Add the given server to the blacklist. Return added server.
//-----------------------------------------------------------------------------
blacklisted_server_t *CBlacklistedServerManager::AddServer( gameserveritem_t &server )
{
	// Make sure we don't already have this IP in the list somewhere
	netadr_t netAdr( server.m_NetAdr.GetIP(), server.m_NetAdr.GetConnectionPort() );

	// Don't let them add reserved addresses to their blacklists
 	if ( netAdr.IsReservedAdr() )
 		return nullptr;

	auto iIdx = m_Blacklist.AddToTail();
	V_strcpy_safe( m_Blacklist[iIdx].m_szServerName, server.GetName() );

	time_t today;
	time( &today );
	m_Blacklist[iIdx].m_ulTimeBlacklistedAt = today;
	m_Blacklist[iIdx].m_NetAdr = netAdr;
	m_Blacklist[iIdx].m_nServerID = m_iNextServerID++;

	return &m_Blacklist[iIdx];
}


//-----------------------------------------------------------------------------
// Purpose: Add the given server to the blacklist. Return added server.
//-----------------------------------------------------------------------------
blacklisted_server_t *CBlacklistedServerManager::AddServer( const char *serverName, uint32 serverIP, uint16 serverPort )
{
	netadr_t netAdr( serverIP, serverPort );

	// Don't let them add reserved addresses to their blacklists
 	if ( netAdr.IsReservedAdr() )
 		return nullptr;

	auto iIdx = m_Blacklist.AddToTail();
	V_strcpy_safe( m_Blacklist[iIdx].m_szServerName, serverName );

	time_t today;
	time( &today );
	m_Blacklist[iIdx].m_ulTimeBlacklistedAt = today;
	m_Blacklist[iIdx].m_NetAdr = netAdr;
	m_Blacklist[iIdx].m_nServerID = m_iNextServerID++;

	return &m_Blacklist[iIdx];
}


//-----------------------------------------------------------------------------
// Purpose: Add the given server to the blacklist. Return added server.
//-----------------------------------------------------------------------------
// dimhotepus: uint32 -> time_t
blacklisted_server_t *CBlacklistedServerManager::AddServer( const char *serverName, const char *netAddressString, time_t timestamp )
{
	netadr_t netAdr;
	// dimhotepus: If not net address, return.
	if ( !netAdr.SetFromString( netAddressString ) )
		return nullptr;

	// Don't let them add reserved addresses to their blacklists
	if ( netAdr.IsReservedAdr() )
		return nullptr;

	auto iIdx = m_Blacklist.AddToTail();
	V_strcpy_safe( m_Blacklist[iIdx].m_szServerName, serverName );

	m_Blacklist[iIdx].m_ulTimeBlacklistedAt = timestamp;
	m_Blacklist[iIdx].m_NetAdr = netAdr;
	m_Blacklist[iIdx].m_nServerID = m_iNextServerID++;

	return &m_Blacklist[iIdx];
}


//-----------------------------------------------------------------------------
// Purpose: Remove server with matching 'server id' from list
//-----------------------------------------------------------------------------
void CBlacklistedServerManager::RemoveServer( unsigned iServerID )
{
	for ( intp i = 0; i < m_Blacklist.Count(); i++ )
	{
		if ( m_Blacklist[i].m_nServerID == iServerID )
		{
			m_Blacklist.Remove(i);
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Given a serverID, return its blacklist entry
//-----------------------------------------------------------------------------
blacklisted_server_t *CBlacklistedServerManager::GetServer( unsigned iServerID )
{
	for ( auto &bl : m_Blacklist )
	{
		if ( bl.m_nServerID == iServerID )
			return &bl;
	}

	return nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if given server is blacklisted
//-----------------------------------------------------------------------------
bool CBlacklistedServerManager::IsServerBlacklisted( const gameserveritem_t &server ) const
{
	return IsServerBlacklisted( server.m_NetAdr.GetIP(), server.m_NetAdr.GetConnectionPort(), server.GetName() );
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if given server is blacklisted
//-----------------------------------------------------------------------------
bool CBlacklistedServerManager::IsServerBlacklisted( uint32 serverIP, uint16 serverPort, const char *serverName ) const
{
	netadr_t netAdr( serverIP, serverPort );

	ConVarRef sb_showblacklists( "sb_showblacklists" );

	for ( auto &bl : m_Blacklist )
	{
		if ( bl.m_NetAdr.ip[3] == 0 )
		{
			if ( bl.m_NetAdr.CompareClassCAdr( netAdr ) )
			{
				if ( sb_showblacklists.IsValid() && sb_showblacklists.GetBool() )
				{
					Msg( "Blacklisted '%s' (%s), due to rule '%s' (Class C).\n", serverName, netAdr.ToString(), bl.m_NetAdr.ToString() );
				}
				return true;
			}
		}
		else
		{
			if ( bl.m_NetAdr.CompareAdr( netAdr, (bl.m_NetAdr.GetPort() == 0) ) )
			{
				if ( sb_showblacklists.IsValid() && sb_showblacklists.GetBool() )
				{
					Msg( "Blacklisted '%s' (%s), due to rule '%s'.\n", serverName, netAdr.ToString(), bl.m_NetAdr.ToString() );
				}
				return true;
			}
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the given server is allowed to be blacklisted at all
//-----------------------------------------------------------------------------
bool CBlacklistedServerManager::CanServerBeBlacklisted( gameserveritem_t &server ) const
{
	return CanServerBeBlacklisted( server.m_NetAdr.GetIP(), server.m_NetAdr.GetConnectionPort(), server.GetName() );
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the given server is allowed to be blacklisted at all
//-----------------------------------------------------------------------------
bool CBlacklistedServerManager::CanServerBeBlacklisted( uint32 serverIP, uint16 serverPort, const char *serverName ) const
{
	netadr_t netAdr( serverIP, serverPort );

	if ( !netAdr.IsValid() )
		return false;

	// Don't let them add reserved addresses to their blacklists
	if ( netAdr.IsReservedAdr() )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Returns vector of blacklisted servers
//-----------------------------------------------------------------------------
const CUtlVector< blacklisted_server_t > &CBlacklistedServerManager::GetServerVector() const
{
	return m_Blacklist;
}
