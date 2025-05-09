//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VAREDITDIALOG_H
#define VAREDITDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

//-----------------------------------------------------------------------------
// Purpose: Handles editing of cvars
//-----------------------------------------------------------------------------
class CVarEditDialog : public vgui::Frame
{
public:
	CVarEditDialog(vgui::Panel *parent, const char *name);
	virtual ~CVarEditDialog();

	void Activate(vgui::Panel *actionSignalTarget, const KeyValues *rules);

protected:
	void OnCommand(const char *command) override;
	void OnClose() override;

	void ApplyChanges();

private:
	vgui::Button *m_pOKButton;
	vgui::Button *m_pCancelButton;
	vgui::TextEntry *m_pStringEdit;
	vgui::ComboBox *m_pComboEdit;

	KeyValues *m_pRules;

	typedef vgui::Frame BaseClass;
};


#endif // VAREDITDIALOG_H
