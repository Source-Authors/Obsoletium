//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CONTROLLERDIALOG_H
#define CONTROLLERDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "OptionsDialog.h"

class CControllerDialog : public COptionsDialogXbox
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CControllerDialog, COptionsDialogXbox );

public:
	CControllerDialog(vgui::Panel *parent);

	void ApplySchemeSettings( vgui::IScheme *pScheme ) override;

};

#endif // CONTROLLERDIALOG_H

