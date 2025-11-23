//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The tool UI has 4 purposes:
//		1) Create the menu bar and client area (lies under the menu bar)
//		2) Forward all mouse messages to the tool workspace so action menus work
//		3) Forward all commands to the tool system so all smarts can reside there
//		4) Control the size of the menu bar + the working area
//=============================================================================

#ifndef TOOLUI_H
#define TOOLUI_H


#include "vgui_controls/Panel.h"
#include "vgui/MouseCode.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CBaseToolSystem;

namespace vgui
{
	class MenuBar;
}

//-----------------------------------------------------------------------------
// The tool UI has 4 purposes:
//		1) Create the menu bar and client area (lies under the menu bar)
//		2) Forward all mouse messages to the tool workspace so action menus work
//		3) Forward all commands to the tool system so all smarts can reside there
//		4) Control the size of the menu bar + the working area
//-----------------------------------------------------------------------------
class CToolUI : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CToolUI, vgui::Panel );

public:
	// Constructor
	CToolUI( vgui::Panel *pParent, const char *pPanelName, CBaseToolSystem *pBaseToolSystem );

	// Overrides of panel methods
	void	PerformLayout() override;
	void	OnCommand( const char *cmd ) override;
	void	OnMousePressed( vgui::MouseCode code ) override;

	virtual void	UpdateMenuBarTitle();

	// Other public methods
	vgui::MenuBar	*GetMenuBar();
	vgui::Panel		*GetClientArea();
	vgui::Panel		*GetStatusBar();

private:
	vgui::MenuBar	*m_pMenuBar;
	vgui::Panel		*m_pStatusBar;
	vgui::Panel		*m_pClientArea;
	CBaseToolSystem *m_pBaseToolSystem;
};


#endif // TOOLUI_H
