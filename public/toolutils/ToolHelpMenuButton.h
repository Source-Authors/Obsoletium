//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Core Movie Maker UI API
//
//=============================================================================

#ifndef TOOLHELPMENUBUTTON_H
#define TOOLHELPMENUBUTTON_H


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
CToolMenuButton* CreateToolHelpMenuButton( char const *toolName, char const *helpBinding, vgui::Panel *parent, const char *panelName, const char *text, vgui::Panel *pActionTarget );


#endif // TOOLHELPMENUBUTTON_H
