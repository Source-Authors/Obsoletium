//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef WEAPON_CROWBAR_H
#define WEAPON_CROWBAR_H

#include "basebludgeonweapon.h"

#if defined( _WIN32 )
#pragma once
#endif

#ifdef HL2MP
#error "weapon_crowbar.h must not be included in hl2mp. The windows compiler will use the wrong class elsewhere if it is"
#endif

#define	CROWBAR_RANGE	75.0f
#define	CROWBAR_REFIRE	0.4f

//-----------------------------------------------------------------------------
// CWeaponCrowbar
//-----------------------------------------------------------------------------

class CWeaponCrowbar : public CBaseHLBludgeonWeapon
{
public:
	DECLARE_CLASS( CWeaponCrowbar, CBaseHLBludgeonWeapon );

	DECLARE_SERVERCLASS_OVERRIDE();
	DECLARE_ACTTABLE();

	CWeaponCrowbar();

	float		GetRange( void ) override		{	return	CROWBAR_RANGE;	}
	float		GetFireRate( void ) override		{	return	CROWBAR_REFIRE;	}

	void		AddViewKick( void ) override;
	float		GetDamageForActivity( Activity hitActivity ) override;

	int			WeaponMeleeAttack1Condition( float flDot, float flDist ) override;
	void		SecondaryAttack( void ) override { }

	// Animation event
	void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator ) override;

private:
	// Animation event handlers
	void HandleAnimEventMeleeHit( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
};

#endif // WEAPON_CROWBAR_H
