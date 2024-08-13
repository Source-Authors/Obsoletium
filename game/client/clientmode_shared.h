//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#if !defined( CLIENTMODE_NORMAL_H )
#define CLIENTMODE_NORMAL_H
#ifdef _WIN32
#pragma once
#endif

#include "iclientmode.h"
#include "GameEventListener.h"
#include <baseviewport.h>

class CBaseHudChat;
class CBaseHudWeaponSelection;
class CViewSetup;
class C_BaseEntity;
class C_BasePlayer;

namespace vgui
{
class Panel;
}

//=============================================================================
// HPE_BEGIN:
// [tj] Moved this from the .cpp file so derived classes could access it
//=============================================================================
 
#define ACHIEVEMENT_ANNOUNCEMENT_MIN_TIME 10
 
//=============================================================================
// HPE_END
//=============================================================================

class CReplayReminderPanel;

#define USERID2PLAYER(i) ToBasePlayer( ClientEntityList().GetEnt( engine->GetPlayerForUserID( i ) ) )	

extern IClientMode *GetClientModeNormal(); // must be implemented

// This class implements client mode functionality common to HL2 and TF2.
class ClientModeShared : public IClientMode, public CGameEventListener
{
// IClientMode overrides.
public:
	DECLARE_CLASS_NOBASE( ClientModeShared );

					ClientModeShared();
	virtual			~ClientModeShared();
	
	void	Init() override;
	void	InitViewport() override;
	void	VGui_Shutdown() override;
	void	Shutdown() override;

	void	LevelInit( const char *newmap ) override;
	void	LevelShutdown( void ) override;

	void	Enable() override;
	void	Disable() override;
	void	Layout() override;

	virtual void	ReloadScheme( bool flushLowLevel );
	void	OverrideView( CViewSetup *pSetup ) override;
	bool	ShouldDrawDetailObjects( ) override;
	bool	ShouldDrawEntity(C_BaseEntity *pEnt) override;
	bool	ShouldDrawLocalPlayer( C_BasePlayer *pPlayer ) override;
	bool	ShouldDrawViewModel() override;
	bool	ShouldDrawParticles( ) override;
	bool	ShouldDrawCrosshair( void ) override;
	bool	ShouldBlackoutAroundHUD() override;
	HeadtrackMovementMode_t ShouldOverrideHeadtrackControl() override;
	void	AdjustEngineViewport( int& x, int& y, int& width, int& height ) override;
	void	PreRender(CViewSetup *pSetup) override;
	void	PostRender() override;
	void	PostRenderVGui() override;
	void	ProcessInput(bool bActive) override;
	bool	CreateMove( float flInputSampleTime, CUserCmd *cmd ) override;
	void	Update() override;

	// Input
	int		KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding ) override;
	virtual int		HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );
	void	OverrideMouseInput( float *x, float *y ) override;
	void	StartMessageMode( int iMessageModeType ) override;
	vgui::Panel *GetMessagePanel() override;

	void	ActivateInGameVGuiContext( vgui::Panel *pPanel ) override;
	void	DeactivateInGameVGuiContext() override;

	// The mode can choose to not draw fog
	bool	ShouldDrawFog( void ) override;
	
	float	GetViewModelFOV( void ) override;
	vgui::Panel* GetViewport() override { return m_pViewport; }
	// Gets at the viewports vgui panel animation controller, if there is one...
	vgui::AnimationController *GetViewportAnimationController() override
		{ return m_pViewport->GetAnimationController(); }
	
	void FireGameEvent( IGameEvent *event ) override;

	bool CanRecordDemo( char *errorMsg, int length ) const override { return true; }

	virtual int HandleSpectatorKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );

	void ComputeVguiResConditions( KeyValues *pkvConditions ) override;

	//=============================================================================
	// HPE_BEGIN:
	// [menglish] Save server information shown to the client in a persistent place
	//=============================================================================
	 
	wchar_t* GetServerName() override { return NULL; }
	void SetServerName(wchar_t* ) override {}
	wchar_t* GetMapName() override { return NULL; }
	void SetMapName(wchar_t* ) override {}
	 
	//=============================================================================
	// HPE_END
	//=============================================================================

	bool	DoPostScreenSpaceEffects( const CViewSetup *pSetup ) override;

	void	DisplayReplayMessage( const char *pLocalizeName, float flDuration, bool bUrgent,
										  const char *pSound, bool bDlg ) override;

	bool	IsInfoPanelAllowed() override { return true; }
	void	InfoPanelDisplayed() override { }
	bool	IsHTMLInfoPanelAllowed() override { return true; }

	bool	IsAnyPanelVisibleExceptScores() { return m_pViewport->IsAnyPanelVisibleExceptScores(); }
	bool	IsPanelVisible( const char* panel ) { return m_pViewport->IsPanelVisible( panel ); }

	void			OnDemoRecordStart( char const* pDemoBaseName ) override {}
	void			OnDemoRecordStop() override {}

protected:
	CBaseViewport			*m_pViewport;

	void			DisplayReplayReminder();

private:
	virtual void	UpdateReplayMessages();

	void			ClearReplayMessageList();

#if defined( REPLAY_ENABLED )
	float					m_flReplayStartRecordTime;
	float					m_flReplayStopRecordTime;
	CReplayReminderPanel	*m_pReplayReminderPanel;
#endif

	// Message mode handling
	// All modes share a common chat interface
	CBaseHudChat			*m_pChatElement;
	vgui::HCursor			m_CursorNone;
	CBaseHudWeaponSelection *m_pWeaponSelection;
	int						m_nRootSize[2];
};

#endif // CLIENTMODE_NORMAL_H

