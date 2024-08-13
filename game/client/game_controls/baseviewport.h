//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TEAMFORTRESSVIEWPORT_H
#define TEAMFORTRESSVIEWPORT_H

// viewport interface for the rest of the dll
#include <game/client/iviewport.h>

#include <utlqueue.h> // a vector based queue template to manage our VGUI menu queue
#include <vgui_controls/Frame.h>
#include "vguitextwindow.h"
#include "vgui/ISurface.h"
#include "commandmenu.h"
#include <igameevents.h>

using namespace vgui;

class IBaseFileSystem;
class IGameUIFuncs;
class IGameEventManager;

//==============================================================================
class CBaseViewport : public vgui::EditablePanel, public IViewPort, public IGameEventListener2
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CBaseViewport, vgui::EditablePanel );

public: 
	CBaseViewport();
	virtual ~CBaseViewport();

	virtual IViewPortPanel* CreatePanelByName(const char *szPanelName);
	IViewPortPanel* FindPanelByName(const char *szPanelName) override;
	IViewPortPanel* GetActivePanel( void ) override;
	virtual void RemoveAllPanels( void);

	void ShowPanel( const char *pName, bool state ) override;
	void ShowPanel( IViewPortPanel* pPanel, bool state ) override;
	virtual bool AddNewPanel( IViewPortPanel* pPanel, char const *pchDebugName );
	virtual void CreateDefaultPanels( void );
	void UpdateAllPanels( void ) override;
	void PostMessageToPanel( const char *pName, KeyValues *pKeyValues ) override;

	virtual void Start( IGameUIFuncs *pGameUIFuncs, IGameEventManager2 *pGameEventManager );
	void SetParent(vgui::VPANEL parent) override;

	virtual void ReloadScheme(const char *fromFile);
	virtual void ActivateClientUI();
	virtual void HideClientUI();
	virtual bool AllowedToPrintText( void );
	
#ifndef _XBOX
	virtual int GetViewPortScheme() { return m_pBackGround->GetScheme(); }
	virtual VPANEL GetViewPortPanel() { return m_pBackGround->GetVParent(); }
#endif
	virtual AnimationController *GetAnimationController() { return m_pAnimController; }

	void ShowBackGround(bool bShow) override
	{ 
#ifndef _XBOX
		m_pBackGround->SetVisible( bShow ); 
#endif
	}

	virtual int GetDeathMessageStartHeight( void );	

	// virtual void ChatInputPosition( int *x, int *y );

	// Check if any panel other than the scoreboard is visible
	virtual bool IsAnyPanelVisibleExceptScores();

	// Walk through all the panels. Handler should be an object taking an IViewPortPanel*
	template<typename THandler> void ForEachPanel( THandler handler )
	{
		FOR_EACH_VEC( m_Panels, i )
		{
			handler( m_Panels[i] );
		}
	}

	// Check if the named panel is visible
	virtual bool IsPanelVisible( const char* panel );
	
public: // IGameEventListener:
	void FireGameEvent( IGameEvent * event) override;


protected:

	bool LoadHudAnimations( void );

#ifndef _XBOX
	class CBackGroundPanel : public vgui::Frame
	{
	private:
		typedef vgui::Frame BaseClass;
	public:
		CBackGroundPanel( vgui::Panel *parent) : Frame( parent, "ViewPortBackGround" ) 
		{
			SetScheme("ClientScheme");

			SetTitleBarVisible( false );
			SetMoveable(false);
			SetSizeable(false);
			SetProportional(true);
		}
	private:

		void ApplySchemeSettings(IScheme *pScheme) override
		{
			BaseClass::ApplySchemeSettings(pScheme);
			SetBgColor(pScheme->GetColor("ViewportBG", Color( 0,0,0,0 ) )); 
		}

		void PerformLayout() override
		{
			int w,h;
			GetHudSize(w, h);

			// fill the screen
			SetBounds(0,0,w,h);

			BaseClass::PerformLayout();
		}

		void OnMousePressed(MouseCode code) override { }// don't respond to mouse clicks
		vgui::VPANEL IsWithinTraverse( int x, int y, bool traversePopups ) override
		{
			return ( vgui::VPANEL )0;
		}

	};
#endif
protected:

	void Paint() override;
	void OnThink() override; 
	void OnScreenSizeChanged(int iOldWide, int iOldTall) override;
	void PostMessageToPanel( IViewPortPanel* pPanel, KeyValues *pKeyValues );

protected:
	IGameUIFuncs*		m_GameuiFuncs; // for key binding details
	IGameEventManager2*	m_GameEventManager;
#ifndef _XBOX
	CBackGroundPanel	*m_pBackGround;
#endif
	CUtlVector<IViewPortPanel*> m_Panels;
	
	bool				m_bHasParent; // Used to track if child windows have parents or not.
	bool				m_bInitialized;
	IViewPortPanel		*m_pActivePanel;
	IViewPortPanel		*m_pLastActivePanel;
	vgui::HCursor		m_hCursorNone;
	vgui::AnimationController *m_pAnimController;
	int					m_OldSize[2];
};


#endif
