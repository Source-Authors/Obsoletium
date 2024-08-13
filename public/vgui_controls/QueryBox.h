//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Creates a Message box with a question in it and yes/no buttons
//
// $NoKeywords: $
//=============================================================================//

#ifndef QUERYBOX_H
#define QUERYBOX_H

#ifdef _WIN32
#pragma once
#endif

#include <KeyValues.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/Button.h>

namespace vgui
{

//-----------------------------------------------------------------------------
// Purpose: Creates A Message box with a question in it and yes/no buttons
//-----------------------------------------------------------------------------
class QueryBox : public MessageBox
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( QueryBox, MessageBox );

public:
	QueryBox(const char *title, const char *queryText,vgui::Panel *parent = NULL );
	QueryBox(const wchar_t *wszTitle, const wchar_t *wszQueryText,vgui::Panel *parent = NULL);
	~QueryBox();

	// Layout the window for drawing 
	void PerformLayout() override;

	// Set the keyvalues to send when ok button is hit
	void SetOKCommand(KeyValues *keyValues);

	// Set the keyvalues to send when the cancel button is hit
	void SetCancelCommand(KeyValues *keyValues);

	// Set the text on the Cancel button
	void SetCancelButtonText(const char *buttonText);
	void SetCancelButtonText(const wchar_t *wszButtonText);

	// Set a value of the ok command
	void SetOKCommandValue(const char *keyName, int value);

protected:
	void OnKeyCodeTyped( KeyCode code ) override;
	void OnKeyCodePressed( KeyCode code ) override;
	void OnCommand(const char *command) override;
	Button		*m_pCancelButton;

private:
	KeyValues	*m_pCancelCommand;
	KeyValues	*m_pOkCommand;
};

}
#endif // QUERYBOX_H
