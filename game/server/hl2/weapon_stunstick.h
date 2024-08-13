//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_STUNSTICK_H
#define WEAPON_STUNSTICK_H
#ifdef _WIN32
#pragma once
#endif

#include "basebludgeonweapon.h"

#define	STUNSTICK_RANGE		75.0f
#define	STUNSTICK_REFIRE	0.6f

class CWeaponStunStick : public CBaseHLBludgeonWeapon
{
	DECLARE_CLASS( CWeaponStunStick, CBaseHLBludgeonWeapon );
	DECLARE_DATADESC_OVERRIDE();

public:

	CWeaponStunStick();

	DECLARE_SERVERCLASS_OVERRIDE();
	DECLARE_ACTTABLE();

	void		Precache() override;

	void		Spawn() override;

	float		GetRange( void ) override		{ return STUNSTICK_RANGE; }
	float		GetFireRate( void ) override		{ return STUNSTICK_REFIRE; }

	int			WeaponMeleeAttack1Condition( float flDot, float flDist ) override;

	bool		Deploy( void ) override;
	bool		Holster( CBaseCombatWeapon *pSwitchingTo = NULL ) override;
	
	void		Drop( const Vector &vecVelocity ) override;
	void		ImpactEffect( trace_t &traceHit ) override;
	void		SecondaryAttack( void ) override {}
	void		SetStunState( bool state );
	bool		GetStunState( void );
	void		Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator ) override
		;
	
	float		GetDamageForActivity( Activity hitActivity ) override;

	bool		CanBePickedUpByNPCs( void ) override { return false;	}

private:

	CNetworkVar( bool, m_bActive );
};

#endif // WEAPON_STUNSTICK_H
