//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTEBASEPICKERPANEL_H
#define ATTRIBUTEBASEPICKERPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/AttributeTextPanel.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;

namespace vgui
{
	class Button;
}


//-----------------------------------------------------------------------------
// CAttributeBasePickerPanel
//-----------------------------------------------------------------------------
class CAttributeBasePickerPanel : public CAttributeTextPanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CAttributeBasePickerPanel, CAttributeTextPanel );

public:
	CAttributeBasePickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info );

	// Inherited from Panel
	void	OnCommand( const char *cmd ) override;
	void	PerformLayout() override;

private:
	// Inherited classes must implement this
	virtual void	ShowPickerDialog() = 0;

	vgui::Button	*m_pOpen;
};


#endif // ATTRIBUTEBASEPICKERPANEL_H
