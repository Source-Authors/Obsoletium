//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GAMECONSOLE_H
#define GAMECONSOLE_H
#ifdef _WIN32
#pragma once
#endif

#include "GameUI/IGameConsole.h"

class CGameConsoleDialog;

//-----------------------------------------------------------------------------
// Purpose: VGui implementation of the game/dev console
//-----------------------------------------------------------------------------
class CGameConsole : public IGameConsole
{
public:
	CGameConsole();
	~CGameConsole();

	// sets up the console for use
	void Initialize() override;

	// activates the console, makes it visible and brings it to the foreground
	void Activate() override;
	// hides the console
	void Hide() override;
	// clears the console
	void Clear() override;

	// returns true if the console is currently in focus
	bool IsConsoleVisible() override;

	// dimhotepus: Use strict type.
	void SetParent( vgui::VPANEL parent ) override;

	// sets up the console for use
	void Initialize( vgui::VPANEL parent, const char *panelModule ) override;

	// activates the console after a delay
	void ActivateDelayed(float time);

	static void OnCmdCondump();
private:

	bool m_bInitialized;
	CGameConsoleDialog *m_pConsole;
};

extern CGameConsole &GameConsole();

#endif // GAMECONSOLE_H
