//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "c_tf_player.h"
#include "c_user_message_register.h"
#include "view.h"
#include "iclientvehicle.h"
#include "ivieweffects.h"
#include "input.h"
#include "IEffects.h"
#include "fx.h"
#include "c_basetempentity.h"
#include "hud_macros.h"
#include "engine/ivdebugoverlay.h"
#include "smoke_fog_overlay.h"
#include "playerandobjectenumerator.h"
#include "bone_setup.h"
#include "in_buttons.h"
#include "r_efx.h"
#include "dlight.h"
#include "shake.h"
#include "cl_animevent.h"
#include "animation.h"
#include "choreoscene.h"
#include "tf_weaponbase.h"
#include "c_tf_playerresource.h"
#include "toolframework/itoolframework.h"
#include "tier1/KeyValues.h"
#include "tier0/vprof.h"
#include "prediction.h"
#include "effect_dispatch_data.h"
#include "c_te_effect_dispatch.h"
#include "tf_fx_muzzleflash.h"
#include "tf_gamerules.h"
#include "view_scene.h"
#include "c_baseobject.h"
#include "toolframework_client.h"
#include "materialsystem/imaterialvar.h"
#include "soundenvelope.h"
#include "voice_status.h"
#include "clienteffectprecachesystem.h"
#include "functionproxy.h"
#include "toolframework_client.h"
#include "choreoevent.h"
#include "vguicenterprint.h"
#include "eventlist.h"
#include "input.h"
#include "tf_weapon_medigun.h"
#include "tf_weapon_pipebomblauncher.h"
#include "tf_weapon_shovel.h"
#include "tf_hud_mediccallers.h"
#include "in_main.h"
#include "c_team.h"
#include "collisionutils.h"
// for spy material proxy
#include "tf_proxyentity.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexturecompositor.h"
#include "c_tf_team.h"
#include "tf_item_inventory.h"
#include "model_types.h"
#include "dt_utlvector_recv.h"
#include "tf_item_wearable.h"
#include "cam_thirdperson.h"
#include "c_tf_projectile_arrow.h"
#include "econ_entity.h"
#include "ihasowner.h"
#include "tf_hud_itemeffectmeter.h"
#include "replay/vgui/replayinputpanel.h"
#include "tf_replay.h"
#include "netadr.h"
#include "input.h"

#include "gcsdk/gcclientsdk.h"
#include "econ_gcmessages.h"
#include "rtime.h"
#include "networkstringtable_clientdll.h"
#include "replay/ireplaymanager.h"
#include "gc_clientsystem.h"
#include "c_entitydissolve.h"
#include "tf_viewmodel.h"
#include "player_vs_environment/c_tf_upgrades.h"
#include "sourcevr/isourcevirtualreality.h"
#include "tempent.h"
#include "confirm_dialog.h"
#include "c_tf_weapon_builder.h"
#include "tf_shared_content_manager.h"
#include "baseanimatedtextureproxy.h"
#include "econ_entity.h"
#include "halloween/tf_weapon_spellbook.h"
#include "tf_weapon_grapplinghook.h"
#include "tf_logic_robot_destruction.h"
#include "econ_notifications.h"
#include "tf_weapon_buff_item.h"
#include "tf_dropped_weapon.h"
#include "tf_hud_notification_panel.h"
#include "tf_dropped_weapon.h"
#include "tf_hud_passtime_reticle.h"
#include "passtime_convars.h"
#include "c_tf_passtime_logic.h"
#include "tf_weapon_passtime_gun.h"
#include "eiface.h"
#include "filesystem.h"
#include "debugoverlay_shared.h"
#include "tf_hud_chat.h"
#include "tf_item_powerup_bottle.h"
#include <vgui_controls/AnimationController.h>

#ifdef STAGING_ONLY
#include "tf_matchmaking_shared.h"
#include "tier2/keyvaluesmacros.h"
#endif // STAGING_ONLY

#if defined( REPLAY_ENABLED )
#include "replay/ienginereplay.h"
#endif

#if defined( CTFPlayer )
#undef CTFPlayer
#endif

#include "materialsystem/imesh.h"		//for materials->FindMaterial
#include "iviewrender.h"				//for view->

// NVNT haptics system interface
#include "c_tf_haptics.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace GCSDK;

static_assert( TEAM_UNASSIGNED == 0, "If this assert fires, update the assert and the enum in ctexturecompositor.cpp which specifies team colors" );
static_assert( TF_TEAM_RED == 2, "If this assert fires, update the assert and the enum in ctexturecompositor.cpp which specifies team colors" );
static_assert( TF_TEAM_BLUE == 3, "If this assert fires, update the assert and the enum in ctexturecompositor.cpp which specifies team colors" );

// Forward decl
CEconItemView *GetEconItemViewFromProxyEntity( void *pEntity );
C_TFPlayer *GetOwnerFromProxyEntity( void *pEntity );

// --------------------------------------------------------------------------------
// Local Convar Helper Function
// --------------------------------------------------------------------------------
void VisionMode_ChangeCallback( IConVar *pConVar, char const *pOldString, float flOldValue )
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pLocalPlayer )
	{
		pLocalPlayer->CalculateVisionUsingCurrentFlags();
	}
}

#ifndef NDEBUG
CON_COMMAND_F ( tf_test_bomb, "Test halloween bomb", 0 )
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer )
		return;

	C_TFPlayer *pPlayer = ToTFPlayer( pLocalPlayer );
	if ( !pPlayer )
		return;

	pPlayer->CreateBombonomiconHint();
}

ConVar test_vision_off( "test_vision_off", "0", FCVAR_NONE, "Force vision modes off!", VisionMode_ChangeCallback );
ConVar test_pyrovision( "test_pyrovision", "0", FCVAR_NONE, "Force Pyrovision on!", VisionMode_ChangeCallback );
ConVar test_romevision( "test_romevision", "0", FCVAR_NONE, "Force Romevision on!", VisionMode_ChangeCallback );
ConVar test_halloweenvision( "test_halloweenvision", "0", FCVAR_NONE, "Force halloween vision on!", VisionMode_ChangeCallback );
#endif

// These are all permanently STAGING_ONLY
#ifdef STAGING_ONLY
	ConVar tf_paint_kit_disable( "tf_paint_kit_disable", "0", FCVAR_NONE, "Set to disable using paintkits completely." );
	ConVar tf_paint_kit_team_override( "tf_paint_kit_team_override", "-1", FCVAR_NONE, "Set to -1:disabled, 0:TF_TEAM_RED, 1:TF_TEAM_BLUE" );
	ConVar tf_paint_kit_seed_override( "tf_paint_kit_seed_override", "0", FCVAR_NONE, "Set to none-0 will force composite weapon skin to use this as random seed number." );
	ConVar tf_paint_kit_force_regen( "tf_paint_kit_force_regen", "0", FCVAR_NONE, "Set to 1 to force regen after every generation. (dev only)" );
	ConVar tf_paint_kit_generating_icons( "tf_paint_kit_generating_icons", "0", FCVAR_NONE, "Only Set by the Icon Generating System." );
	extern ConVar tf_paint_kit_force_wear;

	KeyValues *s_kvOverridePaintkit = NULL;
	bool s_bIsPaintkitOverrideSet = false;

	void LoadPaintKit( const char* pszFile )
	{
		//scripts\items\unencrypted\paintkits
		static char pszPaintkitFile[MAX_PATH] = { '\0' };
		if ( pszFile )
		{
			V_sprintf_safe( pszPaintkitFile, "scripts/items/unencrypted/paintkits/%s.paintkit", pszFile );
		}

		if ( pszPaintkitFile[0] == '\0' )
			return;

		// Make sure we have a file system
		if ( !g_pFullFileSystem || !g_pFullFileSystem->FileExists( pszPaintkitFile ) )
		{
			Msg( "Unable to load %s from File System \n", pszPaintkitFile );
			return;
		}

		if ( s_kvOverridePaintkit )
		{
			s_kvOverridePaintkit->deleteThis();
			s_kvOverridePaintkit = NULL;
		}

		// Parse the KeyValues
		s_kvOverridePaintkit = new KeyValues( "paintkit" );
		if ( !s_kvOverridePaintkit->LoadFromFile( g_pFullFileSystem, pszPaintkitFile ) )
		{
			Msg( "Unable to read KeyValues from %s \n", pszPaintkitFile );
			return;
		}

		HandleKeyValuesMacros( s_kvOverridePaintkit );

		Msg( "Setting %s as Paintkit override.\nCall with no args to disable\ntf_paint_kit_force_regen set to 1\n", pszPaintkitFile );
		s_bIsPaintkitOverrideSet = true;
	}

	CON_COMMAND_F( tf_paint_kit_override_file, "Set a override paintkit file", FCVAR_NONE )
	{
		// Enable force regen
		tf_paint_kit_force_regen.SetValue( 1 );
		s_bIsPaintkitOverrideSet = false;

		// Take the con var
		if ( args.ArgC() <= 1 )
		{
			Msg( "No paintkit file specified, turning override off.\nYou may want to set tf_paint_kit_force_regen to 0\n" );
			return;
		}

		const char *pszFile = args.Arg( 1 );
		LoadPaintKit( pszFile );
	}

	CON_COMMAND( tf_paint_kit_override_refresh, "Refresh the current override paint kit file" )
	{
		if ( !s_bIsPaintkitOverrideSet )
			return;

		LoadPaintKit( NULL );
	}

#endif

ConVar tf_playergib_forceup( "tf_playersgib_forceup", "1.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Upward added velocity for gibs." );
ConVar tf_playergib_force( "tf_playersgib_force", "500.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Gibs force." );
ConVar tf_playergib_maxspeed( "tf_playergib_maxspeed", "400", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Max gib speed." );

ConVar tf_always_deathanim( "tf_always_deathanim", "0", FCVAR_CHEAT, "Force death anims to always play." );

ConVar tf_clientsideeye_lookats( "tf_clientsideeye_lookats", "1", FCVAR_NONE, "When on, players will turn their pupils to look at nearby players." );

extern ConVar tf_halloween_kart_boost_recharge;
extern ConVar tf_halloween_kart_boost_duration;

extern ConVar cl_thirdperson;

#ifdef STAGING_ONLY
ConVar test_local_player_skin_override( "test_local_player_skin_override", "-1", FCVAR_CHEAT, "If >= 0, force this skin index to be used by the local player model." );
ConVar tf_test_setkillcount( "tf_test_setkillcount", "0", FCVAR_NONE, "Force a kill streak count for eye glow testing" );

extern ConVar tf_random_item_min;
extern ConVar tf_random_item_max;

extern ConVar tf_killstreak_eyeglow;
extern ConVar tf_killstreak_color;
extern ConVar tf_eyeglow_wip;

ConVar tf_sheen_all( "tf_sheen_all", "0", FCVAR_NONE, "When on, enables sheens on all weapons" );
ConVar tf_sheen_fast( "tf_sheen_fast", "0", FCVAR_NONE, "When on, no delay's between sheens" );
ConVar tf_sheen_shader_override( "tf_sheen_shader_override", "0", FCVAR_NONE, "Set for shader effect override" );

ConVar tf_sheen_color_override_r( "tf_sheen_color_override_r", "0", FCVAR_HIDDEN, "Set by ConCommand tf_sheen_color_override" );
ConVar tf_sheen_color_override_g( "tf_sheen_color_override_g", "0", FCVAR_HIDDEN, "Set by ConCommand tf_sheen_color_override" );
ConVar tf_sheen_color_override_b( "tf_sheen_color_override_b", "0", FCVAR_HIDDEN, "Set by ConCommand tf_sheen_color_override" );
ConVar tf_sheen_color_override_a( "tf_sheen_color_override_a", "0", FCVAR_HIDDEN, "Set by ConCommand tf_sheen_color_override" );

CON_COMMAND_F ( tf_sheen_color_override, "Sheen Color Override RGBA 0-255", 0 )
{
	if ( args.ArgC() < 5 )
	{
		Msg( "requires 4 inputs, r g b a (in 0 - 255 range) \n" );
		return;
	}

	float r = atof( args.Arg(1) ) / 255.0f;
	float g = atof( args.Arg(2) ) / 255.0f;
	float b = atof( args.Arg(3) ) / 255.0f;
	float a = atof( args.Arg(4) ) / 255.0f;

	tf_sheen_color_override_r.SetValue( r );
	tf_sheen_color_override_g.SetValue( g );
	tf_sheen_color_override_b.SetValue( b );
	tf_sheen_color_override_a.SetValue( a );
}

ConVar cl_taunt_max_turn_speed( "cl_taunt_max_turn_speed", "0", FCVAR_HIDDEN | FCVAR_CHEAT );
ConVar cl_halloween_kart_turn_speed( "cl_halloween_kart_turn_speed", "100" );
#endif // Staging only

ConVar tf_sheen_framerate( "tf_sheen_framerate", "25", FCVAR_NONE | FCVAR_HIDDEN, "Set Sheen Frame Rate" );

extern ConVar tf_killstreak_alwayson;

ConVar tf_sheen_alpha_firstperson( "tf_sheen_alpha_firstperson", "0.1", FCVAR_NONE, "Set the Alpha Value for first person sheens" );
ConVar tf_killstreakeyes_minkills( "tf_killstreakeyes_minkills", "5", FCVAR_DEVELOPMENTONLY, "min kills to get base eyeglow" );
ConVar tf_killstreakeyes_maxkills( "tf_killstreakeyes_maxkills", "10", FCVAR_DEVELOPMENTONLY, "kills to get the max eye glow effect" );

//ConVar spectate_random_server_basetime( "spectate_random_server_basetime", "240", FCVAR_DEVELOPMENTONLY );

ConVar cl_autorezoom( "cl_autorezoom", "1", FCVAR_USERINFO | FCVAR_ARCHIVE, "When set to 1, sniper rifle will re-zoom after firing a zoomed shot." );
ConVar tf_remember_activeweapon( "tf_remember_activeweapon", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_USERINFO, "Setting this to 1 will make the active weapon persist between lives." );
ConVar tf_remember_lastswitched( "tf_remember_lastswitched", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_USERINFO, "Setting this to 1 will make the 'last weapon' persist between lives." );
ConVar cl_autoreload( "cl_autoreload", "1", FCVAR_USERINFO | FCVAR_ARCHIVE, "When set to 1, clip-using weapons will automatically be reloaded whenever they're not being fired." );

ConVar tf_respawn_on_loadoutchanges( "tf_respawn_on_loadoutchanges", "1", FCVAR_ARCHIVE, "When set to 1, you will automatically respawn whenever you change loadouts inside a respawn zone." );

ConVar sb_dontshow_maxplayer_warning( "sb_dontshow_maxplayer_warning", "0", FCVAR_ARCHIVE );
ConVar sb_close_browser_on_connect( "sb_close_browser_on_connect", "1", FCVAR_ARCHIVE );

ConVar tf_spectate_pyrovision( "tf_spectate_pyrovision", "0", FCVAR_ARCHIVE, "When on, spectator will see the world with Pyrovision active", VisionMode_ChangeCallback );
ConVar tf_replay_pyrovision( "tf_replay_pyrovision", "0", FCVAR_ARCHIVE, "When on, replays will be seen with Pyrovision active", VisionMode_ChangeCallback );

ConVar tf_taunt_first_person( "tf_taunt_first_person", "0", FCVAR_NONE, "1 = taunts remain first-person" );

ConVar tf_romevision_opt_in( "tf_romevision_opt_in", "0", FCVAR_ARCHIVE, "Enable Romevision in Mann vs. Machine mode when available." );
ConVar tf_romevision_skip_prompt( "tf_romevision_skip_prompt", "0", FCVAR_ARCHIVE, "If nonzero, skip the prompt about sharing Romevision." );

#ifdef STAGING_ONLY
extern ConVar tf_bountymode_showhealth;
#endif // STAGING_ONLY

#define BDAY_HAT_MODEL		"models/effects/bday_hat.mdl"
#define BOMB_HAT_MODEL		"models/props_lakeside_event/bomb_temp_hat.mdl"
#define BOMBONOMICON_MODEL  "models/props_halloween/bombonomicon.mdl"

IMaterial	*g_pHeadLabelMaterial[2] = { NULL, NULL }; 
void	SetupHeadLabelMaterials( void );

extern CBaseEntity *BreakModelCreateSingle( CBaseEntity *pOwner, breakmodel_t *pModel, const Vector &position, 
										   const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, int nSkin, const breakablepropparams_t &params );

extern int EconWear_ToIntCategory( float flWear );

const char *g_pszHeadGibs[] =
{
	"",
	"models/player\\gibs\\scoutgib007.mdl",
	"models/player\\gibs\\snipergib005.mdl",
	"models/player\\gibs\\soldiergib007.mdl",
	"models/player\\gibs\\demogib006.mdl",
	"models/player\\gibs\\medicgib007.mdl",
	"models/player\\gibs\\heavygib007.mdl",
	"models/player\\gibs\\pyrogib008.mdl",
	"models/player\\gibs\\spygib007.mdl",
	"models/player\\gibs\\engineergib006.mdl",
};

const char *g_pszBotHeadGibs[] =
{
	"",
	"models/bots\\gibs\\scoutbot_gib_head.mdl",
	"models/bots\\gibs\\sniperbot_gib_head.mdl",
	"models/bots\\gibs\\soldierbot_gib_head.mdl",
	"models/bots\\gibs\\demobot_gib_head.mdl",
	"models/bots\\gibs\\medicbot_gib_head.mdl",
	"models/bots\\gibs\\heavybot_gib_head.mdl",
	"models/bots\\gibs\\pyrobot_gib_head.mdl",
	"models/bots\\gibs\\spybot_gib_head.mdl",
	"models/bots\\gibs\\engineerbot_gib_head.mdl",
};

const char *pszHeadLabelNames[] =
{
	"effects/speech_voice_red",
	"effects/speech_voice_blue"
};

extern SkyBoxMaterials_t s_PyroSkyboxMaterials;

#define TF_PLAYER_HEAD_LABEL_RED 0
#define TF_PLAYER_HEAD_LABEL_BLUE 1

CLIENTEFFECT_REGISTER_BEGIN( PrecacheInvuln )
CLIENTEFFECT_MATERIAL( "models/effects/invulnfx_blue.vmt" )
CLIENTEFFECT_MATERIAL( "models/effects/invulnfx_red.vmt" )
CLIENTEFFECT_REGISTER_END()

// *********************************************************************************************************
// KillStreak Effect Data
// *********************************************************************************************************
struct killstreak_params_t
{
	killstreak_params_t ( int iShaderIndex, const char *pTexture, bool bHasTeamColor,
		int sheen_r,  int sheen_g,  int sheen_b,  int sheen_a, 
		int color1_r, int color1_g, int color1_b, int color1_a, 
		int color2_r, int color2_g, int color2_b, int color2_a
		) {
			m_iShaderIndex = iShaderIndex;
			m_pTexture = pTexture;

			m_bHasTeamColor = bHasTeamColor;

			m_sheen_r = (float)sheen_r / 255.0f;
			m_sheen_g = (float)sheen_g / 255.0f;
			m_sheen_b = (float)sheen_b / 255.0f; 
			m_sheen_a = (float)sheen_a / 255.0f;

			m_color1_r = (float)color1_r / 255.0f;
			m_color1_g = (float)color1_g / 255.0f;
			m_color1_b = (float)color1_b / 255.0f; 
			m_color1_a = (float)color1_a / 255.0f;

			m_color2_r = (float)color2_r / 255.0f;
			m_color2_g = (float)color2_g / 255.0f;
			m_color2_b = (float)color2_b / 255.0f; 
			m_color2_a = (float)color2_a / 255.0f;
	}

	int			m_iShaderIndex;
	bool		m_bHasTeamColor;
	const char *m_pTexture;
	float m_sheen_r, m_sheen_g, m_sheen_b, m_sheen_a;
	float m_color1_r, m_color1_g, m_color1_b, m_color1_a;
	float m_color2_r, m_color2_g, m_color2_b, m_color2_a;
};

// *********************************************************************************************************
// Base Sheen Colors (Team Red)
static const killstreak_params_t g_KillStreakEffectsBase[] =
{
	killstreak_params_t( 0, NULL, false,	0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0 ),	// Empty (Index 0)
	killstreak_params_t( 0, "Effects/AnimatedSheen/animatedsheen0", true,	200, 20,  15,  255, 255, 118, 118, 255, 255, 35,  28,  255 ),
	killstreak_params_t( 0, "Effects/AnimatedSheen/animatedsheen0", false,	242, 172, 10,  255, 255, 237, 138, 255, 255, 213, 65,  255 ),
	killstreak_params_t( 0, "Effects/AnimatedSheen/animatedsheen0", false,	255, 75,  5,   255, 255, 111, 5,   255, 255, 137, 31,  255 ),
	killstreak_params_t( 0, "Effects/AnimatedSheen/animatedsheen0", false,	100, 255, 10,  255, 230, 255, 60,  255, 193, 255, 61,  255 ),
	killstreak_params_t( 0, "Effects/AnimatedSheen/animatedsheen0", false,	40,  255, 70,  255, 103, 255, 121, 255, 165, 255, 193, 255 ),
	killstreak_params_t( 0, "Effects/AnimatedSheen/animatedsheen0", false,	105, 20,  255, 255, 105, 20,  255, 255, 185, 145, 255, 255 ),
	killstreak_params_t( 0, "Effects/AnimatedSheen/animatedsheen0", false,	255, 30,  255, 255, 255, 120, 255, 255, 255, 176, 217, 255 ),
};
// *********************************************************************************************************
// Optional Team Color
static const killstreak_params_t g_KillStreakEffectsBlue[] =
{
	killstreak_params_t( 0, NULL, false,	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ),	// Empty (Index 0)
	killstreak_params_t( 0, "Effects/AnimatedSheen/animatedsheen0", true, 40, 98, 200, 255, 0, 92, 255, 255, 134, 203, 243, 255 ),
};

// thirdperson medieval
static ConVar tf_medieval_thirdperson( "tf_medieval_thirdperson", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE , "Turns on third-person camera in medieval mode." );
static ConVar tf_medieval_cam_idealdist( "tf_medieval_cam_idealdist", "125", FCVAR_CLIENTDLL | FCVAR_CHEAT );	 // thirdperson distance
static ConVar tf_medieval_cam_idealdistright( "tf_medieval_cam_idealdistright", "25", FCVAR_CLIENTDLL | FCVAR_CHEAT );	 // thirdperson distance
static ConVar tf_medieval_cam_idealdistup( "tf_medieval_cam_idealdistup", "-10", FCVAR_CLIENTDLL | FCVAR_CHEAT );	 // thirdperson distance
static ConVar tf_medieval_cam_idealpitch( "tf_medieval_cam_idealpitch", "0", FCVAR_CLIENTDLL | FCVAR_CHEAT );	 // thirdperson pitch
extern ConVar cam_idealpitch;
extern ConVar tf_allow_taunt_switch;

static void PromptAcceptReviveCallback( bool bCancel, void *pContext )
{
	if ( bCancel )
	{
		KeyValues *kv = new KeyValues( "MVM_Revive_Response" );
		kv->SetBool( "accepted", false );
		engine->ServerCmdKeyValues( kv );
	}
}


C_TFPlayerPreviewEffect g_PlayerPreviewEffect;


C_TFPlayerPreviewEffect::C_TFPlayerPreviewEffect() :
	m_nPreviewEffect(PREVIEW_EFFECT_NONE)
,	m_nTeam(-1)
{
}

void C_TFPlayerPreviewEffect::SetTeam(int nTeam)
{
	if ( nTeam == m_nTeam )
		return;

	m_nTeam = nTeam;

	const char *pszMaterial = NULL;
	switch ( m_nTeam )
	{
	case TF_TEAM_BLUE:	
		pszMaterial = "models/effects/invulnfx_blue.vmt";
		break;
	case TF_TEAM_RED:	
		pszMaterial = "models/effects/invulnfx_red.vmt";
		break;
	default:
		break;
	}
	if ( pszMaterial )
	{
		m_InvulnerableMaterial.Init( pszMaterial, TEXTURE_GROUP_CLIENT_EFFECTS );
	}
	else
	{
		m_InvulnerableMaterial.Shutdown();
	}
}

void C_TFPlayerPreviewEffect::Reset()
{
	SetEffect(PREVIEW_EFFECT_NONE);
	SetTeam(-1);
}


C_EntityDissolve *DissolveEffect( C_BaseEntity *pTarget, float flTime );

void SetAppropriateCamera( C_TFPlayer *pPlayer )
{
	if ( pPlayer->IsLocalPlayer() == false )
		return;

	if ( TFGameRules() &&
		( ( TFGameRules()->IsInMedievalMode() && tf_medieval_thirdperson.GetBool() )
		|| pPlayer->m_Shared.InCond( TF_COND_HALLOWEEN_GHOST_MODE ) ) )
	{
		g_ThirdPersonManager.SetForcedThirdPerson( true );
		Vector offset( tf_medieval_cam_idealdist.GetFloat(), tf_medieval_cam_idealdistright.GetFloat(), tf_medieval_cam_idealdistup.GetFloat() );
		g_ThirdPersonManager.SetDesiredCameraOffset( offset );
		cam_idealpitch.SetValue( tf_medieval_cam_idealpitch.GetFloat() );

		::input->CAM_ToThirdPerson();

		pPlayer->ThirdPersonSwitch( true );
	}
	else
	{
		g_ThirdPersonManager.SetForcedThirdPerson( false );
	}
}

// -------------------------------------------------------------------------------- //
// Player animation event. Sent to the client when a player fires, jumps, reloads, etc..
// -------------------------------------------------------------------------------- //

class C_TEPlayerAnimEvent : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEPlayerAnimEvent, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	virtual void PostDataUpdate( DataUpdateType_t updateType )
	{
		VPROF( "C_TEPlayerAnimEvent::PostDataUpdate" );

		// Create the effect.
		if ( m_iPlayerIndex == TF_PLAYER_INDEX_NONE )
			return;

		EHANDLE hPlayer = cl_entitylist->GetNetworkableHandle( m_iPlayerIndex );
		if ( !hPlayer )
			return;

		C_TFPlayer *pPlayer = dynamic_cast< C_TFPlayer* >( hPlayer.Get() );
		if ( pPlayer && !pPlayer->IsDormant() )
		{
			// Ignore anim events that are also played on the client.
			PlayerAnimEvent_t animEvent = (PlayerAnimEvent_t) m_iEvent.Get();
			switch ( animEvent )
			{
			case PLAYERANIMEVENT_STUN_BEGIN:
			case PLAYERANIMEVENT_STUN_MIDDLE:
			case PLAYERANIMEVENT_STUN_END:
			case PLAYERANIMEVENT_PASSTIME_THROW_BEGIN:
			case PLAYERANIMEVENT_PASSTIME_THROW_MIDDLE:
			case PLAYERANIMEVENT_PASSTIME_THROW_END:
			case PLAYERANIMEVENT_PASSTIME_THROW_CANCEL:
				break; // ignore these
			default:
				pPlayer->DoAnimationEvent( animEvent, m_nData );
				break;
			};
		}	
	}

public:
	CNetworkVar( int, m_iPlayerIndex );
	CNetworkVar( int, m_iEvent );
	CNetworkVar( int, m_nData );
};

IMPLEMENT_CLIENTCLASS_EVENT( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent, CTEPlayerAnimEvent );

//-----------------------------------------------------------------------------
// Data tables and prediction tables.
//-----------------------------------------------------------------------------
BEGIN_RECV_TABLE_NOBASE( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent )
	RecvPropInt( RECVINFO( m_iPlayerIndex ) ),
	RecvPropInt( RECVINFO( m_iEvent ) ),
	RecvPropInt( RECVINFO( m_nData ) )
END_RECV_TABLE()


//=============================================================================
//
// Ragdoll
//
// ----------------------------------------------------------------------------- //
// Client ragdoll entity.
// ----------------------------------------------------------------------------- //
ConVar cl_ragdoll_physics_enable( "cl_ragdoll_physics_enable", "1", 0, "Enable/disable ragdoll physics." );
ConVar cl_ragdoll_fade_time( "cl_ragdoll_fade_time", "15", FCVAR_CLIENTDLL );
ConVar cl_ragdoll_forcefade( "cl_ragdoll_forcefade", "0", FCVAR_CLIENTDLL );
ConVar cl_ragdoll_pronecheck_distance( "cl_ragdoll_pronecheck_distance", "64", FCVAR_GAMEDLL );

class C_TFRagdoll : public C_BaseFlex
{
public:

	DECLARE_CLASS( C_TFRagdoll, C_BaseFlex );
	DECLARE_CLIENTCLASS();
	
	C_TFRagdoll();
	~C_TFRagdoll();

	virtual void OnDataChanged( DataUpdateType_t type );

	IRagdoll* GetIRagdoll() const;

	void ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName );

	void ClientThink( void );

	// Deal with recording
	virtual void GetToolRecordingState( KeyValues *msg );

	void StartFadeOut( float fDelay );
	void EndFadeOut();
	void DissolveEntity( CBaseEntity* pEnt );

	EHANDLE GetPlayerHandle( void ) 	
	{
		if ( m_iPlayerIndex == TF_PLAYER_INDEX_NONE )
			return NULL;
		return cl_entitylist->GetNetworkableHandle( m_iPlayerIndex );
	}

	bool IsRagdollVisible();
	float GetBurnStartTime() { return m_flBurnEffectStartTime; }

	virtual void BuildTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed );

	virtual void SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights );

	bool IsDeathAnim() { return m_bDeathAnim; }

	int GetDamageCustom() { return m_iDamageCustom; }

	virtual bool GetAttachment( int iAttachment, matrix3x4_t &attachmentToWorld );

	int GetClass() { return m_iClass; }

	float GetPercentInvisible( void ) { return m_flPercentInvisible; }
	bool IsCloaked( void ) { return m_bCloaked; }

	int GetRagdollTeam( void ){ return m_iTeam; }

	float GetHeadScale() const { return m_flHeadScale; }
	float GetTorsoScale() const { return m_flTorsoScale; }
	float GetHandScale() const { return m_flHandScale; }

private:
	
	C_TFRagdoll( const C_TFRagdoll & ) {}

	void Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity );

	void CreateTFRagdoll();
	void CreateTFGibs( bool bDestroyRagdoll = true, bool bCurrentPosition = false );
	void CreateWearableGibs( bool bDisguiseWearables );
	void CreateTFHeadGib();

	virtual float FrameAdvance( float flInterval );

	bool IsDecapitation();
	bool IsHeadSmash();

	virtual int	InternalDrawModel( int flags );

private:

	CNetworkVector( m_vecRagdollVelocity );
	CNetworkVector( m_vecRagdollOrigin );
	int	  m_iPlayerIndex;
	float m_fDeathTime;
	bool  m_bFadingOut;
	bool  m_bGib;
	bool  m_bBurning;
	bool  m_bElectrocuted;
	bool  m_bBatted;
	bool  m_bDissolving;
	bool  m_bFeignDeath;
	bool  m_bWasDisguised;
	bool  m_bCloaked;
	bool  m_bBecomeAsh;
	int	  m_iDamageCustom;
	bool  m_bGoldRagdoll;
	bool  m_bIceRagdoll;
	CountdownTimer m_freezeTimer;
	CountdownTimer m_frozenTimer;
	int	  m_iTeam;
	int	  m_iClass;
	float m_flBurnEffectStartTime;	// start time of burning, or 0 if not burning
	bool  m_bRagdollOn;
	bool  m_bDeathAnim;				
	bool  m_bOnGround;
	bool  m_bFixedConstraints;
	matrix3x4_t m_mHeadAttachment;
	bool  m_bBaseTransform;
	float m_flPercentInvisible;
	float m_flTimeToDissolve;
	bool  m_bCritOnHardHit;	// plays the red mist particle effect
	float m_flHeadScale;
	float m_flTorsoScale;
	float m_flHandScale;

	CMaterialReference		m_MaterialOverride;

	CUtlVector<CHandle<CEconWearable > > m_hRagWearables;		// These look like they are no longer used?

	CUtlVector< CHandle< CEconWearable > > m_hClientWearables;	// wearables on the ragdoll that are "following" it

	bool  m_bCreatedWhilePlaybackSkipping;
};

IMPLEMENT_CLIENTCLASS_DT_NOBASE( C_TFRagdoll, DT_TFRagdoll, CTFRagdoll )
	RecvPropVector( RECVINFO(m_vecRagdollOrigin) ),
	RecvPropInt( RECVINFO( m_iPlayerIndex ) ),
	RecvPropVector( RECVINFO(m_vecForce) ),
	RecvPropVector( RECVINFO(m_vecRagdollVelocity) ),
	RecvPropInt( RECVINFO( m_nForceBone ) ),
	RecvPropBool( RECVINFO( m_bGib ) ),
	RecvPropBool( RECVINFO( m_bBurning ) ),
	RecvPropBool( RECVINFO( m_bElectrocuted ) ),
	RecvPropBool( RECVINFO( m_bFeignDeath ) ),
	RecvPropBool( RECVINFO( m_bWasDisguised ) ),
	RecvPropBool( RECVINFO( m_bOnGround ) ),
	RecvPropBool( RECVINFO( m_bCloaked ) ),
	RecvPropBool( RECVINFO( m_bBecomeAsh ) ),
	RecvPropInt( RECVINFO( m_iDamageCustom ) ),
	RecvPropInt( RECVINFO( m_iTeam ) ),
	RecvPropInt( RECVINFO( m_iClass ) ),		
	RecvPropUtlVector( RECVINFO_UTLVECTOR( m_hRagWearables ), 8,	RecvPropEHandle(NULL, 0, 0) ),
	RecvPropBool( RECVINFO( m_bGoldRagdoll ) ),
	RecvPropBool( RECVINFO( m_bIceRagdoll ) ),
	RecvPropBool( RECVINFO( m_bCritOnHardHit ) ),
	RecvPropFloat( RECVINFO( m_flHeadScale ) ),
	RecvPropFloat( RECVINFO( m_flTorsoScale ) ),
	RecvPropFloat( RECVINFO( m_flHandScale ) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
C_TFRagdoll::C_TFRagdoll()
{
	m_iPlayerIndex = TF_PLAYER_INDEX_NONE;
	m_fDeathTime = -1;
	m_bFadingOut = false;
	m_bGib = false;
	m_bBurning = false;
	m_bElectrocuted = false;
	m_bBatted = false;
	m_bDissolving = false;
	m_bFeignDeath = false;
	m_bWasDisguised = false;
	m_bCloaked = false;
	m_bBecomeAsh = false;
	m_flBurnEffectStartTime = 0.0f;
	m_iDamageCustom = 0;
	m_bGoldRagdoll = false;
	m_bIceRagdoll = false;
	m_freezeTimer.Invalidate();
	m_frozenTimer.Invalidate();
	m_iTeam = -1;
	m_iClass = -1;
	m_nForceBone = -1;
	m_bRagdollOn = false;
	m_bDeathAnim = false;
	m_bOnGround = false;
	m_bBaseTransform = false;
	m_bFixedConstraints = false;
	m_flTimeToDissolve = 0.3f;
	m_bCritOnHardHit = false;
	m_flHeadScale = 1.f;
	m_flTorsoScale = 1.f;
	m_flHandScale = 1.f;

	UseClientSideAnimation();

	m_bCreatedWhilePlaybackSkipping = engine->IsSkippingPlayback();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
C_TFRagdoll::~C_TFRagdoll()
{
	PhysCleanupFrictionSounds( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSourceEntity - 
//-----------------------------------------------------------------------------
void C_TFRagdoll::Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity )
{
	if ( !pSourceEntity )
		return;
	
	VarMapping_t *pSrc = pSourceEntity->GetVarMapping();
	VarMapping_t *pDest = GetVarMapping();
    	
	// Find all the VarMapEntry_t's that represent the same variable.
	for ( int i = 0; i < pDest->m_Entries.Count(); i++ )
	{
		VarMapEntry_t *pDestEntry = &pDest->m_Entries[i];
		for ( int j=0; j < pSrc->m_Entries.Count(); j++ )
		{
			VarMapEntry_t *pSrcEntry = &pSrc->m_Entries[j];
			if ( V_streq( pSrcEntry->watcher->GetDebugName(), pDestEntry->watcher->GetDebugName() ) )
			{
				pDestEntry->watcher->Copy( pSrcEntry->watcher );
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Setup vertex weights for drawing
//-----------------------------------------------------------------------------
void C_TFRagdoll::SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights )
{
	// While we're dying, we want to mimic the facial animation of the player. Once they're dead, we just stay as we are.
	EHANDLE hPlayer = GetPlayerHandle();
	if ( ( hPlayer && hPlayer->IsAlive()) || !hPlayer )
	{
		BaseClass::SetupWeights( pBoneToWorld, nFlexWeightCount, pFlexWeights, pFlexDelayedWeights );
	}
	else if ( hPlayer )
	{
		hPlayer->SetupWeights( pBoneToWorld, nFlexWeightCount, pFlexWeights, pFlexDelayedWeights );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTrace - 
//			iDamageType - 
//			*pCustomImpactName - 
//-----------------------------------------------------------------------------
void C_TFRagdoll::ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName )
{
	VPROF( "C_TFRagdoll::ImpactTrace" );
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	if( !pPhysicsObject )
		return;

	Vector vecDir;
	VectorSubtract( pTrace->endpos, pTrace->startpos, vecDir );

	if ( iDamageType == DMG_BLAST )
	{
		// Adjust the impact strength and apply the force at the center of mass.
		vecDir *= 4000;
		pPhysicsObject->ApplyForceCenter( vecDir );
	}
	else
	{
		// Find the apporx. impact point.
		Vector vecHitPos;  
		VectorMA( pTrace->startpos, pTrace->fraction, vecDir, vecHitPos );
		VectorNormalize( vecDir );

		// Adjust the impact strength and apply the force at the impact point..
		vecDir *= 4000;
		pPhysicsObject->ApplyForceOffset( vecDir, vecHitPos );	
	}

	m_pRagdoll->ResetRagdollSleepAfterTime();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void C_TFRagdoll::CreateTFRagdoll()
{
	// Get the player.
	C_TFPlayer *pPlayer = NULL;
	EHANDLE hPlayer = GetPlayerHandle();
	if ( hPlayer )
	{
		pPlayer = dynamic_cast<C_TFPlayer*>( hPlayer.Get() );
	}

	int nModelIndex = -1;

	if ( pPlayer && pPlayer->GetPlayerClass() && !pPlayer->ShouldDrawSpyAsDisguised() )
	{
		nModelIndex = modelinfo->GetModelIndex( pPlayer->GetPlayerClass()->GetModelName() );
	}
	else
	{
		TFPlayerClassData_t *pData = GetPlayerClassData( m_iClass );
		if ( pData )
		{
			nModelIndex = modelinfo->GetModelIndex( pData->GetModelName() );
		}
	}

	if ( pPlayer )
	{
		m_flHeadScale = pPlayer->GetHeadScale();
		m_flTorsoScale = pPlayer->GetTorsoScale();
		m_flHandScale = pPlayer->GetHandScale();
	}

	if ( nModelIndex != -1 )
	{
		SetModelIndex( nModelIndex );	

		if ( m_iTeam == TF_TEAM_RED )
		{
			m_nSkin = 0;
		}
		else
		{
			m_nSkin = 1;
		}
	}

	// Check for any special player skin override behaviour.
	if ( pPlayer && pPlayer->BRenderAsZombie() )
	{
		C_TFPlayer::AdjustSkinIndexForZombie( m_iClass, m_nSkin );
	}

	// We check against new-style (special flag to indicate goldification) and old style (custom damage type)
	// to maintain old demos involving the golden wrench.
	if ( m_bGoldRagdoll || m_iDamageCustom == TF_DMG_CUSTOM_GOLD_WRENCH )
	{
		EmitSound( "Saxxy.TurnGold" );
		m_bFixedConstraints = true;
	}

	if ( m_bIceRagdoll )
	{
		EmitSound( "Icicle.TurnToIce" );
		ParticleProp()->Create( "xms_icicle_impact_dryice", PATTACH_ABSORIGIN_FOLLOW );
		m_freezeTimer.Start( RandomFloat( 0.1f, 0.75f ) );
		m_frozenTimer.Start( RandomFloat( 9.0f, 11.0f ) );
	}

#ifndef NDEBUG
	DevMsg( 2, "CreateTFRagdoll %d %d\n", gpGlobals->framecount, pPlayer ? pPlayer->entindex() : 0 );
#endif

	if ( pPlayer && !pPlayer->IsDormant() )
	{
		// Move my current model instance to the ragdoll's so decals are preserved.
		pPlayer->SnatchModelInstance( this );

		VarMapping_t *varMap = GetVarMapping();

		// Copy all the interpolated vars from the player entity.
		// The entity uses the interpolated history to get bone velocity.		
		if ( !pPlayer->IsLocalPlayer() && pPlayer->IsInterpolationEnabled() )
		{
			Interp_Copy( pPlayer );

			SetAbsAngles( pPlayer->GetRenderAngles() );
			GetRotationInterpolator().Reset();

			m_flAnimTime = pPlayer->m_flAnimTime;
			SetSequence( pPlayer->GetSequence() );
			m_flPlaybackRate = pPlayer->GetPlaybackRate();
		}
		else
		{
			// This is the local player, so set them in a default
			// pose and slam their velocity, angles and origin
			SetAbsOrigin( /* m_vecRagdollOrigin : */ pPlayer->GetRenderOrigin() );			
			SetAbsAngles( pPlayer->GetRenderAngles() );
			SetAbsVelocity( m_vecRagdollVelocity );

			// Hack! Find a neutral standing pose or use the idle.
			int iSeq = LookupSequence( "RagdollSpawn" );
			if ( iSeq == -1 )
			{
				Assert( false );
				iSeq = 0;
			}			
			SetSequence( iSeq );
			SetCycle( 0.0 );

			Interp_Reset( varMap );
		}

		if ( !m_bFeignDeath || m_bWasDisguised )
		{
			pPlayer->RecalcBodygroupsIfDirty();
			m_nBody = pPlayer->GetBody();
		}
	}
	else
	{
		// Overwrite network origin so later interpolation will use this position.
		SetNetworkOrigin( m_vecRagdollOrigin );
		SetAbsOrigin( m_vecRagdollOrigin );
		SetAbsVelocity( m_vecRagdollVelocity );

		Interp_Reset( GetVarMapping() );
	}

	if ( IsCloaked() )
	{
		AddEffects( EF_NOSHADOW );
	}

	// Play a death anim depending on the custom damage type.
	bool bPlayDeathInAir = false;
	int iDeathSeq = -1;
	if ( pPlayer && !m_bGoldRagdoll )
	{
		iDeathSeq = pPlayer->m_Shared.GetSequenceForDeath( this, m_bBurning, m_iDamageCustom );

		if ( m_bDissolving && !m_bGib )
		{
			bPlayDeathInAir = true;
			iDeathSeq = LookupSequence( "dieviolent" );
		}

		// did we find a death sequence?
		if ( iDeathSeq > -1 && (m_iDamageCustom != TF_DMG_CUSTOM_TAUNTATK_BARBARIAN_SWING) &&
			(m_iDamageCustom != TF_DMG_CUSTOM_TAUNTATK_ENGINEER_GUITAR_SMASH) && (m_iDamageCustom != TF_DMG_CUSTOM_TAUNTATK_ALLCLASS_GUITAR_RIFF) )
		{
			// we only want to show the death anims 25% of the time, unless this is a demoman kill taunt
			// always play backstab animations for the ice ragdoll
			if ( !m_bIceRagdoll && !tf_always_deathanim.GetBool() && (RandomFloat( 0, 1 ) > 0.25f) )
			{
				iDeathSeq = -1;
			}
		}
	}

	bool bPlayDeathAnim = cl_ragdoll_physics_enable.GetBool() && (iDeathSeq > -1) && pPlayer;

	if ( !m_bOnGround && bPlayDeathAnim && !bPlayDeathInAir )
		bPlayDeathAnim = false; // Don't play most death anims in the air (headshot, etc).

	if ( bPlayDeathAnim )
	{
		// Set our position for a death anim.
		SetAbsOrigin( pPlayer->GetRenderOrigin() );			
		SetAbsAngles( pPlayer->GetRenderAngles() );
		SetAbsVelocity( Vector(0,0,0) );
		m_vecForce = Vector(0,0,0);

		// Play the death anim.
		ResetSequence( iDeathSeq );
		m_bDeathAnim = true;
	}
	else if ( m_bIceRagdoll )
	{
		// couldn't play death anim because we were in midair - go ridig immediately
		m_freezeTimer.Invalidate();
		m_frozenTimer.Invalidate();
		m_bFixedConstraints = true;
	}

	// Fade out the ragdoll in a while
	StartFadeOut( cl_ragdoll_fade_time.GetFloat() );
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	// Copy over impact attachments.
	if ( pPlayer )
	{
		pPlayer->CreateBoneAttachmentsFromWearables( this, m_bWasDisguised );
		pPlayer->MoveBoneAttachments( this );
	}


	if ( m_iDamageCustom == TF_DMG_CUSTOM_KART )
	{
		m_vecForce *= 100.0f;
		SetAbsVelocity( GetAbsVelocity() + m_vecForce );
		ApplyAbsVelocityImpulse( m_vecForce );
	}

	// Save ragdoll information.
	if ( cl_ragdoll_physics_enable.GetBool() && !m_bDeathAnim )
	{
		// Make us a ragdoll..
		m_nRenderFX = kRenderFxRagdoll;

		matrix3x4_t boneDelta0[MAXSTUDIOBONES];
		matrix3x4_t boneDelta1[MAXSTUDIOBONES];
		matrix3x4_t currentBones[MAXSTUDIOBONES];
		const float boneDt = 0.05f;

		// We have to make sure that we're initting this client ragdoll off of the same model.
		// GetRagdollInitBoneArrays uses the *player* Hdr, which may be a different model than
		// the ragdoll Hdr, if we try to create a ragdoll in the same frame that the player
		// changes their player model.
		CStudioHdr *pRagdollHdr = GetModelPtr();
		CStudioHdr *pPlayerHdr = pPlayer ? pPlayer->GetModelPtr() : NULL;

		bool bChangedModel = false;

		if ( pRagdollHdr && pPlayerHdr )
		{
			bChangedModel = pRagdollHdr->GetVirtualModel() != pPlayerHdr->GetVirtualModel();

			// Assert( !bChangedModel && "C_TFRagdoll::CreateTFRagdoll: Trying to create ragdoll with a different model than the player it's based on" );
		}

		bool bBoneArraysInited;
		if ( pPlayer && !pPlayer->IsDormant() && !bChangedModel )
		{
			bBoneArraysInited = pPlayer->GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );
		}
		else
		{
			bBoneArraysInited = GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );
		}
		
		if ( bBoneArraysInited )
		{
			InitAsClientRagdoll( boneDelta0, boneDelta1, currentBones, boneDt, m_bFixedConstraints );
		}
	}
	else
	{
		ClientLeafSystem()->SetRenderGroup( GetRenderHandle(), RENDER_GROUP_TRANSLUCENT_ENTITY );
	}

	if ( m_bBurning )
	{
		m_flBurnEffectStartTime = gpGlobals->curtime;
		ParticleProp()->Create( "burningplayer_corpse", PATTACH_ABSORIGIN_FOLLOW );
	}

	if ( m_bElectrocuted )
	{
		const char *pEffectName = ( m_iTeam == TF_TEAM_RED ) ? "electrocuted_red" : "electrocuted_blue";
		ParticleProp()->Create( pEffectName, PATTACH_ABSORIGIN_FOLLOW );
		C_BaseEntity::EmitSound( "TFPlayer.MedicChargedDeath" );
	}

	if ( m_bBecomeAsh && !m_bDissolving && !m_bGib )
	{
		ParticleProp()->Create( "drg_fiery_death", PATTACH_ABSORIGIN_FOLLOW );
		m_flTimeToDissolve = 0.5f;
	}

	if ( pPlayer->HasBombinomiconEffectOnDeath() && !m_bGib && !m_bDissolving )
	{
		m_flTimeToDissolve = 1.2f;
	}

	// Birthday mode.
	if ( pPlayer && TFGameRules() && TFGameRules()->IsBirthday() )
	{
		AngularImpulse angularImpulse( RandomFloat( 0.0f, 120.0f ), RandomFloat( 0.0f, 120.0f ), 0.0 );
		breakablepropparams_t breakParams( m_vecRagdollOrigin, GetRenderAngles(), m_vecRagdollVelocity, angularImpulse );
		breakParams.impactEnergyScale = 1.0f;
		pPlayer->DropPartyHat( breakParams, m_vecRagdollVelocity.GetForModify() );
	}

	const char *materialOverrideFilename = NULL;

	if ( m_bFixedConstraints )
	{
		if ( m_bGoldRagdoll )
		{
			// Gold texture...we've been turned into a golden corpse!
			materialOverrideFilename = "models/player/shared/gold_player.vmt";
		}
	}

	if ( m_bIceRagdoll )
	{
		// Ice texture...we've been turned into an ice statue!
		materialOverrideFilename = "models/player/shared/ice_player.vmt";
	}

	if ( materialOverrideFilename )
	{
		// Ice texture...we've been turned into an ice statue!
		m_MaterialOverride.Init( materialOverrideFilename, TEXTURE_GROUP_CLIENT_EFFECTS );

		// override all of our wearables, too
		for ( C_BaseEntity *pEntity = ClientEntityList().FirstBaseEntity(); pEntity; pEntity = ClientEntityList().NextBaseEntity(pEntity) )
		{
			if ( pEntity->GetFollowedEntity() == this )
			{
				CEconEntity *pItem = dynamic_cast< CEconEntity * >( pEntity );
				if ( pItem )
				{
					pItem->SetMaterialOverride( m_iTeam, materialOverrideFilename );
				}
			}
		}
	}
}

float C_TFRagdoll::FrameAdvance( float flInterval )
{
	// if we're in the process of becoming an ice statue, freeze
	if ( m_freezeTimer.HasStarted() && !m_freezeTimer.IsElapsed() )
	{
		// play the backstab anim until the timer is up
		return BaseClass::FrameAdvance( flInterval );
	}

	if ( m_frozenTimer.HasStarted() )
	{
		if ( m_frozenTimer.IsElapsed() )
		{
			// holding frozen time is up - turn to a stiff ragdoll and fall over
			m_frozenTimer.Invalidate();

			m_nRenderFX = kRenderFxRagdoll;

			matrix3x4_t boneDelta0[MAXSTUDIOBONES];
			matrix3x4_t boneDelta1[MAXSTUDIOBONES];
			matrix3x4_t currentBones[MAXSTUDIOBONES];
			const float boneDt = 0.1f;
			GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );
			InitAsClientRagdoll( boneDelta0, boneDelta1, currentBones, boneDt, true );

			SetAbsVelocity( Vector( 0,0,0 ) );
			m_bRagdollOn = true;
		}
		else
		{
			// don't move at all
			return 0.0f;
		}
	}

	float fRes = BaseClass::FrameAdvance( flInterval );

	if ( !m_bRagdollOn && IsSequenceFinished() && m_bDeathAnim )
	{
		m_nRenderFX = kRenderFxRagdoll;

		matrix3x4_t boneDelta0[MAXSTUDIOBONES];
		matrix3x4_t boneDelta1[MAXSTUDIOBONES];
		matrix3x4_t currentBones[MAXSTUDIOBONES];
		const float boneDt = 0.1f;
		if ( !GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt ) )
		{
			Warning( "C_TFRagdoll::FrameAdvance GetRagdollInitBoneArrays failed.\n" );
		}
		else
		{
			InitAsClientRagdoll( boneDelta0, boneDelta1, currentBones, boneDt );
		}

		SetAbsVelocity( Vector( 0,0,0 ) );
		m_bRagdollOn = true;

		// Make it fade out.
		StartFadeOut( cl_ragdoll_fade_time.GetFloat() );
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	return fRes;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_TFRagdoll::CreateTFHeadGib( void )
{
	C_TFPlayer *pPlayer = NULL;
	EHANDLE hPlayer = GetPlayerHandle();
	if ( hPlayer )
	{
		pPlayer = dynamic_cast<C_TFPlayer*>( hPlayer.Get() );
	}
	if ( pPlayer && ((pPlayer->m_hFirstGib == NULL) || m_bFeignDeath) )
	{
		Vector vecVelocity = m_vecForce + m_vecRagdollVelocity;
		VectorNormalize( vecVelocity );

		pPlayer->CreatePlayerGibs( m_vecRagdollOrigin, vecVelocity, m_vecForce.Length(), m_bBurning, false, true );
		// Decap Death Camera is disorienting on range Decaps (aka bullets)
		// Use normal Deathcam
		if ( m_iDamageCustom == TF_DMG_CUSTOM_HEADSHOT_DECAPITATION )
		{
			pPlayer->m_hHeadGib = NULL;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_TFRagdoll::CreateTFGibs( bool bDestroyRagdoll, bool bCurrentPosition )
{
	C_TFPlayer *pPlayer = NULL;
	EHANDLE hPlayer = GetPlayerHandle();
	if ( hPlayer )
	{
		pPlayer = dynamic_cast<C_TFPlayer*>( hPlayer.Get() );
	}
	
	if ( pPlayer && pPlayer->HasBombinomiconEffectOnDeath() )
	{
		m_vecForce *= 2.0f;
		m_vecForce.z *= 3.0f;

		DispatchParticleEffect( TFGameRules()->IsHolidayActive( kHoliday_Halloween ) ? "bombinomicon_burningdebris_halloween" : "bombinomicon_burningdebris", 
								bCurrentPosition ? GetAbsOrigin() : m_vecRagdollOrigin, GetAbsAngles() );
		EmitSound( "Bombinomicon.Explode" );
	}

	if ( pPlayer && ((pPlayer->m_hFirstGib == NULL) || m_bFeignDeath) )
	{
		Vector vecVelocity = m_vecForce + m_vecRagdollVelocity;
		VectorNormalize( vecVelocity );
		pPlayer->CreatePlayerGibs( bCurrentPosition ? pPlayer->GetRenderOrigin() : m_vecRagdollOrigin, vecVelocity, m_vecForce.Length(), m_bBurning );
	}

	if ( pPlayer )
	{
		if ( TFGameRules() && TFGameRules()->IsBirthdayOrPyroVision() )
		{
			DispatchParticleEffect( "bday_confetti", pPlayer->GetAbsOrigin() + Vector(0,0,32), vec3_angle );

			if ( TFGameRules() && TFGameRules()->IsBirthday() )
			{
				C_BaseEntity::EmitSound( "Game.HappyBirthday" );
			}
		}
		else if ( m_bCritOnHardHit && !UTIL_IsLowViolence() )
		{
			DispatchParticleEffect( "tfc_sniper_mist", pPlayer->WorldSpaceCenter(), vec3_angle );
		}
	}

	if ( bDestroyRagdoll )
	{
		EndFadeOut();
	}
	else
	{
		SetRenderMode( kRenderNone );
		UpdateVisibility();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Separate from CreateTFGibs so we can easily remove it if we don't like it...
//-----------------------------------------------------------------------------
void C_TFRagdoll::CreateWearableGibs( bool bDisguiseWearables )
{
	C_TFPlayer *pPlayer = NULL;
	EHANDLE hPlayer = GetPlayerHandle();
	if ( hPlayer )
	{
		pPlayer = dynamic_cast<C_TFPlayer*>( hPlayer.Get() );
	}
	if ( !pPlayer )
		return;

	Vector vecVelocity = m_vecForce + m_vecRagdollVelocity;
	VectorNormalize( vecVelocity );
	pPlayer->CreatePlayerGibs( m_vecRagdollOrigin, vecVelocity, m_vecForce.Length(), m_bBurning, true, false, bDisguiseWearables );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 
//-----------------------------------------------------------------------------
void C_TFRagdoll::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		bool bCreateRagdoll = true;

		// Get the player.
		EHANDLE hPlayer = GetPlayerHandle();
		if ( hPlayer )
		{
			// If we're getting the initial update for this player (e.g., after resetting entities after
			//  lots of packet loss, then don't create gibs, ragdolls if the player and it's gib/ragdoll
			//  both show up on same frame.
			if ( abs( hPlayer->GetCreationTick() - gpGlobals->tickcount ) < TIME_TO_TICKS( 1.0f ) )
			{
				bCreateRagdoll = false;
			}
		}
		else if ( C_BasePlayer::GetLocalPlayer() )
		{
			// Ditto for recreation of the local player
			if ( abs( C_BasePlayer::GetLocalPlayer()->GetCreationTick() - gpGlobals->tickcount ) < TIME_TO_TICKS( 1.0f ) )
			{
				bCreateRagdoll = false;
			}
		}

		// Prevent replays from creating ragdolls on the first frame of playback after skipping through playback.
		// If a player died (leaving a ragdoll) previous to the first frame of replay playback,
		// their ragdoll wasn't yet initialized because OnDataChanged events are queued but not processed
		// until the first render. 
		if ( engine->IsPlayingDemo() )
		{
			bCreateRagdoll = !m_bCreatedWhilePlaybackSkipping;
		}

		if ( GetDamageCustom() == TF_DMG_CUSTOM_TAUNTATK_GRAND_SLAM )
		{
			m_bBatted = true;
		}
		
		C_TFPlayer *pPlayer = ToTFPlayer( hPlayer.Get() );
		bool bMiniBoss = ( pPlayer && pPlayer->IsMiniBoss() ) ? true : false;

		if ( GetDamageCustom() == TF_DMG_CUSTOM_PLASMA )
		{
			if ( !m_bBecomeAsh && !bMiniBoss )
			{
				m_bDissolving = true;
			}

			m_bGib = false;
		}

		if ( GetDamageCustom() == TF_DMG_CUSTOM_PLASMA_CHARGED )
		{
			if ( !m_bBecomeAsh && !bMiniBoss )
			{
				m_bDissolving = true;
			}

			m_bGib = true;
			SetNextClientThink( CLIENT_THINK_ALWAYS );
		}

		// Don't gib zombies, always just ragdoll
		if ( pPlayer )
		{
			if ( pPlayer->BRenderAsZombie() )
			{
				m_bGib = false;
			}
			pPlayer->UpdateMVMEyeGlowEffect( false );
		}

		if ( bCreateRagdoll )
		{
			if ( m_bGib )
			{
				CreateTFGibs( !m_bDissolving );
			}
			else
			{
				CreateTFRagdoll();
				if ( IsDecapitation() )
				{
					CreateTFHeadGib();
					EmitSound( "TFPlayer.Decapitated" );

					bool bBlood = true;
					if ( TFGameRules() && ( TFGameRules()->UseSillyGibs() || 
											( TFGameRules()->IsMannVsMachineMode() && hPlayer && hPlayer->GetTeamNumber() == TF_TEAM_PVE_INVADERS ) ) )
					{
						bBlood = false;
					}

					if ( bBlood )
					{
						ParticleProp()->Create( "blood_decap", PATTACH_POINT_FOLLOW, "head" );
					}
				}
			}

			m_bNoModelParticles = true;

			// Drop wearables (hats, etc)
			CreateWearableGibs( m_bWasDisguised );			
		}
	}
	else 
	{
		if ( !cl_ragdoll_physics_enable.GetBool() )
		{
			// Don't let it set us back to a ragdoll with data from the server.
			m_nRenderFX = kRenderFxNone;
		}
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int C_TFRagdoll::InternalDrawModel( int flags )
{
	if ( m_MaterialOverride.IsValid() )
	{
		modelrender->ForcedMaterialOverride( m_MaterialOverride );
	}

	int ret = BaseClass::InternalDrawModel( flags );

	if ( m_MaterialOverride.IsValid() )
	{
		modelrender->ForcedMaterialOverride( NULL );
	}

	return ret;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool C_TFRagdoll::IsDecapitation()
{
	return (cl_ragdoll_fade_time.GetFloat() > 5.f) && 
		((m_iDamageCustom == TF_DMG_CUSTOM_DECAPITATION) 
		|| (m_iDamageCustom == TF_DMG_CUSTOM_TAUNTATK_BARBARIAN_SWING)
		|| (m_iDamageCustom == TF_DMG_CUSTOM_DECAPITATION_BOSS) 
		|| (m_iDamageCustom == TF_DMG_CUSTOM_HEADSHOT_DECAPITATION)
		|| (m_iDamageCustom == TF_DMG_CUSTOM_MERASMUS_DECAPITATION) );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool C_TFRagdoll::IsHeadSmash()
{
	return ((cl_ragdoll_fade_time.GetFloat() > 5.f) && (m_iDamageCustom == TF_DMG_CUSTOM_TAUNTATK_ENGINEER_GUITAR_SMASH));
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool C_TFRagdoll::GetAttachment( int iAttachment, matrix3x4_t &attachmentToWorld )
{
	int iHeadAttachment = LookupAttachment( "head" );
	if ( IsDecapitation() && (iAttachment == iHeadAttachment) )
	{
		MatrixCopy( m_mHeadAttachment, attachmentToWorld );
		return true;
	}
	else
	{
		return BaseClass::GetAttachment( iAttachment, attachmentToWorld );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : IRagdoll*
//-----------------------------------------------------------------------------
IRagdoll* C_TFRagdoll::GetIRagdoll() const
{
	return m_pRagdoll;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_TFRagdoll::IsRagdollVisible()
{
	Vector vMins = Vector(-1,-1,-1);	//WorldAlignMins();
	Vector vMaxs = Vector(1,1,1);	//WorldAlignMaxs();
		
	Vector origin = GetAbsOrigin();
	
	if( !engine->IsBoxInViewCluster( vMins + origin, vMaxs + origin) )
	{
		return false;
	}
	else if( engine->CullBox( vMins + origin, vMaxs + origin ) )
	{
		return false;
	}

	return true;
}
#define DISSOLVE_FADE_IN_START_TIME			0.0f
#define DISSOLVE_FADE_IN_END_TIME			1.0f
#define DISSOLVE_FADE_OUT_MODEL_START_TIME	1.9f
#define DISSOLVE_FADE_OUT_MODEL_END_TIME	2.0f
#define DISSOLVE_FADE_OUT_START_TIME		2.0f
#define DISSOLVE_FADE_OUT_END_TIME			2.0f

void C_TFRagdoll::ClientThink( void )
{
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	// Store off the un-shrunken head location for blood spurts.
	if ( IsDecapitation() ) 
	{
		int iAttach = LookupAttachment( "head" );

		m_bBaseTransform = true;
		BaseClass::GetAttachment( iAttach, m_mHeadAttachment );
		m_bBaseTransform = false;

		m_BoneAccessor.SetReadableBones( 0 );
		SetupBones( NULL, -1, BONE_USED_BY_ATTACHMENT, gpGlobals->curtime );
	}

	if ( m_bCloaked && m_flPercentInvisible < 1.f )
	{
		m_flPercentInvisible += gpGlobals->frametime;
		if ( m_flPercentInvisible > 1.f )
		{
			m_flPercentInvisible = 1.f;
		}
	}

	C_TFPlayer *pPlayer = ToTFPlayer( GetPlayerHandle() );
	bool bBombinomicon = ( pPlayer && pPlayer->HasBombinomiconEffectOnDeath() );

	if ( !m_bGib )
	{
		if ( m_bDissolving )
		{
			m_bDissolving = false;
			m_flTimeToDissolve = 1.2f;

			DissolveEntity( this );
			EmitSound( "TFPlayer.Dissolve" );

			// Dissolve all cosmetics as well
			for ( C_BaseEntity *pEntity = ClientEntityList().FirstBaseEntity(); pEntity; pEntity = ClientEntityList().NextBaseEntity(pEntity) )
			{
				if ( pEntity->GetFollowedEntity() == this )
				{
					CEconEntity *pItem = dynamic_cast< CEconEntity * >( pEntity );
					if ( pItem )
					{
						DissolveEntity( pItem );
					}
				}
			}
		}
		else if ( bBombinomicon && ( GetFlags() & FL_DISSOLVING ) )
		{
			m_flTimeToDissolve -= gpGlobals->frametime;
			if ( m_flTimeToDissolve <= 0 )
			{
				CreateTFGibs( true, true );
			}
		}
		else if ( m_bBecomeAsh )
		{
			m_flTimeToDissolve -= gpGlobals->frametime;
			if ( m_flTimeToDissolve <= 0 )
			{
				if ( bBombinomicon )
				{
					CreateTFGibs( true, true );
				}
				else
				{
					// Hide the ragdoll and stop everything but the ash particle effect
					AddEffects( EF_NODRAW );
					ParticleProp()->StopParticlesNamed( "drg_fiery_death", true, true );

					// Hide all cosmetics
					for ( C_BaseEntity *pEntity = ClientEntityList().FirstBaseEntity(); pEntity; pEntity = ClientEntityList().NextBaseEntity(pEntity) )
					{
						if ( pEntity->GetFollowedEntity() == this )
						{
							CEconEntity *pItem = dynamic_cast< CEconEntity * >( pEntity );
							if ( pItem )
							{
								pItem->AddEffects( EF_NODRAW );
							}
						}
					}
				}
				return;
			}
		}
		else if ( bBombinomicon )
		{
			m_flTimeToDissolve -= gpGlobals->frametime;
			if ( m_flTimeToDissolve <= 0 )
			{
				CreateTFGibs( true, true );
				return;
			}
		}
	}
	// Gibbing
	else
	{
		if ( m_bDissolving )
		{
			m_flTimeToDissolve -= gpGlobals->frametime;
			if ( m_flTimeToDissolve <= 0 )
			{
				m_bDissolving = false;

				if ( pPlayer )
				{
					if ( bBombinomicon )
					{
						CreateTFGibs( true, true );
					}
					else
					{
						for ( int i=0; i<pPlayer->m_hSpawnedGibs.Count(); i++ )
						{
							C_BaseEntity* pGib = pPlayer->m_hSpawnedGibs[i].Get();
							if ( pGib )
							{
								pGib->SetAbsVelocity( vec3_origin );
								DissolveEntity( pGib );
								pGib->ParticleProp()->StopParticlesInvolving( pGib );
							}
						}
					}
				}
				EndFadeOut();
			}
			return;
		}
	}

	// Fade us away...
	if ( m_bFadingOut == true )
	{
		int iAlpha = GetRenderColor().a;
		int iFadeSpeed = 600.0f;

		iAlpha = MAX( iAlpha - ( iFadeSpeed * gpGlobals->frametime ), 0 );

		SetRenderMode( kRenderTransAlpha );
		SetRenderColorA( iAlpha );

		if ( iAlpha == 0 )
		{
			// Remove clientside ragdoll.
			EndFadeOut();
		}
		return;
	}

	// If the player is looking at us, delay the fade.
	if ( IsRagdollVisible() )
	{
		if ( cl_ragdoll_forcefade.GetBool() )
		{
			m_bFadingOut = true;
			float flDelay = cl_ragdoll_fade_time.GetFloat() * 0.33f;
			m_fDeathTime = gpGlobals->curtime + flDelay;

			RemoveAllDecals();
		}

		// Fade out after the specified delay.
		StartFadeOut( cl_ragdoll_fade_time.GetFloat() * 0.33f );
		return;
	}

	// Remove us if our death time has passed.
	if ( m_fDeathTime < gpGlobals->curtime )
	{
		EndFadeOut();
		return;
	}

	// Fire an event if we were batted by the scout's taunt kill and we have come to rest.
	if ( m_bBatted )
	{
		Vector vVelocity;
		EstimateAbsVelocity( vVelocity );
		if ( vVelocity.LengthSqr() == 0.f )
		{
			m_bBatted = false;
			IGameEvent *event = gameeventmanager->CreateEvent( "scout_slamdoll_landed" );
			if ( event )
			{
				Vector absOrigin = GetAbsOrigin();
				event->SetInt( "target_index", m_iPlayerIndex );
				event->SetFloat( "x", absOrigin.x );
				event->SetFloat( "y", absOrigin.y );
				event->SetFloat( "z", absOrigin.z );
				gameeventmanager->FireEventClientSide( event );
			}
		}
	}
}

// Deal with recording
void C_TFRagdoll::GetToolRecordingState( KeyValues *msg )
{
#ifndef _XBOX
	BaseClass::GetToolRecordingState( msg );

	if ( m_MaterialOverride.IsValid() )
	{
		msg->SetString( "materialOverride", m_MaterialOverride->GetName() );
	}
#endif
}

void C_TFRagdoll::DissolveEntity( CBaseEntity* pEnt )
{
	C_EntityDissolve *pDissolve = DissolveEffect( pEnt, gpGlobals->curtime );
	if ( pDissolve )
	{
		pDissolve->SetRenderMode( kRenderTransColor );
		pDissolve->m_nRenderFX = kRenderFxNone;
		pDissolve->SetRenderColor( 255, 255, 255, 255 );

		Vector vColor;
		if ( m_iTeam == TF_TEAM_BLUE )
		{
			vColor = TF_PARTICLE_WEAPON_RED_1 * 255;
			pDissolve->SetEffectColor( vColor );
		}
		else
		{
			vColor = TF_PARTICLE_WEAPON_BLUE_1 * 255;
			pDissolve->SetEffectColor( vColor );
		}

		pDissolve->m_vDissolverOrigin = GetAbsOrigin();

		pDissolve->m_flFadeInStart = DISSOLVE_FADE_IN_START_TIME;
		pDissolve->m_flFadeInLength = DISSOLVE_FADE_IN_END_TIME - DISSOLVE_FADE_IN_START_TIME;

		pDissolve->m_flFadeOutModelStart = DISSOLVE_FADE_OUT_MODEL_START_TIME;
		pDissolve->m_flFadeOutModelLength = DISSOLVE_FADE_OUT_MODEL_END_TIME - DISSOLVE_FADE_OUT_MODEL_START_TIME;

		pDissolve->m_flFadeOutStart = DISSOLVE_FADE_OUT_START_TIME;
		pDissolve->m_flFadeOutLength = DISSOLVE_FADE_OUT_END_TIME - DISSOLVE_FADE_OUT_START_TIME;
	}
}

void C_TFRagdoll::StartFadeOut( float fDelay )
{
	if ( !cl_ragdoll_forcefade.GetBool() )
	{
		m_fDeathTime = gpGlobals->curtime + fDelay;
	}
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}


void C_TFRagdoll::EndFadeOut()
{
	SetNextClientThink( CLIENT_THINK_NEVER );
	ClearRagdoll();
	SetRenderMode( kRenderNone );
	UpdateVisibility();
	DestroyBoneAttachments();

	// Remove attached effect entity
	C_BaseEntity *pEffect = GetEffectEntity();
	if ( pEffect )
	{
		pEffect->SUB_Remove();
	}

	ParticleProp()->StopEmission();
	
	// Hide attached wearables.
	// These are server objects so they'll go away when the actual server ragdoll dies.
	for ( int i=0; i<m_hRagWearables.Count(); ++i )
	{
		if ( m_hRagWearables[i] )
		{
			m_hRagWearables[i]->AddEffects( EF_NODRAW );
			m_hRagWearables[i]->SetMoveType( MOVETYPE_NONE );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Used for spy invisiblity material
//-----------------------------------------------------------------------------
class CSpyInvisProxy : public CBaseInvisMaterialProxy
{
public:
	CSpyInvisProxy( void );
	virtual bool		Init( IMaterial *pMaterial, KeyValues* pKeyValues ) OVERRIDE;
	virtual void		OnBind( C_BaseEntity *pBaseEntity ) OVERRIDE;
	virtual void		OnBindNotEntity( void *pRenderable ) OVERRIDE;

private:
	IMaterialVar		*m_pCloakColorTint;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSpyInvisProxy::CSpyInvisProxy( void )
{
	m_pCloakColorTint = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Get pointer to the color value
// Input  : *pMaterial - 
//-----------------------------------------------------------------------------
bool CSpyInvisProxy::Init( IMaterial *pMaterial, KeyValues* pKeyValues )
{
	// Need to get the material var
	bool bInvis = CBaseInvisMaterialProxy::Init( pMaterial, pKeyValues );

	bool bTint;
	m_pCloakColorTint = pMaterial->FindVar( "$cloakColorTint", &bTint );

	return ( bInvis && bTint );
}

ConVar tf_teammate_max_invis( "tf_teammate_max_invis", "0.95", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :
//-----------------------------------------------------------------------------
void CSpyInvisProxy::OnBind( C_BaseEntity *pBaseEntity )
{
	if( !m_pPercentInvisible || !m_pCloakColorTint )
		return;

	float fInvis = 0.0f;

	C_TFPlayer *pPlayer = ToTFPlayer( pBaseEntity );

	if ( !pPlayer )
	{
		C_TFPlayer *pOwningPlayer = ToTFPlayer( pBaseEntity->GetOwnerEntity() );

		C_TFRagdoll *pRagdoll = dynamic_cast< C_TFRagdoll* >( pBaseEntity );
		if ( pRagdoll && pRagdoll->IsCloaked() )
		{
			fInvis = pRagdoll->GetPercentInvisible();
		}
		else if ( pOwningPlayer )
		{
			// mimic the owner's invisibility
			fInvis = pOwningPlayer->GetEffectiveInvisibilityLevel();
		}
	}
	else
	{
		float r = 1.0f, g = 1.0f, b = 1.0f;
		fInvis = pPlayer->GetEffectiveInvisibilityLevel();

		switch( pPlayer->GetTeamNumber() )
		{
		case TF_TEAM_RED:
			r = 1.0; g = 0.5; b = 0.4;
			break;

		case TF_TEAM_BLUE:
		default:
			r = 0.4; g = 0.5; b = 1.0;
			break;
		}

		m_pCloakColorTint->SetVecValue( r, g, b );
	}

	m_pPercentInvisible->SetFloatValue( fInvis );
}

void CSpyInvisProxy::OnBindNotEntity( void *pRenderable )
{
	CBaseInvisMaterialProxy::OnBindNotEntity( pRenderable );

	if ( m_pCloakColorTint )
	{
		m_pCloakColorTint->SetVecValue( 1.f, 1.f, 1.f );
	}
}

EXPOSE_INTERFACE( CSpyInvisProxy, IMaterialProxy, "spy_invis" IMATERIAL_PROXY_INTERFACE_VERSION );

//-----------------------------------------------------------------------------
// Purpose: Used for invulnerability material
//			Returns 1 if the player is invulnerable, and 0 if the player is losing / doesn't have invuln.
//-----------------------------------------------------------------------------
class CProxyInvulnLevel : public CResultProxy
{
public:
	void OnBind( void *pC_BaseEntity )
	{
		Assert( m_pResult );

		float flResult = 1.0;

		C_TFPlayer *pPlayer = NULL;
		C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
		if ( !pEntity )
		{
			if ( g_PlayerPreviewEffect.GetEffect() == C_TFPlayerPreviewEffect::PREVIEW_EFFECT_UBER )
			{
				flResult = 1.0f;
			}
			m_pResult->SetFloatValue( flResult );
			return;
		}

		if ( pEntity->IsPlayer()  )
		{
			pPlayer = dynamic_cast< C_TFPlayer* >( pEntity );
		}
		else
		{
			IHasOwner *pOwnerInterface = dynamic_cast< IHasOwner* >( pEntity );
			if ( pOwnerInterface )
			{
				pPlayer = ToTFPlayer( pOwnerInterface->GetOwnerViaInterface() );
			}
		}

		if ( pPlayer && pPlayer->m_Shared.IsInvulnerable() && pPlayer->m_Shared.InCond( TF_COND_INVULNERABLE_WEARINGOFF ) )
		{
			flResult = 0.0;
		}

		m_pResult->SetFloatValue( flResult );

		if ( ToolsEnabled() )
		{
			ToolFramework_RecordMaterialParams( GetMaterial() );
		}
	}
};

EXPOSE_INTERFACE( CProxyInvulnLevel, IMaterialProxy, "InvulnLevel" IMATERIAL_PROXY_INTERFACE_VERSION );

//-----------------------------------------------------------------------------
// Purpose: Used for burning material on player models
//			Returns 0.0->1.0 for level of burn to show on player skin
//-----------------------------------------------------------------------------
class CProxyBurnLevel : public CResultProxy
{
public:
	void OnBind( void *pC_BaseEntity )
	{
		Assert( m_pResult );

		float flResult = 0.0;

		C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
		if ( !pEntity )
		{
			if ( g_PlayerPreviewEffect.GetEffect() == C_TFPlayerPreviewEffect::PREVIEW_EFFECT_BURN )
			{
				flResult = 1.0f;
			}
			m_pResult->SetFloatValue( flResult );
			return;
		}

		// default to zero
		float flBurnStartTime = 0;
	
		if ( pEntity->IsPlayer() )
		{
			C_TFPlayer *pPlayer = assert_cast< C_TFPlayer* >( pEntity );
			// is the player burning?
			if (  pPlayer->m_Shared.InCond( TF_COND_BURNING ) )
			{
				flBurnStartTime = pPlayer->m_flBurnEffectStartTime;
			}
		}
		else
		{
			// is the ragdoll burning?
			C_TFRagdoll *pRagDoll = dynamic_cast< C_TFRagdoll* >( pEntity );
			if ( pRagDoll )
			{
				flBurnStartTime = pRagDoll->GetBurnStartTime();
			}
		}
		
		// if player/ragdoll is burning, set the burn level on the skin
		if ( flBurnStartTime > 0 )
		{
			float flBurnPeakTime = flBurnStartTime + 0.3;
			float flTempResult;
			if ( gpGlobals->curtime < flBurnPeakTime )
			{
				// fade in from 0->1 in 0.3 seconds
				flTempResult = RemapValClamped( gpGlobals->curtime, flBurnStartTime, flBurnPeakTime, 0.0, 1.0 );
			}
			else
			{
				// fade out from 1->0 in the remaining time until flame extinguished
				flTempResult = RemapValClamped( gpGlobals->curtime, flBurnPeakTime, flBurnStartTime + TF_BURNING_FLAME_LIFE, 1.0, 0.0 );
			}	

			// We have to do some more calc here instead of in materialvars.
			flResult = 1.0 - abs( flTempResult - 1.0 );
		}

		m_pResult->SetFloatValue( flResult );

		if ( ToolsEnabled() )
		{
			ToolFramework_RecordMaterialParams( GetMaterial() );
		}
	}
};

EXPOSE_INTERFACE( CProxyBurnLevel, IMaterialProxy, "BurnLevel" IMATERIAL_PROXY_INTERFACE_VERSION );


//-----------------------------------------------------------------------------
// Purpose: Used for turning player models yellow (jarate) 
//			Returns 0.0->1.0 for level of yellow to show on player skin
//-----------------------------------------------------------------------------
class CProxyUrineLevel : public CResultProxy
{
public:
	void OnBind( void *pC_BaseEntity )
	{
		Assert( m_pResult );

		// default to zero
		Vector vResult = Vector( 1, 1, 1 );

		C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
		if ( !pEntity )
		{
			if ( g_PlayerPreviewEffect.GetEffect() == C_TFPlayerPreviewEffect::PREVIEW_EFFECT_URINE )
			{
				if ( g_PlayerPreviewEffect.GetTeam() == TF_TEAM_RED )
				{
					vResult = Vector ( 6, 9, 2 );
				}
				else	
				{
					vResult = Vector ( 7, 5, 1 );
				}
			}
			m_pResult->SetVecValue( vResult.x, vResult.y, vResult.z );
			return;
		}

		C_TFPlayer *pPlayer = NULL;
		if ( pEntity->IsPlayer() )
		{
			pPlayer = assert_cast< C_TFPlayer* >( pEntity );
		}
		else if ( pEntity->GetOwnerEntity() && pEntity->GetOwnerEntity()->IsPlayer() )
		{
			pPlayer = assert_cast< C_TFPlayer* >( pEntity->GetOwnerEntity() );
		}

		if ( pPlayer )
		{
			// is the player peed on?
			if (  pPlayer->m_Shared.InCond( TF_COND_URINE ) )
			{
				int iVisibleTeam = pPlayer->GetTeamNumber();
				if ( pPlayer->m_Shared.InCond( TF_COND_DISGUISED ) )
				{
					if ( !pPlayer->IsLocalPlayer() && iVisibleTeam != GetLocalPlayerTeam() )
					{
						iVisibleTeam = pPlayer->m_Shared.GetDisguiseTeam();
					}
				}
				if ( iVisibleTeam == TF_TEAM_RED )
				{
					vResult = Vector ( 6, 9, 2 );
				}
				else	
				{
					vResult = Vector ( 7, 5, 1 );
				}
			}
			else
			{
				vResult = Vector( 1, 1, 1 );
			}
		}

		m_pResult->SetVecValue( vResult.x, vResult.y, vResult.z );

		if ( ToolsEnabled() )
		{
			ToolFramework_RecordMaterialParams( GetMaterial() );
		}
	}
};

EXPOSE_INTERFACE( CProxyUrineLevel, IMaterialProxy, "YellowLevel" IMATERIAL_PROXY_INTERFACE_VERSION );


//-----------------------------------------------------------------------------
// Purpose: CritBoosted FX
//			
//-----------------------------------------------------------------------------
class CProxyModelGlowColor : public CResultProxy
{
public:
	void OnBind( void *pC_BaseEntity )
	{
		Assert( m_pResult );

		C_TFPlayer *pPlayer = NULL;
		C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
		if ( !pEntity )
		{
			Vector vResult = Vector( 1, 1, 1 );
#if 0 // It looks like this code path is never used
			if ( g_PlayerPreviewEffect.GetEffect() == C_TFPlayerPreviewEffect::PREVIEW_EFFECT_CRIT )
			{
				if ( g_PlayerPreviewEffect.GetTeam() == TF_TEAM_RED )
				{
					vResult = Vector ( 80, 8, 5 );
				}
				else	
				{
					vResult = Vector ( 5, 20, 80 );
				}
			}
#endif // 0
			m_pResult->SetVecValue( vResult.x, vResult.y, vResult.z );
			return;
		}

		// default to [1 1 1]
		Vector vResult = Vector( 1, 1, 1 );
		int iVisibleTeam = 0;

		IHasOwner *pOwnerInterface = dynamic_cast< IHasOwner* >( pEntity );
		if ( pOwnerInterface )
		{
			pPlayer = ToTFPlayer( pOwnerInterface->GetOwnerViaInterface() );
		}

		if ( pPlayer )
		{
			iVisibleTeam = pPlayer->GetTeamNumber();
			if (  pPlayer->m_Shared.IsCritBoosted() )
			{
				// never show critboosted effect on a disguised spy (unless it's me)
				if ( !pPlayer->m_Shared.InCond( TF_COND_DISGUISED ) || pPlayer->IsLocalPlayer() )
				{
					if ( iVisibleTeam == TF_TEAM_RED )
					{
						vResult = Vector ( 80, 8, 5 );
					}
					else	
					{
						vResult = Vector ( 5, 20, 80 );
					}
				}
				pPlayer->m_Shared.m_bChargeGlowing = false;
			}
			else if ( pPlayer->m_Shared.IsHypeBuffed() )
			{
				vResult = Vector( 50, 2, 48 );
				pPlayer->m_Shared.m_bChargeGlowing = false;
			}
			else if ( pPlayer->m_Shared.InCond( TF_COND_OFFENSEBUFF ) || pPlayer->m_Shared.InCond( TF_COND_ENERGY_BUFF ) )
			{
				// Temporarily hijacking this proxy for buff FX.
				if ( iVisibleTeam == TF_TEAM_RED )
				{
					vResult = Vector ( 226, 150, 62 );
				}
				else
				{
					vResult = Vector( 29, 202, 135 );
				}
				pPlayer->m_Shared.m_bChargeGlowing = false;
			}
			else
			{
				if ( pPlayer->m_Shared.InCond( TF_COND_SHIELD_CHARGE ) || (pPlayer->m_Shared.GetNextMeleeCrit() != MELEE_NOCRIT) )
				{
					float flGlow;
					if ( pPlayer->m_Shared.InCond( TF_COND_SHIELD_CHARGE ) )
					{
						// Ramp up the charge glow while charging.
						flGlow = (100.f - pPlayer->m_Shared.GetDemomanChargeMeter()) / 100.f;
					}
					else
					{
						// Cool down the charge glow after charging.
						flGlow = 1.f - MIN((gpGlobals->curtime - pPlayer->m_Shared.m_flLastNoChargeTime - 1.5f) / 0.3f,1);
					}
					if ( iVisibleTeam == TF_TEAM_RED )
					{
						vResult = Vector( MAX(80*flGlow,1), MAX(8*flGlow,1), MAX(5*flGlow,1) );
					}
					else
					{
						vResult = Vector( MAX(5*flGlow,1), MAX(20*flGlow,1), MAX(80*flGlow,1) );
					}
					pPlayer->m_Shared.m_bChargeGlowing = true;
				}
				else if ( pPlayer->m_Shared.m_bChargeGlowing )
				{
					// Cool down the charge glow after charging.
					float flGlow = 1.f - MIN( (gpGlobals->curtime - pPlayer->m_Shared.m_flLastNoChargeTime) / 0.3f, 1.f );

					if ( flGlow <= 0 )
					{
						pPlayer->m_Shared.m_bChargeGlowing = false;
					}

					if ( iVisibleTeam == TF_TEAM_RED )
					{
						vResult = Vector( MAX(80*flGlow,1), MAX(8*flGlow,1), MAX(5*flGlow,1) );
					}
					else
					{
						vResult = Vector( MAX(5*flGlow,1), MAX(20*flGlow,1), MAX(80*flGlow,1) );
					}
				}
				else
				{
					vResult = Vector( 1, 1, 1 );
				}
			}

		}
		m_pResult->SetVecValue( vResult.x, vResult.y, vResult.z );

		if ( ToolsEnabled() )
		{
			ToolFramework_RecordMaterialParams( GetMaterial() );
		}
	}
};

EXPOSE_INTERFACE( CProxyModelGlowColor, IMaterialProxy, "ModelGlowColor" IMATERIAL_PROXY_INTERFACE_VERSION );

//-----------------------------------------------------------------------------
// Purpose: Proxy used to tell the material it's on a community weapon
//-----------------------------------------------------------------------------
class CProxyCommunityWeapon : public CResultProxy
{
public:
	void OnBind( void *pC_BaseEntity )
	{
		Assert( m_pResult );
		if ( pC_BaseEntity )
		{
			C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
			if ( pEntity )
			{
				CEconEntity *pItem = dynamic_cast< CEconEntity* >( pEntity );
				if ( pItem )
				{
					CEconItemView *pScriptItem = pItem->GetAttributeContainer()->GetItem();
					if ( pScriptItem && pScriptItem->GetStaticData() )
					{
						if ( pScriptItem->GetItemQuality() == AE_COMMUNITY )
						{
							m_pResult->SetIntValue( 1 );
							return;
						}
					}
				}
			}
		}

		m_pResult->SetIntValue( 0 );

		if ( ToolsEnabled() )
		{
			ToolFramework_RecordMaterialParams( GetMaterial() );
		}
	}
};

EXPOSE_INTERFACE( CProxyCommunityWeapon, IMaterialProxy, "CommunityWeapon" IMATERIAL_PROXY_INTERFACE_VERSION );


//-----------------------------------------------------------------------------
// Purpose: Used for scaling the beating heart texture to make it pulse
//-----------------------------------------------------------------------------
class CProxyHeartbeatScale : public CResultProxy
{
public:
	void OnBind( void *pC_BaseEntity )
	{
		Assert( m_pResult );

		const float pi = 3.141592f;
		const float twoPI = 2.0f * pi;

		float s1 = sin( gpGlobals->curtime * twoPI );
		s1 = clamp( s1, 0.0f, 1.0f );
		s1 *= s1;
		s1 *= s1;
		s1 = clamp( s1, 0.5f, 1.0f );
		s1 -= 0.5f;
		s1 *= 2.0f;

		float s2 = sin( ( gpGlobals->curtime + 0.25f ) * twoPI );
		s2 = clamp( s2, 0.0f, 1.0f );
		s2 *= s2;
		s2 *= s2;
		s2 = clamp( s2, 0.5f, 1.0f );
		s2 -= 0.5f;
		s2 *= 2.0f;

		float beat = MAX( s1, s2 );

		const float scale = 0.6f;
		const float loBeat = 1.0f * scale;
		const float hiBeat = 0.8f * scale;

		float scaledBeat = loBeat + ( hiBeat - loBeat ) * beat;

		m_pResult->SetFloatValue( scaledBeat );

		if ( ToolsEnabled() )
		{
			ToolFramework_RecordMaterialParams( GetMaterial() );
		}
	}
};

EXPOSE_INTERFACE( CProxyHeartbeatScale, IMaterialProxy, "HeartbeatScale" IMATERIAL_PROXY_INTERFACE_VERSION );


#ifndef NDEBUG
ConVar tf_benefactor_gift_count( "tf_benefactor_gift_count", "-1", FCVAR_CHEAT, "For testing" );
#endif

//-----------------------------------------------------------------------------
// Purpose: Used for scaling the beating heart based on the number of gifts player has given
//			Returns a texture scale factor where 1 is max size (big benefactor) and factor gets larger the fewer gifts you've given
//-----------------------------------------------------------------------------
class CProxyBenefactorLevel : public CResultProxy
{
public:
	void OnBind( void *pC_BaseEntity )
	{
		Assert( m_pResult );

		CEconItemView *pScriptItem = NULL;

		IClientRenderable *pRend = (IClientRenderable *)pC_BaseEntity;
		C_BaseEntity *pEntity = pRend->GetIClientUnknown()->GetBaseEntity();
		if ( pEntity )
		{
			CEconEntity *pItem = dynamic_cast< CEconEntity* >( pEntity );
			if ( pItem )
			{
				pScriptItem = pItem->GetAttributeContainer()->GetItem();
			}
		}

		if ( pScriptItem )
		{
			static CSchemaAttributeDefHandle pAttrDef_KillEater( "kill eater" );

			// Use the kill-eater prefix if the weapon has one.
			uint32 unKillCount;
			if ( !pScriptItem->FindAttribute( pAttrDef_KillEater, &unKillCount ) )
				return;

#ifndef NDEBUG
			int testCount = tf_benefactor_gift_count.GetInt();
			if ( testCount >= 0 )
			{
				unKillCount = (uint32)testCount;
			}
#endif

			if ( unKillCount == 0 )
			{
				// heartless
				m_pResult->SetFloatValue( 1000.0f );
			}
			else
			{
				const int maxBenefatorLevel = 250;

				float value = (float)unKillCount / (float)maxBenefatorLevel;

				value = clamp( value, 0.0f, 1.0f );

				// a linear scale doesn't show the size change until the very end - nonlinearize it
				value = sin( value * 1.57f );
				value = sin( value * 1.57f );

				// this seems backwards because this is a texture scaling factor
				// the bigger the number, the smaller the textured image is (tiles more in same space)
				const float minValue = 5.0f;
				const float maxValue = 1.0f;

				m_pResult->SetFloatValue( minValue + ( maxValue - minValue ) * value );
			}
		}

		if ( ToolsEnabled() )
		{
			ToolFramework_RecordMaterialParams( GetMaterial() );
		}
	}
};

EXPOSE_INTERFACE( CProxyBenefactorLevel, IMaterialProxy, "BenefactorLevel" IMATERIAL_PROXY_INTERFACE_VERSION );

//-----------------------------------------------------------------------------
// Purpose: Used for scaling the oscilloscope on the Building Rescue Gun
// Flattens the Wave when the player has no energy
//-----------------------------------------------------------------------------
class CProxyBuildingRescueLevel : public CResultProxy
{
public:
	void OnBind( void *pC_BaseEntity )
	{
		Assert( m_pResult );

		C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();
		if ( !pPlayer )
			return;

		int iIncreasedRangeCost = 0;
		CALL_ATTRIB_HOOK_INT_ON_OTHER( pPlayer, iIncreasedRangeCost, building_teleporting_pickup );		
		if ( iIncreasedRangeCost == 0 )
			return;

		CTFWeaponBase * pWeapon = pPlayer->GetActiveTFWeapon();
		if ( !pWeapon )
			return;

		int iAmmo = pPlayer->GetAmmoCount( TF_AMMO_METAL );

		float scale = 1.0f;
		if ( iAmmo < iIncreasedRangeCost )
		{
			scale = 10.0f;
		}
		else 
		{
			scale = ( 3.0f - ((float)(iAmmo - iIncreasedRangeCost) / (float)(pPlayer->GetMaxAmmo( TF_AMMO_METAL ) - iIncreasedRangeCost) * 3.0f ) + 1.0f );
		}

		VMatrix mat, temp;

		Vector2D center( 0.5, 0.5 );
		MatrixBuildTranslation( mat, -center.x, -center.y, 0.0f );

		// scale
		{
			MatrixBuildScale( temp, 1.0f, scale, 1.0f );
			MatrixMultiply( temp, mat, mat );
		}

		MatrixBuildTranslation( temp, center.x, center.y, 0.0f );
		MatrixMultiply( temp, mat, mat );

		m_pResult->SetMatrixValue( mat );

		if ( ToolsEnabled() )
		{
			ToolFramework_RecordMaterialParams( GetMaterial() );
		}
	}
};

EXPOSE_INTERFACE( CProxyBuildingRescueLevel, IMaterialProxy, "BuildingRescueLevel" IMATERIAL_PROXY_INTERFACE_VERSION );


//-----------------------------------------------------------------------------
// Used to pulse the Vaccinator's uber shield
//-----------------------------------------------------------------------------
class CProxyResistShield : public CResultProxy
{
public:
	void OnBind( void *pC_BaseEntity )
	{
		Assert( m_pResult );

		IClientRenderable *pRend = (IClientRenderable *)pC_BaseEntity;
		C_BaseEntity *pEntity = pRend->GetIClientUnknown()->GetBaseEntity();
		if ( pEntity )
		{
			C_LocalTempEntity* pTempEnt = dynamic_cast<C_LocalTempEntity*>(pEntity);
			if ( pTempEnt )
			{
				C_BaseEntity *pBaseEnt = cl_entitylist->GetEnt( pTempEnt->clientIndex );
				// This should be the owning player
				C_TFPlayer* pTFPlayer = ToTFPlayer( pBaseEnt );
				if( pTFPlayer )
				{
					float flTimeSince = gpGlobals->curtime - pTFPlayer->GetLastResistTime();
					float flOut = RemapValClamped( flTimeSince, 0, 0.4f, 7.f, -4.f );
					m_pResult->SetVecValue( flOut, flOut, flOut, 1.f );
					return;
				}
			}
		}

	
		m_pResult->SetVecValue( 1.0, 1.0, 1.0, 1.0 );	
	}
};

EXPOSE_INTERFACE( CProxyResistShield, IMaterialProxy, "ShieldFalloff" IMATERIAL_PROXY_INTERFACE_VERSION );

//-----------------------------------------------------------------------------
// Purpose: Used for pulsing the Wheatly Sappers eye glow when he talks
//-----------------------------------------------------------------------------
class CProxyWheatlyEyeGlow : public CResultProxy
{
public:
	void OnBind( void *pC_BaseEntity )
	{
		Assert( m_pResult );

		C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();
		if ( !pPlayer )
			return;

		const float flBase = 0.4f;
		const float flMinTalking = 0.6f;
		const float flMaxTalking = 1.3f;
		const float flAdjustUp = 0.01f;
		const float flAdjustDown = -0.006f;

		static float s_flCurr = 0.2f;
		static float s_flDir = 0.2f;
		
		const float flEyeHold = 1.9f;
		const float flMinEyes = 0.1f;
		const float flMaxEyes = 0.35f;
		const float flEyeAdjust = 0.005f;

		static float s_flEyePose = 0.0f;
		static float s_flNextEyeChange = 0;
		static float s_flEyeDir = 0.005f;

		C_TFWeaponSapper *pWeapon = dynamic_cast< C_TFWeaponSapper* >( pPlayer->Weapon_GetWeaponByType( TF_WPN_TYPE_BUILDING ) );
		if ( pWeapon )
		{
			if ( pWeapon->IsWheatleyTalking() )
			{
				float flNoise = RandomGaussianFloat( 0.0f, 0.01f );
				s_flCurr += s_flDir + flNoise;
				if ( s_flCurr > flMaxTalking && s_flDir > 0 )
				{
					s_flDir = flAdjustDown;
				}
				else if ( s_flCurr < flMinTalking && s_flDir < 0 )
				{
					s_flDir = flAdjustUp;
				}

				// Animate to eye's and hold for a few seconds
				float currTime = gpGlobals->curtime;
				if ( currTime > s_flNextEyeChange) 
				{
					s_flEyePose += s_flEyeDir;

					if ( s_flEyePose > flMaxEyes && s_flEyeDir > 0 )
					{
						s_flNextEyeChange = currTime + flEyeHold;
						s_flEyeDir = -flEyeAdjust;
					}
					if ( s_flEyePose < flMinEyes && s_flEyeDir < 0 )
					{
						s_flNextEyeChange = currTime + flEyeHold;
						s_flEyeDir = flEyeAdjust;
					}
				}
			}
			else
			{
				// adjust towards base
				s_flCurr += flAdjustDown;
				if ( s_flCurr < flBase )
				{
					s_flCurr = flBase;
					s_flDir = flAdjustUp;
				}

				if ( s_flEyePose > 0 )
				{
					s_flEyePose -= flEyeAdjust;
				}
				s_flEyeDir = flEyeAdjust;
			}

			CBaseViewModel *pViewModel = pPlayer->GetViewModel(0);
			if ( pViewModel )
			{
				pViewModel->SetPoseParameter( "eyelids", s_flEyePose );
			}
		}

		m_pResult->SetFloatValue( s_flCurr );

		if ( ToolsEnabled() )
		{
			ToolFramework_RecordMaterialParams( GetMaterial() );
		}
	}
};

EXPOSE_INTERFACE( CProxyWheatlyEyeGlow, IMaterialProxy, "WheatlyEyeGlow" IMATERIAL_PROXY_INTERFACE_VERSION );

//-----------------------------------------------------------------------------
CEconItemView *GetEconItemViewFromProxyEntity( void *pEntity )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	IClientRenderable *pRend = (IClientRenderable *)pEntity;
	CBaseEntity *pBaseEntity = pRend ? pRend->GetIClientUnknown()->GetBaseEntity() : NULL;
	
	// If an entity, find out what types it is and get the econ item view
	if ( pBaseEntity )
	{
		// Generic Entity that has an Item ( Weapons / Hats )
		IHasAttributes *pAttribInterface = GetAttribInterface( pBaseEntity );
		if ( pAttribInterface )
			return pAttribInterface->GetAttributeContainer()->GetItem();

		// ViewModel Attachment (aka view model Weapon)
		C_ViewmodelAttachmentModel *pViewModelAttachment = dynamic_cast<C_ViewmodelAttachmentModel*>( pBaseEntity );
		if ( pViewModelAttachment && pViewModelAttachment->GetOuter() )
		{
			return pViewModelAttachment->GetOuter()->GetAttributeContainer()->GetItem();
		}

		CTFViewModel *pViewModel = dynamic_cast<CTFViewModel*>( pBaseEntity );
		if ( pViewModel && pViewModel->GetWeapon() )
		{
			return pViewModel->GetWeapon()->GetAttributeContainer()->GetItem();
		}

		CTFDroppedWeapon *pDroppedWeapon = dynamic_cast<CTFDroppedWeapon*>( pBaseEntity );
		if ( pDroppedWeapon && pDroppedWeapon->GetItem() && pDroppedWeapon->GetItem()->GetItemDefIndex() != INVALID_ITEM_DEF_INDEX )
		{
			return pDroppedWeapon->GetItem();
		}
	}
	// No direct entity, might be a EconItem (PlayerModelPanels)
	else
	{
		CEconItemView *pItem = dynamic_cast< CEconItemView* >( pRend );
		if ( pItem )
		{
			return pItem;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
C_TFPlayer *GetOwnerFromProxyEntity( void *pEntity )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	IClientRenderable *pRend = ( IClientRenderable * ) pEntity;
	CBaseEntity *pBaseEntity = pRend ? pRend->GetIClientUnknown()->GetBaseEntity() : NULL;

	// If an entity, find out what types it is and get the econ item view
	if ( pBaseEntity )
	{
		CBaseEntity* pOwner = pBaseEntity->GetOwnerEntity();
		if ( pOwner )
			return dynamic_cast<C_TFPlayer*>( pOwner->GetOwnerEntity() );
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Used to animate the weapon sheen effect for kill streak attr items
//-----------------------------------------------------------------------------
class CProxyAnimatedWeaponSheen : public CBaseAnimatedTextureProxy
{
public:
	CProxyAnimatedWeaponSheen() {}
	virtual ~CProxyAnimatedWeaponSheen() {}

	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues )
	{
		bool foundVar = false;
		m_flNextStartTime = 0;
		m_flScaleX = -1;
		m_flScaleY = -1;
		m_flSheenOffsetX = 0;
		m_flSheenOffsetY = 0;
		m_iSheenDir = 0;

		m_pScaleXVar = NULL;
		m_pScaleYVar = NULL;
		m_pOffsetXVar = NULL;
		m_pOffsetYVar = NULL;
		m_pDirectionVar = NULL;
		m_pSheenIndexVar = NULL;

		// Get Tint
		m_pTintVar = pMaterial->FindVar( "$sheenmaptint", &foundVar );
		if( !foundVar )
			return false;

		m_pSheenIndexVar = pMaterial->FindVar( "$sheenindex", &foundVar );
		if( !foundVar )
			return false;

		// Material vars for scale and offset
		// Need to get the material var
		m_pScaleXVar = pMaterial->FindVar( "$sheenmapmaskscaleX", &foundVar );
		if( !foundVar )
			return false;

		m_pScaleYVar = pMaterial->FindVar( "$sheenmapmaskscaleY", &foundVar );
		if( !foundVar )
			return false;

		m_pOffsetXVar = pMaterial->FindVar( "$sheenmapmaskoffsetX", &foundVar );
		if( !foundVar )
			return false;

		m_pOffsetYVar = pMaterial->FindVar( "$sheenmapmaskoffsetY", &foundVar );
		if( !foundVar )
			return false;

		m_pDirectionVar = pMaterial->FindVar( "$sheenmapmaskdirection", &foundVar );
		if( !foundVar )
			return false;

		m_pSheenVar = pMaterial->FindVar( "$sheenmap", &foundVar );
		if ( !foundVar )
			return false;
		
		m_pSheenMaskVar = pMaterial->FindVar( "$sheenmapmask", &foundVar );
		if ( !foundVar )
			return false;

		return CBaseAnimatedTextureProxy::Init( pMaterial, pKeyValues );
	}

	void OnBind( void *pEntity )
	{
		Assert( m_AnimatedTextureVar );
		if ( m_AnimatedTextureVar->GetType() != MATERIAL_VAR_TYPE_TEXTURE )
			return;

		// if no entity, just check the owner
		if ( !pEntity )
		{
			// Might be gunslinger, hard to get this item so 
			return;
		}

		static CSchemaAttributeDefHandle pAttr_killstreak( "killstreak idleeffect" );	
		if ( !pAttr_killstreak )
			return;

		IClientRenderable *pRend = (IClientRenderable *)pEntity;
		// Find the weapon and player and see if it has the attribute
		if ( !pRend )
			return;

		ITexture *pTexture;
		pTexture = m_AnimatedTextureVar->GetTextureValue();
		int numFrames = pTexture->GetNumAnimationFrames();

		if ( numFrames <= 0 )
		{
			Assert( !"0 frames in material calling animated texture proxy" );
			return;
		}

		C_BaseEntity *pBaseEntity = pRend->GetIClientUnknown()->GetBaseEntity();
		const CEconItemView *pItem = dynamic_cast< CEconItemView* >( pRend );

		uint32 unAttrValue = 0;
		uint32 unEffectValue = 0;

		// !TEST!
#ifdef STAGING_ONLY
		if ( tf_killstreak_alwayson.GetBool() )
		{
			unEffectValue = 1;
		}
		if ( tf_sheen_all.GetFloat() != 0 ) 
		{
			unEffectValue = tf_sheen_all.GetInt();
		}
#endif // STAGING_ONLY

		// Find the tf player owner
		bool bIsFirstPerson = false;
		C_TFPlayer* pTFPlayer = NULL;
		if ( pItem )	// ItemModelPanels
		{
			if ( !pBaseEntity )
			{
				pItem->FindAttribute( pAttr_killstreak, &unAttrValue );
				unEffectValue = (int)((float&)unAttrValue);
			}
			else
			{
				CALL_ATTRIB_HOOK_INT_ON_OTHER( pBaseEntity, unAttrValue, killstreak_idleeffect );
			}
			
			if ( ( unEffectValue ) && pBaseEntity && pBaseEntity->GetOwnerEntity() )
			{
				pTFPlayer = ToTFPlayer( pBaseEntity->GetOwnerEntity() );
			}
			else
			{
				pTFPlayer = C_TFPlayer::GetLocalTFPlayer();
			}
		}
		else	
		{
			CTFWeaponBase* pWeapon = dynamic_cast<CTFWeaponBase*>( pBaseEntity );
			if ( !pWeapon )
			{
				for ( int i = 0; i < 1; ++i )
				{
					CEconWearable* pWearable = dynamic_cast<CEconWearable*>( pBaseEntity );
					if ( pWearable )
					{
						pItem = pWearable->GetAttributeContainer()->GetItem();
						pTFPlayer = ToTFPlayer( pWearable->GetOwnerEntity() );
						break;
					}
					C_ViewmodelAttachmentModel *pModel = dynamic_cast<C_ViewmodelAttachmentModel*>( pBaseEntity );
					if ( pModel )
					{
						if ( pModel->GetOuter() )
						{
							pItem = pModel->GetOuter()->GetAttributeContainer()->GetItem();
							pBaseEntity = pBaseEntity->GetOwnerEntity();
							if ( pItem )
							{
								pTFPlayer = ToTFPlayer( pModel->GetOuter()->GetOwnerEntity() );
							}
						}
						break;
					}

					// not a weapon, is a viewmodel
					IHasOwner *pHasOwner = dynamic_cast<IHasOwner*>( pBaseEntity );
					if ( pHasOwner )
					{
						// View model owner is player, so get the players active weapon
						CBaseEntity *pOwner = pHasOwner->GetOwnerViaInterface();
						pTFPlayer = ToTFPlayer( pOwner );
						if ( pTFPlayer )
						{
							pWeapon = pTFPlayer->GetActiveTFWeapon();
							if ( pWeapon )
							{
								pItem = pWeapon->GetAttributeContainer()->GetItem();
								pBaseEntity = pWeapon;
							}
							bIsFirstPerson = true;
						}
					}
					else if ( pBaseEntity && pBaseEntity->IsPlayer( ) )
					{
						pTFPlayer = ToTFPlayer( pBaseEntity );
						pWeapon = pTFPlayer->GetActiveTFWeapon();
						if ( pWeapon )
						{
							pItem = pWeapon->GetAttributeContainer()->GetItem();
							pBaseEntity = pWeapon;
						}
					}
				}	// for 
			}
			else
			{
				pItem = pWeapon->GetAttributeContainer()->GetItem();
				pBaseEntity = pWeapon;
				pTFPlayer = ToTFPlayer( pWeapon->GetOwner() );
			}

			// I have an econ item, does it have the attr
			if ( ( pBaseEntity && pItem ) || unEffectValue )
			{
				CALL_ATTRIB_HOOK_INT_ON_OTHER( pBaseEntity, unAttrValue, killstreak_idleeffect );

				if ( !unEffectValue )
				{
					unEffectValue = unAttrValue;
				}
				
				// Use the Spies target if disguised and we're on different teams
				if ( pTFPlayer && pTFPlayer->m_Shared.InCond( TF_COND_DISGUISED ) && pTFPlayer->GetTeamNumber() != GetLocalPlayerTeam() )
				{
					pTFPlayer = ToTFPlayer( pTFPlayer->m_Shared.GetDisguiseTarget() );
					if ( pTFPlayer && pTFPlayer->m_Shared.GetDisguiseWeapon() )
					{
						pItem = pTFPlayer->m_Shared.GetDisguiseWeapon()->GetAttributeContainer()->GetItem();
						pBaseEntity = pTFPlayer->m_Shared.GetDisguiseWeapon();
					}
				}
			}
		}

		float flNextStartTime = 0;
		if ( pTFPlayer )
		{
			// if player is taunting, make sure they can start animating
			if ( pTFPlayer->IsTaunting() && flNextStartTime > gpGlobals->curtime )
			{
				pTFPlayer->m_flNextSheenStartTime = gpGlobals->curtime;
			}
			flNextStartTime = pTFPlayer->m_flNextSheenStartTime;
		}
		else
		{
			flNextStartTime = m_flNextStartTime;
		}

		// !TEST!
#ifdef STAGING_ONLY
		if ( tf_killstreak_alwayson.GetBool() )
		{
			unEffectValue = 1;
		}

		if ( tf_sheen_all.GetFloat() != 0 ) 
		{
			unEffectValue = tf_sheen_all.GetInt();
		}
#endif // STAGING_ONLY

		// Not ready, so just exit
		if ( !pItem || !unEffectValue || flNextStartTime > gpGlobals->curtime || unEffectValue > 