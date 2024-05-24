//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//
#if defined( USE_SDL )
#undef PROTECTED_THINGS_ENABLE
#include "include/SDL3/SDL.h"
#include "include/SDL3/SDL_syswm.h"

#if defined( OSX )
#define DONT_DEFINE_BOOL
#include <objc/message.h>
#endif

#endif

#if defined( WIN32 ) && !defined( DX_TO_GL_ABSTRACTION )
#include "winlite.h"
#include <imm.h>
// dimhotepus: Disable accessibility keys (ex. five times Shift).
#include "accessibility_shortcut_keys_toggler.h"
// dimhotepus: Disable Win key.
#include "windows_shortcut_keys_toggler.h"
#endif

#if defined( IS_WINDOWS_PC ) && !defined( USE_SDL )
	#include <winsock.h>
#elif defined(OSX)
#elif defined(LINUX)
	#include "tier0/dynfunction.h"
#elif defined(_WIN32)
	#include "tier0/dynfunction.h"
#else
	#error "Please define your platform"
#endif
#include "appframework/ilaunchermgr.h"

#include "igame.h"
#include "cl_main.h"
#include "host.h"
#include "quakedef.h"
#include "tier0/vcrmode.h"
#include "tier0/icommandline.h"
#include "ivideomode.h"
#include "gl_matsysiface.h"
#include "cdll_engine_int.h"
#include "vgui_baseui_interface.h"
#include "iengine.h"
#include "keys.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "tier3/tier3.h"
#include "sound.h"
#include "vgui_controls/Controls.h"
#include "vgui_controls/MessageDialog.h"
#include "sys_dll.h"
#include "inputsystem/iinputsystem.h"
#include "inputsystem/ButtonCode.h"
#include "GameUI/IGameUI.h"
#include "matchmaking.h"
#include "sv_main.h"
#include "video/ivideoservices.h"
#include "sys.h"
#include "materialsystem/imaterial.h"
#include "vaudio/ivaudio.h"

#if defined( LINUX )
  #include "snd_dev_sdl.h"
#endif


#ifdef DBGFLAG_ASSERT

  #define AssertExit( _exp )		Assert( _exp )
  #define AssertExitF( _exp )		Assert( _exp )

#else

  #define AssertExit( _exp )		if ( !( _exp ) ) return;
  #define AssertExitF( _exp )		if ( !( _exp ) ) return false;

#endif




// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void S_BlockSound();
void S_UnblockSound();
void ClearIOStates();

//-----------------------------------------------------------------------------
// Game input events
//-----------------------------------------------------------------------------
enum GameInputEventType_t
{
	IE_Close = IE_FirstAppEvent,
	IE_WindowMove,
	IE_AppActivated,
};

//-----------------------------------------------------------------------------
// Purpose: Main game interface, including message pump and window creation
//-----------------------------------------------------------------------------
class CGame : public IGame
{
public:
					CGame();
	virtual			~CGame();

	bool			Init( void *pvInstance );
	bool			Shutdown();

	bool			CreateGameWindow();
	void			DestroyGameWindow();
	void			SetGameWindow( void* hWnd );

	// This is used in edit mode to override the default wnd proc associated w/
	bool			InputAttachToGameWindow();
	void			InputDetachFromGameWindow();

	void			PlayStartupVideos();
	
	// This is the SDL_Window* under SDL, HWND otherwise.
	void*			GetMainWindow();
	// This will be the HWND under D3D + Windows (both with and without SDL), SDL_Window* everywhere else.
	void*			GetMainDeviceWindow();
	// This will be the HWND under Windows, the WindowRef under Mac, and (for now) NULL on Linux
	void*			GetMainWindowPlatformSpecificHandle();
	void**			GetMainWindowAddress();

	void			GetDesktopInfo( int &width, int &height, int &refreshrate );


	void			SetWindowXY( int x, int y );
	void			SetWindowSize( int w, int h );
	void			GetWindowRect( int *x, int *y, int *w, int *h );

	bool			IsActiveApp();

	void			SetCanPostActivateEvents( bool bEnable );
	bool			CanPostActivateEvents();

public:
#ifdef USE_SDL
	void			SetMainWindow( SDL_Window* window );
#else
#ifdef WIN32
	LRESULT				WindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
#endif
	void			SetMainWindow( HWND window );
#endif
	void			SetActiveApp( bool active );

// Message handlers.
public:
	void	HandleMsg_WindowMove( const InputEvent_t &event );
	void	HandleMsg_ActivateApp( const InputEvent_t &event );
	void	HandleMsg_Close( const InputEvent_t &event );

	// Call the appropriate HandleMsg_ function.
	void	DispatchInputEvent( const InputEvent_t &event );

	// Dispatch all the queued up messages.
	virtual void	DispatchAllStoredGameMessages();
private:
	void			AppActivate( bool fActive );
	void			PlayVideoAndWait( const char *filename, bool bNeedHealthWarning = false); // plays a video file and waits till it's done to return. Can be interrupted by user.
	
private:
	void AttachToWindow();
	void DetachFromWindow();

	static const wchar_t CLASSNAME[];

	bool			m_bExternallySuppliedWindow;

#if defined( WIN32 )
	HWND			m_hWindow;
	#if !defined( USE_SDL )
		HINSTANCE		m_hInstance;

		// Stores a wndproc to chain message calls to
		WNDPROC			m_ChainedWindowProc;

		RECT			m_rcLastRestoredClientRect;
	#endif
#endif

#if defined( USE_SDL )
	SDL_Window		*m_pSDLWindow;
#endif

	int				m_x;
	int				m_y;
	int				m_width;
	int				m_height;
	bool			m_bActiveApp;

	bool			m_bCanPostActivateEvents;
	int				m_iDesktopWidth, m_iDesktopHeight, m_iDesktopRefreshRate;

#if defined( IS_WINDOWS_PC )
	// dimhotepus: Disable Win key.
	source::engine::win::WindowsShortcutKeysToggler m_windowsShortcutKeysToggler;
	// dimhotepus: Disable accessibility keys (ex. five times Shift).
	source::engine::win::AccessibilityShortcutKeysToggler m_accessibilityShortcutKeysToggler;
#endif

	void			UpdateDesktopInformation();
#ifdef WIN32
	void			UpdateDesktopInformation( WPARAM wParam, LPARAM lParam );
#endif
};

static CGame g_Game;
IGame *game = &g_Game;

const wchar_t CGame::CLASSNAME[] = L"Valve001";

// In VCR playback mode, it sleeps this amount each frame.
int g_iVCRPlaybackSleepInterval = 0;

// During VCR playback, if this is true, then it'll pause at the end of each frame.
bool g_bVCRSingleStep = false;
// Used to prevent it from running frames while you hold the S key down.
bool g_bWaitingForStepKeyUp = false;

bool g_bShowVCRPlaybackDisplay = true;

// These are all the windows messages that can change game state.
// See CGame::WindowProc for a description of how they work.
struct GameMessageHandler_t
{
	int	m_nEventType;
	void (CGame::*pFn)( const InputEvent_t &event );
};

GameMessageHandler_t g_GameMessageHandlers[] = 
{
	{ IE_AppActivated,			&CGame::HandleMsg_ActivateApp },
	{ IE_WindowMove,			&CGame::HandleMsg_WindowMove },
	{ IE_Close,					&CGame::HandleMsg_Close },
	{ IE_Quit,					&CGame::HandleMsg_Close },
};


void CGame::AppActivate( bool fActive )
{
	// If text mode, force it to be active.
	if ( g_bTextMode )
	{
		fActive = true;
	}

	// Don't bother if we're already in the correct state
	if ( IsActiveApp() == fActive )
		return;

	// Don't let video modes changes queue up another activate event
	SetCanPostActivateEvents( false );

#ifndef SWDS
	if ( videomode )
	{
		if ( fActive )
		{
			videomode->RestoreVideo();
		}
		else
		{
			videomode->ReleaseVideo();
		}
	}

	if ( host_initialized )
	{
		if ( fActive )
		{
			// Clear keyboard states (should be cleared already but...)
			// VGui_ActivateMouse will reactivate the mouse soon.
			ClearIOStates();
			
			UpdateMaterialSystemConfig();
		}
		else
		{
			// Clear keyboard input and deactivate the mouse while we're away.
			ClearIOStates();

			if ( g_ClientDLL )
			{
				g_ClientDLL->IN_DeactivateMouse();
			}
		}
	}
#endif // SWDS
	SetActiveApp( fActive );

	// Allow queueing of activation events
	SetCanPostActivateEvents( true );
}

void CGame::HandleMsg_WindowMove( const InputEvent_t &event )
{
	game->SetWindowXY( event.m_nData, event.m_nData2 );
#ifndef SWDS
	videomode->UpdateWindowPosition();
#endif
}

void CGame::HandleMsg_ActivateApp( const InputEvent_t &event )
{
	AppActivate( event.m_nData ? true : false );
}

void CGame::HandleMsg_Close( const InputEvent_t &event )
{
	if ( eng->GetState() == IEngine::DLL_ACTIVE )
	{
		eng->SetQuitting( IEngine::QUIT_TODESKTOP );
	}
}

void CGame::DispatchInputEvent( const InputEvent_t &event )
{
	switch( event.m_nType )
	{
	// Handle button events specially, 
	// since we have all manner of crazy filtering going on	when dealing with them
	case IE_ButtonPressed:
	case IE_ButtonDoubleClicked:
	case IE_ButtonReleased:
		Key_Event( event );
		break;

	default:
		// Let vgui have the first whack at events
		if ( g_pMatSystemSurface && g_pMatSystemSurface->HandleInputEvent( event ) )
			break;

		for ( const auto &h : g_GameMessageHandlers )
		{
			if ( h.m_nEventType == event.m_nType )
			{
				(this->*h.pFn)( event );
				break;
			}
		}
		break;
	}
}


void CGame::DispatchAllStoredGameMessages()
{
#if !defined( NO_VCR )
	if ( VCRGetMode() == VCR_Playback )
	{
		InputEvent_t event;
		while ( VCRHook_PlaybackGameMsg( &event ) )
		{
			event.m_nTick = g_pInputSystem->GetPollTick();
			DispatchInputEvent( event );
		}
	}
	else
	{
		int nEventCount = g_pInputSystem->GetEventCount();
		const InputEvent_t* pEvents = g_pInputSystem->GetEventData( );
		for ( int i = 0; i < nEventCount; ++i )
		{
			VCRHook_RecordGameMsg( pEvents[i] );
			DispatchInputEvent( pEvents[i] );
		}
		VCRHook_RecordEndGameMsg();
	}
#else
	int nEventCount = g_pInputSystem->GetEventCount();
	const InputEvent_t* pEvents = g_pInputSystem->GetEventData( );
	for ( int i = 0; i < nEventCount; ++i )
	{
		DispatchInputEvent( pEvents[i] );
	}
#endif
}

void VCR_EnterPausedState()
{
	// Turn this off in case they're in single-step mode.
	g_bVCRSingleStep = false;

#ifdef WIN32
	// In this mode, we enter a wait state where we only pay attention to R and Q.
	while ( 1 )
	{
		if ( GetAsyncKeyState( 'R' ) & 0x8000 )
			break;

		if ( GetAsyncKeyState( 'Q' ) & 0x8000 )
			TerminateProcess( GetCurrentProcess(), 1 );

		if ( GetAsyncKeyState( 'S' ) & 0x8000 )
		{
			if ( !g_bWaitingForStepKeyUp )
			{
				// Do a single step.
				g_bVCRSingleStep = true;
				g_bWaitingForStepKeyUp = true;	// Don't do another single step until they release the S key.
				break;
			}
		}
		else
		{
			// Ok, they released the S key, so we'll process it next time the key goes down.
			g_bWaitingForStepKeyUp = false;
		}
	
		ThreadSleep( 2 );
	}
#else
	Assert( !"Impl me" );
#endif
}

#ifdef WIN32
void VCR_HandlePlaybackMessages( 
	HWND hWnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam 
	)
{
	if ( uMsg == WM_KEYDOWN )
	{
		if ( wParam == VK_SUBTRACT || wParam == 0xbd )
		{
			g_iVCRPlaybackSleepInterval += 5;
		}
		else if ( wParam == VK_ADD || wParam == 0xbb )
		{
			g_iVCRPlaybackSleepInterval -= 5;
		}
		else if ( toupper( wParam ) == 'Q' )
		{
			TerminateProcess( GetCurrentProcess(), 1 );
		}
		else if ( toupper( wParam ) == 'P' )
		{
			VCR_EnterPausedState();
		}
		else if ( toupper( wParam ) == 'S' && !g_bVCRSingleStep )
		{
			g_bWaitingForStepKeyUp = true;
			VCR_EnterPausedState();
		}
		else if ( toupper( wParam ) == 'D' )
		{
			g_bShowVCRPlaybackDisplay = !g_bShowVCRPlaybackDisplay;
		}

		g_iVCRPlaybackSleepInterval = clamp( g_iVCRPlaybackSleepInterval, 0, 500 );
	}
}

//-----------------------------------------------------------------------------
// Calls the default window procedure
//-----------------------------------------------------------------------------
static LRESULT WINAPI CallDefaultWindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return DefWindowProcW( hWnd, uMsg, wParam, lParam );
}
#endif

#if defined( WIN32 ) && !defined( USE_SDL )
//-----------------------------------------------------------------------------
// Main windows procedure
//-----------------------------------------------------------------------------
LRESULT CGame::WindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LRESULT			lRet = 0;

	//
	// NOTE: the way this function works is to handle all messages that just call through to
	// Windows or provide data to it.
	//
	// Any messages that change the engine's internal state (like key events) are stored in a list
	// and processed at the end of the frame. This is necessary for VCR mode to work correctly because
	// Windows likes to pump messages during some of its API calls like SetWindowPos, and unless we add
	// custom code around every Windows API call so VCR mode can trap the wndproc calls, VCR mode can't 
	// reproduce the calls to the wndproc.
	//

	if ( eng->GetQuitting() != IEngine::QUIT_NOTQUITTING )
		return CallWindowProc( m_ChainedWindowProc, hWnd, uMsg, wParam, lParam );

	// If we're playing back, listen to a couple input things used to drive VCR mode
	if ( VCRGetMode() == VCR_Playback )
	{
		VCR_HandlePlaybackMessages( hWnd, uMsg, wParam, lParam );
	}

	//
	// Note: NO engine state should be changed in here while in VCR record or playback. 
	// We can send whatever we want to Windows, but if we change its state in here instead of 
	// in DispatchAllStoredGameMessages, the playback may not work because Windows messages 
	// are not deterministic, so you might get different messages during playback than you did during record.
	//
	InputEvent_t event;
	memset( &event, 0, sizeof(event) );
	event.m_nTick = g_pInputSystem->GetPollTick();

	switch ( uMsg )
	{
	case WM_CREATE:
		::SetForegroundWindow( hWnd );
		break;

	case WM_ACTIVATEAPP:
		{
			if ( CanPostActivateEvents() )
			{
				bool bActivated = ( wParam == 1 );
				event.m_nType = IE_AppActivated;
				event.m_nData = bActivated;
				g_pInputSystem->PostUserEvent( event );
			}
		}
		break;

	case WM_POWERBROADCAST:
		// Don't go into Sleep mode when running engine, we crash on resume for some reason (as
		//  do half of the apps I have running usually anyway...)
		if ( wParam == PBT_APMQUERYSUSPEND )
		{
			Msg( "OS requested hibernation, ignoring request.\n" );
			return BROADCAST_QUERY_DENY;
		}

		lRet = CallWindowProc( m_ChainedWindowProc, hWnd, uMsg, wParam, lParam );
		break;

	case WM_SYSCOMMAND:
		if ( ( wParam == SC_MONITORPOWER ) || ( wParam == SC_KEYMENU ) || ( wParam == SC_SCREENSAVE ) )
            return lRet;
    
		if ( wParam == SC_CLOSE ) 
		{
#if !defined( NO_VCR )
			// handle the close message, but make sure 
			// it's not because we accidently hit ALT-F4
			if ( HIBYTE(VCRHook_GetKeyState(VK_LMENU)) || HIBYTE(VCRHook_GetKeyState(VK_RMENU) ) )
				return lRet;
#endif
			Cbuf_Clear();
			Cbuf_AddText( "quit\n" );
		}

#ifndef SWDS
		if ( VCRGetMode() == VCR_Disabled )
		{
			S_BlockSound();
			S_ClearBuffer();
		}
#endif

		lRet = CallWindowProc( m_ChainedWindowProc, hWnd, uMsg, wParam, lParam );

#ifndef SWDS
		if ( VCRGetMode() == VCR_Disabled )
		{
			S_UnblockSound();
		}
#endif
		break;

	case WM_CLOSE:
		// Handle close messages
		event.m_nType = IE_Close;
		g_pInputSystem->PostUserEvent( event );
		return 0;

	case WM_MOVE:
		event.m_nType = IE_WindowMove;
		event.m_nData = (short)LOWORD(lParam);
		event.m_nData2 = (short)HIWORD(lParam);
		g_pInputSystem->PostUserEvent( event );
		break;

	case WM_SIZE:
		if ( wParam != SIZE_MINIMIZED )
		{
			// Update restored client rect
			::GetClientRect( hWnd, &m_rcLastRestoredClientRect );
		}
		else
		{
			// Fix the window rect to have same client area as it used to have
			// before it got minimized
			RECT rcWindow;
			::GetWindowRect( hWnd, &rcWindow );

			rcWindow.right = rcWindow.left + m_rcLastRestoredClientRect.right;
			rcWindow.bottom = rcWindow.top + m_rcLastRestoredClientRect.bottom;

			::AdjustWindowRect( &rcWindow, ::GetWindowLong( hWnd, GWL_STYLE ), FALSE );
			::MoveWindow( hWnd, rcWindow.left, rcWindow.top,
				rcWindow.right - rcWindow.left, rcWindow.bottom - rcWindow.top, FALSE );
		}
		break;

	case WM_SYSCHAR:
		// keep Alt-Space from happening
		break;

	case WM_COPYDATA:
		// Hammer -> engine remote console command.
		// Return true to indicate that the message was handled.
		Cbuf_AddText( (const char *)(((COPYDATASTRUCT *)lParam)->lpData) );
		Cbuf_AddText( "\n" );
		lRet = 1;
		break;

	case WM_PAINT:
	{
		PAINTSTRUCT	ps;
		HDC hdc = BeginPaint(hWnd, &ps);
#ifndef SWDS
		RECT rcClient;
		GetClientRect( hWnd, &rcClient );

		// Only renders stuff if running -noshaderapi
		if ( videomode )
		{
			videomode->DrawNullBackground( hdc, rcClient.right, rcClient.bottom );
		}
#endif
		EndPaint(hWnd, &ps);
	}
		break;

	case WM_DISPLAYCHANGE:
		if ( !m_iDesktopHeight || !m_iDesktopWidth )
		{
			UpdateDesktopInformation( wParam, lParam );
		}
		break;

	case WM_IME_NOTIFY:
		switch ( wParam )
		{
		default:
			break;

#ifndef SWDS
		case IMN_PRIVATE:
			if ( !videomode->IsWindowedMode() )
				return 0;
			break;
#endif
		}
		return CallWindowProc( m_ChainedWindowProc, hWnd, uMsg, wParam, lParam );

	default:
		lRet = CallWindowProc( m_ChainedWindowProc, hWnd, uMsg, wParam, lParam );
	    break;
    }

    // return 0 if handled message, 1 if not
    return lRet;
}
#elif defined(OSX)

#elif defined(LINUX)

#elif defined(_WIN32)

#else
#error "Please define your platform"
#endif


#if defined( WIN32 ) && !defined( USE_SDL )
//-----------------------------------------------------------------------------
// Creates the game window 
//-----------------------------------------------------------------------------
static LRESULT WINAPI HLEngineWindowProc( HWND hWnd, UINT uMsg, WPARAM  wParam, LPARAM  lParam )
{
	return g_Game.WindowProc( hWnd, uMsg, wParam, lParam );
}

#define DEFAULT_EXE_ICON 101

#endif

bool CGame::CreateGameWindow( void )
{
	// get the window name
	char windowName[256];
	windowName[0] = '\0';

	{
		auto modinfo = KeyValues::AutoDelete("ModInfo");
		if (modinfo->LoadFromFile(g_pFileSystem, "gameinfo.txt"))
		{
			Q_strncpy( windowName, modinfo->GetString("game"), sizeof(windowName) );
		}
	}

	if (!windowName[0])
	{
		// dimhotepus: Not HALF-LIFE 2 when no info.
		Q_strncpy( windowName, "N/A", sizeof(windowName) );
	}

	if ( IsOpenGL() )
	{
		V_strcat( windowName, " - OpenGL", sizeof( windowName ) );
	}

#if PIX_ENABLE || defined( PIX_INSTRUMENTATION )
	// PIX_ENABLE/PIX_INSTRUMENTATION is a big slowdown (that should never be checked in, but sometimes is by accident), so add this to the Window title too.
	V_strcat( windowName, " - PIX_ENABLE", sizeof( windowName ) );
#endif

	const char *p = CommandLine()->ParmValue( "-window_name_suffix", "" );
	if ( p && V_strlen( p ) )
	{
		V_strcat( windowName, " - ", sizeof( windowName ) );
		V_strcat( windowName, p, sizeof( windowName ) );
	}

#if defined( WIN32 ) && !defined( USE_SDL )
#ifndef SWDS
	WNDCLASSW wc = {0};
	wc.style         = CS_OWNDC | CS_DBLCLKS;
	wc.lpfnWndProc   = CallDefaultWindowProc;
	wc.hInstance     = m_hInstance;
	wc.lpszClassName = CLASSNAME;

	// find the icon file in the filesystem
	if ( IsPC() )
	{
		char localPath[ MAX_PATH ];
		if ( g_pFileSystem->GetLocalPath( "resource/game.ico", localPath, sizeof(localPath) ) )
		{
			g_pFileSystem->GetLocalCopy( localPath );
			wc.hIcon = (HICON)::LoadImage(NULL, localPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
		}
		else
		{
			wc.hIcon = (HICON)::LoadIcon( GetModuleHandle( 0 ), MAKEINTRESOURCE( DEFAULT_EXE_ICON ) );
		}
	}

	wchar_t uc[512];
	if ( IsPC() )
	{
		Q_strtowcs( windowName, std::size(windowName), uc, sizeof(uc) );
	}

	// Oops, we didn't clean up the class registration from last cycle which
	// might mean that the wndproc pointer is bogus
	UnregisterClassW( CLASSNAME, m_hInstance );
	// Register it again
	RegisterClassW( &wc );

	// Note, it's hidden
	DWORD style = WS_POPUP | WS_CLIPSIBLINGS;
	
	// Give it a frame if we want a border
	if ( videomode->IsWindowedMode() )
	{
		if( !CommandLine()->FindParm( "-noborder" ) )
		{
			style |= WS_OVERLAPPEDWINDOW;
			style &= ~WS_THICKFRAME;
		}
	}

	// Never a max box
	style &= ~WS_MAXIMIZEBOX;

	// Create a full screen size window by default, it'll get resized later anyway
	int w = GetSystemMetrics( SM_CXSCREEN );
	int h = GetSystemMetrics( SM_CYSCREEN );

	// Create the window
	DWORD exFlags = 0;
	if ( g_bTextMode )
	{
		style &= ~WS_VISIBLE;
		exFlags |= WS_EX_TOOLWINDOW; // So it doesn't show up in the taskbar.
	}

	HWND hwnd = CreateWindowExW( exFlags, CLASSNAME, uc, style, 
		0, 0, w, h, nullptr, nullptr, m_hInstance, nullptr );
	// NOTE: On some cards, CreateWindowExW slams the FPU control word
	SetupFPUControlWord();

	if ( !hwnd )
	{
		auto error = std::system_category().message(::GetLastError());
		Error( "Fatal Error: Unable to create game window: %s!", error.c_str() );
	}

	SetMainWindow( hwnd );

	AttachToWindow( );
	return true;
#else
	return true;
#endif
#elif defined( USE_SDL )
	bool windowed = videomode->IsWindowedMode();

	modinfo->deleteThis();
	modinfo = NULL;

	if ( !g_pLauncherMgr->CreateGameWindow( windowName, windowed, 0, 0 ) )
	{
		Error( "Fatal Error:  Unable to create game window!" );
		return false;
	}
	
	char localPath[ MAX_PATH ];
	if ( g_pFileSystem->GetLocalPath( "resource/game-icon.bmp", localPath, sizeof(localPath) ) )
	{
		g_pFileSystem->GetLocalCopy( localPath );
		g_pLauncherMgr->SetApplicationIcon( localPath );
	}

	SetMainWindow( (SDL_Window*)g_pLauncherMgr->GetWindowRef() );

	AttachToWindow( );
	return true;
#else
#error "Please define your platform"
#endif
}


//-----------------------------------------------------------------------------
// Destroys the game window 
//-----------------------------------------------------------------------------
void CGame::DestroyGameWindow()
{
#if defined( USE_SDL )
	g_pLauncherMgr->DestroyGameWindow();
#else
#ifndef DEDICATED
	// Destroy all things created when the window was created
	if ( !m_bExternallySuppliedWindow )
	{
		DetachFromWindow( );

		if ( m_hWindow )
		{
			DestroyWindow( m_hWindow );
			m_hWindow = nullptr;
		}

		UnregisterClassW( CLASSNAME, m_hInstance );
	}
	else
	{
		m_hWindow = nullptr;
		m_bExternallySuppliedWindow = false;
	}
#endif // !defined( SWDS )
#endif
}


//-----------------------------------------------------------------------------
// This is used in edit mode to specify a particular game window (created by hammer)
//-----------------------------------------------------------------------------
void CGame::SetGameWindow( void *hWnd )
{
	m_bExternallySuppliedWindow = true;
#if defined( USE_SDL )
	SDL_RaiseWindow( (SDL_Window *)hWnd );
#else
    SetMainWindow( (HWND)hWnd );
#endif
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGame::AttachToWindow()
{
#if defined( WIN32 )
	if ( !m_hWindow )
		return;
#if !defined( USE_SDL )
	m_ChainedWindowProc = (WNDPROC)GetWindowLongPtrW( m_hWindow, GWLP_WNDPROC );
	SetWindowLongPtrW( m_hWindow, GWLP_WNDPROC, (LONG_PTR)HLEngineWindowProc );
#endif
#endif // WIN32

	if ( g_pInputSystem )
	{
		// Attach the input system window proc
#if defined( WIN32 )
		g_pInputSystem->AttachToWindow( (void *)m_hWindow );
#else
		g_pInputSystem->AttachToWindow( (void *)m_pSDLWindow );
#endif
		g_pInputSystem->EnableInput( true );
		g_pInputSystem->EnableMessagePump( false );
	}

	if ( g_pMatSystemSurface )
	{
		// Attach the vgui matsurface window proc
#if defined( WIN32 )
		g_pMatSystemSurface->AttachToWindow( (void *)m_hWindow, true );
#else
		g_pMatSystemSurface->AttachToWindow( (void *)m_pSDLWindow, true );
#endif
		g_pMatSystemSurface->EnableWindowsMessages( true );
	}
}

void CGame::DetachFromWindow()
{
#if defined( WIN32 ) && !defined( USE_SDL )
	if ( !m_hWindow || !m_ChainedWindowProc )
	{
		m_ChainedWindowProc = NULL;
		return;
	}
#endif

	if ( g_pMatSystemSurface )
	{
		// Detach the vgui matsurface window proc
		g_pMatSystemSurface->AttachToWindow( NULL );
	}

	if ( g_pInputSystem )
	{
		// Detach the input system window proc
		g_pInputSystem->EnableInput( false );
		g_pInputSystem->DetachFromWindow( );
	}

#if defined( WIN32 ) && !defined( USE_SDL )
	Assert( (WNDPROC)GetWindowLongPtrW( m_hWindow, GWLP_WNDPROC ) == HLEngineWindowProc );
	SetWindowLongPtrW( m_hWindow, GWLP_WNDPROC, (LONG_PTR)m_ChainedWindowProc );
#endif
}


//-----------------------------------------------------------------------------
// This is used in edit mode to override the default wnd proc associated w/
// the game window specified in SetGameWindow. 
//-----------------------------------------------------------------------------
bool CGame::InputAttachToGameWindow()
{
	// We can't use this feature unless we didn't control the creation of the window
	if ( !m_bExternallySuppliedWindow )
		return true;

	AttachToWindow();

#ifndef DEDICATED
	vgui::surface()->OnScreenSizeChanged( videomode->GetModeStereoWidth(), videomode->GetModeStereoHeight() );
#endif

	// We don't get WM_ACTIVATEAPP messages in this case; simulate one.
	AppActivate( true );

#if defined( WIN32 ) && !defined( USE_SDL )
	// Capture + hide the mouse
	SetCapture( m_hWindow );
#elif defined( USE_SDL )
	Assert( !"Impl me" );
	return false;
#else
#error "Please define your platform"
#endif
	return true;
}

void CGame::InputDetachFromGameWindow()
{
	// We can't use this feature unless we didn't control the creation of the window
	if ( !m_bExternallySuppliedWindow )
		return;

#if defined( WIN32 ) && !defined( USE_SDL )
	if ( !m_ChainedWindowProc )
		return;

	// Release + show the mouse
	ReleaseCapture();
#elif defined( USE_SDL )
	Assert( !"Impl me" );
#else
    #error "have no idea what OS we are building for"
#endif

	// We don't get WM_ACTIVATEAPP messages in this case; simulate one.
	AppActivate( false );

	DetachFromWindow();
}

class ScopedMilesAudioDevice
{
 public:
  ScopedMilesAudioDevice(IVAudio *audio, IVideoServices *video_services) noexcept
      : m_vaudio{audio},
        m_video_services{video_services},
        m_miles_sound_device{m_vaudio->CreateMilesAudioEngine()} {
    const VideoResult_t audio_rc = m_video_services->SoundDeviceCommand(
        VideoSoundDeviceOperation_t::SET_MILES_SOUND_DEVICE, m_miles_sound_device);
    if (audio_rc != VideoResult_t::SUCCESS) {
      Warning(
          "Miles Audio startup failed. Startup videos may be played without "
          "sound.");
    }
  }
  ScopedMilesAudioDevice(const ScopedMilesAudioDevice &) noexcept = delete;
  ScopedMilesAudioDevice& operator=(const ScopedMilesAudioDevice &) noexcept = delete;
  ScopedMilesAudioDevice(ScopedMilesAudioDevice &&) noexcept = delete;
  ScopedMilesAudioDevice &operator=(ScopedMilesAudioDevice &&) noexcept = delete;

  ~ScopedMilesAudioDevice() noexcept {
    // dimhotepus: Clear Miles audio device from Bink.
    if (m_miles_sound_device) {
      m_video_services->SoundDeviceCommand(
          VideoSoundDeviceOperation_t::SET_MILES_SOUND_DEVICE, nullptr);

      m_vaudio->DestroyMilesAudioEngine(m_miles_sound_device);
    }
  }

 private:
  IVAudio *m_vaudio;
  IVideoServices *m_video_services;
  void *m_miles_sound_device;
};

void CGame::PlayStartupVideos( void )
{
	if ( Plat_IsInBenchmarkMode() )
		return;

#ifndef SWDS
	
	// Wait for the mode to change and stabilized
	// FIXME: There's really no way to know when this is completed, so we have to guess a time that will mostly be correct
	if ( videomode->IsWindowedMode() == false )
	{
		Sys_Sleep( 1000 );
	}

	bool bEndGame = CommandLine()->CheckParm( "-endgamevid" );
	bool bRecap = CommandLine()->CheckParm( "-recapvid" );	// FIXME: This is a temp addition until the movie playback is centralized -- jdw
	
	bool bNeedHealthWarning = false;

	const char *HealthFile = "media/HealthWarning.txt";

	FileHandle_t	hFile;

	COM_OpenFile( HealthFile, &hFile );	
		
	//There is no access to steam at this point so we are checking for the presence of an empty file that will only exist in the chinese depot
	if ( hFile != FILESYSTEM_INVALID_HANDLE )
	{
		bNeedHealthWarning = true;
		COM_CloseFile( hFile );
	}

	if (!bNeedHealthWarning && !bEndGame && !bRecap && (CommandLine()->CheckParm("-dev") || CommandLine()->CheckParm("-novid") || CommandLine()->CheckParm("-allowdebug")))
		return;

	const char *pszFile = "media/StartupVids.txt";
	if ( bEndGame )
	{
		// Don't go back into the map that triggered this.
		CommandLine()->RemoveParm( "+map" );
		CommandLine()->RemoveParm( "+load" );
		
		pszFile = "media/EndGameVids.txt";
	}
	else if ( bRecap )
	{
		pszFile = "media/RecapVids.txt";
	}
	
	int vidFileLength;

	// have to use the malloc memory allocation option in COM_LoadFile since the memory system isn't set up at this point.
	const char *buffer = (char *) COM_LoadFile( pszFile, 5, &vidFileLength );
	
	if ((buffer == NULL) || (vidFileLength == 0))
	{
		return;
	}

	// dimhotepus: Need to play startup videos sound, but sound devices are not ready.
	extern void VAudioInit();
	VAudioInit();

	extern IVAudio *vaudio;
	// dimhotepus: Use Miles audio device.
	const ScopedMilesAudioDevice scoped_miles_audio_device{vaudio, g_pVideo};

	// hide cursor while playing videos
  #if defined( USE_SDL )
	g_pLauncherMgr->SetMouseVisible( false );
  #elif defined( WIN32 )
	::ShowCursor( FALSE );
  #endif

#if defined( LINUX )
	Audio_CreateSDLAudioDevice();
#endif

	const char *start = buffer;

	while( true )
	{
		start = COM_Parse(start);
		if ( Q_isempty( com_token ) )
		{
			break;
		}

		// get the path to the media file and play it.
		char localPath[MAX_PATH];

 		g_pFileSystem->GetLocalPath( com_token, localPath, sizeof(localPath) );
 		
		PlayVideoAndWait( localPath, bNeedHealthWarning );
		localPath[0] = 0; // just to make sure we don't play the same avi file twice in the case that one movie is there but another isn't.
	}

	// show cursor again
  #if defined( USE_SDL )
	g_pLauncherMgr->SetMouseVisible( true );
  #elif defined( WIN32 )
	::ShowCursor( TRUE );
  #endif

	// call free on the buffer since the buffer was malloc'd in COM_LoadFile
	free( (void *)buffer );

#endif // SWDS
}

//-----------------------------------------------------------------------------
// Purpose: Plays a video until the video completes or ESC is pressed
// Input  : *filename - Name of the file (relative to the filesystem)
//-----------------------------------------------------------------------------
void CGame::PlayVideoAndWait( const char *filename, bool bNeedHealthWarning )
{
	// do we have a filename and a video system, and not on a console?
	if ( !filename || !filename[0] || g_pVideo == NULL )
		return;

	// is it the valve logo file?		
	bool bIsValveLogo = ( Q_strstr( filename, "valve.") != NULL );

	//Chinese health messages appears for 11 seconds, so we force a minimum delay time for those
	float forcedMinTime = ( bIsValveLogo && bNeedHealthWarning ) ? 11.0f : -1.0f;

#if defined( WIN32 ) && !defined( USE_SDL )
	// Black out the back of the screen once at the beginning of each video (since we're not scaling to fit)
	HDC dc = ::GetDC( m_hWindow );

	RECT rect;
	rect.top = 0;
	rect.bottom = m_height;
	rect.left = 0;
	rect.right = m_width;

	HBRUSH hBlackBrush = (HBRUSH) ::GetStockObject( BLACK_BRUSH );
	::SetViewportOrgEx( dc, 0, 0, NULL );
	::FillRect( dc, &rect, hBlackBrush );
	::ReleaseDC( (HWND) GetMainWindow(), dc );
    
#else
	// need OS specific way to clear screen
    
#endif

	VideoResult_t status = 	g_pVideo->PlayVideoFileFullScreen( filename, "GAME", GetMainWindowPlatformSpecificHandle (),
	                                                           m_width, m_height, m_iDesktopWidth, m_iDesktopHeight, videomode->IsWindowedMode(),
	                                                           forcedMinTime, VideoPlaybackFlags::DEFAULT_FULLSCREEN_OPTIONS | VideoPlaybackFlags::FILL_WINDOW );

	// Everything ok?
	if ( status == VideoResult::SUCCESS )
	{
		return;
	}

	// We don't worry if it could not find something to could play
	if ( status == VideoResult::VIDEO_FILE_NOT_FOUND )	
	{
		return;
	}

	// Debug Builds, we want an error looked at by a developer, Release builds just send a message to the spew
#ifdef _DEBUG
	Error( "Error %d occurred attempting to play video file %s\n", (int) status, filename );
#else
	Msg( "Error %d occurred attempting to play video file %s\n", (int) status, filename );
#endif

}

// dimhotepus: Disable Win key.
//-----------------------------------------------------------------------------
// Purpose: Enable Windows shortcut keys when app is inactive.
//-----------------------------------------------------------------------------
bool ShouldEnableWindowsShortcutKeys() noexcept
{
  return !game->IsActiveApp();
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGame::CGame()
	// dimhotepus: Disable Win key.
#if defined( IS_WINDOWS_PC )
	: m_windowsShortcutKeysToggler{ ::GetModuleHandleW( L"engine.dll" ), ShouldEnableWindowsShortcutKeys }
#endif
{
	// dimhotepus: Disable Win key.
	if ( m_windowsShortcutKeysToggler.errno_code() )
	{
		const auto lastErrorText = m_windowsShortcutKeysToggler.errno_code().message();
		Warning("Can't disable Windows keys for full screen mode: %s\n", lastErrorText.c_str() );
	}

#if defined( USE_SDL )
	m_pSDLWindow = 0;
#endif

#if defined( WIN32 )
#if !defined( USE_SDL )
	m_hInstance = nullptr;
	m_ChainedWindowProc = nullptr;
#endif

	m_hWindow = nullptr;
#endif

	m_x = m_y = 0;
	m_width = m_height = 0;
	m_bActiveApp = false;
	m_bCanPostActivateEvents = true;
	m_iDesktopWidth = 0;
	m_iDesktopHeight = 0;
	m_iDesktopRefreshRate = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGame::~CGame()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CGame::Init( void *pvInstance )
{
	m_bExternallySuppliedWindow = false;

#if defined( WIN32 ) && !defined( USE_SDL )
	m_hInstance = (HINSTANCE)pvInstance;
#endif
	return true;
}


bool CGame::Shutdown( void )
{
#if defined( WIN32 ) && !defined( USE_SDL )
	m_hInstance = nullptr;
#endif
	return true;
}

void *CGame::GetMainWindow( void )
{
#ifdef USE_SDL
	return (void*)m_pSDLWindow;
#else
	return GetMainWindowPlatformSpecificHandle();
#endif
}

void *CGame::GetMainDeviceWindow( void )
{
#if defined( DX_TO_GL_ABSTRACTION ) && defined( USE_SDL )
	return (void*)m_pSDLWindow;
#else
	return (void*)m_hWindow;
#endif
}

void *CGame::GetMainWindowPlatformSpecificHandle( void )
{
#ifdef WIN32
	return (void*)m_hWindow;
#else
	SDL_SysWMinfo pInfo;
	SDL_VERSION( &pInfo.version );
	if ( !SDL_GetWindowWMInfo( (SDL_Window*)m_pSDLWindow, &pInfo ) )
	{
		Error( "Fatal Error: Unable to get window info from SDL." );
		return NULL;
	}

#ifdef OSX
	id nsWindow = (id)pInfo.info.cocoa.window;
	SEL selector = sel_registerName("windowRef");
	id windowRef = objc_msgSend( nsWindow, selector );
	return windowRef;
#else
	// Not used on Linux.
	return NULL;
#endif

#endif // !WIN32
}


void** CGame::GetMainWindowAddress( void )
{
#ifdef WIN32
	return (void**)&m_hWindow;
#else
	return NULL;
#endif
}

void CGame::GetDesktopInfo( int &width, int &height, int &refreshrate )
{
#if defined( USE_SDL )

	width = 640;
	height = 480;
	refreshrate = 0;

	// Go through all the displays and return the size of the largest.
	for( int i = 0; i < SDL_GetNumVideoDisplays(); i++ )
	{
		SDL_Rect rect;

		if ( !SDL_GetDisplayBounds( i, &rect ) )
		{
			if ( ( rect.w > width ) || ( ( rect.w == width ) && ( rect.h > height ) ) )
			{
				width = rect.w;
				height = rect.h;
			}
		}
	}
#else
	// order of initialization means that this might get called early.  In that case go ahead and grab the current
	// screen window and setup based on that.
	// we need to do this when initializing the base list of video modes, for example
	if ( m_iDesktopWidth == 0 )
	{
		HDC dc = ::GetDC( NULL );
		width = ::GetDeviceCaps(dc, HORZRES);
		height = ::GetDeviceCaps(dc, VERTRES);
		refreshrate = ::GetDeviceCaps(dc, VREFRESH);
		::ReleaseDC( NULL, dc );
		return;
	}
	width = m_iDesktopWidth;
	height = m_iDesktopHeight;
	refreshrate = m_iDesktopRefreshRate;
#endif
}

void CGame::UpdateDesktopInformation( )
{
#if defined( USE_SDL )
	// Get the size of the display we will be displayed fullscreen on.
	static ConVarRef sdl_displayindex( "sdl_displayindex" );
	int displayIndex = sdl_displayindex.IsValid() ? sdl_displayindex.GetInt() : 0;

	SDL_DisplayMode mode;
	SDL_GetDesktopDisplayMode( displayIndex, &mode );

	m_iDesktopWidth = mode.w;
	m_iDesktopHeight = mode.h;
	m_iDesktopRefreshRate = mode.refresh_rate;
#else
	HDC dc = ::GetDC( m_hWindow );
	m_iDesktopWidth = ::GetDeviceCaps(dc, HORZRES);
	m_iDesktopHeight = ::GetDeviceCaps(dc, VERTRES);
	m_iDesktopRefreshRate = ::GetDeviceCaps(dc, VREFRESH);
	::ReleaseDC( m_hWindow, dc );
#endif
}

#ifdef WIN32
void CGame::UpdateDesktopInformation( WPARAM wParam, LPARAM lParam )
{
	m_iDesktopWidth = LOWORD( lParam );
	m_iDesktopHeight = HIWORD( lParam );
}
#endif

#ifndef USE_SDL
void CGame::SetMainWindow( HWND window )
{
	m_hWindow = window;
	
	// update our desktop info (since the results will change if we are going to fullscreen mode)
	if ( !m_iDesktopWidth || !m_iDesktopHeight )
	{
		UpdateDesktopInformation();
	}
}
#else
void CGame::SetMainWindow( SDL_Window* window )
{
#if defined( WIN32 )
	// For D3D, we need to access the underlying HWND of the SDL_Window.
	// We also can't do this in GetMainDeviceWindow and just use that, because for some reason
	// people use GetMainWindowAddress and store that pointer to our member.
	SDL_SysWMinfo pInfo;
	SDL_VERSION( &pInfo.version );
	if ( !SDL_GetWindowWMInfo( (SDL_Window*)g_pLauncherMgr->GetWindowRef(), &pInfo ) )
	{
		Error( "Fatal Error: Unable to get window info from SDL." );
		return;
	}

	m_hWindow = pInfo.info.win.window;
#endif

	m_pSDLWindow = window;

	// update our desktop info (since the results will change if we are going to fullscreen mode)
	if ( !m_iDesktopWidth || !m_iDesktopHeight )
	{
		UpdateDesktopInformation();
	}
}
#endif

void CGame::SetWindowXY( int x, int y )
{
	m_x = x;
	m_y = y;
}

void CGame::SetWindowSize( int w, int h )
{
	m_width = w;
	m_height = h;
}

void CGame::GetWindowRect( int *x, int *y, int *w, int *h )
{
	if ( x )
	{
		*x = m_x;
	}
	if ( y )
	{
		*y = m_y;
	}
	if ( w )
	{
		*w = m_width;
	}
	if ( h )
	{
		*h = m_height;
	}
}

bool CGame::IsActiveApp( void )
{
	return m_bActiveApp;
}

void CGame::SetCanPostActivateEvents( bool bEnabled )
{
	m_bCanPostActivateEvents = bEnabled;
}

bool CGame::CanPostActivateEvents()
{
	return m_bCanPostActivateEvents;
}

void CGame::SetActiveApp( bool active )
{
	m_bActiveApp = active;

#if defined(IS_WINDOWS_PC)
  // Disable accessibility shortcut keys when app is active.
  // If app is inactive - restore accessibility shortcut keys to original state.
  m_accessibilityShortcutKeysToggler.Toggle( !IsActiveApp() );
#endif
}

