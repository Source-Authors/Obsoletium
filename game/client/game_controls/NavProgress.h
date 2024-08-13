//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef NAVPROGRESS_H
#define NAVPROGRESS_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/ProgressBar.h>

#include <game/client/iviewport.h>

class CNavProgress : public vgui::Frame, public IViewPortPanel
{
private:
	DECLARE_CLASS_SIMPLE_OVERRIDE( CNavProgress, vgui::Frame );

public:
	CNavProgress(IViewPort *pViewPort);
	virtual ~CNavProgress();

	const char *GetName( void ) override { return PANEL_NAV_PROGRESS; }
	void SetData(KeyValues *data) override;
	void Reset() override;
	void Update() override;
	bool NeedsUpdate( void ) override { return false; }
	bool HasInputElements( void ) override { return true; }
	void ShowPanel( bool bShow ) override;

	// both vgui::Frame and IViewPortPanel define these, so explicitly define them here as passthroughs to vgui
	vgui::VPANEL GetVPanel( void ) override { return BaseClass::GetVPanel(); }
  	bool IsVisible() override { return BaseClass::IsVisible(); }
  	void SetParent( vgui::VPANEL parent ) override { BaseClass::SetParent( parent ); }

	GameActionSet_t GetPreferredActionSet() override { return GAME_ACTION_SET_NONE; }

public:

	void ApplySchemeSettings( vgui::IScheme *pScheme ) override;
	void PerformLayout() override;
	void Init( const char *title, int numTicks, int currentTick );

protected:
	IViewPort	*m_pViewPort;

	int		m_numTicks;
	int		m_currentTick;

	vgui::Label *			m_pTitle;
	vgui::Label *			m_pText;
	vgui::Panel *			m_pProgressBarBorder;
	vgui::Panel *			m_pProgressBar;
	vgui::Panel *			m_pProgressBarSizer;
};

#endif // NAVPROGRESS_H
