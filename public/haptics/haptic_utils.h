#ifndef HAPTIC_UTILS_H
#define HAPTIC_UTILS_H


#ifdef CLIENT_DLL

#include "haptics/ihaptics.h"

// forward decl.
class C_BaseEntity;
class C_BaseCombatCharacter;
class C_BasePlayer;
class bf_read;

// use client side versions.
#ifndef CBasePlayer
#define CBasePlayer C_BasePlayer;
#endif
#ifndef CBaseCombatWeapon
#define CBaseCombatWeapon C_BaseCombatWeapon
#endif

// stubbed version of haptics interface. Used when haptics is not available.
class CHapticsStubbed : public IHaptics
{
public:
public: // Initialization.
	virtual bool Initialize(IVEngineClient*, 
		IViewRender *, 
		vgui::IInputInternal*, 
		CGlobalVarsBase*,
		CreateInterfaceFn, 
		void *,
		IFileSystem*, 
		IEngineVGui*,
		ActivityList_IndexForName_t,
		ActivityList_NameForIndex_t)
		{return false;};

public: // Device methods
	virtual bool HasDevice(){return false;};
	virtual void ShutdownHaptics(){};

public: // Game input handling
	virtual void CalculateMove(float &, float &, float ){};
	virtual void OnPlayerChanged(){}
	virtual void SetNavigationClass(const char *){};
	virtual const char *GetNavigationClass(){ return 0; };
	virtual void GameProcess(){}
	virtual void MenuProcess(){}

public: // Effect methods
	virtual void ProcessHapticEvent(int, ...){}
	virtual void ProcessHapticWeaponActivity(const char *, int ){}
	virtual void HapticsPunch(float, const QAngle &){}
	virtual void ApplyDamageEffect(float, int, const Vector &){}
	virtual void UpdateAvatarVelocity(const Vector &vel){}
	virtual void RemoveAvatarEffect(){}
	virtual void SetConstantForce(const Vector &){}
	virtual Vector GetConstantForce(){return Vector(0,0,0);}
	virtual void SetDrag(float){}
	virtual void SetShake(float, float){}
	virtual void SetHeld(float){}
	virtual void SetMoveSurface(HapticSurfaceType_t){}
	virtual HapticSurfaceType_t GetMoveSurface(){ return HST_NONE; }
	virtual void SetDangling(float){};

public: // Notify methods
	virtual void LocalPlayerReset(){};
	virtual void UpdatePlayerFOV(float){};
	virtual void WorldPrecache() {};
};
#else
// forward decl.
class CBasePlayer;
class CBaseCombatWeapon;
class CTakeDamageInfo;

#endif // CLIENT_DLL

void HapticSendWeaponAnim(class CBaseCombatWeapon* weapon, int iActivity);
void HapticSetConstantForce(class CBasePlayer* pPlayer,Vector force);
void HapticSetDrag(class CBasePlayer* pPlayer, float drag);

// note: does nothing on server.
void HapticProcessSound(const char* soundname, int entIndex);

#ifdef CLIENT_DLL
	void ConnectHaptics(CreateInterfaceFn appFactory);
	void DisconnectHaptics();

	void UpdateAvatarEffect(void);
	void HapticsExitedVehicle(C_BaseEntity* vehicle, C_BaseCombatCharacter *pPassenger );
	void HapticsEnteredVehicle(C_BaseEntity* vehicle, C_BaseCombatCharacter *pPassenger );
	
	//bool value true if user is using a haptic device.
	extern ConVar hap_HasDevice;
#else
	void HapticsDamage(CBasePlayer* pPlayer, const CTakeDamageInfo &info);
	void HapticPunch(CBasePlayer* pPlayer, float amount, float x, float y);
	void HapticMeleeContact(CBasePlayer* pPlayer);
#endif

#endif
