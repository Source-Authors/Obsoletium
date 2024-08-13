//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CONTROLLERMAP_H
#define CONTROLLERMAP_H
#ifdef _WIN32
#pragma once
#endif

#include "Panel.h"
#include "utlmap.h"
#include "utlsymbol.h"

class CControllerMap : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CControllerMap, vgui::Panel )

	void OnKeyCodeTyped( vgui::KeyCode code ) override;

public:
	CControllerMap( vgui::Panel *parent, const char *name );

	void ApplySettings( KeyValues *inResourceData ) override;

	int NumButtons( void )
	{
		return m_buttonMap.Count();
	}

	const char *GetBindingText( int idx );
	const char *GetBindingIcon( int idx );

private:

	struct button_t
	{
		CUtlSymbol	cmd;
		CUtlSymbol	text;
		CUtlSymbol	icon;
	};
	CUtlMap< int, button_t > m_buttonMap;
};

#endif // CONTROLLERMAP_H
