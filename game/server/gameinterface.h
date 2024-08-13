//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Expose things from GameInterface.cpp. Mostly the engine interfaces.
//
// $NoKeywords: $
//=============================================================================//

#ifndef GAMEINTERFACE_H
#define GAMEINTERFACE_H

#ifdef _WIN32
#pragma once
#endif

#include "mapentities.h"

class IReplayFactory;

extern INetworkStringTable *g_pStringTableInfoPanel;
extern INetworkStringTable *g_pStringTableServerMapCycle;

#ifdef TF_DLL
extern INetworkStringTable *g_pStringTableServerPopFiles;
#endif

// Player / Client related functions
// Most of this is implemented in gameinterface.cpp, but some of it is per-mod in files like cs_gameinterface.cpp, etc.
class CServerGameClients : public IServerGameClients
{
public:
	virtual bool			ClientConnect( edict_t *pEntity, char const* pszName, char const* pszAddress, char *reject, int maxrejectlen ) override;
	virtual void			ClientActive( edict_t *pEntity, bool bLoadGame ) override;
	virtual void			ClientDisconnect( edict_t *pEntity ) override;
	virtual void			ClientPutInServer( edict_t *pEntity, const char *playername ) override;
	virtual void			ClientCommand( edict_t *pEntity, const CCommand &args ) override;
	virtual void			ClientSettingsChanged( edict_t *pEntity ) override;
	virtual void			ClientSetupVisibility( edict_t *pViewEntity, edict_t *pClient, unsigned char *pvs, int pvssize ) override;
	virtual float			ProcessUsercmds( edict_t *player, bf_read *buf, int numcmds, int totalcmds,
								int dropped_packets, bool ignore, bool paused ) override;
	// Player is running a command
	virtual void			PostClientMessagesSent_DEPRECIATED( void ) override;
	virtual void			SetCommandClient( int index ) override;
	virtual CPlayerState	*GetPlayerState( edict_t *player ) override;
	virtual void			ClientEarPosition( edict_t *pEntity, Vector *pEarOrigin ) override;

	virtual void			GetPlayerLimits( int& minplayers, int& maxplayers, int &defaultMaxPlayers ) const override;
	
	// returns number of delay ticks if player is in Replay mode (0 = no delay)
	virtual int				GetReplayDelay( edict_t *player, int& entity ) override;
	// Anything this game .dll wants to add to the bug reporter text (e.g., the entity/model under the picker crosshair)
	//  can be added here
	virtual void			GetBugReportInfo( char *buf, int buflen ) override;
	virtual void			NetworkIDValidated( const char *pszUserName, const char *pszNetworkID ) override;

	// The client has submitted a keyvalues command
	virtual void			ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues ) override;

	// Notify that the player is spawned
	virtual void			ClientSpawned( edict_t *pPlayer ) override;
};


class CServerGameDLL : public IServerGameDLL
{
public:
	virtual bool			DLLInit(CreateInterfaceFn engineFactory, CreateInterfaceFn physicsFactory, 
										CreateInterfaceFn fileSystemFactory, CGlobalVars *pGlobals) override;
	virtual void			DLLShutdown( void ) override;
	// Get the simulation interval (must be compiled with identical values into both client and game .dll for MOD!!!)
	virtual bool			ReplayInit( CreateInterfaceFn fnReplayFactory ) override;
	virtual float			GetTickInterval( void ) const override;
	virtual bool			GameInit( void ) override;
	virtual void			GameShutdown( void ) override;
	virtual bool			LevelInit( const char *pMapName, char const *pMapEntities, char const *pOldLevel, char const *pLandmarkName, bool loadGame, bool background ) override;
	virtual void			ServerActivate( edict_t *pEdictList, int edictCount, int clientMax ) override;
	virtual void			LevelShutdown( void ) override;
	virtual void			GameFrame( bool simulating ) override; // could be called multiple times before sending data to clients
	virtual void			PreClientUpdate( bool simulating ) override; // called after all GameFrame() calls, before sending data to clients

	virtual ServerClass*	GetAllServerClasses( void ) override;
	virtual const char     *GetGameDescription( void ) override;
	virtual void			CreateNetworkStringTables( void ) override;
	
	// Save/restore system hooks
	virtual CSaveRestoreData  *SaveInit( int size ) override;
	virtual void			SaveWriteFields( CSaveRestoreData *, char const* , void *, datamap_t *, typedescription_t *, int ) override;
	virtual void			SaveReadFields( CSaveRestoreData *, char const* , void *, datamap_t *, typedescription_t *, int ) override;
	virtual void			SaveGlobalState( CSaveRestoreData * ) override;
	virtual void			RestoreGlobalState( CSaveRestoreData * ) override;
	virtual int				CreateEntityTransitionList( CSaveRestoreData *, int ) override;
	virtual void			BuildAdjacentMapList( void ) override;

	virtual void			PreSave( CSaveRestoreData * ) override;
	virtual void			Save( CSaveRestoreData * ) override;
	virtual void			GetSaveComment( char *comment, int maxlength, float flMinutes, float flSeconds, bool bNoTime = false ) override;
#ifdef _XBOX
	virtual void			GetTitleName( const char *pMapName, char* pTitleBuff, int titleBuffSize ) override;
#endif
	virtual void			WriteSaveHeaders( CSaveRestoreData * ) override;

	virtual void			ReadRestoreHeaders( CSaveRestoreData * ) override;
	virtual void			Restore( CSaveRestoreData *, bool ) override;
	virtual bool			IsRestoring() override;

	// Retrieve info needed for parsing the specified user message
	virtual bool			GetUserMessageInfo( int msg_type, char *name, int maxnamelength, int& size ) override;

	virtual CStandardSendProxies*	GetStandardSendProxies() override;

	virtual void			PostInit() override;
	virtual void			Think( bool finalTick ) override;

	virtual void			OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue ) override;

	virtual void			PreSaveGameLoaded( char const *pSaveName, bool bInGame ) override;

	// Returns true if the game DLL wants the server not to be made public.
	// Used by commentary system to hide multiplayer commentary servers from the master.
	virtual bool			ShouldHideServer( void ) override;

	virtual void			InvalidateMdlCache() override;

	virtual void			SetServerHibernation( bool bHibernating ) override;

	float	m_fAutoSaveDangerousTime;
	float	m_fAutoSaveDangerousMinHealthToCommit;
	bool	m_bIsHibernating;

	// Called after the steam API has been activated post-level startup
	virtual void			GameServerSteamAPIActivated( void ) override;

	// Called after the steam API has been shutdown post-level startup
	virtual void			GameServerSteamAPIShutdown( void ) override;

	// interface to the new GC based lobby system
	virtual IServerGCLobby *GetServerGCLobby() override;

	virtual const char *GetServerBrowserMapOverride() override;
	virtual const char *GetServerBrowserGameData() override;

	// Called to add output to the status command
	virtual void 			Status( void (*print) (const char *fmt, ...) ) override;

	virtual void PrepareLevelResources( /* in/out */ char *pszMapName, size_t nMapNameSize,
	                                    /* in/out */ char *pszMapFile, size_t nMapFileSize ) override;

	virtual ePrepareLevelResourcesResult AsyncPrepareLevelResources( /* in/out */ char *pszMapName, size_t nMapNameSize,
	                                                                 /* in/out */ char *pszMapFile, size_t nMapFileSize,
	                                                                 float *flProgress = NULL ) override;

	virtual eCanProvideLevelResult CanProvideLevel( /* in/out */ char *pMapName, int nMapNameMax ) override;

	// Called to see if the game server is okay with a manual changelevel or map command
	virtual bool			IsManualMapChangeOkay( const char **pszReason ) override;

private:

	// This can just be a wrapper on MapEntity_ParseAllEntities, but CS does some tricks in here
	// with the entity list.
	void LevelInit_ParseAllEntities( const char *pMapEntities );
	void LoadMessageOfTheDay();
	void LoadSpecificMOTDMsg( const ConVar &convar, const char *pszStringName );
};


// Normally, when the engine calls ClientPutInServer, it calls a global function in the game DLL
// by the same name. Use this to override the function that it calls. This is used for bots.
typedef CBasePlayer* (*ClientPutInServerOverrideFn)( edict_t *pEdict, const char *playername );

void ClientPutInServerOverride( ClientPutInServerOverrideFn fn );

// -------------------------------------------------------------------------------------------- //
// Entity list management stuff.
// -------------------------------------------------------------------------------------------- //
// These are created for map entities in order as the map entities are spawned.
class CMapEntityRef
{
public:
	int		m_iEdict;			// Which edict slot this entity got. -1 if CreateEntityByName failed.
	int		m_iSerialNumber;	// The edict serial number. TODO used anywhere ?
};

extern CUtlLinkedList<CMapEntityRef, unsigned short> g_MapEntityRefs;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CMapLoadEntityFilter : public IMapEntityFilter
{
public:
	virtual bool ShouldCreateEntity( const char *pClassname )
	{
		// During map load, create all the entities.
		return true;
	}

	virtual CBaseEntity* CreateNextEntity( const char *pClassname )
	{
		CBaseEntity *pRet = CreateEntityByName( pClassname );

		CMapEntityRef ref;
		ref.m_iEdict = -1;
		ref.m_iSerialNumber = -1;

		if ( pRet )
		{
			ref.m_iEdict = pRet->entindex();
			if ( pRet->edict() )
				ref.m_iSerialNumber = pRet->edict()->m_NetworkSerialNumber;
		}

		g_MapEntityRefs.AddToTail( ref );
		return pRet;
	}
};

bool IsEngineThreaded();

class CServerGameTags : public IServerGameTags
{
public:
	virtual void GetTaggedConVarList( KeyValues *pCvarTagList );

};
EXPOSE_SINGLE_INTERFACE( CServerGameTags, IServerGameTags, INTERFACEVERSION_SERVERGAMETAGS );

#endif // GAMEINTERFACE_H

