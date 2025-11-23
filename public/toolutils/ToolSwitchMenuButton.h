//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Core Movie Maker UI API
//
//=============================================================================

#ifndef TOOLSWITCHMENUBUTTON_H
#define TOOLSWITCHMENUBUTTON_H



//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
namespace vgui
{
class Panel;
}

class CToolMenuButton;


//-----------------------------------------------------------------------------
// Global function to create the switch menu
//-----------------------------------------------------------------------------
CToolMenuButton* CreateToolSwitchMenuButton( vgui::Panel *parent, const char *panelName, const char *text, vgui::Panel *pActionTarget );


#endif // TOOLSWITCHMENUBUTTON_H
