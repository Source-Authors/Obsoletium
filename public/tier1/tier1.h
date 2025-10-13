//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A higher level link library for general use in the game and tools.
//
//===========================================================================//


#ifndef TIER1_H
#define TIER1_H

#include "appframework/IAppSystem.h"
#include "convar.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class ICvar;
class IProcessUtils;


//-----------------------------------------------------------------------------
// These tier1 libraries must be set by any users of this library.
// They can be set by calling ConnectTier1Libraries.
// It is hoped that setting this, and using this library will be the common mechanism for
// allowing link libraries to access tier1 library interfaces
//-----------------------------------------------------------------------------

// These are marked DLL_EXPORT for Linux.
DLL_EXPORT ICvar *cvar;
extern ICvar *g_pCVar;
extern IProcessUtils *g_pProcessUtils;


//-----------------------------------------------------------------------------
// Call this to connect to/disconnect from all tier 1 libraries.
// It's up to the caller to check the globals it cares about to see if ones are missing
//-----------------------------------------------------------------------------
void ConnectTier1Libraries( CreateInterfaceFn *pFactoryList, int nFactoryCount );
void DisconnectTier1Libraries();


//-----------------------------------------------------------------------------
// Helper empty implementation of an IAppSystem for tier2 libraries
//-----------------------------------------------------------------------------
template< class IInterface, int ConVarFlag = 0 > 
class CTier1AppSystem : public CTier0AppSystem< IInterface >
{
	using BaseClass = CTier0AppSystem<IInterface>;

public:
	CTier1AppSystem( bool bIsPrimaryAppSystem = true ) : BaseClass(	bIsPrimaryAppSystem )
	{
	}

	bool Connect( CreateInterfaceFn factory ) override
	{
		if ( !BaseClass::Connect( factory ) )
			return false;

		if ( BaseClass::IsPrimaryAppSystem() )
		{
			ConnectTier1Libraries( &factory, 1 );
		}
		return true;
	}

	void Disconnect() override
	{
		if ( BaseClass::IsPrimaryAppSystem() )
		{
			DisconnectTier1Libraries();
		}
		BaseClass::Disconnect();
	}

	InitReturnVal_t Init() override
	{
		InitReturnVal_t nRetVal = BaseClass::Init();
		if ( nRetVal != INIT_OK )
			return nRetVal;

		if ( g_pCVar && BaseClass::IsPrimaryAppSystem() )
		{
			ConVar_Register( ConVarFlag );
		}
		return INIT_OK;
	}

	void Shutdown() override
	{
		if ( g_pCVar && BaseClass::IsPrimaryAppSystem() )
		{
			ConVar_Unregister( );
		}
		BaseClass::Shutdown( );
	}
};


#endif // TIER1_H

