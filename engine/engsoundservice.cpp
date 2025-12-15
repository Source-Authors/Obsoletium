//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Engine implementation of services required by the audio subsystem
//
// $NoKeywords: $
//=============================================================================//

#include "quakedef.h"
#include "soundservice.h"
#include "zone.h"
#include "cdll_engine_int.h"
#include "gl_model_private.h"
#include "icliententity.h"
#include "icliententitylist.h"
#include "mouthinfo.h"
#include "host.h"
#include "vstdlib/random.h"
#include "tier0/icommandline.h"
#include "igame.h"
#include "client.h"
#include "server.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "sound.h"
#include "vgui_controls/Controls.h"
#include "vgui/ILocalize.h"
#include "vgui_baseui_interface.h"
#include "datacache/idatacache.h"
#include "sys_dll.h"
#include "toolframework/itoolframework.h"
#include "tier0/vprof.h"
#include "cl_steamauth.h"
#include "tier1/fmtstr.h"
#include "MapReslistGenerator.h"
#include "cl_main.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void Snd_Restart_f();

#define MAPLIST_FILE "maplist.txt"

class CEngineSoundServices : public ISoundServices
{
public:
	CEngineSoundServices() { m_frameTime = 0; }

	virtual void *LevelAlloc( unsigned nBytes, const char *pszTag )
	{
		return Hunk_AllocName<byte>(nBytes, pszTag);
	}

	virtual void OnExtraUpdate()
	{
		if ( IsPC() && g_ClientDLL && game && game->IsActiveApp() )
		{
			g_ClientDLL->IN_Accumulate();
		}
	}

	virtual bool GetSoundSpatialization( int entIndex, SpatializationInfo_t& info )
	{
		if ( !entitylist )
		{
			return false;
		}

		// Entity has been deleted
		IClientEntity *pClientEntity = entitylist->GetClientEntity( entIndex );
		if ( !pClientEntity )
		{
			// FIXME:  Should this assert?
			return false;
		}
		
		MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );
		bool bResult = pClientEntity->GetSoundSpatialization( info );

		return bResult;
	}

	virtual bool GetToolSpatialization( int iUserData, int guid, SpatializationInfo_t& info )
	{
		if ( IsX360() )
		{
			return false;
		}

		return toolframework->GetSoundSpatialization( iUserData, guid, info );
	}

	virtual float GetClientTime()
	{
		return cl.GetTime();
	}

	// Filtered local time
	virtual float GetHostTime()
	{
		return host_time;
	}

	virtual int GetViewEntity()
	{
		return cl.m_nViewEntity;
	}

	virtual void SetSoundFrametime( float realDt, float hostDt )
	{
		extern bool IsReplayRendering();
		if ( cl_movieinfo.IsRecording() || IsReplayRendering() )
		{
			m_frameTime = hostDt;
		}
		else
		{
			m_frameTime = realDt;
		}
	}

	virtual float GetHostFrametime()
	{
		return m_frameTime;
	}

	virtual int GetServerCount()
	{
		return cl.m_nServerCount;
	}

	virtual bool IsPlayer( SoundSource source )
	{
		return ( source == cl.m_nPlayerSlot + 1 );
	}

	virtual void OnChangeVoiceStatus( int entity, bool status)
	{
		ClientDLL_VoiceStatus(entity, status);
	}

	virtual bool IsConnected() 
	{
		return cl.IsConnected();
	}

	// Calls into client .dll with list of close caption tokens to construct a caption out of
	virtual void EmitSentenceCloseCaption( char const *tokenstream )
	{
		if ( g_ClientDLL )
		{
			g_ClientDLL->EmitSentenceCloseCaption( tokenstream );
		}
	}

	virtual void EmitCloseCaption( char const *captionname, float duration )
	{
		if ( g_ClientDLL )
		{
			g_ClientDLL->EmitCloseCaption( captionname, duration );
		}
	}

	virtual char const *GetGameDir() 
	{
		return com_gamedir;
	}

	// If the game is paused, certain audio will pause, too (anything with phoneme/sentence data for now)
	virtual bool IsGamePaused()
	{
		extern IVEngineClient *engineClient;
		if ( !engineClient )
		{
			AssertMsg( false, "No engineClient, bug???" );
			return false;
		}

		return engineClient->IsPaused();
	}
	
	virtual bool IsGameActive()
	{
		extern IVEngineClient *engineClient;
		if ( !engineClient )
		{
			AssertMsg( false, "No engineClient, bug???" );
			return true;
		}

		return engineClient->IsActiveApp();
	}

	virtual void RestartSoundSystem()
	{
		Snd_Restart_f();
	}

	virtual void GetAllManifestFiles( CUtlRBTree< FileNameHandle_t, int >& list )
	{
		list.RemoveAll();

		char token[COM_TOKEN_MAX_LENGTH];

		// Load them in
		FileHandle_t resfilehandle = g_pFileSystem->Open( MAPLIST_FILE, "rb", "MOD" );
		if ( FILESYSTEM_INVALID_HANDLE != resfilehandle )
		{
			// Read in and parse mapcycle.txt
			int length = g_pFileSystem->Size(resfilehandle);
			if ( length > 0 )
			{
				std::unique_ptr<char[]> pStart = std::make_unique<char[]>( length + 1 );
				if ( pStart && ( length == g_pFileSystem->Read(pStart.get(), length, resfilehandle) )
				   )
				{
					pStart[ length ] = 0;
					const char *pFileList = pStart.get();

					while ( 1 )
					{
						pFileList = COM_Parse( pFileList, token );
						if ( Q_isempty( token ) )
							break;

						char manifest_file[ 512 ];
						V_sprintf_safe( manifest_file, "%s/%s.manifest", AUDIOSOURCE_CACHE_ROOTDIR, token );

						if ( g_pFileSystem->FileExists( manifest_file, "MOD" ) )
						{
							FileNameHandle_t handle = g_pFileSystem->FindOrAddFileName( manifest_file );
							if ( list.Find( handle ) == list.InvalidIndex() )
							{
								list.Insert( handle );
							}
						}

						// Any more tokens on this line?
						while ( COM_TokenWaiting( pFileList ) )
						{
							pFileList = COM_Parse( pFileList, token );
						}
					}
				}
			}

			g_pFileSystem->Close(resfilehandle);
		}
		else
		{
			Warning( "GetAllManifestFiles:  Unable to load %s\n", MAPLIST_FILE );
		}
	}

	virtual void GetAllSoundFilesInManifest( CUtlRBTree< FileNameHandle_t, int >& list, char const *manifestfile )
	{
		list.RemoveAll();
		CacheSoundsFromResFile( true, list, manifestfile, false );
	}

	virtual void GetAllSoundFilesReferencedInReslists( CUtlRBTree< FileNameHandle_t, int >& list )
	{
		char reslistdir[ MAX_PATH ];
		Q_strncpy( reslistdir, MapReslistGenerator().GetResListDirectory(), sizeof( reslistdir ) );
		list.RemoveAll();

		char token[COM_TOKEN_MAX_LENGTH];

		// Load them in
		FileHandle_t resfilehandle = g_pFileSystem->Open( MAPLIST_FILE, "rb", "MOD" );
		if ( FILESYSTEM_INVALID_HANDLE != resfilehandle )
		{
			// Read in and parse mapcycle.txt
			int length = g_pFileSystem->Size(resfilehandle);
			if ( length > 0 )
			{
				std::unique_ptr<char[]> pStart = std::make_unique<char[]>( length + 1 );
				if ( pStart && ( length == g_pFileSystem->Read(pStart.get(), length, resfilehandle) )
				   )
				{
					pStart[ length ] = 0;
					const char *pFileList = pStart.get();

					while ( 1 )
					{
						char resfile[ 512 ];

						pFileList = COM_Parse( pFileList, token );
						if ( Q_isempty( token ) )
							break;

						Q_snprintf( resfile, sizeof( resfile ), "%s\\%s.lst", reslistdir, token );

						CacheSoundsFromResFile( false, list, resfile );

						// Any more tokens on this line?
						while ( COM_TokenWaiting( pFileList ) )
						{
							pFileList = COM_Parse( pFileList, token );
						}
					}
				}
			}

			g_pFileSystem->Close(resfilehandle);

			CacheSoundsFromResFile( false, list, CFmtStr( "%s\\engine.lst", reslistdir ) );
			CacheSoundsFromResFile( false, list, CFmtStr( "%s\\all.lst", reslistdir ) );
		}
		else
		{
			Warning( "GetAllSoundFilesReferencedInReslists:  Unable to load file %s\n", MAPLIST_FILE );
		}
	}

	virtual void CacheBuildingStart()
	{
		if ( IsX360() )
		{
			return;
		}

		EngineVGui()->ActivateGameUI();
		EngineVGui()->StartCustomProgress();
		const wchar_t *str = g_pVGuiLocalize->Find( "#Valve_CreatingCache" );
		if ( str )
		{
			EngineVGui()->UpdateCustomProgressBar( 0.0f, str );
		}
	}

	virtual void CacheBuildingUpdateProgress( float percent, char const *cachefile )
	{
		if ( IsX360() )
		{
			return;
		}

		const wchar_t *format = g_pVGuiLocalize->Find( "Valve_CreatingSpecificSoundCache" );
		if ( format )
		{
			wchar_t constructed[ 1024 ];
			wchar_t file[ 256 ];
			g_pVGuiLocalize->ConvertANSIToUnicode( cachefile, file );

			g_pVGuiLocalize->ConstructString_safe( 
				constructed, 
				format,
				1,
				file );

			EngineVGui()->UpdateCustomProgressBar( percent, constructed );
		}
	}

	virtual void CacheBuildingFinish()
	{
		if ( IsX360() )
		{
			return;
		}

		EngineVGui()->FinishCustomProgress();
		EngineVGui()->HideGameUI();
	}

	virtual int GetPrecachedSoundCount()
	{
		if ( !sv.IsActive() )
			return 0;

		INetworkStringTable *table = sv.GetSoundPrecacheTable();
		if ( !table )
			return 0;

		return table->GetNumStrings();
	}

	virtual char const *GetPrecachedSound( int index )
	{
		Assert( sv.IsActive() );

		INetworkStringTable *table = sv.GetSoundPrecacheTable();
		if ( !table )
			return "";

		return table->GetString( index );
	}

	virtual char const *GetUILanguage()
	{
		extern ConVar cl_language;
		return cl_language.GetString();
	}

private:

	float m_frameTime;

	void CacheSoundsFromResFile( bool quiet, CUtlRBTree< FileNameHandle_t, int >& list, char const *resfile, bool checkandcleanname = true )
	{
		if ( !g_pFileSystem->FileExists( resfile, "MOD" ) )
		{
			Warning( "CacheSoundsFromResFile:  Unable to find '%s'\n", resfile );
			return;
		}

		int oldCount = list.Count();

		char token[COM_TOKEN_MAX_LENGTH];

		FileHandle_t resfilehandle = g_pFileSystem->Open( resfile, "rb", "MOD" );
		if ( FILESYSTEM_INVALID_HANDLE != resfilehandle )
		{
			// Read in and parse mapcycle.txt
			int length = g_pFileSystem->Size(resfilehandle);
			if ( length > 0 )
			{
				std::unique_ptr<char[]> pStart = std::make_unique<char[]>( length + 1 );
				if ( pStart && ( length == g_pFileSystem->Read(pStart.get(), length, resfilehandle) )
				   )
				{
					pStart[ length ] = 0;
					const char *pFileList = pStart.get();

					while ( 1 )
					{
						pFileList = COM_Parse( pFileList, token );
						if ( Q_isempty( token ) )
							break;

						if ( checkandcleanname )
						{
							if ( Q_stristr( token, ".wav" ) ||
								 Q_stristr( token, ".mp3" ) )
							{
								// skip past the game/mod directory		   "hl2/sound/player/footstep.wav"
								Q_FixSlashes(token);				// "hl2\sound\player\footstep.wav"
								const char *pName = token;
								while (pName[0] && pName[0] != CORRECT_PATH_SEPARATOR)
								{
									pName++;
								}										// "\sound\player\footstep.wav"
								FileNameHandle_t handle = g_pFileSystem->FindOrAddFileName( pName+1 );   // "sound\player\footstep.wav"
								if ( list.Find( handle ) == list.InvalidIndex() )
								{
									list.Insert( handle );
								}
							}
						}
						else
						{
							FileNameHandle_t handle = g_pFileSystem->FindOrAddFileName( token );
							if ( list.Find( handle ) == list.InvalidIndex() )
							{
								list.Insert( handle );
							}
						}
					}
				}
			}

			g_pFileSystem->Close(resfilehandle);
		}

		int newCount = list.Count();

		if ( !quiet )
		{
			Msg( "Processing (%i new) from %s\n", newCount - oldCount, resfile );
		}
	}

	virtual void OnSoundStarted( int guid, StartSoundParams_t& params, char const *soundname )
	{
		VPROF("OnSoundStarted");

		if ( !toolframework->IsToolRecording() || params.suppressrecording )
			return;

		KeyValuesAD msg( "StartSound" );
		msg->SetInt( "guid", guid );
		msg->SetFloat( "time", cl.GetTime() );
		msg->SetInt( "staticsound", params.staticsound ? 1 : 0 );
		msg->SetInt( "soundsource", params.soundsource );
		msg->SetInt( "entchannel", params.entchannel );
		msg->SetString( "soundname", soundname );
		msg->SetFloat( "originx", params.origin.x );
		msg->SetFloat( "originy", params.origin.y );
		msg->SetFloat( "originz", params.origin.z );
		msg->SetFloat( "directionx", params.direction.x );
		msg->SetFloat( "directiony", params.direction.y );
		msg->SetFloat( "directionz", params.direction.z );
		msg->SetInt( "updatepositions", params.bUpdatePositions );
		msg->SetFloat( "fvol", params.fvol );
		msg->SetInt( "soundlevel", (int)params.soundlevel );
		msg->SetInt( "flags", params.flags );
		msg->SetInt( "pitch", params.pitch );
		msg->SetInt( "specialdsp", params.specialdsp );
		msg->SetInt( "fromserver", params.fromserver ? 1 : 0 );
		msg->SetFloat( "delay", params.delay );
		msg->SetInt( "speakerentity", params.speakerentity );

		toolframework->PostMessage( msg );
	}

	virtual void OnSoundStopped( int guid, int soundsource, int channel, char const *soundname )
	{
		// NOTE: At the moment, if we don't receive a StartSound message but we do
		// receive a StopSound message, the StopSound message is ignored. In a perfect
		// world, if the StartSound message was not sent, a StopSound message should not
		// be sent for that guid either. This requires more plumbing, though, and
		// for the moment, it's not necessary to do that plumbing.

		VPROF("OnSoundStopped");

		if ( !toolframework->IsToolRecording() )
			return;

		KeyValuesAD msg( "StopSound" );
		msg->SetInt( "guid", guid );
		msg->SetFloat( "time", cl.GetTime() );
		msg->SetInt( "soundsource", soundsource );
		msg->SetInt( "entchannel", channel );
		msg->SetString( "soundname", soundname );

		toolframework->PostMessage( msg );
	}
};


static CEngineSoundServices g_EngineSoundServices;
ISoundServices *g_pSoundServices = &g_EngineSoundServices;
