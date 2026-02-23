//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//


#include "server_pch.h"
#include "sv_filter.h"

#include "vfilter.h" // Renamed to avoid conflict with Microsoft's filter.h
#include "sv_steamauth.h"
#include "GameEventManager.h"
#include "proto_oob.h"
#include "tier1/CommandBuffer.h"
#ifndef DEDICATED
#include "cl_steamauth.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// dimhotepus: Restrict values to 0...1.
static ConVar sv_filterban( "sv_filterban", "1", 0, "Set packet filtering by IP mode", true, 0, true, 1 );

CUtlVector< ipfilter_t > g_IPFilters;
CUtlVector< userfilter_t > g_UserFilters;
#if defined(WIN32) || defined(_WIN32)
CThreadSpinRWLock g_IPFilterMutex;
#else
CThreadRWLock g_IPFilterMutex;
#endif

#define BANNED_IP_FILENAME "banned_ip.cfg"
#define BANNED_USER_FILENAME "banned_user.cfg"
#define CONFIG_DIR "cfg/"
#define STEAM_PREFIX "STEAM_"

//-----------------------------------------------------------------------------
// Purpose: Sends a message to prospective clients letting them know they're banned
// Input  : *adr - 
//-----------------------------------------------------------------------------
void Filter_SendBan( const netadr_t& adr )
{
	NET_OutOfBandPrintf( NS_SERVER, adr, "%cBanned by server\n", A2A_PRINT );
}

//-----------------------------------------------------------------------------
// Purpose: Checks an IP address to see if it is banned
// Input  : *adr - 
// Output : bool
// RaphaelIT7: This function is fully threadsafe in case the network layer gets threaded
//-----------------------------------------------------------------------------
bool Filter_ShouldDiscard( const netadr_t& adr )
{
	if ( sv_filterban.GetInt() == 0 )
	{
		return false;
	}

	const bool bNegativeFilter = sv_filterban.GetInt() == 1;

	unsigned in = *(unsigned *)&adr.ip[0];

	AUTO_LOCK_READ( g_IPFilterMutex );

	// Handle timeouts 
	for ( intp i = g_IPFilters.Count() - 1 ; i >= 0 ; i--)
	{
		if ( ( g_IPFilters[i].compare != 0xffffffff) &&
			 ( g_IPFilters[i].banEndTime != 0.0f ) &&
			 ( g_IPFilters[i].banEndTime <= realtime ) )
		{
			g_IPFilters.Remove(i);
			continue;
		}

		// Only get here if ban is still in effect.
		if ( (in & g_IPFilters[i].mask) == g_IPFilters[i].compare)
		{
			return bNegativeFilter;
		}
	}

	return !bNegativeFilter;
}

//-----------------------------------------------------------------------------
// Purpose: Takes an IP address string and fills in an ipfilter_t mask and compare (raw address)
// Input  : *s - 
//			*f - 
// Output : bool Filter_ConvertString
//-----------------------------------------------------------------------------
static bool Filter_ConvertString( const char *s, ipfilter_t *f )
{
	char	num[16];
	alignas(unsigned) byte	b[4];
	alignas(unsigned) byte	m[4];
	
	int i;
	for (i=0 ; i<4 ; i++)
	{
		b[i] = 0;
		m[i] = 0;
	}
	
	for (i=0 ; i<4 ; i++)
	{
		if (*s < '0' || *s > '9')
		{
			ConMsg("Bad filter address: %s.\n", s);
			return false;
		}
		
		int j = 0;
		while (j < ssize(num) && *s >= '0' && *s <= '9')
		{
			num[j++] = *s++;
		}

		if (j == ssize(num))
		{
			ConMsg("Bad filter address: %s.\n", s);
			return false;
		}

		num[j] = '\0';

		b[i] = atoi(num);
		if (b[i] != 0)
			m[i] = 255;

		if (!*s)
			break;

		if (s[0] != '.')
		{
			ConMsg("Bad filter address, expect ddd.ddd.ddd.ddd\n");
			return false;
		}

		s++;
	}
	
	f->mask = *(unsigned int *)m;
	f->compare = *(unsigned int *)b;
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Adds an IP ban
//-----------------------------------------------------------------------------
static void Filter_Add_f( const CCommand& args )
{
	bool		bKick = true;
	bool		bFound = false;
	char		szDuration[256];
	CGameClient *client = NULL;

	if ( !Q_stricmp( args[0], "banip" ) )
	{
		ConWarning( "Please use \"addip\" instead of \"banip\".\n" );
	}

	if ( args.ArgC() != 3 )
	{
		ConMsg( "Usage: addip <minutes> <ipaddress>\nUse 0 minutes for permanent.\n" );
		return;
	}

	// dimhotepus: atof -> strtod
	char *end = nullptr;
	double banTime = strtod( args[1], &end );
	if ( !Q_isempty(end) )
	{
		ConMsg( "Usage: addip <minutes> <ipaddress>\nUse 0 minutes for permanent.\n" );
		return;
	}

	ipfilter_t	f;
	if ( !Filter_ConvertString( args[2], &f ) )
		return;

	intp i;
	{ // Scoped due to a possible dead lock since Filter_ShouldDiscard further down locks for read
		AUTO_LOCK_WRITE( g_IPFilterMutex );
		g_IPFilterMutex.LockForWrite();

		for (i=0 ; i<g_IPFilters.Count(); i++)
		{
			if ( g_IPFilters[i].compare == 0xffffffff ||
				( g_IPFilters[i].compare == f.compare && g_IPFilters[i].mask == f.mask ) )
				break;		// free spot
		}
	
		if (i == g_IPFilters.Count())
		{
			if (g_IPFilters.Count() == MAX_IPFILTERS)
			{
				ConMsg( "addip: IP filter list is full (max %zd).\n", MAX_IPFILTERS );
				g_IPFilterMutex.UnlockWrite();
				return;
			}

			i = g_IPFilters.AddToTail();
		}
		else
		{
			// updating in-place, so don't kick people
			bKick = false;
		}
	
		if (banTime < 0.01)
		{
			banTime = 0.0;
		}

		g_IPFilters[i].banTime = banTime;
		// Time to unban.
		g_IPFilters[i].banEndTime = ( banTime != 0.0 ) ? ( realtime + 60.0 * banTime ) : 0.0;

		if ( !Filter_ConvertString( args[2], &g_IPFilters[i]) )
		{
			g_IPFilters[i].compare = 0xffffffff;
		}
	}

	if ( bKick )
	{
		// Kick him if he's on
		for ( i=0; i < sv.GetClientCount(); ++i )
		{
			client = sv.Client(i);
			if ( !client || !client->IsActive() || !client->IsConnected() || !client->IsSpawned() )
				continue;

			if ( client->IsFakeClient() )
				continue;

			if ( !Filter_ShouldDiscard( client->GetNetChannel()->GetRemoteAddress() ) )
				continue;

			bFound = true;
			break;
		}
	}

	// Build a duration string for the ban
	if ( banTime == 0.0 )
	{
		V_sprintf_safe( szDuration, "permanently" );
	}
	else
	{
		V_sprintf_safe( szDuration, "for %.2f minutes", banTime );
	}
	
	// fire the event
	IGameEvent *event = g_GameEventManager.CreateEvent( "server_addban" );
	if ( event )
	{
		if ( bFound && client )
		{
			event->SetString( "name", client->m_Name );
			event->SetInt( "userid", client->GetUserID() );
			event->SetString( "networkid", client->GetNetworkIDString() );
		}
		else
		{
			event->SetString( "name", "" );
			event->SetInt( "userid", 0 );
			event->SetString( "networkid", "" );
		}

		event->SetString( "ip", args[2] );
		event->SetString( "duration", szDuration );
		event->SetString( "by", ( cmd_source == src_command ) ? "Console" : host_client->m_Name );
		event->SetBool( "kicked", bKick && bFound && client  );

		g_GameEventManager.FireEvent( event );
	}

	if ( bKick && bFound && client )
	{
		client->ClientPrintf ( "The server operator has added you to the banned list.\n" );
		client->Disconnect( "Added to banned list" );
	}
}

// IP Address filtering ConCommands
static ConCommand addip( "addip", Filter_Add_f, "Add an IP address to the ban list." );
static ConCommand banip( "banip", Filter_Add_f, "Add an IP address to the ban list (obsolete, use addip)." );


//-----------------------------------------------------------------------------
// Purpose: Removes an IP ban
//-----------------------------------------------------------------------------
CON_COMMAND( removeip, "Remove an IP address from the ban list." )
{
	if ( args.ArgC() < 1 )
	{
		ConMsg( "Usage: removeip <slot | ipaddress>\n" );
		return;
	}

	const char *arg = args[1];
	bool removed = false;
	char removedIP[32] = {0};
	const char *eventIP = nullptr;

	// RaphaelIT7: We scope it due to my preference to have it unlocked when firing the gameevent
	//             to avoid any potential deadlocks if any code may add a filter inside the gameevent.
	{
		AUTO_LOCK_WRITE( g_IPFilterMutex );

		// if no "." in the string we'll assume it's a slot number
		if ( !V_strstr( args[1], "." ) )
		{
			int slot = V_atoi( args[1] );
			if ( slot <= 0 || slot > g_IPFilters.Count() )
			{
				ConMsg( "removeip: Invalid slot %i\n", slot );
				return;
			}

			// array access is zero based
			byte b[4];
			memcpy( b, &g_IPFilters[--slot].compare, sizeof(b) );

			g_IPFilters.Remove( slot );
		
			V_sprintf_safe( removedIP, "%i.%i.%i.%i", b[0], b[1], b[2], b[3] );

			removed = true;
			eventIP = removedIP;
			return;
		}

		ipfilter_t	f;
		if ( !Filter_ConvertString( args[1], &f ) )
			return;

		for ( intp i = 0 ; i < g_IPFilters.Count() ; i++ )
		{
			if ( ( g_IPFilters[i].mask == f.mask ) &&
				 ( g_IPFilters[i].compare == f.compare ) )
			{
				g_IPFilters.Remove(i);

				removed = true;
				eventIP = arg;
				break;
			}
		}
	}

	if ( !removed )
	{
		ConMsg( "removeip: Couldn't find %s\n", arg );
		return;
	}

	ConMsg( "removeip: Filter removed for %s\n", eventIP );

	IGameEvent *event = g_GameEventManager.CreateEvent( "server_removeban" );
	if ( event )
	{
		event->SetString( "networkid", "" );
		event->SetString( "ip", eventIP );
		event->SetString( "by", ( cmd_source == src_command ) ? "Console" : host_client->m_Name );
		g_GameEventManager.FireEvent( event );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Lists IP bans
//-----------------------------------------------------------------------------
CON_COMMAND( listip, "List IP addresses on the ban list." )
{
	AUTO_LOCK_READ( g_IPFilterMutex );
	const intp count = g_IPFilters.Count();
	if ( !count )
	{
		ConMsg( "IP filter list: empty\n" );
		return;
	}
	else
	{
		ConMsg( "IP filter list: %zd entr%s\n", count, count == 1 ? "y" : "ies" );
	}
	
	byte b[4];
	for ( intp i = 0 ; i < count ; i++ )
	{
		memcpy( b, &g_IPFilters[i].compare, sizeof(b) );

		if ( g_IPFilters[i].banTime != 0.0 )
		{
			ConMsg( "%zi %i.%i.%i.%i: %.3f min\n", i+1, b[0], b[1], b[2], b[3], g_IPFilters[i].banTime );
		}
		else
		{
			ConMsg( "%zi %i.%i.%i.%i: permanent\n", i+1, b[0], b[1], b[2], b[3] );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Saves IP bans to a file
//-----------------------------------------------------------------------------
CON_COMMAND( writeip, "Save the ban list to " BANNED_IP_FILENAME "." )
{
	char	name[MAX_OSPATH];
	V_strcpy_safe( name, CONFIG_DIR BANNED_IP_FILENAME );

	ConMsg( "Writing %s.\n", name );

	FileHandle_t f = g_pFileSystem->Open(name, "wb");
	if ( !f )
	{
		ConMsg( "Couldn't open %s.\n", name );
		return;
	}

	RunCodeAtScopeExit(g_pFileSystem->Close(f));
	
	AUTO_LOCK_READ( g_IPFilterMutex );

	byte b[4];
	for ( const auto &ipf : g_IPFilters )
	{
		memcpy( b, &ipf.compare, sizeof(b) );

		// Only store out the permanent bad guys from this server.
		double banTime = ipf.banTime;
		if ( banTime != 0.0 )
		{
			continue;
		}

		g_pFileSystem->FPrintf( f, "addip 0 %i.%i.%i.%i\r\n", b[0], b[1], b[2], b[3] );
	}
}



//-----------------------------------------------------------------------------
// Purpose: Checks a USERID_t to see if the Steam ID has been banned
//-----------------------------------------------------------------------------
bool Filter_IsUserBanned( const USERID_t& userid )
{
	if ( sv_filterban.GetInt() == 0 )
		return false;

	bool bNegativeFilter = sv_filterban.GetInt() == 1;
	
	// Handle timeouts 
	for ( intp i = g_UserFilters.Count() - 1 ; i >= 0 ; i-- )
	{
		// Time out old filters
		if ( ( g_UserFilters[i].banEndTime != 0.0f ) &&
			 ( g_UserFilters[i].banEndTime <= realtime ) )
		{
			g_UserFilters.Remove( i );
			continue;
		}

		// Only get here if ban is still in effect.
		if ( Steam3Server().CompareUserID( userid, g_UserFilters[i].userid ) )
		{
			return bNegativeFilter;
		}
	}

	return !bNegativeFilter;
}

//-----------------------------------------------------------------------------
// Purpose: Converts a "STEAM_X:Y:Z" string into a USERID_t
//-----------------------------------------------------------------------------
USERID_t *Filter_StringToUserID( const char *str )
{
	static USERID_t id;
	Q_memset( &id, 0, sizeof( id ) );

	if ( str && !Q_isempty(str) )
	{
		if ( char szTemp[128]; !Q_strnicmp( str, STEAM_PREFIX, ssize( STEAM_PREFIX ) - 1 ) )
		{
			Q_strncpy( szTemp, str + ssize( STEAM_PREFIX ) - 1, sizeof( szTemp ) - 1 );
			id.idtype = IDTYPE_STEAM;

			szTemp[ sizeof( szTemp ) - 1 ] = '\0';

			CCommand args;
			args.Tokenize( szTemp );
			if ( args.ArgC() >= 5 )
			{
				// allow settings from old style steam2 format
				TSteamGlobalUserID steam2ID;
				steam2ID.m_SteamInstanceID = (SteamInstanceID_t)atoi( args[ 0 ] );
				steam2ID.m_SteamLocalUserID.Split.High32bits = (int)atoi( args[ 2 ] );
				steam2ID.m_SteamLocalUserID.Split.Low32bits = (int)atoi( args[ 4 ] );

				EUniverse eUniverse = k_EUniversePublic;
				if ( Steam3Server().GetGSSteamID().IsValid() )
					eUniverse = Steam3Server().GetGSSteamID().GetEUniverse();
#ifndef DEDICATED
				else if ( Steam3Client().SteamUser() )
					eUniverse = Steam3Client().SteamUser()->GetSteamID().GetEUniverse();
#endif

				id.steamid.SetFromSteam2( &steam2ID, eUniverse );
			}
		}
		else
		{
			id.steamid.SetFromString( str, k_EUniversePublic );
			if ( id.steamid.IsValid() )
				id.idtype = IDTYPE_STEAM;
		}
	}

	return &id;
}


//-----------------------------------------------------------------------------
// Purpose: Saves Steam ID bans to a file
//-----------------------------------------------------------------------------
CON_COMMAND( writeid, "Writes a list of permanently-banned user IDs to " BANNED_USER_FILENAME "." )
{
	char name[MAX_OSPATH];
	V_sprintf_safe( name, CONFIG_DIR BANNED_USER_FILENAME );

	ConMsg( "Writing %s.\n", name );

	FileHandle_t f = g_pFileSystem->Open ( name, "wb" );
	if ( !f )
	{
		ConMsg( "Couldn't open %s.\n", name );
		return;
	}

	RunCodeAtScopeExit(g_pFileSystem->Close(f));
	
	for ( intp i = 0 ; i < g_UserFilters.Count() ; i++ )
	{
		double banTime = g_UserFilters[i].banTime;
		if ( banTime != 0.0 )
		{
			continue;
		}

		g_pFileSystem->FPrintf( f, "banid 0 %s\r\n", GetUserIDString( g_UserFilters[i].userid ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Removes a Steam ID ban
//-----------------------------------------------------------------------------
CON_COMMAND( removeid, "Remove a user ID from the ban list." )
{
	if ( args.ArgC() != 2 && args.ArgC() != 6 )
	{
		ConMsg( "Usage: removeid <slot | uniqueid>\n" );
		return;
	}

	// get the first argument
	const char *pszArg1 = args[1];

	// don't need the # if they're using it
	if ( !Q_strncmp( pszArg1, "#", 1 ) )
	{
		ConMsg( "Usage: removeid <userid | uniqueid>\n" );
		ConMsg( "No # necessary.\n" );
		return;
	}

	// if the first letter is a character then we're searching for a uniqueid ( e.g. STEAM_ )
	if ( *pszArg1 < '0' || *pszArg1 > '9' || V_atoi64( pszArg1 ) > ( (uint32)~0 ) )
	{
		bool bValid = false;

		char szSearchString[64];
		// SteamID ( need to reassemble it )
		if ( !Q_strnicmp( pszArg1, STEAM_PREFIX, ssize( STEAM_PREFIX ) - 1 ) && Q_strstr( args[2], ":" ) )
		{
			V_sprintf_safe( szSearchString, "%s:%s:%s", pszArg1, args[3], args[5] );
			
			USERID_t *id = Filter_StringToUserID( szSearchString );
			if ( id )
			{
				bValid = id->steamid.IsValid();
				if ( bValid )
					V_sprintf_safe( szSearchString, "%s", id->steamid.Render() );
			}
		}
		else
		{
			if ( const char *pchUUID = args.ArgS(); pchUUID )
			{
				CSteamID cSteamIDCheck;
				cSteamIDCheck.SetFromString( pchUUID, k_EUniversePublic );

				bValid = cSteamIDCheck.IsValid();
				if ( bValid )
					V_sprintf_safe( szSearchString, "%s", cSteamIDCheck.Render() );
			}
		}

		// some other ID (e.g. "UNKNOWN", "STEAM_ID_PENDING", "STEAM_ID_LAN")
		// NOTE: assumed to be one argument
		if ( !bValid )
		{
			ConMsg( "removeid: Invalid ban ID \"%s\"\n", pszArg1 );
			return;
		}

		for ( intp i = 0 ; i < g_UserFilters.Count() ; i++ )
		{
			if ( Q_stricmp( GetUserIDString( g_UserFilters[i].userid ), szSearchString ) )
				continue;

			g_UserFilters.Remove( i );
			ConMsg( "removeid: Filter removed for %s\n", szSearchString );

			// send an event
			IGameEvent *event = g_GameEventManager.CreateEvent( "server_removeban" );
			if ( event )
			{
				event->SetString( "networkid", szSearchString );
				event->SetString( "ip", "" );
				event->SetString( "by", ( cmd_source == src_command ) ? "Console" : host_client->m_Name );

				g_GameEventManager.FireEvent( event );
			}

			return;
		}
		
		ConMsg( "removeid: Couldn't find %s\n", szSearchString );
		return;
	}

	// this is a userid
	int slot = Q_atoi( pszArg1 );
	if ( slot <= 0 || slot > g_UserFilters.Count() )
	{
		ConMsg( "removeid: Invalid slot %i\n", slot );
		return;
	}

	// array access is zero based
	// Copy off slot
	USERID_t id = g_UserFilters[--slot].userid;
	g_UserFilters.Remove( slot );

	// Tell server operator
	ConMsg( "removeid: Filter removed for %s, ID %s\n", pszArg1, GetUserIDString( id ) );

	// send an event
	IGameEvent *event = g_GameEventManager.CreateEvent( "server_removeban" );
	if ( event )
	{
		event->SetString( "networkid", GetUserIDString( id ) );
		event->SetString( "ip", "" );
		event->SetString( "by", ( cmd_source == src_command ) ? "Console" : host_client->m_Name );

		g_GameEventManager.FireEvent( event );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Prints Steam ID bans to the console
//-----------------------------------------------------------------------------
CON_COMMAND( listid, "Lists banned users." )
{
	const intp count = g_UserFilters.Count();
	if ( !count )
	{
		ConMsg( "ID filter list: empty\n" );
		return;
	}

	ConMsg( "ID filter list: %zi entr%s\n", count, count == 1 ? "y" : "ies" );

	for ( intp i = 0 ; i < count ; i++ )
	{
		const char *uid = GetUserIDString( g_UserFilters[i].userid );

		if ( g_UserFilters[i].banTime != 0.0 )
		{
			ConMsg( "%zi %s: %.3f min\n", i+1, uid, g_UserFilters[i].banTime );
		}
		else
		{
			ConMsg( "%zi %s: permanent\n", i+1, uid );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Bans a Steam ID
//-----------------------------------------------------------------------------
CON_COMMAND( banid, "Add a user ID to the ban list." )
{
	if ( Steam3Server().BLanOnly() )
	{
		ConMsg( "Can't ban users on a LAN.\n" );
		return;
	}

	if ( args.ArgC() < 3 || args.ArgC() > 8 )
	{
		ConMsg( "Usage: banid <minutes> <userid | uniqueid> {kick}\n" );
		ConMsg( "Use 0 minutes for permanent.\n");
		return;
	}

	char *end = nullptr;
	// dimhotepus: atof -> strtod
	double banTime = strtod( args[1], &end );
	if ( !Q_isempty(end) )
	{
		ConMsg( "Usage: banid <minutes> <userid | uniqueid> {kick}\n" );
		return;
	}

	if ( banTime < 0.01 )
	{
		banTime = 0.0;
	}

	// get the first argument
	const char *pszArg2 = args[2];
	// don't need the # if they're using it
	if ( !Q_strncmp( pszArg2, "#", 1 ) )
	{
		ConMsg( "Usage: banid <minutes> <userid | uniqueid> {kick}\n" );
		ConMsg( "No # necessary.\n");
		return;
	}

	const bool bKick = args.ArgC() >= 3 && Q_strcasecmp( args[ args.ArgC() - 1 ], "kick" ) == 0;
	
	char szSearchString[64];
	szSearchString[0] = '\0';
	int	iSearchIndex = -1;
	// if the first letter is a character then
	// we're searching for a uniqueid ( e.g. STEAM_ )
	if ( *pszArg2 < '0' || *pszArg2 > '9' || V_atoi64( pszArg2 ) > ((uint32)~0) )
	{
		bool bValid = false;

		iSearchIndex = -1;

		// SteamID (need to reassemble it)
		if ( !Q_strnicmp( pszArg2, STEAM_PREFIX, ssize( STEAM_PREFIX ) - 1 ) && Q_strstr( args[3], ":" ) )
		{
			V_sprintf_safe( szSearchString, "%s:%s:%s", pszArg2, args[4], args[6] );

			bValid = true;
		}
		else
		{
			CSteamID cSteamIDCheck;
			const char *pchArgs = args.ArgS();

			if ( const char *pchUUID = strchr( pchArgs, ' ' ); pchUUID )
			{
				cSteamIDCheck.SetFromString( pchUUID + 1, k_EUniversePublic );

				bValid = cSteamIDCheck.IsValid();
				if ( bValid )
					V_sprintf_safe( szSearchString, "%s", cSteamIDCheck.Render() );
			}
		}

		// some other ID (e.g. "UNKNOWN", "STEAM_ID_PENDING", "STEAM_ID_LAN")
		// NOTE: assumed to be one argument
		if ( !bValid )
		{
			ConMsg( "Can't ban users with ID \"%s\".\n", pszArg2 );
			return;
		}
	}
	else
	{
		// see if it is a userid
		iSearchIndex = Q_atoi( pszArg2 );
	}
	
	intp i;
	bool bPlaying = false;
	USERID_t localId;
	USERID_t *id = nullptr;
	CGameClient *client = nullptr;
	// find this client (if they're currently in the server)
	for ( i = 0; i < sv.GetClientCount(); i++ )
	{
		client = sv.Client(i);
		if ( !client || !client->IsActive() || !client->IsConnected() || !client->IsSpawned() )
		{
			continue;
		}

		if ( client->IsFakeClient() )
		{
			continue;
		}

		// searching by UserID
		if ( iSearchIndex != -1 )
		{
			if ( client->GetUserID() == iSearchIndex )
			{
				// found!
				localId = client->GetNetworkID();
				id = &localId;
				bPlaying = true;
				break;
			}
		}
		else
		{
			// searching by UniqueID
			if ( Q_stricmp( client->GetNetworkIDString(), szSearchString ) == 0 ) 
			{
				// found!
				localId = client->GetNetworkID();
				id = &localId;
				bPlaying = true;
				break;
			}
		}
	}

	// if we were searching by userid and we didn't find the person, we're done
	if ( iSearchIndex != -1 && !id )
	{
		ConMsg( "banid: Couldn't find userid %d.\n", iSearchIndex );
		return;
	}

	if ( !id )
	{
		// we're searching by SteamID and we haven't found them actively playing
		id = Filter_StringToUserID( szSearchString );
		if ( !id )
		{
			ConMsg( "banid: Couldn't resolve uniqueid \"%s\".\n", szSearchString );
			ConMsg( "Usage: banid <minutes> <userid | uniqueid> {kick}\n" );
			ConMsg( "Use 0 minutes for permanent.\n");
			return;
		}
	}

	// See if it's in the list already
	for ( i = 0 ; i < g_UserFilters.Count() ; i++ )
	{
		// We're just updating an existing id
		if ( Steam3Server().CompareUserID( g_UserFilters[i].userid, *id ) )
			break;
	}

	// Adding a new one
	if ( i == g_UserFilters.Count() )
	{
		// See if we have space for it
		if ( g_UserFilters.Count() >= MAX_USERFILTERS )
		{
			ConMsg( "banid: User filter list is full (max %zd).\n", MAX_USERFILTERS );
			return;
		}

		i = g_UserFilters.AddToTail( userfilter_t{} );
	}

	g_UserFilters[i].banTime = banTime;
	g_UserFilters[i].banEndTime = ( banTime != 0.0 ) ? ( realtime + 60.0 * banTime ) : 0.0;
	g_UserFilters[i].userid = *id;
	
	char szDuration[64];
	// Build a duration string for the ban
	if ( banTime == 0.0 )
	{
		V_sprintf_safe( szDuration, "permanently" );
	}
	else
	{
		V_sprintf_safe( szDuration, "for %.2f minutes", banTime );
	}

	// fire the event
	IGameEvent *event = g_GameEventManager.CreateEvent( "server_addban" );
	if ( event )
	{
		if ( bPlaying )
		{
			event->SetString( "name", client->m_Name );
			event->SetInt( "userid", client->GetUserID() );
			event->SetString( "networkid", client->GetNetworkIDString() );
		}
		else
		{
			event->SetString( "name", "" );
			event->SetInt( "userid", 0 );
			event->SetString( "networkid", GetUserIDString( *id ) );
		}

		event->SetString( "ip", "" );
		event->SetString( "duration", szDuration );
		event->SetString( "by", ( cmd_source == src_command ) ? "Console" : host_client->m_Name );
		event->SetInt( "kicked", ( bKick && bPlaying && client ) ? 1 : 0 );

		g_GameEventManager.FireEvent( event );
	}

	if ( bKick && bPlaying )
	{
		client->ClientPrintf ( "You have been kicked and banned %s by the server.\n", szDuration );
		client->Disconnect( "Kicked and banned" );
	}
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Filter_Init( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Filter_Shutdown( void )
{
}
