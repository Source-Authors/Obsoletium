//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "GameConsole.h"
#include "GameConsoleDialog.h"
#include "LoadingDialog.h"
#include "vgui/ISurface.h"

#include "tier1/KeyValues.h"
#include "vgui/VGUI.h"
#include "vgui/IVGui.h"
#include "vgui_controls/Panel.h"
#include "tier1/convar.h"

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
	if (!m_pConsole)
		return;

	vgui::surface()->RestrictPaintToSinglePanel(0);
	m_pConsole->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: hides the console
//-----------------------------------------------------------------------------
void CGameConsole::Hide()
{
	if (!m_pConsole)
		return;

	m_pConsole->Hide();
}

//-----------------------------------------------------------------------------
// Purpose: clears the console
//-----------------------------------------------------------------------------
void CGameConsole::Clear()
{
	if (!m_pConsole)
		return;

	m_pConsole->Clear();
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the console is currently in focus
//-----------------------------------------------------------------------------
bool CGameConsole::IsConsoleVisible()
{
	if (!m_pConsole)
		return false;
	
	return m_pConsole->IsVisible();
}

//-----------------------------------------------------------------------------
// Purpose: activates the console after a delay
//-----------------------------------------------------------------------------
void CGameConsole::ActivateDelayed(float time)
{
	if (!m_pConsole)
		return;

	m_pConsole->PostMessage(m_pConsole, new KeyValues("Activate"), time);
}

void CGameConsole::SetParent( vgui::VPANEL parent )
{	
	if (!m_pConsole)
		return;

	m_pConsole->SetParent( parent );

	if (parent && vgui::ipanel()->IsProportional(parent))
	{
		// dimhotepus: Apply proportional from parent.
		m_pConsole->InvalidateLayout(true, true);
	}
}

//-----------------------------------------------------------------------------
// Purpose: static command handler
//-----------------------------------------------------------------------------
void CGameConsole::OnCmdCondump()
{
	// dimhotepus: Ensure no nullptr dereference when console doesn't initialized yet.
	if ( g_GameConsole.m_pConsole )
	{
		g_GameConsole.m_pConsole->DumpConsoleTextToFile();
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets up the console for use
//-----------------------------------------------------------------------------
void CGameConsole::Initialize( vgui::VPANEL parent, const char *panelModule )
{
	m_pConsole = vgui::SETUP_PANEL( new CGameConsoleDialog( vgui::ipanel()->GetPanel( parent, panelModule ) ) ); // we add text before displaying this so set it up now!

	// set the console to taking up most of the right-half of the screen
	int swide, stall;
	vgui::surface()->GetScreenSize(swide, stall);
	// dimhotepus: Console should take more space as scaled it is too small.
	int offsetx = vgui::scheme()->GetProportionalScaledValue(40);
	int offsety = vgui::scheme()->GetProportionalScaledValue(64);

	m_pConsole->SetBounds(
		swide / 2 - offsetx,
		offsety,
		// dimhotepus: Console should take more space as scaled it is too small.
		swide / 2 + offsetx - vgui::scheme()->GetProportionalScaledValue(8),
		stall - (offsety * 2));
}

CON_COMMAND( condump, "dump the text currently in the console to condumpXX.log" )
{
	g_GameConsole.OnCmdCondump();
}
