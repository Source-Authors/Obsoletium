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
#include <combaseapi.h>
#include <imm.h>
// dimhotepus: Disable accessibility keys (ex. five times Shift).
#include "accessibility_shortcut_keys_toggler.h"
#ifndef _DEBUG
// dimhotepus: Disable Win key.
#include "windows_shortcut_keys_toggler.h"
#endif
// dimhotepus: Notify when system suspend / resume. Save game when suspend.
#include "scoped_power_suspend_resume_notifications_registrator.h"
// dimhotepus: Notify when system power settings changed (battery, AC, power plan, etc.).
#include "scoped_power_settings_notifications_registrator.h"
// dimhotepus: Do not disable display during video playback when no user input.
#include "scoped_thread_execution_state.h"
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

#if defined(WIN32) && !defined(USE_SDL)

/**
 * @brief AC power status.
 */
enum class SystemPowerAcLineStatus : unsigned char
{
	/**
	 * @brief Offline.
	 */
	kOffline = 0,
	/**
	 * @brief Online.
	 */
	kOnline = 1,
	/**
	 * @brief Unknown status.
	 */
	kUnknown = 255
};

/**
 * @brief The battery charge status flags.
 */
enum class SystemPowerBatteryChargeStatusFlags : unsigned char
{
	/**
	 * @brief The battery is not being charged and the battery capacity is
	 * between low and high.
	 */
	kNotChargingAndBetween33And66Percent = 0,
	/**
	 * @brief High - the battery capacity is at more than 66 percent.
	 */
	kMoreThan66Percent = 1,
	/**
	 * @brief Low - the battery capacity is at less than 33 percent.
	 */
	kLessThan33Percent = 2,
	/**
	 * @brief Critical - the battery capacity is at less than five percent.
	 */
	kLessThan5Percent = 4,
	/**
	 * @brief Charging.
	 */
	kCharging = 8,
	/**
	 * @brief No system battery.
	 */
	kNoSystemBattery = 128,
	/**
	 * @brief Unknown status - unable to read the battery flag information.
	 */
	kUnknown = 255
};

/**
 * @brief The status of battery saver.  To participate in energy conservation,
 * avoid resource intensive tasks when battery saver is on.
 */
enum class SystemPowerBatterySaverStatus : unsigned char {
	/**
	 * @brief Battery saver is off.
	 */
	kOff = 0,
	/**
	 * @brief Battery saver on.  Save energy where possible.
	 */
	kOn = 1
};

#endif

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
	LRESULT			WindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
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
	void	DispatchAllStoredGameMessages() override;

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

	SystemPowerAcLineStatus m_last_ac_line_status{SystemPowerAcLineStatus::kUnknown};
	SystemPowerBatteryChargeStatusFlags m_last_battery_status_flags{SystemPowerBatteryChargeStatusFlags::kUnknown};
	SystemPowerBatterySaverStatus m_last_battery_saver_status{SystemPowerBatterySaverStatus::kOff};

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

#ifdef IS_WINDOWS_PC
#ifndef _DEBUG
	// dimhotepus: Disable Win key.
	// Only in Release builds as it slowdowns IDE typing a lot in debugging sessions.
	// See https://github.com/Source-Authors/Obsoletium/issues/124
	source::engine::win::WindowsShortcutKeysToggler m_windowsShortcutKeysToggler;
#endif
	// dimhotepus: Disable accessibility keys (ex. five times Shift).
	source::engine::win::AccessibilityShortcutKeysToggler m_accessibilityShortcutKeysToggler;
	// dimhotepus: Notify when system suspend / resume.
	std::unique_ptr<source::engine::win::ScopedPowerSuspendResumeNotificationsRegistrator> m_powerSuspendResumeRegistrator;
	// dimhotepus: Notify when active session display on / offs changed.
	std::unique_ptr<source::engine::win::ScopedPowerSettingNotificationsRegistrator> m_sessionDisplayStatusChangedNotificationsRegistrator;
#endif  // IS_WINDOWS_PC

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
		intp nEventCount = g_pInputSystem->GetEventCount();
		const InputEvent_t* pEvents = g_pInputSystem->GetEventData( );
		for ( intp i = 0; i < nEventCount; ++i )
		{
			VCRHook_RecordGameMsg( pEvents[i] );
			DispatchInputEvent( pEvents[i] );
		}
		VCRHook_RecordEndGameMsg();
	}
#else
	intp nEventCount = g_pInputSystem->GetEventCount();
	const InputEvent_t* pEvents = g_pInputSystem->GetEventData( );
	for ( intp i = 0; i < nEventCount; ++i )
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

		// dimhotepus: Run destructors, 1 -> 0.
		if ( GetAsyncKeyState( 'Q' ) & 0x8000 )
			exit( 0 );

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
	AssertMsg( false, "Impl me" );
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
		else
		{
			const int upper = toupper(static_cast<unsigned short>(wParam));

			if ( upper == 'Q' )
			{
				// dimhotepus: Run destructors, 1 -> 0
				exit( 0 );
			}
			else if ( upper == 'P' )
			{
				VCR_EnterPausedState();
			}
			else if ( upper == 'S' && !g_bVCRSingleStep )
			{
				g_bWaitingForStepKeyUp = true;
				VCR_EnterPausedState();
			}
			else if ( upper == 'D' )
			{
				g_bShowVCRPlaybackDisplay = !g_bShowVCRPlaybackDisplay;
			}
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

#if defined(WIN32) && !defined(USE_SDL)

[[nodiscard]] const char* GetAcLineStatusDescription(SystemPowerAcLineStatus status) noexcept {
  switch (status) {
    case SystemPowerAcLineStatus::kOffline:
      return "Offline";
    case SystemPowerAcLineStatus::kOnline:
      return "Online";
    case SystemPowerAcLineStatus::kUnknown:
    default:
      return "Unknown";
  }
}

[[nodiscard]] const char* GetBatteryChargeStatusDescription(SystemPowerBatteryChargeStatusFlags flags) noexcept {
  switch (flags) {
    case SystemPowerBatteryChargeStatusFlags::kNotChargingAndBetween33And66Percent:
      return "33..66%";
    case SystemPowerBatteryChargeStatusFlags::kMoreThan66Percent:
      return "> 66%";
    case SystemPowerBatteryChargeStatusFlags::kLessThan33Percent:
      return "< 33%";
    case SystemPowerBatteryChargeStatusFlags::kLessThan5Percent:
      return "< 5%";
    case SystemPowerBatteryChargeStatusFlags::kCharging:
      return "Charging";
    case SystemPowerBatteryChargeStatusFlags::kNoSystemBattery:
      return "No battery";
	default:
    case SystemPowerBatteryChargeStatusFlags::kUnknown:
      return "Unknown";
  }
}

[[nodiscard]] const char *GetBatterySaverStatusDescription(
    SystemPowerBatterySaverStatus status) noexcept {
  switch (status) {
    case SystemPowerBatterySaverStatus::kOff:
      return "Off";
    case SystemPowerBatterySaverStatus::kOn:
      return "On";
    default:
      return "Unknown";
  }
}

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

	// Sent to both the window being activated and the window being deactivated.
	// If the windows use the same input queue, the message is sent
	// synchronously, first to the window procedure of the top-level window
	// being deactivated, then to the window procedure of the top-level window
	// being activated.  If the windows use different input queues, the message
	// is sent asynchronously, so the window is activated immediately.
	case WM_ACTIVATE:
		{
			const unsigned short window_activation_state{LOWORD( wParam )};
			const bool is_activated{window_activation_state != WA_INACTIVE};
			// The high-order word specifies the minimized state of the window
			// being activated or deactivated.  A nonzero value indicates the
			// window is minimized.
			// const bool is_window_minimized{HIWORD( wParam ) != 0};

			// dimhotepus: Unify way to handle sound mute on focus lost.
			ConVarRef snd_mute_losefocus("snd_mute_losefocus");
			if (snd_mute_losefocus.GetBool())
			{
				if ( is_activated )
				{
					S_UnblockSound();
				}
				else
				{
					S_BlockSound();
					// dimhotepus: Need to be thread-safe.
					// S_ClearBuffer();
				}
			}

			if ( CanPostActivateEvents() )
			{
				event.m_nType = IE_AppActivated;
				event.m_nData = is_activated;
				g_pInputSystem->PostUserEvent( event );
			}

			break;
		}

	case WM_POWERBROADCAST:
		switch (wParam)
		{
			case PBT_APMPOWERSTATUSCHANGE:
				{
					// Notifies applications of a change in the power status of the
					// computer, such as a switch from battery power to A/C.  The
					// system also broadcasts this event when remaining battery
					// power slips below the threshold specified by the user or if
					// the battery power changes by a specified percentage.
					//
					// This event can occur when battery life drops to less than 5
					// minutes, or when the percentage of battery life drops below
					// 10 percent, or if the battery life changes by 3 percent.
					SYSTEM_POWER_STATUS power_status = {};
					if (::GetSystemPowerStatus(&power_status))
					{
						const SystemPowerAcLineStatus new_ac_line_status =
							static_cast<SystemPowerAcLineStatus>(power_status.ACLineStatus);
						const SystemPowerBatteryChargeStatusFlags new_battery_status_flags =
							static_cast<SystemPowerBatteryChargeStatusFlags>(power_status.BatteryFlag);
						const SystemPowerBatterySaverStatus new_battery_saver_status =
							static_cast<SystemPowerBatterySaverStatus>(power_status.SystemStatusFlag);

						// Only if any status changed, as Windows sends the same ^ statuses in a row.
						if ( new_ac_line_status != m_last_ac_line_status ||
							 new_battery_status_flags != m_last_battery_status_flags ||
							 new_battery_saver_status != m_last_battery_saver_status )
						{
							Msg("System power status changed: AC power '%s', battery charge '%s', battery saver '%s'.\n",
								GetAcLineStatusDescription(new_ac_line_status),
								GetBatteryChargeStatusDescription(new_battery_status_flags),
								GetBatterySaverStatusDescription(new_battery_saver_status));

							// The number of seconds of battery life remaining, or
							// -1 if remaining seconds are unknown or if the device
							// is connected to AC power.
							if (power_status.BatteryLifeTime != static_cast<DWORD>(-1))
							{
								Msg("System power status changed: %lu hours %.1f mins estimated battery life remaining.\n",
									power_status.BatteryLifeTime / 3600, ((power_status.BatteryLifeTime % 3600) / 60.0));

								// Ok, less than 3 minutes left.
								if (power_status.BatteryLifeTime < 180)
								{
									Cbuf_AddText( "save low-battery-auto-save" );
								}
							}
						}
					}
				}
				break;

			case PBT_APMRESUMEAUTOMATIC:
				// Notifies applications that the system is resuming from sleep
				// or hibernation.  This event is delivered every time the system
				// resumes and does not indicate whether a user is present.
				//
				// Possible reasons include:
				// * user activity (such as pressing the power button)
				// * user interaction at the physical console (such as mouse or
				//   keyboard input) after waking unattended
				// * remote wake signal.
				//
				// In Windows 10, version 1507 systems and later, if the system
				// is resuming from sleep only to immediately enter hibernation,
				// then this event isn't delivered.  A WM_POWERBROADCAST message
				// is not sent in this case.
				Msg("System is resuming from sleep or hibernation by any reason.\n");
				break;

			case PBT_APMRESUMESUSPEND:
				// Notifies applications that the system has resumed operation
				// after being suspended.
				//
				// Your application should reopen files that it closed when the
				// system entered sleep and prepare for user input.
				Msg("System is resuming from sleep or hibernation by user input.\n");
				break;

			case PBT_APMSUSPEND:
				// Notifies applications that the computer is about to enter a
				// suspended state.  This event is typically broadcast when all
				// applications and installable drivers have returned TRUE to a
				// previous PBT_APMQUERYSUSPEND event.
				//
				// An application should process this event by completing all
				// tasks necessary to save data.  The system allows approximately
				// two seconds for an application to handle this notification.
				// If an application is still performing operations after its
				// time allotment has expired, the system may interrupt the
				// application.
				Msg("System is about to enter suspended state. Saving the game...\n");
				Cbuf_AddText( "save suspend-auto-save" );
				break;

			case PBT_POWERSETTINGCHANGE:
				{
					// Power setting change event.
					POWERBROADCAST_SETTING *power_broadcast_setting =
						reinterpret_cast<POWERBROADCAST_SETTING *>(lParam);
					Assert(power_broadcast_setting);

					if (power_broadcast_setting)
					{
						if (IsEqualGUID(power_broadcast_setting->PowerSetting,
								GUID_SESSION_DISPLAY_STATUS))
						{
							// The display associated with the application's
							// session has been powered on or off.
							DWORD data;
							memcpy(&data,
								power_broadcast_setting->Data,
								std::min(static_cast<DWORD>(sizeof(DWORD)), power_broadcast_setting->DataLength));

							const MONITOR_DISPLAY_STATE display_state =
								static_cast<MONITOR_DISPLAY_STATE>(data);
							
							Msg("App session display status changed to '%s'.\n",
								display_state == MONITOR_DISPLAY_STATE::PowerMonitorOff
								? "Off"
								: display_state == MONITOR_DISPLAY_STATE::PowerMonitorOn
									? "On"
									: display_state == MONITOR_DISPLAY_STATE::PowerMonitorDim
										? "Dimmed"
										: "Unknown");

							// TODO: Stop rendering when display is off.
						}
					}
				} break;
		}

		lRet = CallWindowProc( m_ChainedWindowProc, hWnd, uMsg, wParam, lParam );
		break;

	case WM_SYSCOMMAND:
		{
			// In WM_SYSCOMMAND messages, the four low-order bits of the wParam
			// parameter are used internally by the system.  To obtain the
			// correct result when testing the value of wParam, an application
			// must combine the value 0xFFF0 with the wParam value by using the
			// bitwise AND operator.
			const unsigned short command_type =
				static_cast<unsigned short>(wParam & 0xFFF0);
			// Sets the state of the display.  This command supports devices that
			// have power-saving features, such as a battery-powered personal
			// computer.
			if ( ( command_type == SC_MONITORPOWER ) ||
				// Retrieves the window menu as a result of a keystroke.
				( command_type == SC_KEYMENU ) ||
				// Executes the screen saver application specified.
				( command_type == SC_SCREENSAVE ) )
				return lRet;

			if ( command_type == SC_CLOSE )
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
		} break;

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
		if ( wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED )
		{
			RECT rcClient;
			::GetClientRect( hWnd, &rcClient );
			if ( rcClient.top == 0 && rcClient.bottom == 0 )
			{
				// Rapidly clicking the task bar to minimize and restore a window
				// can cause a WM_SIZE message with SIZE_RESTORED when
				// the window has actually become minimized due to rapid change
				// so just ignore this message
				break;
			}

			// Update restored client rect
			m_rcLastRestoredClientRect = rcClient;
		}
		else if ( wParam == SIZE_MINIMIZED )
		{
			// Fix the window rect to have same client area as it used to have
			// before it got minimized
			RECT rcWindow;
			::GetWindowRect( hWnd, &rcWindow );

			rcWindow.right = rcWindow.left + m_rcLastRestoredClientRect.right;
			rcWindow.bottom = rcWindow.top + m_rcLastRestoredClientRect.bottom;

			const DWORD windowStyle = ::GetWindowLong( hWnd, GWL_STYLE ); //-V303 //-V2002
			const DWORD windowExStyle = ::GetWindowLong( hWnd, GWL_EXSTYLE ); //-V303 //-V2002
			// dimhotepus: Honor DPI.
			::AdjustWindowRectExForDpi( &rcWindow, windowStyle, FALSE, windowExStyle, ::GetDpiForWindow( hWnd ) ); //-V303 //-V2002
			::MoveWindow( hWnd, rcWindow.left, rcWindow.top,
				rcWindow.right - rcWindow.left, rcWindow.bottom - rcWindow.top, FALSE );
		}
		break;

	// dimhotepus: Handy when we will add window resize.
	case WM_GETMINMAXINFO:
	{
		auto minmaxInfo = reinterpret_cast<MINMAXINFO*>(lParam);
		minmaxInfo->ptMinTrackSize.x = BASE_WIDTH;
		minmaxInfo->ptMinTrackSize.y = BASE_HEIGHT;
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
		RunCodeAtScopeExit(EndPaint(hWnd, &ps));

#ifndef SWDS
		RECT rc_client;
		GetClientRect( hWnd, &rc_client );

		// Only renders stuff if running -noshaderapi
		if ( videomode )
		{
			videomode->DrawNullBackground( hdc, rc_client.right - rc_client.left, rc_client.bottom - rc_client.top );
		}
#endif
	}
		break;

	case WM_DISPLAYCHANGE:
		if ( !m_iDesktopHeight || !m_iDesktopWidth )
		{
			UpdateDesktopInformation( wParam, lParam );
		}
		break;

	case WM_SETTINGCHANGE:
	{
		// dimhotepus: Handle Dark / Light mode change.
		const wchar_t *area = reinterpret_cast<const wchar_t *>( lParam );
		if ( area && V_wcscmp( area, L"ImmersiveColorSet" ) == 0 )
		{
			// dimhotepus: Reapply Dark or Light mode if any and Mica styles to window title bar.
			Plat_ApplySystemTitleBarTheme( hWnd, SystemBackdropType::MainWindow );
		}
		else
		{
			lRet = CallWindowProc( m_ChainedWindowProc, hWnd, uMsg, wParam, lParam );
		}
	}
		break;

	case WM_IME_NOTIFY:
		switch ( wParam )
		{
		default:
			break;

#ifndef SWDS
		case IMN_PRIVATE:
			if ( !videomode->IsWindowedMode() && !videomode->IsBorderlessMode() )
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
	char utf8_window_name[256];
	utf8_window_name[0] = '\0';

	if ( KeyValuesAD modinfo("ModInfo");
		 modinfo->LoadFromFile(g_pFileSystem, "gameinfo.txt") )
	{
		V_strcpy_safe( utf8_window_name, modinfo->GetString("game") );
	}

	if (!utf8_window_name[0])
	{
		// dimhotepus: Not HALF-LIFE 2 when no info.
		V_strcpy_safe( utf8_window_name, "N/A" );
	}

	if ( IsOpenGL() )
	{
		V_strcat_safe( utf8_window_name, " - OpenGL" );
	}
	else
	{
		// dimhotepus: Match steam_legacy branch behavior.
		V_strcat_safe( utf8_window_name, " - Direct3D 9Ex" );
	}

#if PIX_ENABLE || defined( PIX_INSTRUMENTATION )
	// PIX_ENABLE/PIX_INSTRUMENTATION is a big slowdown (that should never be
	// checked in, but sometimes is by accident), so add this to the Window title too.
	V_strcat_safe( windowName, " - PIX_ENABLE" );
#endif

#ifdef PLATFORM_64BITS
	V_strcat_safe( utf8_window_name, " - 64 bit" );
#endif

	if ( const char *p = CommandLine()->ParmValue( "-window_name_suffix", "" );
		 p && !Q_isempty( p ) )
	{
		V_strcat_safe( utf8_window_name, " - " );
		V_strcat_safe( utf8_window_name, p );
	}

#if defined( WIN32 ) && !defined( USE_SDL )
#ifndef SWDS
	WNDCLASSW     wc = {};
	wc.style         = CS_OWNDC | CS_DBLCLKS;
	wc.lpfnWndProc   = CallDefaultWindowProc;
	wc.hInstance     = m_hInstance;
	wc.lpszClassName = CLASSNAME;

	// find the icon file in the filesystem
	if ( char localPath[ MAX_PATH ];
		 g_pFileSystem->GetLocalPath_safe( "resource/game.ico", localPath ) )
	{
		g_pFileSystem->GetLocalCopy( localPath );

		wc.hIcon = (HICON)::LoadImage(NULL, localPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
	}
	else
	{
		wc.hIcon = ::LoadIcon( m_hInstance, MAKEINTRESOURCE( DEFAULT_EXE_ICON ) );
	}

	wchar_t utf16_window_name[512];
	V_strtowcs( utf8_window_name, std::size(utf8_window_name), utf16_window_name );

	if ( WNDCLASSW ewc; GetClassInfoW( m_hInstance, CLASSNAME, &ewc ) )
	{
		// Oops, we didn't clean up the class registration from last cycle which
		// might mean that the wndproc pointer is bogus.
		UnregisterClassW( CLASSNAME, m_hInstance );
	}

	// Register it again.
	if (!RegisterClassW( &wc ))
	{
		auto error = std::system_category().message(::GetLastError());
		Warning("Unable to register window '%s' class: %s.\n",
			utf8_window_name, error.c_str());
		return false;
	}

	// Note, it's hidden
	DWORD style = WS_POPUP | WS_CLIPSIBLINGS;
	
	// Give it a frame if we want a border
	if ( videomode->IsWindowedMode() )
	{
		if ( !videomode->IsBorderlessMode() )
		{
			style |= WS_OVERLAPPEDWINDOW;
			style &= ~WS_THICKFRAME;
		}
	}

	// Never a max box
	style &= ~WS_MAXIMIZEBOX;
	
	const unsigned system_dpi{::GetDpiForSystem()};

	// Create a full screen size window by default, it'll get resized later anyway
	int w = GetSystemMetricsForDpi( SM_CXSCREEN, system_dpi );
	int h = GetSystemMetricsForDpi( SM_CYSCREEN, system_dpi );

	// Create the window
	DWORD exFlags = 0;
	if ( g_bTextMode )
	{
		style &= ~WS_VISIBLE;
		exFlags |= WS_EX_TOOLWINDOW; // So it doesn't show up in the taskbar.
	}

	HWND hwnd = CreateWindowExW( exFlags, CLASSNAME, utf16_window_name, style,
		0, 0, w, h, nullptr, nullptr, m_hInstance, nullptr );
	if ( !hwnd )
	{
		auto error = std::system_category().message(::GetLastError());
		Error( "Unable to create game window '%s': %s.\n", utf8_window_name, error.c_str() );
		return false;
	}

	// dimhotepus: Apply Dark mode if any and Mica styles to window title bar.
	Plat_ApplySystemTitleBarTheme( hwnd, SystemBackdropType::MainWindow );

	// NOTE: On some cards, CreateWindowExW slams the FPU control word
	SetupFPUControlWord();

	SetMainWindow( hwnd );

	AttachToWindow( );
	return true;
#else
	return true;
#endif
#elif defined( USE_SDL )
	bool windowed = videomode->IsWindowedMode();

	if ( !g_pLauncherMgr->CreateGameWindow( windowName, windowed, 0, 0 ) )
	{
		Error( "Fatal Error:  Unable to create game window!" );
		return false;
	}
	
	char localPath[ MAX_PATH ];
	if ( g_pFileSystem->GetLocalPath_safe( "resource/game-icon.bmp", localPath ) )
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

		if ( WNDCLASSW ewc; GetClassInfoW( m_hInstance, CLASSNAME, &ewc ) )
		{
			UnregisterClassW( CLASSNAME, m_hInstance );
		}
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

	m_powerSuspendResumeRegistrator =
		std::move(std::make_unique<source::engine::win::ScopedPowerSuspendResumeNotificationsRegistrator>(m_hWindow));
	m_sessionDisplayStatusChangedNotificationsRegistrator = std::move(
		std::make_unique<source::engine::win::ScopedPowerSettingNotificationsRegistrator>(m_hWindow, GUID_SESSION_DISPLAY_STATUS));

#if !defined( USE_SDL )
	m_ChainedWindowProc = (WNDPROC)GetWindowLongPtrW( m_hWindow, GWLP_WNDPROC ); //-V204
	SetWindowLongPtrW( m_hWindow, GWLP_WNDPROC, (LONG_PTR)HLEngineWindowProc ); //-V221
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

	m_sessionDisplayStatusChangedNotificationsRegistrator.reset();
	m_powerSuspendResumeRegistrator.reset();
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
	Assert( (WNDPROC)GetWindowLongPtrW( m_hWindow, GWLP_WNDPROC ) == HLEngineWindowProc ); //-V204
	SetWindowLongPtrW( m_hWindow, GWLP_WNDPROC, (LONG_PTR)m_ChainedWindowProc ); //-V221
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
	AssertMsg( false, "Impl me" );
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
	AssertMsg( false, "Impl me" );
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

#ifdef _WIN32
class ScopedShowCursor
{
 public:
  ScopedShowCursor(bool is_shown) noexcept : m_is_shown{is_shown}
  {
	  ::ShowCursor(is_shown ? TRUE : FALSE);
  }
  ~ScopedShowCursor() noexcept
  {
	  ::ShowCursor(m_is_shown ? FALSE : TRUE);
  }

  ScopedShowCursor(ScopedShowCursor &) = delete;
  ScopedShowCursor& operator=(ScopedShowCursor &) = delete;
  ScopedShowCursor(ScopedShowCursor &&) = delete;
  ScopedShowCursor& operator=(ScopedShowCursor &&) = delete;

 private:
  const bool m_is_shown;
};
#endif

void CGame::PlayStartupVideos( void )
{
	if ( Plat_IsInBenchmarkMode() )
		return;

#ifndef SWDS
	
	// Wait for the mode to change and stabilized
	// FIXME: There's really no way to know when this is completed, so we have to guess a time that will mostly be correct
	if ( videomode->IsWindowedMode() == false && videomode->IsBorderlessMode() == false )
	{
		// dimhotepus: Reduce mode stabilization time to 300ms as CRTs are rarely used anymore.
		Sys_Sleep( 300 );
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
	char *buffer = (char *) COM_LoadFile( pszFile, 5, &vidFileLength );
	if ((buffer == NULL) || (vidFileLength == 0))
	{
		return;
	}

	// dimhotepus: Need to play startup videos sound, but sound devices are not ready.
	extern void VAudioInit();
	VAudioInit();

	extern IVAudio *vaudio;
	// dimhotepus: Use Miles audio device.
	// ASAN breaks inside Miles in x64 mode, so disable sound.
#if !defined(__SANITIZE_ADDRESS__) || !defined(PLATFORM_64BITS)
	const ScopedMilesAudioDevice scoped_miles_audio_device{vaudio, g_pVideo};
#endif

	// dimhotepus: Ensure display is not disabled by powersaver when playing startup videos.
	const source::engine::win::ScopedThreadExecutionState
		scoped_display_required_state{ES_DISPLAY_REQUIRED};
	if (!scoped_display_required_state.is_success())
	{
		Warning("Unable to signal system display is required. Display may disable during video playback.\n");
	}

	// hide cursor while playing videos
  #if defined( USE_SDL )
	g_pLauncherMgr->SetMouseVisible( false );
  #elif defined( WIN32 )
	const ScopedShowCursor scoped_show_cursor{false};
  #endif

#if defined( LINUX )
	Audio_CreateSDLAudioDevice();
#endif

	char token[COM_TOKEN_MAX_LENGTH];
	const char *start = buffer;

	while( true )
	{
		start = COM_Parse(start, token);
		if ( Q_isempty( token ) )
		{
			break;
		}

		// get the path to the media file and play it.
		char localPath[MAX_PATH];
 		g_pFileSystem->GetLocalPath_safe( token, localPath );
 		
		PlayVideoAndWait( localPath, bNeedHealthWarning );
		localPath[0] = 0; // just to make sure we don't play the same avi file twice in the case that one movie is there but another isn't.
	}

	// show cursor again
  #if defined( USE_SDL )
	g_pLauncherMgr->SetMouseVisible( true );
  #endif

	// call free on the buffer since the buffer was malloc'd in COM_LoadFile
	free( buffer );

#endif // SWDS
}

//-----------------------------------------------------------------------------
// Purpose: Plays a video until the video completes or ESC is pressed
// Input  : *filename - Name of the file (relative to the filesystem)
//-----------------------------------------------------------------------------
void CGame::PlayVideoAndWait( const char *filename, bool bNeedHealthWarning )
{
	// do we have a filename and a video system, and not on a console?
	if ( Q_isempty( filename ) || g_pVideo == NULL )
		return;

	// is it the valve logo file?		
	bool bIsValveLogo = ( Q_strstr( filename, "valve.") != NULL );

	//Chinese health messages appears for 11 seconds, so we force a minimum delay time for those
	float forcedMinTime = ( bIsValveLogo && bNeedHealthWarning ) ? 11.0f : -1.0f;

#if defined( WIN32 ) && !defined( USE_SDL )
	{
		// Black out the back of the screen once at the beginning of each video (since we're not scaling to fit)
		HDC dc = ::GetDC( m_hWindow );
		RunCodeAtScopeExit(::ReleaseDC( m_hWindow, dc ));

		RECT rect{0, 0, m_width, m_height};

		HBRUSH hBlackBrush = (HBRUSH)::GetStockObject( BLACK_BRUSH );
		RunCodeAtScopeExit(::DeleteObject(hBlackBrush));

		::SetViewportOrgEx( dc, 0, 0, nullptr );
		::FillRect( dc, &rect, hBlackBrush );
	}
#else
	// need OS specific way to clear screen
    
#endif

	VideoResult_t status = 	g_pVideo->PlayVideoFileFullScreen( filename, "GAME", GetMainWindowPlatformSpecificHandle (),
	                                                           m_width, m_height, m_iDesktopWidth, m_iDesktopHeight, videomode->IsWindowedMode() || videomode->IsBorderlessMode(),
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
#if !defined(_DEBUG) && defined(IS_WINDOWS_PC)
	: m_windowsShortcutKeysToggler{ ::GetModuleHandleW( L"engine.dll" ), ShouldEnableWindowsShortcutKeys }
#endif
{
#if !defined(_DEBUG) && defined(IS_WINDOWS_PC)
	// dimhotepus: Disable Win key.
	if ( m_windowsShortcutKeysToggler.errno_code() )
	{
		const auto lastErrorText = m_windowsShortcutKeysToggler.errno_code().message();
		Warning("Can't disable Windows keys for full screen mode: %s\n", lastErrorText.c_str() );
	}
#endif

	m_bExternallySuppliedWindow = false;

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
	m_rcLastRestoredClientRect.left = m_rcLastRestoredClientRect.right =
	m_rcLastRestoredClientRect.top = m_rcLastRestoredClientRect.bottom = 0;
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
#ifdef OSX
	SDL_SysWMinfo pInfo;
	SDL_VERSION( &pInfo.version );
	if ( !SDL_GetWindowWMInfo( (SDL_Window*)m_pSDLWindow, &pInfo ) )
	{
		Error( "Fatal Error: Unable to get window info from SDL." );
		return NULL;
	}

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

	width = BASE_WIDTH;
	height = BASE_HEIGHT;
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
		RunCodeAtScopeExit( ReleaseDC( NULL, dc ));

		width = ::GetDeviceCaps(dc, HORZRES);
		height = ::GetDeviceCaps(dc, VERTRES);
		refreshrate = ::GetDeviceCaps(dc, VREFRESH);
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
	RunCodeAtScopeExit( ReleaseDC( m_hWindow, dc ) );

	m_iDesktopWidth = ::GetDeviceCaps(dc, HORZRES);
	m_iDesktopHeight = ::GetDeviceCaps(dc, VERTRES);
	m_iDesktopRefreshRate = ::GetDeviceCaps(dc, VREFRESH);
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

