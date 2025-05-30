//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTEMDLPICKERPANEL_H
#define ATTRIBUTEMDLPICKERPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/AttributeBasePickerPanel.h"
#include "vgui_controls/PHandle.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CMDLPickerFrame;


//-----------------------------------------------------------------------------
// CAttributeMDLPickerPanel
//-----------------------------------------------------------------------------
class CAttributeMDLPickerPanel : public CAttributeBasePickerPanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CAttributeMDLPickerPanel, CAttributeBasePickerPanel );

public:
	CAttributeMDLPickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info );
	~CAttributeMDLPickerPanel();

private:
	MESSAGE_FUNC_PARAMS( OnMDLSelected, "MDLSelected", kv );
	void ShowPickerDialog() override;
};


#endif // ATTRIBUTEMDLPICKERPANEL_H
