//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#include "quakedef.h"
#include "bspfile.h"
#include "host.h"
#include "sys.h"
#include "filesystem_engine.h"
#include "utldict.h"
#include "demo.h"
#ifndef SWDS
#include "vgui_baseui_interface.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Imported from other .cpp files
void Host_Map_f( const CCommand &args );
void Host_Map_Background_f( const CCommand &args );
void Host_Map_Commentary_f( const CCommand &args );
void Host_Changelevel_f( const CCommand &args );
void Host_Changelevel2_f( const CCommand &args );

//-----------------------------------------------------------------------------
// Purpose: For each map, stores when the map last changed on disk and whether
//  it is a valid map
//-----------------------------------------------------------------------------
class CMapListItem
{
public:
	enum
	{
		INVALID = 0,
		PENDING,
		VALID,
	};

					CMapListItem( void );
	
	void			SetValid( int valid );
	int				GetValid( void ) const;

	void			SetFileTimestamp( time_t ts );
	time_t			GetFileTimestamp( void ) const;

	bool			IsSameTime( time_t ts ) const;

	static time_t	GetFSTimeStamp( char const *name );
	static int		CheckFSHeaderVersion( char const *name );

private:
	int				m_nValid;
	// dimhotepus: long -> time_t.
	time_t			m_lFileTimestamp;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapListItem::CMapListItem( void )
{
	m_nValid = PENDING;
	m_lFileTimestamp = 0L;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : valid - 
//-----------------------------------------------------------------------------
void CMapListItem::SetValid( int valid )
{
	m_nValid = valid;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
int CMapListItem::GetValid( void ) const
{
	return m_nValid;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ts - 
//-----------------------------------------------------------------------------
void CMapListItem::SetFileTimestamp( time_t ts )
{
	m_lFileTimestamp = ts;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : long
//-----------------------------------------------------------------------------
time_t CMapListItem::GetFileTimestamp( void ) const
{
	return m_lFileTimestamp;
}

//-----------------------------------------------------------------------------
// Purpose: Check whether this map file has changed related to the passed in timestamp
// Input  : ts - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMapListItem::IsSameTime( time_t ts ) const
{
	return ( m_lFileTimestamp == ts ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: Get the timestamp for the file from the file system
// Input  : *name - 
// Output : long
//-----------------------------------------------------------------------------
time_t CMapListItem::GetFSTimeStamp( char const *name )
{
	time_t ts = g_pFileSystem->GetFileTime( name );
	return ts;
}

//-----------------------------------------------------------------------------
// Purpose: Check whether the specified map header version is up-to-date
// Input  : *name - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
int CMapListItem::CheckFSHeaderVersion( char const *name )
{
	dheader_t header;
	memset( &header, 0, sizeof( header ) );

	FileHandle_t fp = g_pFileSystem->Open ( name, "rb" );
	if ( fp )
	{
		g_pFileSystem->Read( &header, sizeof( header ), fp );
		g_pFileSystem->Close( fp );
	}

	return ( header.version >= MINBSPVERSION && header.version <= BSPVERSION ) ? VALID : INVALID;
}

// How often to check the filesystem for updated map info
#define MIN_REFRESH_INTERVAL 60.0

//-----------------------------------------------------------------------------
// Purpose: Stores the current list of maps for the engine
//-----------------------------------------------------------------------------
class CMapListManager
{
public:
	CMapListManager( void );
	~CMapListManager( void );

	// See if it's time to revisit the items in the list
	void			RefreshList( void );

	// Get item count, etc
	int				GetMapCount( void ) const;
	int				IsMapValid( int index ) const;
	char const 		*GetMapName( int index ) const;

	void			Think( void );

private:
	// Clear list
	void			ClearList( void );
	// Rebuild list from scratch
	void			BuildList( void );

private:
	// Time of last update
	double			m_flLastRefreshTime;

	// Dictionary of items
	CUtlDict< CMapListItem, int > m_Items;

	bool			m_bDirty;
};

// Singleton manager object
static CMapListManager g_MapListMgr;

void Host_UpdateMapList( void )
{
	g_MapListMgr.Think();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapListManager::CMapListManager( void )
{
	m_flLastRefreshTime = -1.0;
	m_bDirty = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapListManager::~CMapListManager( void )
{
	ClearList();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapListManager::Think( void )
{
	return;

	if ( !m_bDirty )
		return;

#ifndef SWDS
	// Only update pending files if console is visible to avoid slamming FS while in a map
	if ( !EngineVGui()->IsConsoleVisible() )
		return;
#endif

	m_bDirty = false;
		
	for ( auto i = m_Items.Count() - 1; i >= 0 ; i-- )
	{
		CMapListItem *item = &m_Items[ i ];
		if ( item->GetValid() != CMapListItem::PENDING )
		{
			continue;
		}

		char const *filename = m_Items.GetElementName( i );

		item->SetValid( CMapListItem::CheckFSHeaderVersion( filename ) );

		// Keep fixing things up next frame
		m_bDirty = true;
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: FIXME:  Refresh doesn't notice maps that have been deleted... oh well
//-----------------------------------------------------------------------------
void CMapListManager::RefreshList( void )
{
	if ( m_flLastRefreshTime == -1.0 )
	{
		BuildList();
		return;
	}

	if ( realtime < m_flLastRefreshTime + MIN_REFRESH_INTERVAL )
		return;

	ConDMsg( "Refreshing map list...\n" );

	// Search the directory structure.
	char mapwild[MAX_QPATH];
	Q_strncpy(mapwild,"maps/*.bsp", sizeof( mapwild ) );
	char const *findfn = Sys_FindFirst( mapwild, NULL, 0 );
	while ( findfn )
	{
		if ( IsPC() && V_stristr( findfn, ".360.bsp" ) )
		{
			// ignore 360 bsp
			findfn = Sys_FindNext( NULL, 0 );
			continue;
		}
		else if ( IsX360() && !V_stristr( findfn, ".360.bsp" ) )
		{
			// ignore pc bsp
			findfn = Sys_FindNext( NULL, 0 );
			continue;
		}

		// Make full fileame (maps/foo.bsp) and map name (foo)
		char szFileName[ MAX_QPATH ] = { 0 };
		V_snprintf( szFileName, sizeof( szFileName ), "maps/%s", findfn );

		char szMapName[256] = { 0 };
		V_strncpy( szMapName, findfn, sizeof( szMapName ) );
		char *pExt = V_stristr( szMapName, ".bsp" );
		if ( pExt )
		{
			*pExt = '\0';
		}

		auto idx = m_Items.Find( szMapName );
		if ( idx == m_Items.InvalidIndex() )
		{
			CMapListItem item;
			item.SetFileTimestamp( item.GetFSTimeStamp( szFileName ) );
			item.SetValid( CMapListItem::PENDING );
			// Insert into dictionary
			m_Items.Insert( szMapName, item );

			m_bDirty = true;
		}
		else
		{
			CMapListItem *item = &m_Items[ idx ];
			Assert( item );

			// Make sure data is up to date
			time_t timestamp = g_pFileSystem->GetFileTime( szFileName );
			if ( !item->IsSameTime( timestamp ) )
			{
				item->SetFileTimestamp( timestamp );
				item->SetValid( CMapListItem::PENDING );

				m_bDirty = true;
			}
		}

		findfn = Sys_FindNext( NULL, 0 );
	}

	Sys_FindClose();

	m_flLastRefreshTime = realtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CMapListManager::GetMapCount( void ) const
{
	return m_Items.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
int CMapListManager::IsMapValid( int index ) const
{
	if ( !m_Items.IsValidIndex( index ) )
		return false;

	CMapListItem const *item = &m_Items[ index ];
	Assert( item );
	return item->GetValid();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : char const
//-----------------------------------------------------------------------------
char const *CMapListManager::GetMapName( int index ) const
{
	if ( !m_Items.IsValidIndex( index ) )
		return "Invalid!!!";

	return m_Items.GetElementName( index );
}

//-----------------------------------------------------------------------------
// Purpose: Wipe the list
//-----------------------------------------------------------------------------
void CMapListManager::ClearList( void )
{
	m_Items.Purge();
	m_bDirty = false;
}

//-----------------------------------------------------------------------------
// Purpose: Rebuild the entire list
//-----------------------------------------------------------------------------
void CMapListManager::BuildList( void )
{
	ClearList();

	// Search the directory structure.
	char mapwild[MAX_QPATH];
	Q_strncpy(mapwild,"maps/*.bsp", sizeof( mapwild ) );
	char const *findfn = Sys_FindFirst( mapwild, NULL, 0 );
	while ( findfn )
	{
		if ( V_stristr( findfn, ".360.bsp" ) )
		{
			// ignore 360 bsp
			findfn = Sys_FindNext( NULL, 0 );
			continue;
		}

		// Make full fileame (maps/foo.bsp) and map name (foo)
		char szFileName[ MAX_QPATH ] = { 0 };
		V_snprintf( szFileName, sizeof( szFileName ), "maps/%s", findfn );

		char szMapName[256] = { 0 };
		V_strncpy( szMapName, findfn, sizeof( szMapName ) );
		char *pExt = V_stristr( szMapName, ".bsp" );
		if ( pExt )
		{
			*pExt = '\0';
		}

		CMapListItem item;
		item.SetFileTimestamp( item.GetFSTimeStamp( szFileName ) );
		item.SetValid( CMapListItem::PENDING );

		// Insert into dictionary
		auto idx = m_Items.Find( szMapName );
		if ( idx == m_Items.InvalidIndex() )
		{
			m_Items.Insert( szMapName, item );
		}

		findfn = Sys_FindNext( NULL, 0 );
	}

	Sys_FindClose();

	// Remember time we build the list
	m_flLastRefreshTime = realtime;

	m_bDirty = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pakorfilesys - 
//			*mapname - 
// Output : static void
//-----------------------------------------------------------------------------
static bool MapList_CheckPrintMap( const char *pakorfilesys, const char *mapname, int valid,
							   bool showoutdated, bool verbose )
{
	bool validorpending = ( valid != CMapListItem::INVALID ) ? true : false;

	if ( !verbose )
	{
		return validorpending;
	}

	char prefix[ 32 ];
	prefix[ 0 ] = 0;

	switch ( valid )
	{
	default:
	case CMapListItem::VALID:
		break;
	case CMapListItem::PENDING:
		Q_strncpy( prefix, "PENDING:  ", sizeof( prefix ) );
		break;
	case CMapListItem::INVALID:
		Q_strncpy( prefix, "OUTDATED:  ", sizeof( prefix ) );
		break;
	}

	if ( validorpending ^ showoutdated )
	{
		ConMsg( "%s %s %s\n", prefix, pakorfilesys, mapname );
	}

	return validorpending;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszSubString - 
//			listobsolete - 
//			maxitemlength - 
// Output : static int
//-----------------------------------------------------------------------------
static intp MapList_CountMaps( const char *pszSubString, bool listobsolete, intp& maxitemlength )
{
	g_MapListMgr.RefreshList();

	maxitemlength = 0;

	intp substringlength = 0;
	if ( pszSubString && pszSubString[0] )
	{
		substringlength = V_strlen(pszSubString);
	}

	//
	// search through the path, one element at a time
	//
	int count = 0;
	int showOutdated;
	for( showOutdated = listobsolete ? 1 : 0; showOutdated >= 0; showOutdated-- )
	{
		for ( int i = 0; i < g_MapListMgr.GetMapCount(); i++ )
		{
			char const *mapname = g_MapListMgr.GetMapName( i );
			int valid = g_MapListMgr.IsMapValid( i );

			if ( !substringlength || V_stristr( mapname, pszSubString ) )
			{
				if ( MapList_CheckPrintMap( "(fs)", mapname, valid, showOutdated ? true : false, false ) )
				{
					maxitemlength = max( maxitemlength, (intp)( V_strlen( mapname ) + 1 ) );
					count++;
				}
			}
		}
	}

	return count;
}

//-----------------------------------------------------------------------------
// Purpose: 
//  Lists all maps matching the substring
//  If the substring is empty, or "*", then lists all maps
// Input  : *pszSubString - 
//-----------------------------------------------------------------------------
intp MapList_ListMaps( const char *pszSubString, bool listobsolete, bool verbose, intp maxcount, intp maxitemlength, char maplist[][ 64 ] )
{
	g_MapListMgr.RefreshList();
	intp substringlength = 0;
	if (pszSubString && pszSubString[0])
	{
		substringlength = V_strlen(pszSubString);
	}

	//
	// search through the path, one element at a time
	//

	if ( verbose )
	{
		ConMsg( "-------------\n");
	}

	intp count = 0;
	int showOutdated;
	for( showOutdated = listobsolete ? 1 : 0; showOutdated >= 0; showOutdated-- )
	{
		if ( count >= maxcount )
			break;

		//search the directory structure.
		for ( int i = 0; i < g_MapListMgr.GetMapCount(); i++ )
		{
			if ( count >= maxcount )
				break;

			char const *mapname = g_MapListMgr.GetMapName( i );
			int valid = g_MapListMgr.IsMapValid( i );

			if ( !substringlength || V_stristr( mapname, pszSubString ) )
			{
				if ( MapList_CheckPrintMap( "(fs)", mapname, valid, showOutdated ? true : false, verbose ) )
				{
					if ( maxitemlength != 0 )
					{
						Q_strncpy( maplist[ count ], mapname, maxitemlength );
					}
					count++;
				}
			}
		}
	}

	return count;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *partial - 
//			context - 
//			longest - 
//			maxcommands - 
//			**commands - 
// Output : int
//-----------------------------------------------------------------------------
int _Host_Map_f_CompletionFunc( char const *cmdname, char const *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	char *substring = (char *)partial;
	if ( Q_strstr( partial, cmdname ) )
	{
		substring = (char *)partial + strlen( cmdname );
	}

	intp longest = 0;
	intp count = min( MapList_CountMaps( substring, false, longest ), static_cast<intp>(COMMAND_COMPLETION_MAXITEMS) );
	if ( count > 0 )
	{
		MapList_ListMaps( substring, false, false, COMMAND_COMPLETION_MAXITEMS, longest, commands );

		// Now prepend maps * in front of all of the options
		intp i;
		for ( i = 0; i < count ; i++ )
		{
			char old[ COMMAND_COMPLETION_ITEM_LENGTH ];
			Q_strncpy( old, commands[ i ], sizeof( old ) );
			Q_snprintf( commands[ i ], sizeof( commands[ i ] ), "%s%s", cmdname, old );
		}
	}

	return count;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *partial - 
//			context - 
//			longest - 
//			maxcommands - 
//			**commands - 
// Output : int
//-----------------------------------------------------------------------------
static int Host_Map_f_CompletionFunc( char const *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	char const *cmdname = "map ";
	return _Host_Map_f_CompletionFunc( cmdname, partial, commands );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *partial - 
//			context - 
//			longest - 
//			maxcommands - 
//			**commands - 
// Output : int
//-----------------------------------------------------------------------------
static int Host_Map_Commentary_f_CompletionFunc( char const *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	char const *cmdname = "map_commentary ";
	return _Host_Map_f_CompletionFunc( cmdname, partial, commands );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *partial - 
//			context - 
//			longest - 
//			maxcommands - 
//			**commands - 
// Output : int
//-----------------------------------------------------------------------------
static int Host_Changelevel_f_CompletionFunc( char const *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	char const *cmdname = "changelevel ";
	return _Host_Map_f_CompletionFunc( cmdname, partial, commands );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *partial - 
//			context - 
//			longest - 
//			maxcommands - 
//			**commands - 
// Output : int
//-----------------------------------------------------------------------------
static int Host_Changelevel2_f_CompletionFunc( char const *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	char const *cmdname = "changelevel2 ";
	return _Host_Map_f_CompletionFunc( cmdname, partial, commands );
}


//-----------------------------------------------------------------------------
// Purpose: do a dir of the maps dir
//-----------------------------------------------------------------------------
static void Host_Maps_f( const CCommand &args )
{
	const char *pszSubString = NULL;

	if ( args.ArgC() != 2 && args.ArgC() != 3 )
	{
		ConMsg( "Usage:  maps <substring>\nmaps * for full listing\n" );
		return;
	}

	if ( args.ArgC() == 2 )
	{
		pszSubString = args[1];
		if (!pszSubString || !pszSubString[0])
			return;
	}

	if ( pszSubString && ( pszSubString[0] == '*' ))
		pszSubString = NULL;

	intp longest = 0;
	intp count = MapList_CountMaps( pszSubString, true, longest );
	if ( count > 0 )
	{
		MapList_ListMaps( pszSubString, true, true, count, 0, NULL );
	}
}

#ifndef BENCHMARK
static ConCommand maps("maps", Host_Maps_f, "Displays list of maps." );
static ConCommand map("map", Host_Map_f, "Start playing on specified map.", FCVAR_DONTRECORD, Host_Map_f_CompletionFunc );
static ConCommand map_background("map_background", Host_Map_Background_f, "Runs a map as the background to the main menu.", FCVAR_DONTRECORD, Host_Map_f_CompletionFunc );
static ConCommand map_commentary("map_commentary", Host_Map_Commentary_f, "Start playing, with commentary, on a specified map.", FCVAR_DONTRECORD, Host_Map_Commentary_f_CompletionFunc );
static ConCommand changelevel("changelevel", Host_Changelevel_f, "Change server to the specified map", FCVAR_DONTRECORD, Host_Changelevel_f_CompletionFunc );
static ConCommand changelevel2("changelevel2", Host_Changelevel2_f, "Transition to the specified map in single player", FCVAR_DONTRECORD, Host_Changelevel2_f_CompletionFunc );
#endif
