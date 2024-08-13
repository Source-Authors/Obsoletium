//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CVARNEGATECHECKBUTTON_H
#define CVARNEGATECHECKBUTTON_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/CheckButton.h>

class CCvarNegateCheckButton : public vgui::CheckButton
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CCvarNegateCheckButton, vgui::CheckButton );

public:
	CCvarNegateCheckButton( vgui::Panel *parent, const char *panelName, const char *text, 
		char const *cvarname );
	~CCvarNegateCheckButton();

	void			SetSelected( bool state ) override;
	void			Paint() override;

	void			Reset();
	void			ApplyChanges();
	bool			HasBeenModified();

private:
	MESSAGE_FUNC( OnButtonChecked, "CheckButtonChecked" );

	char			*m_pszCvarName;
	bool			m_bStartState;
};

#endif // CVARNEGATECHECKBUTTON_H
