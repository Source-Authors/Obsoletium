//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL1.
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "tf_player.h"
#include "tf_gamerules.h"
#include "tf_gamestats.h"
#include "KeyValues.h"
#include "viewport_panel_names.h"
#include "client.h"
#include "team.h"
#include "tf_weaponbase.h"
#include "tf_client.h"
#include "tf_team.h"
#include "tf_viewmodel.h"
#include "tf_item.h"
#include "in_buttons.h"
#include "entity_capture_flag.h"
#include "effect_dispatch_data.h"
#include "te_effect_dispatch.h"
#include "game.h"
#include "tf_weapon_builder.h"
#include "tf_obj.h"
#include "tf_ammo_pack.h"
#include "datacache/imdlcache.h"
#include "particle_parse.h"
#include "props_shared.h"
#include "filesystem.h"
#include "toolframework_server.h"
#include "IEffects.h"
#include "func_respawnroom.h"
#include "networkstringtable_gamedll.h"
#include "team_control_point_master.h"
#include "tf_weapon_pda.h"
#include "sceneentity.h"
#include "fmtstr.h"
#include "tf_weapon_sniperrifle.h"
#include "tf_weapon_minigun.h"
#include "tf_weapon_fists.h"
#include "tf_weapon_shotgun.h"
#include "tf_weapon_lunchbox.h"
#include "tf_weapon_knife.h"
#include "tf_weapon_bottle.h"
#include "tf_weapon_sword.h"
#include "tf_weapon_grenade_pipebomb.h"
#include "tf_weapon_buff_item.h"
#include "tf_weapon_flamethrower.h"
#include "tf_projectile_flare.h"
#include "trigger_area_capture.h"
#include "triggers.h"
#include "tf_weapon_medigun.h"
#include "tf_weapon_invis.h"
#include "hl2orange.spa.h"
#include "te_tfblood.h"
#include "activitylist.h"
#include "cdll_int.h"
#include "econ_entity_creation.h"
#include "tf_weaponbase_gun.h"
#include "team_train_watcher.h"
#include "vgui/ILocalize.h"
#include "tier3/tier3.h"
#include "serverbenchmark_base.h"
#include "trains.h"
#include "tf_fx.h"
#include "recipientfilter.h"
#include "ilagcompensationmanager.h"
#include "dt_utlvector_send.h"
#include "tf_item_wearable.h"
#include "tf_item_powerup_bottle.h"
#include "nav_mesh/tf_nav_mesh.h"
#include "tier0/vprof.h"
#include "econ_gcmessages.h"
#include "tf_gcmessages.h"
#include "tf_obj_sentrygun.h"
#include "tf_weapon_shovel.h"
#include "bot/tf_bot.h"
#include "bot/tf_bot_manager.h"
#include "NextBotUtil.h"
#include "tf_wearable_item_demoshield.h"
#include "tier0/icommandline.h"
#include "entity_healthkit.h"
#include "choreoevent.h"
#include "minigames/tf_duel.h"
#include "tf_bot_temp.h"
#include "tf_objective_resource.h"
#include "tf_weapon_pipebomblauncher.h"
#include "func_achievement.h"
#include "halloween/merasmus/merasmus.h"
#include "inetchannel.h"
#include "tf_wearable_levelable_item.h"
#include "tf_weapon_jar.h"
#include "halloween/tf_weapon_spellbook.h"
#include "soundenvelope.h"
#include "tf_triggers.h"
#include "collisionutils.h"
#include "tf_taunt_prop.h"
#include "eventlist.h"
#include "entity_rune.h"
#include "entity_halloween_pickup.h"
#include "tf_gc_server.h"
#include "tf_logic_halloween_2014.h"
#include "tf_weapon_knife.h"
#include "tf_weapon_grapplinghook.h"
#include "tf_dropped_weapon.h"
#include "tf_passtime_logic.h"
#include "tf_weapon_passtime_gun.h"
#include "player_resource.h"
#include "tf_player_resource.h"
#include "gcsdk/gcclient_sharedobjectcache.h"
#include "tf_party.h"
#ifdef STAGING_ONLY
#include "tf_extra_map_entity.h"
#endif

#ifdef TF_RAID_MODE
#include "bot_npc/bot_npc_decoy.h"
#include "raid/tf_raid_logic.h"
#endif

#include "entity_currencypack.h"
#include "tf_mann_vs_machine_stats.h"
#include "player_vs_environment/tf_upgrades.h"
#include "player_vs_environment/tf_population_manager.h"
#include "tf_revive.h"
#include "tf_logic_halloween_2014.h"
#include "tf_logic_player_destruction.h"

// NVNT haptic utils
#include "haptics/haptic_utils.h"

#include "gc_clientsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#pragma warning( disable: 4355 ) // disables ' 'this' : used in base member initializer list'

ConVar sv_motd_unload_on_dismissal( "sv_motd_unload_on_dismissal", "0", 0, "If enabled, the MOTD contents will be unloaded when the player closes the MOTD." );

#define DAMAGE_FORCE_SCALE_SELF				9
#define SCOUT_ADD_BIRD_ON_GIB_CHANCE		5
#define MEDIC_RELEASE_DOVE_COUNT			10

#define JUMP_MIN_SPEED	268.3281572999747f		

extern bool IsInCommentaryMode( void );
extern void SpawnClientsideFlyingBird( Vector &vecSpawn );

extern ConVar	sk_player_head;
extern ConVar	sk_player_chest;
extern ConVar	sk_player_stomach;
extern ConVar	sk_player_arm;
extern ConVar	sk_player_leg;

extern ConVar	tf_spy_invis_time;
extern ConVar	tf_spy_invis_unstealth_time;
extern ConVar	tf_stalematechangeclasstime;
extern ConVar	tf_gravetalk;

extern ConVar	tf_bot_quota_mode;
extern ConVar	tf_bot_quota;
extern ConVar	halloween_starting_souls;

float GetCurrentGravity( void );

float			m_flNextReflectZap = 0.f;

static CTFPlayer *gs_pRecursivePlayerCheck = NULL;

bool CTFPlayer::m_bTFPlayerNeedsPrecache = true;

static const char g_pszIdleKickString[] = "#TF_Idle_kicked";

EHANDLE g_pLastSpawnPoints[TF_TEAM_COUNT];

EHANDLE	g_hTestSub;

ConVar tf_playerstatetransitions( "tf_playerstatetransitions", "-2", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "tf_playerstatetransitions <ent index or -1 for all>. Show player state transitions." );
ConVar tf_playergib( "tf_playergib", "1", FCVAR_NOTIFY, "Allow player gibbing. 0: never, 1: normal, 2: always", true, 0, true, 2 );

ConVar tf_damageforcescale_other( "tf_damageforcescale_other", "6.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar tf_damageforcescale_self_soldier_rj( "tf_damageforcescale_self_soldier_rj", "10.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar tf_damageforcescale_self_soldier_badrj( "tf_damageforcescale_self_soldier_badrj", "5.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar tf_damageforcescale_pyro_jump( "tf_damageforcescale_pyro_jump", "8.5", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar tf_damagescale_self_soldier( "tf_damagescale_self_soldier", "0.60", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );


ConVar tf_damage_range( "tf_damage_range", "0.5", FCVAR_DEVELOPMENTONLY );
ConVar tf_damage_multiplier_blue( "tf_damage_multiplier_blue", "1.0", FCVAR_CHEAT, "All incoming damage to a blue player is multiplied by this value" );
ConVar tf_damage_multiplier_red( "tf_damage_multiplier_red", "1.0", FCVAR_CHEAT, "All incoming damage to a red player is multiplied by this value" );


ConVar tf_max_voice_speak_delay( "tf_max_voice_speak_delay", "1.5", FCVAR_DEVELOPMENTONLY, "Max time after a voice command until player can do another one" );

ConVar tf_allow_player_use( "tf_allow_player_use", "0", FCVAR_NOTIFY, "Allow players to execute +use while playing." );

ConVar tf_deploying_bomb_time( "tf_deploying_bomb_time", "1.90", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Time to deploy bomb before the point of no return." );
ConVar tf_deploying_bomb_delay_time( "tf_deploying_bomb_delay_time", "0.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Time to delay before deploying bomb." );

#ifdef TF_RAID_MODE
ConVar tf_raid_team_size( "tf_raid_team_size", "5", FCVAR_NOTIFY, "Max number of Raiders" );
ConVar tf_raid_respawn_safety_time( "tf_raid_respawn_safety_time", "1.5", FCVAR_NOTIFY, "Number of seconds of invulnerability after respawning" );
ConVar tf_raid_allow_class_change( "tf_raid_allow_class_change", "1", FCVAR_NOTIFY, "If nonzero, allow invaders to change their class after leaving the safe room" );
ConVar tf_raid_use_rescue_closets( "tf_raid_use_rescue_closets", "1", FCVAR_NOTIFY );
ConVar tf_raid_drop_healthkit_chance( "tf_raid_drop_healthkit_chance", "50" ); // , FCVAR_CHEAT );

ConVar tf_boss_battle_team_size( "tf_boss_battle_team_size", "5", FCVAR_NOTIFY, "Max number of players in Boss Battle mode" );
ConVar tf_boss_battle_respawn_safety_time( "tf_boss_battle_respawn_safety_time", "3", FCVAR_NOTIFY, "Number of seconds of invulnerability after respawning" );
ConVar tf_boss_battle_respawn_on_friends( "tf_boss_battle_respawn_on_friends", "1", FCVAR_NOTIFY );
#endif

ConVar tf_mvm_death_penalty( "tf_mvm_death_penalty", "0", FCVAR_NOTIFY | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "How much currency players lose when dying" );
extern ConVar tf_populator_damage_multiplier;
extern ConVar tf_mvm_skill;

#ifdef STAGING_ONLY
ConVar tf_debug_ballistics( "tf_debug_ballistics", "0", FCVAR_CHEAT );
ConVar tf_debug_ballistic_targeting( "tf_debug_ballistic_targeting", "0", FCVAR_CHEAT );
ConVar tf_debug_ballistic_targeting_tolerance( "tf_debug_ballistic_targeting_tolerance", "5", FCVAR_CHEAT );
static Vector tf_debug_ballistic_target( 0, 0, 0 );

ConVar  tf_space_thrust_scout( "tf_space_thrust_scout", "40.0", FCVAR_CHEAT | FCVAR_REPLICATED, "How much thrust is added while holding jump" );
ConVar  tf_space_thrust_sniper( "tf_space_thrust_sniper", "34.0", FCVAR_CHEAT | FCVAR_REPLICATED, "How much thrust is added while holding jump" );
ConVar  tf_space_thrust_spy( "tf_space_thrust_spy", "35.0", FCVAR_CHEAT | FCVAR_REPLICATED, "How much thrust is added while holding jump" );
ConVar  tf_space_thrust_pyro( "tf_space_thrust_pyro", "35.0", FCVAR_CHEAT | FCVAR_REPLICATED, "How much thrust is added while holding jump" );
ConVar  tf_space_thrust_soldier( "tf_space_thrust_soldier", "33.0", FCVAR_CHEAT | FCVAR_REPLICATED, "How much thrust is added while holding jump" );
ConVar  tf_space_thrust_engy( "tf_space_thrust_engy", "35.0", FCVAR_CHEAT | FCVAR_REPLICATED, "How much thrust is added while holding jump" );
ConVar  tf_space_thrust_medic( "tf_space_thrust_medic", "37.0", FCVAR_CHEAT | FCVAR_REPLICATED, "How much thrust is added while holding jump" );
ConVar  tf_space_thrust_heavy( "tf_space_thrust_heavy", "33.0", FCVAR_CHEAT | FCVAR_REPLICATED, "How much thrust is added while holding jump" );
ConVar  tf_space_thrust_demo( "tf_space_thrust_demo", "33.0", FCVAR_CHEAT | FCVAR_REPLICATED, "How much thrust is added while holding jump" );
ConVar  tf_space_thrust_use_rate( "tf_space_thrust_use_rate", "2.0", FCVAR_CHEAT | FCVAR_REPLICATED, "How much fuel is used per tick" );
ConVar  tf_space_thrust_recharge_rate( "tf_space_thrust_recharge_rate", "0.5", FCVAR_CHEAT | FCVAR_REPLICATED, "How much fuel is recharged per tick" );
ConVar  tf_skip_intro_and_spectate( "tf_skip_intro_and_spectate", "0", FCVAR_REPLICATED, "Skip intro panels and start spectating." );
#endif

#ifdef STAGING_ONLY
ConVar tf_highfive_separation_forward( "tf_highfive_separation_forward", "0", FCVAR_CHEAT, "Forward distance between high five partners" );
ConVar tf_highfive_separation_right( "tf_highfive_separation_right", "0", FCVAR_CHEAT, "Right distance between high five partners" );
#else
ConVar tf_highfive_separation_forward( "tf_highfive_separation_forward", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Forward distance between high five partners" );
ConVar tf_highfive_separation_right( "tf_highfive_separation_right", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Right distance between high five partners" );
#endif

ConVar tf_highfive_max_range( "tf_highfive_max_range", "150", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "The farthest away a high five partner can be" );
ConVar tf_highfive_height_tolerance( "tf_highfive_height_tolerance", "12", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "The maximum height difference allowed for two high-fivers." );
ConVar tf_highfive_debug( "tf_highfive_debug", "0", FCVAR_NONE, "Turns on some console spew for debugging high five issues." );

ConVar tf_test_teleport_home_fx( "tf_test_teleport_home_fx", "0", FCVAR_CHEAT );

ConVar tf_halloween_giant_health_scale( "tf_halloween_giant_health_scale", "10", FCVAR_CHEAT );

ConVar tf_grapplinghook_los_force_detach_time( "tf_grapplinghook_los_force_detach_time", "1", FCVAR_CHEAT );
ConVar tf_powerup_max_charge_time( "tf_powerup_max_charge_time", "30", FCVAR_CHEAT );

extern ConVar tf_powerup_mode;
extern ConVar tf_mvm_buybacks_method;
extern ConVar tf_mvm_buybacks_per_wave;

#define TF_CANNONBALL_FORCE_SCALE	80.f
#define TF_CANNONBALL_FORCE_UPWARD	300.f

#ifdef STAGING_ONLY
void CC_tf_debug_ballistic_targeting_mark_target( const CCommand &args )
{
	CBasePlayer *player = UTIL_GetListenServerHost();
	if ( !player )
	{
		return;
	}

	Vector forward;
	AngleVectors( player->EyeAngles() + player->GetPunchAngle(), &forward );

	trace_t result;
	UTIL_TraceLine( player->EyePosition(), player->EyePosition() + 2000.0f * forward, MASK_SHOT, player, COLLISION_GROUP_NONE, &result );

	tf_debug_ballistic_target = result.endpos;
}
static ConCommand tf_debug_ballistic_targeting_mark_target( "tf_debug_ballistic_targeting_mark_target", CC_tf_debug_ballistic_targeting_mark_target, "Mark a spot for testing ballistic targeting.", FCVAR_CHEAT );

ConVar tf_infinite_ammo( "tf_infinite_ammo", "0", FCVAR_CHEAT );

extern ConVar tf_bountymode_currency_starting;
extern ConVar tf_bountymode_upgrades_wipeondeath;
extern ConVar tf_bountymode_currency_penalty_ondeath;
#endif // STAGING_ONLY

ConVar tf_halloween_unlimited_spells( "tf_halloween_unlimited_spells", "0", FCVAR_CHEAT );
extern ConVar tf_halloween_kart_boost_recharge;
extern ConVar tf_halloween_kart_boost_duration;

ConVar tf_halloween_kart_impact_force( "tf_halloween_kart_impact_force", "0.75f", FCVAR_CHEAT, "Impact force scaler" );
ConVar tf_halloween_kart_impact_damage( "tf_halloween_kart_impact_damage", "1.0f", FCVAR_CHEAT, "Impact damage scaler" );
ConVar tf_halloween_kart_impact_rate( "tf_halloween_kart_impact_rate", "0.5f", FCVAR_CHEAT, "rate of allowing impact damage" );
ConVar tf_halloween_kart_boost_impact_force( "tf_halloween_kart_boost_impact_force", "0.75f", FCVAR_CHEAT, "Impact force scaler on boosts" );
ConVar tf_halloween_kart_impact_bounds_scale( "tf_halloween_kart_impact_bounds_scale", "1.0f", FCVAR_CHEAT );
ConVar tf_halloween_kart_impact_feedback( "tf_halloween_kart_impact_feedback", "0.25f", FCVAR_CHEAT );
ConVar tf_halloween_kart_impact_lookahead( "tf_halloween_kart_impact_lookahead", "12.0f", FCVAR_CHEAT );
ConVar tf_halloween_kart_bomb_head_damage_scale( "tf_halloween_kart_bomb_head_damage_scale", "2", FCVAR_CHEAT );
ConVar tf_halloween_kart_bomb_head_impulse_scale( "tf_halloween_kart_bomb_head_impulse_scale", "2", FCVAR_CHEAT );
ConVar tf_halloween_kart_impact_air_scale( "tf_halloween_kart_impact_air_scale", "0.75f", FCVAR_CHEAT );
ConVar tf_halloween_kart_damage_to_force( "tf_halloween_kart_damage_to_force", "300.0f", FCVAR_CHEAT );
ConVar tf_halloween_kart_stun_duration_scale( "tf_halloween_kart_stun_duration_scale", "0.70f", FCVAR_CHEAT );
ConVar tf_halloween_kart_stun_amount( "tf_halloween_kart_stun_amount", "1.0f", FCVAR_CHEAT );
ConVar tf_halloween_kart_stun_enabled( "tf_halloween_kart_stun_enabled", "1", FCVAR_CHEAT );

ConVar tf_tauntcam_fov_override( "tf_tauntcam_fov_override", "0", FCVAR_CHEAT );

ConVar tf_nav_in_combat_range( "tf_nav_in_combat_range", "1000", FCVAR_CHEAT );

ConVar tf_halloween_kart_punting_ghost_force_scale( "tf_halloween_kart_punting_ghost_force_scale", "4", FCVAR_CHEAT );
ConVar tf_halloween_allow_ghost_hit_by_kart_delay( "tf_halloween_allow_ghost_hit_by_kart_delay", "0.5", FCVAR_CHEAT );

extern ConVar tf_feign_death_duration;
extern ConVar spec_freeze_time;
extern ConVar spec_freeze_traveltime;
extern ConVar sv_maxunlag;
extern ConVar tf_allow_taunt_switch;
extern ConVar weapon_medigun_chargerelease_rate;
extern ConVar tf_scout_energydrink_consume_rate;
extern ConVar tf_mm_trusted;
extern ConVar mp_spectators_restricted;
extern ConVar mp_teams_unbalance_limit;
extern ConVar tf_tournament_classchange_allowed;
extern ConVar tf_tournament_classchange_ready_allowed;
#if !defined(NDEBUG) || defined( STAGING_ONLY )
extern ConVar mp_developer;
#endif  // !NDEBUG || STAGING_ONLY 
#ifdef STAGING_ONLY
extern ConVar tf_skillrating_debug_bots_allowed;
#endif // STAGING_ONLY

extern CBaseEntity *FindPickerEntity( CBasePlayer *pPlayer );
extern bool CanScatterGunKnockBack( CTFWeaponBase *pWeapon, float flDamage, float flDistanceSq );

static const char *s_pszTauntRPSParticleNames[] =
{
	"rps_rock_red",
	"rps_paper_red",
	"rps_scissors_red",
	"rps_rock_red_win",
	"rps_paper_red_win",
	"rps_scissors_red_win",
	"rps_rock_blue",
	"rps_paper_blue",
	"rps_scissors_blue",
	"rps_rock_blue_win",
	"rps_paper_blue_win",
	"rps_scissors_blue_win"
};

// -------------------------------------------------------------------------------- //
// Player animation event. Sent to the client when a player fires, jumps, reloads, etc..
// -------------------------------------------------------------------------------- //

class CTEPlayerAnimEvent : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEPlayerAnimEvent, CBaseTempEntity );
	DECLARE_SERVERCLASS();

	CTEPlayerAnimEvent( const char *name ) : CBaseTempEntity( name )
	{
		m_iPlayerIndex = TF_PLAYER_INDEX_NONE;
	}

	CNetworkVar( int, m_iPlayerIndex );
	CNetworkVar( int, m_iEvent );
	CNetworkVar( int, m_nData );
};

IMPLEMENT_SERVERCLASS_ST_NOBASE( CTEPlayerAnimEvent, DT_TEPlayerAnimEvent )
	SendPropInt( SENDINFO( m_iPlayerIndex ), 7, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iEvent ), Q_log2( PLAYERANIMEVENT_COUNT ) + 1, SPROP_UNSIGNED ),
	// BUGBUG:  ywb  we assume this is either 0 or an animation sequence #, but it could also be an activity, which should fit within this limit, but we're not guaranteed.
	SendPropInt( SENDINFO( m_nData ), ANIMATION_SEQUENCE_BITS ),
END_SEND_TABLE()

static CTEPlayerAnimEvent g_TEPlayerAnimEvent( "PlayerAnimEvent" );

void TE_PlayerAnimEvent( CBasePlayer *pPlayer, PlayerAnimEvent_t event, int nData )
{
    Vector vecEyePos = pPlayer->EyePosition();
	CPVSFilter filter( vecEyePos );
	if ( !IsCustomPlayerAnimEvent( event ) && ( event != PLAYERANIMEVENT_SNAP_YAW ) && ( event != PLAYERANIMEVENT_VOICE_COMMAND_GESTURE ) )
	{
		// if prediction is off, alway send jump
		if ( !( ( event == PLAYERANIMEVENT_JUMP ) && ( FStrEq(engine->GetClientConVarValue( pPlayer->entindex(), "cl_predict" ), "0" ) ) ) )
		{
			filter.RemoveRecipient( pPlayer );
		}
	}

	Assert( pPlayer->entindex() >= 1 && pPlayer->entindex() <= MAX_PLAYERS );
	g_TEPlayerAnimEvent.m_iPlayerIndex = pPlayer->entindex();
	g_TEPlayerAnimEvent.m_iEvent = event;
	Assert( nData < (1<<ANIMATION_SEQUENCE_BITS) );
	Assert( (1<<ANIMATION_SEQUENCE_BITS) >= ActivityList_HighestIndex() );
	g_TEPlayerAnimEvent.m_nData = nData;
	g_TEPlayerAnimEvent.Create( filter, 0 );
}

//=================================================================================
//
// Ragdoll Entity
//
class CTFRagdoll : public CBaseAnimatingOverlay
{
public:

	DECLARE_CLASS( CTFRagdoll, CBaseAnimatingOverlay );
	DECLARE_SERVERCLASS();

	CTFRagdoll()
	{
		m_iPlayerIndex.Set( TF_PLAYER_INDEX_NONE );
		m_bGib = false;
		m_bBurning = false;
		m_bElectrocuted = false;
		m_bFeignDeath = false;
		m_bWasDisguised = false;
		m_bBecomeAsh = false;
		m_bOnGround = false;
		m_bCloaked = false;
		m_iDamageCustom = 0;
		m_bCritOnHardHit = false;
		m_vecRagdollOrigin.Init();
		m_vecRagdollVelocity.Init();
	}

	~CTFRagdoll()
	{
		// Destroy all of our attached wearables.
		for ( int i=0; i<m_hRagWearables.Count(); ++i )
		{
			if ( m_hRagWearables[i] )
			{
				m_hRagWearables[i]->Remove();
			}
		}
		m_hRagWearables.Purge();
	}

	// Transmit ragdolls to everyone.
	virtual int UpdateTransmitState()
	{
		UseClientSideAnimation();
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	CNetworkVar( int, m_iPlayerIndex );
	CNetworkVector( m_vecRagdollVelocity );
	CNetworkVector( m_vecRagdollOrigin );
	CNetworkVar( bool, m_bGib );
	CNetworkVar( bool, m_bBurning );
	CNetworkVar( bool, m_bElectrocuted );
	CNetworkVar( bool, m_bFeignDeath );
	CNetworkVar( bool, m_bWasDisguised );
	CNetworkVar( bool, m_bBecomeAsh );
	CNetworkVar( bool, m_bOnGround );
	CNetworkVar( bool, m_bCloaked );
	CNetworkVar( int, m_iDamageCustom );
	CNetworkVar( int, m_iTeam );
	CNetworkVar( int, m_iClass );
	CNetworkVar( bool, m_bGoldRagdoll );
	CNetworkVar( bool, m_bIceRagdoll );
	CNetworkVar( bool, m_bCritOnHardHit );
	CNetworkVar( float, m_flHeadScale );
	CNetworkVar( float, m_flTorsoScale );
	CNetworkVar( float, m_flHandScale );
	CUtlVector<CHandle<CEconWearable > > m_hRagWearables;
};

LINK_ENTITY_TO_CLASS( tf_ragdoll, CTFRagdoll );

IMPLEMENT_SERVERCLASS_ST_NOBASE( CTFRagdoll, DT_TFRagdoll )
	SendPropVector( SENDINFO( m_vecRagdollOrigin ), -1,  SPROP_COORD ),
	SendPropInt( SENDINFO( m_iPlayerIndex ), 7, SPROP_UNSIGNED ),
	SendPropVector	( SENDINFO(m_vecForce), -1, SPROP_NOSCALE ),
	SendPropVector( SENDINFO( m_vecRagdollVelocity ), 13, SPROP_ROUNDDOWN, -2048.0f, 2048.0f ),
	SendPropInt( SENDINFO( m_nForceBone ) ),
	SendPropBool( SENDINFO( m_bGib ) ),
	SendPropBool( SENDINFO( m_bBurning ) ),
	SendPropBool( SENDINFO( m_bElectrocuted ) ),
	SendPropBool( SENDINFO( m_bFeignDeath ) ),
	SendPropBool( SENDINFO( m_bWasDisguised ) ),
	SendPropBool( SENDINFO( m_bBecomeAsh ) ),
	SendPropBool( SENDINFO( m_bOnGround ) ),
	SendPropBool( SENDINFO( m_bCloaked ) ),
	SendPropInt( SENDINFO( m_iDamageCustom ) ),
	SendPropInt( SENDINFO( m_iTeam ), 3, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iClass ), 4, SPROP_UNSIGNED ),			
	SendPropUtlVector( SENDINFO_UTLVECTOR( m_hRagWearables ), 8, SendPropEHandle( NULL, 0 ) ),
	SendPropBool( SENDINFO( m_bGoldRagdoll ) ),
	SendPropBool( SENDINFO( m_bIceRagdoll ) ),
	SendPropBool( SENDINFO( m_bCritOnHardHit ) ),
	SendPropFloat( SENDINFO( m_flHeadScale ) ),
	SendPropFloat( SENDINFO( m_flTorsoScale ) ),
	SendPropFloat( SENDINFO( m_flHandScale ) ),
END_SEND_TABLE()

// -------------------------------------------------------------------------------- //
// Tables.
// -------------------------------------------------------------------------------- //

//-----------------------------------------------------------------------------
// Purpose: Filters updates to a variable so that only non-local players see
// the changes.  This is so we can send a low-res origin to non-local players
// while sending a hi-res one to the local player.
// Input  : *pVarData - 
//			*pOut - 
//			objectID - 
//-----------------------------------------------------------------------------

void* SendProxy_SendNonLocalDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID )
{
	pRecipients->SetAllRecipients();
	pRecipients->ClearRecipient( objectID - 1 );
	return ( void * )pVarData;
}
REGISTER_SEND_PROXY_NON_MODIFIED_POINTER( SendProxy_SendNonLocalDataTable );

//-----------------------------------------------------------------------------
// Purpose: SendProxy that converts the UtlVector list of objects to entindexes, where it's reassembled on the client
//-----------------------------------------------------------------------------
void SendProxy_PlayerObjectList( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	CTFPlayer *pPlayer = (CTFPlayer*)pStruct;

	// If this fails, then SendProxyArrayLength_PlayerObjects didn't work.
	Assert( iElement < pPlayer->GetObjectCount() );

	CBaseObject *pObject = pPlayer->GetObject(iElement);

	EHANDLE hObject;
	hObject = pObject;

	SendProxy_EHandleToInt( pProp, pStruct, &hObject, pOut, iElement, objectID );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int SendProxyArrayLength_PlayerObjects( const void *pStruct, int objectID )
{
	CTFPlayer *pPlayer = (CTFPlayer*)pStruct;
	int iObjects = pPlayer->GetObjectCount();
	Assert( iObjects <= MAX_OBJECTS_PER_PLAYER );
	return iObjects;
}

//-----------------------------------------------------------------------------
// Purpose: Send to attached medics
//-----------------------------------------------------------------------------
void* SendProxy_SendHealersDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID )
{
	CTFPlayer *pPlayer = (CTFPlayer*)pStruct;
	if ( pPlayer )
	{
		// Add attached medics
		for ( int i = 0; i < pPlayer->m_Shared.GetNumHealers(); i++ )
		{
			CTFPlayer *pMedic = ToTFPlayer( pPlayer->m_Shared.GetHealerByIndex( i ) );
			if ( !pMedic )
				continue;

			pRecipients->SetRecipient( pMedic->GetClientIndex() );
			return (void*)pVarData;
		}
	}

	return NULL;
}
REGISTER_SEND_PROXY_NON_MODIFIED_POINTER( SendProxy_SendHealersDataTable );

BEGIN_DATADESC( CTFPlayer )
	DEFINE_INPUTFUNC( FIELD_VOID, "IgnitePlayer", InputIgnitePlayer ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetCustomModel", InputSetCustomModel ),
	DEFINE_INPUTFUNC( FIELD_VECTOR, "SetCustomModelOffset", InputSetCustomModelOffset ),
	DEFINE_INPUTFUNC( FIELD_VECTOR, "SetCustomModelRotation", InputSetCustomModelRotation ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ClearCustomModelRotation", InputClearCustomModelRotation ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetCustomModelRotates", InputSetCustomModelRotates ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetCustomModelVisibleToSelf", InputSetCustomModelVisibleToSelf ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetForcedTauntCam", InputSetForcedTauntCam ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ExtinguishPlayer", InputExtinguishPlayer ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "BleedPlayer", InputBleedPlayer ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TriggerLootIslandAchievement", InputTriggerLootIslandAchievement ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TriggerLootIslandAchievement2", InputTriggerLootIslandAchievement2 ),
	DEFINE_INPUTFUNC( FIELD_STRING,	"SpeakResponseConcept",	InputSpeakResponseConcept ),
	DEFINE_INPUTFUNC( FIELD_VOID, "RollRareSpell", InputRollRareSpell ),
	DEFINE_INPUTFUNC( FIELD_VOID, "RoundSpawn", InputRoundSpawn ),
END_DATADESC()

EXTERN_SEND_TABLE( DT_ScriptCreatedItem );

// specific to the local player
BEGIN_SEND_TABLE_NOBASE( CTFPlayer, DT_TFLocalPlayerExclusive )
	// send a hi-res origin to the local player for use in prediction
	SendPropVectorXY(SENDINFO(m_vecOrigin),               -1, SPROP_NOSCALE|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginXY ),
	SendPropFloat   (SENDINFO_VECTORELEM(m_vecOrigin, 2), -1, SPROP_NOSCALE|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginZ ),
	SendPropArray2( 
		SendProxyArrayLength_PlayerObjects,
		SendPropInt("player_object_array_element", 0, SIZEOF_IGNORE, NUM_NETWORKED_EHANDLE_BITS, SPROP_UNSIGNED, SendProxy_PlayerObjectList), 
		MAX_OBJECTS_PER_PLAYER, 
		0, 
		"player_object_array"
		),

	SendPropFloat( SENDINFO_VECTORELEM(m_angEyeAngles, 0), 8, SPROP_CHANGES_OFTEN, -90.0f, 90.0f ),		// No longer used by the local player, could be omitted. Preserved for backwards-compat for now.
//	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 1), 10, SPROP_CHANGES_OFTEN ),

	SendPropBool( SENDINFO( m_bIsCoaching ) ),
	SendPropEHandle( SENDINFO( m_hCoach ) ),
	SendPropEHandle( SENDINFO( m_hStudent ) ),

	SendPropInt( SENDINFO( m_nCurrency ), -1, SPROP_VARINT ),
	SendPropInt( SENDINFO( m_nExperienceLevel ), 7, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nExperienceLevelProgress ), 7, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bMatchSafeToLeave ) ),

END_SEND_TABLE()

// all players except the local player
BEGIN_SEND_TABLE_NOBASE( CTFPlayer, DT_TFNonLocalPlayerExclusive )
	// send a lo-res origin to other players
	SendPropVectorXY(SENDINFO(m_vecOrigin),               -1, SPROP_COORD_MP_LOWPRECISION|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginXY ),
	SendPropFloat   (SENDINFO_VECTORELEM(m_vecOrigin, 2), -1, SPROP_COORD_MP_LOWPRECISION|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginZ ),

	SendPropFloat( SENDINFO_VECTORELEM(m_angEyeAngles, 0), 8, SPROP_CHANGES_OFTEN, -90.0f, 90.0f ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 1), 10, SPROP_CHANGES_OFTEN ),

END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: Sent to attached medics
//-----------------------------------------------------------------------------
BEGIN_SEND_TABLE_NOBASE( CTFPlayer, DT_TFSendHealersDataTable )
	SendPropInt( SENDINFO( m_nActiveWpnClip ), -1, SPROP_VARINT | SPROP_UNSIGNED ),
END_SEND_TABLE()

//============

LINK_ENTITY_TO_CLASS( player, CTFPlayer );
PRECACHE_REGISTER(player);

IMPLEMENT_SERVERCLASS_ST( CTFPlayer, DT_TFPlayer )
	SendPropExclude( "DT_BaseAnimating", "m_flPoseParameter" ),
	SendPropExclude( "DT_BaseAnimating", "m_flPlaybackRate" ),	
	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
	SendPropExclude( "DT_BaseAnimating", "m_nBody" ),
	SendPropExclude( "DT_BaseEntity", "m_angRotation" ),
	SendPropExclude( "DT_BaseAnimatingOverlay", "overlay_vars" ),
	SendPropExclude( "DT_BaseEntity", "m_nModelIndex" ),
	SendPropExclude( "DT_BaseEntity", "m_vecOrigin" ),

	// cs_playeranimstate and clientside animation takes care of these on the client
	SendPropExclude( "DT_ServerAnimationData" , "m_flCycle" ),	
	SendPropExclude( "DT_AnimTimeMustBeFirst" , "m_flAnimTime" ),

	SendPropExclude( "DT_BaseFlex", "m_flexWeight" ),
	SendPropExclude( "DT_BaseFlex", "m_blinktoggle" ),
	SendPropExclude( "DT_BaseFlex", "m_viewtarget" ),

	SendPropBool(SENDINFO(m_bSaveMeParity)),
	SendPropBool(SENDINFO(m_bIsMiniBoss)),
	SendPropBool(SENDINFO(m_bIsABot)),
	SendPropInt( SENDINFO(m_nBotSkill), 3, SPROP_UNSIGNED ),

	// This will create a race condition will the local player, but the data will be the same so.....
	SendPropInt( SENDINFO( m_nWaterLevel ), 2, SPROP_UNSIGNED ),

	// Ragdoll.
	SendPropEHandle( SENDINFO( m_hRagdoll ) ),
	SendPropDataTable( SENDINFO_DT( m_PlayerClass ), &REFERENCE_SEND_TABLE( DT_TFPlayerClassShared ) ),
	SendPropDataTable( SENDINFO_DT( m_Shared ), &REFERENCE_SEND_TABLE( DT_TFPlayerShared ) ),
	SendPropEHandle(SENDINFO(m_hItem)),

	// Data that only gets sent to the local player
	SendPropDataTable( "tflocaldata", 0, &REFERENCE_SEND_TABLE(DT_TFLocalPlayerExclusive), SendProxy_SendLocalDataTable ),

	// Data that gets sent to all other players
	SendPropDataTable( "tfnonlocaldata", 0, &REFERENCE_SEND_TABLE(DT_TFNonLocalPlayerExclusive), SendProxy_SendNonLocalDataTable ),

	SendPropBool( SENDINFO( m_bAllowMoveDuringTaunt ) ),
	SendPropBool( SENDINFO( m_bIsReadyToHighFive ) ),
	SendPropEHandle( SENDINFO( m_hHighFivePartner ) ),
	SendPropInt( SENDINFO( m_nForceTauntCam ), 2, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flTauntYaw ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_nActiveTauntSlot ) ),
	SendPropInt( SENDINFO( m_iTauntItemDefIndex ) ),
	SendPropFloat( SENDINFO( m_flCurrentTauntMoveSpeed ) ),
	SendPropFloat( SENDINFO( m_flVehicleReverseTime ) ),

	SendPropFloat( SENDINFO( m_flLastDamageTime ), 16, SPROP_ROUNDUP ),

	SendPropBool( SENDINFO( m_bInPowerPlay ) ),

	SendPropInt( SENDINFO( m_iSpawnCounter ) ),
	SendPropBool( SENDINFO( m_bArenaSpectator ) ),
	SendPropFloat( SENDINFO( m_flHeadScale ) ),
	SendPropFloat( SENDINFO( m_flTorsoScale ) ),
	SendPropFloat( SENDINFO( m_flHandScale ) ),

	SendPropBool( SENDINFO( m_bUseBossHealthBar ) ),

	SendPropBool( SENDINFO( m_bUsingVRHeadset ) ),

	SendPropBool( SENDINFO( m_bForcedSkin ) ),
	SendPropInt( SENDINFO( m_nForcedSkin ), ANIMATION_SKIN_BITS ),

	SendPropDataTable( SENDINFO_DT( m_AttributeManager ), &REFERENCE_SEND_TABLE(DT_AttributeManager) ),

	SendPropDataTable( "TFSendHealersDataTable", 0, &REFERENCE_SEND_TABLE( DT_TFSendHealersDataTable ), SendProxy_SendHealersDataTable ),

	SendPropFloat( SENDINFO( m_flKartNextAvailableBoost ) ),
	SendPropInt( SENDINFO( m_iKartHealth ) ),
	SendPropInt( SENDINFO( m_iKartState ) ),
	SendPropEHandle( SENDINFO( m_hGrapplingHookTarget ) ),
	SendPropEHandle( SENDINFO( m_hSecondaryLastWeapon ) ),
	SendPropBool( SENDINFO( m_bUsingActionSlot ) ),
	SendPropFloat( SENDINFO( m_flInspectTime ) ),
	SendPropInt( SENDINFO( m_iCampaignMedals ) ),
	SendPropInt( SENDINFO( m_iPlayerSkinOverride ) ),
END_SEND_TABLE()

// -------------------------------------------------------------------------------- //

void cc_CreatePredictionError_f()
{
	CBaseEntity *pEnt = CBaseEntity::Instance( 1 );
	pEnt->SetAbsOrigin( pEnt->GetAbsOrigin() + Vector( 63, 0, 0 ) );
}
ConCommand cc_CreatePredictionError( "CreatePredictionError", cc_CreatePredictionError_f, "Create a prediction error", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

// -------------------------------------------------------------------------------- //

enum eCoachCommand
{
	kCoachCommand_Look = 1, // slot1
	kCoachCommand_Go,		// slot2
	kCoachCommand_Attack,
	kCoachCommand_Defend,
	kNumCoachCommands,
};

/**
 * Handles a command from the coach
 */
static void HandleCoachCommand( CTFPlayer *pPlayer, eCoachCommand command )
{
	if ( pPlayer && pPlayer->IsCoaching() && pPlayer->GetStudent() && command < kNumCoachCommands )
	{
		const float kMaxRateCoachCommands = 1.0f;
		float flLastCoachCommandDelta = gpGlobals->curtime - pPlayer->m_flLastCoachCommand;
		if ( flLastCoachCommandDelta < kMaxRateCoachCommands && flLastCoachCommandDelta > 0.0f )
		{
			return;
		}
		pPlayer->m_flLastCoachCommand = gpGlobals->curtime;
		IGameEvent *pEvent = gameeventmanager->CreateEvent( "show_annotation" );
		if ( pEvent )
		{
			Vector vForward;
 			AngleVectors( pPlayer->EyeAngles(), &vForward );

			trace_t	trace;
			CTraceFilterSimple filter( pPlayer->GetStudent(), COLLISION_GROUP_NONE );
			UTIL_TraceLine( pPlayer->EyePosition(), pPlayer->EyePosition() + vForward * MAX_TRACE_LENGTH, MASK_SOLID, &filter, &trace );

			CBaseEntity *pHitEntity = trace.m_pEnt && trace.m_pEnt->IsWorld() == false && trace.m_pEnt != pPlayer->GetStudent() ? trace.m_pEnt : NULL;
			pEvent->SetInt( "id", pPlayer->entindex() );
			pEvent->SetFloat( "worldPosX", trace.endpos.x );
			pEvent->SetFloat( "worldPosY", trace.endpos.y );
			pEvent->SetFloat( "worldPosZ", trace.endpos.z );
			pEvent->SetFloat( "worldNormalX", trace.plane.normal.x );
			pEvent->SetFloat( "worldNormalY", trace.plane.normal.y );
			pEvent->SetFloat( "worldNormalZ", trace.plane.normal.z );
			pEvent->SetFloat( "lifetime", 10.0f );
			if ( pHitEntity )
			{
				pEvent->SetInt( "follow_entindex", pHitEntity->entindex() );
			}
			pEvent->SetInt( "visibilityBitfield", ( 1 << pPlayer->entindex() | 1 << pPlayer->GetStudent()->entindex() ) );
			pEvent->SetBool( "show_distance", true );
			pEvent->SetBool( "show_effect", true );

			switch ( command )
			{
			case kCoachCommand_Attack:	
				pEvent->SetString( "text", pHitEntity ? "#TF_Coach_AttackThis" : "#TF_Coach_AttackHere" ); 
				pEvent->SetString( "play_sound", "coach/coach_attack_here.wav" );
				break;
			case kCoachCommand_Defend:	
				pEvent->SetString( "text", pHitEntity ? "#TF_Coach_DefendThis" : "#TF_Coach_DefendHere" ); 
				pEvent->SetString( "play_sound", "coach/coach_defend_here.wav" );
				break;
			case kCoachCommand_Look:	
				pEvent->SetString( "text", pHitEntity ? "#TF_Coach_LookAt" : "#TF_Coach_LookHere" ); 
				pEvent->SetString( "play_sound", "coach/coach_look_here.wav" );
				break;
			case kCoachCommand_Go:
				pEvent->SetString( "text", pHitEntity ? "#TF_Coach_GoToThis" : "#TF_Coach_GoHere" ); 
				pEvent->SetString( "play_sound", "coach/coach_go_here.wav" );
				break;
			}
			gameeventmanager->FireEvent( pEvent );
		}

	}
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFPlayer::CTFPlayer() 
{
	m_pAttributes = this;

	m_PlayerAnimState = CreateTFPlayerAnimState( this );

	SetArmorValue( 10 );

	m_hItem = NULL;
	m_hTauntScene = NULL;
	m_hTauntProp = NULL;

	UseClientSideAnimation();
	m_angEyeAngles.Init();
	m_pStateInfo = NULL;
	m_lifeState = LIFE_DEAD; // Start "dead".
	m_iMaxSentryKills = 0;
	m_flLastCoachCommand = 0;

	m_flNextTimeCheck = gpGlobals->curtime;
	m_flSpawnTime = 0;

	m_flWaterExitTime = 0;

	SetViewOffset( TF_PLAYER_VIEW_OFFSET );

	m_Shared.Init( this );

	m_iLastSkin = -1;

	m_bHudClassAutoKill = false;
	m_bMedigunAutoHeal = false;

	m_vecLastDeathPosition = Vector( FLT_MAX, FLT_MAX, FLT_MAX );

	SetDesiredPlayerClassIndex( TF_CLASS_UNDEFINED );

	SetContextThink( &CTFPlayer::TFPlayerThink, gpGlobals->curtime, "TFPlayerThink" );

	m_flLastAction = gpGlobals->curtime;
	m_flTimeInSpawn = 0;

	m_bInitTaunt = false;

	m_bSpeakingConceptAsDisguisedSpy = false;

	m_iPreviousteam = TEAM_UNASSIGNED;
	m_bArenaSpectator = false;

	m_bArenaIsAFK = false;
	m_bIsAFK = false;

	m_nDeployingBombState = TF_BOMB_DEPLOYING_NONE;

	m_flNextChangeClassTime = 0.0f;
	m_flNextChangeTeamTime = 0.0f;

	m_bScattergunJump = false;
	m_iOldStunFlags = 0;
	m_iLastWeaponSlot = 1;
	m_iNumberofDominations = 0;
	m_bFlipViewModels = false;
	m_iBlastJumpState = 0;
	m_flBlastJumpLandTime = 0;
	m_fMaxHealthTime = -1;
	m_iHealthBefore = 0;

	m_iTeamChanges = 0;
	m_iClassChanges = 0;

	m_hReviveMarker = NULL;

	// Bounty Mode
	m_nExperienceLevel = 1;
	m_nExperiencePoints = 0;
	m_nExperienceLevelProgress = 0;

	SetDefLessFunc( m_Cappers );		// Tracks victims for demo achievement

 	//=============================================================================
	// HPE_BEGIN:
	// [msmith]	Added a player type so we can distinguish between bots and humans.
	//============================================================================= 
	m_playerType = HUMAN_PLAYER;
	//=============================================================================
	// HPE_END
	//=============================================================================

	m_bIsTargetDummy = false;
	
	m_bCollideWithSentry = false;

	m_flCommentOnCarrying = 0;

	m_bIsReadyToHighFive = false;
	m_hHighFivePartner = NULL;
	m_nForceTauntCam = 0;
	m_bAllowMoveDuringTaunt = false;
	m_bTauntForceMoveForward = false;
	m_flTauntForceMoveForwardSpeed = 0.f;
	m_flTauntMoveAccelerationTime = 0.f;
	m_flTauntTurnSpeed = 0.f;
	m_flTauntTurnAccelerationTime = 0.f;
	m_bTauntMimic = false;
	m_bIsTauntInitiator = false;
	m_TauntEconItemView.Invalidate();
	m_iPreTauntWeaponSlot = -1;

	m_bIsCalculatingMaximumSpeed = false;

	m_flLastThinkTime = -1.f;

	m_nCurrency = 0;
	m_pWaveSpawnPopulator = NULL;
	m_flLastReadySoundTime = 0.f;

	m_damageRateArray = new int[ DPS_Period ];
	ResetDamagePerSecond();

	m_nActiveWpnClip.Set( 0 );
	m_nActiveWpnClipPrev = 0;
	m_flNextClipSendTime = 0;

	m_nCanPurchaseUpgradesCount = 0;

	m_flHeadScale = 1.f;
	m_flTorsoScale = 1.f;
	m_flHandScale = 1.f;

	m_bPendingMerasmusPlayerBombExplode = false;
	m_fLastBombHeadTimestamp = 0.0f;

	m_bIsSapping = false;
	m_iSappingEvent = TF_SAPEVENT_NONE;
	m_flSapStartTime = 0.00;

	m_bIsMiniBoss = false;

	m_bUseBossHealthBar = false;

	m_bUsingVRHeadset = false;

	m_bForcedSkin = false;
	m_nForcedSkin = 0;

	SetRespawnOverride( -1.f, NULL_STRING );

	m_qPreviousChargeEyeAngle.Init();

	m_vHalloweenKartPush.Zero();
	m_flHalloweenKartPushEventTime = 0.f;
	m_bCheckKartCollision = false;
	m_flHHHKartAttackTime = 0.f;
	m_flNextBonusDucksVOAllowedTime = 0.f;

	m_flGhostLastHitByKartTime = 0.f;

	m_flVehicleReverseTime = FLT_MAX;
	m_iCampaignMedals = 0;

	m_bPasstimeBallSlippery = false;
	m_flNextScorePointForPD = -1;

	m_iPlayerSkinOverride = 0;

	m_nPrevRoundTeamNum = TEAM_UNASSIGNED;
	m_flLastDamageResistSoundTime = -1.f;
	m_hLastDamageDoneEntity = NULL;
	
	m_mapCustomAttributes.SetLessFunc( UtlStringCaseInsensitiveLessFunc );

	SetDefLessFunc( m_PlayersExtinguished );

	m_flLastAutobalanceTime = 0.f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ForcePlayerViewAngles( const QAngle& qTeleportAngles )
{
	CSingleUserRecipientFilter filter( this );

	UserMessageBegin( filter, "ForcePlayerViewAngles" );
	WRITE_BYTE( 0x01 ); // Reserved space for flags.
	WRITE_BYTE( entindex() );
	WRITE_ANGLES( qTeleportAngles );
	MessageEnd();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::SetGrapplingHookTarget( CBaseEntity *pTarget, bool bShouldBleed /*= false*/ )
{
	if ( pTarget )
	{
		// prevent fall damage after a successful hook
		m_Shared.AddCond( TF_COND_GRAPPLINGHOOK_SAFEFALL );
		m_Shared.AddCond( TF_COND_GRAPPLINGHOOK_LATCHED );
	}
	else
	{
		m_Shared.RemoveCond( TF_COND_GRAPPLINGHOOK_LATCHED );
	}

	CBaseEntity *pPreviousTarget = m_hGrapplingHookTarget;
	m_hGrapplingHookTarget = pTarget;
	
	if ( pTarget )
	{
		if ( pTarget->IsPlayer() )
		{
			CTFPlayer *pTargetPlayer = ToTFPlayer( pTarget );

			m_Shared.AddCond( TF_COND_GRAPPLED_TO_PLAYER );

			// make player bleed
			if ( bShouldBleed )
			{
				CTFGrapplingHook *pGrapplingHook = dynamic_cast< CTFGrapplingHook* >( GetEntityForLoadoutSlot( LOADOUT_POSITION_ACTION ) );
				if ( pGrapplingHook )
					pTargetPlayer->m_Shared.MakeBleed( this, pGrapplingHook, 0, TF_BLEEDING_DMG, true );

				pTargetPlayer->m_nHookAttachedPlayers++;
			}

			if ( !pTargetPlayer->m_Shared.InCond( TF_COND_GRAPPLINGHOOK_BLEEDING ) && pTargetPlayer->m_nHookAttachedPlayers > 0 )
			{
				pTargetPlayer->m_Shared.AddCond( TF_COND_GRAPPLINGHOOK_BLEEDING );
			}
			if ( !pTargetPlayer->m_Shared.InCond( TF_COND_GRAPPLED_BY_PLAYER ) && pTargetPlayer->m_nHookAttachedPlayers > 0 )
			{
				pTargetPlayer->m_Shared.AddCond( TF_COND_GRAPPLED_BY_PLAYER );
			}
		}

		m_flLastSeenHookTarget = gpGlobals->curtime;
	}
	else
	{
		if ( pPreviousTarget && pPreviousTarget->IsPlayer() )
		{
			CTFPlayer *pPreviousTargetPlayer = ToTFPlayer( pPreviousTarget );

			m_Shared.RemoveCond( TF_COND_GRAPPLED_TO_PLAYER );
			
			// try to remove bleeding from hook if there's one
			if ( pPreviousTargetPlayer->m_Shared.InCond( TF_COND_BLEEDING ) )
			{
				CTFGrapplingHook *pGrapplingHook = dynamic_cast< CTFGrapplingHook* >( GetEntityForLoadoutSlot( LOADOUT_POSITION_ACTION ) );
				if ( pGrapplingHook )
					pPreviousTargetPlayer->m_Shared.StopBleed( this, pGrapplingHook );
			}

			pPreviousTargetPlayer->m_nHookAttachedPlayers--;
			Assert( pPreviousTargetPlayer->m_nHookAttachedPlayers >= 0 );
			if ( pPreviousTargetPlayer->m_nHookAttachedPlayers == 0 )
			{
				pPreviousTargetPlayer->m_Shared.RemoveCond( TF_COND_GRAPPLINGHOOK_BLEEDING );
				pPreviousTargetPlayer->m_Shared.RemoveCond( TF_COND_GRAPPLED_BY_PLAYER );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::CanBeForcedToLaugh( void )
{
	if ( TFGameRules() && TFGameRules()->IsMannVsMachineMode() && IsBot() && ( GetTeamNumber() == TF_TEAM_PVE_INVADERS ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::TFPlayerThink()
{
	if ( m_pStateInfo && m_pStateInfo->pfnThink )
	{
		(this->*m_pStateInfo->pfnThink)();
	}

	if ( m_flSendPickupWeaponMessageTime != -1.f && gpGlobals->curtime >= m_flSendPickupWeaponMessageTime )
	{
		CSingleUserRecipientFilter filter( this );
		filter.MakeReliable();
		UserMessageBegin( filter, "PlayerPickupWeapon" );
		MessageEnd();

		m_flSendPickupWeaponMessageTime = -1.f;
	}

	// In doomsday event, kart can run over ghost to do stuff
	if ( m_Shared.InCond( TF_COND_HALLOWEEN_KART ) )
	{
		CUtlVector< CTFPlayer * > playerVector;
		CollectPlayers( &playerVector, TEAM_ANY, true );
		CUtlVector< CTFPlayer * > ghostVector;
		for ( int i=0; i<playerVector.Count(); ++i )
		{
			if ( playerVector[i] == this )
				continue;

			// touching ghost player?
			// we just check for radius of 100 and assume that we touch to avoid custom collision for ghost
			if ( playerVector[i]->m_Shared.InCond( TF_COND_HALLOWEEN_GHOST_MODE ) )
			{
				if ( ( playerVector[i]->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr() < Square( 100 ) )
				{
					ghostVector.AddToTail( playerVector[i] );
				}
			}
		}

		for ( int i=0; i<ghostVector.Count(); ++i )
		{
			CTFPlayer *pGhost = ghostVector[i];
			
			// revive ghost on the same team
			if ( pGhost->GetTeamNumber() == GetTeamNumber() )
			{
				// Trace the ghosts bbox right where they are to see if they collide with enemy players
				trace_t trace;
				Ray_t ray;
				ray.Init( pGhost->GetAbsOrigin(), pGhost->GetAbsOrigin(), pGhost->GetPlayerMins(), pGhost->GetPlayerMaxs() );
				UTIL_TraceRay( ray, PlayerSolidMask(), pGhost, COLLISION_GROUP_PLAYER, &trace );

				// If our trace is clear, spawn that ghost
				if ( trace.fraction == 1.0f )
				{
					// Force the players kart angles to line up with our current ghost angles.
					// This should put us in the kart at the same direction we are currently looking.
					pGhost->ForcePlayerViewAngles( pGhost->GetAbsAngles() );

					pGhost->m_Shared.RemoveCond( TF_COND_HALLOWEEN_GHOST_MODE );
					pGhost->m_Shared.AddCond( TF_COND_HALLOWEEN_KART );
					pGhost->m_Shared.AddCond( TF_COND_HALLOWEEN_IN_HELL ); // keep you in hell to be able to respawn as ghost
					pGhost->m_Shared.AddCond( TF_COND_HALLOWEEN_QUICK_HEAL, 3, this );
					pGhost->EmitSound( "BumperCar.SpawnFromLava" );
					DispatchParticleEffect( "ghost_appearation", PATTACH_ABSORIGIN, pGhost );

					if ( TFGameRules()->IsHalloweenScenario( CTFGameRules::HALLOWEEN_SCENARIO_DOOMSDAY ) )
					{
						// achievement for me!
						AwardAchievement( ACHIEVEMENT_TF_HALLOWEEN_DOOMSDAY_RESPAWN_TEAMMATES );

						IGameEvent *pEvent = gameeventmanager->CreateEvent( "respawn_ghost" );
						if ( pEvent )
						{
							pEvent->SetInt( "reviver", GetUserID() );
							pEvent->SetInt( "ghost", pGhost->GetUserID() );
							gameeventmanager->FireEvent( pEvent, true );
						}
					}
				}
			}
			else if ( tf_halloween_allow_ghost_hit_by_kart_delay.GetFloat() > 0 && gpGlobals->curtime - pGhost->m_flGhostLastHitByKartTime > tf_halloween_allow_ghost_hit_by_kart_delay.GetFloat() )
			{
				// punt off other team ghost
				float flImpactForce = GetLocalVelocity().Length();
				flImpactForce = MAX( 100.f, flImpactForce ); // add min force
				Vector vOffset = pGhost->WorldSpaceCenter() - WorldSpaceCenter();
				vOffset.z = 0;
				Vector vPuntDir = ( vOffset ).Normalized();
				vPuntDir.z = 0.5f;
				pGhost->ApplyAirBlastImpulse( tf_halloween_kart_punting_ghost_force_scale.GetFloat() * flImpactForce * vPuntDir );
				pGhost->EmitSound( "BumperCar.HitGhost" );
				pGhost->m_flGhostLastHitByKartTime = gpGlobals->curtime;
			}
		}
	}

	if ( TFGameRules() && TFGameRules()->IsUsingGrapplingHook() )
	{
		if ( IsUsingActionSlot() && GetActiveTFWeapon() && GetActiveTFWeapon()->GetWeaponID() != TF_WEAPON_GRAPPLINGHOOK )
		{
			CTFGrapplingHook *pGrapplingHook = dynamic_cast< CTFGrapplingHook* >( GetEntityForLoadoutSlot( LOADOUT_POSITION_ACTION ) );
			if ( pGrapplingHook )
			{
				Weapon_Switch( pGrapplingHook );
			}
		}

		CBaseEntity *pHookTarget = GetGrapplingHookTarget();
		if ( pHookTarget )
		{
			// detatch hook if the object's picked up
			if ( pHookTarget->IsBaseObject() )
			{
				CBaseObject *pObj = assert_cast< CBaseObject* >( pHookTarget );
				if ( pObj->IsCarried() )
				{
					SetGrapplingHookTarget( NULL );
					pHookTarget = NULL;
				}
			}

			// check if something is blocking the player from traveling to the hook target
			if ( pHookTarget )
			{
				trace_t tr;
				CTraceFilterLOS filter( this, COLLISION_GROUP_PLAYER_MOVEMENT, pHookTarget );
				UTIL_TraceLine( WorldSpaceCenter(), pHookTarget->WorldSpaceCenter(), MASK_PLAYERSOLID, &filter, &tr );
				if ( !tr.DidHit() )
				{
					m_flLastSeenHookTarget = gpGlobals->curtime;
				}
				else if ( gpGlobals->curtime - m_flLastSeenHookTarget > tf_grapplinghook_los_force_detach_time.GetFloat() )
				{
					// force to detach if the hooker lost sight of the target for sometime
					SetGrapplingHookTarget( NULL );
				}
			}
		}
	}

	UpdateCustomAttributes();

	// Time to finish the current random expression? Or time to pick a new one?
	if ( IsAlive() && !IsReadyToTauntWithPartner() && ( m_flNextSpeakWeaponFire < gpGlobals->curtime ) && m_flNextRandomExpressionTime >= 0 && gpGlobals->curtime > m_flNextRandomExpressionTime )
	{
		// Random expressions need to be cleared, because they don't loop. So if we
		// pick the same one again, we want to restart it.
		ClearExpression();
		m_iszExpressionScene = NULL_STRING;
		UpdateExpression();
	}

	if ( IsTaunting() )
	{
		if ( !m_strTauntSoundName.IsEmpty() && m_flTauntSoundTime > 0 && m_flTauntSoundTime <= gpGlobals->curtime )
		{
			EmitSound( m_strTauntSoundName.String() );
			m_flTauntSoundTime = 0.f;
		}

		if ( !m_strTauntSoundLoopName.IsEmpty() && m_flTauntSoundLoopTime > 0 && m_flTauntSoundLoopTime <= gpGlobals->curtime )
		{
			CReliableBroadcastRecipientFilter filter;
			UserMessageBegin( filter, "PlayerTauntSoundLoopStart" );
				WRITE_BYTE( entindex() );
				WRITE_STRING( m_strTauntSoundLoopName.String() );
			MessageEnd();

			m_flTauntSoundLoopTime = 0.f;
		}
		
		// play taunt outro
		if ( m_flTauntOutroTime > 0.f && m_flTauntOutroTime <= gpGlobals->curtime )
		{
			m_bAllowedToRemoveTaunt = true;
			float flDuration = PlayTauntOutroScene();
			m_flTauntRemoveTime = gpGlobals->curtime + flDuration;
			m_flTauntOutroTime = 0.f;
		}
	}

	// Halloween Hacks
	// Spell Casting on Attack1
	if ( m_Shared.InCond( TF_COND_HALLOWEEN_KART ) )
	{
		// Check if this is the spellbook so we can save off info to preserve weapon switching
		CTFSpellBook *pSpellBook = dynamic_cast<CTFSpellBook*>( GetEntityForLoadoutSlot( LOADOUT_POSITION_ACTION ) );
		if ( pSpellBook )
		{
			// cast Spell
			if ( m_nButtons & IN_ATTACK )
			{
				if ( pSpellBook )
				{
					pSpellBook->PrimaryAttack();
				}
			}
		}

		// Speed Boost
		if ( m_nButtons & IN_ATTACK2 )
		{
			if ( GetKartSpeedBoost() >= 1.0f )
			{
				m_flKartNextAvailableBoost = gpGlobals->curtime + tf_halloween_kart_boost_recharge.GetFloat();
				m_Shared.AddCond( TF_COND_HALLOWEEN_KART_DASH, tf_halloween_kart_boost_duration.GetFloat() );
			}
		}
	}

	CBaseEntity *pGroundEntity = GetGroundEntity();

	// We consider players "in air" if they have no ground entity and they're not in water.
	if ( pGroundEntity == NULL && GetWaterLevel() == WL_NotInWater )
	{
		if ( m_iLeftGroundHealth < 0 )
		{
			m_iLeftGroundHealth = GetHealth();
		}
	}
	else
	{
		m_iLeftGroundHealth = -1;
		if ( GetFlags() & FL_ONGROUND )
		{
			m_Shared.RemoveCond( TF_COND_KNOCKED_INTO_AIR );
		}

		if ( m_iBlastJumpState )
		{
			const char *pszEvent = NULL;

			if ( StickyJumped() )
			{
				pszEvent = "sticky_jump_landed";
			}
			else if ( RocketJumped() )
			{
				pszEvent = "rocket_jump_landed";
			}

			ClearBlastJumpState();

			if ( pszEvent )
			{
				IGameEvent * event = gameeventmanager->CreateEvent( pszEvent );
				if ( event )
				{
					event->SetInt( "userid", GetUserID() );
					gameeventmanager->FireEvent( event );
				}
			}
		}
	}

	if( IsTaunting() )
	{
		bool bStopTaunt = false;
		// if I'm not supposed to move during taunt
		// stop taunting if I lost my ground entity or was moved at all
		if ( !CanMoveDuringTaunt() )
		{
			bStopTaunt |= pGroundEntity == NULL;
			
			if ( m_TauntEconItemView.IsValid() && m_TauntEconItemView.GetStaticData()->GetTauntData()->ShouldStopTauntIfMoved() )
				bStopTaunt |= m_vecTauntStartPosition.DistToSqr( GetAbsOrigin() ) > 0.1f;
		}

		if ( !bStopTaunt  )
		{
			bStopTaunt |= ShouldStopTaunting();
		}

		if ( bStopTaunt )
		{
			CancelTaunt();
		}
	}

	if ( ( RocketJumped() || StickyJumped() ) && IsAlive() && m_bCreatedRocketJumpParticles == false )
	{
		const char *pEffectName = "rocketjump_smoke";
		DispatchParticleEffect( pEffectName, PATTACH_POINT_FOLLOW, this, "foot_L" );
		DispatchParticleEffect( pEffectName, PATTACH_POINT_FOLLOW, this, "foot_R" );
		m_bCreatedRocketJumpParticles = true;
	}

	if ( !m_bCollideWithSentry )
	{
		if ( IsPlayerClass( TF_CLASS_ENGINEER ) )
		{
			CBaseObject	*pSentry = GetObjectOfType( OBJ_SENTRYGUN );
			if ( !pSentry )
			{
				m_bCollideWithSentry = true;
			}
			else
			{
				if ( ( pSentry->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr() > 2500 )
				{
					m_bCollideWithSentry = true;
				}
			}
		}
		else
		{
			m_bCollideWithSentry = true;
		}
	}

	if ( gpGlobals->curtime > m_flCommentOnCarrying && (m_flCommentOnCarrying != 0.f) )
	{
		m_flCommentOnCarrying = 0.f;

		CBaseObject* pObj = m_Shared.GetCarriedObject();
		if ( pObj )
		{
			SpeakConceptIfAllowed( MP_CONCEPT_CARRYING_BUILDING, pObj->GetResponseRulesModifier() );
		}
	}

#ifdef TF_RAID_MODE
	CTFNavArea *area = (CTFNavArea *)GetLastKnownArea();
	if ( area && area->HasAttributeTF( TF_NAV_RESCUE_CLOSET ) )
	{
		// we're standing in a rescue closet and need a friend to let us out - call for help!
		SpeakConceptIfAllowed( MP_CONCEPT_PLAYER_HELP );
	}
#endif

	// Wrenchmotron taunt effect
	if ( m_bIsTeleportingUsingEurekaEffect )
	{
		if ( m_teleportHomeFlashTimer.HasStarted() && m_teleportHomeFlashTimer.IsElapsed() )
		{
			m_teleportHomeFlashTimer.Invalidate();

			if ( !tf_test_teleport_home_fx.GetBool() )
			{
				// cover up the end of the taunt with a flash
				color32 colorHit = { 255, 255, 255, 255 };
				UTIL_ScreenFade( this, colorHit, 0.25f, 0.25f, FFADE_IN );
			}

			Vector origin = GetAbsOrigin();
			CPVSFilter filter( origin );

			UserMessageBegin( filter, "PlayerTeleportHomeEffect" );
				WRITE_BYTE( entindex() );
			MessageEnd();

			// DispatchParticleEffect( "drg_wrenchmotron_teleport", PATTACH_ABSORIGIN );

			switch( GetTeamNumber() )
			{
			case TF_TEAM_RED:
				TE_TFParticleEffect( filter, 0.0, "teleported_red", origin, vec3_angle );
				TE_TFParticleEffect( filter, 0.0, "player_sparkles_red", origin, vec3_angle, this, PATTACH_POINT );
				break;
			case TF_TEAM_BLUE:
				TE_TFParticleEffect( filter, 0.0, "teleported_blue", origin, vec3_angle );
				TE_TFParticleEffect( filter, 0.0, "player_sparkles_blue", origin, vec3_angle, this, PATTACH_POINT );
				break;
			default:
				break;
			}
		}

		// teleport home when taunt finishes
		if ( !IsTaunting() )
		{
			// drop the intel and any powerup we are carrying
			DropFlag();
			DropRune();

			EmitSound( "Building_Teleporter.Send" );
			m_bIsTeleportingUsingEurekaEffect = false;

			CObjectTeleporter* pTeleExit = assert_cast< CObjectTeleporter* >( GetObjectOfType( OBJ_TELEPORTER, MODE_TELEPORTER_EXIT ) );
			 
			// Check if they wanted to go to their teleporter AND their teleporter can accept them
			if ( m_eEurekaTeleportTarget == EUREKA_TELEPORT_TELEPORTER_EXIT && pTeleExit && ( pTeleExit->GetState() != TELEPORTER_STATE_BUILDING ) )
			{
				pTeleExit->RecieveTeleportingPlayer( this );
			}
			else
			{
				// Default to the spawn
				TFGameRules()->GetPlayerSpawnSpot( this );
			}
		}
	}

	// Send active weapon's clip state to attached medics
	bool bSendClipInfo = gpGlobals->curtime > m_flNextClipSendTime &&
						 m_Shared.GetNumHealers() &&
						 IsAlive();
	if ( bSendClipInfo )
	{
		CTFWeaponBase *pTFWeapon = GetActiveTFWeapon();
		if ( pTFWeapon )
		{
			int nClip = 0;

			if ( m_Shared.InCond( TF_COND_DISGUISED ) )
			{
				nClip = m_Shared.GetDisguiseAmmoCount();
			}
			else
			{
				nClip = pTFWeapon->UsesClipsForAmmo1() ? pTFWeapon->Clip1() : GetAmmoCount( pTFWeapon->GetPrimaryAmmoType() );
			}

			if ( nClip >= 0 && nClip != m_nActiveWpnClipPrev )
			{
				if ( nClip > 500 )
				{
					Warning( "Heal Target: ClipSize Data Limit Exceeded: %d (max 500)\n", nClip );
					nClip = MIN( nClip, 500 );
				}
				m_nActiveWpnClip.Set( nClip );
				m_nActiveWpnClipPrev = m_nActiveWpnClip;
				m_flNextClipSendTime = gpGlobals->curtime + 0.25f;
			}
		}
	}

	if ( GetPlayerClass()->GetClassIndex() == TF_CLASS_SPY && ( GetFlags() & FL_DUCKING ) && ( pGroundEntity != NULL ) )
	{
		int nDisguiseAsDispenserOnCrouch = 0;
		CALL_ATTRIB_HOOK_FLOAT( nDisguiseAsDispenserOnCrouch, disguise_as_dispenser_on_crouch );
		if ( nDisguiseAsDispenserOnCrouch != 0 )
		{
			m_Shared.AddCond( TF_COND_DISGUISED_AS_DISPENSER, 0.5f );
		}
	}

	// rune charge over time
	if ( m_Shared.CanRuneCharge() && !m_Shared.IsRuneCharged() )
	{
		float dt = gpGlobals->curtime - m_flLastRuneChargeUpdate;
		float flAdd = dt * 100.f / tf_powerup_max_charge_time.GetFloat();
		m_Shared.SetRuneCharge( m_Shared.GetRuneCharge() + flAdd );

		if (m_Shared.GetCarryingRuneType() == RUNE_SUPERNOVA && m_Shared.IsRuneCharged() )
		{
			ClientPrint( this, HUD_PRINTCENTER, "#TF_Powerup_Supernova_Deploy" );
		}
	}
	m_flLastRuneChargeUpdate = gpGlobals->curtime;

	// You can't touch a hooked target, so transmit plague when you get as close as you can
	if ( GetGrapplingHookTarget() && GetGrapplingHookTarget()->IsPlayer() && m_Shared.GetCarryingRuneType() == RUNE_PLAGUE )
	{
		CTFPlayer *pHookedPlayer = ToTFPlayer( GetGrapplingHookTarget() );

		float flDistSqrToTarget = GetAbsOrigin().DistToSqr( pHookedPlayer->GetAbsOrigin() );
		if ( flDistSqrToTarget < 8100 && !pHookedPlayer->m_Shared.InCond( TF_COND_PLAGUE ) &&
			!m_Shared.IsAlly( pHookedPlayer ) &&
			!pHookedPlayer->m_Shared.IsInvulnerable() && 
			pHookedPlayer->m_Shared.GetCarryingRuneType() != RUNE_RESIST )
		{
			pHookedPlayer->m_Shared.AddCond( TF_COND_PLAGUE, PERMANENT_CONDITION, this );
		}
	}

	if ( TFGameRules() && TFGameRules()->IsMannVsMachineMode() )
	{
		// prevents player from standing on bot's head to block its movement.
		if ( pGroundEntity && pGroundEntity->IsPlayer() )
		{
			Vector vPush = GetAbsOrigin() - pGroundEntity->GetAbsOrigin();
			vPush.z = 0.f;
			vPush.NormalizeInPlace();
			vPush.z = 1.f;
			vPush *= 100.f;

			ApplyAbsVelocityImpulse( vPush );
		}
	}

	// Scale our head
	m_flHeadScale = Approach( GetDesiredHeadScale(), m_flHeadScale, GetHeadScaleSpeed() );

	// scale our torso
	m_flTorsoScale = Approach( GetDesiredTorsoScale(), m_flTorsoScale, GetTorsoScaleSpeed() );

	// scale our torso
	m_flHandScale = Approach( GetDesiredHandScale(), m_flHandScale, GetHandScaleSpeed() );

/*
#ifdef STAGING_ONLY
	if ( m_Shared.InCond( TF_COND_SPACE_GRAVITY ) )
	{
		// JetPack testing
		if ( m_nButtons & IN_JUMP && !( GetFlags() & FL_ONGROUND ) && m_Shared.GetSpaceJumpChargeMeter() > tf_space_thrust_use_rate.GetFloat() )
		{
			//mv->m_vecVelocity[2] += 10.0f;
			Vector vThrust = Vector(0,0,0);
			switch( GetPlayerClass()->GetClassIndex() )
			{
			case TF_CLASS_SCOUT :				vThrust.z = tf_space_thrust_scout.GetFloat();				break;
			case TF_CLASS_SNIPER :				vThrust.z = tf_space_thrust_sniper.GetFloat();				break;
			case TF_CLASS_SOLDIER :				vThrust.z = tf_space_thrust_soldier.GetFloat();				break;
			case TF_CLASS_DEMOMAN :				vThrust.z = tf_space_thrust_demo.GetFloat();				break;
			case TF_CLASS_MEDIC :				vThrust.z = tf_space_thrust_medic.GetFloat();				break;
			case TF_CLASS_HEAVYWEAPONS :		vThrust.z = tf_space_thrust_heavy.GetFloat();				break;
			case TF_CLASS_PYRO :				vThrust.z = tf_space_thrust_pyro.GetFloat();				break;
			case TF_CLASS_SPY :					vThrust.z = tf_space_thrust_spy.GetFloat();				break;
			case TF_CLASS_ENGINEER :			vThrust.z = tf_space_thrust_engy.GetFloat();				break;
			}

			ApplyAbsVelocityImpulse( vThrust );

			m_Shared.SetSpaceJumpChargeMeter( m_Shared.GetSpaceJumpChargeMeter() - tf_space_thrust_use_rate.GetFloat() );
		}
		else
		{
			if (  GetFlags() & FL_ONGROUND )
			{
				m_Shared.SetSpaceJumpChargeMeter( m_Shared.GetSpaceJumpChargeMeter() + tf_space_thrust_recharge_rate.GetFloat() );
			}
		}
	}
#endif
*/

	SetContextThink( &CTFPlayer::TFPlayerThink, gpGlobals->curtime, "TFPlayerThink" );
	m_flLastThinkTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a portion of health every think.
//-----------------------------------------------------------------------------
void CTFPlayer::RegenThink( void )
{
	if ( !IsAlive() )
		return;

	// Queue the next think
	SetContextThink( &CTFPlayer::RegenThink, gpGlobals->curtime + TF_REGEN_TIME, "RegenThink" );

	// if we're going in to this too often, quit out.
	if ( m_flLastHealthRegenAt + TF_REGEN_TIME > gpGlobals->curtime )
		return;

	bool bShowRegen = true;

	// Medic has a base regen amount
	if ( GetPlayerClass()->GetClassIndex() == TF_CLASS_MEDIC )
	{
		// Heal faster if we haven't been in combat for a while.
		float flTimeSinceDamage = gpGlobals->curtime - GetLastDamageReceivedTime();
		float flScale = RemapValClamped( flTimeSinceDamage, 5.0f, 10.0f, 1.0f, 2.0f );
		float flRegenAmt = TF_REGEN_AMOUNT;

		// If you are healing a hurt patient, increase your base regen
		CTFPlayer *pPatient = ToTFPlayer( MedicGetHealTarget() );
		if ( pPatient && pPatient->GetHealth() < pPatient->GetMaxHealth() )
		{
			// Double regen amount
			flRegenAmt += TF_REGEN_AMOUNT;
		}

		flRegenAmt *= flScale;

		// If the medic has this attribute, increase their regen.
		if ( TFGameRules() && TFGameRules()->GameModeUsesUpgrades() )
		{
			int iHealingMastery = 0;
			CALL_ATTRIB_HOOK_INT( iHealingMastery, healing_mastery );
			if ( iHealingMastery )
			{
				float flPerc = RemapValClamped( (float)iHealingMastery, 1.f, 4.f, 1.25f, 2.f );
				flRegenAmt *= flPerc;
			}
		}

		m_flAccumulatedHealthRegen += flRegenAmt;

		bShowRegen = false;
	}

	// Other classes can be regenerated by items
	float flRegenAmount = 0;
	CALL_ATTRIB_HOOK_FLOAT( flRegenAmount, add_health_regen );
	if ( flRegenAmount )
	{
		float flTimeSinceDamage = gpGlobals->curtime - GetLastDamageReceivedTime();
		float flScale = 1.0f;
		// Ignore Scale for MvM, always give full regen
		if ( TFGameRules() && TFGameRules()->IsMannVsMachineMode() )
		{
			flScale = 1.0f;
		}
		else if ( flTimeSinceDamage < 5.0f )
		{
			flScale = 0.25f;
		}
		else
		{
			flScale = RemapValClamped( flTimeSinceDamage, 5.0f, 10.0f, 0.5f, 1.0f );
		}
		
		flRegenAmount *= flScale;
	}
	m_flAccumulatedHealthRegen += flRegenAmount;

// 	if ( m_Shared.InCond( TF_COND_HEALING_DEBUFF ) )
// 	{
// 		m_flAccumulatedHealthRegen *= 0.75f;
// 	}

	int nHealAmount = 0;
	if ( m_flAccumulatedHealthRegen >= 1.f )
	{
		nHealAmount = floor( m_flAccumulatedHealthRegen );
		if ( GetHealth() < GetMaxHealth() )
		{
			int nHealedAmount = TakeHealth( nHealAmount, DMG_GENERIC | DMG_IGNORE_DEBUFFS );
			if ( nHealedAmount > 0 )
			{
				IGameEvent *event = gameeventmanager->CreateEvent( "player_healed" );
				if ( event )
				{
					event->SetInt( "priority", 1 );	// HLTV event priority
					event->SetInt( "patient", GetUserID() );
					event->SetInt( "healer", GetUserID() );
					event->SetInt( "amount", nHealedAmount );
					gameeventmanager->FireEvent( event );
				}
			}
		}
	}
	else if ( m_flAccumulatedHealthRegen < -1.f )
	{
		nHealAmount = ceil( m_flAccumulatedHealthRegen );
		TakeDamage( CTakeDamageInfo( this, this, NULL, vec3_origin, WorldSpaceCenter(), nHealAmount * -1, DMG_GENERIC ) );
	}

	if ( GetHealth() < GetMaxHealth() && nHealAmount != 0 && bShowRegen )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "player_healonhit" );
		if ( event )
		{
			event->SetInt( "amount", nHealAmount );
			event->SetInt( "entindex", entindex() );
			event->SetInt( "weapon_def_index", INVALID_ITEM_DEF_INDEX );
			gameeventmanager->FireEvent( event ); 
		}
	}

	m_flAccumulatedHealthRegen -= nHealAmount;
	m_flLastHealthRegenAt = gpGlobals->curtime;

	// Regenerate ammo 
	if ( m_flNextAmmoRegenAt < gpGlobals->curtime )
	{
		// We regen ammo every 5 seconds
		m_flNextAmmoRegenAt = gpGlobals->curtime + 5.0;

		flRegenAmount = 0;
		CALL_ATTRIB_HOOK_FLOAT( flRegenAmount, addperc_ammo_regen );

		if ( flRegenAmount )
		{
			RegenAmmoInternal( TF_AMMO_PRIMARY, flRegenAmount );
			RegenAmmoInternal( TF_AMMO_SECONDARY, flRegenAmount );
		}

		// Regenerate metal
		int iMetal = 0;
		CALL_ATTRIB_HOOK_INT( iMetal, add_metal_regen );

		if ( iMetal )
		{
			GiveAmmo( iMetal, TF_AMMO_METAL, true );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns a portion of health every think.
//-----------------------------------------------------------------------------
void CTFPlayer::RuneRegenThink( void )
{
	if ( !IsAlive() )
		return;

	// Queue the next think
	SetContextThink( &CTFPlayer::RuneRegenThink, gpGlobals->curtime + TF_REGEN_TIME_RUNE, "RuneRegenThink" );

	// if we're going in to this too often, quit out.
	if ( m_flLastRuneHealthRegenAt + TF_REGEN_TIME_RUNE > gpGlobals->curtime )
		return;

	int nRuneType = m_Shared.GetCarryingRuneType();
	if ( nRuneType == RUNE_NONE && !HasTheFlag() )
		return;

	// Regenerate health 
	float flAmount = 0.f;
	switch ( GetPlayerClass()->GetClassIndex() )
	{
	case TF_CLASS_SCOUT:
	case TF_CLASS_SPY:
		flAmount = 16;
		break;
	case TF_CLASS_SNIPER:
	case TF_CLASS_ENGINEER:
		flAmount = 14;
		break;
	case TF_CLASS_MEDIC:
	case TF_CLASS_DEMOMAN:
	case TF_CLASS_PYRO:
		flAmount = 12;
		break;
	case TF_CLASS_SOLDIER:
		flAmount = 10;
		break;
	case TF_CLASS_HEAVYWEAPONS:
		flAmount = 8;
		break;
	}
	if ( nRuneType == RUNE_REGEN )
	{
		m_flAccumulatedRuneHealthRegen += flAmount;
	}
	// King and buffed team mates get some health regeneration unless they have the plague
	else if ( ( nRuneType == RUNE_KING || m_Shared.InCond( TF_COND_KING_BUFFED ) ) && !m_Shared.InCond( TF_COND_PLAGUE ) )
	{
		flAmount *= 0.3;
		m_flAccumulatedRuneHealthRegen += flAmount;
	}
	// non powered up flag carriers get a small health regeneration
	else if ( HasTheFlag() && nRuneType == RUNE_NONE )
	{
		flAmount *= 0.1;
		m_flAccumulatedRuneHealthRegen += flAmount;
	}

	int nHealAmount = 0;
	if ( m_flAccumulatedRuneHealthRegen >= 1.0 )
	{
		nHealAmount = floor( m_flAccumulatedRuneHealthRegen );
		if ( GetHealth() < GetMaxHealth() )
		{
			TakeHealth( nHealAmount, DMG_GENERIC );
			int nHealedAmount = TakeHealth( nHealAmount, DMG_GENERIC );
			if ( nHealedAmount > 0 )
			{
				IGameEvent *event = gameeventmanager->CreateEvent( "player_healed" );
				if ( event )
				{
					event->SetInt("priority", 1);	// HLTV event priority
					event->SetInt("patient", GetUserID());
					event->SetInt( "healer", GetUserID() );
					event->SetInt( "amount", nHealedAmount );
					gameeventmanager->FireEvent( event );
				}
			}
		}
	}
	else if ( m_flAccumulatedRuneHealthRegen < -1.0 )
	{
		nHealAmount = ceil( m_flAccumulatedRuneHealthRegen );
		TakeDamage( CTakeDamageInfo( this, this, NULL, vec3_origin, WorldSpaceCenter(), nHealAmount * -1, DMG_GENERIC ) );
	}

	if ( GetHealth() < GetMaxHealth() && nHealAmount != 0 )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "player_healonhit" );
		if ( event )
		{
			event->SetInt( "amount", nHealAmount );
			event->SetInt( "entindex", entindex() );
			event->SetInt( "weapon_def_index", INVALID_ITEM_DEF_INDEX );
			gameeventmanager->FireEvent( event );
		}
	}

	m_flAccumulatedRuneHealthRegen -= nHealAmount;
	m_flLastRuneHealthRegenAt = gpGlobals->curtime;

	// Regenerate ammo and metal
	if ( m_flNextRuneAmmoRegenAt < gpGlobals->curtime )
	{
		m_flNextRuneAmmoRegenAt = gpGlobals->curtime + 5;

		if ( nRuneType == RUNE_REGEN )
		{
			RegenAmmoInternal( TF_AMMO_PRIMARY, 0.5f );
			RegenAmmoInternal( TF_AMMO_SECONDARY, 0.5f );
			GiveAmmo( 200, TF_AMMO_METAL, true );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::RegenAmmoInternal( int iIndex, float flRegen )
{
	m_flAccumulatedAmmoRegens[iIndex] += flRegen;
	
	// As soon as we have enough accumulated to regen a single unit of ammo, do it.
	int iMaxAmmo = GetMaxAmmo(iIndex);
	int iAmmo = m_flAccumulatedAmmoRegens[iIndex] * iMaxAmmo;
	if ( iAmmo >= 1 )
	{
		GiveAmmo( iAmmo, iIndex, true );
		m_flAccumulatedAmmoRegens[iIndex] -= ((float)iAmmo / (float)iMaxAmmo);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFPlayer::~CTFPlayer()
{
	delete [] m_damageRateArray;

	DestroyRagdoll();
	m_PlayerAnimState->Release();

	FOR_EACH_VEC( m_ItemsToTest, i )
	{
		int iDef = TESTITEM_DEFINITIONS_BEGIN_AT + m_ItemsToTest[i].scriptItem.GetItemDefIndex();
		ItemSystem()->GetItemSchema()->ItemTesting_DiscardTestDefinition( iDef );

		m_ItemsToTest[i].pKV->deleteThis();
		m_ItemsToTest[i].pKV = NULL;
	}

	if ( m_hReviveMarker )
	{
		UTIL_Remove( m_hReviveMarker );
		m_hReviveMarker = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFPlayer *CTFPlayer::CreatePlayer( const char *className, edict_t *ed )
{
	CTFPlayer::s_PlayerEdict = ed;
	return (CTFPlayer*)CreateEntityByName( className );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::UpdateTimers( void )
{
	m_Shared.ConditionThink();
	m_Shared.InvisibilityThink();
}

//-----------------------------------------------------------------------------
// Estimate where a projectile fired from the given weapon will initially hit (it may bounce on from there).
// NOTE: We should be able to directly compute this knowing initial velocity, angle, gravity, etc, 
// but I have been unable to find a formula that reproduces what our physics actually
// do.
//-----------------------------------------------------------------------------
Vector CTFPlayer::EstimateProjectileImpactPosition( CTFWeaponBaseGun *weapon )
{
	if ( !weapon )
	{
		return GetAbsOrigin();
	}

	const QAngle &angles = EyeAngles();

	float initVel = weapon->IsWeapon( TF_WEAPON_PIPEBOMBLAUNCHER ) ? 900.0f : weapon->GetProjectileSpeed();  
	CALL_ATTRIB_HOOK_FLOAT( initVel, mult_projectile_range );

	return EstimateProjectileImpactPosition( angles.x, angles.y, initVel );
}

//-----------------------------------------------------------------------------
// Estimate where a stickybomb projectile will hit,
// using given pitch, yaw, and weapon charge (0-1)
//-----------------------------------------------------------------------------
Vector CTFPlayer::EstimateStickybombProjectileImpactPosition( float pitch, float yaw, float charge )
{
	// estimate impact spot
	float initVel = charge * ( TF_PIPEBOMB_MAX_CHARGE_VEL - TF_PIPEBOMB_MIN_CHARGE_VEL ) + TF_PIPEBOMB_MIN_CHARGE_VEL;
	CALL_ATTRIB_HOOK_FLOAT( initVel, mult_projectile_range );

	return EstimateProjectileImpactPosition( pitch, yaw, initVel );
}

//-----------------------------------------------------------------------------
// Estimate where a projectile fired will initially hit (it may bounce on from there),
// using given pitch, yaw, and initial velocity.
//-----------------------------------------------------------------------------
Vector CTFPlayer::EstimateProjectileImpactPosition( float pitch, float yaw, float initVel )
{
	// copied from CTFWeaponBaseGun::FirePipeBomb()
	Vector vecForward, vecRight, vecUp;
	QAngle angles( pitch, yaw, 0.0f );
	AngleVectors( angles, &vecForward, &vecRight, &vecUp );

	// we will assume bots never flip viewmodels
	float fRight = 8.f;
	Vector vecSrc = Weapon_ShootPosition();
	vecSrc += vecForward * 16.0f + vecRight * fRight + vecUp * -6.0f;

	const float initVelScale = 0.9f;
	Vector vecVelocity = initVelScale * ( ( vecForward * initVel ) + ( vecUp * 200.0f ) );

	const float timeStep = 0.01f;
	const float maxTime = 5.0f;
								 
	Vector pos = vecSrc;
	Vector lastPos = pos;
	const float g = GetCurrentGravity();


	// compute forward facing unit vector in horiz plane
	Vector alongDir = vecForward;
	alongDir.z = 0.0f;
	alongDir.NormalizeInPlace();

	float alongVel = FastSqrt( vecVelocity.x * vecVelocity.x + vecVelocity.y * vecVelocity.y );

	trace_t trace;	
	NextBotTraceFilterIgnoreActors traceFilter( NULL, COLLISION_GROUP_NONE );

	float t;
	for( t = 0.0f; t < maxTime; t += timeStep )
	{
		float along = alongVel * t;
		float height = vecVelocity.z * t - 0.5f * g * t * t;

		pos.x = vecSrc.x + alongDir.x * along;
		pos.y = vecSrc.y + alongDir.y * along;
		pos.z = vecSrc.z + height;

		UTIL_TraceHull( lastPos, pos, -Vector(8,8,8), Vector(8,8,8), MASK_SOLID_BRUSHONLY, &traceFilter, &trace );

		if ( trace.DidHit() )
		{
#ifdef STAGING_ONLY
			if ( tf_debug_ballistics.GetBool() )
			{
				NDebugOverlay::Cross3D( trace.endpos, 10.0f, 100, 255, 0, true, NDEBUG_PERSIST_TILL_NEXT_SERVER );
			}
#endif
			break;
		}

#ifdef STAGING_ONLY
		if ( tf_debug_ballistics.GetBool() )
		{
			NDebugOverlay::Line( lastPos, pos, 0, 255, 0, false, NDEBUG_PERSIST_TILL_NEXT_SERVER );
		}
#endif

		lastPos = pos;
	}

	return trace.endpos;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::ProcessSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event )
{
	// TF Players only process scene events on the server while running taunts
	if ( !IsTaunting() )
		return false;

	// Only process sequences
	if ( event->GetType() != CChoreoEvent::SEQUENCE )
		return false;

	return BaseClass::ProcessSceneEvent( info, scene, event );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::PreThink()
{
	// Update timers.
	UpdateTimers();

	// Pass through to the base class think.
	BaseClass::PreThink();

	// Reset bullet force accumulator, only lasts one frame, for ragdoll forces from multiple shots.
	m_vecTotalBulletForce = vec3_origin;

	CheckForIdle();

	ProcessSceneEvents();

	if ( TFGameRules()->IsInArenaMode() == true )
	{
		if ( TFGameRules()->State_Get() != GR_STATE_TEAM_WIN )
		{
			if ( GetTeamNumber() == TEAM_SPECTATOR )
			{
				m_Local.m_iHideHUD &= ~HIDEHUD_MISCSTATUS;
			}
		}
	}

	// Hype Decreases over time
	if ( IsPlayerClass( TF_CLASS_SCOUT ) )
	{
		float flHypeDecays = 0;
		CALL_ATTRIB_HOOK_FLOAT( flHypeDecays, hype_decays_over_time );

		if ( flHypeDecays != 0 )
		{
			// Loose hype over time
			float flHype = m_Shared.GetScoutHypeMeter();
			flHype = flHype - flHypeDecays;
			m_Shared.SetScoutHypeMeter( flHype );
			TeamFortress_SetSpeed();
		}
	}

#ifdef STAGING_ONLY 
	// show ballistic path for currently equipped weapon (ie: grenades)
	if ( tf_debug_ballistics.GetBool() )
	{
		CTFWeaponBaseGun *myWeapon = dynamic_cast< CTFWeaponBaseGun * >( m_Shared.GetActiveTFWeapon() );
		EstimateProjectileImpactPosition( myWeapon );
	}

	if ( tf_debug_ballistic_targeting.GetBool() )
	{
		CTFWeaponBaseGun *myWeapon = dynamic_cast< CTFWeaponBaseGun * >( m_Shared.GetActiveTFWeapon() );
		Vector hitPos = EstimateProjectileImpactPosition( myWeapon );

		Vector toEye = EyePosition() - hitPos;
		Vector toTarget = tf_debug_ballistic_target - hitPos;
		float error = toTarget.NormalizeInPlace();

		if ( error < 20.0f )
		{
			// on target
			NDebugOverlay::Cross3D( tf_debug_ballistic_target, 10.0f, 0, 255, 0, true, NDEBUG_PERSIST_TILL_NEXT_SERVER );
		}
		else
		{
			if ( DotProduct( toEye, toTarget ) > 0.0f )
			{
				// too far
				NDebugOverlay::Cross3D( tf_debug_ballistic_target, 10.0f, 0, 0, 255, true, NDEBUG_PERSIST_TILL_NEXT_SERVER );
				NDebugOverlay::VertArrow( tf_debug_ballistic_target, tf_debug_ballistic_target - Vector( 0, 0, error ), 10.0f, 0, 0, 255, 255, true, NDEBUG_PERSIST_TILL_NEXT_SERVER );
			}
			else
			{
				// too near
				NDebugOverlay::Cross3D( tf_debug_ballistic_target, 10.0f, 255, 0, 0, true, NDEBUG_PERSIST_TILL_NEXT_SERVER );
				NDebugOverlay::VertArrow( tf_debug_ballistic_target, tf_debug_ballistic_target + Vector( 0, 0, error ), 10.0f, 255, 0, 0, 255, true, NDEBUG_PERSIST_TILL_NEXT_SERVER );
			}
		}		
	}
#endif // STAGING_ONLY
}

ConVar mp_idledealmethod( "mp_idledealmethod", "1", FCVAR_GAMEDLL, "Deals with Idle Players. 1 = Sends them into Spectator mode then kicks them if they're still idle, 2 = Kicks them out of the game;" );
ConVar mp_idlemaxtime( "mp_idlemaxtime", "3", FCVAR_GAMEDLL, "Maximum time a player is allowed to be idle (in minutes)" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::CheckForIdle( void )
{
	if ( m_afButtonLast != m_nButtons )
		m_flLastAction = gpGlobals->curtime;

	if ( mp_idledealmethod.GetInt() )
	{
		if ( IsHLTV() || IsReplay() )
			return;

		if ( IsFakeClient() )
			return;

		if ( IsCoaching() && GetStudent() != NULL )
			return;

		if ( TFGameRules() && TFGameRules()->ShowMatchSummary() )
			return;

		if ( TFGameRules()->State_Get() == GR_STATE_BETWEEN_RNDS )
			return;

		//Don't mess with the host on a listen server (probably one of us debugging something)
		if ( engine->IsDedicatedServer() == false && entindex() == 1 )
			return;

		if ( IsAutoKickDisabled() )
			return;

		const bool cbMoving = ( m_nButtons & ( IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT ) ) != 0;

		m_bIsAFK = false;

		if ( !cbMoving && PointInRespawnRoom( this, WorldSpaceCenter() ) )
		{
			m_flTimeInSpawn += TICK_INTERVAL;
		}
		else
			m_flTimeInSpawn = 0;

		if ( TFGameRules()->IsInArenaMode() && tf_arena_use_queue.GetBool() == true )
		{
			if ( GetTeamNumber() == TEAM_SPECTATOR )
				return;

			if ( TFGameRules()->State_Get() == GR_STATE_TEAM_WIN && TFGameRules()->GetWinningTeam() != GetTeamNumber() )
			{
				if ( m_bArenaIsAFK )
				{
					m_bIsAFK = true;
					m_bArenaIsAFK = false;
				}
			}
		}
		else
		{
			// Cannot possibly get out of the spawn room in 0 seconds--so if the ConVar says 0, let's assume 30 seconds.
			float flIdleTime = Max( mp_idlemaxtime.GetFloat() * 60, 30.0f );

			if ( TFGameRules()->InStalemate() )
			{
				flIdleTime = mp_stalemate_timelimit.GetInt() * 0.5f;
			}

			m_bIsAFK = ( gpGlobals->curtime - m_flLastAction ) > flIdleTime
			        || ( m_flTimeInSpawn > flIdleTime ); 
		}
		
		if ( m_bIsAFK == true )
		{
			bool bKickPlayer = false;

			ConVarRef mp_allowspectators( "mp_allowspectators" );
			if ( ( mp_allowspectators.IsValid() && mp_allowspectators.GetBool() == false ) || ( TFGameRules()->IsInArenaMode() && tf_arena_use_queue.GetBool() ) )
			{
				// just kick the player if this server doesn't allow spectators
				bKickPlayer = true;
			}
			else if ( mp_idledealmethod.GetInt() == 1 )
			{
				if ( GetTeamNumber() < FIRST_GAME_TEAM )
				{
					bKickPlayer = true;
				}
				else
				{
					//First send them into spectator mode then kick him.
					ForceChangeTeam( TEAM_SPECTATOR );
					m_flLastAction = gpGlobals->curtime;
					m_flTimeInSpawn = 0;
					return;
				}
			}
			else if ( mp_idledealmethod.GetInt() == 2 )
			{
				bKickPlayer = true;
			}

			if ( bKickPlayer == true )
			{
				UTIL_ClientPrintAll( HUD_PRINTCONSOLE, "#game_idle_kick", GetPlayerName() );
				engine->ServerCommand( UTIL_VarArgs( "kickid %d %s\n", GetUserID(), g_pszIdleKickString ) );
				m_flLastAction = gpGlobals->curtime;
				m_flTimeInSpawn = 0;
			}
		}
	}
}

extern ConVar flashlight;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFPlayer::FlashlightIsOn( void )
{
	return IsEffectActive( EF_DIMLIGHT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::FlashlightTurnOn( void )
{
	if( flashlight.GetInt() > 0 && IsAlive() )
	{
		AddEffects( EF_DIMLIGHT );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::FlashlightTurnOff( void )
{
	if( IsEffectActive(EF_DIMLIGHT) )
	{
		RemoveEffects( EF_DIMLIGHT );
	}	
}

//-----------------------------------------------------------------------------
// Purpose: Update Halloween scenario effects on players.
//-----------------------------------------------------------------------------
void CTFPlayer::UpdateHalloween( void )
{
	// This is a push force
	if ( !m_vHalloweenKartPush.IsZero() )
	{
		if ( !m_Shared.InCond( TF_COND_INVULNERABLE_USER_BUFF ) )
		{		
			CPVSFilter filter( GetAbsOrigin() );
			TE_TFParticleEffect( filter, 0.0, "kart_impact_sparks", GetAbsOrigin(), vec3_angle, this, PATTACH_ABSORIGIN );

			float flStunDuration = m_vHalloweenKartPush.Length() / 1000.0f;
			if ( m_vHalloweenKartPush.LengthSqr() > 1000 * 1000 )
			{
				TE_TFParticleEffect( filter, 0.0, "kartimpacttrail", GetAbsOrigin(), vec3_angle, this, PATTACH_ABSORIGIN_FOLLOW );
				EmitSound( "BumperCar.BumpIntoAir" );
				EmitSound( "BumperCar.BumpHard" );
			}
			else
			{
				EmitSound( "BumperCar.Bump" );
			}

			if ( tf_halloween_kart_stun_enabled.GetBool() )
			{
				m_Shared.StunPlayer( flStunDuration * tf_halloween_kart_stun_duration_scale.GetFloat(), tf_halloween_kart_stun_amount.GetFloat(), TF_STUN_BOTH | TF_STUN_NO_EFFECTS );
			}

			if ( m_Shared.InCond( TF_COND_HALLOWEEN_KART_DASH ) )
			{
				m_Shared.RemoveCond( TF_COND_HALLOWEEN_KART_DASH );
			}

			ApplyAirBlastImpulse( m_vHalloweenKartPush );
		}
		
		m_vHalloweenKartPush.Zero();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::AddHalloweenKartPushEvent( CTFPlayer *pOther, CBaseEntity *pInflictor, CBaseEntity *pWeapon, Vector vForce, int iDamage, int iDamageType /* = 0 */ )
{
	// Create a damage event so they can get credit for the kill
	//m_vHalloweenKartPushEventTime + 0.2 > gpGlobals->curtime &&
	if ( m_Shared.InCond( TF_COND_INVULNERABLE_USER_BUFF ) )
		return;

	// Ignore small forces
	float flForce = vForce.LengthSqr();
	if ( flForce < 100.0f )
		return;

	float flExtraMultiplier = 1.f;
	if ( pOther && pOther != this && pOther->m_Shared.InCond( TF_COND_HALLOWEEN_BOMB_HEAD ) )
	{
		iDamage *= tf_halloween_kart_bomb_head_damage_scale.GetFloat();
		flExtraMultiplier = tf_halloween_kart_bomb_head_impulse_scale.GetFloat();
		pOther->SetKartBombHeadTarget( pOther );
		pOther->m_Shared.RemoveCond( TF_COND_HALLOWEEN_BOMB_HEAD );
	}

	const float flCurrentKartKnockbackMultiplier =  GetKartKnockbackMultiplier( flExtraMultiplier );

	if ( pOther )
	{
		// Fake Damage
		IGameEvent * event = gameeventmanager->CreateEvent( "player_hurt" );
		if ( event )
		{
			int iKartDamageType = DMG_CLUB | DMG_PREVENT_PHYSICS_FORCE;
			if ( pOther != this && flCurrentKartKnockbackMultiplier * vForce.LengthSqr() > 1000 * 1000 )
			{
				iKartDamageType |= DMG_CRITICAL;
			}

			event->SetInt( "userid", GetUserID() );
			event->SetInt( "health", MAX( 0, m_iHealth ) );

			// HLTV event priority, not transmitted
			event->SetInt( "priority", 5 );
			event->SetInt( "damageamount", iDamage );

			// Hurt by another player.
			event->SetInt( "attacker", pOther->GetUserID() );
			event->SetInt( "custom", TF_DMG_CUSTOM_SUICIDE );
			event->SetBool( "crit", ( iKartDamageType & DMG_CRITICAL ) != 0 );
			event->SetBool( "allseecrit", ( iKartDamageType & DMG_CRITICAL ) != 0 );
			event->SetInt( "bonuseffect", (int)kBonusEffect_None );
			//
			gameeventmanager->FireEvent( event );
		}
		
		m_AchievementData.AddDamagerToHistory( pOther );
		m_AchievementData.AddPusherToHistory( pOther );
		//m_Shared.SetAssist( pOther );
	}

	if ( m_Shared.InCond( TF_COND_HALLOWEEN_KART ) )
	{
		m_iKartHealth += iDamage;
	}

	// HHH
	if ( TFGameRules()->IsIT( pOther ) )
	{
		// Tag! You're IT!
		TFGameRules()->SetIT( this );
	}

	if ( iDamageType == TF_DMG_CUSTOM_DECAPITATION_BOSS )
	{
		m_flHHHKartAttackTime = gpGlobals->curtime;
	}

	//m_vHalloweenKartPushEventTime = gpGlobals->curtime;
	float flImpulseScale = 1.0f;

	// m_flKartHealth might change by this point, calculate new knock back multiplier
	const float flNewKartKnockbackMultiplier = GetKartKnockbackMultiplier( flExtraMultiplier );

	// push other
	if ( m_Shared.InCond( TF_COND_HALLOWEEN_KART ) )
	{
		// If applying damage increase the force, otherwise its likely just a wall collision
		if ( iDamage > 0 )
		{
			vForce *= flImpulseScale * flNewKartKnockbackMultiplier;
			vForce.z *= ( ( flNewKartKnockbackMultiplier - 1.0f ) * 0.20f ) + 1.0f;

			// Decrease all forces if in the air
			if (!(GetFlags() & FL_ONGROUND) )
			{
				vForce *= tf_halloween_kart_impact_air_scale.GetFloat();
			}
		}
	}
	else
	{
		// Make non-karters take damage!
		vForce *= ( flImpulseScale * 2.0f );

		//ghostinfo.SetDamageCustom( TF_DMG_CUSTOM_KART );	
		// Create a damage event based on Speed
		float flDamage = vForce.Length() / 50.0f + RandomFloat( 3.0f, 7.0f );
		CTakeDamageInfo info;
		info.SetAttacker( pOther );
		info.SetInflictor( pOther );
		info.SetDamage( flDamage );
		info.SetDamageCustom( TF_DMG_CUSTOM_KART );
		info.SetDamagePosition( GetAbsOrigin() );
		int iKartDamageType = DMG_CLUB;
		if ( flDamage > 20 )
		{
			iKartDamageType |= DMG_CRITICAL;
		}
		info.SetDamageType( iKartDamageType );
		info.SetDamageForce( vForce );
		TakeDamage( info );
		return;
	}

	m_vHalloweenKartPush += vForce;

	// Dropped collection game tokens if hit hard enough and we're in the collection minigame
	if ( pOther && iDamage > 10 && CTFMinigameLogic::GetMinigameLogic() && CTFMinigameLogic::GetMinigameLogic()->GetActiveMinigame() && 
		CTFMinigameLogic::GetMinigameLogic()->GetActiveMinigame()->GetMinigameType() == CTFMiniGame::EMinigameType::MINIGAME_HALLOWEEN2014_COLLECTION )
	{
		CUtlVector< CTFPlayer* > vecEveryone;
		CollectPlayers( &vecEveryone );
		int nNumPlayers = vecEveryone.Count();

		// Drop tokens
		uint32 nNumToSpawn = ( iDamage / 5 ) + 1;
		nNumToSpawn = RemapValClamped( nNumPlayers, 8, 16, nNumToSpawn, 1 );

		if ( gpGlobals->curtime > m_flNextBonusDucksVOAllowedTime )
		{
			// Tell this user to play a "Bonus Ducks!" line. 
			CSingleUserRecipientFilter filter( pOther );
			UserMessageBegin( filter, "BonusDucks" );
				WRITE_BYTE( entindex() );
				WRITE_BYTE( false );
			MessageEnd();
		}

		while( nNumToSpawn-- )
		{
			CHalloweenPickup *pPickup = dynamic_cast< CHalloweenPickup * >( CreateEntityByName( "tf_halloween_pickup" ) );
			if (pPickup)
			{
				pPickup->m_nSkin = 2; // Golden skin
				pPickup->Precache();
				DispatchSpawn(pPickup);
				Vector vecRandom = RandomVector( -200.f, 200.f );
				vecRandom.z = RandomFloat( 300.f, 400.f );
				Vector vecDropVector = vecRandom + vForce * 0.2f;
				pPickup->DropSingleInstance( vecDropVector, this, 1.f, 1.f );

				pPickup->SetAbsOrigin( GetAbsOrigin() + Vector( 0.f, 0.f, 40.f ) );

				pPickup->Activate();
			}
		}
	}
	
	//DevMsg( "Kart Impact %fx,%fy,%fz - %f Base. %f Multiplayer, %f TotalForce, %d Damage, %i Class \n", 
	//	vForce.x, vForce.y, vForce.z, vForce.Length(), flNewKartKnockbackMultiplier, vForce.Length() * flNewKartKnockbackMultiplier, iDamage, GetPlayerClass()->GetClassIndex() );
	
	vForce.z = 0;
	vForce.NormalizeInPlace();
	DispatchParticleEffect( "kart_impact_sparks", GetAbsOrigin() + vForce * 24, GetAbsAngles() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFPlayer::GetKartKnockbackMultiplier( float flExtraMultiplier /*= 1.f*/ ) const
{
	return flExtraMultiplier * ( 1.0f + (float)m_iKartHealth / tf_halloween_kart_damage_to_force.GetFloat() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ResetKartDamage() 
{ 
	m_iKartHealth = 0; 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::CancelEurekaTeleport()
{
	m_bIsTeleportingUsingEurekaEffect = false;
	m_teleportHomeFlashTimer.Invalidate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::PostThink()
{
	BaseClass::PostThink();

	QAngle angles = GetLocalAngles();
	angles[PITCH] = 0;
	SetLocalAngles( angles );
	
	// Store the eye angles pitch so the client can compute its animation state correctly.
	m_angEyeAngles = EyeAngles();

    m_PlayerAnimState->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );

	if ( m_flTauntAttackTime && m_flTauntAttackTime < gpGlobals->curtime )
	{
		m_flTauntAttackTime = 0;
		DoTauntAttack();
	}

	// if we are coaching, then capture events for adding annotations
	if ( m_bIsCoaching && m_hStudent )
	{
		if ( ( m_afButtonPressed & ( IN_ATTACK | IN_ATTACK2 ) ) != 0 )
		{
			if ( m_afButtonPressed & IN_ATTACK )
			{
				HandleCoachCommand( this, kCoachCommand_Attack );
			}
			else if ( m_afButtonPressed & IN_ATTACK2 )
			{
				HandleCoachCommand( this, kCoachCommand_Defend );
			}
		}
		if ( m_hStudent->GetTeamNumber() != TEAM_SPECTATOR )
		{
			// tether coach to student--if the coach gets too far, move them toward the student
			Vector vecTarget = m_hStudent->GetAbsOrigin();
			Vector vecDelta = GetAbsOrigin() - vecTarget;
			float flDistance = vecDelta.Length();
			const float kInchesToMeters = 0.0254f;
			const float kMetersToInches = 1.0f / kInchesToMeters;
			const float kMaxDistanceToStudent = 30;
			int distance = RoundFloatToInt( flDistance * kInchesToMeters );
			if ( distance > kMaxDistanceToStudent )
			{
				VectorNormalize( vecDelta );
				SetAbsOrigin( vecTarget + vecDelta * ( kMaxDistanceToStudent * kMetersToInches ) );
			}
		}
	}

	if ( TFGameRules()->IsMannVsMachineMode() )
	{
		// metal is free during setup time
		if ( TFGameRules()->IsQuickBuildTime() )
		{
			GiveAmmo( 1000, TF_AMMO_METAL, true );
		}

		// clamp maximum velocity to avoid sending mini-bosses into the stratosphere
		if ( GetTeamNumber() == TF_TEAM_PVE_INVADERS )
		{
			Vector ahead = GetAbsVelocity();
			float speed = ahead.NormalizeInPlace();

			const float velocityLimit = 1000.0f;
			if ( speed > velocityLimit )
			{
				speed = velocityLimit;
			}

			SetAbsVelocity( speed * ahead );
		}
	}

	UpdateHalloween();

#ifdef STAGING_ONLY
	m_Shared.DoRocketPack();
#endif // STAGING_ONLY
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::PrecacheMvM()
{
	for ( int i = TF_FIRST_NORMAL_CLASS; i < TF_LAST_NORMAL_CLASS; ++i )
	{
		COMPILE_TIME_ASSERT( 