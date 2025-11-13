//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef IVGUIMODULE_H
#define IVGUIMODULE_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"
#include <vgui/VGUI.h>

namespace vgui {

class Panel;

}

//-----------------------------------------------------------------------------
// Purpose: Standard interface to loading vgui modules
//-----------------------------------------------------------------------------
abstract_class IVGuiModule : public IBaseInterface
{
public:
	// called first to setup the module with the vgui
	// returns true on success, false on failure
	virtual bool Initialize(CreateInterfaceFn *vguiFactories, intp factoryCount) = 0;

	// called after all the modules have been initialized
	// modules should use this time to link to all the other module interfaces
	virtual bool PostInitialize(CreateInterfaceFn *modules, intp factoryCount) = 0;

	// called when the module is selected from the menu or otherwise activated
	virtual bool Activate() = 0;

	// returns true if the module is successfully initialized and available
	virtual bool IsValid() = 0;

	// requests that the UI is temporarily disabled and all data files saved
	virtual void Deactivate() = 0;

	// restart from a Deactivate()
	virtual void Reactivate() = 0;

	// called when the module is about to be shutdown
	virtual void Shutdown() = 0;

	// returns a handle to the main module panel
	virtual vgui::VPANEL GetPanel() = 0;

	// sets the parent of the main module panel
	virtual void SetParent(vgui::VPANEL parent) = 0;

	// dimhotepus: Initialize with parent to immediately scale UI.
	// called after all the modules have been initialized
	// modules should use this time to link to all the other module interfaces
	virtual bool PostInitialize(CreateInterfaceFn *modules, intp factoryCount, vgui::Panel *parent) = 0;

	// messages sent through through the panel returned by GetPanel():
	//
	//  "ConnectedToGame"	"ip" "port" "gamedir"
	//	"DisconnectedFromGame"
	//  "ActiveGameName"	"name"
	//	"LoadingStarted"	"type"	"name"
	//  "LoadingFinished"	"type"	"name"
};

#define VGUIMODULE_INTERFACE_VERSION "VGuiModule003"


#endif // IVGUIMODULE_H
