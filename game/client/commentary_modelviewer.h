//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef COMMENTARY_MODELVIEWER_H
#define COMMENTARY_MODELVIEWER_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <game/client/iviewport.h>
#include "basemodelpanel.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCommentaryModelPanel : public CModelPanel
{
public:
	DECLARE_CLASS_SIMPLE_OVERRIDE( CCommentaryModelPanel, CModelPanel );

	CCommentaryModelPanel( vgui::Panel *parent, const char *name );
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCommentaryModelViewer : public vgui::Frame, public IViewPortPanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CCommentaryModelViewer, vgui::Frame );
public:
	CCommentaryModelViewer(IViewPort *pViewPort);
	virtual ~CCommentaryModelViewer();

	void	ApplySchemeSettings( vgui::IScheme *pScheme ) override;
	void	PerformLayout( void ) override;
	void	OnCommand( const char *command ) override;
	void	OnKeyCodePressed( vgui::KeyCode code ) override;
	void	OnThink( void ) override;

	void			SetModel( const char *pszName, const char *pszAttached );

	void			HandleMovementInput( void );

	// IViewPortPanel
public:
	const char *GetName( void ) override { return PANEL_COMMENTARY_MODELVIEWER; }
	void SetData(KeyValues *data) override {};
	void Reset() override {};
	void Update() override {};
	bool NeedsUpdate( void ) override { return false; }
	bool HasInputElements( void ) override { return true; }
	void ShowPanel( bool bShow ) override;

	// both vgui::Frame and IViewPortPanel define these, so explicitly define them here as passthroughs to vgui
	vgui::VPANEL GetVPanel( void ) override { return BaseClass::GetVPanel(); }
	bool IsVisible() override { return BaseClass::IsVisible(); }
	void SetParent( vgui::VPANEL parent ) override { BaseClass::SetParent( parent ); }

	GameActionSet_t GetPreferredActionSet() override { return GAME_ACTION_SET_MENUCONTROLS; }

private:
	IViewPort				*m_pViewPort;
	CCommentaryModelPanel	*m_pModelPanel;

	Vector					m_vecResetPos;
	Vector					m_vecResetAngles;
	bool					m_bTranslating;
	float					m_flYawSpeed;
	float					m_flZoomSpeed;
};

#endif // COMMENTARY_MODELVIEWER_H
