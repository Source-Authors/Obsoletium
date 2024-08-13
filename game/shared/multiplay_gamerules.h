//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MULTIPLAY_GAMERULES_H
#define MULTIPLAY_GAMERULES_H
#ifdef _WIN32
#pragma once
#endif


#include "gamerules.h"

#ifdef CLIENT_DLL

	#define CMultiplayRules C_MultiplayRules

#else

extern ConVar mp_restartgame;
extern ConVar mp_restartgame_immediate;
extern ConVar mp_waitingforplayers_time;
extern ConVar mp_waitingforplayers_restart;
extern ConVar mp_waitingforplayers_cancel;
extern ConVar mp_clan_readyrestart;
extern ConVar mp_clan_ready_signal;
extern ConVar nextlevel;
extern INetworkStringTable *g_pStringTableServerMapCycle;

#if defined ( TF_DLL ) || defined ( TF_CLIENT_DLL )
extern INetworkStringTable *g_pStringTableServerPopFiles;
extern INetworkStringTable *g_pStringTableServerMapCycleMvM;
#endif

#define VOICE_COMMAND_MAX_SUBTITLE_DIST	1900

class CBaseMultiplayerPlayer;

#endif

extern ConVar mp_show_voice_icons;

#define MAX_SPEAK_CONCEPT_LEN 64
#define MAX_VOICE_COMMAND_SUBTITLE	256

typedef struct
{
#ifndef CLIENT_DLL
	// concept to speak
	int	 m_iConcept;

	// play subtitle?
	bool m_bShowSubtitle;
	bool m_bDistanceBasedSubtitle;

	char m_szGestureActivity[64];

#else
	// localizable subtitle
	char m_szSubtitle[MAX_VOICE_COMMAND_SUBTITLE];

	// localizable string for menu
	char m_szMenuLabel[MAX_VOICE_COMMAND_SUBTITLE];
#endif

} VoiceCommandMenuItem_t;

extern ConVar mp_timelimit;

//=========================================================
// CMultiplayRules - rules for the basic half life multiplayer
// competition
//=========================================================
class CMultiplayRules : public CGameRules
{
public:
	DECLARE_CLASS( CMultiplayRules, CGameRules );

// Functions to verify the single/multiplayer status of a game
	bool IsMultiplayer( void ) override;

	bool Init() override;

	// Damage query implementations.
	bool	Damage_IsTimeBased( int iDmgType ) override;			// Damage types that are time-based.
	bool	Damage_ShouldGibCorpse( int iDmgType ) override;		// Damage types that gib the corpse.
	bool	Damage_ShowOnHUD( int iDmgType ) override;				// Damage types that have client HUD art.
	bool	Damage_NoPhysicsForce( int iDmgType ) override;			// Damage types that don't have to supply a physics force & position.
	bool	Damage_ShouldNotBleed( int iDmgType ) override;			// Damage types that don't make the player bleed.
	// TEMP: These will go away once DamageTypes become enums.
	int		Damage_GetTimeBased( void ) override;
	int		Damage_GetShouldGibCorpse( void ) override;
	int		Damage_GetShowOnHud( void ) override;
	int		Damage_GetNoPhysicsForce( void ) override;
	int		Damage_GetShouldNotBleed( void ) override;

	CMultiplayRules();
	virtual ~CMultiplayRules() {}

	void LoadVoiceCommandScript( void );

	bool ShouldDrawHeadLabels() override
	{ 
		if ( mp_show_voice_icons.GetBool() == false )
			return false;

		return BaseClass::ShouldDrawHeadLabels();
	}

#ifndef CLIENT_DLL
	void FrameUpdatePostEntityThink() override;

// GR_Think
	void Think( void ) override;
	void RefreshSkillData( bool forceUpdate ) override;
	bool IsAllowedToSpawn( CBaseEntity *pEntity ) override;
	bool FAllowFlashlight( void ) override;

	bool FShouldSwitchWeapon( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon ) override;
	CBaseCombatWeapon *GetNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon ) override;
	bool SwitchToNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon ) override;

// Functions to verify the single/multiplayer status of a game
	bool IsDeathmatch( void ) override;
	bool IsCoOp( void ) override;

// Client connection/disconnection
	// If ClientConnected returns FALSE, the connection is rejected and the user is provided the reason specified in
	//  svRejectReason
	// Only the client's name and remote address are provided to the dll for verification.
	bool ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen ) override;
	void InitHUD( CBasePlayer *pl ) override;		// the client dll is ready for updating
	void ClientDisconnected( edict_t *pClient ) override;

// Client damage rules
	float FlPlayerFallDamage( CBasePlayer *pPlayer ) override;
	bool  FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker, const CTakeDamageInfo &info ) override;
	bool AllowDamage( CBaseEntity *pVictim, const CTakeDamageInfo &info ) override;

// Client spawn/respawn control
	void PlayerSpawn( CBasePlayer *pPlayer ) override;
	void PlayerThink( CBasePlayer *pPlayer ) override;
	bool FPlayerCanRespawn( CBasePlayer *pPlayer ) override;
	float FlPlayerSpawnTime( CBasePlayer *pPlayer ) override;
	CBaseEntity *GetPlayerSpawnSpot( CBasePlayer *pPlayer ) override;

	bool AllowAutoTargetCrosshair( void ) override;

// Client kills/scoring
	int IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled ) override;
	void PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info ) override;
	void DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info ) override;
	CBasePlayer *GetDeathScorer( CBaseEntity *pKiller, CBaseEntity *pInflictor );									// old version of method - kept for backward compat
	virtual CBasePlayer *GetDeathScorer( CBaseEntity *pKiller, CBaseEntity *pInflictor, CBaseEntity *pVictim );		// new version of method

// Weapon retrieval
	bool CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon ) override;// The player is touching an CBaseCombatWeapon, do I give it to him?

// Weapon spawn/respawn control
	int WeaponShouldRespawn( CBaseCombatWeapon *pWeapon ) override;
	float FlWeaponRespawnTime( CBaseCombatWeapon *pWeapon ) override;
	float FlWeaponTryRespawn( CBaseCombatWeapon *pWeapon ) override;
	Vector VecWeaponRespawnSpot( CBaseCombatWeapon *pWeapon ) override;

// Item retrieval
	bool CanHaveItem( CBasePlayer *pPlayer, CItem *pItem ) override;
	void PlayerGotItem( CBasePlayer *pPlayer, CItem *pItem ) override;

// Item spawn/respawn control
	int ItemShouldRespawn( CItem *pItem ) override;
	float FlItemRespawnTime( CItem *pItem ) override;
	Vector VecItemRespawnSpot( CItem *pItem ) override;
	QAngle VecItemRespawnAngles( CItem *pItem ) override;

// Ammo retrieval
	void PlayerGotAmmo( CBaseCombatCharacter *pPlayer, char *szName, int iCount ) override;

// Healthcharger respawn control
	float FlHealthChargerRechargeTime( void ) override;
	float FlHEVChargerRechargeTime( void ) override;

// What happens to a dead player's weapons
	int DeadPlayerWeapons( CBasePlayer *pPlayer ) override;

// What happens to a dead player's ammo	
	int DeadPlayerAmmo( CBasePlayer *pPlayer ) override;

// Teamplay stuff	
	const char *GetTeamID( CBaseEntity *pEntity ) override {return "";}
	int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget ) override;
	bool PlayerCanHearChat( CBasePlayer *pListener, CBasePlayer *pSpeaker ) override;

	bool PlayTextureSounds( void ) override { return FALSE; }
	bool PlayFootstepSounds( CBasePlayer *pl ) override;

// NPCs
	bool FAllowNPCs( void ) override;
	
	// Immediately end a multiplayer game
	void EndMultiplayerGame( void ) override { GoToIntermission(); }

// Voice commands
	bool ClientCommand( CBaseEntity *pEdict, const CCommand &args ) override;
	virtual VoiceCommandMenuItem_t *VoiceCommand( CBaseMultiplayerPlayer *pPlayer, int iMenu, int iItem );
	
// Bugbait report	
	bool IsLoadingBugBaitReport( void );

	void ResetMapCycleTimeStamp( void ) override { m_nMapCycleTimeStamp = 0; }

	virtual void HandleTimeLimitChange( void ) {}

	void IncrementMapCycleIndex();

	void HaveAllPlayersSpeakConceptIfAllowed( int iConcept, int iTeam = TEAM_UNASSIGNED, const char *modifiers = NULL );
	void RandomPlayersSpeakConceptIfAllowed( int iConcept, int iNumRandomPlayer = 1, int iTeam = TEAM_UNASSIGNED, const char *modifiers = NULL );

	void GetTaggedConVarList( KeyValues *pCvarTagList ) override;

	void SkipNextMapInCycle();

	void	ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues ) override;

public:

	struct ResponseRules_t
	{
		CUtlVector<IResponseSystem*> m_ResponseSystems;
	};
	CUtlVector<ResponseRules_t>	m_ResponseRules;

	virtual void InitCustomResponseRulesDicts()	{}
	virtual void ShutdownCustomResponseRulesDicts() {}

	// NVNT virtual to check for haptic device 
	void ClientSettingsChanged( CBasePlayer *pPlayer ) override;
	virtual void GetNextLevelName( char *szNextMap, int bufsize, bool bRandom = false );

	static void DetermineMapCycleFilename( char *pszResult, int nSizeResult, bool bForceSpew );
	virtual void LoadMapCycleFileIntoVector ( const char *pszMapCycleFile, CUtlVector<char *> &mapList );
	static void FreeMapCycleFileVector ( CUtlVector<char *> &mapList );

	// LoadMapCycleFileIntoVector without the fixups inherited versions of gamerules may provide
	static void RawLoadMapCycleFileIntoVector ( const char *pszMapCycleFile, CUtlVector<char *> &mapList );

	bool IsMapInMapCycle( const char *pszName );

	virtual bool IsManualMapChangeOkay( const char **pszReason ) override;

protected:
	virtual bool UseSuicidePenalty() { return true; }		// apply point penalty for suicide?
 	virtual float GetLastMajorEventTime( void ){ return -1.0f; }

public:
	virtual void ChangeLevel( void );

protected:
	virtual void GoToIntermission( void );
	virtual void LoadMapCycleFile( void );
	void ChangeLevelToMap( const char *pszMap );

	float m_flIntermissionEndTime;
	static int m_nMapCycleTimeStamp;
	static int m_nMapCycleindex;
	static CUtlVector<char*> m_MapList;

	float m_flTimeLastMapChangeOrPlayerWasConnected;

#else
	
	public:
		const char *GetVoiceCommandSubtitle( int iMenu, int iItem );
		bool GetVoiceMenuLabels( int iMenu, KeyValues *pKV );

#endif

	private:
		CUtlVector< CUtlVector< VoiceCommandMenuItem_t > > m_VoiceCommandMenus;
};

inline CMultiplayRules* MultiplayRules()
{
	return static_cast<CMultiplayRules*>(g_pGameRules);
}

#endif // MULTIPLAY_GAMERULES_H
