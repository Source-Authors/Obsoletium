//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef RADIOBUTTON_H
#define RADIOBUTTON_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/ToggleButton.h>
#include <vgui_controls/TextImage.h>

namespace vgui
{

//-----------------------------------------------------------------------------
// Purpose: Radio buttons are automatically selected into groups by who their
//			parent is. At most one radio button is active at any time.
//-----------------------------------------------------------------------------
class RadioButton : public ToggleButton
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( RadioButton, ToggleButton );

public:
	RadioButton(Panel *parent, const char *panelName, const char *text);
	~RadioButton();

	// Set the radio button checked. When a radio button is checked, a 
	// message is sent to all other radio buttons in the same group so
	// they will become unchecked.
	void SetSelected(bool state) override;

	// Get the tab position of the radio button with the set of radio buttons
	// A group of RadioButtons must have the same TabPosition, with [1, n] subtabpositions
	virtual int GetSubTabPosition();
	virtual void SetSubTabPosition(int position);

	// Return the RadioButton's real tab position (its Panel one changes)
	virtual int GetRadioTabPosition();

	// Set the selection state of the radio button, but don't fire
	// any action signals or messages to other radio buttons
	virtual void SilentSetSelected(bool state);

protected:
	void DoClick() override;

	void Paint() override;
	void ApplySchemeSettings(IScheme *pScheme) override;
	MESSAGE_FUNC_INT( OnRadioButtonChecked, "RadioButtonChecked", tabposition);
	void OnKeyCodeTyped(KeyCode code) override;

	IBorder *GetBorder(bool depressed, bool armed, bool selected, bool keyfocus) override;

	void ApplySettings(KeyValues *inResourceData) override;
	void GetSettings(KeyValues *outResourceData) override;
	const char *GetDescription() override;
	void PerformLayout() override;

	RadioButton *FindBestRadioButton(int direction);

private:
	class RadioImage *_radioBoxImage;
	int _oldTabPosition;
	Color _selectedFgColor;

	int _subTabPosition;	// tab position with the radio button list

	void InternalSetSelected(bool state, bool bFireEvents);
};


//-----------------------------------------------------------------------------
// Purpose: Radio buton image
//-----------------------------------------------------------------------------
// dimhotepus: Moved after RadioButton to scale UI.
class RadioImage : public TextImage
{
public:
	RadioImage(RadioButton *radioButton) : TextImage( "n" )
	{
		_radioButton = radioButton;

		// dimhotepus: Scale UI.
		SetSize(QuickPropScalePanel( 20, radioButton ), QuickPropScalePanel( 13, radioButton ));
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
	RadioButton *_radioButton;
};

}; // namespace vgui

#endif // RADIOBUTTON_H
