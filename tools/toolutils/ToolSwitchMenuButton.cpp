//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Core Movie Maker UI API
//
//=============================================================================

#include "toolutils/ToolSwitchMenuButton.h"
#include "vgui_controls/Panel.h"
#include "toolutils/ToolMenuButton.h"
#include "toolutils/enginetools_int.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/Menu.h"
#include "toolframework/ienginetool.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Menu to switch between tools
//-----------------------------------------------------------------------------
class CToolSwitchMenuButton : public CToolMenuButton
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CToolSwitchMenuButton, CToolMenuButton );

public:
	CToolSwitchMenuButton( vgui::Panel *parent, const char *panelName, const char *text, vgui::Panel *pActionTarget );
	void OnShowMenu(vgui::Menu *menu) override;
};


//-----------------------------------------------------------------------------
// Global function to create the switch menu
//-----------------------------------------------------------------------------
CToolMenuButton* CreateToolSwitchMenuButton( vgui::Panel *parent, const char *panelName, const char *text, vgui::Panel *pActionTarget )
{
	return new CToolSwitchMenuButton( parent, panelName, text, pActionTarget );
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CToolSwitchMenuButton::CToolSwitchMenuButton( vgui::Panel *parent, const char *panelName, const char *text, vgui::Panel *pActionTarget ) :
	BaseClass( parent, panelName, text, pActionTarget )
{
	SetMenu(m_pMenu);
}


//-----------------------------------------------------------------------------
// Is called when the menu is made visible
//-----------------------------------------------------------------------------
void CToolSwitchMenuButton::OnShowMenu(vgui::Menu *menu)
{
	BaseClass::OnShowMenu( menu );

	Reset();

	int c = enginetools->GetToolCount();
	for ( int i = 0 ; i < c; ++i )
	{
		char const *toolname = enginetools->GetToolName( i );

		char toolcmd[ 32 ];
		V_sprintf_safe( toolcmd, "OnTool%i", i );

		int id = AddCheckableMenuItem( toolname, toolname, new KeyValues ( "Command", "command", toolcmd ), m_pActionTarget );
		m_pMenu->SetItemEnabled( id, true );
		m_pMenu->SetMenuItemChecked( id, enginetools->IsTopmostTool( enginetools->GetToolSystem( i ) ) );
	}
}
