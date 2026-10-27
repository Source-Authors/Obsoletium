//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "cbase.h"
#include "tf_gamerules.h"
#include "tf_player_shared.h"
#include "takedamageinfo.h"
#include "tf_weaponbase.h"
#include "effect_dispatch_data.h"
#include "tf_item.h"
#include "entity_capture_flag.h"
#include "tf_weapon_medigun.h"
#include "tf_weapon_pipebomblauncher.h"
#include "tf_weapon_invis.h"
#include "tf_weapon_sniperrifle.h"
#include "tf_weapon_shovel.h"
#include "tf_weapon_sword.h"
#include "tf_weapon_shotgun.h"
#include "in_buttons.h"
#include "tf_weapon_lunchbox.h"
#include "tf_weapon_flaregun.h"
#include "tf_weapon_wrench.h"
#include "econ_wearable.h"
#include "econ_item_system.h"
#include "tf_weapon_knife.h"
#include "tf_weapon_syringegun.h"
#include "tf_weapon_flamethrower.h"
#include "econ_entity_creation.h"
#include "tf_mapinfo.h"
#include "tf_dropped_weapon.h"
#include "tf_weapon_passtime_gun.h"

// Client specific.
#ifdef CLIENT_DLL
#include "c_tf_player.h"
#include "c_te_effect_dispatch.h"
#include "c_tf_fx.h"
#include "soundenvelope.h"
#include "c_tf_playerclass.h"
#include "iviewrender.h"
#include "prediction.h"
#include "achievementmgr.h"
#include "baseachievement.h"
#include "achievements_tf.h"
#include "c_tf_weapon_builder.h"
#include "dt_utlvector_recv.h"
#include "recvproxy.h"
#include "c_tf_weapon_builder.h"
#include "c_func_capture_zone.h"
#include "tf_hud_target_id.h"
#include "tempent.h"
#include "cam_thirdperson.h"
#include "vgui/IInput.h"

#define CTFPlayerClass C_TFPlayerClass
#define CCaptureZone C_CaptureZone
#define CRecipientFilter C_RecipientFilter

#include "c_tf_objective_resource.h"
#include "tf_weapon_buff_item.h"
#include "c_tf_passtime_logic.h"

// Server specific.
#else
#include "tf_player.h"
#include "te_effect_dispatch.h"
#include "tf_fx.h"
#include "util.h"
#include "tf_team.h"
#include "tf_gamestats.h"
#include "tf_playerclass.h"
#include "SpriteTrail.h"
#include "tf_weapon_builder.h"
#include "nav_mesh/tf_nav_area.h"
#include "nav_pathfind.h"
#include "tf_obj_dispenser.h"
#include "dt_utlvector_send.h"
#include "tf_item_wearable.h"
#include "NextBotManager.h"
#include "tf_weapon_builder.h"
#include "func_capture_zone.h"
#include "hl2orange.spa.h"
#include "bot/tf_bot.h"
#include "tf_objective_resource.h"
#include "halloween/tf_weapon_spellbook.h"
#include "tf_weapon_buff_item.h"
#include "tf_passtime_logic.h"
#include "tf_weapon_passtime_gun.h"
#include "entity_healthkit.h"
#include "halloween/merasmus/merasmus.h"
#include "tf_weapon_grapplinghook.h"
#include "tf_wearable_levelable_item.h"
#include "tf_obj_sentrygun.h"
#endif

#include "tf_wearable_item_demoshield.h"
#include "tf_weapon_bonesaw.h"

static ConVar tf_demoman_charge_frametime_scaling( "tf_demoman_charge_frametime_scaling", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "When enabled, scale yaw limiting based on client performance (frametime)." );
static const float YAW_CAP_SCALE_MIN = 0.2f;
static const float YAW_CAP_SCALE_MAX = 2.f;

ConVar tf_halloween_kart_boost_recharge( "tf_halloween_kart_boost_recharge", "5.0f", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar tf_halloween_kart_boost_duration( "tf_halloween_kart_boost_duration", "1.5f", FCVAR_REPLICATED | FCVAR_CHEAT );

ConVar tf_scout_air_dash_count( "tf_scout_air_dash_count", "1", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

ConVar tf_spy_invis_time( "tf_spy_invis_time", "1.0", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "Transition time in and out of spy invisibility", true, 0.1, true, 5.0 );
ConVar tf_spy_invis_unstealth_time( "tf_spy_invis_unstealth_time", "2.0", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "Transition time in and out of spy invisibility", true, 0.1, true, 5.0 );

ConVar tf_spy_max_cloaked_speed( "tf_spy_max_cloaked_speed", "999", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED );	// no cap
ConVar tf_whip_speed_increase( "tf_whip_speed_increase", "105", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED );
ConVar tf_max_health_boost( "tf_max_health_boost", "1.5", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "Max health factor that players can be boosted to by healers.", true, 1.0, false, 0 );
ConVar tf_invuln_time( "tf_invuln_time", "1.0", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "Time it takes for invulnerability to wear off." );
ConVar tf_player_movement_stun_time( "tf_player_movement_stun_time", "0.5", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED );
extern ConVar tf_player_movement_restart_freeze;
extern ConVar mp_tournament_readymode_countdown;
extern ConVar tf_max_charge_speed;

ConVar tf_always_loser( "tf_always_loser", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "Force loserstate to true." );

ConVar tf_mvm_bot_flag_carrier_movement_penalty( "tf_mvm_bot_flag_carrier_movement_penalty", "0.5", FCVAR_REPLICATED | FCVAR_CHEAT );

//ConVar tf_scout_dodge_move_penalty_duration( "tf_scout_dodge_move_penalty_duration", "3.0", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED );
//ConVar tf_scout_dodge_move_penalty( "tf_scout_dodge_move_penalty", "0.5", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED );


#ifdef GAME_DLL
ConVar tf_boost_drain_time( "tf_boost_drain_time", "15.0", FCVAR_DEVELOPMENTONLY, "Time is takes for a full health boost to drain away from a player.", true, 0.1, false, 0 );
#ifndef NDEBUG
CON_COMMAND_F( tf_add_bombhead, "Add Merasmus Bomb Head Condition", 0 )
{
	CUtlVector< CTFPlayer * > playerVector;
	CollectPlayers( &playerVector, TF_TEAM_RED, true );

	FOR_EACH_VEC ( playerVector, i )
	{
		float flBuffDuration = 7.0f;
		playerVector[i]->m_Shared.StunPlayer( flBuffDuration, 0.f, TF_STUN_LOSER_STATE );
		playerVector[i]->m_Shared.AddCond( TF_COND_HALLOWEEN_BOMB_HEAD, flBuffDuration );
		playerVector[i]->m_Shared.AddCond( TF_COND_SPEED_BOOST, flBuffDuration );
		//playerVector[i]->m_Shared.AddCond( TF_COND_HALLOWEEN_BOMB_HEAD, 7 );
	}
}

ConVar tf_debug_bullets( "tf_debug_bullets", "0", FCVAR_DEVELOPMENTONLY, "Visualize bullet traces." );
#endif  // !NDEBUG

ConVar tf_damage_events_track_for( "tf_damage_events_track_for", "30",  FCVAR_DEVELOPMENTONLY );

extern ConVar tf_halloween_giant_health_scale;

ConVar tf_allow_sliding_taunt( "tf_allow_sliding_taunt", "0", FCVAR_NONE, "1 - Allow player to slide for a bit after taunting" );

#endif // GAME_DLL

#ifdef STAGING_ONLY
ConVar tf_force_allow_move_during_taunt( "tf_force_allow_move_during_taunt", "0", FCVAR_REPLICATED );
#endif // STAGING_ONLY

ConVar tf_useparticletracers( "tf_useparticletracers", "1", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "Use particle tracers instead of old style ones." );
ConVar tf_spy_cloak_consume_rate( "tf_spy_cloak_consume_rate", "10.0", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "cloak to use per second while cloaked, from 100 max )" );	// 10 seconds of invis
ConVar tf_spy_cloak_regen_rate( "tf_spy_cloak_regen_rate", "3.3", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "cloak to regen per second, up to 100 max" );		// 30 seconds to full charge
ConVar tf_spy_cloak_no_attack_time( "tf_spy_cloak_no_attack_time", "2.0", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "time after uncloaking that the spy is prohibited from attacking" );
ConVar tf_tournament_hide_domination_icons( "tf_tournament_hide_domination_icons", "0", FCVAR_REPLICATED, "Tournament mode server convar that forces clients to not display the domination icons above players dominating them." );
ConVar tf_damage_disablespread( "tf_damage_disablespread", "1", FCVAR_REPLICATED | FCVAR_NOTIFY, "Toggles the random damage spread applied to all player damage." );

ConVar tf_scout_energydrink_regen_rate( "tf_scout_energydrink_regen_rate", "3.3", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "energy drink regen per second, up to 100 max" );
ConVar tf_scout_energydrink_consume_rate( "tf_scout_energydrink_consume_rate", "12.5", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "energy drink to use per second while boosted, from 100 max" );
ConVar tf_scout_energydrink_activation( "tf_scout_energydrink_activation", "0.0", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "how long it takes for the energy buff to become active" );

ConVar tf_demoman_charge_regen_rate( "tf_demoman_charge_regen_rate", "8.3", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "" );
ConVar tf_demoman_charge_drain_time( "tf_demoman_charge_drain_time", "1.5", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "" );

// STAGING_SPY
ConVar tf_feign_death_duration( "tf_feign_death_duration", "3.0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT, "Time that feign death buffs last." );
ConVar tf_feign_death_speed_duration( "tf_feign_death_speed_duration", "3.0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT, "Time that feign death speed boost last." );

ConVar tf_allow_taunt_switch( "tf_allow_taunt_switch", "0", FCVAR_REPLICATED, "0 - players are not allowed to switch weapons while taunting, 1 - players can switch weapons at the start of a taunt (old bug behavior), 2 - players can switch weapons at any time during a taunt." );

ConVar tf_allow_all_team_partner_taunt( "tf_allow_all_team_partner_taunt", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );

#ifdef STAGING_ONLY
ConVar tf_random_item_min( "tf_random_item_min", "-1", FCVAR_REPLICATED, "Min Itemdef for random cosmetics" );
ConVar tf_random_item_max( "tf_random_item_max", "-1", FCVAR_REPLICATED, "Max Itemdef for random cosmetics" );

ConVar tf_killstreak_eyeglow( "tf_killstreak_eyeglow", "0", FCVAR_REPLICATED, "Force Kill Streak effect index for Eye Glows. -1 to disable regardless of equipped items.  Stats at 2002" );
ConVar tf_killstreak_color( "tf_killstreak_color", "0", FCVAR_REPLICATED, "Force Kill Streak color." );
ConVar tf_eyeglow_wip( "tf_eyeglow_wip", "0", FCVAR_REPLICATED, "Activate to Always have specific wip eyeglow (DEV)" );
#endif // Staging only

#ifdef CLIENT_DLL
ConVar tf_colorblindassist( "tf_colorblindassist", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Setting this to 1 turns on colorblind mode." );

extern ConVar cam_idealdist;
extern ConVar cam_idealdistright;

#endif // CLIENT_DLL

extern ConVar tf_flamethrower_flametime;
extern ConVar weapon_medigun_chargerelease_rate;
#if !defined(NDEBUG) || defined( STAGING_ONLY )
extern ConVar mp_developer;
#endif  // !NDEBUG || STAGING_ONLY

//ConVar tf_spy_stealth_blink_time( "tf_spy_stealth_blink_time", "0.3", FCVAR_DEVELOPMENTONLY, "time after being hit the spy blinks into view" );
//ConVar tf_spy_stealth_blink_scale( "tf_spy_stealth_blink_scale", "0.85", FCVAR_DEVELOPMENTONLY, "percentage visible scalar after being hit the spy blinks into view" );
#define TF_SPY_STEALTH_BLINKTIME   0.3f
#define TF_SPY_STEALTH_BLINKSCALE  0.85f

#define TF_BUILDING_PICKUP_RANGE 150
#define TF_BUILDING_RESCUE_MIN_RANGE_SQ 62500  //250 * 250
#define TF_BUILDING_RESCUE_MAX_RANGE 5500

#define TF_PLAYER_CONDITION_CONTEXT	"TFPlayerConditionContext"

#define TF_SCREEN_OVERLAY_MATERIAL_BURNING		"effects/imcookin" 
#define TF_SCREEN_OVERLAY_MATERIAL_INVULN_RED	"effects/invuln_overlay_red" 
#define TF_SCREEN_OVERLAY_MATERIAL_INVULN_BLUE	"effects/invuln_overlay_blue" 

#define TF_SCREEN_OVERLAY_MATERIAL_MILK				"effects/milk_screen" 
#define TF_SCREEN_OVERLAY_MATERIAL_URINE			"effects/jarate_overlay" 
#define TF_SCREEN_OVERLAY_MATERIAL_BLEED			"effects/bleed_overlay" 
#define TF_SCREEN_OVERLAY_MATERIAL_STEALTH			"effects/stealth_overlay"
#define TF_SCREEN_OVERLAY_MATERIAL_SWIMMING_CURSE	"effects/jarate_overlay" 

#define TF_SCREEN_OVERLAY_MATERIAL_PHASE	"effects/dodge_overlay"

#define MAX_DAMAGE_EVENTS		128

const char *g_pszBDayGibs[22] = 
{
	"models/effects/bday_gib01.mdl",
	"models/effects/bday_gib02.mdl",
	"models/effects/bday_gib03.mdl",
	"models/effects/bday_gib04.mdl",
	"models/player/gibs/gibs_balloon.mdl",
	"models/player/gibs/gibs_burger.mdl",
	"models/player/gibs/gibs_boot.mdl",
	"models/player/gibs/gibs_bolt.mdl",
	"models/player/gibs/gibs_can.mdl",
	"models/player/gibs/gibs_clock.mdl",
	"models/player/gibs/gibs_fish.mdl",
	"models/player/gibs/gibs_gear1.mdl",
	"models/player/gibs/gibs_gear2.mdl",
	"models/player/gibs/gibs_gear3.mdl",
	"models/player/gibs/gibs_gear4.mdl",
	"models/player/gibs/gibs_gear5.mdl",
	"models/player/gibs/gibs_hubcap.mdl",
	"models/player/gibs/gibs_licenseplate.mdl",
	"models/player/gibs/gibs_spring1.mdl",
	"models/player/gibs/gibs_spring2.mdl",
	"models/player/gibs/gibs_teeth.mdl",
	"models/player/gibs/gibs_tire.mdl"
};

ETFCond g_SoldierBuffAttributeIDToConditionMap[kSoldierBuffCount + 1] =
{
	TF_COND_LAST,				// dummy entry to deal with attribute value of "1" being the lowest value we store in the attribute itself
	TF_COND_OFFENSEBUFF,
	TF_COND_DEFENSEBUFF,
	TF_COND_REGENONDAMAGEBUFF,
	TF_COND_NOHEALINGDAMAGEBUFF,
	TF_COND_CRITBOOSTED_RAGE_BUFF,
	TF_COND_SNIPERCHARGE_RAGE_BUFF
};

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void RecvProxy_BuildablesListChanged( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	RecvProxy_Int32ToInt32( pData, pStruct, pOut );

	C_TFPlayer* pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( !pLocalPlayer || pLocalPlayer->entindex() != pData->m_ObjectID )
		return;

	int index = pData->m_pRecvProp->GetOffset() / sizeof(int);
	int object = pData->m_Value.m_Int;

	IGameEvent *event = gameeventmanager->CreateEvent( "update_status_item" );
	if ( event )
	{
		event->SetInt( "index", index );
		event->SetInt( "object", object );
		gameeventmanager->FireEventClientSide( event );
	}
}
#endif

//=============================================================================
//
// Tables.
//

// Client specific.

#ifdef CLIENT_DLL

BEGIN_RECV_TABLE_NOBASE( localplayerscoring_t, DT_TFPlayerScoringDataExclusive )
	RecvPropInt( RECVINFO( m_iCaptures ) ),
	RecvPropInt( RECVINFO( m_iDefenses ) ),
	RecvPropInt( RECVINFO( m_iKills ) ),
	RecvPropInt( RECVINFO( m_iDeaths ) ),
	RecvPropInt( RECVINFO( m_iSuicides ) ),
	RecvPropInt( RECVINFO( m_iDominations ) ),
	RecvPropInt( RECVINFO( m_iRevenge ) ),
	RecvPropInt( RECVINFO( m_iBuildingsBuilt ) ),
	RecvPropInt( RECVINFO( m_iBuildingsDestroyed ) ),
	RecvPropInt( RECVINFO( m_iHeadshots ) ),
	RecvPropInt( RECVINFO( m_iBackstabs ) ),
	RecvPropInt( RECVINFO( m_iHealPoints ) ),
	RecvPropInt( RECVINFO( m_iInvulns ) ),
	RecvPropInt( RECVINFO( m_iTeleports ) ),
	RecvPropInt( RECVINFO( m_iResupplyPoints ) ),
	RecvPropInt( RECVINFO( m_iKillAssists ) ),
	RecvPropInt( RECVINFO( m_iPoints ) ),
	RecvPropInt( RECVINFO( m_iBonusPoints ) ),
	RecvPropInt( RECVINFO( m_iDamageDone ) ),
	RecvPropInt( RECVINFO( m_iCrits ) ),
END_RECV_TABLE()

EXTERN_RECV_TABLE(DT_TFPlayerConditionListExclusive);

BEGIN_RECV_TABLE_NOBASE( CTFPlayerShared, DT_TFPlayerSharedLocal )
	RecvPropInt( RECVINFO( m_nDesiredDisguiseTeam ) ),
	RecvPropInt( RECVINFO( m_nDesiredDisguiseClass ) ),
	RecvPropTime( RECVINFO( m_flStealthNoAttackExpire ) ),
	RecvPropTime( RECVINFO( m_flStealthNextChangeTime ) ),
	RecvPropBool( RECVINFO( m_bLastDisguisedAsOwnTeam ) ),
	RecvPropFloat( RECVINFO( m_flRageMeter ) ),
	RecvPropBool( RECVINFO( m_bRageDraining ) ),
	RecvPropTime( RECVINFO( m_flNextRageEarnTime ) ),
	RecvPropBool( RECVINFO( m_bInUpgradeZone ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_bPlayerDominated ), RecvPropBool( RECVINFO( m_bPlayerDominated[0] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_bPlayerDominatingMe ), RecvPropBool( RECVINFO( m_bPlayerDominatingMe[0] ) ) ),
	RecvPropDataTable( RECVINFO_DT(m_ScoreData),0, &REFERENCE_RECV_TABLE(DT_TFPlayerScoringDataExclusive) ),
	RecvPropDataTable( RECVINFO_DT(m_RoundScoreData),0, &REFERENCE_RECV_TABLE(DT_TFPlayerScoringDataExclusive) ),
END_RECV_TABLE()

BEGIN_RECV_TABLE_NOBASE( condition_source_t, DT_TFPlayerConditionSource )
	//RecvPropInt( RECVINFO( m_nPreventedDamageFromCondition ) ),
	//RecvPropFloat( RECVINFO( m_flExpireTime ) ),
	RecvPropEHandle( RECVINFO( m_pProvider ) ),
	//RecvPropBool( RECVINFO( m_bPrevActive ) ),
END_RECV_TABLE()

BEGIN_RECV_TABLE_NOBASE( CTFPlayerShared, DT_TFPlayerShared )
	RecvPropInt( RECVINFO( m_nPlayerCond ) ),
	RecvPropInt( RECVINFO( m_bJumping) ),
	RecvPropInt( RECVINFO( m_nNumHealers ) ),
	RecvPropInt( RECVINFO( m_iCritMult ) ),
	RecvPropInt( RECVINFO( m_iAirDash ) ),
	RecvPropInt( RECVINFO( m_nAirDucked ) ),
	RecvPropFloat( RECVINFO( m_flDuckTimer ) ),
	RecvPropInt( RECVINFO( m_nPlayerState ) ),
	RecvPropInt( RECVINFO( m_iDesiredPlayerClass ) ),
	RecvPropFloat( RECVINFO( m_flMovementStunTime ) ),
	RecvPropInt( RECVINFO( m_iMovementStunAmount ) ),
	RecvPropInt( RECVINFO( m_iMovementStunParity ) ),
	RecvPropEHandle( RECVINFO( m_hStunner ) ),
	RecvPropInt( RECVINFO( m_iStunFlags ) ),
	RecvPropInt( RECVINFO( m_nArenaNumChanges ) ),
	RecvPropBool( RECVINFO( m_bArenaFirstBloodBoost ) ),
	RecvPropInt( RECVINFO( m_iWeaponKnockbackID ) ),
	RecvPropBool( RECVINFO( m_bLoadoutUnavailable ) ),
	RecvPropInt( RECVINFO( m_iItemFindBonus ) ),
	RecvPropBool( RECVINFO( m_bShieldEquipped ) ),
	RecvPropBool( RECVINFO( m_bParachuteEquipped ) ),
	RecvPropInt( RECVINFO( m_iNextMeleeCrit ) ),
	RecvPropInt( RECVINFO( m_iDecapitations ) ),
	RecvPropInt( RECVINFO( m_iRevengeCrits ) ),
	RecvPropInt( RECVINFO( m_iDisguiseBody ) ),
	RecvPropEHandle( RECVINFO( m_hCarriedObject ) ),
	RecvPropBool( RECVINFO( m_bCarryingObject ) ),
	RecvPropFloat( RECVINFO( m_flNextNoiseMakerTime ) ),
	RecvPropInt( RECVINFO( m_iSpawnRoomTouchCount ) ),
	RecvPropInt( RECVINFO( m_iKillCountSinceLastDeploy ) ),
	RecvPropFloat( RECVINFO( m_flFirstPrimaryAttack ) ),

	//Scout
	RecvPropFloat( RECVINFO( m_flEnergyDrinkMeter) ),
	RecvPropFloat( RECVINFO( m_flHypeMeter) ),

	// Demoman
	RecvPropFloat( RECVINFO( m_flChargeMeter) ),

	// Spy.
	RecvPropTime( RECVINFO( m_flInvisChangeCompleteTime ) ),
	RecvPropInt( RECVINFO( m_nDisguiseTeam ) ),
	RecvPropInt( RECVINFO( m_nDisguiseClass ) ),
	RecvPropInt( RECVINFO( m_nDisguiseSkinOverride ) ),
	RecvPropInt( RECVINFO( m_nMaskClass ) ),
	RecvPropInt( RECVINFO( m_iDisguiseTargetIndex ) ),
	RecvPropInt( RECVINFO( m_iDisguiseHealth ) ),
	RecvPropBool( RECVINFO( m_bFeignDeathReady ) ),
	RecvPropEHandle( RECVINFO( m_hDisguiseWeapon ) ),
	RecvPropInt( RECVINFO( m_nTeamTeleporterUsed ) ),
	RecvPropFloat( RECVINFO( m_flCloakMeter ) ),
	RecvPropFloat( RECVINFO( m_flSpyTranqBuffDuration ) ),

	// Local Data.
	RecvPropDataTable( "tfsharedlocaldata", 0, 0, &REFERENCE_RECV_TABLE(DT_TFPlayerSharedLocal) ),
	RecvPropDataTable( RECVINFO_DT(m_ConditionList),0, &REFERENCE_RECV_TABLE(DT_TFPlayerConditionListExclusive) ),

	RecvPropInt( RECVINFO( m_iTauntIndex ) ),
	RecvPropInt( RECVINFO( m_iTauntConcept ) ),

	RecvPropInt( RECVINFO( m_nPlayerCondEx ) ),
	RecvPropInt( RECVINFO( m_iStunIndex ) ),

	RecvPropInt( RECVINFO( m_nHalloweenBombHeadStage ) ),

	RecvPropInt( RECVINFO( m_nPlayerCondEx2 ) ),
	RecvPropInt( RECVINFO( m_nPlayerCondEx3 ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_nStreaks ), RecvPropInt( RECVINFO( m_nStreaks[0] ) ) ),
	RecvPropInt( RECVINFO( m_unTauntSourceItemID_Low ) ),
	RecvPropInt( RECVINFO( m_unTauntSourceItemID_High ) ),
	RecvPropFloat( RECVINFO( m_flRuneCharge ) ),
#ifdef STAGING_ONLY
	RecvPropFloat( RECVINFO( m_flSpaceJumpCharge ) ),
#endif
	RecvPropBool( RECVINFO( m_bHasPasstimeBall ) ),
	RecvPropBool( RECVINFO( m_bIsTargetedForPasstimePass ) ),
	RecvPropEHandle( RECVINFO( m_hPasstimePassTarget ) ),
	RecvPropFloat( RECVINFO( m_askForBallTime ) ),
	RecvPropBool( RECVINFO( m_bKingRuneBuffActive ) ),

	RecvPropUtlVectorDataTable( m_ConditionData, TF_COND_LAST, DT_TFPlayerConditionSource ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA_NO_BASE( CTFPlayerShared )
	DEFINE_PRED_FIELD( m_nPlayerState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nPlayerCond, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flCloakMeter, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flRageMeter, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bRageDraining, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flNextRageEarnTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flEnergyDrinkMeter, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flHypeMeter, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flChargeMeter, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bJumping, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iAirDash, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nAirDucked, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flDuckTimer, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flInvisChangeCompleteTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nDisguiseTeam, FIELD_INTEGER, FTYPEDESC_INSENDTABLE  ),
	DEFINE_PRED_FIELD( m_nDisguiseClass, FIELD_INTEGER, FTYPEDESC_INSENDTABLE  ),
	DEFINE_PRED_FIELD( m_nDisguiseSkinOverride, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nMaskClass, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nDesiredDisguiseTeam, FIELD_INTEGER, FTYPEDESC_INSENDTABLE  ),
	DEFINE_PRED_FIELD( m_nDesiredDisguiseClass, FIELD_INTEGER, FTYPEDESC_INSENDTABLE  ),
	DEFINE_PRED_FIELD( m_bLastDisguisedAsOwnTeam, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE  ),
	DEFINE_PRED_FIELD( m_bFeignDeathReady, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nPlayerCondEx, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nPlayerCondEx2, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nPlayerCondEx3, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
//	DEFINE_PRED_FIELD( m_hDisguiseWeapon, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),
	DEFINE_FIELD( m_flDisguiseCompleteTime, FIELD_FLOAT ),
	DEFINE_PRED_FIELD( m_bHasPasstimeBall, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bIsTargetedForPasstimePass, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ), // does this belong here?
	DEFINE_PRED_FIELD( m_askForBallTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

// Server specific.
#else

BEGIN_SEND_TABLE_NOBASE( localplayerscoring_t, DT_TFPlayerScoringDataExclusive )
	SendPropInt( SENDINFO( m_iCaptures ), 10, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iDefenses ), 10, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iKills ), 10, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iDeaths ), 10, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iSuicides ), 10, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iDominations ), 10, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iRevenge ), 10, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iBuildingsBuilt ), 10, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iBuildingsDestroyed ), 10, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iHeadshots ), 10, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iBackstabs ), 10, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iHealPoints ), 20, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iInvulns ), 10, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iTeleports ), 10, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iDamageDone ), 20, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iCrits ), 10, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iResupplyPoints ), 10, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iKillAssists ), 12, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iBonusPoints ), 10, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iPoints ), 10, SPROP_UNSIGNED ),
END_SEND_TABLE()

EXTERN_SEND_TABLE(DT_TFPlayerConditionListExclusive);
BEGIN_SEND_TABLE_NOBASE( CTFPlayerShared, DT_TFPlayerSharedLocal )
	SendPropInt( SENDINFO( m_nDesiredDisguiseTeam ), 3, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nDesiredDisguiseClass ), 4, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bLastDisguisedAsOwnTeam ) ),
	SendPropTime( SENDINFO( m_flStealthNoAttackExpire ) ),
	SendPropTime( SENDINFO( m_flStealthNextChangeTime ) ),
	SendPropFloat( SENDINFO( m_flRageMeter ), 0, SPROP_NOSCALE, 0.0, 100.0 ),
	SendPropBool( SENDINFO( m_bRageDraining ) ),
	SendPropTime( SENDINFO( m_flNextRageEarnTime ) ),
	SendPropBool( SENDINFO( m_bInUpgradeZone ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_bPlayerDominated ), SendPropBool( SENDINFO_ARRAY( m_bPlayerDominated ) ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_bPlayerDominatingMe ), SendPropBool( SENDINFO_ARRAY( m_bPlayerDominatingMe ) ) ),
	SendPropDataTable( SENDINFO_DT(m_ScoreData), &REFERENCE_SEND_TABLE(DT_TFPlayerScoringDataExclusive) ),
	SendPropDataTable( SENDINFO_DT(m_RoundScoreData), &REFERENCE_SEND_TABLE(DT_TFPlayerScoringDataExclusive) ),
END_SEND_TABLE()

BEGIN_SEND_TABLE_NOBASE( condition_source_t, DT_TFPlayerConditionSource )
	//SendPropInt( SENDINFO( m_nPreventedDamageFromCondition ) ),
	//SendPropFloat( SENDINFO( m_flExpireTime ) ),
	SendPropEHandle( SENDINFO( m_pProvider ) ),
	//SendPropBool( SENDINFO( m_bPrevActive ) ),
END_SEND_TABLE()

BEGIN_SEND_TABLE_NOBASE( CTFPlayerShared, DT_TFPlayerShared )
	SendPropInt( SENDINFO( m_nPlayerCond ), -1, SPROP_VARINT | SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bJumping ), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nNumHealers ), 5, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iCritMult ), 8, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iAirDash ), -1, SPROP_VARINT | SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nAirDucked ), 2, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flDuckTimer )  ),
	SendPropInt( SENDINFO( m_nPlayerState ), Q_log2( TF_STATE_COUNT )+1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iDesiredPlayerClass ), Q_log2( TF_CLASS_COUNT_ALL )+1, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flMovementStunTime )  ),
	SendPropInt( SENDINFO( m_iMovementStunAmount ), 8, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iMovementStunParity ), MOVEMENTSTUN_PARITY_BITS, SPROP_UNSIGNED ),
	SendPropEHandle( SENDINFO( m_hStunner ) ),
	SendPropInt( SENDINFO( m_iStunFlags  ), 12, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nArenaNumChanges ), 5, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bArenaFirstBloodBoost ) ),
	SendPropInt( SENDINFO( m_iWeaponKnockbackID ) ),
	SendPropBool( SENDINFO( m_bLoadoutUnavailable ) ),
	SendPropInt( SENDINFO( m_iItemFindBonus ) ),
	SendPropBool( SENDINFO( m_bShieldEquipped ) ),
	SendPropBool( SENDINFO( m_bParachuteEquipped ) ),
	SendPropInt( SENDINFO( m_iNextMeleeCrit ) ),
	SendPropInt( SENDINFO( m_iDecapitations ), 8, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iRevengeCrits ), 7, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iDisguiseBody ) ),
	SendPropEHandle( SENDINFO( m_hCarriedObject ) ),
	SendPropBool( SENDINFO( m_bCarryingObject ) ),
	SendPropFloat( SENDINFO( m_flNextNoiseMakerTime ) ),
	SendPropInt( SENDINFO( m_iSpawnRoomTouchCount ) ),
	SendPropInt( SENDINFO( m_iKillCountSinceLastDeploy ) ),
	SendPropFloat( SENDINFO( m_flFirstPrimaryAttack ) ),

	//Scout
	SendPropFloat( SENDINFO( m_flEnergyDrinkMeter ), 0, SPROP_NOSCALE, 0.0, 100.0 ),
	SendPropFloat( SENDINFO( m_flHypeMeter ), 0, SPROP_NOSCALE, 0.0, 100.0 ),

	// Demoman
	SendPropFloat( SENDINFO( m_flChargeMeter ), 8, SPROP_NOSCALE, 0.0, 100.0 ),

	// Spy
	SendPropTime( SENDINFO( m_flInvisChangeCompleteTime ) ),
	SendPropInt( SENDINFO( m_nDisguiseTeam ), 3, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nDisguiseClass ), 4, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nDisguiseSkinOverride ), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nMaskClass ), 4, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iDisguiseTargetIndex ), 7, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iDisguiseHealth ), -1, SPROP_VARINT ),
	SendPropBool( SENDINFO( m_bFeignDeathReady ) ),
	SendPropEHandle( SENDINFO( m_hDisguiseWeapon ) ),
	SendPropInt( SENDINFO( m_nTeamTeleporterUsed ), 3, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flCloakMeter ), 16, SPROP_NOSCALE, 0.0, 100.0 ),
	SendPropFloat( SENDINFO( m_flSpyTranqBuffDuration ), 16, SPROP_NOSCALE, 0.0, 100.0 ),
	
	// Local Data.
	SendPropDataTable( "tfsharedlocaldata", 0, &REFERENCE_SEND_TABLE( DT_TFPlayerSharedLocal ), SendProxy_SendLocalDataTable ),	
	SendPropDataTable( SENDINFO_DT(m_ConditionList), &REFERENCE_SEND_TABLE(DT_TFPlayerConditionListExclusive) ),

	SendPropInt( SENDINFO( m_iTauntIndex ), 8, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iTauntConcept ), 8, SPROP_UNSIGNED ),

	SendPropInt( SENDINFO( m_nPlayerCondEx ), -1, SPROP_VARINT | SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iStunIndex ), 8 ),
	
	SendPropInt( SENDINFO( m_nHalloweenBombHeadStage ), 2, SPROP_UNSIGNED ),

	SendPropInt( SENDINFO( m_nPlayerCondEx2 ), -1, SPROP_VARINT | SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nPlayerCondEx3 ), -1, SPROP_VARINT | SPROP_UNSIGNED ),

	SendPropArray3( SENDINFO_ARRAY3( m_nStreaks ), SendPropInt( SENDINFO_ARRAY( m_nStreaks ) ) ),
	SendPropInt( SENDINFO( m_unTauntSourceItemID_Low ), -1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_unTauntSourceItemID_High ), -1, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flRuneCharge ), 8, 0, 0.0, 100.0 ),
#ifdef STAGING_ONLY
	SendPropFloat( SENDINFO( m_flSpaceJumpCharge ), 8, 0, 0.0, 100.0 ),
#endif
	SendPropBool( SENDINFO( m_bHasPasstimeBall ) ),
	SendPropBool( SENDINFO( m_bIsTargetedForPasstimePass ) ),
	SendPropEHandle( SENDINFO( m_hPasstimePassTarget ) ),
	SendPropFloat( SENDINFO( m_askForBallTime ) ),
	SendPropBool( SENDINFO( m_bKingRuneBuffActive ) ),

	SendPropUtlVectorDataTable( m_ConditionData, TF_COND_LAST, DT_TFPlayerConditionSource ),
END_SEND_TABLE()

#endif

extern void HandleRageGain( CTFPlayer *pPlayer, unsigned int iRequiredBuffFlags, float flDamage, float fInverseRageGainScale );

CTFWearableDemoShield* GetEquippedDemoShield( CTFPlayer * pPlayer )
{
	// Loop through our wearables in search of a shield
	for ( int i=0; i<pPlayer->GetNumWearables(); ++i )
	{
		CTFWearableDemoShield *pWearableShield = dynamic_cast<CTFWearableDemoShield*>( pPlayer->GetWearable( i ) );
		if ( pWearableShield )
		{
			return pWearableShield;
		}
	}

	return NULL;
}

CTFPlayer *GetRuneCarrier( RuneTypes_t type, int iTeam = TEAM_ANY )
{
	for( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
	{
		CTFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( iPlayerIndex ) );
		if ( !pPlayer )
			continue;

		if ( iTeam != TEAM_ANY && pPlayer->GetTeamNumber() != iTeam )
			continue;

		if ( pPlayer->m_Shared.GetCarryingRuneType() == type )
		{
			return pPlayer;
		}
	}

	return NULL;
}

// --------------------------------------------------------------------------------------------------- //
// Shared CTFPlayer implementation.
// --------------------------------------------------------------------------------------------------- //

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::HasCampaignMedal( int iMedal )
{ 
	return ( ( m_iCampaignMedals & iMedal ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::IsAllowedToTaunt( void )
{
	if ( !IsAlive() )
		return false;

	// Check to see if we can taunt again!
	if ( m_Shared.InCond( TF_COND_TAUNTING ) )
		return false;

	if ( m_Shared.InCond( TF_COND_HALLOWEEN_KART ) )
		return false;

	if ( m_Shared.InCond( TF_COND_HALLOWEEN_GHOST_MODE ) )
		return false;

	// Can't taunt while charging.
	if ( m_Shared.InCond( TF_COND_SHIELD_CHARGE ) )
		return false;

	if ( m_Shared.InCond( TF_COND_COMPETITIVE_LOSER ) )
		return false;

	if ( IsLerpingFOV() )
		return false;

	// Check for things that prevent taunting
	if ( ShouldStopTaunting() )
		return false;

	// Check to see if we are on the ground.
	if ( GetGroundEntity() == NULL && !m_Shared.InCond( TF_COND_HALLOWEEN_KART ) )
		return false;

	CTFWeaponBase *pActiveWeapon = m_Shared.GetActiveTFWeapon();
	if ( pActiveWeapon )
	{
		if ( !pActiveWeapon->OwnerCanTaunt() )
			return false;

		// ignore taunt key if one of these if active weapon
		if ( pActiveWeapon->GetWeaponID() == TF_WEAPON_PDA_ENGINEER_BUILD 
			 ||	pActiveWeapon->GetWeaponID() == TF_WEAPON_PDA_ENGINEER_DESTROY )
			return false;
	}

	// can't taunt while carrying an object
	if ( m_Shared.IsCarryingObject() )
		return false;

	// Can't taunt if hooked into a player
	if ( m_Shared.InCond( TF_COND_GRAPPLED_TO_PLAYER ) )
		return false;

	if ( IsPlayerClass( TF_CLASS_SCOUT ) )
	{
		if ( pActiveWeapon && pActiveWeapon->GetWeaponID() == TF_WEAPON_LUNCHBOX )
		{
			//Scouts can't drink while they're already phasing.
			if ( m_Shared.InCond( TF_COND_ENERGY_BUFF ) || m_Shared.InCond( TF_COND_PHASE ) )
				return false;
			
			// Or if their energy drink meter isn't refilled
			if ( m_Shared.GetScoutEnergyDrinkMeter() < 100 )
				return false;

			//They can't drink the default (phase) item while carrying a flag
			pActiveWeapon = m_Shared.GetActiveTFWeapon();
			if ( pActiveWeapon && pActiveWeapon->GetWeaponID() == TF_WEAPON_LUNCHBOX )
			{
				CTFLunchBox *pLunchbox = (CTFLunchBox*)pActiveWeapon;

				if ( ( pLunchbox->GetLunchboxType() == LUNCHBOX_STANDARD ) || ( pLunchbox->GetLunchboxType() == LUNCHBOX_STANDARD_ROBO ) )
				{
					if ( !TFGameRules()->IsMannVsMachineMode() && HasItem() )
						return false;
				}
			}
		}
	}

	if ( IsPlayerClass( TF_CLASS_SPY ) )
	{
		if ( m_Shared.IsStealthed() || m_Shared.InCond( TF_COND_STEALTHED_BLINK ) || 
			 m_Shared.InCond( TF_COND_DISGUISED ) || m_Shared.InCond( TF_COND_DISGUISING ) )
		{
			return false;
		}
	}

	return true;
}


// --------------------------------------------------------------------------------------------------- //
// CTFPlayerShared implementation.
// --------------------------------------------------------------------------------------------------- //

CTFPlayerShared::CTFPlayerShared()
{
	// If you hit this assert, CONGRATULATIONS!  You've added a condition that has gone 
	// beyond the amount of bits we network for conditions.  Take a look at the pattern
	// of m_nPlayerCond, m_nPlayerCondEx, m_nPlayerCondEx2, and m_nPlayerCondEx3 to get more bits.
	// This pattern is as such to preserve replays.
	// Don't forget to add an m_nOldCond* and m_nForceCond*
	COMPILE_TIME_ASSERT( TF_COND_LAST < (32 + 32 + 32 + 32) );

	m_nPlayerState.Set( TF_STATE_WELCOME );
	m_bJumping = false;
	m_iAirDash = 0;
	m_nAirDucked = 0;
	m_flDuckTimer = 0.0f;
	m_flStealthNoAttackExpire = 0.0f;
	m_flStealthNextChangeTime = 0.0f;
	m_iCritMult = 0;
	m_flInvisibility = 0.0f;
	m_flPrevInvisibility = 0.f;
	m_flTmpDamageBonusAmount = 1.0f;

	m_bFeignDeathReady = false;

	m_fCloakConsumeRate = tf_spy_cloak_consume_rate.GetFloat();
	m_fCloakRegenRate = tf_spy_cloak_regen_rate.GetFloat();

	m_fEnergyDrinkConsumeRate = tf_scout_energydrink_consume_rate.GetFloat();
	m_fEnergyDrinkRegenRate = tf_scout_energydrink_regen_rate.GetFloat();

	m_bMotionCloak = false;

	m_hStunner = NULL;
	m_iStunFlags = 0;

	m_hAssist = NULL;

	m_bLastDisguisedAsOwnTeam = false;

	m_bRageDraining = false;
	m_bInUpgradeZone = false;
	m_bPhaseFXOn = false;
	ResetRageBuffs();
	
	m_iPhaseDamage = 0;

	Q_memset(m_pPhaseTrail, 0, sizeof(m_pPhaseTrail));
	
	m_iWeaponKnockbackID = -1;

	m_bLoadoutUnavailable = false;

	m_nMaskClass = 0;

	m_iItemFindBonus = 0;

	m_nTeamTeleporterUsed = TEAM_UNASSIGNED;

	m_bShieldEquipped = false;
	m_bPostShieldCharge = false;
	m_iNextMeleeCrit = 0;

	m_bParachuteEquipped = false;

	m_iDecapitations = m_iOldDecapitations = 0;
	m_iOldKillStreak = 0;
	m_iOldKillStreakWepSlot = 0;

	m_flNextNoiseMakerTime = 0;
	m_iSpawnRoomTouchCount = 0;

	m_iKillCountSinceLastDeploy = 0;
	m_flFirstPrimaryAttack = 0.0f;

#ifdef GAME_DLL
	m_flBestOverhealDecayMult = -1;
	m_hPeeAttacker = NULL;

	m_flHealedPerSecondTimer = -1000;
	m_bPulseRadiusHeal = false;

	m_flRadiusCurrencyCollectionTime = 0;
	m_flRadiusSpyScanTime = 0;

	m_flCloakStartTime = -1.0f;

	ListenForGameEvent( "player_disconnect" );
#else
	m_pWheelEffect = NULL;
	m_angVehicleMoveAngles = QAngle( 0.f, 0.f, 0.f );
	m_angVehicleMovePitchLast = 0.0f;
	// Save Prediction value
	m_bPreKartPredictionState = cl_predict->GetBool();
	m_hKartParachuteEntity = NULL;
#endif

	m_nForceConditions = 0;
	m_nForceConditionsEx = 0;
	m_nForceConditionsEx2 = 0;
	m_nForceConditionsEx3 = 0;

	m_flChargeEndTime = -1000;
	m_flLastChargeTime = -1000;
	m_flLastNoChargeTime = 0;
	m_bChargeGlowing = false;

	m_bChargeOffSounded = false;

	m_bBiteEffectWasApplied = false;

	m_flLastMovementStunChange = 0;
	m_bStunNeedsFadeOut = false;

	m_flChargeMeter = 100;
	m_flEnergyDrinkMeter = 0;
	m_flHypeMeter = 0;

	m_bCarryingObject = false;
	m_hCarriedObject = NULL;

	m_iStunIndex = -1;
	m_flLastNoMovementTime = -1.f;
	m_flRuneCharge = 0.f;
#ifdef STAGING_ONLY
	m_flSpaceJumpCharge = 100.0f;
	m_flSpyTranqBuffDuration = 0.0f;
#endif
	m_iPasstimeThrowAnimState = PASSTIME_THROW_ANIM_NONE;
	m_bHasPasstimeBall = false;
	m_bIsTargetedForPasstimePass = false;
	m_askForBallTime = 0.0f;

	// make sure we have all conditions in the list
	m_ConditionData.EnsureCount( TF_COND_LAST );
}

void CTFPlayerShared::Init( CTFPlayer *pPlayer )
{
	m_pOuter = pPlayer;

	m_flNextBurningSound = 0;

	m_bArenaFirstBloodBoost = false;
	m_iStunAnimState = STUN_ANIM_NONE;
	m_iPhaseDamage = 0;
	m_iWeaponKnockbackID = -1;
	m_hStunner = NULL;

	m_iPasstimeThrowAnimState = PASSTIME_THROW_ANIM_NONE;
	m_bHasPasstimeBall = false;
	m_bIsTargetedForPasstimePass = false;
	m_askForBallTime = 0.0f;

	m_bMotionCloak = false;

	m_bShieldEquipped = false;
	m_bPostShieldCharge = false;
	m_iNextMeleeCrit = 0;

	m_bParachuteEquipped = false;

	m_iDecapitations = m_iOldDecapitations = 0;
	m_iOldKillStreak = 0;
	m_iOldKillStreakWepSlot = 0;

	SetJumping( false );
	SetAssist( NULL );

	m_flInvulnerabilityRemoveTime = -1;

	SetNextMeleeCrit( MELEE_NOCRIT );

	Spawn();
}

void CTFPlayerShared::ResetRageBuffs( void )
{
	for ( int i = 0; i < kBuffSlot_MAX; i++ )
	{
		m_RageBuffSlots[i].m_iBuffTypeActive = 0;
		m_RageBuffSlots[i].m_iBuffPulseCount = 0;
		m_RageBuffSlots[i].m_flNextBuffPulseTime = 0.0f;
	}
}

void CTFPlayerShared::Spawn( void )
{
#ifdef GAME_DLL
	m_hPeeAttacker = NULL;

	if ( m_bCarryingObject )
	{
		CBaseObject* pObj = GetCarriedObject();
		if ( pObj )
		{
			pObj->DetonateObject();
		}
	}

	m_bCarryingObject = false;
	m_hCarriedObject = NULL;

	m_flRadiusHealCheckTime = 0;
	m_flKingRuneBuffCheckTime = 0.f;

	m_bBiteEffectWasApplied = false;

	m_flNextRocketPackTime = 0.f;
	m_iSpawnRoomTouchCount = 0;

	SetRevengeCrits( 0 );

	m_PlayerStuns.RemoveAll();
	m_iStunIndex = -1;

	m_iPasstimeThrowAnimState = PASSTIME_THROW_ANIM_NONE;
	m_bHasPasstimeBall = false;
	m_bIsTargetedForPasstimePass = false;
	m_askForBallTime = 0.0f;
#else
	m_bSyncingConditions = false;
#endif
	m_bKingRuneBuffActive = false;

	// Reset our assist here incase something happens before we get killed
	// again that checks this (getting slapped with a fish)
	SetAssist( NULL );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template < typename tIntType >
class CConditionVars
{
public:
	CConditionVars( tIntType& nPlayerCond, tIntType& nPlayerCondEx, tIntType& nPlayerCondEx2, tIntType& nPlayerCondEx3, ETFCond eCond )
	{
		if ( eCond >= 96 )
		{
			Assert( eCond < 96 + 32 );
			m_pnCondVar = &nPlayerCondEx3;
			m_nCondBit = eCond - 96;
		}
		else if( eCond >= 64 )
		{
			Assert( eCond < (64 + 32) );
			m_pnCondVar = &nPlayerCondEx2;
			m_nCondBit = eCond - 64;
		}
		else if ( eCond >= 32 )
		{
			Assert( eCond < (32 + 32) );
			m_pnCondVar = &nPlayerCondEx;
			m_nCondBit = eCond - 32;
		}
		else
		{
			m_pnCondVar = &nPlayerCond;
			m_nCondBit = eCond;
		}
	}

	tIntType& CondVar() const
	{
		return *m_pnCondVar;
	}

	int CondBit() const
	{
		return 1 << m_nCondBit;
	}

private:
	tIntType *m_pnCondVar;
	int m_nCondBit;
};

//-----------------------------------------------------------------------------
// Purpose: Add a condition and duration
// duration of PERMANENT_CONDITION means infinite duration
//-----------------------------------------------------------------------------
void CTFPlayerShared::AddCond( ETFCond eCond, float flDuration /* = PERMANENT_CONDITION */, CBaseEntity *pProvider /*= NULL */)
{
	Assert( eCond >= 0 && eCond < TF_COND_LAST );
	Assert( eCond < m_ConditionData.Count() );

	// If we're dead, don't take on any new conditions
	if( !m_pOuter || !m_pOuter->IsAlive() )
	{
		return;
	}

#ifdef CLEINT_DLL
	if ( m_pOuter->IsDormant() )
	{
		return;
	}
#endif

	// sanity check to prevent servers from adding these conditions when they shouldn't
	if ( ( eCond == TF_COND_COMPETITIVE_WINNER ) || ( eCond == TF_COND_COMPETITIVE_LOSER ) )
	{
		if ( TFGameRules() && !TFGameRules()->ShowMatchSummary() )
			return;
	}

	// Which bitfield are we tracking this condition variable in? Which bit within
	// that variable will we track it as?
	CConditionVars<int> cPlayerCond( m_nPlayerCond.m_Value, m_nPlayerCondEx.m_Value, m_nPlayerCondEx2.m_Value, m_nPlayerCondEx3.m_Value, eCond );

	// See if there is an object representation of the condition.
	bool bAddedToExternalConditionList = m_ConditionList.Add( eCond, flDuration, m_pOuter, pProvider );
	if ( !bAddedToExternalConditionList )
	{
		// Set the condition bit for this condition.
		cPlayerCond.CondVar() |= cPlayerCond.CondBit();

		// Flag for gamecode to query
		m_ConditionData[eCond].m_bPrevActive = ( m_ConditionData[eCond].m_flExpireTime != 0.f ) ? true : false;

		if ( flDuration != PERMANENT_CONDITION )
		{
			// if our current condition is permanent or we're trying to set a new
			// time that's less our current time remaining, use our current time instead
			if ( ( m_ConditionData[eCond].m_flExpireTime == PERMANENT_CONDITION ) || 
				 ( flDuration < m_ConditionData[eCond].m_flExpireTime ) )
			{
				flDuration = m_ConditionData[eCond].m_flExpireTime;
			}
		}

		m_ConditionData[eCond].m_flExpireTime = flDuration;
		m_ConditionData[eCond].m_pProvider = pProvider;
		m_ConditionData[ eCond ].m_nPreventedDamageFromCondition = 0;

		OnConditionAdded( eCond );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Forcibly remove a condition
//-----------------------------------------------------------------------------
void CTFPlayerShared::RemoveCond( ETFCond eCond, bool ignore_duration )
{
	Assert( eCond >= 0 && eCond < TF_COND_LAST );
	Assert( eCond < m_ConditionData.Count() );

	if ( !InCond( eCond ) )
		return;

	CConditionVars<int> cPlayerCond( m_nPlayerCond.m_Value, m_nPlayerCondEx.m_Value, m_nPlayerCondEx2.m_Value, m_nPlayerCondEx3.m_Value, eCond );

	// If this variable is handled by the condition list, abort before doing the
	// work for the condition flags.
	if ( m_ConditionList.Remove( eCond, ignore_duration ) )
		return;

	cPlayerCond.CondVar() &= ~cPlayerCond.CondBit();
	OnConditionRemoved( eCond );

	if ( m_ConditionData[ eCond ].m_nPreventedDamageFromCondition )
	{
		IGameEvent *pEvent = gameeventmanager->CreateEvent( "damage_prevented" );
		if ( pEvent )
		{
			pEvent->SetInt( "preventor", m_ConditionData[eCond].m_pProvider ? m_ConditionData[eCond].m_pProvider->entindex() : m_pOuter->entindex() );
			pEvent->SetInt( "victim", m_pOuter->entindex() );
			pEvent->SetInt( "amount", m_ConditionData[ eCond ].m_nPreventedDamageFromCondition );
			pEvent->SetInt( "condition", eCond );

			gameeventmanager->FireEvent( pEvent, true );
		}

		m_ConditionData[ eCond ].m_nPreventedDamageFromCondition = 0;
	}

	m_ConditionData[eCond].m_flExpireTime = 0;
	m_ConditionData[eCond].m_pProvider = NULL;
	m_ConditionData[eCond].m_bPrevActive = false;

}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFPlayerShared::InCond( ETFCond eCond ) const
{
	Assert( eCond >= 0 && eCond < TF_COND_LAST );

	// Old condition system, only used for the first 32 conditions
	if ( eCond < 32 && m_ConditionList.InCond( eCond ) )
		return true;

	CConditionVars<const int> cPlayerCond( m_nPlayerCond.m_Value, m_nPlayerCondEx.m_Value, m_nPlayerCondEx2.m_Value, m_nPlayerCondEx3.m_Value, eCond );
	return (cPlayerCond.CondVar() & cPlayerCond.CondBit()) != 0;
}

//-----------------------------------------------------------------------------
// Purpose: Return whether or not we were in this condition before.
//-----------------------------------------------------------------------------
bool CTFPlayerShared::WasInCond( ETFCond eCond ) const
{
	// I don't know if this actually works for conditions < 32, because we definitely cannot peak into m_ConditionList (back in time).
	// But others think that m_ConditionList is propogated into m_nOldConditions, so just check if you hit the assert. (And then remove the 
	// assert. And this comment).
	Assert( eCond >= 32 && eCond < TF_COND_LAST );

	CConditionVars<const int> cPlayerCond( m_nOldConditions, m_nOldConditionsEx, m_nOldConditionsEx2, m_nOldConditionsEx3, eCond );
	return (cPlayerCond.CondVar() & cPlayerCond.CondBit()) != 0;
}

//-----------------------------------------------------------------------------
// Purpose: Set a bit to force this condition off and then back on next time we sync bits from the server. 
//-----------------------------------------------------------------------------
void CTFPlayerShared::ForceRecondNextSync( ETFCond eCond )
{
	// I don't know if this actually works for conditions < 32. We may need to set this bit in m_ConditionList, too.
	// Please check if you hit the assert. (And then remove the assert. And this comment).
	Assert(eCond >= 32 && eCond < TF_COND_LAST);

	CConditionVars<int> playerCond( m_nForceConditions, m_nForceConditionsEx, m_nForceConditionsEx2, m_nForceConditionsEx3, eCond );
	playerCond.CondVar() |= playerCond.CondBit();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CTFPlayerShared::GetConditionDuration( ETFCond eCond ) const
{
	Assert( eCond >= 0 && eCond < TF_COND_LAST );
	Assert( eCond < m_ConditionData.Count() );

	if ( InCond( eCond ) )
	{
		return m_ConditionData[eCond].m_flExpireTime;
	}
	
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the entity that provided the passed in condition
//-----------------------------------------------------------------------------
CBaseEntity *CTFPlayerShared::GetConditionProvider( ETFCond eCond ) const
{
	Assert( eCond >= 0 && eCond < TF_COND_LAST );
	Assert( eCond < m_ConditionData.Count() );

	CBaseEntity *pProvider = NULL;
	if ( InCond( eCond ) )
	{
		if ( eCond == TF_COND_CRITBOOSTED )
		{
			pProvider = m_ConditionList.GetProvider( eCond );
		}
		else
		{
			pProvider = m_ConditionData[eCond].m_pProvider;
		}
	}

	return pProvider;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the entity that applied this condition to us - for granting an assist when we die
//-----------------------------------------------------------------------------
CBaseEntity *CTFPlayerShared::GetConditionAssistFromVictim( void )
{
	// We only give an assist to one person.  That means this list is order
	// sensitive, so consider how "powerful" an effect is when adding it here.
	static const ETFCond nTrackedConditions[] = 
	{
		TF_COND_URINE,
		TF_COND_MAD_MILK,
		TF_COND_MARKEDFORDEATH,
	};

	CBaseEntity *pProvider = NULL;
	for ( int i = 0; i < SE_ARRAYSIZE( nTrackedConditions ); i++ )
	{
		if ( InCond( nTrackedConditions[i] ) )
		{
			pProvider = GetConditionProvider( nTrackedConditions[i] );
			break;
		}
	}

	return pProvider;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the entity that applied this condition to us - for granting an assist when we kill someone
//-----------------------------------------------------------------------------
CBaseEntity *CTFPlayerShared::GetConditionAssistFromAttacker( void )
{
	// We only give an assist to one person.  That means this list is order
	// sensitive, so consider how "powerful" an effect is when adding it here.
	static const ETFCond nTrackedConditions[] = 
	{
		TF_COND_OFFENSEBUFF,			// Highest priority
		TF_COND_DEFENSEBUFF,
		TF_COND_REGENONDAMAGEBUFF,
		TF_COND_NOHEALINGDAMAGEBUFF,	// Lowest priority
	};

	CBaseEntity *pProvider = NULL;
	for ( int i = 0; i < SE_ARRAYSIZE( nTrackedConditions ); i++ )
	{
		if ( InCond( nTrackedConditions[i] ) )
		{
			CBaseEntity* pPotentialProvider = GetConditionProvider( nTrackedConditions[i] );
			// Check to make sure we're not providing the condition to ourselves
			if( pPotentialProvider != m_pOuter )
			{
				pProvider = pPotentialProvider;
				break;
			}
		}
	}

	return pProvider;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayerShared::DebugPrintConditions( void )
{
#ifndef CLIENT_DLL
	const char *szDll = "Server";
#else
	const char *szDll = "Client";
#endif

	Msg( "( %s ) Conditions for player ( %d )\n", szDll, m_pOuter->entindex() );

	int i;
	int iNumFound = 0;
	for ( i=0;i<TF_COND_LAST;i++ )
	{
		if ( InCond( (ETFCond)i ) )
		{
			if ( m_ConditionData[i].m_flExpireTime == PERMANENT_CONDITION )
			{
				Msg( "( %s ) Condition %d - ( permanent cond )\n", szDll, i );
			}
			else
			{
				Msg( "( %s ) Condition %d - ( %.1f left )\n", szDll, i, m_ConditionData[i].m_flExpireTime );
			}

			iNumFound++;
		}
	}

	if ( iNumFound == 0 )
	{
		Msg( "( %s ) No active conditions\n", szDll );
	}
}

void CTFPlayerShared::InstantlySniperUnzoom( void )
{
	// Unzoom if we are a sniper zoomed!
	if ( m_pOuter->GetPlayerClass()->GetClassIndex() == TF_CLASS_SNIPER )
	{
		CTFWeaponBase *pWpn = m_pOuter->GetActiveTFWeapon();

		if ( pWpn && WeaponID_IsSniperRifle( pWpn->GetWeaponID() ) )
		{
			CTFSniperRifle *pRifle = static_cast<CTFSniperRifle*>( pWpn );
			if ( pRifle->IsZoomed() )
			{
				// Let the rifle clean up conditions and state
				pRifle->ToggleZoom();
				// Slam the FOV right now
				m_pOuter->SetFOV( m_pOuter, 0, 0.0f );
			}
		}
	}
}

#ifdef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnPreDataChanged( void )
{
	m_ConditionList.OnPreDataChanged();

	m_nOldConditions = m_nPlayerCond;
	m_nOldConditionsEx = m_nPlayerCondEx;
	m_nOldConditionsEx2 = m_nPlayerCondEx2;
	m_nOldConditionsEx3 = m_nPlayerCondEx3;
	m_nOldDisguiseClass = GetDisguiseClass();
	m_nOldDisguiseTeam = GetDisguiseTeam();
	m_iOldMovementStunParity = m_iMovementStunParity;

	InvisibilityThink();
	ConditionThink();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnDataChanged( void )
{
	m_ConditionList.OnDataChanged( m_pOuter );

	if ( m_iOldMovementStunParity != m_iMovementStunParity )
	{
		m_flStunFade = gpGlobals->curtime + m_flMovementStunTime;
		m_flStunEnd  = m_flStunFade;
		if ( IsControlStunned() && (m_iStunAnimState == STUN_ANIM_NONE) )
		{
			m_flStunEnd += CONTROL_STUN_ANIM_TIME;
		}

		UpdateLegacyStunSystem();
	}

	// Update conditions from last network change
	SyncConditions( m_nOldConditions, m_nPlayerCond, m_nForceConditions, 0 );
	SyncConditions( m_nOldConditionsEx, m_nPlayerCondEx, m_nForceConditionsEx, 32 );
	SyncConditions( m_nOldConditionsEx2, m_nPlayerCondEx2, m_nForceConditionsEx2, 64 );
	SyncConditions( m_nOldConditionsEx3, m_nPlayerCondEx3, m_nForceConditionsEx3, 96 );

	// Make sure these items are present
	m_nPlayerCond		|= m_nForceConditions;
	m_nPlayerCondEx		|= m_nForceConditionsEx;
	m_nPlayerCondEx2	|= m_nForceConditionsEx2;
	m_nPlayerCondEx3	|= m_nForceConditionsEx3;

	// Clear our force bits now that we've used them.
	m_nForceConditions = 0;
	m_nForceConditionsEx = 0;
	m_nForceConditionsEx2 = 0;
	m_nForceConditionsEx3 = 0;

	if ( m_nOldDisguiseClass != GetDisguiseClass() || m_nOldDisguiseTeam != GetDisguiseTeam() )
	{
		OnDisguiseChanged();
	}

	if ( m_hDisguiseWeapon )
	{
		m_hDisguiseWeapon->UpdateVisibility();
		m_hDisguiseWeapon->UpdateParticleSystems();
	}

	if ( ( IsLoser() || InCond( TF_COND_COMPETITIVE_LOSER ) ) && GetActiveTFWeapon() && !GetActiveTFWeapon()->IsEffectActive( EF_NODRAW ) )
	{
		GetActiveTFWeapon()->SetWeaponVisible( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: check the newly networked conditions for changes
//-----------------------------------------------------------------------------
void CTFPlayerShared::SyncConditions( int nPreviousConditions, int nNewConditions, int nForceConditions, int nBaseCondBit )
{
	if ( nPreviousConditions == nNewConditions )
		return;

	int nCondChanged = nNewConditions ^ nPreviousConditions;
	int nCondAdded = nCondChanged & nNewConditions;
	int nCondRemoved = nCondChanged & nPreviousConditions;
	m_bSyncingConditions = true;

	for ( int i=0;i<32;i++ )
	{
		const int testBit = 1<<i;
		if ( nForceConditions & testBit )
		{
			if ( nPreviousConditions & testBit )
			{
				OnConditionRemoved((ETFCond)(nBaseCondBit + i));
			}
			OnConditionAdded((ETFCond)(nBaseCondBit + i));
		}
		else
		{
			if ( nCondAdded & testBit )
			{
				OnConditionAdded( (ETFCond)(nBaseCondBit + i) );
			}
			else if ( nCondRemoved & testBit )
			{
				OnConditionRemoved( (ETFCond)(nBaseCondBit + i) );
			}
		}
	}
	m_bSyncingConditions = false;
}

#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: Remove any conditions affecting players
//-----------------------------------------------------------------------------
void CTFPlayerShared::RemoveAllCond()
{
	m_ConditionList.RemoveAll();

	int i;
	for ( i=0;i<TF_COND_LAST;i++ )
	{
		if ( InCond( (ETFCond)i ) )
		{
			RemoveCond( (ETFCond)i );
		}
	}

	// Now remove all the rest
	m_nPlayerCond = 0;
	m_nPlayerCondEx = 0;
	m_nPlayerCondEx2 = 0;
	m_nPlayerCondEx3 = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Called on both client and server. Server when we add the bit,
// and client when it receives the new cond bits and finds one added
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnConditionAdded( ETFCond eCond )
{
	switch( eCond )
	{
	case TF_COND_ZOOMED:
		OnAddZoomed();
		break;
	case TF_COND_HEALTH_BUFF:
#ifdef GAME_DLL
		m_flHealFraction = 0;
		m_flDisguiseHealFraction = 0;

		m_flHealedPerSecondTimer = gpGlobals->curtime + 1.0f;
#endif
		break;
	
	case TF_COND_HEALTH_OVERHEALED:
		OnAddOverhealed();
		break;

	case TF_COND_FEIGN_DEATH:
		OnAddFeignDeath();
		break;
	
	case TF_COND_STEALTHED:
	case TF_COND_STEALTHED_USER_BUFF:
		OnAddStealthed();
		break;

	case TF_COND_INVULNERABLE:
	case TF_COND_INVULNERABLE_USER_BUFF:
	case TF_COND_INVULNERABLE_CARD_EFFECT:
		OnAddInvulnerable();
		break;

	case TF_COND_TELEPORTED:
		OnAddTeleported();
		break;

	case TF_COND_BURNING:
		OnAddBurning();
		break;

	case TF_COND_CRITBOOSTED:
		Assert( !"TF_COND_CRITBOOSTED should be handled by the condition list!" );
		break;

	case TF_COND_CRITBOOSTED_DEMO_CHARGE:
		OnAddDemoCharge();
		break;

	// First blood falls through on purpose.
	case TF_COND_CRITBOOSTED_FIRST_BLOOD:
		SetFirstBloodBoosted( true );
	case TF_COND_CRITBOOSTED_PUMPKIN:
	case TF_COND_CRITBOOSTED_USER_BUFF:
	case TF_COND_CRITBOOSTED_BONUS_TIME:
	case TF_COND_CRITBOOSTED_CTF_CAPTURE:
	case TF_COND_CRITBOOSTED_ON_KILL:
	case TF_COND_CRITBOOSTED_RAGE_BUFF:
	case TF_COND_SNIPERCHARGE_RAGE_BUFF:
	case TF_COND_CRITBOOSTED_CARD_EFFECT:
	case TF_COND_CRITBOOSTED_RUNE_TEMP:
		OnAddCritBoost();
		break;

	case TF_COND_SODAPOPPER_HYPE:
		OnAddSodaPopperHype();
		break;

	case TF_COND_DISGUISING:
		OnAddDisguising();
		break;

	case TF_COND_DISGUISED:
		OnAddDisguised();
		break;

	case TF_COND_URINE:
		OnAddUrine();
		break;

	case TF_COND_MARKEDFORDEATH:
		OnAddMarkedForDeath();
		break;

	case TF_COND_BLEEDING:
		OnAddBleeding();
		break;

	case TF_COND_TAUNTING:
		OnAddTaunting();
		break;

	case TF_COND_STUNNED:
		OnAddStunned();
		break;

	case TF_COND_PHASE:
		OnAddPhase();
		break;

	case TF_COND_OFFENSEBUFF:
		OnAddOffenseBuff();
		break;

	case TF_COND_DEFENSEBUFF:
	case TF_COND_DEFENSEBUFF_NO_CRIT_BLOCK:
	case TF_COND_DEFENSEBUFF_HIGH:
		OnAddDefenseBuff();
		break;

	case TF_COND_REGENONDAMAGEBUFF:
		OnAddOffenseHealthRegenBuff();
		break;

	case TF_COND_NOHEALINGDAMAGEBUFF:
		OnAddNoHealingDamageBuff();
		break;

	case TF_COND_SHIELD_CHARGE:
		OnAddShieldCharge();
		break;

	case TF_COND_DEMO_BUFF:
		OnAddDemoBuff();
		break;

	case TF_COND_ENERGY_BUFF:
		OnAddEnergyDrinkBuff();
		break;
		
	case TF_COND_RADIUSHEAL:
		OnAddRadiusHeal();
		break;

	case TF_COND_MEGAHEAL:
		OnAddMegaHeal();
		break;

	case TF_COND_MAD_MILK:
		OnAddMadMilk();
		break;

	case TF_COND_SPEED_BOOST:				OnAddSpeedBoost( false );		break;
#ifdef STAGING_ONLY //STAGING_ENGY
	case TF_COND_NO_COMBAT_SPEED_BOOST:		OnAddSpeedBoost( true );		break;
#endif

	case TF_COND_SAPPED:
		OnAddSapped();
		break;

	case TF_COND_REPROGRAMMED:
		OnAddReprogrammed();
		break;

	case TF_COND_PASSTIME_PENALTY_DEBUFF:
	case TF_COND_MARKEDFORDEATH_SILENT:
		OnAddMarkedForDeathSilent();
		break;

	case TF_COND_DISGUISED_AS_DISPENSER:
		OnAddDisguisedAsDispenser();
		break;

	case TF_COND_HALLOWEEN_BOMB_HEAD:
		OnAddHalloweenBombHead();
		break;

	case TF_COND_HALLOWEEN_THRILLER:
		OnAddHalloweenThriller();
		break;

	case TF_COND_RADIUSHEAL_ON_DAMAGE:
		OnAddRadiusHealOnDamage();
		break;

	case TF_COND_MEDIGUN_UBER_BULLET_RESIST:
		OnAddMedEffectUberBulletResist();
		break;

	case TF_COND_MEDIGUN_UBER_BLAST_RESIST:
		OnAddMedEffectUberBlastResist();
		break;

	case TF_COND_MEDIGUN_UBER_FIRE_RESIST:
		OnAddMedEffectUberFireResist();
		break;

	case TF_COND_MEDIGUN_SMALL_BULLET_RESIST:
		OnAddMedEffectSmallBulletResist();
		break;

	case TF_COND_MEDIGUN_SMALL_BLAST_RESIST:
		OnAddMedEffectSmallBlastResist();
		break;

	case TF_COND_MEDIGUN_SMALL_FIRE_RESIST:
		OnAddMedEffectSmallFireResist();
		break;
	
	case TF_COND_STEALTHED_USER_BUFF_FADING:
		OnAddStealthedUserBuffFade();
		break;

	case TF_COND_BULLET_IMMUNE:
		OnAddBulletImmune();
		break;

	case TF_COND_BLAST_IMMUNE:
		OnAddBlastImmune();
		break;

	case TF_COND_FIRE_IMMUNE:
		OnAddFireImmune();
		break;

	case TF_COND_MVM_BOT_STUN_RADIOWAVE:
		OnAddMVMBotRadiowave();
		break;

	case TF_COND_HALLOWEEN_SPEED_BOOST:
		OnAddHalloweenSpeedBoost();
		break;
	
	case TF_COND_HALLOWEEN_QUICK_HEAL:
		OnAddHalloweenQuickHeal();
		break;

	case TF_COND_HALLOWEEN_GIANT:
		OnAddHalloweenGiant();
		break;

	case TF_COND_HALLOWEEN_TINY:
		OnAddHalloweenTiny();
		break;

	case TF_COND_HALLOWEEN_GHOST_MODE:
		OnAddHalloweenGhostMode();
		break;

	case TF_COND_PARACHUTE_DEPLOYED:
		OnAddCondParachute();
		break;

	case TF_COND_HALLOWEEN_KART_DASH:
		OnAddHalloweenKartDash();
		break;
		
	case TF_COND_HALLOWEEN_KART:
		OnAddHalloweenKart();
		break;
		
	case TF_COND_BALLOON_HEAD:
		OnAddBalloonHead();
		break;

	case TF_COND_MELEE_ONLY:
		OnAddMeleeOnly();
		break;

	case TF_COND_SWIMMING_CURSE:
		OnAddSwimmingCurse();
		break;

	case TF_COND_HALLOWEEN_KART_CAGE:
		OnAddHalloweenKartCage();
		break;

	case TF_COND_RUNE_RESIST:
		OnAddRuneResist();
		break;

	case TF_COND_GRAPPLINGHOOK_LATCHED:
		OnAddGrapplingHookLatched();
		break;

	case TF_COND_PASSTIME_INTERCEPTION:
		OnAddPasstimeInterception();
		break;

	case TF_COND_RUNE_PLAGUE:
		OnAddRunePlague();
		break;

	case TF_COND_PLAGUE:
		OnAddPlague();
		break;

	case TF_COND_PURGATORY:
		OnAddInPurgatory();
		break;

	case TF_COND_COMPETITIVE_WINNER:
		OnAddCompetitiveWinner();
		break;

	case TF_COND_COMPETITIVE_LOSER:
		OnAddCompetitiveLoser();
		break;

#ifdef STAGING_ONLY
	case TF_COND_SPY_CLASS_STEAL:
		OnAddCondSpyClassSteal();
		break;

	case TF_COND_TRANQ_MARKED:
		OnAddTranqMark();
		break;
/*
	case TF_COND_SPACE_GRAVITY:
		OnAddSpaceGravity();
		break;
	case TF_COND_SELF_CONC:
		OnAddSelfConc();
		break;
*/
	case TF_COND_ROCKETPACK:
		OnAddRocketPack();
		break;

	case TF_COND_STEALTHED_PHASE:
		OnAddStealthedPhase();
		break;

	case TF_COND_CLIP_OVERLOAD:
		OnAddClipOverload();
		break;

	case TF_COND_KING_BUFFED:
		OnAddKingBuff();
		break;


#endif // STAGING_ONLY

	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called on both client and server. Server when we remove the bit,
// and client when it receives the new cond bits and finds one removed
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnConditionRemoved( ETFCond eCond )
{
	switch( eCond )
	{
	case TF_COND_ZOOMED:
		OnRemoveZoomed();
		break;

	case TF_COND_BURNING:
		OnRemoveBurning();
		break;

	case TF_COND_CRITBOOSTED:
		Assert( !"TF_COND_CRITBOOSTED should be handled by the condition list!" );
		break;

	case TF_COND_CRITBOOSTED_DEMO_CHARGE:
		OnRemoveDemoCharge();
		break;

	// First blood falls through on purpose.
	case TF_COND_CRITBOOSTED_FIRST_BLOOD:
		SetFirstBloodBoosted( false );
	case TF_COND_CRITBOOSTED_PUMPKIN:
	case TF_COND_CRITBOOSTED_USER_BUFF:
	case TF_COND_CRITBOOSTED_BONUS_TIME:
	case TF_COND_CRITBOOSTED_CTF_CAPTURE:
	case TF_COND_CRITBOOSTED_ON_KILL:
	case TF_COND_CRITBOOSTED_RAGE_BUFF:
	case TF_COND_SNIPERCHARGE_RAGE_BUFF:
	case TF_COND_CRITBOOSTED_CARD_EFFECT:
	case TF_COND_CRITBOOSTED_RUNE_TEMP:
		OnRemoveCritBoost();
		break;

	case TF_COND_SODAPOPPER_HYPE:
		OnRemoveSodaPopperHype();
		break;

	case TF_COND_TMPDAMAGEBONUS:
		OnRemoveTmpDamageBonus();
		break;

	case TF_COND_HEALTH_BUFF:
#ifdef GAME_DLL
		m_flHealFraction = 0;
		m_flDisguiseHealFraction = 0;
#endif
		break;

	case TF_COND_HEALTH_OVERHEALED:
		OnRemoveOverhealed();
		break;

	case TF_COND_FEIGN_DEATH:
		OnRemoveFeignDeath();
		break;

	case TF_COND_STEALTHED:
	case TF_COND_STEALTHED_USER_BUFF:
		OnRemoveStealthed();
		break;

	case TF_COND_DISGUISED:
		OnRemoveDisguised();
		break;

	case TF_COND_DISGUISING:
		OnRemoveDisguising();
		break;

	case TF_COND_INVULNERABLE:
	case TF_COND_INVULNERABLE_USER_BUFF:
	case TF_COND_INVULNERABLE_CARD_EFFECT:
		OnRemoveInvulnerable();
		break;

	case TF_COND_TELEPORTED:
		OnRemoveTeleported();
		break;

	case TF_COND_STUNNED:
		OnRemoveStunned();
		break;

	case TF_COND_PHASE:
		OnRemovePhase();
		break;

	case TF_COND_URINE:
		OnRemoveUrine();
		break;

	case TF_COND_MARKEDFORDEATH:
		OnRemoveMarkedForDeath();
		break;

	case TF_COND_BLEEDING:
		OnRemoveBleeding();
		break;

	case TF_COND_INVULNERABLE_WEARINGOFF:
		OnRemoveInvulnerableWearingOff();
		break;	

	case TF_COND_OFFENSEBUFF:
		OnRemoveOffenseBuff();
		break;

	case TF_COND_DEFENSEBUFF:
	case TF_COND_DEFENSEBUFF_NO_CRIT_BLOCK:
	case TF_COND_DEFENSEBUFF_HIGH:
		OnRemoveDefenseBuff();
		break;

	case TF_COND_REGENONDAMAGEBUFF:
		OnRemoveOffenseHealthRegenBuff();
		break;

	case TF_COND_NOHEALINGDAMAGEBUFF:
		OnRemoveNoHealingDamageBuff();
		break;

	case TF_COND_SHIELD_CHARGE:
		OnRemoveShieldCharge();
		break;

	case TF_COND_DEMO_BUFF:
		OnRemoveDemoBuff();
		break;

	case TF_COND_ENERGY_BUFF:
		OnRemoveEnergyDrinkBuff();
		break;
		
	case TF_COND_RADIUSHEAL:
		OnRemoveRadiusHeal();
		break;

	case TF_COND_MEGAHEAL:
		OnRemoveMegaHeal();
		break;

	case TF_COND_MAD_MILK:
		OnRemoveMadMilk();
		break;

	case TF_COND_TAUNTING:
		OnRemoveTaunting();
		break;

	case TF_COND_SPEED_BOOST:					OnRemoveSpeedBoost( false );		break;
#ifdef STAGING_ONLY
	case TF_COND_NO_COMBAT_SPEED_BOOST:			OnRemoveSpeedBoost( true );			break;
#endif
		

	case TF_COND_SAPPED:
		OnRemoveSapped();
		break;

	case TF_COND_REPROGRAMMED:
		OnRemoveReprogrammed();
		break;

	case TF_COND_PASSTIME_PENALTY_DEBUFF:
	case TF_COND_MARKEDFORDEATH_SILENT:
		OnRemoveMarkedForDeathSilent();
		break;

	case TF_COND_DISGUISED_AS_DISPENSER:
		OnRemoveDisguisedAsDispenser();
		break;

	case TF_COND_HALLOWEEN_BOMB_HEAD:
		OnRemoveHalloweenBombHead();
		break;

	case TF_COND_HALLOWEEN_THRILLER:
		OnRemoveHalloweenThriller();
		break;

	case TF_COND_RADIUSHEAL_ON_DAMAGE:
		OnRemoveRadiusHealOnDamage();
		break;

	case TF_COND_MEDIGUN_UBER_BULLET_RESIST:
		OnRemoveMedEffectUberBulletResist();
		break;

	case TF_COND_MEDIGUN_UBER_BLAST_RESIST:
		OnRemoveMedEffectUberBlastResist();
		break;

	case TF_COND_MEDIGUN_UBER_FIRE_RESIST:
		OnRemoveMedEffectUberFireResist();
		break;

	case TF_COND_MEDIGUN_SMALL_BULLET_RESIST:
		OnRemoveMedEffectSmallBulletResist();
		break;

	case TF_COND_MEDIGUN_SMALL_BLAST_RESIST:
		OnRemoveMedEffectSmallBlastResist();
		break;

	case TF_COND_MEDIGUN_SMALL_FIRE_RESIST:
		OnRemoveMedEffectSmallFireResist();
		break;

	case TF_COND_STEALTHED_USER_BUFF_FADING:
		OnRemoveStealthedUserBuffFade();
		break;

	case TF_COND_BULLET_IMMUNE:
		OnRemoveBulletImmune();
		break;

	case TF_COND_BLAST_IMMUNE:
		OnRemoveBlastImmune();
		break;

	case TF_COND_FIRE_IMMUNE:
		OnRemoveFireImmune();
		break;

	case TF_COND_MVM_BOT_STUN_RADIOWAVE:
		OnRemoveMVMBotRadiowave();
		break;

	case TF_COND_HALLOWEEN_SPEED_BOOST:
		OnRemoveHalloweenSpeedBoost();
		break;

	case TF_COND_HALLOWEEN_QUICK_HEAL:
		OnRemoveHalloweenQuickHeal();
		break;

	case TF_COND_HALLOWEEN_GIANT:
		OnRemoveHalloweenGiant();
		break;

	case TF_COND_HALLOWEEN_TINY:
		OnRemoveHalloweenTiny();
		break;

	case TF_COND_HALLOWEEN_GHOST_MODE:
		OnRemoveHalloweenGhostMode();
		break;

	case TF_COND_PARACHUTE_DEPLOYED:
		OnRemoveCondParachute();
		break;

	case TF_COND_HALLOWEEN_KART_DASH:
		OnRemoveHalloweenKartDash();
		break;
		
	case TF_COND_HALLOWEEN_KART:
		OnRemoveHalloweenKart();
		break;
		
	case TF_COND_BALLOON_HEAD:
		OnRemoveBalloonHead();
		break;

	case TF_COND_MELEE_ONLY:
		OnRemoveMeleeOnly();
		break;

	case TF_COND_SWIMMING_CURSE:
		OnRemoveSwimmingCurse();				

	case TF_COND_HALLOWEEN_KART_CAGE:
		OnRemoveHalloweenKartCage();
		break;

	case TF_COND_RUNE_RESIST:
		OnRemoveRuneResist();
		break;

	case TF_COND_GRAPPLINGHOOK_LATCHED:
		OnRemoveGrapplingHookLatched();
		break;

	case TF_COND_PASSTIME_INTERCEPTION:
		OnRemovePasstimeInterception();
		break;

	case TF_COND_RUNE_PLAGUE:
		OnRemoveRunePlague();
		break;

	case TF_COND_PLAGUE:
		OnRemovePlague();
		break;

	case TF_COND_PURGATORY:
		OnRemoveInPurgatory();
		break;

	case TF_COND_RUNE_KING:
		OnRemoveRuneKing();
		break;

	case TF_COND_KING_BUFFED:
		OnRemoveKingBuff();
		break;

	case TF_COND_RUNE_SUPERNOVA:
		OnRemoveRuneSupernova();
		break;

	case TF_COND_COMPETITIVE_WINNER:
		OnRemoveCompetitiveWinner();
		break;

	case TF_COND_COMPETITIVE_LOSER:
		OnRemoveCompetitiveLoser();
		break;

#ifdef STAGING_ONLY
	case TF_COND_SPY_CLASS_STEAL:
		OnRemoveCondSpyClassSteal();
		break;

	case TF_COND_TRANQ_MARKED:
		OnRemoveTranqMark();
		break;
/*
	case TF_COND_SPACE_GRAVITY:
		OnRemoveSpaceGravity();
		break;
	case TF_COND_SELF_CONC:
		OnRemoveSelfConc();
		break;
*/
	case TF_COND_ROCKETPACK:
		OnRemoveRocketPack();
		break;

	case TF_COND_STEALTHED_PHASE:
		OnRemoveStealthedPhase();
		break;

	case TF_COND_CLIP_OVERLOAD:
		OnRemoveClipOverload();
		break;

#endif // STAGING_ONLY

	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the overheal bonus the specified healer is capable of buffing to
//-----------------------------------------------------------------------------
int CTFPlayerShared::GetMaxBuffedHealth( bool bIgnoreAttributes /*= false*/, bool bIgnoreHealthOverMax /*= false*/ )
{
	// Find the healer we have who's providing the most overheal
	float flBoostMax = m_pOuter->GetMaxHealthForBuffing() * tf_max_health_boost.GetFloat();
#ifdef GAME_DLL
	if ( !bIgnoreAttributes )
	{
		for ( int i = 0; i < m_aHealers.Count(); i++ )
		{
			float flOverheal = m_pOuter->GetMaxHealthForBuffing() * m_aHealers[i].flOverhealBonus;
			if ( flOverheal > flBoostMax )
			{
				flBoostMax = flOverheal;
			}
		}
	}
#endif

	int iRoundDown = floor( flBoostMax / 5 );
	iRoundDown = iRoundDown * 5;

	if ( !bIgnoreHealthOverMax )
	{
		// Don't allow overheal total to be less than the buffable + unbuffable max health or the current health
		int nBoostMin = MAX( m_pOuter->GetMaxHealth(), m_pOuter->GetHealth() );
		if ( iRoundDown < nBoostMin )
		{
			iRoundDown = nBoostMin;
		}
	}

	return iRoundDown;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFPlayerShared::GetDisguiseMaxBuffedHealth( bool bIgnoreAttributes /*= false*/, bool bIgnoreHealthOverMax /*= false*/ )
{
	// Find the healer we have who's providing the most overheal
	float flBoostMax = GetDisguiseMaxHealth() * tf_max_health_boost.GetFloat();
#ifdef GAME_DLL
	if ( !bIgnoreAttributes )
	{
		for ( int i = 0; i < m_aHealers.Count(); i++ )
		{
			float flOverheal = GetDisguiseMaxHealth() * m_aHealers[i].flOverhealBonus;
			if ( flOverheal > flBoostMax )
			{
				flBoostMax = flOverheal;
			}
		}
	}
#endif

	int iRoundDown = floor( flBoostMax / 5 );
	iRoundDown = iRoundDown * 5;

	if ( !bIgnoreHealthOverMax )
	{
		// Don't allow overheal total to be less than the buffable + unbuffable max health or the current health
		int nBoostMin = MAX(GetDisguiseMaxHealth(), GetDisguiseHealth() );
		if ( iRoundDown < nBoostMin )
		{
			iRoundDown = nBoostMin;
		}
	}

	return iRoundDown;
}

//-----------------------------------------------------------------------------
bool ShouldRemoveConditionOnTimeout( ETFCond eCond )
{
	switch( eCond )
	{
	case TF_COND_HALLOWEEN_GHOST_MODE:
	case TF_COND_HALLOWEEN_IN_HELL:
	case TF_COND_HALLOWEEN_KART:
		break;
		return !TFGameRules()->ArePlayersInHell();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Runs SERVER SIDE only Condition Think
// If a player needs something to be updated no matter what do it here (invul, etc).
//-----------------------------------------------------------------------------
void CTFPlayerShared::ConditionGameRulesThink( void )
{
#ifdef GAME_DLL

	m_ConditionList.ServerThink();

	if ( m_flNextCritUpdate < gpGlobals->curtime )
	{
		UpdateCritMult();
		m_flNextCritUpdate = gpGlobals->curtime + 0.5;
	}

	for ( int i=0; i < TF_COND_LAST; ++i )
	{
		// if we're in this condition and it's not already being handled by the condition list
		if ( InCond( (ETFCond)i ) && ((i >= 32) || !m_ConditionList.InCond( (ETFCond)i )) )
		{
			// Ignore permanent conditions
			if ( m_ConditionData[i].m_flExpireTime != PERMANENT_CONDITION )
			{
				float flReduction = gpGlobals->frametime;

				// If we're being healed, we reduce bad conditions faster
				if ( ConditionExpiresFast( (ETFCond)i) && m_aHealers.Count() > 0 )
				{
					if ( i == TF_COND_URINE )
					{
						flReduction += (m_aHealers.Count() * flReduction);
					}
					else
					{
						flReduction += (m_aHealers.Count() * flReduction * 4);
					}
				}

				m_ConditionData[i].m_flExpireTime = MAX( m_ConditionData[i].m_flExpireTime - flReduction, 0 );

				if ( m_ConditionData[i].m_flExpireTime == 0 )
				{
					RemoveCond( (ETFCond)i );
				}
			}
			else
			{
#if !defined( DEBUG )
				// Prevent hacked usercommand exploits
				if ( m_pOuter->GetTimeSinceLastUserCommand() > 5.f || m_pOuter->GetTimeSinceLastThink() > 5.f )
				{
					ETFCond eCond = (ETFCond)i;

					if ( GetCarryingRuneType() != RUNE_NONE )
					{
						m_pOuter->DropRune();
					}

					if ( ShouldRemoveConditionOnTimeout( eCond ) )
					{
						RemoveCond( eCond );

						// Reset active weapon to prevent stale-state bugs
						CTFWeaponBase *pTFWeapon = m_pOuter->GetActiveTFWeapon();
						if ( pTFWeapon )
						{
							pTFWeapon->WeaponReset();
						}

						m_pOuter->TeamFortress_SetSpeed();
					}
				}
#endif
			}
		}
	}

	// Our health will only decay ( from being medic buffed ) if we are not being healed by a medic
	// Dispensers can give us the TF_COND_HEALTH_BUFF, but will not maintain or give us health above 100%s
	bool bDecayHealth = true;
	bool bDecayDisguiseHealth = true;

	// If we're being healed, heal ourselves
	if ( InCond( TF_COND_HEALTH_BUFF ) )
	{
		// Heal faster if we haven't been in combat for a while
		float flTimeSinceDamage = gpGlobals->curtime - m_pOuter->GetLastDamageReceivedTime();
		float flScale = RemapValClamped( flTimeSinceDamage, 10, 15, 1.0, 3.0 );
		float flAttribModScale = 1.0;
		CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( m_pOuter, flAttribModScale, mult_health_fromhealers );

		float flCurOverheal = (float)m_pOuter->GetHealth() / (float)m_pOuter->GetMaxHealth();

		if ( flCurOverheal > 1.0f )
		{
			// If they're over their max health the overheal calculation is relative to the max buffable amount scale
			float flMaxHealthForBuffing = m_pOuter->GetMaxHealthForBuffing();
			float flBuffableRangeHealth = m_pOuter->GetHealth() - ( m_pOuter->GetMaxHealth() - flMaxHealthForBuffing );
			flCurOverheal = flBuffableRangeHealth / flMaxHealthForBuffing;
		}

		float flCurDisguiseOverheal = ( GetDisguiseMaxHealth() != 0 ) ? ( (float)GetDisguiseHealth() / (float)GetDisguiseMaxHealth() ) : ( flCurOverheal );

		float fTotalHealAmount = 0.0f;
		for ( int i = 0; i < m_aHealers.Count(); i++ )
		{
			Assert( m_aHealers[i].pHealer );

			float flPerHealerAttribModScale = 1.f;
			// Check if the healer has an attribute that modifies their overheal rate
			if( flCurOverheal > 1.f && !m_aHealers[i].bDispenserHeal )
			{
				CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( m_aHealers[i].pHealer, flPerHealerAttribModScale, overheal_fill_rate );
			}

			bool bHealDisguise = InCond( TF_COND_DISGUISED );
			bool bHealActual = true;

			// dispensers heal cloak
			if ( m_aHealers[i].bDispenserHeal )
			{
				AddToSpyCloakMeter( gpGlobals->frametime * m_aHealers[i].flAmount );	
			}

			// Don't heal over the healer's overheal bonus
			if ( flCurOverheal >= m_aHealers[i].flOverhealBonus )
			{
				bHealActual = false;
			}

			// Same overheal check, but for fake health
			if ( InCond( TF_COND_DISGUISED ) && flCurDisguiseOverheal >= m_aHealers[i].flOverhealBonus )
			{
				// Fake over-heal
				bHealDisguise = false;
			}

			CTFPlayer *pTFHealer = ToTFPlayer( m_aHealers[i].pHealer );
			if ( !bHealActual && !bHealDisguise )
			{
				if ( pTFHealer )
				{
					// Quick fix never lets health decay, even when they're at or above max overheal
					CWeaponMedigun *pMedigun = dynamic_cast< CWeaponMedigun* >( pTFHealer->GetActiveTFWeapon() );
					if ( pMedigun && pMedigun->GetMedigunType() == MEDIGUN_QUICKFIX )
					{
						bDecayHealth = false;
						bDecayDisguiseHealth = false;
					}
				}

				continue;
			}

			// Being healed by a medigun, don't decay our health
			if ( bHealActual )
			{
				bDecayHealth = false;
			}

			if ( bHealDisguise )
			{
				bDecayDisguiseHealth = false;
			}

			// What we multiply the heal amount by (can be changed by conditions or items).
			float flHealAmountMult = 1.0f;

			// Quick-Fix uber
			if ( InCond( TF_COND_MEGAHEAL ) )
			{
				flHealAmountMult = 3.0f;
			}

			flScale *= flHealAmountMult;

			// Dispensers heal at a constant rate
			if ( m_aHealers[i].bDispenserHeal )
			{
				// Dispensers heal at a slower rate, but ignore flScale
				if ( bHealActual )
				{
					float flDispenserFraction = gpGlobals->frametime * m_aHealers[i].flAmount * flAttribModScale;
					m_flHealFraction += flDispenserFraction;

					// track how much this healer has actually done so far
					m_aHealers[i].flHealAccum += clamp( flDispenserFraction, 0.f, (float) GetMaxBuffedHealth() - m_pOuter->GetHealth() );
				}
				if ( bHealDisguise )
				{
					m_flDisguiseHealFraction += gpGlobals->frametime * m_aHealers[i].flAmount * flAttribModScale;
				}
			}
			else	// player heals are affected by the last damage time
			{
				if ( bHealActual )
				{
					// Scale this if needed
					CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( m_pOuter, flScale, mult_healing_from_medics );
					m_flHealFraction += gpGlobals->frametime * m_aHealers[i].flAmount * flScale * flAttribModScale * flPerHealerAttribModScale;
				}
				if ( bHealDisguise )
				{
					m_flDisguiseHealFraction += gpGlobals->frametime * m_aHealers[i].flAmount * flScale * flAttribModScale * flPerHealerAttribModScale;
				}
			}

			fTotalHealAmount += m_aHealers[i].flAmount;

			// Keep our decay multiplier uptodate
			if ( m_flBestOverhealDecayMult == -1 || m_aHealers[i].flOverhealDecayMult < m_flBestOverhealDecayMult )
			{
				m_flBestOverhealDecayMult = m_aHealers[i].flOverhealDecayMult;
			}
		}

		if ( InCond( TF_COND_HEALING_DEBUFF ) )
		{
			m_flHealFraction *= 0.75f;
		}

		int nHealthToAdd = (int)m_flHealFraction;
		int nDisguiseHealthToAdd = (int)m_flDisguiseHealFraction;
		if ( nHealthToAdd > 0 || nDisguiseHealthToAdd > 0 )
		{
			if ( nHealthToAdd > 0 )
			{
				m_flHealFraction -= nHealthToAdd;
			}

			if ( nDisguiseHealthToAdd > 0 )
			{
				m_flDisguiseHealFraction -= nDisguiseHealthToAdd;
			}

			int iBoostMax = GetMaxBuffedHealth();

			if ( InCond( TF_COND_DISGUISED ) )
			{
				// Separate cap for disguised health
				int nFakeHealthToAdd = clamp( nDisguiseHealthToAdd, 0, GetDisguiseMaxBuffedHealth() - m_iDisguiseHealth );
				m_iDisguiseHealth += nFakeHealthToAdd;
			}

			// Track health prior to healing
			int nPrevHealth = m_pOuter->GetHealth();
			
			// Cap it to the max we'll boost a player's health
			nHealthToAdd = clamp( nHealthToAdd, 0, iBoostMax - m_pOuter->GetHealth() );
			
			m_pOuter->TakeHealth( nHealthToAdd, DMG_IGNORE_MAXHEALTH | DMG_IGNORE_DEBUFFS );
			
			m_pOuter->AdjustDrownDmg( -1.0 * nHealthToAdd ); // subtract this from the drowndmg in case they're drowning and being healed at the same time

			// split up total healing based on the amount each healer contributes
			if ( fTotalHealAmount > 0 )
			{
				for ( int i = 0; i < m_aHealers.Count(); i++ )
				{
					Assert( m_aHealers[i].pHealScorer );
					Assert( m_aHealers[i].pHealer );
					if ( m_aHealers[i].pHealScorer.IsValid() && m_aHealers[i].pHealer.IsValid() )
					{
						CBaseEntity *pHealer = m_aHealers[i].pHealer;
						float flHealAmount = nHealthToAdd * ( m_aHealers[i].flAmount / fTotalHealAmount );

						if ( pHealer && IsAlly( pHealer ) )
						{
							CTFPlayer *pHealScorer = ToTFPlayer( m_aHealers[i].pHealScorer );
							if ( pHealScorer )
							{	
								// Don't report healing when we're close to the buff cap and haven't taken damage recently.
								// This avoids sending bogus heal stats while maintaining our max overheal.  Ideally we
								// wouldn't decay in this scenario, but that would be a risky change.
								if ( iBoostMax - nPrevHealth > 1 || gpGlobals->curtime - m_pOuter->GetLastDamageReceivedTime() <= 1.f )
								{
									CTF_GameStats.Event_PlayerHealedOther( pHealScorer, flHealAmount );
								}

								// Add this to the one-second-healing counter
								m_aHealers[i].flHealedLastSecond += flHealAmount;
					
								HandleRageGain( m_pOuter, kRageBuffFlag_OnMedicHealingReceived, flHealAmount / 2.f, 1.0f );
								
								float flRage = flHealAmount;
								if ( TFGameRules() && TFGameRules()->IsMannVsMachineMode() && 
									 TFObjectiveResource() && TFObjectiveResource()->GetMannVsMachineIsBetweenWaves() )
								{
									flRage = Max( flHealAmount, 10.f );
								}
								HandleRageGain( pHealScorer, kRageBuffFlag_OnHeal, flRage, 1.0f );

								// If it's been one second, or we know healing beyond this point will be overheal, generate an event
								if ( ( m_flHealedPerSecondTimer <= gpGlobals->curtime || m_pOuter->GetHealth() >= m_pOuter->GetMaxHealth() ) 
									   && m_aHealers[i].flHealedLastSecond > 1 )
								{
									// Make sure this isn't pure overheal
									if ( m_pOuter->GetHealth() - m_aHealers[i].flHealedLastSecond < m_pOuter->GetMaxHealth() )
									{
										float flOverHeal = m_pOuter->GetHealth() - m_pOuter->GetMaxHealth();
										if ( flOverHeal > 0 )
										{
											m_aHealers[i].flHealedLastSecond -= flOverHeal;
										}

										// TEST THIS
										// Give the medic some uber if it is from their (AoE heal) which has no overheal
										if ( m_aHealers[i].flOverhealBonus <= 1.0f )
										{
											// Give a litte bit of uber based on actual healing
											// Give them a little bit of Uber
											CWeaponMedigun *pMedigun = static_cast<CWeaponMedigun *>( pHealScorer->Weapon_OwnsThisID( TF_WEAPON_MEDIGUN ) );
											if ( pMedigun )
											{
												// On Mediguns, per frame, the amount of uber added is based on 
												// Default heal rate is 24per second, we scale based on that and frametime
												pMedigun->AddCharge( ( m_aHealers[i].flHealedLastSecond / 24.0f ) * gpGlobals->frametime * 0.33f );
											}
										}

										IGameEvent * event = gameeventmanager->CreateEvent( "player_healed" );
										if ( event )
										{
											// HLTV event priority, not transmitted
											event->SetInt( "priority", 1 );	

											// Healed by another player.
											event->SetInt( "patient", m_pOuter->GetUserID() );
											event->SetInt( "healer", pHealScorer->GetUserID() );
											event->SetInt( "amount", m_aHealers[i].flHealedLastSecond );
											gameeventmanager->FireEvent( event );
										}

										// Can we figure out which item is doing this healing?
										if ( pHealScorer )
										{
											// Can be Mediguns or anything that gives off 'heal' buff like amputator aoe heal
											EconEntity_OnOwnerKillEaterEvent_Batched( pHealScorer->GetActiveTFWeapon(), pHealScorer, m_pOuter, kKillEaterEvent_AllyHealingDone, m_aHealers[i].flHealedLastSecond );
										}
									}

									m_aHealers[i].flHealedLastSecond = 0;
									m_flHealedPerSecondTimer = gpGlobals->curtime + 1.0f;
								}
							}
						}
						else
						{
							CTF_GameStats.Event_PlayerLeachedHealth( m_pOuter, m_aHealers[i].bDispenserHeal, flHealAmount );
						}
					}
				}
			}
		}

		if ( InCond( TF_COND_BURNING ) )
		{
			// Reduce the duration of this burn 
			float flReduction = 2;	 // ( flReduction + 1 ) x faster reduction
			m_flFlameRemoveTime -= flReduction * gpGlobals->frametime;
		}
		if ( InCond( TF_COND_BLEEDING ) )
		{
			// Reduce the duration of this bleeding 
			float flReduction = 2;	 // ( flReduction + 1 ) x faster reduction
			FOR_EACH_VEC( m_PlayerBleeds, i )
			{
				m_PlayerBleeds[i].flBleedingRemoveTime -= flReduction * gpGlobals->frametime;
			}
		}
	}

	if ( !InCond( TF_COND_HEALTH_OVERHEALED ) && m_pOuter->GetHealth() > ( m_pOuter->GetMaxHealth() - m_pOuter->GetRuneHealthBonus() ) )
	{
		AddCond( TF_COND_HEALTH_OVERHEALED, PERMANENT_CONDITION );
	}
	else if ( InCond( TF_COND_HEALTH_OVERHEALED ) && m_pOuter->GetHealth() <= ( m_pOuter->GetMaxHealth() - m_pOuter->GetRuneHealthBonus() ) )
	{
		RemoveCond( TF_COND_HEALTH_OVERHEALED );
	}

	if ( bDecayHealth )
	{
		float flOverheal = GetMaxBuffedHealth( false, true );

		// If we're not being buffed, our health drains back to our max
		if ( m_pOuter->GetHealth() > m_pOuter->GetMaxHealth() )
		{
			// Items exist that get us over max health, without ever being healed, in which case our m_flBestOverhealDecayMult will still be -1.
			float flDrainMult = (m_flBestOverhealDecayMult == -1) ? 1.0 : m_flBestOverhealDecayMult;
			float flBoostMaxAmount = flOverheal - m_pOuter->GetMaxHealth();
			float flDrain = flBoostMaxAmount / (tf_boost_drain_time.GetFloat() * flDrainMult);
			m_flHealFraction += (gpGlobals->frametime * flDrain);

			int nHealthToDrain = (int)m_flHealFraction;
			if ( nHealthToDrain > 0 )
			{
				m_flHealFraction -= nHealthToDrain;

				// Manually subtract the health so we don't generate pain sounds / etc
				m_pOuter->m_iHealth -= nHealthToDrain;
			}
		}
		else if ( m_flBestOverhealDecayMult != -1 )
		{
			m_flBestOverhealDecayMult = -1;
		}

	}

	if ( bDecayDisguiseHealth )
	{
		float flOverheal = GetDisguiseMaxBuffedHealth( false, true );

		if ( InCond( TF_COND_DISGUISED ) && (GetDisguiseHealth() > GetDisguiseMaxHealth()) )
		{
			// Items exist that get us over max health, without ever being healed, in which case our m_flBestOverhealDecayMult will still be -1.
			float flDrainMult = (m_flBestOverhealDecayMult == -1) ? 1.0 : m_flBestOverhealDecayMult;
			float flBoostMaxAmount = flOverheal - GetDisguiseMaxHealth();
			float flDrain = (flBoostMaxAmount / tf_boost_drain_time.GetFloat()) * flDrainMult;
			m_flDisguiseHealFraction += (gpGlobals->frametime * flDrain);

			int nHealthToDrain = (int)m_flDisguiseHealFraction;
			if ( nHealthToDrain > 0 )
			{
				m_flDisguiseHealFraction -= nHealthToDrain;

				// Reduce our fake disguised health by roughly the same amount
				m_iDisguiseHealth -= nHealthToDrain;
			}
		}
	}

	// Taunt
	if ( InCond( TF_COND_TAUNTING ) )
	{
		if ( m_pOuter->IsAllowedToRemoveTaunt() && gpGlobals->curtime > m_pOuter->GetTauntRemoveTime() )
		{
			RemoveCond( TF_COND_TAUNTING );
		}
	}

	if ( InCond( TF_COND_BURNING ) && !m_pOuter->m_bInPowerPlay )
	{
		if ( TFGameRules() && TFGameRules()->IsTruceActive() && m_hBurnAttacker && m_hBurnAttacker->IsTruceValidForEnt() )
		{
			RemoveCond( TF_COND_BURNING );
		}
		else if ( gpGlobals->curtime > m_flFlameRemoveTime || m_pOuter->GetWaterLevel() >= WL_Waist )
		{
			// If we're underwater, put the fire out
			if ( m_pOuter->GetWaterLevel() >= WL_Waist )
			{
				// General achievement for jumping into water while you're on fire
				m_pOuter->AwardAchievement( ACHIEVEMENT_TF_FIRE_WATERJUMP );

				// Pyro achievement for forcing players into water
				if ( m_hBurnAttacker )
				{
					m_hBurnAttacker->AwardAchievement( ACHIEVEMENT_TF_PYRO_FORCE_WATERJUMP );
				}
			}

			RemoveCond( TF_COND_BURNING );

			if ( InCond( TF_COND_HEALTH_BUFF ) )
			{
				// one or more players is healing us, send a "player_extinguished" event. We
				// need to send one for each player who's healing us.
				for ( int i = 0; i < m_aHealers.Count(); i++ )
				{
					Assert( m_aHealers[i].pHealer );

					if ( m_aHealers[i].bDispenserHeal )
					{
						CObjectDispenser *pDispenser = dynamic_cast<CObjectDispenser*>( m_aHealers[i].pHealer.Get() );
						if ( pDispenser )
						{
							CTFPlayer *pTFPlayer = pDispenser->GetBuilder();
							if ( pTFPlayer )
							{
								UTIL_LogPrintf( "\"%s<%i><%s><%s>\" triggered \"player_extinguished\" against \"%s<%i><%s><%s>\" with \"dispenser\" (attacker_position \"%d %d %d\") (victim_position \"%d %d %d\")\n",    
									pTFPlayer->GetPlayerName(),
									pTFPlayer->GetUserID(),
									pTFPlayer->GetNetworkIDString(),
									pTFPlayer->GetTeam()->GetName(),
									m_pOuter->GetPlayerName(),
									m_pOuter->GetUserID(),
									m_pOuter->GetNetworkIDString(),
									m_pOuter->GetTeam()->GetName(),
									(int)m_aHealers[i].pHealer->GetAbsOrigin().x, 
									(int)m_aHealers[i].pHealer->GetAbsOrigin().y,
									(int)m_aHealers[i].pHealer->GetAbsOrigin().z,
									(int)m_pOuter->GetAbsOrigin().x, 
									(int)m_pOuter->GetAbsOrigin().y,
									(int)m_pOuter->GetAbsOrigin().z );
							}
						}

//						continue;
					}

					EHANDLE pHealer = m_aHealers[i].pHealer;
					if ( m_aHealers[i].bDispenserHeal || !pHealer || !pHealer->IsPlayer() )
						pHealer = m_aHealers[i].pHealScorer;

					if ( !pHealer )
						continue;

					CTFPlayer *pTFPlayer = ToTFPlayer( pHealer );
					if ( pTFPlayer && !m_aHealers[i].bDispenserHeal )
					{
						UTIL_LogPrintf( "\"%s<%i><%s><%s>\" triggered \"player_extinguished\" against \"%s<%i><%s><%s>\" with \"%s\" (attacker_position \"%d %d %d\") (victim_position \"%d %d %d\")\n",    
							pTFPlayer->GetPlayerName(),
							pTFPlayer->GetUserID(),
							pTFPlayer->GetNetworkIDString(),
							pTFPlayer->GetTeam()->GetName(),
							m_pOuter->GetPlayerName(),
							m_pOuter->GetUserID(),
							m_pOuter->GetNetworkIDString(),
							m_pOuter->GetTeam()->GetName(),
							( pTFPlayer->GetActiveTFWeapon() ) ? pTFPlayer->GetActiveTFWeapon()->GetName() : "tf_weapon_medigun",
							(int)pTFPlayer->GetAbsOrigin().x, 
							(int)pTFPlayer->GetAbsOrigin().y,
							(int)pTFPlayer->GetAbsOrigin().z,
							(int)m_pOuter->GetAbsOrigin().x, 
							(int)m_pOuter->GetAbsOrigin().y,
							(int)m_pOuter->GetAbsOrigin().z );
					}

					// Tell the clients involved 
					CRecipientFilter involved_filter;
					CBasePlayer *pBasePlayerHealer = ToBasePlayer( pHealer );
					if ( pBasePlayerHealer )
					{
						involved_filter.AddRecipient( pBasePlayerHealer );
					}
					involved_filter.AddRecipient( m_pOuter );
					UserMessageBegin( involved_filter, "PlayerExtinguished" );
						WRITE_BYTE( pHealer->entindex() );
						WRITE_BYTE( m_pOuter->entindex() );
					MessageEnd();

					IGameEvent *event = gameeventmanager->CreateEvent( "player_extinguished" );
					if ( event )
					{
						event->SetInt( "victim", m_pOuter->entindex() );
						event->SetInt( "healer", pHealer->entindex() );

						gameeventmanager->FireEvent( event, true );
					}
				}
			}
		}
		else if ( ( gpGlobals->curtime >= m_flFlameBurnTime ) && ( TF_CLASS_PYRO != m_pOuter->GetPlayerClass()->GetClassIndex() ) )
		{
			// Burn the player (if not pyro, who does not take persistent burning damage)

			float flBurnDamage = TF_BURNING_DMG;
			int nKillType = TF_DMG_CUSTOM_BURNING;

			if ( m_hBurnWeapon )
			{
				CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( m_hBurnWeapon, flBurnDamage, mult_wpn_burndmg );

				if ( m_hBurnWeapon.Get()->GetWeaponID() == TF_WEAPON_FLAREGUN )
				{
					nKillType = TF_DMG_CUSTOM_BURNING_FLARE;
				}
				else if ( m_hBurnWeapon.Get()->GetWeaponID() == TF_WEAPON_COMPOUND_BOW )
				{
					nKillType = TF_DMG_CUSTOM_BURNING_ARROW;
				}
			}
		
			// Halloween Spell
			if ( TF_IsHolidayActive( kHoliday_HalloweenOrFullMoon ) )
			{
				int iHalloweenSpell = 0;
				CALL_ATTRIB_HOOK_INT_ON_OTHER( m_hBurnWeapon, iHalloweenSpell, halloween_green_flames );
				if ( iHalloweenSpell > 0 )
				{
					const char *pEffectName = "halloween_burningplayer_flyingbits";
					// Extra Halloween Particles
					DispatchParticleEffect( pEffectName, PATTACH_ABSORIGIN_FOLLOW, m_pOuter, 0, false );
				}
			}
	
			CTakeDamageInfo info( m_hBurnAttacker, m_hBurnAttacker, m_hBurnWeapon, flBurnDamage, DMG_BURN | DMG_PREVENT_PHYSICS_FORCE, nKillType );
			m_pOuter->TakeDamage( info );

			// Give health to attacker if they are carrying the Vampire Powerup.
			if ( TFGameRules() && TFGameRules()->IsPowerupMode() )  
			{
				CTFPlayer *pTFAttacker = ToTFPlayer( GetConditionProvider( TF_COND_BURNING ) );

				if ( pTFAttacker && pTFAttacker != m_pOuter )
				{
					if ( pTFAttacker->m_Shared.GetCarryingRuneType() == RUNE_VAMPIRE )
					{
						pTFAttacker->TakeHealth( flBurnDamage, DMG_GENERIC );
					}
				}
			}

			m_flFlameBurnTime = gpGlobals->curtime + TF_BURNING_FREQUENCY;
		}

		if ( m_flNextBurningSound < gpGlobals->curtime )
		{
			m_pOuter->SpeakConceptIfAllowed( MP_CONCEPT_ONFIRE );
			m_flNextBurningSound = gpGlobals->curtime + 2.5;
		}
	}


	// Stops the drain hack.
	if ( m_pOuter->IsPlayerClass( TF_CLASS_MEDIC ) )
	{
		CWeaponMedigun *pWeapon = ( CWeaponMedigun* )m_pOuter->Weapon_OwnsThisID( TF_WEAPON_MEDIGUN );
		if ( pWeapon && pWeapon->IsReleasingCharge() )
		{
			pWeapon->DrainCharge();
		}
	}

	TestAndExpireChargeEffect( MEDIGUN_CHARGE_INVULN );
	TestAndExpireChargeEffect( MEDIGUN_CHARGE_CRITICALBOOST );
	TestAndExpireChargeEffect( MEDIGUN_CHARGE_MEGAHEAL );
	//TestAndExpireChargeEffect( MEDIGUN_CHARGE_BULLET_RESIST );
	//TestAndExpireChargeEffect( MEDIGUN_CHARGE_BLAST_RESIST );
	//TestAndExpireChargeEffect( MEDIGUN_CHARGE_FIRE_RESIST );

	if ( InCond( TF_COND_STEALTHED_BLINK ) )
	{
		float flBlinkTime = TF_SPY_STEALTH_BLINKTIME;
		CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( m_pOuter, flBlinkTime, cloak_blink_time_penalty );
		if ( flBlinkTime < ( gpGlobals->curtime - m_flLastStealthExposeTime ) )
		{
			RemoveCond( TF_COND_STEALTHED_BLINK );
		}
	}

	if ( InCond( TF_COND_FEIGN_DEATH ) )
	{
		if ( m_flFeignDeathEnd < gpGlobals->curtime )
		{
			RemoveCond( TF_COND_FEIGN_DEATH );
		}
	}

	if ( m_pOuter->GetWaterLevel() >= WL_Waist )
	{
		if ( InCond( TF_COND_URINE ) )
		{
			// If we're underwater, wash off the urine.
			RemoveCond( TF_COND_URINE );
		}

		if ( InCond( TF_COND_MAD_MILK ) )
		{
			// If we're underwater, wash off the Mad Milk.
			RemoveCond( TF_COND_MAD_MILK );
		}
	}

	if ( !InCond( TF_COND_DISGUISED ) )
	{
		// Remove our disguise weapon if we are ever not disguised and we have one.
		RemoveDisguiseWeapon();

		// also clear the disguise weapon list
		m_pOuter->ClearDisguiseWeaponList();
	}

	if ( InCond( TF_COND_BLEEDING ) )
	{
		FOR_EACH_VEC_BACK( m_PlayerBleeds, i )
		{
			bleed_struct_t& bleed = m_PlayerBleeds[i];
			if ( TFGameRules() && TFGameRules()->IsTruceActive() && bleed.hBleedingAttacker && ( bleed.hBleedingAttacker != m_pOuter ) && bleed.hBleedingAttacker->IsTruceValidForEnt() )
			{
				m_PlayerBleeds.FastRemove( i );
			}
			else if ( gpGlobals->curtime >= bleed.flBleedingRemoveTime && !bleed.bPermanentBleeding )
			{
				m_PlayerBleeds.FastRemove( i );
			}
			else if ( ( gpGlobals->curtime >= bleed.flBleedingTime ) )
			{
				bleed.flBleedingTime = gpGlobals->curtime + TF_BLEEDING_FREQUENCY;

				CTakeDamageInfo info( bleed.hBleedingAttacker, bleed.hBleedingAttacker, bleed.hBleedingWeapon, bleed.nBleedDmg, DMG_SLASH, TF_DMG_CUSTOM_BLEEDING );
				m_pOuter->TakeDamage( info );

				// It's very possible we died from the take damage, which clears all our conditions
				// and nukes m_PlayerBleeds.  If that happens, bust out of this loop.
				if( m_PlayerBleeds.Count() == 0 )
					break;
			}
		}

		if ( !m_PlayerBleeds.Count() )
		{
			RemoveCond( TF_COND_BLEEDING );
		}
	}

#ifdef STAGING_ONLY
	if ( InCond( TF_COND_TRANQ_SPY_BOOST ) )
	{
		m_flSpyTranqBuffDuration = GetConditionDuration( TF_COND_TRANQ_SPY_BOOST );
	}
	else
#endif // STAGING_ONLY
	{
		m_flSpyTranqBuffDuration = 0;
	}

	if ( TFGameRules()->IsMannVsMachineMode() || TFGameRules()->IsPlayingRobotDestructionMode() )
	{
		RadiusCurrencyCollectionCheck();
	}

	if ( TFGameRules()->IsMannVsMachineMode() && m_pOuter->IsPlayerClass( TF_CLASS_SPY) )
	{
		// In MvM, Spies reveal other spies in a radius around them
		RadiusSpyScan();
	}
	if ( GetCarryingRuneType() == RUNE_PLAGUE )
	{
		RadiusHealthkitCollectionCheck();
	}

#endif // GAME_DLL
}

//-----------------------------------------------------------------------------
// Purpose: Do CLIENT/SERVER SHARED condition thinks.
//-----------------------------------------------------------------------------
void CTFPlayerShared::ConditionThink( void )
{
	// Client Only Updates Meters for Local Only
#ifdef CLIENT_DLL
	if ( m_pOuter->IsLocalPlayer() )
#endif
	{
		UpdateCloakMeter();
		UpdateRageBuffsAndRage();
		UpdateEnergyDrinkMeter();
		UpdateChargeMeter();
		DemoShieldChargeThink();

#ifdef STAGING_ONLY
		UpdateRocketPack();
#endif // STAGING_ONLY
	}

	VehicleThink();

	if ( m_pOuter->GetFlags() & FL_ONGROUND && InCond( TF_COND_PARACHUTE_DEPLOYED ))
	{
		RemoveCond( TF_COND_PARACHUTE_DEPLOYED );
	}

	// See if we should be pulsing our radius heal
	PulseMedicRadiusHeal();
	PulseKingRuneBuff();

	m_ConditionList.Think();

	if ( InCond( TF_COND_STUNNED ) )
	{
#ifdef GAME_DLL
		if ( IsControlStunned() )
		{
			m_pOuter->SetAbsAngles( m_pOuter->m_angTauntCamera );
			m_pOuter->SetLocalAngles( m_pOuter->m_angTauntCamera );
		}
#endif
		if ( GetActiveStunInfo() && gpGlobals->curtime > GetActiveStunInfo()->flExpireTime )
		{
#ifdef GAME_DLL	
			m_PlayerStuns.Remove( m_iStunIndex );
			m_iStunIndex = -1;

			// Apply our next stun
			if ( m_PlayerStuns.Count() )
			{
				int iStrongestIdx = 0;
				for ( int i = 1; i < m_PlayerStuns.Count(); i++ )
				{
					if ( m_PlayerStuns[i].flStunAmount > m_PlayerStuns[iStrongestIdx].flStunAmount )
					{
						iStrongestIdx = i;
					}
				}
				m_iStunIndex = iStrongestIdx;

				AddCond( TF_COND_STUNNED, -1.f, m_PlayerStuns[m_iStunIndex].hPlayer );
				m_iMovementStunParity = ( m_iMovementStunParity + 1 ) & ( ( 1 << MOVEMENTSTUN_PARITY_BITS ) - 1 ); 

				Assert( GetActiveStunInfo() );
			}
			else
			{
				RemoveCond( TF_COND_STUNNED );
			}
#endif // GAME_DLL

			UpdateLegacyStunSystem();
		}
		else if ( IsControlStunned() && GetActiveStunInfo() && ( gpGlobals->curtime > GetActiveStunInfo()->flStartFadeTime ) )
		{
			// Control stuns have a final anim to play.
			ControlStunFading();
		}

#ifdef CLIENT_DLL
		// turn off stun effect that gets turned on when incomplete stun msg is received on the client
		if ( GetActiveStunInfo() && GetActiveStunInfo()->iStunFlags & TF_STUN_NO_EFFECTS )
		{
			if ( m_pOuter->m_pStunnedEffect )
			{
				// Remove stun stars if they are still around.
				// They might be if we died, etc.
				m_pOuter->ParticleProp()->StopEmission( m_pOuter->m_pStunnedEffect );
				m_pOuter->m_pStunnedEffect = NULL;
			}
		}
#endif
	}

	if ( InCond( TF_COND_HALLOWEEN_BOMB_HEAD ) )
	{
#ifdef GAME_DLL
		static struct 
		{
			float flTimeLeft;
			int nStage;
		} s_vecBombStages[] = { { 8.0f, 0 }, { 3.0f, 1 }, { 0.0f, 2 } };

		for ( int i = 0; i < 