//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include <stdio.h>

#include "GameConsole.h"
#include "GameConsoleDialog.h"
#include "LoadingDialog.h"
#include "vgui/ISurface.h"

#include "KeyValues.h"
#include "vgui/VGUI.h"
#include "vgui/IVGui.h"
#include "vgui_controls/Panel.h"
#include "convar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CGameConsole g_GameConsole;
//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CGameConsole &GameConsole()
{
	return g_GameConsole;
}
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameConsole, IGameConsole, GAMECONSOLE_INTERFACE_VERSION, g_GameConsole);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGameConsole::CGameConsole()
{
	m_bInitialized = false;
	m_pConsole = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGameConsole::~CGameConsole()
{
	m_bInitialized = false;
}

//-----------------------------------------------------------------------------
// Purpose: sets up the console for use
//-----------------------------------------------------------------------------
void CGameConsole::Initialize()
{
	Initialize( 0, "" );
}

//-----------------------------------------------------------------------------
// Purpose: activates the console, makes it visible and brings it to the foreground
//-----------------------------------------------------------------------------
void CGameConsole::Activate()
{
#ifndef _XBOX
	if (!m_bInitialized)
		return;

	vgui::surface()->RestrictPaintToSinglePanel(NULL);
	m_pConsole->Activate();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: hides the console
//-----------------------------------------------------------------------------
void CGameConsole::Hide()
{
#ifndef _XBOX
	if (!m_bInitialized)
		return;

	m_pConsole->Hide();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: clears the console
//-----------------------------------------------------------------------------
void CGameConsole::Clear()
{
#ifndef _XBOX
	if (!m_bInitialized)
		return;

	m_pConsole->Clear();
#endif
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the console is currently in focus
//-----------------------------------------------------------------------------
bool CGameConsole::IsConsoleVisible()
{
#ifndef _XBOX
	if (!m_bInitialized)
		return false;
	
	return m_pConsole->IsVisible();
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: activates the console after a delay
//-----------------------------------------------------------------------------
void CGameConsole::ActivateDelayed(float time)
{
#ifndef _XBOX
	if (!m_bInitialized)
		return;

	m_pConsole->PostMessage(m_pConsole, new KeyValues("Activate"), time);
#endif
}

void CGameConsole::SetParent( vgui::VPANEL parent )
{	
#ifndef _XBOX
	if (!m_bInitialized)
		return;

	m_pConsole->SetParent( parent );

	if (vgui::ipanel()->IsProportional(parent))
	{
		// dimhotepus: Apply proportional from parent.
		m_pConsole->InvalidateLayout(true, true);
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: static command handler
//-----------------------------------------------------------------------------
void CGameConsole::OnCmdCondump()
{
#ifndef _XBOX
	g_GameConsole.m_pConsole->DumpConsoleTextToFile();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: sets up the console for use
//-----------------------------------------------------------------------------
void CGameConsole::Initialize( vgui::VPANEL parent, const char *panelModule )
{
#ifndef _XBOX
	m_pConsole = vgui::SETUP_PANEL( new CGameConsoleDialog( vgui::ipanel()->GetPanel( parent, panelModule ) ) ); // we add text before displaying this so set it up now!

	// set the console to taking up most of the right-half of the screen
	int swide, stall;
	vgui::surface()->GetScreenSize(swide, stall);
	// dimhotepus: Console should take more space as scaled it is too small.
	int offsetx = vgui::scheme()->GetProportionalScaledValue(48);
	int offsety = vgui::scheme()->GetProportionalScaledValue(64);

	m_pConsole->SetBounds(
		swide / 2 - offsetx,
		offsety,
		// dimhotepus: Console should take more space as scaled it is too small.
		swide / 2 + offsetx - vgui::scheme()->GetProportionalScaledValue(8),
		stall - (offsety * 2));

	m_bInitialized = true;
#endif
}

#ifndef _XBOX
CON_COMMAND( condump, "dump the text currently in the console to condumpXX.log" )
{
	g_GameConsole.OnCmdCondump();
}
#endif
