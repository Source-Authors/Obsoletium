//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SPECTATORGUI_H
#define SPECTATORGUI_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/IScheme.h>
#include <vgui/KeyCode.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ComboBox.h>
#include <igameevents.h>
#include "GameEventListener.h"

#include <game/client/iviewport.h>

class KeyValues;

namespace vgui
{
	class TextEntry;
	class Button;
	class Panel;
	class ImagePanel;
	class ComboBox;
}

#define BLACK_BAR_COLOR	Color(0, 0, 0, 196)

class IBaseFileSystem;

//-----------------------------------------------------------------------------
// Purpose: Spectator UI
//-----------------------------------------------------------------------------
class CSpectatorGUI : public vgui::EditablePanel, public IViewPortPanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CSpectatorGUI, vgui::EditablePanel );

public:
	CSpectatorGUI( IViewPort *pViewPort );
	virtual ~CSpectatorGUI();

	const char *GetName( void ) override { return PANEL_SPECGUI; }
	void SetData(KeyValues *) override {};
	void Reset() override {};
	void Update() override;
	bool NeedsUpdate( void ) override { return false; }
	bool HasInputElements( void ) override { return false; }
	void ShowPanel( bool bShow ) override;
	
	// both vgui::Frame and IViewPortPanel define these, so explicitly define them here as passthroughs to vgui
	vgui::VPANEL GetVPanel( void ) override { return BaseClass::GetVPanel(); }
	bool IsVisible() override { return BaseClass::IsVisible(); }
	void SetParent(vgui::VPANEL parent) override { BaseClass::SetParent(parent); }
	void OnThink() override;

	virtual int GetTopBarHeight() { return m_pTopBar->GetTall(); }
	virtual int GetBottomBarHeight() { return m_pBottomBarBlank->GetTall(); }
	
	virtual bool ShouldShowPlayerLabel( int specmode );

	virtual Color GetBlackBarColor( void ) { return BLACK_BAR_COLOR; }

	virtual const char *GetResFile( void );

	GameActionSet_t GetPreferredActionSet() override { return GAME_ACTION_SET_SPECTATOR; }
	
protected:

	void SetLabelText(const char *textEntryName, const char *text);
	void SetLabelText(const char *textEntryName, wchar_t *text);
	void MoveLabelToFront(const char *textEntryName);
	void UpdateTimer();
	void SetLogoImage(const char *image);

protected:	
	enum { INSET_OFFSET = 2 } ; 

	// vgui overrides
	void PerformLayout() override;
	void ApplySchemeSettings(vgui::IScheme *pScheme) override;
//	virtual void OnCommand( const char *command );

	vgui::Panel *m_pTopBar;
	vgui::Panel *m_pBottomBarBlank;

	vgui::ImagePanel *m_pBannerImage;
	vgui::Label *m_pPlayerLabel;

	IViewPort *m_pViewPort;

	// bool m_bHelpShown;
	// bool m_bInsetVisible;
	bool m_bSpecScoreboard;
};


//-----------------------------------------------------------------------------
// Purpose: the bottom bar panel, this is a separate panel because it
// wants mouse input and the main window doesn't
//----------------------------------------------------------------------------
class CSpectatorMenu : public vgui::Frame, public IViewPortPanel, public CGameEventListener
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(  CSpectatorMenu, vgui::Frame );

public:
	CSpectatorMenu( IViewPort *pViewPort );
	~CSpectatorMenu() {}

	const char *GetName( void ) override { return PANEL_SPECMENU; }
	void SetData(KeyValues *data) override {};
	void Reset( void ) override { m_pPlayerList->DeleteAllItems(); }
	void Update( void ) override;
	bool NeedsUpdate( void ) override { return false; }
	bool HasInputElements( void ) override { return true; }
	void ShowPanel( bool bShow ) override;
	void FireGameEvent( IGameEvent *event ) override;

	// both vgui::Frame and IViewPortPanel define these, so explicitly define them here as passthroughs to vgui
	bool IsVisible() override { return BaseClass::IsVisible(); }
	vgui::VPANEL GetVPanel( void ) override { return BaseClass::GetVPanel(); }
	void SetParent(vgui::VPANEL parent) override { BaseClass::SetParent(parent); }

	GameActionSet_t GetPreferredActionSet() override { return GAME_ACTION_SET_SPECTATOR; }

private:
	// VGUI2 overrides
	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );
	void OnCommand( const char *command ) override;
	void OnKeyCodePressed(vgui::KeyCode code) override;
	void ApplySchemeSettings(vgui::IScheme *pScheme) override;
	void PerformLayout() override;

	void SetViewModeText( const char *text ) { m_pViewOptions->SetText( text ); }
	void SetPlayerFgColor( Color c1 ) { m_pPlayerList->SetFgColor(c1); }

	vgui::ComboBox *m_pPlayerList;
	vgui::ComboBox *m_pViewOptions;
	vgui::ComboBox *m_pConfigSettings;

	vgui::Button *m_pLeftButton;
	vgui::Button *m_pRightButton;

	IViewPort *m_pViewPort;
	ButtonCode_t m_iDuckKey;
};

extern CSpectatorGUI * g_pSpectatorGUI;

#endif // SPECTATORGUI_H
