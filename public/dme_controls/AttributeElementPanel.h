//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTEELEMENTPANEL_H
#define ATTRIBUTEELEMENTPANEL_H

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
// CAttributeElementPanel
//-----------------------------------------------------------------------------
class CAttributeElementPanel : public CBaseAttributePanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CAttributeElementPanel, CBaseAttributePanel );

public:
	CAttributeElementPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info );

	void PostConstructor() override;
	void Apply() override;

protected:
	vgui::Panel *GetDataPanel() override;
	void OnCreateDragData( KeyValues *msg ) override;

	MESSAGE_FUNC(OnTextChanged, "TextChanged")
	{
		SetDirty( true );
	}

private:
	void Refresh() override;

	CAttributeTextEntry		*m_pData;
	bool					m_bShowMemoryUsage;
};


#endif // ATTRIBUTEELEMENTPANEL_H
