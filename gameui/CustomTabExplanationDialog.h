//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef CUSTOMTABEXPLANATIONDIALOG_H
#define CUSTOMTABEXPLANATIONDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"
#include "utlvector.h"
#include <vgui/KeyCode.h>
#include "vgui_controls/URLLabel.h"

//-----------------------------------------------------------------------------
// Purpose: Dialog that explains the custom tab
//-----------------------------------------------------------------------------
class CCustomTabExplanationDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CCustomTabExplanationDialog, vgui::Frame );

public:
	CCustomTabExplanationDialog(vgui::Panel *parent);
	~CCustomTabExplanationDialog();

	void ApplySchemeSettings( vgui::IScheme *pScheme ) override;
	void OnKeyCodePressed(vgui::KeyCode code) override;
	void OnCommand( const char *command ) override;
	void OnClose( void ) override;
};

#endif // CUSTOMTABEXPLANATIONDIALOG_H
