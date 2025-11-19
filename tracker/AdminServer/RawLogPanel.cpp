//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "RawLogPanel.h"

#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui/ILocalize.h>

#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/RichText.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ToggleButton.h>
#include <vgui_controls/RadioButton.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/PHandle.h>
#include <vgui_controls/PropertySheet.h>
#include "vgui_controls/ConsoleDialog.h"

#include "tier1/KeyValues.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CRawLogPanel::CRawLogPanel(vgui::Panel *parent, const char *name) : vgui::PropertyPage(parent, name)
{
	// dimhotepus: Scale UI.
	SetSize(QuickPropScale( 200 ), QuickPropScale( 100 ));
	m_pConsole = new CConsolePanel( this, "Console", false );
	
	LoadControlSettings("Admin\\RawLogPanel.res", "PLATFORM");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CRawLogPanel::~CRawLogPanel()
{
}

//-----------------------------------------------------------------------------
// Purpose: Activates the page
//-----------------------------------------------------------------------------
void CRawLogPanel::OnPageShow()
{
	BaseClass::OnPageShow();
}

//-----------------------------------------------------------------------------
// Purpose: Hides the page
//-----------------------------------------------------------------------------
void CRawLogPanel::OnPageHide()
{
	BaseClass::OnPageHide();
}

//-----------------------------------------------------------------------------
// Purpose: inserts a new string into the main chat panel
//-----------------------------------------------------------------------------
void CRawLogPanel::DoInsertString(const char *str) 
{
	if ( str )
	{
		m_pConsole->Print( str );
	}
}

//-----------------------------------------------------------------------------
// Purpose: run when the send button is pressed, execs a command on the server
//-----------------------------------------------------------------------------
void CRawLogPanel::OnCommandSubmitted( char const *pchCommand )
{
	if ( !pchCommand || !*pchCommand )
		return;

	// execute the typed command
	RemoteServer().SendCommand( pchCommand );
}
