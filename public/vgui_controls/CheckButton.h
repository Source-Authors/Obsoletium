//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CHECKBUTTON_H
#define CHECKBUTTON_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/ToggleButton.h>
#include <vgui_controls/TextImage.h>

namespace vgui
{

class CheckImage;
class TextImage;

//-----------------------------------------------------------------------------
// Purpose: Tick-box button
//-----------------------------------------------------------------------------
class CheckButton : public ToggleButton
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CheckButton, ToggleButton );

public:
	CheckButton(Panel *parent, const char *panelName, const char *text);
	~CheckButton();

	// Check the button
	void SetSelected(bool state ) override;

	// sets whether or not the state of the check can be changed
	// if this is set to false, then no input in the code or by the user can change it's state
	virtual void SetCheckButtonCheckable(bool state);
	virtual bool IsCheckButtonCheckable() const { return m_bCheckButtonCheckable; }

	Color GetDisabledFgColor() const { return _disabledFgColor; }
	Color GetDisabledBgColor() const { return _disabledBgColor; }

	CheckImage *GetCheckImage() { return _checkBoxImage; }

	virtual void SetHighlightColor(Color fgColor);
	void ApplySettings( KeyValues *inResourceData ) override;

protected:
	void ApplySchemeSettings(IScheme *pScheme) override;
	MESSAGE_FUNC_PTR( OnCheckButtonChecked, "CheckButtonChecked", panel );
	Color GetButtonFgColor() override;

	IBorder *GetBorder(bool depressed, bool armed, bool selected, bool keyfocus) override;

	/* MESSAGES SENT
		"CheckButtonChecked" - sent when the check button state is changed
			"state"	- button state: 1 is checked, 0 is unchecked
	*/


private:
	enum { CHECK_INSET = 6 };
	bool m_bCheckButtonCheckable;
	bool m_bUseSmallCheckImage;
	CheckImage *_checkBoxImage;
	Color _disabledFgColor;
	Color _disabledBgColor;
	Color _highlightFgColor;
};

//-----------------------------------------------------------------------------
// Purpose: Check box image
//-----------------------------------------------------------------------------
class CheckImage : public TextImage
{
public:
	CheckImage(CheckButton *CheckButton) : TextImage( "g" )
	{
		_CheckButton = CheckButton;

		// dimhotepus: Scale UI.
		SetSize(QuickPropScalePanel( 20, CheckButton ), QuickPropScalePanel( 13, CheckButton ));
	}

	void Paint() override;

	void SetColor(Color color) override
	{
		_borderColor1 = color;
		_borderColor2 = color;
		_checkColor = color;
	}

	Color _borderColor1;
	Color _borderColor2;
	Color _checkColor;

	Color _bgColor;

private:
	CheckButton *_CheckButton;
};


} // namespace vgui

#endif // CHECKBUTTON_H
