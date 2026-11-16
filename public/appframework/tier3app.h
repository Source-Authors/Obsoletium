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
// The application objects for apps that use tier3
//=============================================================================

#ifndef TIER3APP_H
#define TIER3APP_H


#include "appframework/tier2app.h"
#include "tier1/interface.h"
#include "tier3/tier3.h"
#include "vgui_controls/Controls.h"


//-----------------------------------------------------------------------------
// The application object for apps that use tier3
//-----------------------------------------------------------------------------
class CTier3SteamApp : public CTier2SteamApp
{
	using BaseClass = CTier2SteamApp;

public:
	// Methods of IApplication
	bool PreInit() override
	{
		if ( !BaseClass::PreInit() )
			return false;

		CreateInterfaceFn factory = GetFactory();
		ConnectTier3Libraries( &factory, 1 );
		return true;			
	}

	void PostShutdown() override
	{
		DisconnectTier3Libraries();
		BaseClass::PostShutdown();
	}
};


//-----------------------------------------------------------------------------
// The application object for apps that use tier3 and datamodel
//-----------------------------------------------------------------------------
class CTier3DmSteamApp : public CTier2DmSteamApp
{
	using BaseClass = CTier2DmSteamApp;

public:
	// Methods of IApplication
	bool PreInit() override
	{
		if ( !BaseClass::PreInit() )
			return false;

		CreateInterfaceFn factory = GetFactory();
		ConnectTier3Libraries( &factory, 1 );
		return true;			
	}

	void PostShutdown() override
	{
		DisconnectTier3Libraries();
		BaseClass::PostShutdown();
	}
};


//-----------------------------------------------------------------------------
// The application object for apps that use vgui
//-----------------------------------------------------------------------------
class CVguiSteamApp : public CTier3SteamApp
{
	using BaseClass = CTier3SteamApp;

public:
	// Methods of IApplication
	bool PreInit() override
	{
		if ( !BaseClass::PreInit() )
			return false;

		CreateInterfaceFn factory = GetFactory();
		if ( !vgui::VGui_InitInterfacesList( "CVguiSteamApp", &factory, 1 ) )
		{
			// dimhotepus: PostShutdown is not called when PreInit fails, so roll back
			// everything connected by base classes to not leak tier1/2/3 connections.
			BaseClass::PostShutdown();
			return false;
		}

		return true;
	}

	void PostShutdown() override
	{
		vgui::VGui_ShutdownInterfacesList( "CVguiSteamApp" );

		BaseClass::PostShutdown();
	}
};


//-----------------------------------------------------------------------------
// The application object for apps that use vgui and datamodel
//-----------------------------------------------------------------------------
class CVguiDmSteamApp : public CTier3DmSteamApp
{
	using BaseClass = CTier3DmSteamApp;

public:
	// Methods of IApplication
	bool PreInit() override
	{
		if ( !BaseClass::PreInit() )
			return false;

		CreateInterfaceFn factory = GetFactory();
		// dimhotepus: Use own module name (was copy-pasted "CVguiSteamApp"), so
		// vgui diagnostics point at the right application object.
		if ( !vgui::VGui_InitInterfacesList( "CVguiDmSteamApp", &factory, 1 ) )
		{
			// dimhotepus: Roll back base class state, see CVguiSteamApp::PreInit.
			BaseClass::PostShutdown();
			return false;
		}

		return true;
	}
	
	void PostShutdown() override
	{
		// dimhotepus: Use own module name (was copy-pasted "CVguiSteamApp"), so
		// vgui diagnostics point at the right application object.
		vgui::VGui_ShutdownInterfacesList( "CVguiDmSteamApp" );

		BaseClass::PostShutdown();
	}
};


#endif // TIER3APP_H