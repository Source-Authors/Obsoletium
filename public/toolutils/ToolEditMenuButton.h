//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Standard file menu
//
//=============================================================================


#ifndef TOOLEDITMENUBUTTON_H
#define TOOLEDITMENUBUTTON_H


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
CToolMenuButton* CreateToolEditMenuButton( vgui::Panel *parent, const char *panelName, 
	const char *text, vgui::Panel *pActionTarget );


#endif // TOOLEDITMENUBUTTON_H

