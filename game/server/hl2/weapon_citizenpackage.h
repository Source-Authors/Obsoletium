//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#ifndef WEAPON_CITIZENPACKAGE_H
#define WEAPON_CITIZENPACKAGE_H
#ifdef _WIN32
#pragma once
#endif

#include "basehlcombatweapon.h"

//=============================================================================
//
// Weapon - Citizen Package Class
//
class CWeaponCitizenPackage : public CBaseHLCombatWeapon
{
	DECLARE_CLASS( CWeaponCitizenPackage, CBaseHLCombatWeapon );
public:

	DECLARE_SERVERCLASS_OVERRIDE();
	DECLARE_DATADESC_OVERRIDE();	
	DECLARE_ACTTABLE();

	void ItemPostFrame( void ) override;
	void Drop( const Vector &vecVelocity ) override;
};

#endif // WEAPON_CITIZENPACKAGE_H