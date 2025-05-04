//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef FILTERCOMBOBOX_H
#define FILTERCOMBOBOX_H

#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/ComboBox.h"


//-----------------------------------------------------------------------------
// Combo box that adds entry to its history when focus is lost
//-----------------------------------------------------------------------------
class CFilterComboBox : public vgui::ComboBox
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CFilterComboBox, vgui::ComboBox );

public:
	CFilterComboBox( Panel *parent, const char *panelName, int numLines, bool allowEdit );
	void OnKillFocus() override;
};


#endif // FILTERCOMBOBOX_H

	
