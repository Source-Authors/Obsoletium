//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef URLLABEL_H
#define URLLABEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Label.h>

namespace vgui
{

class URLLabel : public Label
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( URLLabel, Label );

public:	
	URLLabel(Panel *parent, const char *panelName, const char *text, const char *pszURL);
	URLLabel(Panel *parent, const char *panelName, const wchar_t *wszText, const char *pszURL);
    ~URLLabel();

    void SetURL(const char *pszURL);

protected:
	void OnMousePressed(MouseCode code) override;
	void ApplySettings( KeyValues *inResourceData ) override;
	void GetSettings( KeyValues *outResourceData ) override;
	void ApplySchemeSettings(IScheme *pScheme) override;
	const char *GetDescription() override;

	const char *GetURL( void ) { return m_pszURL; }

private:
    char    *m_pszURL;
    int     m_iURLSize;
	bool	m_bUnderline;
};

}

#endif // URLLABEL_H
