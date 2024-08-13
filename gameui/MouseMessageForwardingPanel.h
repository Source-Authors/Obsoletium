//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MOUSEMESSAGEFORWARDINGPANEL_H
#define MOUSEMESSAGEFORWARDINGPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Panel.h"

//-----------------------------------------------------------------------------
// Purpose: Invisible panel that forwards up mouse movement
//-----------------------------------------------------------------------------
class CMouseMessageForwardingPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CMouseMessageForwardingPanel, vgui::Panel );
public:
	CMouseMessageForwardingPanel( Panel *parent, const char *name );

	void PerformLayout( void ) override;
	void OnMousePressed( vgui::MouseCode code ) override;
	void OnMouseDoublePressed( vgui::MouseCode code ) override;
};

#endif //MOUSEMESSAGEFORWARDINGPANEL_H