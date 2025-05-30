//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef SERVERCONFIGPANEL_H
#define SERVERCONFIGPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "VarListPropertyPage.h"

//-----------------------------------------------------------------------------
// Purpose: Displays and allows modification to variable specific to the game
//-----------------------------------------------------------------------------
class CServerConfigPanel : public CVarListPropertyPage
{
public:
	CServerConfigPanel(vgui::Panel *parent, const char *name, const char *mod);
	virtual ~CServerConfigPanel();

protected:
	// variable updates
	void OnResetData() override;
	void OnThink() override;

private:
	// dimhotepus: float -> double.
	double m_flUpdateTime;
	typedef CVarListPropertyPage BaseClass;
};

#endif // SERVERCONFIGPANEL_H