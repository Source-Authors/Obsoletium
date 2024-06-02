//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// The application object for apps that use tier2
//=============================================================================

#ifndef TIER2APP_H
#define TIER2APP_H


#include "appframework/IAppSystemGroup.h"
#include "appframework/IAppSystem.h"
#include "tier2/tier2.h"
#include "tier2/tier2dm.h"
#include "tier1/tier1.h"
#include "tier1/interface.h"
#include "tier1/convar.h"


//-----------------------------------------------------------------------------
// The application object for apps that use tier2
//-----------------------------------------------------------------------------
class CTier2SteamApp : public CSteamAppSystemGroup
{
	using BaseClass = CSteamAppSystemGroup;

public:
	// Methods of IApplication
	bool PreInit() override
	{
		CreateInterfaceFn factory = GetFactory();
		ConnectTier1Libraries( &factory, 1 );
		ConVar_Register( 0 );
		ConnectTier2Libraries( &factory, 1 );
		return true;			
	}

	void PostShutdown() override
	{
		DisconnectTier2Libraries();
		ConVar_Unregister();
		DisconnectTier1Libraries();
	}
};


//-----------------------------------------------------------------------------
// The application object for apps that use tier2 and datamodel
//-----------------------------------------------------------------------------
class CTier2DmSteamApp : public CTier2SteamApp
{
	using BaseClass = CTier2SteamApp;

public:
	// Methods of IApplication
	bool PreInit() override
	{
		if ( !BaseClass::PreInit() )
			return false;

		CreateInterfaceFn factory = GetFactory();
		if ( !ConnectDataModel( factory ) )
			return false;

		InitReturnVal_t nRetVal = InitDataModel();
		return ( nRetVal == INIT_OK );
	}

	void PostShutdown() override
	{
		ShutdownDataModel();
		DisconnectDataModel();
		BaseClass::PostShutdown();
	}
};


#endif // TIER2APP_H
