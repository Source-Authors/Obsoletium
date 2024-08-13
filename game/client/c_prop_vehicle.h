//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_PROP_VEHICLE_H
#define C_PROP_VEHICLE_H
#pragma once

#include "iclientvehicle.h"
#include "vehicle_viewblend_shared.h"
class C_PropVehicleDriveable : public C_BaseAnimating, public IClientVehicle
{

	DECLARE_CLASS( C_PropVehicleDriveable, C_BaseAnimating );

public:

	DECLARE_CLIENTCLASS();
	DECLARE_INTERPOLATION();
	DECLARE_DATADESC_OVERRIDE();

	C_PropVehicleDriveable();
	~C_PropVehicleDriveable();

// IVehicle overrides.
public:

	C_BaseCombatCharacter* GetPassenger( int nRole ) override;
	int	GetPassengerRole( C_BaseCombatCharacter *pEnt ) override;
	bool IsPassengerUsingStandardWeapons( int nRole = VEHICLE_ROLE_DRIVER ) override { return false; }
	void GetVehicleViewPosition( int nRole, Vector *pOrigin, QAngle *pAngles, float *pFOV = NULL ) override;

	void SetupMove( C_BasePlayer *, CUserCmd *, IMoveHelper *, CMoveData * ) override {}
	void ProcessMovement( C_BasePlayer *, CMoveData * ) override {}
	void FinishMove( C_BasePlayer *, CUserCmd *, CMoveData * ) override {}

	void ItemPostFrame( C_BasePlayer * ) override {}

// IClientVehicle overrides.
public:

	void GetVehicleFOV( float &flFOV ) override { flFOV = m_flFOV; }
	void DrawHudElements() override;
	void UpdateViewAngles( C_BasePlayer *pLocalPlayer, CUserCmd *pCmd ) override;
	virtual void DampenEyePosition( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles );
	void GetVehicleClipPlanes( float &flZNear, float &flZFar ) const override;

#ifdef HL2_CLIENT_DLL
	int GetPrimaryAmmoType() const override { return -1; }
	int GetPrimaryAmmoCount() const override { return -1; }
	int GetPrimaryAmmoClip() const override  { return -1; }
	bool PrimaryAmmoUsesClips() const override { return false; }
#endif

	bool IsPredicted() const override { return false; }
	int GetJoystickResponseCurve() const override;

// C_BaseEntity overrides.
public:

	IClientVehicle*	GetClientVehicle() override { return this; }
	C_BaseEntity	*GetVehicleEnt() override { return this; }
	bool IsSelfAnimating() override { return false; };

	void OnPreDataChanged( DataUpdateType_t updateType ) override;
	void OnDataChanged( DataUpdateType_t updateType ) override;

	// Should this object cast render-to-texture shadows?
	ShadowType_t ShadowCastType() override;

	// Mark the shadow as dirty while the vehicle is being driven
	void ClientThink( void ) override;

// C_PropVehicleDriveable
public:

	bool	IsRunningEnterExitAnim( void ) { return m_bEnterAnimOn || m_bExitAnimOn; }
	// NVNT added to check if the vehicle needs to aim
	virtual bool HasGun(void){return m_bHasGun;}

protected:

	virtual void OnEnteredVehicle( C_BaseCombatCharacter *pPassenger );
	// NVNT added to notify haptics system of vehicle exit.
	virtual void OnExitedVehicle( C_BaseCombatCharacter *pPassenger );

	virtual void RestrictView( float *pYawBounds, float *pPitchBounds, float *pRollBounds, QAngle &vecViewAngles );
	virtual void SetVehicleFOV( float flFOV ) { m_flFOV = flFOV; }

protected:

	CHandle<C_BasePlayer>		m_hPlayer;
	int							m_nSpeed;
	int							m_nRPM;
	float						m_flThrottle;
	int							m_nBoostTimeLeft;
	int							m_nHasBoost;
	int							m_nScannerDisabledWeapons;
	int							m_nScannerDisabledVehicle;

	// timers/flags for flashing icons on hud
	int							m_iFlashTimer;
	bool						m_bLockedDim;
	bool						m_bLockedIcon;

	int							m_iScannerWepFlashTimer;
	bool						m_bScannerWepDim;
	bool						m_bScannerWepIcon;

	int							m_iScannerVehicleFlashTimer;
	bool						m_bScannerVehicleDim;
	bool						m_bScannerVehicleIcon;

	float						m_flSequenceChangeTime;
	bool						m_bEnterAnimOn;
	bool						m_bExitAnimOn;
	float						m_flFOV;

	Vector						m_vecGunCrosshair;
	CInterpolatedVar<Vector>	m_iv_vecGunCrosshair;
	Vector						m_vecEyeExitEndpoint;
	bool						m_bHasGun;
	bool						m_bUnableToFire;

	// Used to smooth view entry
	CHandle<C_BasePlayer>		m_hPrevPlayer;

	ViewSmoothingData_t			m_ViewSmoothingData;
};


#endif // C_PROP_VEHICLE_H
