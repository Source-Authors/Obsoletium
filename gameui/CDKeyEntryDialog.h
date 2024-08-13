//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CDKEYENTRYDIALOG_H
#define CDKEYENTRYDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCDKeyEntryDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CCDKeyEntryDialog, vgui::Frame );

public:
	CCDKeyEntryDialog(vgui::Panel *parent, bool inConnect = false);
	~CCDKeyEntryDialog();

	void Activate() override;

	static bool IsValidWeakCDKeyInRegistry();

private:
	enum { MAX_CDKEY_ERRORS = 5 };

	void OnCommand(const char *command) override;
	void OnClose() override;
	void OnThink() override;

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	bool IsEnteredKeyValid();

	vgui::Button *m_pOK;
	vgui::Button *m_pQuitGame;
	vgui::TextEntry *m_pEntry1;
	vgui::TextEntry *m_pEntry2;
	vgui::TextEntry *m_pEntry3;
	vgui::TextEntry *m_pEntry4;
	vgui::TextEntry *m_pEntry5;

	vgui::DHANDLE<vgui::MessageBox> m_hErrorBox;

	bool m_bEnteredValidCDKey;

	bool m_bInConnect;
	int m_iErrCount; 
};


#endif // CDKEYENTRYDIALOG_H
