//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef WEAPON_ALYXGUN_H
#define WEAPON_ALYXGUN_H

#include "basehlcombatweapon.h"

#if defined( _WIN32 )
#pragma once
#endif

class CWeaponAlyxGun : public CHLSelectFireMachineGun
{
	DECLARE_DATADESC_OVERRIDE();
public:
	DECLARE_CLASS( CWeaponAlyxGun, CHLSelectFireMachineGun );

	CWeaponAlyxGun();
	~CWeaponAlyxGun();

	DECLARE_SERVERCLASS_OVERRIDE();
	
	void	Precache( void ) override;

	int		GetMinBurst( void ) override { return 4; }
	int		GetMaxBurst( void ) override { return 7; }
	float	GetMinRestTime( void ) override;
	float	GetMaxRestTime( void ) override;

	void Equip(CBaseCombatCharacter *pOwner) override;

	float	GetFireRate( void ) override { return 0.1f; }
	int		CapabilitiesGet( void ) override { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	int		WeaponRangeAttack1Condition( float flDot, float flDist ) override;
	int		WeaponRangeAttack2Condition( float flDot, float flDist ) override;

	const Vector& GetBulletSpread( void ) override;

	void FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );

	void Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary ) override;
	void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator ) override;

	void SetPickupTouch( void ) override
	{
		// Alyx gun cannot be picked up
		SetTouch(NULL);
	}

	float m_flTooCloseTimer;

	DECLARE_ACTTABLE();

};

#endif // WEAPON_ALYXGUN_H
