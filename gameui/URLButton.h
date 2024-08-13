//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef URLBUTTON_H
#define URLBUTTON_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui/Dar.h>
#include <Color.h>
#include <vgui_controls/Label.h>
#include "vgui/MouseCode.h"

namespace vgui
{

//-----------------------------------------------------------------------------
// Purpose: A control that looks like a hyperlink, but behaves like a button.
//-----------------------------------------------------------------------------
class URLButton : public Label
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( URLButton, Label );

public:
	// You can optionally pass in the panel to send the click message to and the name of the command to send to that panel.
	URLButton(Panel *parent, const char *panelName, const char *text, Panel *pActionSignalTarget=NULL, const char *pCmd=NULL);
	URLButton(Panel *parent, const char *panelName, const wchar_t *text, Panel *pActionSignalTarget=NULL, const char *pCmd=NULL);
	~URLButton();
private:
	void Init() override;
public:
	// Set armed state.
	virtual void SetArmed(bool state);
	// Check armed state
	virtual bool IsArmed( void );

	// Check depressed state
	virtual bool IsDepressed();
	// Set button force depressed state.
	virtual void ForceDepressed(bool state);
	// Set button depressed state with respect to the force depressed state.
	virtual void RecalculateDepressedState( void );

	// Set button selected state.
	virtual void SetSelected(bool state);
	// Check selected state
	virtual bool IsSelected( void );

	//Set whether or not the button captures all mouse input when depressed.
	virtual void SetUseCaptureMouse( bool state );
	// Check if mouse capture is enabled.
	virtual bool IsUseCaptureMouseEnabled( void );

	// Activate a button click.
	MESSAGE_FUNC( DoClick, "PressButton" );
	MESSAGE_FUNC( OnHotkey, "Hotkey" )
	{
		DoClick();
	}

	// Set button to be mouse clickable or not.
	virtual void SetMouseClickEnabled( MouseCode code, bool state );

   // sets the how this button activates
	enum ActivationType_t
	{
		ACTIVATE_ONPRESSEDANDRELEASED,	// normal button behaviour
		ACTIVATE_ONPRESSED,				// menu buttons, toggle buttons
		ACTIVATE_ONRELEASED,			// menu items
	};
	virtual void SetButtonActivationType(ActivationType_t activationType);

	// Message targets that the button has been pressed
	virtual void FireActionSignal( void );
	// Perform graphical layout of button
	void PerformLayout() override;

	bool RequestInfo(KeyValues *data) override;

	// Respond when key focus is received
	void OnSetFocus() override;
	// Respond when focus is killed
	void OnKillFocus() override;

	// Set button border attribute enabled, controls display of button.
	virtual void SetButtonBorderEnabled( bool state );

	// Get button foreground color
	virtual Color GetButtonFgColor();
	// Get button background color
	virtual Color GetButtonBgColor();

	// Set the command to send when the button is pressed
	// Set the panel to send the command to with AddActionSignalTarget()
	virtual void SetCommand( const char *command );
	// Set the message to send when the button is pressed
	virtual void SetCommand( KeyValues *message );

	/* CUSTOM MESSAGE HANDLING
		"PressButton"	- makes the button act as if it had just been pressed by the user (just like DoClick())
			input: none		
	*/

	void OnCursorEntered() override;
	void OnCursorExited() override;
	void SizeToContents() override;

	virtual KeyValues *GetCommand();

	bool IsDrawingFocusBox();
	void DrawFocusBox( bool bEnable );

protected:

	// Paint button on screen
	void Paint(void) override;
	// Get button border attributes.

	void ApplySchemeSettings(IScheme *pScheme) override;
	MESSAGE_FUNC_INT( OnSetState, "SetState", state );
	
	void OnMousePressed(MouseCode code) override;
	void OnMouseDoublePressed(MouseCode code) override;
	void OnMouseReleased(MouseCode code) override;
	void OnKeyCodePressed(KeyCode code) override;
	void OnKeyCodeReleased(KeyCode code) override;

	// Get control settings for editing
	void GetSettings( KeyValues *outResourceData ) override;
	void ApplySettings( KeyValues *inResourceData ) override;
	const char *GetDescription( void ) override;

	KeyValues *GetActionMessage();

private:
	enum ButtonFlags_t
	{
		ARMED					= 0x0001,
		DEPRESSED				= 0x0002,
		FORCE_DEPRESSED			= 0x0004,
		BUTTON_BORDER_ENABLED	= 0x0008,
		USE_CAPTURE_MOUSE		= 0x0010,
		BUTTON_KEY_DOWN			= 0x0020,
		DEFAULT_BUTTON			= 0x0040,
		SELECTED				= 0x0080,
		DRAW_FOCUS_BOX			= 0x0100,
		BLINK					= 0x0200,
		ALL_FLAGS				= 0xFFFF,
	};

	CUtlFlags< unsigned short > _buttonFlags;	// see ButtonFlags_t
	int                _mouseClickMask;
	KeyValues		  *_actionMessage;
	ActivationType_t   _activationType;


	Color			   _defaultFgColor, _defaultBgColor;

	bool m_bSelectionStateSaved;
};

} // namespace vgui

#endif // URLBUTTON_H
