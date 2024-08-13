//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Projectile shot from the AR2 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef	WEAPONAR2_H
#define	WEAPONAR2_H

#include "basegrenade_shared.h"
#include "basehlcombatweapon.h"

class CWeaponAR2 : public CHLMachineGun
{
public:
	DECLARE_CLASS( CWeaponAR2, CHLMachineGun );

	CWeaponAR2();

	DECLARE_SERVERCLASS_OVERRIDE();

	void	ItemPostFrame( void ) override;
	void	Precache( void ) override;
	
	void	SecondaryAttack( void ) override;
	void	DelayedAttack( void );

	const char *GetTracerType( void ) override { return "AR2Tracer"; }

	void	AddViewKick( void ) override;

	void	FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
	void	FireNPCSecondaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
	void	Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary ) override;
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator ) override;

	int		GetMinBurst( void ) override { return 2; }
	int		GetMaxBurst( void ) override { return 5; }
	float	GetFireRate( void ) override { return 0.1f; }

	bool	CanHolster( void ) const override;
	bool	Reload( void ) override;

	int		CapabilitiesGet( void ) override { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	Activity	GetPrimaryAttackActivity( void ) override;
	
	void	DoImpactEffect( trace_t &tr, int nDamageType ) override;

	const Vector& GetBulletSpread( void ) override
	{
		static Vector cone;
		
		cone = VECTOR_CONE_3DEGREES;

		return cone;
	}

	const WeaponProficiencyInfo_t *GetProficiencyValues() override;

protected:

	float					m_flDelayedFire;
	bool					m_bShotDelayed;
	int						m_nVentPose;
	
	DECLARE_ACTTABLE();
	DECLARE_DATADESC_OVERRIDE();
};


#endif	//WEAPONAR2_H
