//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef TOGGLEBUTTON_H
#define TOGGLEBUTTON_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Button.h>

namespace vgui
{

//-----------------------------------------------------------------------------
// Purpose: Type of button that when pressed stays selected & depressed until pressed again
//-----------------------------------------------------------------------------
class ToggleButton : public Button
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( ToggleButton, Button );

public:
	ToggleButton(Panel *parent, const char *panelName, const char *text);

	void DoClick() override;

	/* messages sent (get via AddActionSignalTarget()):
		"ButtonToggled"
			int "state"
	*/

protected:
	// overrides
	void OnMouseDoublePressed(MouseCode code) override;

	Color GetButtonFgColor() override;
	void ApplySchemeSettings(IScheme *pScheme) override;

	bool CanBeDefaultButton(void) override;
	void OnKeyCodePressed(KeyCode code) override;

private:
	Color _selectedColor;
};

} // namespace vgui

#endif // TOGGLEBUTTON_H
