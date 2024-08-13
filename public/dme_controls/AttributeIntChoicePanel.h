//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTEINTCHOICEPANEL_h
#define ATTRIBUTEINTCHOICEPANEL_h

#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/BaseAttributeChoicePanel.h"
#include "movieobjects/dmeeditortypedictionary.h"
#include "vgui_controls/MessageMap.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct AttributeWidgetInfo_t;

namespace vgui
{
	class Panel;
	class ComboBox;
}


//-----------------------------------------------------------------------------
// Configuration for integer choices
//-----------------------------------------------------------------------------
class CDmeEditorIntChoicesInfo : public CDmeEditorChoicesInfo
{
	DEFINE_ELEMENT( CDmeEditorIntChoicesInfo, CDmeEditorChoicesInfo );

public:
	// Add a choice
	void AddChoice( int nValue, const char *pChoiceString );

	// Gets the choices
	int GetChoiceValue( int nIndex ) const;
};


//-----------------------------------------------------------------------------
// CAttributeIntChoicePanel
//-----------------------------------------------------------------------------
class CAttributeIntChoicePanel : public CBaseAttributeChoicePanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CAttributeIntChoicePanel, CBaseAttributeChoicePanel );

public:
	CAttributeIntChoicePanel( vgui::Panel *parent,	const AttributeWidgetInfo_t &info );

private:
	// Derived classes can re-implement this to fill the combo box however they like
	void PopulateComboBox( vgui::ComboBox *pComboBox ) override;
	void SetAttributeFromComboBox( vgui::ComboBox *pComboBox, KeyValues *pKeyValues ) override;
	void SetComboBoxFromAttribute( vgui::ComboBox *pComboBox ) override;
};


#endif // ATTRIBUTEINTCHOICEPANEL_h
