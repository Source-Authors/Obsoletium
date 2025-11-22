//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "vgui_controls/URLLabel.h"

#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/MouseCode.h"
#include "vgui/Cursor.h"

#include "vgui_controls/TextImage.h"

#include "tier1/KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

// dimhotepus: Add parent to scale UI.
static vgui::Panel *URLLabel_Factory( vgui::Panel* parent )
{
	return new URLLabel(parent, NULL, "URLLabel", NULL);
}

DECLARE_BUILD_FACTORY_CUSTOM( URLLabel, URLLabel_Factory );
//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
URLLabel::URLLabel(Panel *parent, const char *panelName, const char *text, const char *pszURL) : Label(parent, panelName, text)
{
    m_pszURL = nullptr;
	m_bUnderline = false;
    m_iURLSize = 0;
    if ( !Q_isempty( pszURL ) )
    {
        SetURL(pszURL);
    }
}

//-----------------------------------------------------------------------------
// Purpose: unicode constructor
//-----------------------------------------------------------------------------
URLLabel::URLLabel(Panel *parent, const char *panelName, const wchar_t *wszText, const char *pszURL) : Label(parent, panelName, wszText)
{
    m_pszURL = nullptr;
	m_bUnderline = false;
    m_iURLSize = 0;
    if ( !Q_isempty( pszURL ) )
    {
        SetURL(pszURL);
    }
}

//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
URLLabel::~URLLabel()
{
	delete [] m_pszURL;
}

// dimhotepus: TF2 backport.
//-----------------------------------------------------------------------------
// Purpose: checks the URL
//-----------------------------------------------------------------------------
bool URLLabel::IsValidURL( const char *pszURL ) const
{
	if ( Q_isempty( pszURL ) )
		return false;

	if ( ( Q_strncmp( pszURL, "http://", 7 ) != 0 ) && ( Q_strncmp( pszURL, "https://", 8 ) != 0 ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: sets the URL
//-----------------------------------------------------------------------------
void URLLabel::SetURL(const char *pszURL)
{
	// dimhotepus: TF2 backport.
	if ( !IsValidURL( pszURL ) )
	{
		Warning( "Invalid URL in URLLabel '%s'\n", pszURL );
		return;
	}

	intp iNewURLSize = V_strlen(pszURL);
	if (iNewURLSize > m_iURLSize || !m_pszURL)
	{
		delete [] m_pszURL;
		m_pszURL = V_strdup(pszURL);
	}
	else
	{
		V_strncpy(m_pszURL, pszURL, m_iURLSize);
	}

	m_iURLSize = iNewURLSize;
}

//-----------------------------------------------------------------------------
// Purpose: If we were left clicked on, launch the URL
//-----------------------------------------------------------------------------
void URLLabel::OnMousePressed(MouseCode code)
{
    if (code == MOUSE_LEFT)
    {
		if (m_pszURL)
		{
			// dimhotepus: TF2 backport.
			// URL should have already been validated in SetURL()
			if ( !IsValidURL( m_pszURL ) )
			{
				Warning( "Invalid URL '%s'\n", m_pszURL );
			}
			else
			{
				system()->ShellExecute( "open", m_pszURL );
			}
		}
    }
}

//-----------------------------------------------------------------------------
// Purpose: Applies resouce settings
//-----------------------------------------------------------------------------
void URLLabel::ApplySettings(KeyValues *inResourceData)
{
	BaseClass::ApplySettings(inResourceData);

	const char *pszURL = inResourceData->GetString("URLText", NULL);
	if (pszURL)
	{
		if (pszURL[0] == '#')
		{
			// it's a localized url, look it up
			const wchar_t *ws = g_pVGuiLocalize->Find(pszURL + 1);
			if (ws)
			{
				char localizedUrl[512];
				g_pVGuiLocalize->ConvertUnicodeToANSI(ws, localizedUrl);
				SetURL(localizedUrl);
			}
		}
		else
		{
	        SetURL(pszURL);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: saves them to disk
//-----------------------------------------------------------------------------
void URLLabel::GetSettings( KeyValues *outResourceData )
{
	BaseClass::GetSettings(outResourceData);

	if (m_pszURL)
	{
		outResourceData->SetString("URLText", m_pszURL);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns a description of the label string
//-----------------------------------------------------------------------------
const char *URLLabel::GetDescription( void )
{
	static char buf[1024];
	V_sprintf_safe(buf, "%s, string URLText", BaseClass::GetDescription());
	return buf;
}

//-----------------------------------------------------------------------------
// Purpose: scheme settings
//-----------------------------------------------------------------------------
void URLLabel::ApplySchemeSettings(IScheme *pScheme)
{
	// set our font to be underlined by default
	// the Label::ApplySchemeSettings() will override it if override set in scheme file
	SetFont( pScheme->GetFont( "DefaultUnderline", IsProportional() ) );
    BaseClass::ApplySchemeSettings(pScheme);
    SetCursor(dc_hand);
}

