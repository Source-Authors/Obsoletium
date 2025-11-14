//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTECOLORPICKERPANEL_H
#define ATTRIBUTECOLORPICKERPANEL_H

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
// CAttributeColorPickerPanel
//-----------------------------------------------------------------------------
class CAttributeColorPickerPanel : public CAttributeTextPanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CAttributeColorPickerPanel, CAttributeTextPanel );

public:
	CAttributeColorPickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info );

	// Inherited from Panel
	void	OnCommand( const char *cmd ) override;
	void	PerformLayout() override;
	void	Refresh() override;
	void	ApplySchemeSettings(vgui::IScheme *pScheme) override;

private:
	MESSAGE_FUNC_PARAMS( OnPreview, "ColorPickerPreview", data );
	MESSAGE_FUNC_PARAMS( OnPicked, "ColorPickerPicked", data );
	MESSAGE_FUNC( OnCancelled, "ColorPickerCancel" );
	void UpdateButtonColor();

	vgui::Button	*m_pOpen;
	Color			m_InitialColor;
};


#endif // ATTRIBUTECOLORPICKERPANEL_H
