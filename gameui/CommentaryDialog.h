//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef COMMENTARYDIALOG_H
#define COMMENTARYDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"
#include "utlvector.h"
#include <vgui/KeyCode.h>

class CGameChapterPanel;
class CSkillSelectionDialog;

//-----------------------------------------------------------------------------
// Purpose: Handles selection of commentary mode on/off
//-----------------------------------------------------------------------------
class CCommentaryDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CCommentaryDialog, vgui::Frame );

public:
	CCommentaryDialog(vgui::Panel *parent);
	~CCommentaryDialog();

	void OnClose( void ) override;
	void OnCommand( const char *command ) override;
	void OnKeyCodePressed(vgui::KeyCode code) override;
};

//-----------------------------------------------------------------------------
// Purpose: Small dialog to remind players on method of changing commentary mode
//-----------------------------------------------------------------------------
class CPostCommentaryDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CPostCommentaryDialog, vgui::Frame );

public:
	CPostCommentaryDialog(vgui::Panel *parent);
	~CPostCommentaryDialog();

	void OnFinishedClose( void ) override;
	void OnKeyCodeTyped(vgui::KeyCode code) override;
	void OnKeyCodePressed(vgui::KeyCode code) override;

private:
	bool m_bResetPaintRestrict;
};

#endif // COMMENTARYDIALOG_H
