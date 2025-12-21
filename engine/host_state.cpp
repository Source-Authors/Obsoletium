//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Runs the state machine for the host & server
//
// $NoKeywords: $
//=============================================================================//


#include "host_state.h"
#include "eiface.h"
#include "quakedef.h"
#include "server.h"
#include "sv_main.h"
#include "host_cmd.h"
#include "host.h"
#include "screen.h"
#include "icliententity.h"
#include "icliententitylist.h"
#include "client.h"
#include "host_jmp.h"
#include "tier0/vprof.h"
#include "tier0/icommandline.h"
#include "filesystem_engine.h"
#include "zone.h"
#include "iengine.h"
#include "snd_audio_source.h"
#include "sv_steamauth.h"
#ifndef SWDS
#include "vgui_baseui_interface.h"
#endif
#include "sv_plugin.h"
#include "cl_main.h"
#include "datacache/imdlcache.h"
#include "sys_dll.h"
#include "testscriptmgr.h"
#if defined( REPLAY_ENABLED )
#include "replay_internal.h"
#include "replayserver.h"
#endif
#include "GameEventManager.h"
#include "tier0/etwprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern bool		g_bAbortServerSet;
#ifndef SWDS
extern ConVar	reload_materials;
#endif

enum class HOSTSTATES
{
	HS_NEW_GAME = 0,
	HS_LOAD_GAME,
	HS_CHANGE_LEVEL_SP,
	HS_CHANGE_LEVEL_MP,
	HS_RUN,
	HS_GAME_SHUTDOWN,
	HS_SHUTDOWN,
	HS_RESTART,
};

// a little class that manages the state machine for the host
class CHostState
{
public:
				CHostState();
	void		Init();
	void		FrameUpdate( float time );
	void		SetNextState( HOSTSTATES nextState );

	void		RunGameInit();

	void		SetState( HOSTSTATES newState, bool clearNext );
	void		GameShutdown();

	void		State_NewGame();
	void		State_LoadGame();
	void		State_ChangeLevelMP();
	void		State_ChangeLevelSP();
	void		State_Run( float time );
	void		State_GameShutdown();
	void		State_Shutdown();
	void		State_Restart();

	bool		IsGameShuttingDown();

	void		RememberLocation();
	void		OnClientConnected(); // Client fully connected
	void		OnClientDisconnected(); // Client disconnected

	HOSTSTATES	m_currentState;
	HOSTSTATES	m_nextState;
	Vector		m_vecLocation;
	QAngle		m_angLocation;
	char		m_levelName[256];
	char		m_landmarkName[256];
	char		m_saveName[256];
	float		m_flShortFrameTime;		// run a few one-tick frames to avoid large timesteps while loading assets

	bool		m_activeGame;
	bool		m_bRememberLocation;
	bool		m_bBackgroundLevel;
	bool		m_bWaitingForConnection;
};

static bool Host_ValidGame( void );
static CHostState	g_HostState;


//-----------------------------------------------------------------------------
// external API for manipulating the host state machine
//-----------------------------------------------------------------------------
void HostState_Init()
{
	g_HostState.Init();
}

void HostState_Frame( float time )
{
	g_HostState.FrameUpdate( time );
}

void HostState_RunGameInit()
{
	g_HostState.RunGameInit();
	g_ServerGlobalVariables.bMapLoadFailed = false;
}

//-----------------------------------------------------------------------------
// start a new game as soon as possible
//-----------------------------------------------------------------------------
void HostState_NewGame( char const *pMapName, bool remember_location, bool background )
{
	V_strcpy_safe( g_HostState.m_levelName, pMapName );

	g_HostState.m_landmarkName[0] = 0;
	g_HostState.m_bRememberLocation = remember_location;
	g_HostState.m_bWaitingForConnection = true;
	g_HostState.m_bBackgroundLevel = background;
	if ( remember_location )
	{
		g_HostState.RememberLocation();
	}
	g_HostState.SetNextState( HOSTSTATES::HS_NEW_GAME );
}

//-----------------------------------------------------------------------------
// load a new game as soon as possible
//-----------------------------------------------------------------------------
void HostState_LoadGame( char const *pSaveFileName, bool remember_location )
{
#ifndef SWDS
	// Make sure the freaking save file exists....
	if ( !saverestore->SaveFileExists( pSaveFileName ) )
	{
		Warning("Save file %s can't be found!\n", pSaveFileName );
		SCR_EndLoadingPlaque();
		return;
	}

	V_strcpy_safe( g_HostState.m_saveName, pSaveFileName );

	// Tell the game .dll we are loading another game
	serverGameDLL->PreSaveGameLoaded( pSaveFileName, sv.IsActive() );

	g_HostState.m_bRememberLocation = remember_location;
	g_HostState.m_bBackgroundLevel = false;
	g_HostState.m_bWaitingForConnection = true;
	if ( remember_location )
	{
		g_HostState.RememberLocation();
	}

	g_HostState.SetNextState( HOSTSTATES::HS_LOAD_GAME );
#endif
}

// change level (single player style - smooth transition)
void HostState_ChangeLevelSP( char const *pNewLevel, char const *pLandmarkName )
{
	V_strcpy_safe( g_HostState.m_levelName, pNewLevel );
	V_strcpy_safe( g_HostState.m_landmarkName, pLandmarkName );
	g_HostState.SetNextState( HOSTSTATES::HS_CHANGE_LEVEL_SP );
}

// change level (multiplayer style - respawn all connected clients)
void HostState_ChangeLevelMP( char const *pNewLevel, char const *pLandmarkName )
{
	Steam3Server().NotifyOfLevelChange();

	V_strcpy_safe( g_HostState.m_levelName, pNewLevel );
	V_strcpy_safe( g_HostState.m_landmarkName, pLandmarkName );
	g_HostState.SetNextState( HOSTSTATES::HS_CHANGE_LEVEL_MP );
}

// shutdown the game as soon as possible
void HostState_GameShutdown()
{
	Steam3Server().NotifyOfLevelChange();

	// This will get called during shutdown, ignore it.
	if ( g_HostState.m_currentState != HOSTSTATES::HS_SHUTDOWN &&
		 g_HostState.m_currentState != HOSTSTATES::HS_RESTART &&
		 g_HostState.m_currentState != HOSTSTATES::HS_GAME_SHUTDOWN
		 )
	{
		g_HostState.SetNextState( HOSTSTATES::HS_GAME_SHUTDOWN );
	}
}

// shutdown the engine/program as soon as possible
void HostState_Shutdown()
{
#if defined( REPLAY_ENABLED )
	// If we're recording the game, finalize the replay so players can download it.
	if ( sv.IsDedicated() && g_pReplay && g_pReplay->IsRecording() )
	{
		// Stop recording and publish all blocks/session info data synchronously.
		g_pReplay->SV_EndRecordingSession( true );
	}
#endif

	g_HostState.SetNextState( HOSTSTATES::HS_SHUTDOWN );
}

//-----------------------------------------------------------------------------
// Purpose: Restart the engine
//-----------------------------------------------------------------------------
void HostState_Restart()
{
	g_HostState.SetNextState( HOSTSTATES::HS_RESTART );
}

bool HostState_IsGameShuttingDown()
{
	return g_HostState.IsGameShuttingDown();
}

bool HostState_IsShuttingDown()
{
	return ( g_HostState.m_currentState == HOSTSTATES::HS_SHUTDOWN ||
		g_HostState.m_currentState == HOSTSTATES::HS_RESTART ||
		g_HostState.m_currentState == HOSTSTATES::HS_GAME_SHUTDOWN );
}


void HostState_OnClientConnected()
{
	g_HostState.OnClientConnected();
}

void HostState_OnClientDisconnected()
{
	g_HostState.OnClientDisconnected();
}

void HostState_SetSpawnPoint(Vector &position, QAngle &angle)
{
	g_HostState.m_angLocation = angle;
	g_HostState.m_vecLocation = position;
	g_HostState.m_bRememberLocation = true;
}

//-----------------------------------------------------------------------------
static void WatchDogHandler()
{
	Host_Error( "WatchdogHandler called - server exiting.\n" );
}

//-----------------------------------------------------------------------------
// Class implementation
//-----------------------------------------------------------------------------

CHostState::CHostState() = default;

void CHostState::Init()
{
	SetState( HOSTSTATES::HS_RUN, true );
	m_currentState = HOSTSTATES::HS_RUN;
	m_nextState = HOSTSTATES::HS_RUN;
	m_activeGame = false;
	m_levelName[0] = 0;
	m_saveName[0] = 0;
	m_landmarkName[0] = 0;
	m_bRememberLocation = 0;
	m_bBackgroundLevel = false;
	m_vecLocation.Init();
	m_angLocation.Init();
	m_bWaitingForConnection = false;
	m_flShortFrameTime = 1.0;

	Plat_SetWatchdogHandlerFunction( WatchDogHandler );
}

void CHostState::SetState( HOSTSTATES newState, bool clearNext )
{
	m_currentState = newState;
	if ( clearNext )
	{
		m_nextState = newState;
	}
}

void CHostState::SetNextState( HOSTSTATES next )
{
	Assert( m_currentState == HOSTSTATES::HS_RUN );
	m_nextState = next;
}

void CHostState::RunGameInit()
{
	Assert( !m_activeGame );

	if ( serverGameDLL )
	{
		serverGameDLL->GameInit();
	}
	m_activeGame = true;
}

void CHostState::GameShutdown()
{
	if ( m_activeGame )
	{
		serverGameDLL->GameShutdown();
		m_activeGame = false;
	}
}


// These State_ functions execute that state's code right away
// The external API queues up state changes to happen when the state machine is processed.
void CHostState::State_NewGame()
{
	CETWScope timer( "CHostState::State_NewGame" );

	if ( Host_ValidGame() )
	{
		// Demand load game .dll if running with -nogamedll flag, etc.
		if ( !serverGameClients )
		{
			SV_InitGameDLL();
		}

		if ( !serverGameClients )
		{
			Warning( "Can't start game, no valid server" DLL_EXT_STRING " loaded\n" );
		}
		else
		{
			if ( Host_NewGame( m_levelName, false, m_bBackgroundLevel ) )
			{
				// succesfully started the new game
				SetState( HOSTSTATES::HS_RUN, true );
				return;
			}
		}
	}

	SCR_EndLoadingPlaque();

	// new game failed
	GameShutdown();
	// run the server at the console
	SetState( HOSTSTATES::HS_RUN, true );
}

void CHostState::State_LoadGame()
{
#ifndef SWDS
	HostState_RunGameInit();
	
	if ( saverestore->LoadGame( m_saveName ) )
	{
		// succesfully started the new game
		GetTestScriptMgr()->CheckPoint( "load_game" );
		SetState( HOSTSTATES::HS_RUN, true );
		return;
	}
#endif

	SCR_EndLoadingPlaque();

	// load game failed
	GameShutdown();
	// run the server at the console
	SetState( HOSTSTATES::HS_RUN, true );
}


void CHostState::State_ChangeLevelMP()
{
	if ( Host_ValidGame() )
	{
		Steam3Server().NotifyOfLevelChange();

#ifndef SWDS
		// start progress bar immediately for multiplayer level transitions
		EngineVGui()->EnabledProgressBarForNextLoad();
#endif
		if ( Host_Changelevel( false, m_levelName, m_landmarkName ) )
		{
			SetState( HOSTSTATES::HS_RUN, true );
			return;
		}
	}
	// fail
	ConMsg( "Unable to change level!\n" );
	SetState( HOSTSTATES::HS_RUN, true );

	IGameEvent *event = g_GameEventManager.CreateEvent( "server_changelevel_failed" );
	if ( event )
	{
		event->SetString( "levelname", m_levelName );
		g_GameEventManager.FireEvent( event );
	}
}


void CHostState::State_ChangeLevelSP()
{
	if ( Host_ValidGame() )
	{
		Host_Changelevel( true, m_levelName, m_landmarkName );
		SetState( HOSTSTATES::HS_RUN, true );
		return;
	}
	// fail
	ConMsg( "Unable to change level!\n" );
	SetState( HOSTSTATES::HS_RUN, true );
}

static bool IsClientActive()
{
	if ( !sv.IsActive() )
		return cl.IsActive();

	for ( int i = 0; i < sv.GetClientCount(); i++ )
	{
		CGameClient *pClient = sv.Client( i );
		if ( pClient->IsActive() )
			return true;
	}
	return false;
}

static bool IsClientConnected()
{
	if ( !sv.IsActive() )
		return cl.IsConnected();

	for ( int i = 0; i < sv.GetClientCount(); i++ )
	{
		CGameClient *pClient = sv.Client( i );
		if ( pClient->IsConnected() )
			return true;
	}
	return false;
}

void CHostState::State_Run( float frameTime )
{
	static bool s_bFirstRunFrame = true;

	if ( m_flShortFrameTime > 0 )
	{
		if ( IsClientActive() )
		{
			m_flShortFrameTime = (m_flShortFrameTime > frameTime) ? (m_flShortFrameTime-frameTime) : 0;
		}
		// Only clamp time if client is in process of connecting or is already connected.
		if ( IsClientConnected() )
		{
			frameTime = min( frameTime, host_state.interval_per_tick );
		}
	}

	// Nice big timeout to play it safe while still ensuring that we don't get stuck in
	// infinite loops.
	int nTimerWaitSeconds = 60;
	if ( s_bFirstRunFrame ) 
	{
		// The first frame can take a while especially during fork startup.
		s_bFirstRunFrame = false;
		nTimerWaitSeconds *= 2;
	}
	if ( sv.IsDedicated() )
	{
		Plat_BeginWatchdogTimer( nTimerWaitSeconds );
	}

	Host_RunFrame( frameTime );

	if ( sv.IsDedicated() )
	{
		Plat_EndWatchdogTimer();
	}

	switch( m_nextState )
	{
	case HOSTSTATES::HS_RUN:
		break;

	case HOSTSTATES::HS_LOAD_GAME:
	case HOSTSTATES::HS_NEW_GAME:
#if !defined( SWDS )
		SCR_BeginLoadingPlaque();
#endif
		// FALL THROUGH INTENTIONALLY TO SHUTDOWN
		[[fallthrough]];

	case HOSTSTATES::HS_SHUTDOWN:
	case HOSTSTATES::HS_RESTART:
		// NOTE: The game must be shutdown before a new game can start, 
		// before a game can load, and before the system can be shutdown.
		// This is done here instead of pathfinding through a state transition graph.
		// That would be equivalent as the only way to get from HS_RUN to HS_LOAD_GAME is through HS_GAME_SHUTDOWN.
	case HOSTSTATES::HS_GAME_SHUTDOWN:
		SetState( HOSTSTATES::HS_GAME_SHUTDOWN, false );
		break;

	case HOSTSTATES::HS_CHANGE_LEVEL_MP:
	case HOSTSTATES::HS_CHANGE_LEVEL_SP:
		SetState( m_nextState, true );
		break;

	default:
		SetState( HOSTSTATES::HS_RUN, true );
		break;
	}
}

void CHostState::State_GameShutdown()
{
	if ( serverGameDLL )
	{
		Steam3Server().NotifyOfLevelChange();
		g_pServerPluginHandler->LevelShutdown();
#if !defined(SWDS)
		audiosourcecache->LevelShutdown();
#endif
	}

	GameShutdown();
#ifndef SWDS
	saverestore->ClearSaveDir();
#endif
	Host_ShutdownServer();

	switch( m_nextState )
	{
	case HOSTSTATES::HS_LOAD_GAME:
	case HOSTSTATES::HS_NEW_GAME:
	case HOSTSTATES::HS_SHUTDOWN:
	case HOSTSTATES::HS_RESTART:
		SetState( m_nextState, true );
		break;
	default:
		SetState( HOSTSTATES::HS_RUN, true );
		break;
	}
}


// Tell the launcher we're done.
void CHostState::State_Shutdown()
{
#if !defined(SWDS)
	CL_EndMovie();
#endif

	eng->SetNextState( IEngine::DLL_CLOSE );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHostState::State_Restart()
{
	// Just like a regular shutdown
	State_Shutdown();

	// But signal launcher/front end to restart engine
	eng->SetNextState( IEngine::DLL_RESTART );
}


//-----------------------------------------------------------------------------
// this is the state machine's main processing loop
//-----------------------------------------------------------------------------
void CHostState::FrameUpdate( float time )
{
#if _DEBUG
	int loopCount = 0;
#endif

	if ( setjmp (host_abortserver) )
	{
		Init();
		return;
	}

	g_bAbortServerSet = true;

	while ( true )
	{
		HOSTSTATES oldState = m_currentState;

		// execute the current state (and transition to the next state if not in HS_RUN)
		switch( m_currentState )
		{
		case HOSTSTATES::HS_NEW_GAME:
			g_pMDLCache->BeginMapLoad();
			State_NewGame();
			break;
		case HOSTSTATES::HS_LOAD_GAME:
			g_pMDLCache->BeginMapLoad();
			State_LoadGame();
			break;
		case HOSTSTATES::HS_CHANGE_LEVEL_MP:
			g_pMDLCache->BeginMapLoad();
			m_flShortFrameTime = 0.5f;
			State_ChangeLevelMP();
			break;
		case HOSTSTATES::HS_CHANGE_LEVEL_SP:
			g_pMDLCache->BeginMapLoad();
			m_flShortFrameTime = 1.5f; // 1.5s of slower frames
			State_ChangeLevelSP();
			break;
		case HOSTSTATES::HS_RUN:
			State_Run( time );
			break;
		case HOSTSTATES::HS_GAME_SHUTDOWN:
			State_GameShutdown();
			break;
		case HOSTSTATES::HS_SHUTDOWN:
			State_Shutdown();
			break;
		case HOSTSTATES::HS_RESTART:
			g_pMDLCache->BeginMapLoad();
			State_Restart();
			break;
		}

		// only do a single pass at HS_RUN per frame.  All other states loop until they reach HS_RUN 
		if ( oldState == HOSTSTATES::HS_RUN )
			break;

		// shutting down
		if ( oldState == HOSTSTATES::HS_SHUTDOWN ||
			 oldState == HOSTSTATES::HS_RESTART )
			break;

		// Only HS_RUN is allowed to persist across loops!!!
		// Also, detect circular state graphs (more than 8 cycling changes is probably a loop)
		// NOTE: In the current graph, there are at most 2.
#if _DEBUG
		if ( (m_currentState == oldState) || (++loopCount > 8) )
		{
			Host_Error( "state crash!\n" );
		}
#endif
	}
}


bool CHostState::IsGameShuttingDown()
{
	return ( ( m_currentState == HOSTSTATES::HS_GAME_SHUTDOWN ) || ( m_nextState == HOSTSTATES::HS_GAME_SHUTDOWN ) );
}

void CHostState::RememberLocation()
{
#ifndef SWDS
	Assert( m_bRememberLocation );

	m_vecLocation = MainViewOrigin();
	VectorAngles( MainViewForward(), m_angLocation );

	IClientEntity *localPlayer = entitylist ? entitylist->GetClientEntity( cl.m_nPlayerSlot + 1 ) : NULL;
	if ( localPlayer )
	{
		m_vecLocation = localPlayer->GetAbsOrigin();
	}

	m_vecLocation.z -= 64.0f; // subtract out a bit of Z to position the player lower
#endif
}

void CHostState::OnClientConnected()
{
	if ( !m_bWaitingForConnection )
		return;
	m_bWaitingForConnection = false;

	if ( m_bRememberLocation )
	{
		m_bRememberLocation = false;
		Cbuf_AddText( va( "setpos_exact %f %f %f\n", m_vecLocation.x, m_vecLocation.y, m_vecLocation.z ) ); 
		Cbuf_AddText( va( "setang_exact %f %f %f\n", m_angLocation.x, m_angLocation.y, m_angLocation.z ) );
	}

#if !defined( SWDS )
	if ( reload_materials.GetBool() )
	{
		// building cubemaps requires the materials to reload after map
		reload_materials.SetValue( 0 );
		Cbuf_AddText( "mat_reloadallmaterials\n" );
	}

	// Spew global texture memory usage if asked to
	if( !CommandLine()->CheckParm( "-dumpvidmemstats" ) ) return;
	
	// dimhotepus: vidmemstats.txt -> gpu_memstats.txt.
	// dimhotepus: Notify user we dump stats.
	constexpr char kGpuMemStatsName[]{"gpu_memstats.txt"};
	Msg("Dumping GPU memory stats to '%s' as requested by '%s' switch.",
		kGpuMemStatsName, "-dumpvidmemstats");

	FileHandle_t fp = g_pFileSystem->Open( kGpuMemStatsName, "a" );
	if (!fp)
	{
		Warning("Unable to open '%s'. GPU VRAM stats will not be available.\n", kGpuMemStatsName);
		return;
	}
	RunCodeAtScopeExit(g_pFileSystem->Close( fp ));

	g_pFileSystem->FPrintf( fp, "%s:\n", g_HostState.m_levelName );
	
#ifdef VPROF_ENABLED
	constexpr char kGroupNamePrefix[]{"TexGroup_Global_"};
	constexpr intp kGroupNamePrefixLen{ssize( kGroupNamePrefix ) - 1};
	constexpr float kMibsMultiplier{1.0f / (1024.0f * 1024.0f)};

	const CVProfile &profile{g_VProfCurrentProfile};
	float totalMibs{0.0f};

	for ( int i = 0; i < profile.GetNumCounters(); i++ )
	{
		if ( profile.GetCounterGroup( i ) == COUNTER_GROUP_TEXTURE_GLOBAL )
		{
			const float valueMibs = profile.GetCounterValue( i ) * kMibsMultiplier;

			totalMibs += valueMibs;

			const char *counterName = profile.GetCounterName( i );
			if ( Q_strnicmp( counterName, kGroupNamePrefix, kGroupNamePrefixLen ) == 0 )
			{
				counterName += kGroupNamePrefixLen;
			}

			g_pFileSystem->FPrintf( fp, "%s: %0.3f MiB\n", counterName, valueMibs );
		}
	}
	g_pFileSystem->FPrintf( fp, "GPU memory total: %0.3f MiB\n", totalMibs );
#else
	// dimhotepus: Notify user VProfiler is not supported.
	g_pFileSystem->FPrintf( fp, "VProfiler is not supported.\n" );
#endif

	g_pFileSystem->FPrintf( fp, "---------------------------------\n" );

	Cbuf_AddText( "quit\n" );
#endif
}

void CHostState::OnClientDisconnected() 
{
}

// Determine if this is a valid game
static bool Host_ValidGame()
{
	// No multi-client single player games
	if ( sv.IsMultiplayer() )
	{
		if ( deathmatch.GetInt() )
			return true;
	}
	else
	{
		return true;
	}

	ConDMsg("Unable to launch game\n");
	return false;
}

