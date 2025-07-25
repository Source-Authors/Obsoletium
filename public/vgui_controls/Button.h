//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef BUTTON_H
#define BUTTON_H

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
// Purpose: 
//-----------------------------------------------------------------------------
class Button : public Label
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( Button, Label );

public:
	// You can optionally pass in the panel to send the click message to and the name of the command to send to that panel.
	Button(Panel *parent, const char *panelName, const char *text, Panel *pActionSignalTarget=NULL, const char *pCmd=NULL);
	Button(Panel *parent, const char *panelName, const wchar_t *text, Panel *pActionSignalTarget=NULL, const char *pCmd=NULL);
	~Button();
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

	virtual void SetBlink(bool state);
	virtual bool IsBlinking( void );

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
	// Check if button is mouse clickable
	virtual bool IsMouseClickEnabled( MouseCode code );
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

    virtual bool CanBeDefaultButton(void);

	// Set this button to be the button that is accessed by default when the user hits ENTER or SPACE
	MESSAGE_FUNC_INT( SetAsDefaultButton, "SetAsDefaultButton", state );
	// Set this button to be the button that is currently accessed by default when the user hits ENTER or SPACE
	MESSAGE_FUNC_INT( SetAsCurrentDefaultButton, "SetAsCurrentDefaultButton", state );

	// Respond when key focus is received
	void OnSetFocus() override;
	// Respond when focus is killed
	void OnKillFocus() override;

	// Set button border attribute enabled, controls display of button.
	virtual void SetButtonBorderEnabled( bool state );

	// Set default button colors.
	virtual void SetDefaultColor(Color fgColor, Color bgColor);
	// Set armed button colors
	virtual void SetArmedColor(Color fgColor, Color bgColor);
	// Set selected button colors
	virtual void SetSelectedColor(Color fgColor, Color bgColor);
	// Set depressed button colors
	virtual void SetDepressedColor(Color fgColor, Color bgColor);
	// Set blink button color
	virtual void SetBlinkColor(Color fgColor);

	// Get button foreground color
	virtual Color GetButtonFgColor();
	// Get button background color
	virtual Color GetButtonBgColor();

	Color		  GetButtonDefaultFgColor() const { return _defaultFgColor; }
	Color		  GetButtonDefaultBgColor() const { return _defaultBgColor; }

	Color		  GetButtonArmedFgColor() const { return _armedFgColor; }
	Color		  GetButtonArmedBgColor() const { return _armedBgColor; }

	Color		  GetButtonSelectedFgColor() const { return _selectedFgColor; }
	Color		  GetButtonSelectedBgColor() const { return _selectedBgColor; }

	Color		  GetButtonDepressedFgColor() const { return _depressedFgColor; }
	Color		  GetButtonDepressedBgColor() const { return _depressedBgColor; }

	// Set default button border attributes.
	virtual void SetDefaultBorder(IBorder *border);
	// Set depressed button border attributes.
	virtual void SetDepressedBorder(IBorder *border);
	// Set key focused button border attributes.
	virtual void SetKeyFocusBorder(IBorder *border);

	// Set the command to send when the button is pressed
	// Set the panel to send the command to with AddActionSignalTarget()
	virtual void SetCommand( const char *command );
	// Set the message to send when the button is pressed
	virtual void SetCommand( KeyValues *message );

	// sound handling
	void SetArmedSound(const char *sound);
	void SetDepressedSound(const char *sound);
	void SetReleasedSound(const char *sound);

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

	bool ShouldPaint(){ return _paint; }
	void SetShouldPaint( bool paint ){ _paint = paint; }

	void ApplySettings( KeyValues *inResourceData ) override;
	void NavigateTo() override;
	void NavigateFrom() override;

protected:
	virtual void DrawFocusBorder(int tx0, int ty0, int tx1, int ty1);

	// Paint button on screen
	void Paint() override;
	// Get button border attributes.
	virtual IBorder *GetBorder(bool depressed, bool armed, bool selected, bool keyfocus);

	void ApplySchemeSettings(IScheme *pScheme) override;
	MESSAGE_FUNC_INT( OnSetState, "SetState", state );
	
	void OnMousePressed(MouseCode code) override;
	void OnMouseDoublePressed(MouseCode code) override;
	void OnMouseReleased(MouseCode code) override;
	void OnKeyCodePressed(KeyCode code) override;
	void OnKeyCodeReleased(KeyCode code) override;

	// Get control settings for editing
	void GetSettings( KeyValues *outResourceData ) override;
	const char *GetDescription( void ) override;

	KeyValues *GetActionMessage();
	void PlayButtonReleasedSound();

protected:
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

	IBorder			  *_defaultBorder;
	IBorder			  *_depressedBorder;
	IBorder			  *_keyFocusBorder;

	Color			   _defaultFgColor, _defaultBgColor;
	Color			   _armedFgColor, _armedBgColor;
	Color			   _selectedFgColor, _selectedBgColor;
	Color              _depressedFgColor, _depressedBgColor;
	Color              _keyboardFocusColor;
	Color			   _blinkFgColor;

	bool				_paint;

	unsigned short	   m_sArmedSoundName, m_sDepressedSoundName, m_sReleasedSoundName;
	bool m_bSelectionStateSaved;
	bool m_bStaySelectedOnClick;
	bool m_bStayArmedOnClick;
};

} // namespace vgui

#endif // BUTTON_H
