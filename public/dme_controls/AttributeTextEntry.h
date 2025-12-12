//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTETEXTENTRY_H
#define ATTRIBUTETEXTENTRY_H

#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/TextEntry.h"
#include "tier1/utlvector.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;
class CAttributeTextPanel;
class KeyValues;

namespace vgui
{
	class IScheme;
	class Label;
	class Menu;
}


//-----------------------------------------------------------------------------
// CAttributeTextEntry
//-----------------------------------------------------------------------------
class CAttributeTextEntry : public vgui::TextEntry
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CAttributeTextEntry, vgui::TextEntry );

public:
	CAttributeTextEntry( Panel *parent, const char *panelName );
	bool GetSelectedRange( intp& cx0, intp& cx1 ) override
	{
		return BaseClass::GetSelectedRange( cx0, cx1 );
	}

protected:
	CAttributeTextPanel *GetParentAttributePanel();
	void OnMouseWheeled( int delta ) override;

	// We'll only create an "undo" record if the values differ upon focus change
	void OnSetFocus() override;
	void OnKillFocus() override;
	void OnKeyCodeTyped( vgui::KeyCode code ) override;

	void OnPanelDropped( CUtlVector< KeyValues * >& data ) override;
	bool GetDropContextMenu( vgui::Menu *menu, CUtlVector< KeyValues * >& msglist ) override;
	bool IsDroppable( CUtlVector< KeyValues * >& msglist ) override;

	void ApplySchemeSettings( vgui::IScheme *pScheme ) override;

	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );

private:
	enum
	{
		MAX_TEXT_LENGTH = 1024
	};

    template<class T> void ApplyMouseWheel( T newValue, T originalValue );
	void StoreInitialValue( bool bForce = false );
	void WriteValueToAttribute();
	void WriteInitialValueToAttribute();

	bool				m_bValueStored;
	char				m_szOriginalText[ MAX_TEXT_LENGTH ];
	union
	{
		float			m_flOriginalValue;
		int				m_nOriginalValue;
		bool			m_bOriginalValue;
	};
};


// ----------------------------------------------------------------------------
#endif // ATTRIBUTETEXTENTRY_H
