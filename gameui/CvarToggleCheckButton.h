//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CVARTOGGLECHECKBUTTON_H
#define CVARTOGGLECHECKBUTTON_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/CheckButton.h>

class CCvarToggleCheckButton : public vgui::CheckButton
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CCvarToggleCheckButton, vgui::CheckButton );

public:
	CCvarToggleCheckButton( vgui::Panel *parent, const char *panelName, const char *text, 
		char const *cvarname );
	~CCvarToggleCheckButton();

	void	SetSelected( bool state ) override;

	void	Paint() override;

	void			Reset();
	void			ApplyChanges();
	bool			HasBeenModified();
	void			ApplySettings( KeyValues *inResourceData ) override;

private:
	MESSAGE_FUNC( OnButtonChecked, "CheckButtonChecked" );

	char			*m_pszCvarName;
	bool			m_bStartValue;
};

#endif // CVARTOGGLECHECKBUTTON_H
