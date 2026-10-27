//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The TF Game rules 
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "tf_gamerules.h"
#include "tf_classdata.h"
#include "ammodef.h"
#include "KeyValues.h"
#include "tf_weaponbase.h"
#include "tf_weaponbase_gun.h"
#include "tier0/icommandline.h"
#include "convar_serverbounded.h"
#include "econ_item_system.h"
#include "tf_weapon_grenadelauncher.h"
#include "tf_logic_robot_destruction.h"
#include "tf_logic_player_destruction.h"
#include "tf_matchmaking_shared.h"

#ifdef CLIENT_DLL
	#include <game/client/iviewport.h>
	#include "c_tf_player.h"
	#include "c_tf_objective_resource.h"
	#include <filesystem.h>
	#include "c_tf_team.h"
	#include "dt_utlvector_recv.h"
	#include "tf_autorp.h"
	#include "player_vs_environment/c_tf_upgrades.h"
	#include "video/ivideoservices.h"
	#include "tf_gc_client.h"
	#include "c_tf_playerresource.h"
#else
	#include "basemultiplayerplayer.h"
	#include "voice_gamemgr.h"
	#include "items.h"
	#include "team.h"
	#include "game.h"
	#include "tf_bot_temp.h"
	#include "tf_player.h"
	#include "tf_team.h"
	#include "player_resource.h"
	#include "entity_tfstart.h"
	#include "filesystem.h"
	#include "minigames/tf_duel.h"
	#include "tf_obj.h"
	#include "tf_objective_resource.h"
	#include "tf_player_resource.h"
	#include "team_control_point_master.h"
	#include "team_train_watcher.h"
	#include "playerclass_info_parse.h"
	#include "team_control_point_master.h"
	#include "coordsize.h"
	#include "entity_healthkit.h"
	#include "tf_gamestats.h"
	#include "entity_capture_flag.h"
	#include "tf_player_resource.h"
	#include "tf_obj_sentrygun.h"
	#include "activitylist.h"
	#include "AI_ResponseSystem.h"
	#include "hl2orange.spa.h"
	#include "hltvdirector.h"
	#include "tf_projectile_arrow.h"
	#include "func_suggested_build.h"
	#include "tf_gc_api.h"
	#include "tf_weaponbase_grenadeproj.h"
	#include "engine/IEngineSound.h"
	#include "soundenvelope.h"
	#include "dt_utlvector_send.h"
	#include "tf_tactical_mission.h"
	#include "nav_mesh/tf_nav_area.h"
	#include "bot/tf_bot.h"
	#include "bot/tf_bot_manager.h"
	#include "bot/map_entities/tf_bot_roster.h"
	#include "econ_gcmessages.h"
	#include "vgui/ILocalize.h"
	#include "tier3/tier3.h"
	#include "tf_ammo_pack.h"
	#include "tf_gcmessages.h"
	#include "vote_controller.h"
	#include "tf_voteissues.h"
	#include "halloween/headless_hatman.h"
	#include "halloween/ghost/ghost.h"
	#include "halloween/eyeball_boss/eyeball_boss.h"
	#include "halloween/merasmus/merasmus.h"
	#include "halloween/merasmus/merasmus_dancer.h"
	#include "tf_extra_map_entity.h"
	#include "tf_weapon_grenade_pipebomb.h"
	#include "tf_weapon_flaregun.h"
	#include "tf_weapon_sniperrifle.h"
	#include "tf_weapon_knife.h"
	#include "tf_weapon_jar.h"
	#include "halloween/tf_weapon_spellbook.h"
	
	#include "player_vs_environment/tf_population_manager.h"
	#include "player_vs_environment/monster_resource.h"
	#include "util_shared.h"
	#include "gc_clientsystem.h"

	#include "raid/tf_raid_logic.h"
	#include "player_vs_environment/tf_boss_battle_logic.h"
	#include "player_vs_environment/tf_mann_vs_machine_logic.h"
	#include "player_vs_environment/tf_upgrades.h"

	#include "tf_wheel_of_doom.h"
	#include "tf_halloween_souls_pickup.h"
	#include "halloween/zombie/zombie.h"
	#include "teamplay_round_timer.h"
	#include "halloween/spell/tf_spell_pickup.h"
	#include "tf_weapon_laser_pointer.h"
	#include "effect_dispatch_data.h"
	#include "tf_fx.h"
	#include "econ_game_account_server.h"
	#include "tf_gc_server.h"
	#include "tf_logic_halloween_2014.h"
	#include "tf_obj_sentrygun.h"
	#include "entity_halloween_pickup.h"
	#include "entity_rune.h"
	#include "func_powerupvolume.h"
	#include "workshop/maps_workshop.h"
	#include "tf_passtime_logic.h"
	#include "cdll_int.h"
	#include "halloween/halloween_gift_spawn_locations.h"
	#include "tf_weapon_invis.h"
	#include "tf_gc_server.h"
	#include "gcsdk/msgprotobuf.h"
	#include "tf_party.h"
	#include "tf_autobalance.h"
#endif

#include "tf_mann_vs_machine_stats.h"
#include "tf_upgrades_shared.h"

#include "tf_item_powerup_bottle.h"
#include "tf_weaponbase_gun.h"
#include "tf_weaponbase_melee.h"
#include "tf_wearable_item_demoshield.h"
#include "tf_weapon_buff_item.h"
#include "tf_weapon_flamethrower.h"
#include "tf_weapon_medigun.h"

#include "econ_holidays.h"
#include "rtime.h"
#include "tf_revive.h"
#include "tf_duckleaderboard.h"

#include "passtime_convars.h"

#include "tier3/tier3.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define ITEM_RESPAWN_TIME	10.0f
#define MASK_RADIUS_DAMAGE  ( MASK_SHOT & ~( CONTENTS_HITBOX ) )

// Halloween 2013 VO defines for plr_hightower_event
#define HELLTOWER_TIMER_INTERVAL	( 60 + RandomInt( -30, 30 )	)
#define HELLTOWER_RARE_LINE_CHANCE	0.15	// 15%
#define HELLTOWER_MISC_CHANCE		0.50	// 50%

static int g_TauntCamRagdollAchievements[] = 
{
	0,		// TF_CLASS_UNDEFINED

	0,		// TF_CLASS_SCOUT,	
	0,		// TF_CLASS_SNIPER,
	0,		// TF_CLASS_SOLDIER,
	0,		// TF_CLASS_DEMOMAN,
	ACHIEVEMENT_TF_MEDIC_FREEZECAM_RAGDOLL,		// TF_CLASS_MEDIC,
	0,		// TF_CLASS_HEAVYWEAPONS,
	0,		// TF_CLASS_PYRO,
	ACHIEVEMENT_TF_SPY_FREEZECAM_FLICK,		// TF_CLASS_SPY,
	0,		// TF_CLASS_ENGINEER,

	0,		// TF_CLASS_CIVILIAN,
	0,		// TF_CLASS_COUNT_ALL,
};

static int g_TauntCamAchievements[] = 
{
	0,		// TF_CLASS_UNDEFINED

	0,		// TF_CLASS_SCOUT,	
	ACHIEVEMENT_TF_SNIPER_FREEZECAM_HAT,		// TF_CLASS_SNIPER,
	ACHIEVEMENT_TF_SOLDIER_FREEZECAM_GIBS,		// TF_CLASS_SOLDIER,		(extra check to count the number of gibs onscreen)
	ACHIEVEMENT_TF_DEMOMAN_FREEZECAM_SMILE,		// TF_CLASS_DEMOMAN,
	0,		// TF_CLASS_MEDIC,
	ACHIEVEMENT_TF_HEAVY_FREEZECAM_TAUNT,		// TF_CLASS_HEAVYWEAPONS,  (there's an extra check on this one to see if we're also invuln)
	ACHIEVEMENT_TF_PYRO_FREEZECAM_TAUNTS,		// TF_CLASS_PYRO,
	0,		// TF_CLASS_SPY,
	ACHIEVEMENT_TF_ENGINEER_FREEZECAM_TAUNT,	// TF_CLASS_ENGINEER,
	0,		// TF_CLASS_CIVILIAN,
	0,		// TF_CLASS_COUNT_ALL,
};

// used for classes that have more than one freeze cam achievement (example: Sniper)
static int g_TauntCamAchievements2[] = 
{
	0,		// TF_CLASS_UNDEFINED

	0,		// TF_CLASS_SCOUT,	
	ACHIEVEMENT_TF_SNIPER_FREEZECAM_WAVE,		// TF_CLASS_SNIPER,
	ACHIEVEMENT_TF_SOLDIER_FREEZECAM_TAUNT,		// TF_CLASS_SOLDIER,
	ACHIEVEMENT_TF_DEMOMAN_FREEZECAM_RUMP,		// TF_CLASS_DEMOMAN,
	0,		// TF_CLASS_MEDIC,
	0,		// TF_CLASS_HEAVYWEAPONS,
	0,		// TF_CLASS_PYRO,
	0,		// TF_CLASS_SPY,
	0,		// TF_CLASS_ENGINEER,

	0,		// TF_CLASS_CIVILIAN,
	0,		// TF_CLASS_COUNT_ALL,
};

struct MapInfo_t
{
	const char	*pDiskName;
	const char	*pDisplayName;
	const char	*pGameType;
};

static MapInfo_t s_ValveMaps[] = {
	{ "ctf_2fort",	"2Fort",		"#Gametype_CTF" },
	{ "cp_dustbowl",	"Dustbowl",		"#TF_AttackDefend" },
	{ "cp_granary",	"Granary",		"#Gametype_CP" },
	{ "cp_well",		"Well",			"#Gametype_CP" },
	{ "cp_foundry",	"Foundry",		"#Gametype_CP" },
	{ "cp_gravelpit", "Gravel Pit",	"#TF_AttackDefend" },
	{ "tc_hydro",		"Hydro",		"#TF_TerritoryControl" },
	{ "ctf_well",		"Well",			"#Gametype_CTF" },
	{ "cp_badlands",	"Badlands",		"#Gametype_CP" },
	{ "pl_goldrush",	"Gold Rush",	"#Gametype_Escort" },
	{ "pl_badwater",	"Badwater Basin",	"#Gametype_Escort" },
	{ "plr_pipeline",	"Pipeline",		"#Gametype_EscortRace" },
	{ "cp_gorge",		"Gorge",		"#TF_AttackDefend" },
	{ "ctf_doublecross",		"Double Cross",		"#Gametype_CTF" },
	{ "pl_thundermountain",	"Thunder Mountain",	"#Gametype_Escort" },
	{ "tr_target",	"Target",		"#GameType_Training" },
	{ "tr_dustbowl",	"Dustbowl",		"#GameType_Training" },
	{ "cp_manor_event",	"Mann Manor",	"#TF_AttackDefend" },
	{ "cp_mountainlab",	"Mountain Lab",	"#TF_AttackDefend" },
	{ "cp_degrootkeep",	"DeGroot Keep",	"#TF_MedievalAttackDefend" },
	{ "pl_barnblitz",	"Barnblitz",	"#Gametype_Escort" },
	{ "pl_upward",	"Upward",	"#Gametype_Escort" },
	{ "plr_hightower",	"Hightower",	"#Gametype_EscortRace" },
	{ "koth_viaduct",	"Viaduct",	"#Gametype_Koth" },
	{ "koth_viaduct_event",	"Eyeaduct",	"#Gametype_Koth" },
	{ "koth_king",	"Kong King",	"#Gametype_Koth" },
	{ "koth_lakeside_event",	"Ghost Fort",	"#Gametype_Koth" },
	{ "plr_hightower_event",	"Helltower",	"#Gametype_EscortRace" },
	{ "rd_asteroid",	"Asteroid",	"#Gametype_RobotDestruction" },
	{ "pl_cactuscanyon",	"Cactus Canyon",	"#Gametype_Escort" },
	{ "sd_doomsday",	"Doomsday",	"#Gametype_SD" },
	{ "sd_doomsday_event",	"Carnival of Carnage",	"#Gametype_SD" },
};

static MapInfo_t s_CommunityMaps[] = {
	{ "pl_borneo", "Borneo", "#Gametype_Escort" },
	{ "koth_suijin", "Suijin", "#Gametype_Koth" },
	{ "cp_snowplow", "Snowplow", "#TF_AttackDefend" },
	{ "koth_probed", "Probed", "#Gametype_Koth" },
	{ "pd_watergate", "Watergate", "#Gametype_PlayerDestruction" },
	{ "arena_byre", "Byre", "#Gametype_Arena" },
	{ "ctf_2fort_invasion", "2Fort Invasion", "#Gametype_CTF" },
	{ "cp_sunshine_event", "Sinshine", "#Gametype_CP" },
	{ "pl_millstone_event", "Hellstone", "#Gametype_Escort" },
	{ "cp_gorge_event", "Gorge Event", "#TF_AttackDefend" },
	{ "koth_moonshine_event", "Moonshine Event", "#Gametype_Koth" },
	{ "pl_snowycoast", "Snowycoast", "#Gametype_Escort" },
	{ "cp_vanguard", "Vanguard", "#Gametype_CP" },
	{ "ctf_landfall", "Landfall", "#Gametype_CTF" },
	{ "koth_highpass", "Highpass", "#Gametype_Koth" },
	{ "koth_maple_ridge_event", "Maple Ridge Event", "#Gametype_Koth" },
	{ "pl_fifthcurve_event", "Brimstone", "#Gametype_Escort" },
	{ "pd_pit_of_death_event", "Pit of Death", "#Gametype_PlayerDestruction" },
};

/*

  !! Commented out until we use this data, but we should keep updating it so its current when we need it

struct FeaturedWorkshopMap_t
{
	// The name the map was shipped as in our files (e.g. it was available as maps/<this>.bsp)
	// NOTE After maps are un-shipped we leave them here and don't change this value, this allows mapcycles that have
	// this map to proper redirect to the workshop in the future
	const char *pShippedName;
	// The workshop ID of the map (The public one, not the the private upload they use to send us the sources)
	PublishedFileId_t nWorkshopID;
};

static FeaturedWorkshopMap_t s_FeaturedWorkshopMaps[] = {
	// !! DO NOT remove these after we stop shipping the file -- they exist to ensure users can refer to
	// !! e.g. "koth_suijin" and get redirected to the workshop map once the file is no longer in our depots.

	// Summer 2015 Operation
	{ "pl_borneo",             454139147 },
	{ "koth_suijin",           454188876 },
	{ "cp_snowplow",           454116615 },

	// September 2015 Invasion Community Update
	{ "koth_probed",           454139808 },
	{ "pd_watergate",          456016898 },
	{ "arena_byre",            454142123 },
	{ "ctf_2fort_invasion",    FIXME     }, // No public workshop entry yet

	// Halloween 2015
	{ "cp_sunshine_event",     532473747 },
	{ "pl_millstone_event",    531384846 },
	{ "cp_gorge_event",        527145539 },
	{ "koth_moonshine_event",  534874830 },

	// December 2015 Campaign
	{ "pl_snowycoast",         469072819 },
	{ "cp_vanguard",           462908782 },
	{ "ctf_landfall",          459651881 },
	{ "koth_highpass",         463803443 },

	// Halloween 2016
	{ "koth_maple_ridge_event",	537540619 },
	{ "pl_fifthcurve_event",	764966851 },
	{ "pd_pit_of_death_event",	537319626 },
};

*/

bool IsValveMap( const char *pMapName )
{
	for ( int i = 0; i < SE_ARRAYSIZE( s_ValveMaps ); ++i )
	{
		if ( V_strieq( s_ValveMaps[i].pDiskName, pMapName ) )
		{
			return true;
		}
	}
	return false;
}

extern ConVar mp_capstyle;
extern ConVar sv_turbophysics;

extern ConVar tf_vaccinator_small_resist;
extern ConVar tf_vaccinator_uber_resist;

extern ConVar tf_teleporter_fov_time;
extern ConVar tf_teleporter_fov_start;

#ifdef GAME_DLL
extern ConVar mp_holiday_nogifts;
extern ConVar tf_debug_damage;
extern ConVar tf_damage_range;
extern ConVar tf_damage_disablespread;
extern ConVar tf_populator_damage_multiplier;
extern ConVar tf_mm_trusted;
extern ConVar tf_weapon_criticals;
extern ConVar tf_weapon_criticals_melee;
extern ConVar mp_idledealmethod;
extern ConVar mp_idlemaxtime;

extern ConVar tf_mm_strict;
extern ConVar mp_autoteambalance;

// STAGING_SPY
ConVar tf_feign_death_activate_damage_scale( "tf_feign_death_activate_damage_scale", "0.25", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar tf_feign_death_damage_scale( "tf_feign_death_damage_scale", "0.35", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar tf_stealth_damage_reduction( "tf_stealth_damage_reduction", "0.8", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

// training
ConVar training_class( "training_class", "3", FCVAR_REPLICATED, "Class to use in training." );
ConVar training_can_build_sentry( "training_can_build_sentry", "1", FCVAR_REPLICATED, "Player can build sentry as engineer." );
ConVar training_can_build_dispenser( "training_can_build_dispenser", "1", FCVAR_REPLICATED, "Player can build dispenser as engineer." );
ConVar training_can_build_tele_entrance( "training_can_build_tele_entrance", "1", FCVAR_REPLICATED, "Player can build teleporter entrance as engineer." );
ConVar training_can_build_tele_exit( "training_can_build_tele_exit", "1", FCVAR_REPLICATED, "Player can build teleporter exit as engineer." );
ConVar training_can_destroy_buildings( "training_can_destroy_buildings", "1", FCVAR_REPLICATED, "Player can destroy buildings as engineer." );
ConVar training_can_pickup_sentry( "training_can_pickup_sentry", "1", FCVAR_REPLICATED, "Player can pickup sentry gun as engineer." );
ConVar training_can_pickup_dispenser( "training_can_pickup_dispenser", "1", FCVAR_REPLICATED, "Player can pickup dispenser as engineer." );
ConVar training_can_pickup_tele_entrance( "training_can_pickup_tele_entrance", "1", FCVAR_REPLICATED, "Player can pickup teleporter entrance as engineer." );
ConVar training_can_pickup_tele_exit( "training_can_pickup_tele_exit", "1", FCVAR_REPLICATED, "Player can pickup teleporter entrance as engineer." );
ConVar training_can_select_weapon_primary	( "training_can_select_weapon_primary", "1", FCVAR_REPLICATED, "In training player select primary weapon." );
ConVar training_can_select_weapon_secondary	( "training_can_select_weapon_secondary", "1", FCVAR_REPLICATED, "In training player select secondary weapon." );
ConVar training_can_select_weapon_melee		( "training_can_select_weapon_melee", "1", FCVAR_REPLICATED, "In training player select melee weapon." );
ConVar training_can_select_weapon_building	( "training_can_select_weapon_building", "1", FCVAR_REPLICATED, "In training player select building tool." );
ConVar training_can_select_weapon_pda		( "training_can_select_weapon_pda", "1", FCVAR_REPLICATED, "In training player select pda." );
ConVar training_can_select_weapon_item1		( "training_can_select_weapon_item1", "1", FCVAR_REPLICATED, "In training player select item 1." );
ConVar training_can_select_weapon_item2		( "training_can_select_weapon_item2", "1", FCVAR_REPLICATED, "In training player select item 2." );

ConVar tf_birthday_ball_chance( "tf_birthday_ball_chance", "100", FCVAR_REPLICATED, "Percent chance of a birthday beach ball spawning at each round start" );

ConVar tf_halloween_boss_spawn_interval( "tf_halloween_boss_spawn_interval", "480", FCVAR_CHEAT, "Average interval between boss spawns, in seconds" );
ConVar tf_halloween_boss_spawn_interval_variation( "tf_halloween_boss_spawn_interval_variation", "60", FCVAR_CHEAT, "Variation of spawn interval +/-" );

ConVar tf_halloween_eyeball_boss_spawn_interval( "tf_halloween_eyeball_boss_spawn_interval", "180", FCVAR_CHEAT, "Average interval between boss spawns, in seconds" );
ConVar tf_halloween_eyeball_boss_spawn_interval_variation( "tf_halloween_eyeball_boss_spawn_interval_variation", "30", FCVAR_CHEAT, "Variation of spawn interval +/-" );

ConVar tf_merasmus_spawn_interval( "tf_merasmus_spawn_interval", "180", FCVAR_CHEAT, "Average interval between boss spawns, in seconds" );
ConVar tf_merasmus_spawn_interval_variation( "tf_merasmus_spawn_interval_variation", "30", FCVAR_CHEAT, "Variation of spawn interval +/-" );

ConVar tf_halloween_zombie_mob_enabled( "tf_halloween_zombie_mob_enabled", "0", FCVAR_CHEAT, "If set to 1, spawn zombie mobs on non-Halloween Valve maps" );
ConVar tf_halloween_zombie_mob_spawn_interval( "tf_halloween_zombie_mob_spawn_interval", "180", FCVAR_CHEAT, "Average interval between zombie mob spawns, in seconds" );
ConVar tf_halloween_zombie_mob_spawn_count( "tf_halloween_zombie_mob_spawn_count", "20", FCVAR_CHEAT, "How many zombies to spawn" );

ConVar tf_halloween_allow_truce_during_boss_event( "tf_halloween_allow_truce_during_boss_event", "0", FCVAR_NOTIFY, "Determines if RED and BLU can damage each other while fighting Monoculus or Merasmus on non-Valve maps." );

ConVar tf_player_spell_drop_on_death_rate( "tf_player_spell_drop_on_death_rate", "0", FCVAR_REPLICATED );
ConVar tf_player_drop_bonus_ducks( "tf_player_drop_bonus_ducks", "-1", FCVAR_REPLICATED, "-1 Default (Holiday-based)\n0 - Force off\n1 - Force on" );

ConVar tf_player_name_change_time( "tf_player_name_change_time", "60", FCVAR_NOTIFY, "Seconds between name changes." );

ConVar tf_weapon_criticals_distance_falloff( "tf_weapon_criticals_distance_falloff", "0", FCVAR_CHEAT, "Critical weapon damage will take distance into account." );
ConVar tf_weapon_minicrits_distance_falloff( "tf_weapon_minicrits_distance_falloff", "0", FCVAR_CHEAT, "Mini-crit weapon damage will take distance into account." );

ConVar mp_spectators_restricted( "mp_spectators_restricted", "0", FCVAR_NONE, "Prevent players on game teams from joining team spectator if it would unbalance the teams." );

ConVar tf_test_special_ducks( "tf_test_special_ducks", "1", FCVAR_DEVELOPMENTONLY );

ConVar tf_mm_abandoned_players_per_team_max( "tf_mm_abandoned_players_per_team_max", "1", FCVAR_DEVELOPMENTONLY );

#endif // GAME_DLL
ConVar tf_mm_next_map_vote_time( "tf_mm_next_map_vote_time", "30", FCVAR_REPLICATED );

#ifdef STAGING_ONLY
#ifdef GAME_DLL
void cc_tf_truce_changed( IConVar *pConVar, const char *pOldString, float flOldValue )
{

	if ( TFGameRules() )
	{
		TFGameRules()->RecalculateTruce();
	}
}
#endif // GAME_DLL
ConVar tf_truce( "tf_truce", "0", FCVAR_REPLICATED, "Force a team truce on or off.", true, 0, true, 1
#ifdef GAME_DLL
	, cc_tf_truce_changed
#endif // GAME_DLL
	);
#endif // STAGING_ONLY

static float g_fEternaweenAutodisableTime = 0.0f;

ConVar tf_spec_xray( "tf_spec_xray", "1", FCVAR_NOTIFY | FCVAR_REPLICATED, "Allows spectators to see player glows. 1 = same team, 2 = both teams" );
ConVar tf_spawn_glows_duration( "tf_spawn_glows_duration", "10", FCVAR_NOTIFY | FCVAR_REPLICATED, "How long should teammates glow after respawning\n" );

#ifdef GAME_DLL
void cc_tf_forced_holiday_changed( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	// Tell the listeners to recalculate the holiday
	IGameEvent *event = gameeventmanager->CreateEvent( "recalculate_holidays" );
	if ( event )
	{
		gameeventmanager->FireEvent( event );
	}
}
#endif // GAME_DLL

ConVar tf_forced_holiday( "tf_forced_holiday", "0", FCVAR_REPLICATED, "Forced holiday, \n   Birthday = 1\n   Halloween = 2\n" //  Christmas = 3\n   Valentines = 4\n   MeetThePyro = 5\n   FullMoon=6
#ifdef GAME_DLL
, cc_tf_forced_holiday_changed
#endif // GAME_DLL
);
ConVar tf_item_based_forced_holiday( "tf_item_based_forced_holiday", "0", FCVAR_REPLICATED, "" 	// like a clone of tf_forced_holiday, but controlled by client consumable item use
#ifdef GAME_DLL
	, cc_tf_forced_holiday_changed
#endif // GAME_DLL
);
ConVar tf_force_holidays_off( "tf_force_holidays_off", "0", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, ""
#ifdef GAME_DLL
, cc_tf_forced_holiday_changed
#endif // GAME_DLL
);
ConVar tf_birthday( "tf_birthday", "0", FCVAR_NOTIFY | FCVAR_REPLICATED );
ConVar tf_spells_enabled( "tf_spells_enabled", "0", FCVAR_NOTIFY | FCVAR_REPLICATED, "Enable to Allow Halloween Spells to be dropped and used by players" );

ConVar tf_caplinear( "tf_caplinear", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "If set to 1, teams must capture control points linearly." );
ConVar tf_stalematechangeclasstime( "tf_stalematechangeclasstime", "20", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Amount of time that players are allowed to change class in stalemates." );
ConVar mp_tournament_redteamname( "mp_tournament_redteamname", "RED", FCVAR_REPLICATED | FCVAR_HIDDEN );
ConVar mp_tournament_blueteamname( "mp_tournament_blueteamname", "BLU", FCVAR_REPLICATED | FCVAR_HIDDEN );

//tagES revisit this later
ConVar tf_attack_defend_map( "tf_attack_defend_map", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );

#ifdef GAME_DLL
void cc_tf_stopwatch_changed( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	IGameEvent *event = gameeventmanager->CreateEvent( "stop_watch_changed" );
	if ( event )
	{
		gameeventmanager->FireEvent( event );
	}
}
#endif // GAME_DLL
ConVar mp_tournament_stopwatch( "mp_tournament_stopwatch", "1", FCVAR_REPLICATED | FCVAR_NOTIFY, "Use Stopwatch mode while using Tournament mode (mp_tournament)"
#ifdef GAME_DLL
	, cc_tf_stopwatch_changed 
#endif
);
ConVar mp_tournament_readymode( "mp_tournament_readymode", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "Enable per-player ready status for tournament mode." );
ConVar mp_tournament_readymode_min( "mp_tournament_readymode_min", "2", FCVAR_REPLICATED | FCVAR_NOTIFY, "Minimum number of players required on the server before players can toggle ready status." );
ConVar mp_tournament_readymode_team_size( "mp_tournament_readymode_team_size", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "Minimum number of players required to be ready per-team before the game can begin." );
ConVar mp_tournament_readymode_countdown( "mp_tournament_readymode_countdown", "10", FCVAR_REPLICATED | FCVAR_NOTIFY, "The number of seconds before a match begins when both teams are ready." );
ConVar mp_windifference( "mp_windifference", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "Score difference between teams before server changes maps", true, 0, false, 0 );
ConVar mp_windifference_min( "mp_windifference_min", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "Minimum score needed for mp_windifference to be applied", true, 0, false, 0 );

ConVar tf_tournament_classlimit_scout( "tf_tournament_classlimit_scout", "-1", FCVAR_REPLICATED, "Tournament mode per-team class limit for Scouts.\n" );
ConVar tf_tournament_classlimit_sniper( "tf_tournament_classlimit_sniper", "-1", FCVAR_REPLICATED, "Tournament mode per-team class limit for Snipers.\n" );
ConVar tf_tournament_classlimit_soldier( "tf_tournament_classlimit_soldier", "-1", FCVAR_REPLICATED, "Tournament mode per-team class limit for Soldiers.\n" );
ConVar tf_tournament_classlimit_demoman( "tf_tournament_classlimit_demoman", "-1", FCVAR_REPLICATED, "Tournament mode per-team class limit for Demomenz.\n" );
ConVar tf_tournament_classlimit_medic( "tf_tournament_classlimit_medic", "-1", FCVAR_REPLICATED, "Tournament mode per-team class limit for Medics.\n" );
ConVar tf_tournament_classlimit_heavy( "tf_tournament_classlimit_heavy", "-1", FCVAR_REPLICATED, "Tournament mode per-team class limit for Heavies.\n" );
ConVar tf_tournament_classlimit_pyro( "tf_tournament_classlimit_pyro", "-1", FCVAR_REPLICATED, "Tournament mode per-team class limit for Pyros.\n" );
ConVar tf_tournament_classlimit_spy( "tf_tournament_classlimit_spy", "-1", FCVAR_REPLICATED, "Tournament mode per-team class limit for Spies.\n" );
ConVar tf_tournament_classlimit_engineer( "tf_tournament_classlimit_engineer", "-1", FCVAR_REPLICATED, "Tournament mode per-team class limit for Engineers.\n" );
ConVar tf_tournament_classchange_allowed( "tf_tournament_classchange_allowed", "1", FCVAR_REPLICATED, "Allow players to change class while the game is active?.\n" );
ConVar tf_tournament_classchange_ready_allowed( "tf_tournament_classchange_ready_allowed", "1", FCVAR_REPLICATED, "Allow players to change class after they are READY?.\n" );

ConVar tf_classlimit( "tf_classlimit", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "Limit on how many players can be any class (i.e. tf_class_limit 2 would limit 2 players per class).\n", true, 0.f, false, 0.f );
ConVar tf_player_movement_restart_freeze( "tf_player_movement_restart_freeze", "1", FCVAR_REPLICATED, "When set, prevent player movement during round restart" );

ConVar tf_autobalance_query_lifetime( "tf_autobalance_query_lifetime", "30", FCVAR_REPLICATED );
ConVar tf_autobalance_xp_bonus( "tf_autobalance_xp_bonus", "150", FCVAR_REPLICATED );

//tagES
#ifdef STAGING_ONLY
ConVar tf_test_match_summary( "tf_test_match_summary", "0", FCVAR_REPLICATED );

#ifdef GAME_DLL
void cc_tf_fake_mm_changed( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( (EMatchGroup)var.GetInt() );
	if ( pMatchDesc )
	{
		pMatchDesc->InitServerSettingsForMatch( NULL );
	}
}
#endif // GAME_DLL
ConVar tf_fake_mm_group( "tf_fake_mm_group", "-1", FCVAR_REPLICATED, "Fake what kind of MM group is being played"
#ifdef GAME_DLL	
	, cc_tf_fake_mm_changed
#endif // GAME_DLL
	);
#endif // STAGING_ONLY

#ifdef GAME_DLL

static const float g_flStrangeEventBatchProcessInterval = 30.0f;

ConVar mp_humans_must_join_team("mp_humans_must_join_team", "any", FCVAR_REPLICATED, "Restricts human players to a single team {any, blue, red, spectator}" );

void cc_tf_medieval_changed( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	bool bOldValue = flOldValue > 0;
	if ( var.IsValid() && ( bOldValue != var.GetBool() ) )
	{
		Msg( "Medieval mode changes take effect after the next map change.\n" );
	}
}

#endif
ConVar tf_medieval( "tf_medieval", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "Enable Medieval Mode.\n", true, 0, true, 1
#ifdef GAME_DLL
				   , cc_tf_medieval_changed
#endif 
				    );

ConVar tf_medieval_autorp( "tf_medieval_autorp", "1", FCVAR_REPLICATED | FCVAR_NOTIFY, "Enable Medieval Mode auto-roleplaying.\n", true, 0, true, 1 );

ConVar tf_sticky_radius_ramp_time( "tf_sticky_radius_ramp_time", "2.0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT | FCVAR_REPLICATED, "Amount of time to get full radius after arming" );
ConVar tf_sticky_airdet_radius( "tf_sticky_airdet_radius", "0.85", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT | FCVAR_REPLICATED, "Radius Scale if detonated in the air" );

#ifdef STAGING_ONLY
ConVar tf_killstreak_alwayson( "tf_killstreak_alwayson", "0", FCVAR_REPLICATED, "enable to have killstreak effects for all players, bots included");

#ifdef GAME_DLL
// Bounty Mode
ConVar tf_bountymode_currency_starting( "tf_bountymode_currency_starting", "1000", FCVAR_ARCHIVE, "How much new players start with when playing Bounty Mode.\n" );
ConVar tf_bountymode_currency_limit( "tf_bountymode_currency_limit", "0", FCVAR_ARCHIVE, "The maximum amount a player can hold in Bounty Mode.\n" );
ConVar tf_bountymode_currency_penalty_ondeath( "tf_bountymode_currency_penalty_ondeath", "0", FCVAR_ARCHIVE, "The percentage of unspent money players lose when they die in Bounty Mode.\n" );
ConVar tf_bountymode_upgrades_wipeondeath( "tf_bountymode_upgrades_wipeondeath", "0", FCVAR_ARCHIVE, "If set to true, wipe player/item upgrades on death.\n" );

void cc_bountymode_changed( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	if ( var.IsValid() && TFGameRules() )
	{
		TFGameRules()->SetBountyMode( var.GetBool() );

		if ( TFGameRules()->IsMannVsMachineMode() )
			return;

		mp_restartgame_immediate.SetValue( 1 );

		if ( !g_pPopulationManager )
		{
			CreateEntityByName( "info_populator" );
		}
		
		int nCurrency = tf_bountymode_currency_starting.GetInt();
		if ( nCurrency > 0 )
		{
			// Give everyone starting money
			for ( int i = 0; i <= MAX_PLAYERS; ++i )
			{
				CTFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( i ) );
				if ( !pPlayer )
					continue;

				pPlayer->SetCurrency( nCurrency );
			}
		}

		g_MannVsMachineUpgrades.LoadUpgradesFile();
		
		IGameEvent *pEvent = gameeventmanager->CreateEvent( "upgrades_file_changed" );
		if ( pEvent )
		{
			pEvent->SetString( "path", "" ); // Have the client load the default
			gameeventmanager->FireEvent( pEvent );
		}
	}
}
#endif // GAME_DLL

ConVar tf_bountymode( "tf_bountymode", "0", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_ARCHIVE, "Allow upgrades and award currency for mission objectives and killing enemy players.\n", true, 0, true, 1
#ifdef GAME_DLL
					  , cc_bountymode_changed
#endif // GAME_DLL
					  );
#endif // STAGING_ONLY

#ifndef GAME_DLL
extern ConVar cl_burninggibs;
extern ConVar english;
ConVar tf_particles_disable_weather( "tf_particles_disable_weather", "0", FCVAR_ARCHIVE, "Disable particles related to weather effects." );
#endif

// arena mode cvars
ConVar tf_arena_force_class( "tf_arena_force_class", "0", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT, "Forces players to play a random class each time they spawn." );
ConVar tf_arena_change_limit( "tf_arena_change_limit", "1", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT, "Number of times players can change their class when mp_force_random_class is being used." );
ConVar tf_arena_override_cap_enable_time( "tf_arena_override_cap_enable_time", "-1", FCVAR_REPLICATED | FCVAR_NOTIFY, "Overrides the time (in seconds) it takes for the capture point to become enable, -1 uses the level designer specified time." );
ConVar tf_arena_override_team_size( "tf_arena_override_team_size", "0", FCVAR_REPLICATED, "Overrides the maximum team size in arena mode. Set to zero to keep the default behavior of 1/3 maxplayers.");
ConVar tf_arena_first_blood( "tf_arena_first_blood", "1", FCVAR_REPLICATED | FCVAR_NOTIFY, "Rewards the first player to get a kill each round." );
extern ConVar tf_arena_preround_time;
extern ConVar tf_arena_max_streak;
#if !defined(NDEBUG) || defined( STAGING_ONLY )
extern ConVar mp_developer;
#endif  // !NDEBUG || STAGING_ONLY

//=============================================================================
// HPE_BEGIN
// [msmith] Used for the client to tell the server that we're watching a movie or not.
//			Also contains the name of a movie if it's an in game video.
//=============================================================================
// Training mode cvars
ConVar tf_training_client_message( "tf_training_client_message", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "A simple way for the training client to communicate with the server." );
//=============================================================================
// HPE_END
//=============================================================================

#define TF_ARENA_MODE_FIRST_BLOOD_CRIT_TIME 5.0f
#define TF_ARENA_MODE_FAST_FIRST_BLOOD_TIME 20.0f
#define TF_ARENA_MODE_SLOW_FIRST_BLOOD_TIME 50.0f

#ifdef TF_RAID_MODE
// Raid mode
ConVar tf_gamemode_raid( "tf_gamemode_raid", "0", FCVAR_REPLICATED | FCVAR_NOTIFY );		// client needs access to this for IsRaidMode()
ConVar tf_raid_enforce_unique_classes( "tf_raid_enforce_unique_classes", "0", FCVAR_REPLICATED | FCVAR_NOTIFY );
ConVar tf_raid_respawn_time( "tf_raid_respawn_time", "5", FCVAR_REPLICATED | FCVAR_NOTIFY /*| FCVAR_CHEAT*/, "How long it takes for a Raider to respawn with his team after death." );
ConVar tf_raid_allow_all_classes( "tf_raid_allow_all_classes", "1", FCVAR_REPLICATED | FCVAR_NOTIFY );

ConVar tf_gamemode_boss_battle( "tf_gamemode_boss_battle", "0", FCVAR_REPLICATED | FCVAR_NOTIFY );

#ifdef GAME_DLL
ConVar tf_raid_allow_overtime( "tf_raid_allow_overtime", "0"/*, FCVAR_CHEAT*/ );
#endif // GAME_DLL
#endif // TF_RAID_MODE

enum { kMVM_MaxConnectedPlayers = 10, };

ConVar tf_mvm_min_players_to_start( "tf_mvm_min_players_to_start", "3", FCVAR_REPLICATED | FCVAR_NOTIFY, "Minimum number of players connected to start a countdown timer" );
ConVar tf_mvm_respec_enabled( "tf_mvm_respec_enabled", "1", FCVAR_CHEAT | FCVAR_REPLICATED, "Allow players to refund credits spent on player and item upgrades." );
ConVar tf_mvm_respec_limit( "tf_mvm_respec_limit", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "The total number of respecs a player can earn.  Default: 0 (no limit).", true, 0.f, true, 100.f );
ConVar tf_mvm_respec_credit_goal( "tf_mvm_respec_credit_goal", "2000", FCVAR_CHEAT | FCVAR_REPLICATED, "When tf_mvm_respec_limit is non-zero, the total amount of money the team must collect to earn a respec credit." );
#ifdef STAGING_ONLY
ConVar tf_mvm_buybacks_method( "tf_mvm_buybacks_method", "1", FCVAR_REPLICATED | FCVAR_HIDDEN, "When set to 0, use the traditional, currency-based system.  When set to 1, use finite, charge-based system.", true, 0.0, true, 1.0 );
#else
ConVar tf_mvm_buybacks_method( "tf_mvm_buybacks_method", "0", FCVAR_REPLICATED | FCVAR_HIDDEN, "When set to 0, use the traditional, currency-based system.  When set to 1, use finite, charge-based system.", true, 0.0, true, 1.0 );
#endif 
ConVar tf_mvm_buybacks_per_wave( "tf_mvm_buybacks_per_wave", "3", FCVAR_REPLICATED | FCVAR_HIDDEN, "The fixed number of buybacks players can use per-wave." );


#ifdef GAME_DLL
enum { kMVM_CurrencyPackMinSize = 1, };
#endif // GAME_DLL

extern ConVar mp_tournament;
extern ConVar mp_tournament_post_match_period;

extern ConVar tf_flag_return_on_touch;
extern ConVar tf_flag_return_time_credit_factor;
ConVar tf_grapplinghook_enable( "tf_grapplinghook_enable", "0", FCVAR_REPLICATED );

#ifdef GAME_DLL
CUtlString s_strNextMvMPopFile;
CON_COMMAND_F( tf_mvm_popfile, "Change to a target popfile for MvM", FCVAR_GAMEDLL )
{
	// Listenserver host or rcon access only!
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( args.ArgC() <= 1 )
	{
		if ( TFGameRules() && g_pPopulationManager )
		{
			const char *pszFile = g_pPopulationManager->GetPopulationFilename();
			if ( pszFile && pszFile[0] )
			{
				Msg( "Current popfile is: %s\n", pszFile );
				return;
			}
		}

		Msg( "Missing Popfile name\n" );
		return;
	}

	const char *pszShortName = args.Arg(1);

	if ( !TFGameRules() || !g_pPopulationManager )
	{
		Warning( "Cannot set population file before map load.\n" );
		return;
	}

	// Make sure we have a file system
	if ( !g_pFullFileSystem )
	{
		Msg( "No File System to find Popfile to load\n" );
		return;
	}

	// Form full path
	CUtlString fullPath;

	if ( g_pPopulationManager->FindPopulationFileByShortName( pszShortName, fullPath ) )
	{
		g_pPopulationManager->SetPopulationFilename( fullPath );
		g_pPopulationManager->ResetMap();
		return;
	}

	// Give them a message to make it clear what file we were looking for
	Warning( "Could not find a population file matching: %s.\n", pszShortName );
}

#ifdef STAGING_ONLY
// Never ship this
CON_COMMAND_F( tf_competitive_mode_force_victory, "For testing.", FCVAR_GAMEDLL )
{
	// Listenserver host or rcon access only!
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CTFPlayer *pPlayer = ToTFPlayer( UTIL_GetCommandClient() );
	if ( !pPlayer )
		return;

	CSteamID steamIDForPlayer;
	if ( !pPlayer->GetSteamID( &steamIDForPlayer ) )
		return;

	// TODO: Rewrite this based on the shipping version of comp/casual
}
#endif // STAGING_ONLY
#endif

static bool BIsCvarIndicatingHolidayIsActive( int iCvarValue, /*EHoliday*/ int eHoliday )
{
	if ( iCvarValue == 0 )
		return false;

	// Unfortunately Holidays are not a proper bitfield
	switch ( eHoliday )
	{
	case kHoliday_TFBirthday:						return iCvarValue == kHoliday_TFBirthday;
	case kHoliday_Halloween:						return iCvarValue == kHoliday_Halloween || iCvarValue == kHoliday_HalloweenOrFullMoon || iCvarValue == kHoliday_HalloweenOrFullMoonOrValentines;
	case kHoliday_Christmas:						return iCvarValue == kHoliday_Christmas;
	case kHoliday_Valentines:						return iCvarValue == kHoliday_Valentines || iCvarValue == kHoliday_HalloweenOrFullMoonOrValentines;
	case kHoliday_MeetThePyro:						return iCvarValue == kHoliday_MeetThePyro;
	case kHoliday_FullMoon:							return iCvarValue == kHoliday_FullMoon || iCvarValue == kHoliday_HalloweenOrFullMoon || iCvarValue == kHoliday_HalloweenOrFullMoonOrValentines;
	case kHoliday_HalloweenOrFullMoon:				return iCvarValue == kHoliday_Halloween || iCvarValue == kHoliday_FullMoon || iCvarValue == kHoliday_HalloweenOrFullMoon || iCvarValue == kHoliday_HalloweenOrFullMoonOrValentines;
	case kHoliday_HalloweenOrFullMoonOrValentines:	return iCvarValue == kHoliday_Halloween || iCvarValue == kHoliday_FullMoon || iCvarValue == kHoliday_Valentines || iCvarValue == kHoliday_HalloweenOrFullMoon || iCvarValue == kHoliday_HalloweenOrFullMoonOrValentines;
	case kHoliday_AprilFools:						return iCvarValue == kHoliday_AprilFools;
	case kHoliday_EOTL:								return iCvarValue == kHoliday_EOTL;
	case kHoliday_CommunityUpdate:					return iCvarValue == kHoliday_CommunityUpdate;
	}

	return false;
}

// Fetch holiday setting taking into account convars, etc, but NOT
// taking into consideration the current game rules, map, etc.
//
// This version can be used outside of gameplay, ie., for matchmaking
bool TF_IsHolidayActive( /*EHoliday*/ int eHoliday )
{
	if ( IsX360() || tf_force_holidays_off.GetBool() )
		return false;

	if ( BIsCvarIndicatingHolidayIsActive( tf_forced_holiday.GetInt(), eHoliday ) )
		return true;

	if ( BIsCvarIndicatingHolidayIsActive( tf_item_based_forced_holiday.GetInt(), eHoliday ) )
		return true;

	if ( ( eHoliday == kHoliday_TFBirthday ) && tf_birthday.GetBool() )
		return true;

	if ( TFGameRules() )
	{
		if ( eHoliday == kHoliday_HalloweenOrFullMoon )
		{
			if ( TFGameRules()->IsHolidayMap( kHoliday_Halloween ) )
				return true;
			if ( TFGameRules()->IsHolidayMap( kHoliday_FullMoon ) )
				return true;
		}
		if ( TFGameRules()->IsHolidayMap( eHoliday ) )
		{
			return true;
		}
	}

	return UTIL_IsHolidayActive( eHoliday );
}

#ifdef TF_CREEP_MODE
ConVar tf_gamemode_creep_wave( "tf_gamemode_creep_wave", "0", FCVAR_REPLICATED | FCVAR_NOTIFY );
ConVar tf_creep_wave_player_respawn_time( "tf_creep_wave_player_respawn_time", "10", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_CHEAT, "How long it takes for a player to respawn with his team after death." );
#endif

#ifdef GAME_DLL
// TF overrides the default value of this convar

#ifndef NDEBUG
#define WAITING_FOR_PLAYERS_FLAGS	0
#else
#define WAITING_FOR_PLAYERS_FLAGS	FCVAR_DEVELOPMENTONLY
#endif

ConVar hide_server( "hide_server", "0", FCVAR_GAMEDLL, "Whether the server should be hidden from the master server" );

ConVar mp_waitingforplayers_time( "mp_waitingforplayers_time", (IsX360()?"15":"30"), FCVAR_GAMEDLL | WAITING_FOR_PLAYERS_FLAGS, "WaitingForPlayers time length in seconds" );

ConVar tf_gamemode_arena ( "tf_gamemode_arena", "0", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_DEVELOPMENTONLY );
ConVar tf_gamemode_cp ( "tf_gamemode_cp", "0", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_DEVELOPMENTONLY );
ConVar tf_gamemode_ctf ( "tf_gamemode_ctf", "0", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_DEVELOPMENTONLY );
ConVar tf_gamemode_sd ( "tf_gamemode_sd", "0", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_DEVELOPMENTONLY );
ConVar tf_gamemode_rd ( "tf_gamemode_rd", "0", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_DEVELOPMENTONLY );
ConVar tf_gamemode_pd ( "tf_gamemode_pd", "0", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_DEVELOPMENTONLY );
ConVar tf_gamemode_tc ( "tf_gamemode_tc", "0", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_DEVELOPMENTONLY );
ConVar tf_beta_content ( "tf_beta_content", "0", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_DEVELOPMENTONLY );
ConVar tf_gamemode_payload ( "tf_gamemode_payload", "0", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_DEVELOPMENTONLY );
ConVar tf_gamemode_mvm ( "tf_gamemode_mvm", "0", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_DEVELOPMENTONLY );
ConVar tf_gamemode_passtime ( "tf_gamemode_passtime", "0", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_DEVELOPMENTONLY );
ConVar tf_gamemode_misc ( "tf_gamemode_misc", "0", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_DEVELOPMENTONLY );

ConVar tf_bot_count( "tf_bot_count", "0", FCVAR_NOTIFY | FCVAR_DEVELOPMENTONLY );

#ifndef NDEBUG
ConVar tf_debug_ammo_and_health( "tf_debug_ammo_and_health", "0", FCVAR_CHEAT );
#endif  // !NDEBUG

static Vector s_BotSpawnPosition;

ConVar tf_gravetalk( "tf_gravetalk", "1", FCVAR_NOTIFY, "Allows living players to hear dead players using text/voice chat.", true, 0, true, 1 );

ConVar tf_ctf_bonus_time ( "tf_ctf_bonus_time", "10", FCVAR_NOTIFY, "Length of team crit time for CTF capture." );

#ifndef NDEBUG
ConVar mp_scrambleteams_debug( "mp_scrambleteams_debug", "0", FCVAR_NONE, "Debug spew." );
#endif  // !NDEBUG

#ifdef STAGING_ONLY
ConVar mp_tournament_readymode_bots_allowed( "mp_tournament_readymode_bots_allowed", "0", FCVAR_ARCHIVE, "Allow bot data to go through the system for debugging." );
#endif // STAGING_ONLY

extern ConVar tf_mm_servermode;
extern ConVar tf_flag_caps_per_round;

void cc_competitive_mode( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	IGameEvent *event = gameeventmanager->CreateEvent( "competitive_state_changed" );
	if ( event )
	{
		// Server-side here.  Client-side down below in the RecvProxy
		gameeventmanager->FireEvent( event, true );
	}
}
ConVar tf_competitive_preround_duration( "tf_competitive_preround_duration", "3", FCVAR_REPLICATED, "How long we stay in pre-round when in competitive games." );
ConVar tf_competitive_preround_countdown_duration( "tf_competitive_preround_countdown_duration", "10.5", FCVAR_HIDDEN, "How long we stay in countdown when in competitive games." );
ConVar tf_competitive_abandon_method( "tf_competitive_abandon_method", "0", FCVAR_HIDDEN );
ConVar tf_competitive_required_late_join_timeout( "tf_competitive_required_late_join_timeout", "120", FCVAR_DEVELOPMENTONLY,
                                                  "How long to wait for late joiners in matches requiring full player counts before canceling the match" );
ConVar tf_competitive_required_late_join_confirm_timeout( "tf_competitive_required_late_join_confirm_timeout", "30", FCVAR_DEVELOPMENTONLY,
                                                          "How long to wait for the GC to confirm we're in the late join pool before canceling the match" );
#endif // GAME_DLL

#ifdef GAME_DLL
void cc_powerup_mode( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	if ( var.IsValid() )
	{
		if ( !TFGameRules() )
			return;

		if ( var.GetBool() )
		{
			if ( TFGameRules()->IsMannVsMachineMode() )
				return;
		}

		TFGameRules()->SetPowerupMode( var.GetBool() );
		TFGameRules()->State_Transition( GR_STATE_PREROUND );
		tf_flag_caps_per_round.SetValue( var.GetBool() ? 7 : 3 );	// Hack
	}
}

ConVar tf_powerup_mode( "tf_powerup_mode", "0", FCVAR_NOTIFY, "Enable/disable powerup mode. Not compatible with Mann Vs Machine mode", cc_powerup_mode );
ConVar tf_powerup_mode_imbalance_delta( "tf_powerup_mode_imbalance_delta", "24", FCVAR_CHEAT, "Powerup kill score lead one team must have before imbalance measures are initiated" );

ConVar tf_skillrating_update_interval( "tf_skillrating_update_interval", "180", FCVAR_ARCHIVE, "How often to update the GC and OGS." );

extern ConVar mp_teams_unbalance_limit;

static bool g_bRandomMap = false;

void cc_RandomMap( const CCommand& args )
{
	CTFGameRules *pRules = TFGameRules();
	if ( pRules )
	{
		if ( !UTIL_IsCommandIssuedByServerAdmin() )
			return;

		g_bRandomMap = true;
	}
	else
	{
		// There's no game rules object yet, so let's load the map cycle and pick a map.
		char mapcfile[MAX_PATH];
		CMultiplayRules::DetermineMapCycleFilename( mapcfile, sizeof(mapcfile), true );
		if ( !mapcfile[0] )
		{
			Msg( "No mapcyclefile specified. Cannot pick a random map.\n" );
			return;
		}

		CUtlVector<char*> mapList;
		// No gamerules entity yet, since we don't need the fixups to find a map just use the base version
		CMultiplayRules::RawLoadMapCycleFileIntoVector ( mapcfile, mapList );
		if ( !mapList.Count() )
		{
			Msg( "Map cycle file \"%s\" contains no valid maps or cannot be read. Cannot pick a random map.\n", mapcfile );
			return;
		}

		int iMapIndex = RandomInt( 0, mapList.Count() - 1 );
		Msg ( "randommap: selecting map %i out of %i\n", iMapIndex + 1, mapList.Count() );
		engine->ServerCommand( UTIL_VarArgs( "map %s\n", mapList[iMapIndex] ) );

		CMultiplayRules::FreeMapCycleFileVector( mapList );
	}
}

// Simple class for tracking previous gamemode across level transitions
// Allows clean-up of UI/state when going between things like MvM and PvP
class CTFGameModeHistory : public CAutoGameSystem
{
public:
	virtual bool Init()
	{
		m_nPrevState = 0;
		return true;
	}
	void SetPrevState( int nState ) { m_nPrevState = nState; }
	int	 GetPrevState( void ) { return m_nPrevState; }
private:
	int m_nPrevState;
} g_TFGameModeHistory;

static ConCommand randommap( "randommap", cc_RandomMap, "Changelevel to a random map in the mapcycle file" );
#endif	// GAME_DLL

#ifdef GAME_DLL
static bool PlayerHasDuckStreaks( CTFPlayer *pPlayer )
{
	CEconItemView *pActionItem = pPlayer->GetEquippedItemForLoadoutSlot( LOADOUT_POSITION_ACTION );
	if ( !pActionItem )
		return false;

	// Duck Badge Cooldown is based on badge level.  Noisemaker is more like an easter egg
	static CSchemaAttributeDefHandle pAttr_DuckStreaks( "duckstreaks active" );
	uint32 iDuckStreaksActive = 0;

	// Don't care about the level, just if the attribute is found
	return FindAttribute_UnsafeBitwiseCast<attrib_value_t>( pActionItem, pAttr_DuckStreaks, &iDuckStreaksActive ) && iDuckStreaksActive > 0;
}

void ValidateCapturesPerRound( IConVar *pConVar, const char *oldValue, float flOldValue )
{
	ConVarRef var( pConVar );

	if ( var.GetInt() <= 0 )
	{
		// reset the flag captures being played in the current round
		int nTeamCount = TFTeamMgr()->GetTeamCount();
		for ( int iTeam = FIRST_GAME_TEAM; iTeam < nTeamCount; ++iTeam )
		{
			CTFTeam *pTeam = GetGlobalTFTeam( iTeam );
			if ( !pTeam )
				continue;

			pTeam->SetFlagCaptures( 0 );
		}
	}
}

static void Coaching_Start( CTFPlayer *pCoach, CTFPlayer *pStudent )
{
	pCoach->SetIsCoaching( true );
	pCoach->ForceChangeTeam( TEAM_SPECTATOR );
	pCoach->SetObserverTarget( pStudent );
	pCoach->StartObserverMode( OBS_MODE_CHASE );
	pCoach->SetStudent( pStudent );
	pStudent->SetCoach( pCoach );
}

static void Coaching_Stop( CTFPlayer *pCoach )
{
	CTFPlayer *pStudent = pCoach->GetStudent();
	if ( pStudent )
	{
		pStudent->SetCoach( NULL );
	}
	pCoach->SetIsCoaching( false );
	pCoach->SetStudent( NULL );
	pCoach->ForceChangeTeam( TEAM_SPECTATOR );
}

#endif	

ConVar tf_flag_caps_per_round( "tf_flag_caps_per_round", "3", FCVAR_REPLICATED, "Number of captures per round on CTF and PASS Time maps. Set to 0 to disable.", true, 0, false, 0
#ifdef GAME_DLL
							  , ValidateCapturesPerRound
#endif
							  );


/**
 * Player hull & eye position for standing, ducking, etc.  This version has a taller
 * player height, but goldsrc-compatible collision bounds.
 */
static CViewVectors g_TFViewVectors(
	Vector( 0, 0, 72 ),		//VEC_VIEW (m_vView) eye position
							
	Vector(-24, -24, 0 ),	//VEC_HULL_MIN (m_vHullMin) hull min
	Vector( 24,  24, 82 ),	//VEC_HULL_MAX (m_vHullMax) hull max
												
	Vector(-24, -24, 0 ),	//VEC_DUCK_HULL_MIN (m_vDuckHullMin) duck hull min
	Vector( 24,  24, 62 ),	//VEC_DUCK_HULL_MAX	(m_vDuckHullMax) duck hull max
	Vector( 0, 0, 45 ),		//VEC_DUCK_VIEW		(m_vDuckView) duck view
												
	Vector( -10, -10, -10 ),	//VEC_OBS_HULL_MIN	(m_vObsHullMin) observer hull min
	Vector(  10,  10,  10 ),	//VEC_OBS_HULL_MAX	(m_vObsHullMax) observer hull max
												
	Vector( 0, 0, 14 )		//VEC_DEAD_VIEWHEIGHT (m_vDeadViewHeight) dead view height
);							

Vector g_TFClassViewVectors[11] =
{
	Vector( 0, 0, 72 ),		// TF_CLASS_UNDEFINED

	Vector( 0, 0, 65 ),		// TF_CLASS_SCOUT,			// TF_FIRST_NORMAL_CLASS
	Vector( 0, 0, 75 ),		// TF_CLASS_SNIPER,
	Vector( 0, 0, 68 ),		// TF_CLASS_SOLDIER,
	Vector( 0, 0, 68 ),		// TF_CLASS_DEMOMAN,
	Vector( 0, 0, 75 ),		// TF_CLASS_MEDIC,
	Vector( 0, 0, 75 ),		// TF_CLASS_HEAVYWEAPONS,
	Vector( 0, 0, 68 ),		// TF_CLASS_PYRO,
	Vector( 0, 0, 75 ),		// TF_CLASS_SPY,
	Vector( 0, 0, 68 ),		// TF_CLASS_ENGINEER,

	Vector( 0, 0, 65 ),		// TF_CLASS_CIVILIAN,		// TF_LAST_NORMAL_CLASS
};

const CViewVectors *CTFGameRules::GetViewVectors() const
{
	return &g_TFViewVectors;
}

REGISTER_GAMERULES_CLASS( CTFGameRules );

#ifdef CLIENT_DLL
void RecvProxy_MatchSummary( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	bool bMatchSummary = ( pData->m_Value.m_Int > 0 );
	if ( bMatchSummary && !(*(bool*)(pOut)))
	{
		C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
		if ( pLocalPlayer )
		{
			pLocalPlayer->TurnOffTauntCam();
			pLocalPlayer->TurnOffTauntCam_Finish();;
		}

		IGameEvent *event = gameeventmanager->CreateEvent( "show_match_summary" );
		if ( event )
		{
			gameeventmanager->FireEventClientSide( event );
		}
	}

	*(bool*)(pOut) = bMatchSummary;
}

void RecvProxy_CompetitiveMode( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	*(bool*)(pOut) = ( pData->m_Value.m_Int > 0 );

	IGameEvent *event = gameeventmanager->CreateEvent( "competitive_state_changed" );
	if ( event )
	{
		// Client-side once it's actually happened
		gameeventmanager->FireEventClientSide( event );
	}
}

void RecvProxy_PlayerVotedForMap( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	if ( *(int*)(pOut) != pData->m_Value.m_Int )
	{
		*(int*)(pOut) = pData->m_Value.m_Int;

		IGameEvent *event = gameeventmanager->CreateEvent( "player_next_map_vote_change" );
		if ( event )
		{
			event->SetInt( "map_index", pData->m_Value.m_Int );
			event->SetInt( "vote", pData->m_Value.m_Int );
			// Client-side once it's actually happened
			gameeventmanager->FireEventClientSide( event );
		}
	}
}

void RecvProxy_NewMapVoteStateChanged( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	bool bChange = *(int*)(pOut) != pData->m_Value.m_Int;
	*(int*)(pOut) = pData->m_Value.m_Int;

	if ( bChange )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "vote_maps_changed" );
		if ( event )
		{
			// Client-side once it's actually happened
			gameeventmanager->FireEventClientSide( event );
		}
	}
}
#endif

BEGIN_NETWORK_TABLE_NOBASE( CTFGameRules, DT_TFGameRules )
#ifdef CLIENT_DLL

	RecvPropInt( RECVINFO( m_nGameType ) ),
	RecvPropInt( RECVINFO( m_nStopWatchState ) ),
	RecvPropString( RECVINFO( m_pszTeamGoalStringRed ) ),
	RecvPropString( RECVINFO( m_pszTeamGoalStringBlue ) ),
	RecvPropTime( RECVINFO( m_flCapturePointEnableTime ) ),

//=============================================================================
// HPE_BEGIN:
// [msmith]	Training status and HUD Type.
//=============================================================================
	RecvPropInt( RECVINFO( m_nHudType ) ),
	RecvPropBool( RECVINFO( m_bIsInTraining ) ),
	RecvPropBool( RECVINFO( m_bAllowTrainingAchievements ) ),
	RecvPropBool( RECVINFO( m_bIsWaitingForTrainingContinue ) ),
//=============================================================================
// HPE_END
//=============================================================================
	RecvPropBool( RECVINFO( m_bIsTrainingHUDVisible ) ),
	RecvPropBool( RECVINFO( m_bIsInItemTestingMode ) ),

	RecvPropEHandle( RECVINFO( m_hBonusLogic ) ),
	RecvPropBool( RECVINFO( m_bPlayingKoth ) ),
	RecvPropBool( RECVINFO( m_bPlayingMedieval ) ),
	RecvPropBool( RECVINFO( m_bPlayingHybrid_CTF_CP ) ),
	RecvPropBool( RECVINFO( m_bPlayingSpecialDeliveryMode ) ),
	RecvPropBool( RECVINFO( m_bPlayingRobotDestructionMode ) ),
	RecvPropEHandle( RECVINFO( m_hRedKothTimer ) ),
	RecvPropEHandle( RECVINFO( m_hBlueKothTimer ) ),
	RecvPropInt( RECVINFO( m_nMapHolidayType ) ),

	RecvPropEHandle( RECVINFO( m_itHandle ) ),
	RecvPropBool( RECVINFO( m_bPlayingMannVsMachine ) ),
	RecvPropEHandle( RECVINFO( m_hBirthdayPlayer ) ),

	RecvPropInt( RECVINFO( m_nBossHealth ) ),
	RecvPropInt( RECVINFO( m_nMaxBossHealth ) ),
	RecvPropInt( RECVINFO( m_fBossNormalizedTravelDistance ) ),
	RecvPropBool( RECVINFO( m_bMannVsMachineAlarmStatus ) ),
	RecvPropBool( RECVINFO( m_bHaveMinPlayersToEnableReady ) ),

	RecvPropBool( RECVINFO( m_bBountyModeEnabled ) ),

	RecvPropInt( RECVINFO( m_nHalloweenEffect ) ),
	RecvPropFloat( RECVINFO( m_fHalloweenEffectStartTime ) ),
	RecvPropFloat( RECVINFO( m_fHalloweenEffectDuration ) ),
	RecvPropInt( RECVINFO( m_halloweenScenario ) ),
	RecvPropBool( RECVINFO( m_bHelltowerPlayersInHell ) ),
	RecvPropBool( RECVINFO( m_bIsUsingSpells ) ),
	RecvPropBool( RECVINFO( m_bCompetitiveMode ), 0, RecvProxy_CompetitiveMode ),
	RecvPropInt( RECVINFO( m_nMatchGroupType ) ),
	RecvPropBool( RECVINFO( m_bMatchEnded ) ),
	RecvPropBool( RECVINFO( m_bPowerupMode ) ),
	RecvPropString( RECVINFO( m_pszCustomUpgradesFile ) ),
	RecvPropBool( RECVINFO( m_bTruceActive ) ),
	RecvPropBool( RECVINFO( m_bShowMatchSummary ), 0, RecvProxy_MatchSummary ),
	RecvPropBool( RECVINFO_NAME( m_bShowMatchSummary, "m_bShowCompetitiveMatchSummary" ), 0, RecvProxy_MatchSummary ),     // Renamed
	RecvPropBool( RECVINFO( m_bTeamsSwitched ) ),
	RecvPropBool( RECVINFO( m_bMapHasMatchSummaryStage ) ),
	RecvPropBool( RECVINFO( m_bPlayersAreOnMatchSummaryStage ) ),
	RecvPropBool( RECVINFO( m_bStopWatchWinner ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_ePlayerWantsRematch), RecvPropInt( RECVINFO(m_ePlayerWantsRematch[0]), 0, RecvProxy_PlayerVotedForMap ) ),
	RecvPropInt( RECVINFO( m_eRematchState ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_nNextMapVoteOptions), RecvPropInt( RECVINFO(m_nNextMapVoteOptions[0]), 0, RecvProxy_NewMapVoteStateChanged ) ),
#else

	SendPropInt( SENDINFO( m_nGameType ), 4, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nStopWatchState ), 3, SPROP_UNSIGNED ),
	SendPropString( SENDINFO( m_pszTeamGoalStringRed ) ),
	SendPropString( SENDINFO( m_pszTeamGoalStringBlue ) ),
	SendPropTime( SENDINFO( m_flCapturePointEnableTime ) ),

//=============================================================================
// HPE_BEGIN:
// [msmith]	Training status and hud type.
//=============================================================================
	SendPropInt( SENDINFO( m_nHudType ), 3, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bIsInTraining ) ),
	SendPropBool( SENDINFO( m_bAllowTrainingAchievements ) ),
	SendPropBool( SENDINFO( m_bIsWaitingForTrainingContinue ) ),
//=============================================================================
// HPE_END
//=============================================================================
	SendPropBool( SENDINFO( m_bIsTrainingHUDVisible ) ),
	SendPropBool( SENDINFO( m_bIsInItemTestingMode ) ),

	SendPropEHandle( SENDINFO( m_hBonusLogic ) ),
	SendPropBool( SENDINFO( m_bPlayingKoth ) ),
	SendPropBool( SENDINFO( m_bPlayingMedieval ) ),
	SendPropBool( SENDINFO( m_bPlayingHybrid_CTF_CP ) ),
	SendPropBool( SENDINFO( m_bPlayingSpecialDeliveryMode ) ),
	SendPropBool( SENDINFO( m_bPlayingRobotDestructionMode ) ),
	SendPropEHandle( SENDINFO( m_hRedKothTimer ) ),
	SendPropEHandle( SENDINFO( m_hBlueKothTimer ) ),
	SendPropInt( SENDINFO( m_nMapHolidayType ), 3, SPROP_UNSIGNED ),

	SendPropEHandle( SENDINFO( m_itHandle ) ),
	SendPropBool( SENDINFO( m_bPlayingMannVsMachine ) ),
	SendPropEHandle( SENDINFO( m_hBirthdayPlayer ) ),

	SendPropInt( SENDINFO( m_nBossHealth ) ),
	SendPropInt( SENDINFO( m_nMaxBossHealth ) ),
	SendPropInt( SENDINFO( m_fBossNormalizedTravelDistance ) ),
	SendPropBool( SENDINFO( m_bMannVsMachineAlarmStatus ) ),
	SendPropBool( SENDINFO( m_bHaveMinPlayersToEnableReady ) ),

	SendPropBool( SENDINFO( m_bBountyModeEnabled ) ),

	SendPropInt( SENDINFO( m_nHalloweenEffect ) ),
	SendPropFloat( SENDINFO( m_fHalloweenEffectStartTime ) ),
	SendPropFloat( SENDINFO( m_fHalloweenEffectDuration ) ),
	SendPropInt( SENDINFO( m_halloweenScenario ) ),
	SendPropBool( SENDINFO( m_bHelltowerPlayersInHell ) ),
	SendPropBool( SENDINFO( m_bIsUsingSpells ) ),
	SendPropBool( SENDINFO( m_bCompetitiveMode ) ),
	SendPropBool( SENDINFO( m_bPowerupMode ) ),
	SendPropInt( SENDINFO( m_nMatchGroupType ) ),
	SendPropBool( SENDINFO( m_bMatchEnded ) ),
	SendPropString( SENDINFO( m_pszCustomUpgradesFile ) ),
	SendPropBool( SENDINFO( m_bTruceActive ) ),
	SendPropBool( SENDINFO( m_bShowMatchSummary ) ),
	SendPropBool( SENDINFO( m_bTeamsSwitched ) ),
	SendPropBool( SENDINFO( m_bMapHasMatchSummaryStage ) ),
	SendPropBool( SENDINFO( m_bPlayersAreOnMatchSummaryStage ) ),
	SendPropBool( SENDINFO( m_bStopWatchWinner ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_ePlayerWantsRematch), SendPropInt( SENDINFO_ARRAY(m_ePlayerWantsRematch), -1, SPROP_UNSIGNED | SPROP_VARINT ) ),
	SendPropInt( SENDINFO( m_eRematchState ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_nNextMapVoteOptions), SendPropInt( SENDINFO_ARRAY(m_nNextMapVoteOptions), -1, SPROP_UNSIGNED | SPROP_VARINT ) ),
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( tf_gamerules, CTFGameRulesProxy );
IMPLEMENT_NETWORKCLASS_ALIASED( TFGameRulesProxy, DT_TFGameRulesProxy )

#ifdef CLIENT_DLL
	void RecvProxy_TFGameRules( const RecvProp *pProp, void **pOut, void *pData, int objectID )
	{
		CTFGameRules *pRules = TFGameRules();
		Assert( pRules );
		*pOut = pRules;
	}

	BEGIN_RECV_TABLE( CTFGameRulesProxy, DT_TFGameRulesProxy )
		RecvPropDataTable( "tf_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE( DT_TFGameRules ), RecvProxy_TFGameRules )
	END_RECV_TABLE()
#else
	void *SendProxy_TFGameRules( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
	{
		CTFGameRules *pRules = TFGameRules();
		Assert( pRules );
		pRecipients->SetAllRecipients();
		return pRules;
	}

	BEGIN_SEND_TABLE( CTFGameRulesProxy, DT_TFGameRulesProxy )
		SendPropDataTable( "tf_gamerules_data", 0, &REFERENCE_SEND_TABLE( DT_TFGameRules ), SendProxy_TFGameRules )
	END_SEND_TABLE()
#endif

#ifdef GAME_DLL
BEGIN_DATADESC( CTFGameRulesProxy )
//=============================================================================
// HPE_BEGIN:
// [msmith]	Training HUD type
//=============================================================================
	DEFINE_KEYFIELD( m_nHudType, FIELD_INTEGER, "hud_type" ),
//=============================================================================
// HPE_END
//=============================================================================

	DEFINE_KEYFIELD( m_bOvertimeAllowedForCTF, FIELD_BOOLEAN, "ctf_overtime" ),

	// Inputs.
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetRedTeamRespawnWaveTime", InputSetRedTeamRespawnWaveTime ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetBlueTeamRespawnWaveTime", InputSetBlueTeamRespawnWaveTime ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "AddRedTeamRespawnWaveTime", InputAddRedTeamRespawnWaveTime ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "AddBlueTeamRespawnWaveTime", InputAddBlueTeamRespawnWaveTime ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetRedTeamGoalString", InputSetRedTeamGoalString ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetBlueTeamGoalString", InputSetBlueTeamGoalString ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetRedTeamRole", InputSetRedTeamRole ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetBlueTeamRole", InputSetBlueTeamRole ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetRequiredObserverTarget", InputSetRequiredObserverTarget ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "AddRedTeamScore", InputAddRedTeamScore ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "AddBlueTeamScore", InputAddBlueTeamScore ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SetRedKothClockActive", InputSetRedKothClockActive ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SetBlueKothClockActive", InputSetBlueKothClockActive ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetCTFCaptureBonusTime", InputSetCTFCaptureBonusTime ),
	DEFINE_INPUTFUNC( FIELD_STRING, "PlayVORed", InputPlayVORed ),
	DEFINE_INPUTFUNC( FIELD_STRING, "PlayVOBlue", InputPlayVOBlue ),
	DEFINE_INPUTFUNC( FIELD_STRING, "PlayVO", InputPlayVO ),
	DEFINE_INPUTFUNC( FIELD_STRING, "HandleMapEvent", InputHandleMapEvent ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetCustomUpgradesFile", InputSetCustomUpgradesFile ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetRoundRespawnFreezeEnabled", InputSetRoundRespawnFreezeEnabled ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetMapForcedTruceDuringBossFight", InputSetMapForcedTruceDuringBossFight ),

	DEFINE_OUTPUT( m_OnWonByTeam1,	"OnWonByTeam1" ),
	DEFINE_OUTPUT( m_OnWonByTeam2,	"OnWonByTeam2" ),
	DEFINE_OUTPUT( m_Team1PlayersChanged,	"Team1PlayersChanged" ),
	DEFINE_OUTPUT( m_Team2PlayersChanged,	"Team2PlayersChanged" ),
	DEFINE_OUTPUT( m_OnPowerupImbalanceTeam1, "OnPowerupImbalanceTeam1" ),
	DEFINE_OUTPUT( m_OnPowerupImbalanceTeam2, "OnPowerupImbalanceTeam2" ),
	DEFINE_OUTPUT( m_OnPowerupImbalanceMeasuresOver, "OnPowerupImbalanceMeasuresOver" ),
	DEFINE_OUTPUT( m_OnStateEnterRoundRunning, "OnStateEnterRoundRunning" ),
	DEFINE_OUTPUT( m_OnStateEnterBetweenRounds, "OnStateEnterBetweenRounds" ),
	DEFINE_OUTPUT( m_OnStateEnterPreRound, "OnStateEnterPreRound" ),
	DEFINE_OUTPUT( m_OnStateExitPreRound, "OnStateExitPreRound" ),
	DEFINE_OUTPUT( m_OnMatchSummaryStart, "OnMatchSummaryStart" ),
	DEFINE_OUTPUT( m_OnTruceStart, "OnTruceStart" ),
	DEFINE_OUTPUT( m_OnTruceEnd, "OnTruceEnd" ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFGameRulesProxy::CTFGameRulesProxy()
{
	m_nHudType = TF_HUDTYPE_UNDEFINED;
	m_bOvertimeAllowedForCTF = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputAddRedTeamScore( inputdata_t &inputdata )
{
	TFTeamMgr()->AddTeamScore( TF_TEAM_RED, inputdata.value.Int() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputAddBlueTeamScore( inputdata_t &inputdata )
{
	TFTeamMgr()->AddTeamScore( TF_TEAM_BLUE, inputdata.value.Int() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputSetRedKothClockActive( inputdata_t &inputdata )
{
	if ( TFGameRules() )
	{
		variant_t sVariant;
		CTeamRoundTimer *pTimer = TFGameRules()->GetKothTeamTimer( TF_TEAM_BLUE );
		if ( pTimer )
		{
			pTimer->AcceptInput( "Pause", NULL, NULL, sVariant, 0 );
		}

		pTimer = TFGameRules()->GetKothTeamTimer( TF_TEAM_RED );
		if ( pTimer )
		{
			pTimer->AcceptInput( "Resume", NULL, NULL, sVariant, 0 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputSetCTFCaptureBonusTime( inputdata_t &inputdata )
{
	if ( TFGameRules() )
	{
		TFGameRules()->SetCTFCaptureBonusTime( inputdata.value.Float() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputSetBlueKothClockActive( inputdata_t &inputdata )
{
	if ( TFGameRules() )
	{
		variant_t sVariant;
		CTeamRoundTimer *pTimer = TFGameRules()->GetKothTeamTimer( TF_TEAM_BLUE );
		if ( pTimer )
		{
			pTimer->AcceptInput( "Resume", NULL, NULL, sVariant, 0 );
		}

		pTimer = TFGameRules()->GetKothTeamTimer( TF_TEAM_RED );
		if ( pTimer )
		{
			pTimer->AcceptInput( "Pause", NULL, NULL, sVariant, 0 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputSetRequiredObserverTarget( inputdata_t &inputdata )
{
	const char *pszEntName = inputdata.value.String();
	CBaseEntity *pEnt = NULL;

	if ( pszEntName && pszEntName[0] )
	{
		pEnt = gEntList.FindEntityByName( NULL, pszEntName );
	}

	TFGameRules()->SetRequiredObserverTarget( pEnt );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputSetRedTeamRespawnWaveTime( inputdata_t &inputdata )
{
	TFGameRules()->SetTeamRespawnWaveTime( TF_TEAM_RED, inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputSetBlueTeamRespawnWaveTime( inputdata_t &inputdata )
{
	TFGameRules()->SetTeamRespawnWaveTime( TF_TEAM_BLUE, inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputAddRedTeamRespawnWaveTime( inputdata_t &inputdata )
{
	TFGameRules()->AddTeamRespawnWaveTime( TF_TEAM_RED, inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputAddBlueTeamRespawnWaveTime( inputdata_t &inputdata )
{
	TFGameRules()->AddTeamRespawnWaveTime( TF_TEAM_BLUE, inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputSetRedTeamGoalString( inputdata_t &inputdata )
{
	TFGameRules()->SetTeamGoalString( TF_TEAM_RED, inputdata.value.String() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputSetBlueTeamGoalString( inputdata_t &inputdata )
{
	TFGameRules()->SetTeamGoalString( TF_TEAM_BLUE, inputdata.value.String() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputSetRedTeamRole( inputdata_t &inputdata )
{
	CTFTeam *pTeam = TFTeamMgr()->GetTeam( TF_TEAM_RED );
	if ( pTeam )
	{
		pTeam->SetRole( inputdata.value.Int() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputSetBlueTeamRole( inputdata_t &inputdata )
{
	CTFTeam *pTeam = TFTeamMgr()->GetTeam( TF_TEAM_BLUE );
	if ( pTeam )
	{
		pTeam->SetRole( inputdata.value.Int() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Pass in a VO sound entry to play for RED
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputPlayVORed( inputdata_t &inputdata )
{
	const char *szSoundName = inputdata.value.String();
	if ( szSoundName )
	{
		TFGameRules()->BroadcastSound( TF_TEAM_RED, szSoundName );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Pass in a VO sound entry to play for BLUE
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputPlayVOBlue( inputdata_t &inputdata )
{
	const char *szSoundName = inputdata.value.String();
	if ( szSoundName )
	{
		TFGameRules()->BroadcastSound( TF_TEAM_BLUE, szSoundName );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Pass in a VO sound entry to play
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputPlayVO( inputdata_t &inputdata )
{
	const char *szSoundName = inputdata.value.String();
	if ( szSoundName )
	{
		TFGameRules()->BroadcastSound( 255, szSoundName );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputHandleMapEvent( inputdata_t &inputdata )
{
	if ( TFGameRules() )
	{
		TFGameRules()->HandleMapEvent( inputdata );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputSetCustomUpgradesFile( inputdata_t &inputdata )
{
	if ( TFGameRules() )
	{
		TFGameRules()->SetCustomUpgradesFile( inputdata );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputSetRoundRespawnFreezeEnabled( inputdata_t &inputdata )
{
	tf_player_movement_restart_freeze.SetValue( inputdata.value.Bool() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputSetMapForcedTruceDuringBossFight( inputdata_t &inputdata )
{
	if ( TFGameRules() )
	{
		TFGameRules()->SetMapForcedTruceDuringBossFight( inputdata.value.Bool() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::Activate()
{
	TFGameRules()->Activate();

//=============================================================================
// HPE_BEGIN:
// [msmith]	We added a HUDTYPE so that the objective and the HUD are independent.
//			This lets us have a non training HUD on a training map.  And a training
//			HUD on another type of map.
//=============================================================================
	if ( m_nHudType != TF_HUDTYPE_UNDEFINED )
	{
		TFGameRules()->SetHUDType( m_nHudType );
	}
//=============================================================================
// HPE_END
//=============================================================================

	TFGameRules()->SetOvertimeAllowedForCTF( m_bOvertimeAllowedForCTF );

	ListenForGameEvent( "teamplay_round_win" );

	BaseClass::Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::TeamPlayerCountChanged( CTFTeam *pTeam )
{
	if ( pTeam == TFTeamMgr()->GetTeam( TF_TEAM_RED ) )
	{
		m_Team1PlayersChanged.Set( pTeam->GetNumPlayers(), this, this );
	}
	else if ( pTeam == TFTeamMgr()->GetTeam( TF_TEAM_BLUE ) )
	{
		m_Team2PlayersChanged.Set( pTeam->GetNumPlayers(), this, this );
	}

	// Tell the clients
	IGameEvent *event = gameeventmanager->CreateEvent( "teams_changed" );
	if ( event )
	{
		gameeventmanager->FireEvent( event );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::PowerupTeamImbalance( int nTeam )
{
	switch ( nTeam )
	{
	case TF_TEAM_RED:
		m_OnPowerupImbalanceTeam1.FireOutput( this, this );
		break;
	case TF_TEAM_BLUE:
		m_OnPowerupImbalanceTeam2.FireOutput( this, this );
		break;
	default:
		m_OnPowerupImbalanceMeasuresOver.FireOutput( this, this );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::StateEnterRoundRunning( void )
{
	m_OnStateEnterRoundRunning.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::StateEnterBetweenRounds( void )
{
	m_OnStateEnterBetweenRounds.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::StateEnterPreRound( void )
{
	m_OnStateEnterPreRound.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::StateExitPreRound( void )
{
	m_OnStateExitPreRound.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::MatchSummaryStart( void )
{
	m_OnMatchSummaryStart.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::TruceStart( void )
{
	m_OnTruceStart.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::TruceEnd( void )
{
	m_OnTruceEnd.FireOutput( this, this );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::FireGameEvent( IGameEvent *event )
{
#ifdef GAME_DLL
	const char *pszEventName = event->GetName();

	if ( FStrEq( pszEventName, "teamplay_round_win" ) )
	{
		int iWinningTeam = event->GetInt( "team" );

		switch( iWinningTeam )
		{
		case TF_TEAM_RED:
			m_OnWonByTeam1.FireOutput( this, this );
			break;
		case TF_TEAM_BLUE:
			m_OnWonByTeam2.FireOutput( this, this );
			break;
		default:
			break;
		}
	}
#endif
}

// (We clamp ammo ourselves elsewhere).
ConVar ammo_max( "ammo_max", "5000", FCVAR_REPLICATED );

#ifndef CLIENT_DLL
ConVar sk_plr_dmg_grenade( "sk_plr_dmg_grenade","0");		// Very lame that the base code needs this defined
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTFGameRules::Damage_IsTimeBased( int iDmgType )
{
	// Damage types that are time-based.
	return ( ( iDmgType & ( DMG_PARALYZE | DMG_NERVEGAS | DMG_DROWNRECOVER ) ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTFGameRules::Damage_ShowOnHUD( int iDmgType )
{
	// Damage types that have client HUD art.
	return ( ( iDmgType & ( DMG_DROWN | DMG_BURN | DMG_NERVEGAS | DMG_SHOCK ) ) != 0 );
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTFGameRules::Damage_ShouldNotBleed( int iDmgType )
{
	// Should always bleed currently.
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFGameRules::Damage_GetTimeBased( void )
{
	int iDamage = ( DMG_PARALYZE | DMG_NERVEGAS | DMG_DROWNRECOVER );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFGameRules::Damage_GetShowOnHud( void )
{
	int iDamage = ( DMG_DROWN | DMG_BURN | DMG_NERVEGAS | DMG_SHOCK );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CTFGameRules::Damage_GetShouldNotBleed( void )
{
	return 0;
}

//-----------------------------------------------------------------------------
// Return true if we are playing a PvE mode
//-----------------------------------------------------------------------------
bool CTFGameRules::IsPVEModeActive( void ) const
{
#ifdef TF_RAID_MODE
	if ( IsRaidMode() || IsBossBattleMode() )
		return true;
#endif

	if ( IsMannVsMachineMode() )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Return true for PvE opponents (ie: enemy bot team)
//-----------------------------------------------------------------------------
bool CTFGameRules::IsPVEModeControlled( CBaseEntity *who ) const
{
	if ( !who )
	{
		return false;
	}

#ifdef GAME_DLL
	if ( IsMannVsMachineMode() )
	{
		return who->GetTeamNumber() == TF_TEAM_PVE_INVADERS ? true : false;
	}

	if ( IsPVEModeActive() )
	{
		return who->GetTeamNumber() == TF_TEAM_RED ? true : false;
	}
#endif

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Return true if engineers can build quickly now
//-----------------------------------------------------------------------------
bool CTFGameRules::IsQuickBuildTime( void )
{
	return IsMannVsMachineMode() && ( InSetup() || TFObjectiveResource()->GetMannVsMachineIsBetweenWaves() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFGameRules::GameModeUsesUpgrades( void )
{
	if ( IsMannVsMachineMode() || IsBountyMode() )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFGameRules::CanPlayerUseRespec( CTFPlayer *pTFPlayer )
{
	if ( !pTFPlayer )
		return false;

	bool bAllowed = IsMannVsMachineRespecEnabled() && State_Get() == GR_STATE_BETWEEN_RNDS;

#ifdef GAME_DLL
	if ( !g_pPopulationManager )
		return false;

	bAllowed &= ( g_pPopulationManager->GetNumRespecsAvailableForPlayer( pTFPlayer ) ) ? true : false;
#else
	if ( !g_TF_PR )
		return false;

	bAllowed &= ( g_TF_PR->GetNumRespecCredits( pTFPlayer->entindex() ) ) ? true : false;
#endif
	
	return bAllowed;
}

bool CTFGameRules::IsCompetitiveMode( void ) const
{
	const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( GetCurrentMatchGroup() );
	if ( pMatchDesc )
	{
		return pMatchDesc->m_params.m_eMatchType == MATCH_TYPE_COMPETITIVE
			|| pMatchDesc->m_params.m_eMatchType == MATCH_TYPE_CASUAL;
	}

	return false;
}

bool CTFGameRules::IsMatchTypeCasual( void ) const
{
	const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( GetCurrentMatchGroup() );
	if ( pMatchDesc )
	{
		return ( pMatchDesc->m_params.m_eMatchType == MATCH_TYPE_CASUAL );
	}

	return false;
}

bool CTFGameRules::IsMatchTypeCompetitive( void ) const
{
	const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( GetCurrentMatchGroup() );
	if ( pMatchDesc )
	{
		return ( pMatchDesc->m_params.m_eMatchType == MATCH_TYPE_COMPETITIVE );
	}

	return false;
}

bool CTFGameRules::BInMatchStartCountdown() const
{
	if ( IsCompetitiveMode() )
	{
		float flTime = GetRoundRestartTime();
		if ( ( flTime > 0.f ) && ( (int)( flTime - gpGlobals->curtime ) <= mp_tournament_readymode_countdown.GetInt() ) )
		{
			return true;
		}
	}

	return false;
}

EMatchGroup CTFGameRules::GetCurrentMatchGroup() const
{
#if defined STAGING_ONLY && defined CLIENT_DLL
	if ( tf_fake_mm_group.GetInt() != -1 )
	{
		return (EMatchGroup)tf_fake_mm_group.GetInt();
	}
#endif

#ifdef GAME_DLL
	CMatchInfo *pMatch = GTFGCClientSystem()->GetMatch();
	return pMatch ? pMatch->m_eMatchGroup : k_nMatchGroup_Invalid;
#else
	// Client
	// We only care about what the server says if we are in an MM match.  We pass false
	// into BConnectedToMatch because we want the match group of the server EVEN IF
	// the match is over, but we're still connected.
	return GTFGCClientSystem()->BConnectedToMatchServer( false ) ? (EMatchGroup)m_nMatchGroupType.Get() : k_nMatchGroup_Invalid;
#endif
}

bool CTFGameRules::IsManagedMatchEnded() const
{
#ifdef GAME_DLL
	CMatchInfo *pMatch = GTFGCClientSystem()->GetMatch();
	return !pMatch || pMatch->BMatchTerminated();
#else
	// Client
	// We only care about what the server says if we are in an MM match.  (Note that the GC client system calls this to
	// determine if an MM server considers the match over, so beware circular logic)
	return !GTFGCClientSystem()->BConnectedToMatchServer( true ) || m_bMatchEnded;
#endif
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
void CTFGameRules::SyncMatchSettings()
{
	// These mirror the MatchInfo for the client's sake.
	CMatchInfo *pMatch = GTFGCClientSystem()->GetMatch();

	m_nMatchGroupType.Set( pMatch ? pMatch->m_eMatchGroup : k_nMatchGroup_Invalid );
	m_bMatchEnded.Set( IsManagedMatchEnded() );
}

//-----------------------------------------------------------------------------
bool CTFGameRules::StartManagedMatch()
{
	CMatchInfo *pMatch = GTFGCClientSystem()->GetMatch();
	if ( !pMatch )
	{
		Warning( "Starting a managed match with no match info\n" );
		return false;
	}

	// Cleanup
	m_eRematchState = NEXT_MAP_VOTE_STATE_NONE;

	/// Sync these before level change, so there's no race condition where clients may connect during/before the
	/// changelevel and see that the match is ended or wrong.
	SyncMatchSettings();

	// Change the the correct map from the match.  If no match specified, perform a fresh load of the current map
	const char *pszMap = pMatch->GetMatchMap();
	if ( !pszMap )
	{
		pszMap = STRING( gpGlobals->mapname );
		Warning( "Managed match did not specify map, using current map (%s)\n", pszMap );
	}

	engine->ServerCommand( CFmtStr( "changelevel %s\n", pszMap ) );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::SetCompetitiveMode( bool bValue )
{
	m_bCompetitiveMode.Set( bValue );
	//// Competitive mode is only supported on official servers.
	//// It requires matchmaking, and doesn't allow ad-hoc connections.
	//// If cheats are ever enabled, we force this mode off.

	//tf_mm_trusted.SetValue( bValue );
	//mp_tournament.SetValue( bValue );
	//mp_tournament_readymode.SetValue( bValue );

	//// No ad-hoc connections
	//tf_mm_strict.SetValue( bValue );

	//if ( bValue )
	//{
	//	engine->ServerCommand( "exec server_ladder.cfg\n" );

	//	Assert( tf_mm_servermode.GetInt() == 1 );
	//}

	//// Any state toggle is a reset (so don't spam this call)
	//State_Transition( GR_STATE_PREROUND );
	//SetInWaitingForPlayers( bValue );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::StartCompetitiveMatch( void )
{
	m_flSafeToLeaveTimer = -1.f;

	SetInWaitingForPlayers( false );
	RoundRespawn();
	State_Transition( GR_STATE_RESTART );
	ResetPlayerAndTeamReadyState();
}

//-----------------------------------------------------------------------------
// Purpose: Stops a match for some specified reason
//-----------------------------------------------------------------------------
void CTFGameRules::StopCompetitiveMatch( CMsgGC_Match_Result_Status nCode )
{
	CMatchInfo *pMatch = GTFGCClientSystem()->GetMatch();
	int nActiveMatchPlayers = pMatch->GetNumActiveMatchPlayers();
	if ( BAttemptMapVoteRollingMatch() &&
		nCode == CMsgGC_Match_Result_Status_MATCH_SUCCEEDED &&
		nActiveMatchPlayers > 0 )
	{
		ChooseNextMapVoteOptions();
	}
	else
	{
		// If we're not attempting a rolling match, end it
		// TODO ROLLING MATCHES: If we bail between now and RequestNewMatchForLobby, we need to call this or we'll get stuck.
		if ( !IsManagedMatchEnded() )
		{
			GTFGCClientSystem()->EndManagedMatch( /* bKickPlayersToParties */ false );
			Assert( IsManagedMatchEnded() );
			m_bMatchEnded.Set( true );
		}
	}

	if ( nCode == CMsgGC_Match_Result_Status_MATCH_SUCCEEDED )
	{
		IGameEvent *winEvent = gameeventmanager->CreateEvent( "competitive_victory" );
		if ( winEvent )
		{
			gameeventmanager->FireEvent( winEvent );
		}

		//
		// This determines new ratings and creates the GC messages
		//

		CMatchInfo *pInfo = GTFGCClientSystem()->GetMatch();
		if ( pInfo )
		{
			pInfo->CalculateMatchSkillRatingAdjustments( m_iWinningTeam );

			// Performance ranking with medals is currently server-side
			if ( pInfo->CalculatePlayerMatchRankData() )
			{
				// Send scoreboard event with final data
				int nPlayers = pInfo->GetNumTotalMatchPlayers();
				for ( int idx = 0; idx < nPlayers; idx++ )
				{
					IGameEvent *pEvent = gameeventmanager->CreateEvent( "competitive_stats_update" );
					if ( pEvent )
					{
						CMatchInfo::PlayerMatchData_t *pMatchRankData = pInfo->GetMatchDataForPlayer( idx );

						CBasePlayer *pPlayer = UTIL_PlayerBySteamID( pMatchRankData->steamID );
						if ( !pPlayer )
							continue;

						pEvent->SetInt( "index", pPlayer->entindex() );
						pEvent->SetInt( "score_rank", pMatchRankData ? pMatchRankData->nScoreMedal : 0 );		// medal won (if any)
						pEvent->SetInt( "kills_rank", pMatchRankData ? pMatchRankData->nKillsMedal : 0 );		//
						pEvent->SetInt( "damage_rank", pMatchRankData ? pMatchRankData->nDamageMedal : 0 );		//
						pEvent->SetInt( "healing_rank", pMatchRankData ? pMatchRankData->nHealingMedal : 0 );	//
						pEvent->SetInt( "support_rank", pMatchRankData ? pMatchRankData->nSupportMedal : 0 );	//
						gameeventmanager->FireEvent( pEvent );
					}
				}
			}
		}
		else
		{
			Warning( "CalculatePlayerMatchRankData(): General failure (investigate).\n" );
		}

		ReportMatchResultsToGC( nCode );
	}
	else if ( nCode == CMsgGC_Match_Result_Status_MATCH_FAILED_ABANDON )
	{
		// This generates a "safe to leave" notification on clients
		IGameEvent *pEvent = gameeventmanager->CreateEvent( "player_abandoned_match" );
		if ( pEvent )
		{
			pEvent->SetBool( "game_over", ( tf_competitive_abandon_method.GetBool() || State_Get() == GR_STATE_BETWEEN_RNDS ) );
			gameeventmanager->FireEvent( pEvent );
		}

		ReportMatchResultsToGC( nCode );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Fully completes the match
//-----------------------------------------------------------------------------
void CTFGameRules::EndCompetitiveMatch( void )
{
	MatchSummaryEnd();

	Log( "Competitive match ended.  Kicking all players.\n" );
	engine->ServerCommand( "kickall #TF_Competitive_Disconnect\n" );

	// Prepare for next match
	g_fGameOver = false;
	m_bAllowBetweenRounds = true;
	State_Transition( GR_STATE_RESTART );
	SetInWaitingForPlayers( true );
}

//-----------------------------------------------------------------------------
// Purpose: Called during CTFGameRules::Think()
//-----------------------------------------------------------------------------
void CTFGameRules::ManageCompetitiveMode( void )
{
	if ( !IsCompetitiveMode() )
		return;

	// Bring this back when we ship?
// 	// Security check
// 	if ( !tf_skillrating_debug.GetBool() )
// 	{
// 		m_bCompetitiveMode &= tf_mm_trusted.GetBool() &&
// 							  IsInTournamentMode() &&
// 							  !HaveCheatsBeenEnabledDuringLevel();
// 	}

	// We lost trusted status
	if ( !tf_mm_trusted.GetBool() )
	{
		m_nMatchGroupType.Set( k_nMatchGroup_Invalid );
		StopCompetitiveMatch( CMsgGC_Match_Result_Status_MATCH_FAILED_TRUSTED );
		UTIL_ClientPrintAll( HUD_PRINTCENTER, "Exiting Competitive Mode!" );
		Log( "Server lost trusted status.  Exiting Competitive Mode!" );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFGameRules::ReportMatchResultsToGC( CMsgGC_Match_Result_Status nCode )
{
	CMatchInfo *pMatch = GTFGCClientSystem()->GetMatch();
	if ( !pMatch )
		return false;

	GCSDK::CProtoBufMsg< CMsgGC_Match_Result > *pMsg = new GCSDK::CProtoBufMsg< CMsgGC_Match_Result >( k_EMsgGC_Match_Result );

	pMsg->Body().set_match_id( pMatch->m_nMatchID );
	pMsg->Body().set_match_group( pMatch->m_eMatchGroup );
	pMsg->Body().set_status( nCode );
	pMsg->Body().set_duration( CTF_GameStats.m_currentMap.m_Header.m_iTotalTime + ( gpGlobals->curtime - m_flRoundStartTime ) );

	CTeam *pTeam = GetGlobalTeam( TF_TEAM_RED );
	pMsg->Body().set_red_score( pTeam ? pTeam->GetScore() : (uint32)-1 );
	pTeam = GetGlobalTeam( TF_TEAM_BLUE );
	pMsg->Body().set_blue_score( pTeam ? pTeam->GetScore() : (uint32)-1 );
	Assert( m_iWinningTeam >= 0 );
	pMsg->Body().set_winning_team( Max( 0, (int)m_iWinningTeam ) );
	const MapDef_t *pMap = GetItemSchema()->GetMasterMapDefByName( STRING( gpGlobals->mapname ) );
	pMsg->Body().set_map_index( ( pMap ) ? pMap->m_nDefIndex : 0 );
	pMsg->Body().set_game_type( 1 );	// TODO: eMapGameType

	const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( pMatch->m_eMatchGroup );
	if( !pMatchDesc || !pMatchDesc->m_pProgressionDesc )
		return false;

	int nTotalScore = 0;

	CTFPlayerResource *pTFResource = dynamic_cast< CTFPlayerResource* >( g_pPlayerResource );
	if ( pTFResource )
	{
		pTFResource->UpdatePlayerData();

		for ( int i=0; i < MAX_PLAYERS; ++i )
		{
			nTotalScore += pTFResource->GetTotalScore( i );
		}
	}

	float flBlueScoreRatio = 0.5f;

	const CTFTeam* pTFTeamRed = GetGlobalTFTeam( TF_TEAM_RED );
	const CTFTeam* pTFTeamBlue = GetGlobalTFTeam( TF_TEAM_BLUE );

	// Figure out how much XP to give each team based on the game mode played
	if ( HasMultipleTrains() )
	{
		// In PLR we want to use the distance along the tracks each of the 
		// trains were at the end of each round
		flBlueScoreRatio = RemapValClamped( pTFTeamBlue->GetTotalPLRTrackPercentTraveled(), 0.f, pTFTeamBlue->GetTotalPLRTrackPercentTraveled() + pTFTeamRed->GetTotalPLRTrackPercentTraveled(), 0.f, 1.f );
	}
	else if ( !m_bPlayingKoth && !m_bPowerupMode && ( tf_gamemode_cp.GetInt() || tf_gamemode_sd.GetInt() || tf_gamemode_payload.GetInt() ) )
	{
		// Rounds Won
		// CP -	Points can flow back and forever, so we can't count total caps. And the
		//		state of the game is always the same at match end, so rounds won is our
		//		only real metric.
		// SD -	A flag cap is a round, so we could count caps or rounds here.  Again, the
		//		flag can go back and forth forever, but the match end state is always the same.
		// PL - Count the points captured by each team.  We do this A/D style where each team has
		//		a chance to score points.
		flBlueScoreRatio = RemapValClamped( pTFTeamBlue->GetScore(), 0.f, pTFTeamBlue->GetScore() + pTFTeamRed->GetScore() , 0.f, 1.f );
	}
	else if ( tf_gamemode_ctf.GetInt() || m_bPowerupMode || tf_gamemode_passtime.GetInt() )
	{
		// Flag captures
		// Mannpower is a variant of CTF and Passtime effectively is CTF. In all of these modes
		// we don't use rounds so our best metric is individual flag captures.
		flBlueScoreRatio = RemapValClamped( pTFTeamBlue->GetTotalFlagCaptures(), 0.f, pTFTeamBlue->GetTotalFlagCaptures() + pTFTeamRed->GetTotalFlagCaptures(), 0.f, 1.f );
	}
	else if ( m_bPlayingKoth )
	{
		// Time capped
		// Looking at the capped time for each team will let us give xp in a fair way.  We can
		// actually get as close as 50/50
		float flBlueTimeCapped = pTFTeamBlue->GetKOTHTime();
		float flRedTiemCapped = pTFTeamRed->GetKOTHTime();

		flBlueScoreRatio = RemapValClamped( flBlueTimeCapped, 0.f, flBlueTimeCapped + flRedTiemCapped, 0.f, 1.f );
	}
	else if ( tf_gamemode_rd.GetInt() || tf_gamemode_pd.GetInt() )
	{
		CTFRobotDestructionLogic* pRDLogic = CTFRobotDestructionLogic::GetRobotDestructionLogic();

		// Count bottles/cores scored by each team
		flBlueScoreRatio = RemapValClamped( pRDLogic->GetScore( TF_TEAM_BLUE ), 0.f, pRDLogic->GetScore( TF_TEAM_BLUE ) + pRDLogic->GetScore( TF_TEAM_RED ), 0.f, 1.f );
	}
	else if ( tf_gamemode_tc.GetInt() )
	{
		CTeamControlPointMaster *pMaster = g_hControlPointMasters.Count() ? g_hControlPointMasters[0] : NULL;
		
		int nBlueMiniRounds = 0;
		int nRedMiniRounds = 0;

		// Use the number of mini-rounds won by each team
		for( int i=0; i < pMaster->GetNumRounds(); ++ i )
		{
			const CTeamControlPointRound* pRound = pMaster->GetRoundByIndex( i );
			if ( pRound->RoundOwnedByTeam( TF_TEAM_RED ) )
			{
				++nRedMiniRounds;
			}
			else if ( pRound->RoundOwnedByTeam( TF_TEAM_BLUE ) )
			{
				++nBlueMiniRounds;
			}
			else
			{
				Assert( false );
			}
		}


		flBlueScoreRatio = RemapValClamped( nBlueMiniRounds, 0.f, nBlueMiniRounds + nRedMiniRounds, 0.f, 1.f );
	}
	else
	{
		Assert( !"Game mode not handled for team XP bonus!" );
	}

	const float flRedScoreRatio = 1.f - flBlueScoreRatio;
	const int nBlueTeamObjectiveBonus = flBlueScoreRatio * nTotalScore;
	const int nRedTeamObjectiveBonus = flRedScoreRatio * nTotalScore;

	// Player info
	for ( int idxPlayer = 0; idxPlayer < pMatch->GetNumTotalMatchPlayers(); idxPlayer++ )
	{
		CMatchInfo::PlayerMatchData_t *pMatchPlayer = pMatch->GetMatchDataForPlayer( idxPlayer );
		if ( !pMatchPlayer->steamID.BIndividualAccount() )
		{
			Assert( false );
			continue;
		}

		CTFPlayer *pTFPlayer = ToTFPlayer( UTIL_PlayerBySteamID( pMatchPlayer->steamID ) );
		PlayerStats_t *pStats = CTF_GameStats.FindPlayerStats( pTFPlayer );

		CMsgGC_Match_Result_Player *pMsgPlayer = pMsg->Body().add_players();
		int nTeam = GetGameTeamForGCTeam( pMatchPlayer->eGCTeam );
		pMsgPlayer->set_steam_id( pMatchPlayer->steamID.ConvertToUint64() );
		pMsgPlayer->set_team( nTeam );
		if ( pTFResource && pTFPlayer )
		{
			pMsgPlayer->set_score( pTFResource->GetTotalScore( pTFPlayer->entindex() ) );
		}
		else
		{
			// They left
		}

		int nPing = 0;
		int nPacketLoss = 0;
		if ( pTFPlayer )
		{
			UTIL_GetPlayerConnectionInfo( pTFPlayer->entindex(), nPing, nPacketLoss );
		}
		pMsgPlayer->set_ping( nPing );
		uint32 unPlayerFlags = 0U;
		if ( pMatchPlayer->bDropped )
		{
			unPlayerFlags |= MATCH_FLAG_PLAYER_LEAVER;
		}
		if ( pMatchPlayer->bLateJoin )
		{
			unPlayerFlags |= MATCH_FLAG_PLAYER_LATEJOIN;
		}
		if ( pMatchPlayer->BDropWasAbandon() )
		{
			unPlayerFlags |= MATCH_FLAG_PLAYER_ABANDONER;
		}
		if ( pMatchPlayer->bPlayed )
		{
			unPlayerFlags |= MATCH_FLAG_PLAYER_PLAYED;
		}

		pMsgPlayer->set_flags( unPlayerFlags );
		// server-side skill system
		FixmeMMRatingBackendSwapping(); // Assuming skill rating is drillo
		pMsgPlayer->set_classes_played( pMatchPlayer->unClassesPlayed );
		pMsgPlayer->set_kills( pStats ? pStats->statsAccumulated.m_iStat[TFSTAT_KILLS] : 0 );
		pMsgPlayer->set_damage( pStats ? pStats->statsAccumulated.m_iStat[TFSTAT_DAMAGE] : 0 );
		pMsgPlayer->set_healing( pStats ? pStats->statsAccumulated.m_iStat[TFSTAT_HEALING] : 0 );
		pMsgPlayer->set_support( pStats ? CalcPlayerSupportScore( &pStats->statsAccumulated, pTFPlayer->entindex() ) : 0 );
		pMsgPlayer->set_score_medal( pMatchPlayer->nScoreMedal );
		pMsgPlayer->set_kills_medal( pMatchPlayer->nKillsMedal );
		pMsgPlayer->set_damage_medal( pMatchPlayer->nDamageMedal );
		pMsgPlayer->set_healing_medal( pMatchPlayer->nHealingMedal );
		pMsgPlayer->set_support_medal( pMatchPlayer->nSupportMedal );
		FixmeMMRatingBackendSwapping(); // Assuming we're using skill rating for rank? Why even include this?
		pMsgPlayer->set_rank( pMatchDesc->m_pProgressionDesc->GetLevelForExperience( pMatchPlayer->unMMSkillRating ).m_nLevelNum );
		pMsgPlayer->set_deaths( pStats ? pStats->statsAccumulated.m_iStat[TFSTAT_DEATHS] : 0 );
		pMsgPlayer->set_party_id( pMatchPlayer->uPartyID );
		uint32 unLeaveTime = ( pMatchPlayer && ( pMatchPlayer->bDropped || pMatchPlayer->BDropWasAbandon() ) ) ? 
							   pMatchPlayer->GetLastActiveEventTime() : 0u;
		pMsgPlayer->set_leave_time( unLeaveTime );
		pMsgPlayer->set_leave_reason( pMatchPlayer->GetDropReason() );
		pMsgPlayer->set_connect_time( pMatchPlayer->rtJoinedMatch );

		// Somebody won! Match finish bonus
		if ( nCode == CMsgGC_Match_Result_Status_MATCH_SUCCEEDED )
		{
			// Give points based on team performance
			int nPerformanceScore = RemapValClamped( pMsgPlayer->score(), 0, nTotalScore / 24, 0, nTeam == TF_TEAM_RED ? nRedTeamObjectiveBonus : nBlueTeamObjectiveBonus );
			pMatch->GiveXPRewardToPlayerForAction( pMatchPlayer->steamID, CMsgTFXPSource::SOURCE_OBJECTIVE_BONUS, nPerformanceScore );
		
			// Everyone gets base completion points
			int nCompletionScore = RemapValClamped( pMsgPlayer->score(), 0, nTotalScore / 24, 0, nTotalScore );
			pMatch->GiveXPRewardToPlayerForAction( pMatchPlayer->steamID, CMsgTFXPSource::SOURCE_COMPLETED_MATCH, nCompletionScore );
		}

		// Copy any pending XP sources they had ready to send up
		for( int i=0; i < pMatchPlayer->GetXPSources().sources_size(); ++i )
		{
			CMsgTFXPSource* pXPSource = pMsgPlayer->add_xp_breakdown();
			pXPSource->CopyFrom( pMatchPlayer->GetXPSources().sources( i ) );
		}
	}

	pMsg->Body().set_win_reason( GetWinReason() );
	uint32 unMatchFlags = 0u;

	if ( pMatch->m_uLobbyFlags & LOBBY_FLAG_LOWPRIORITY )
	{
		unMatchFlags |= MATCH_FLAG_LOWPRIORITY;
	}

	if ( pMatch->m_uLobbyFlags & LOBBY_FLAG_REMATCH )
	{
		unMatchFlags |= MATCH_FLAG_REMATCH;
	}

	pMsg->Body().set_flags( unMatchFlags );
	pMsg->Body().set_bots( pMatch->m_nBotsAdded );

	GTFGCClientSystem()->SendCompetitiveMatchResult( pMsg );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFGameRules::MatchmakingShouldUseStopwatchMode()
{
	CTeamControlPointMaster *pMaster = g_hControlPointMasters.Count() ? g_hControlPointMasters[0] : NULL;
	bool bRetVal = !HasMultipleTrains() && ( tf_gamemode_payload.GetBool() || ( pMaster && ( pMaster->PlayingMiniRounds() || pMaster->ShouldSwitchTeamsOnRoundWin() ) ) );
	
	tf_attack_defend_map.SetValue( bRetVal );
	return bRetVal;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameRules::SetPowerupMode( bool bValue )
{
	// Powerup mode uses grapple and changes some gamerule variables.

	if ( bValue )
	{
		tf_grapplinghook_enable.SetValue( 1 );
		tf_flag_return_time_credit_factor.SetValue( 0 );
	}
	else
	{
		tf_grapplinghook_enable.SetValue( 0 );
		tf_flag_return_time_credit_factor.SetValue( 1 );
	}

	m_bPowerupMode = bValue;
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
void CTFGameRules::EndManagedMvMMatch( bool bKickPlayersToParties )
{
	// Primarily a pass through so we can ensure our match end state is sync'd -- CPopulationManager manages most of the
	// MvM meta round state
	if ( !IsManagedMatchEnded() )
	{
		GTFGCClientSystem()->EndManagedMatch( bKickPlayersToParties );
		Assert( IsManagedMatchEnded() );
		m_bMatchEnded.Set( true );
	}
}
#endif // GAME_DLL


#ifdef STAGING_ONLY
//-----------------------------------------------------------------------------
// Purpose: Enable/Disable Bounty Mode
//-----------------------------------------------------------------------------
void CTFGameRules::SetBountyMode( bool bValue )
{
	if ( m_bBountyModeEnabled.Get() != bValue )
	{
		m_bBountyModeEnabled.Set( bValue );
	}
	
	// If enabling, dynamically create an upgrade entity
	if ( bValue )
	{
		if ( !g_hUpgradeEntity && !m_pUpgrades )
		{
			m_pUpgrades = CBaseEntity::Create( "func_upgradestation", vec3_origin, vec3_angle );
		}
	}

	// If disabling, remove upgrade entity
	if ( !bValue && m_pUpgrades )
	{
		UTIL_Remove( m_pUpgrades );
		m_pUpgrades = NULL;
	}

	IGameEvent *event = gameeventmanager->CreateEvent( "bountymode_toggled" );
	if ( event )
	{
		event->SetBool( "active", bValue );
		gameeventmanager->FireEvent( event );
	}
}

#endif // GAME_DLL
#endif // STAGING_ONLY

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFGameRules::UsePlayerReadyStatusMode( void )
{
	if ( IsMannVsMachineMode() )
		return true;

	if ( IsCompetitiveMode() )
		return true;

	if ( mp_tournament.GetBool() && mp_tournament_readymode.GetBool() )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFGameRules::PlayerReadyStatus_HaveMinPlayersToEnable( void )
{
	// we always have enough players if the match wants players to autoready
	const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( GetCurrentMatchGroup() );
	if ( pMatchDesc && pMatchDesc->m_params.m_bAutoReady )
		return true;

#ifdef GAME_DLL
	// Count connected players
	int nNumPlayers = 0;
	CUtlVector< CTFPlayer* > playerVector;
	CollectPlayers( &playerVector );
	FOR_EACH_VEC( playerVector, i )
	{
		if ( !playerVector[i] )
			continue;

		if ( playerVector[i]->IsFakeClient() )
			continue;

#ifdef STAGING_ONLY
		if ( !mp_tournament_readymode_bots_allowed.GetBool() && playerVector[i]->IsBot() )
			continue;
#else
		if ( playerVector[i]->IsBot() )
			continue;
#endif

		if ( playerVector[i]->IsHLTV() )
			continue;

		if ( playerVector[i]->IsReplay() )
			continue;

		nNumPlayers++;
	}

	// Default
	int nMinPlayers = 1;
	CMatchInfo *pMatch = GTFGCClientSystem()->GetMatch();

	if ( pMatch && !pMatch->BMatchTerminated() && pMatchDesc->m_params.m_bRequireCompleteMatch )
	{
		nMinPlayers = pMatch->GetCanonicalMatchSize();
	}
	else if ( IsMannVsMachineMode() &&
	          ( engine->IsDedicatedServer() || ( !engine->IsDedicatedServer() && nNumPlayers > 1 ) ) )
	{
		nMinPlayers = tf_mvm_min_players_to_start.GetInt();
	}
	else if ( UsePlayerReadyStatusMode() && engine->IsDedicatedServer() )
	{
		nMinPlayers = mp_tournament_readymode_min.GetInt();
	}

	// Should be renamed to m_bEnableReady, not sure why we encoded our criteria in the names of all associated functions and variables...
	m_bHaveMinPlayersToEnableReady.Set( nNumPlayers >= nMinPlayers );

#endif

	return m_bHaveMinPlayersToEnableReady;
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
bool CTFGameRules::PlayerReadyStatus_ArePlayersOnTeamReady( int iTeam )
{
	if ( IsMannVsMachineMode() && iTeam == TF_TEAM_PVE_INVADERS )
		return true;

	CMatchInfo *pMatch = GTFGCClientSystem()->GetMatch();
	if ( pMatch )
	{
		int nMatchPlayers = pMatch->GetNumTotalMatchPlayers();
		if ( nMatchPlayers <= 0 )
			return false;

		int iPlayerReadyCount = 0;
		for ( int i = 0; i < nMatchPlayers; i++ )
		{
			CMatchInfo::PlayerMatchData_t *pPlayerData = pMatch->GetMatchDataForPlayer( i );
			if ( !pPlayerData->bDropped && GetGameTeamForGCTeam( pPlayerData->eGCTeam ) == iTeam )
			{
				CBasePlayer *pPlayer = UTIL_PlayerBySteamID( pPlayerData->steamID );
				// XXX(JohnS): Not quite valid yet, We let them join first onto spectate, which is probably a bit
				//             confusing
				//
				// AssertMsg( !pPlayer || ToTFPlayer( pPlayer )->GetTeamNumber() == GetGameTeamForGCTeam( pPlayerData->eGCTeam ),
				//            "Player's GC assigned team does not match their current team" );
				if ( !pPlayer || !m_bPlayerReady[ pPlayer->entindex() ] )
					return false;

				iPlayerReadyCount++;
			}
		}

		const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( GetCurrentMatchGroup() );
		if ( pMatchDesc && pMatchDesc->m_params.m_bAutoReady )
		{
			return iPlayerReadyCount > 0 || pMatch->GetNumTotalMatchPlayers() == 1 ;
		}
		else
		{
			int iTeamSize = IsMannVsMachineMode() ? pMatch->GetCanonicalMatchSize() : pMatch->GetCanonicalMatchSize() / 2;
			return iPlayerReadyCount >= iTeamSize;
		}
	}

	// Non-match
	bool bAtLeastOneReady = false;
	for ( int i = 1; i <= MAX_PLAYERS; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer || ToTFPlayer( pPlayer )->GetTeamNumber() != iTeam )
			continue;

		if ( !m_bPlayerReady[i] )
		{
			return false;
		}
		else
		{
			bAtLeastOneReady = true;
		}
	}

	// Team isn't ready if there was nobody on it.
	return bAtLeastOneReady;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFGameRules::PlayerReadyStatus_ShouldStartCountdown( void )
{
	CMatchInfo *pMatch = GTFGCClientSystem()->GetMatch();

#if defined( STAGING_ONLY )
	// Local testing hack - allow match size of one where just that player is ready
	if ( !IsMannVsMachineMode() && !pMatch && BHavePlayers() && ( IsTeamReady( TF_TEAM_RED ) || IsTeamReady( TF_TEAM_BLUE ) ) )
		return true;
#endif // STAGING_ONLY

	if ( IsMannVsMachineMode() )
	{
		if ( !IsTeamReady( TF_TEAM_PVE_DEFENDERS ) && m_flRestartRoundTime >= gpGlobals->curtime + mp_tournament_readymode_countdown.GetInt() )
		{
			bool bIsTeamReady = PlayerReadyStatus_ArePlayersOnTeamReady( TF_TEAM_PVE_DEFENDERS );
			if ( bIsTeamReady )
			{
				SetTeamReadyState( true, TF_TEAM_PVE_DEFENDERS );
				return true;
			}
		}
	}
	else if ( pMatch &&
	          PlayerReadyStatus_ArePlayersOnTeamReady( TF_TEAM_RED ) &&
	          PlayerReadyStatus_ArePlayersOnTeamReady( TF_TEAM_BLUE ) )
	{
		return true;
	}
	else if ( IsTeamReady( TF_TEAM_RED ) && IsTeamReady( TF_TEAM_BLUE ) )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::PlayerReadyStatus_ResetState( void )
{
	// Reset the players
	ResetPlayerAndTeamReadyState();

	// Reset the team
	SetTeamReadyState( false, TF_TEAM_RED );
	SetTeamReadyState( false, TF_TEAM_BLUE );

	m_flRestartRoundTime.Set( -1.f );
	mp_restartgame.SetValue( 0 );
	m_bAwaitingReadyRestart = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::PlayerReadyStatus_UpdatePlayerState( CTFPlayer *pTFPlayer, bool bState )
{
	if ( !UsePlayerReadyStatusMode() )
		return;

	if ( !pTFPlayer )
		return;

	if ( pTFPlayer->GetTeamNumber() < FIRST_GAME_TEAM )
		return;

	if ( State_Get() != GR_STATE_BETWEEN_RNDS )
		return;

	// Don't allow toggling state in the final countdown
	if ( GetRoundRestartTime() > 0.f && GetRoundRestartTime() <= gpGlobals->curtime + TOURNAMENT_NOCANCEL_TIME )
		return;

	// Make sure we have enough to allow ready mode commands
	if ( !PlayerReadyStatus_HaveMinPlayersToEnable() )
		return;

	int nEntIndex = pTFPlayer->entindex();

	// Already this state
	if ( bState == IsPlayerReady( nEntIndex ) )
		return;

	SetPlayerReadyState( nEntIndex, bState );

	if ( !bState )
	{
		// Slam team status to Not Ready for any player that sets Not Ready
		m_bTeamReady.Set( pTFPlayer->GetTeamNumber(), false );

		// If everyone cancels ready state, stop the clock
		bool bAnyoneReady = false;
		for ( int i = 1; i <= MAX_PLAYERS; ++i )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( !pPlayer )
				continue;

			if ( m_bPlayerReady[i] )
			{
				bAnyoneReady = true;
				break;
			}
		}

		if ( !bAnyoneReady )
		{
			m_flRestartRoundTime.Set( -1.f );
			mp_restartgame.SetValue( 0 );
			ResetPlayerAndTeamReadyState();
		}
	}
	else
	{
		if ( IsMannVsMachineMode() || IsCompetitiveMode() )
		{
			// Reduce timer as each player hits Ready, but only once per-player
			if ( !m_bPlayerReadyBefore[nEntIndex] && m_flRestartRoundTime > gpGlobals->curtime + 60.f )
			{
				float flReduceBy = 30.f;
				if ( m_flRestartRoundTime < gpGlobals->curtime + 90.f )
				{
					// Never reduce below 60 seconds remaining
					flReduceBy = m_flRestartRoundTime - gpGlobals->curtime - 60.f;
				}

				m_flRestartRoundTime -= flReduceBy;
			}
			else if ( m_flRestartRoundTime < 0 && !PlayerReadyStatus_ShouldStartCountdown() )
			{
				m_flRestartRoundTime.Set( gpGlobals->curtime + 150.f );
				m_bAwaitingReadyRestart = false;

				IGameEvent* pEvent = gameeventmanager->CreateEvent( "teamplay_round_restart_seconds" );
				if ( pEvent )
				{
					pEvent->SetInt( "seconds", 150 );
					gameeventmanager->FireEvent( pEvent );
				}
			}
		}

		// Unofficial modes set team ready state here
		CMatchInfo *pMatch = GTFGCClientSystem()->GetMatch();
		if ( !pMatch && !IsMannVsMachineMode() )
		{
			intp nRed = 0;
			intp nRedCount = 0;
			intp nBlue = 0;
			intp nBlueCount = 0;

			for ( int iTeam = FIRST_GAME_TEAM; iTeam < TFTeamMgr()->GetTeamCount(); iTeam++ )
			{
				CTFTeam *pTeam = GetGlobalTFTeam( iTeam );
				if ( pTeam )
				{
					Assert( pTeam->GetTeamNumber() == TF_TEAM_RED || pTeam->GetTeamNumber() == TF_TEAM_BLUE );

					for ( intp i = 0; i < pTeam->GetNumPlayers(); ++i )
					{
						if ( !pTeam->GetPlayer(i) )
							continue;

						if ( pTeam->GetTeamNumber() == TF_TEAM_RED && IsPlayerReady( pTeam->GetPlayer(i)->entindex() ) )
						{
							if ( !nRedCount )
							{
								nRedCount = pTeam->GetNumPlayers();
							}

							nRed++;
						}
						else if ( pTeam->GetTeamNumber() == TF_TEAM_BLUE && IsPlayerReady( pTeam->GetPlayer(i)->entindex() ) )
						{
							if ( !nBlueCount )
							{
								nBlueCount = pTeam->GetNumPlayers();
							}

							nBlue++;
						}
					}
				}
			}

			// Check for the convar that requires min team size, or just go with whatever each team has
			int nRedMin = ( mp_tournament_readymode_team_size.GetInt() > 0 ) ? mp_tournament_readymode_team_size.GetInt() : Max( nRedCount, 1 );
			int nBlueMin = ( mp_tournament_readymode_team_size.GetInt() > 0 ) ? mp_tournament_readymode_team_size.GetInt() : Max( nBlueCount, 1 );

			SetTeamReadyState( ( nRed == nRedMin ), TF_TEAM_RED );
			SetTeamReadyState( ( nBlue == nBlueMin ), TF_TEAM_BLUE );
		}

		m_bPlayerReadyBefore[nEntIndex] = true;
	}
}
#endif // GAME_DLL

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFGameRules::IsDefaultGameMode( void )
{
	if ( IsMannVsMachineMode() )
		return false;

	if ( IsInArenaMode() )
		return false;

	if ( IsInMedievalMode() )
		return false;

	if ( IsCompetitiveMode() )
		return false;

	if ( IsInTournamentMode() )
		return false;

	if ( IsInTraining() )
		return false;

	if ( IsInItemTestingMode() )
		return false;

#ifdef STAGING_ONLY
	if ( IsPVEModeActive() )
		return false;
#endif // STAGING_ONLY

#ifdef TF_RAID_MODE
	if ( IsRaidMode() )
		return false;

	if ( IsBossBattleMode() )
		return false;
#endif // TF_RAID_MODE

#ifdef TF_CREEP_MODE
	if ( IsCreepWaveMode() )
		return false;
#endif // TF_CREEP_MODE

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Allows us to give situational discounts, such as secondary weapons
//-----------------------------------------------------------------------------
int CTFGameRules::GetCostForUpgrade( CMannVsMachineUpgrades *pUpgrade, int iItemSlot, int nClass, CTFPlayer *pPurchaser /*= NULL*/ )
{
	Assert( pUpgrade );

	if ( !pUpgrade )
		return 0;

	int iCost = pUpgrade->nCost;

	CEconItemSchema *pSchema = ItemSystem()->GetItemSchema();
	if ( pSchema )
	{
		const CEconItemAttributeDefinition *pAttr = pSchema->GetAttributeDefinitionByName( pUpgrade->szAttrib );
		if ( pAttr )
		{
			if ( ( iItemSlot == LOADOUT_POSITION_PRIMARY && nClass == TF_CLASS_ENGINEER ) ||
				 ( iItemSlot == LOADOUT_POSITION_SECONDARY && nClass != TF_CLASS_DEMOMAN ) ||
				 ( iItemSlot == LOADOUT_POSITION_MELEE && nClass != TF_CLASS_SPY && nClass != TF_CLASS_ENGINEER ) )
			{
				switch ( pAttr->GetDefinitionIndex() )
				{
				case 4:		// clip size bonus
				case 6:		// fire rate bonus
				case 78:	// maxammo secondary increased
				case 180:	// heal on kill
				case 266:	// projectile penetration
				case 335:	// clip size bonus upgrade
				case 397:	// projectile penetration heavy
					iCost *= 0.5f;
					break;
				default:
					break;
				}
			}
			else if ( iItemSlot == LOADOUT_POSITION_ACTION )
			{
				if ( pPurchaser )
				{
					int iCanteenSpec = 0;
					CALL_ATTRIB_HOOK_INT_ON_OTHER( pPurchaser, iCanteenSpec, canteen_specialist );
					if ( iCanteenSpec )
					{

// 						iCost *= ( 1.f - ( iCanteenSpec * 0.1f ) );
// 						iCost = Max( 1, ( iCost - ( iCost % 5 ) ) );

						iCost -= ( 10 * iCanteenSpec );
						iCost = Max( 5, iCost );
					}
				}
			}
		}
	}

	return iCost;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFGameRules::CTFGameRules()
#ifdef GAME_DLL
	: m_mapCoachToStudentMap( DefLessFunc(uint32) )
	, m_flNextStrangeEventProcessTime( g_flStrangeEventBatchProcessInterval )
	, m_mapTeleportLocations( DefLessFunc(string_t) )
	, m_bMapCycleNeedsUpdate( false )
	, m_flSafeToLeaveTimer( -1.f )
	, m_flCompModeRespawnPlayersAtMatchStart( -1.f )
	, m_bMapForcedTruceDuringBossFight( false )
	, m_flNextHalloweenGiftUpdateTime( -1 )
#else
	: m_bRecievedBaseline( false )
#endif
{
#ifdef GAME_DLL
	// Create teams.
	TFTeamMgr()->Init();

	ResetMapTime();

	// Create the team managers
//	for ( int i = 0; i < 