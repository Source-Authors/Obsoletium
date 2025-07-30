//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "c_weapon__stubs.h"
#include "basehlcombatweapon_shared.h"
#include "c_basehlcombatweapon.h"

// dimhotepus: For old animation events.
#include "cl_animevent.h"

// dimhotepus: For new animation events.
#include "eventlist.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

STUB_WEAPON_CLASS( cycler_weapon, WeaponCycler, C_BaseCombatWeapon );

STUB_WEAPON_CLASS( weapon_binoculars, WeaponBinoculars, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_bugbait, WeaponBugBait, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_flaregun, Flaregun, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_annabelle, WeaponAnnabelle, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_gauss, WeaponGaussGun, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_cubemap, WeaponCubemap, C_BaseCombatWeapon );
STUB_WEAPON_CLASS( weapon_alyxgun, WeaponAlyxGun, C_HLSelectFireMachineGun );
STUB_WEAPON_CLASS( weapon_citizenpackage, WeaponCitizenPackage, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_citizensuitcase, WeaponCitizenSuitcase, C_WeaponCitizenPackage );

#ifndef HL2MP
STUB_WEAPON_CLASS( weapon_ar2, WeaponAR2, C_HLMachineGun );
STUB_WEAPON_CLASS( weapon_frag, WeaponFrag, C_BaseHLCombatWeapon );

STUB_WEAPON_CLASS_BEGIN( weapon_rpg, WeaponRPG, C_BaseHLCombatWeapon )
	bool OnFireEventEx( [[maybe_unused]] C_BaseViewModel *pViewModel,
		[[maybe_unused]] const Vector& origin,
		[[maybe_unused]] const QAngle& angles,
		int event,
		const char *&options ) override
	{
		static const char muzzleFlashRpg[2] = {static_cast<char>('0' + to_underlying(MUZZLEFLASH_RPG)), '\0'};
		
		// dimhotepus: RPG in Half-Life 2 uses obsolete muzzle flash events.
		// dimhotepus: and does not set options, but options used later to get weapon type.
		// dimhotepus: We ensure options set to weapon type for muzzle flashes to work.
		switch (event)
		{
			// OBSOLETE EVENTS. REPLACED BY NEWER SYSTEMS.
			case CL_EVENT_MUZZLEFLASH0:
			case CL_EVENT_MUZZLEFLASH1:
			case CL_EVENT_MUZZLEFLASH2:
			case CL_EVENT_MUZZLEFLASH3:
			case CL_EVENT_NPC_MUZZLEFLASH0:
			case CL_EVENT_NPC_MUZZLEFLASH1:
			case CL_EVENT_NPC_MUZZLEFLASH2:
			case CL_EVENT_NPC_MUZZLEFLASH3:
				if (Q_isempty(options)) 
				{
					options = muzzleFlashRpg;
				}
		}

		// dimhotepus: False to continue processing.
		return false;
	}
STUB_WEAPON_CLASS_END( weapon_rpg, WeaponRPG, C_BaseHLCombatWeapon );

STUB_WEAPON_CLASS_BEGIN(weapon_pistol, WeaponPistol, C_BaseHLCombatWeapon)
	bool OnFireEventEx( [[maybe_unused]] C_BaseViewModel *pViewModel,
		[[maybe_unused]] const Vector& origin,
		[[maybe_unused]] const QAngle& angles,
		int event,
		const char *&options ) override
	{
		switch (event)
		{
			case AE_MUZZLEFLASH:
			case AE_NPC_MUZZLEFLASH:
			{
				// dimhotepus: Default pistol missed brass ejection.
				// dimhotepus: Eject brass for player and NPC on muzzle flash.
				// "0" for default pistol shells.
				FireEvent( origin, angles, CL_EVENT_EJECTBRASS1, "0" );
				break;
			}
		}

		// dimhotepus: False to continue processing.
		return false;
	}
STUB_WEAPON_CLASS_END(weapon_pistol, WeaponPistol, C_BaseHLCombatWeapon);

STUB_WEAPON_CLASS( weapon_357, Weapon357, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_crossbow, WeaponCrossbow, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_slam, Weapon_SLAM, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_crowbar, WeaponCrowbar, C_BaseHLBludgeonWeapon );
#ifdef HL2_EPISODIC
STUB_WEAPON_CLASS( weapon_hopwire, WeaponHopwire, C_BaseHLCombatWeapon );
//STUB_WEAPON_CLASS( weapon_proto1, WeaponProto1, C_BaseHLCombatWeapon );
#endif
#ifdef HL2_LOSTCOAST
STUB_WEAPON_CLASS( weapon_oldmanharpoon, WeaponOldManHarpoon, C_WeaponCitizenPackage );
#endif
#endif


