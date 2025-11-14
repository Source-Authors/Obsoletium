//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTETEXTPANEL_H
#define ATTRIBUTETEXTPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/BaseAttributePanel.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;
class CAttributeTextEntry;

namespace vgui
{
	class Label;
}


//-----------------------------------------------------------------------------
// CAttributeTextPanel
//-----------------------------------------------------------------------------
class CAttributeTextPanel : public CBaseAttributePanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CAttributeTextPanel, CBaseAttributePanel );

public:
	CAttributeTextPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info );
	void SetFont( vgui::HFont font ) override;
	void PostConstructor() override;
	void Apply() override;
	void Refresh() override;

	// Returns the text type
	const char *GetTextType();

protected:
	vgui::Panel *GetDataPanel() override;

	MESSAGE_FUNC(OnTextChanged, "TextChanged")
	{
		SetDirty( true );
	}

protected:
	CAttributeTextEntry	*m_pData;
	bool m_bShowMemoryUsage;
};


#endif // ATTRIBUTETEXTPANEL_H
