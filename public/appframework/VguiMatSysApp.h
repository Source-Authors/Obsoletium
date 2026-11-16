//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// Material editor
//=============================================================================

#ifndef VGUIMATSYSAPP_H
#define VGUIMATSYSAPP_H

#include "appframework/tier3app.h"


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CVguiMatSysApp : public CVguiSteamApp
{
	using BaseClass = CVguiSteamApp;

public:
	CVguiMatSysApp() = default;

	// Methods of IApplication
	bool Create() override;
	bool PreInit() override;
	void PostShutdown() override;
	void Destroy() override;

	// Returns the window handle (HWND in Win32, SDL_Window* on SDL platforms)
	[[nodiscard]] void* GetAppWindow();

	// Gets the window size
	[[nodiscard]] int GetWindowWidth() const;
	[[nodiscard]] int GetWindowHeight() const;

protected:
	void AppPumpMessages();

	// Sets the video mode
	bool SetVideoMode( );

	// Sets up the game path
	// NOTE: Intentionally hides CSteamAppSystemGroup::SetupSearchPaths (non-virtual).
	bool SetupSearchPaths( const char *pStartingDir, bool bOnlyUseStartingDir, bool bIsTool );

private:
	// Returns the app name
	virtual const char *GetAppName() = 0;
	virtual bool AppUsesReadPixels() { return false; }

	// Creates the app window
	virtual void *CreateAppWindow( char const *pTitle, bool bWindowed, int w, int h );

	// dimhotepus: Destroys the window created by CreateAppWindow (no-op when launcher manager owns it).
	void DestroyAppWindow();

	void *m_HWnd{nullptr};
	int m_nWidth{-1};
	int m_nHeight{-1};
};


#endif // VGUIMATSYSAPP_H