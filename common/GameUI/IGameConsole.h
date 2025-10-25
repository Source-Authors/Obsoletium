//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef IGAMECONSOLE_H
#define IGAMECONSOLE_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"

namespace vgui
{

// handle to an internal vgui panel
// this is the only handle to a panel that is valid across dll boundaries
using VPANEL = uintp;

}

//-----------------------------------------------------------------------------
// Purpose: interface to game/dev console
//-----------------------------------------------------------------------------
abstract_class IGameConsole : public IBaseInterface
{
public:
	// activates the console, makes it visible and brings it to the foreground
	virtual void Activate() = 0;

	virtual void Initialize() = 0;

	// hides the console
	virtual void Hide() = 0;

	// clears the console
	virtual void Clear() = 0;

	// return true if the console has focus
	virtual bool IsConsoleVisible() = 0;

	// dimhotepus: Use strict type.
	virtual void SetParent( vgui::VPANEL parent ) = 0;

	// dimhotepus: Initialize with parent to scale UI.
	virtual void Initialize( vgui::VPANEL parent, const char *panelModule ) = 0;
};

// dimhotepus: Bump version.
#define GAMECONSOLE_INTERFACE_VERSION "GameConsole005"

#endif // IGAMECONSOLE_H
