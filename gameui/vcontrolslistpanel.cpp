//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "vcontrolslistpanel.h"
#include "GameUI_Interface.h"
#include "EngineInterface.h"

#include <vgui/IInput.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui/Cursor.h>
#include <KeyValues.h>

// NVNT including for input system access
#include "tier2/tier2.h"
#include "inputsystem/iinputsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: panel used for inline editing of key bindings
//-----------------------------------------------------------------------------
class CInlineEditPanel : public vgui::Panel
{
public:
	// dimhotepus: Add parent to support scaling.
	CInlineEditPanel( vgui::Panel *parent ) : vgui::Panel(parent, "InlineEditPanel")
	{
	}

	virtual void Paint()
	{
		int x = 0, y = 0, wide, tall;
		GetSize(wide, tall);

		// Draw a white rectangle around that cell
		vgui::surface()->DrawSetColor( 255, 165, 0, 255 );
		vgui::surface()->DrawFilledRect( x, y, x + wide, y + tall );
	}

	virtual void OnKeyCodeTyped(KeyCode code)
	{
		// forward up
		if (GetParent())
		{
			GetParent()->OnKeyCodeTyped(code);
		}
	}

	virtual void ApplySchemeSettings(IScheme *pScheme)
	{
		Panel::ApplySchemeSettings(pScheme);
		SetBorder(pScheme->GetBorder("DepressedButtonBorder"));
	}

	void OnMousePressed(vgui::MouseCode code)
	{
		// forward up mouse pressed messages to be handled by the key options
		if (GetParent())
		{
			GetParent()->OnMousePressed(code);
		}
	}
};

//-----------------------------------------------------------------------------
// Purpose: Construction
//-----------------------------------------------------------------------------
VControlsListPanel::VControlsListPanel( vgui::Panel *parent, const char *listName )	: vgui::SectionedListPanel( parent, listName )
{
	m_bCaptureMode	= false;
	m_nClickRow		= 0;
	m_hFont = INVALID_FONT;
	m_pInlineEditPanel = new CInlineEditPanel(parent);
	// dimhotepus: To scale UI inline edit got parent
	// dimhotepus: Due to parent it is rendered in its bounds on start, which is not needed.
	m_pInlineEditPanel->SetVisible(false);
	m_iMouseX = m_iMouseY = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
VControlsListPanel::~VControlsListPanel()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VControlsListPanel::ApplySchemeSettings(IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	m_hFont	= pScheme->GetFont("Default", IsProportional() ); 
}

//-----------------------------------------------------------------------------
// Purpose: Start capture prompt display
//-----------------------------------------------------------------------------
void VControlsListPanel::StartCaptureMode( HCursor hCursor )
{
	m_bCaptureMode = true;
	EnterEditMode(m_nClickRow, 1, m_pInlineEditPanel);
	input()->SetMouseFocus(m_pInlineEditPanel->GetVPanel());
	input()->SetMouseCapture(m_pInlineEditPanel->GetVPanel());
	// NVNT tell the input system that novint devices
	// should be set unable to do menu mouse emulation.
	g_pInputSystem->SetNovintPure(true);

	engine->StartKeyTrapMode();

	if (hCursor)
	{
		m_pInlineEditPanel->SetCursor(hCursor);

		// save off the cursor position so we can restore it
		vgui::input()->GetCursorPos( m_iMouseX, m_iMouseY );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Finish capture prompt display
//-----------------------------------------------------------------------------
void VControlsListPanel::EndCaptureMode( HCursor hCursor )
{
	m_bCaptureMode = false;
	input()->SetMouseCapture(NULL);
	LeaveEditMode();
	RequestFocus();
	input()->SetMouseFocus(GetVPanel());
	// NVNT tell the input system that novint devices
	// should be allowed to do menu mouse emulation.
	g_pInputSystem->SetNovintPure(false);
	if (hCursor)
	{
		m_pInlineEditPanel->SetCursor(hCursor);
		surface()->SetCursor(hCursor);	
		if ( hCursor != dc_none )
		{
			vgui::input()->SetCursorPos ( m_iMouseX, m_iMouseY );	
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set active row column
//-----------------------------------------------------------------------------
void VControlsListPanel::SetItemOfInterest(int itemID)
{
	m_nClickRow	= itemID;
}

//-----------------------------------------------------------------------------
// Purpose: Retrieve row, column of interest
//-----------------------------------------------------------------------------
int VControlsListPanel::GetItemOfInterest()
{
	return m_nClickRow;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we're currently waiting to capture a key
//-----------------------------------------------------------------------------
bool VControlsListPanel::IsCapturing( void )
{
	return m_bCaptureMode;
}

//-----------------------------------------------------------------------------
// Purpose: Forwards mouse pressed message up to keyboard page when in capture
//-----------------------------------------------------------------------------
void VControlsListPanel::OnMousePressed(vgui::MouseCode code)
{
	if (IsCapturing())
	{
		// forward up mouse pressed messages to be handled by the key options
		if (GetParent())
		{
			GetParent()->OnMousePressed(code);
		}
	}
	else
	{
		BaseClass::OnMousePressed(code);
	}
}


//-----------------------------------------------------------------------------
// Purpose: input handler
//-----------------------------------------------------------------------------
void VControlsListPanel::OnMouseDoublePressed( vgui::MouseCode code )
{
	if (IsItemIDValid(GetSelectedItem()))
	{
		// enter capture mode
		OnKeyCodePressed(KEY_ENTER);
	}
	else
	{
		BaseClass::OnMouseDoublePressed(code);
	}
}
