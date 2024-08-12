//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Linux Joystick implementation for inputsystem.dll
//
//===========================================================================//

/* For force feedback testing. */
#ifdef _WIN32
#include "winlite.h"
#endif

#include "inputsystem.h"
#include "tier1/convar.h"
#include "tier0/icommandline.h"

#include "include/SDL3/SDL.h"
#include "include/SDL3/SDL_gamepad.h"
#include "include/SDL3/SDL_haptic.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

static ButtonCode_t ControllerButtonToButtonCode( SDL_GamepadButton button );
static AnalogCode_t ControllerAxisToAnalogCode( SDL_GamepadAxis axis );
static SDL_bool SDLCALL JoystickSDLWatcher(void *userInfo, SDL_Event *event);

ConVar joy_axisbutton_threshold( "joy_axisbutton_threshold", "0.3", FCVAR_ARCHIVE, "Analog axis range before a button press is registered." );
ConVar joy_axis_deadzone( "joy_axis_deadzone", "0.2", FCVAR_ARCHIVE, "Dead zone near the zero point to not report movement." );

static void joy_active_changed_f( IConVar *var, const char *pOldValue, float flOldValue );
ConVar joy_active( "joy_active", "-1", FCVAR_NONE, "Which of the connected joysticks / gamepads to use (-1 means first found)", &joy_active_changed_f);

static void joy_gamecontroller_config_changed_f( IConVar *var, const char *pOldValue, float flOldValue );
ConVar joy_gamecontroller_config( "joy_gamecontroller_config", "", FCVAR_ARCHIVE, "Game controller mapping (passed to SDL with SDL_HINT_GAMECONTROLLERCONFIG), can also be configured in Steam Big Picture mode.", &joy_gamecontroller_config_changed_f );

void SearchForDevice()
{
	CInputSystem *pInputSystem = static_cast<CInputSystem *>(g_pInputSystem);
	if ( !pInputSystem )
	{
		return;
	}

	unsigned newJoystickId = static_cast<unsigned>(joy_active.GetInt());	

	int joystickCount;
	SDL_JoystickID *joystickIds = SDL_GetJoysticks(&joystickCount);
	if (!joystickIds)
	{
		Warning("inputsystem: Unable to get joysticks. %s\n", SDL_GetError());
		return;
	}

	// -1 means "first available."
	if ( newJoystickId == std::numeric_limits<unsigned>::max() )
	{
		// dimhotepus: Do not pass 0 as it means invalid joystick id.
		// Pass first joystick.
		pInputSystem->JoystickHotplugAdded(*joystickIds);
		return;
	}

	for ( int device_index = 0; device_index < joystickCount; ++device_index )
	{
		SDL_JoystickID joystickId = *(joystickIds + device_index);
		SDL_Joystick *joystick = SDL_OpenJoystick(joystickId);
		if ( joystick == NULL )
		{
			Warning("inputsystem: Unable to open joystick %u. %s\n", joystickId, SDL_GetError());
			continue;
		}

		SDL_CloseJoystick(joystick);

		if ( joystickId == newJoystickId )
		{
			// dimhotepus: Expect joystick id here, not joystick index.
			pInputSystem->JoystickHotplugAdded(joystickId);
			break;
		}
	}

	SDL_free(joystickIds);
}

//---------------------------------------------------------------------------------------
// Switch our active joystick to another device
//---------------------------------------------------------------------------------------
void joy_active_changed_f( IConVar *, const char *, float )
{
	SearchForDevice();
}

//---------------------------------------------------------------------------------------
// Reinitialize the game controller layer when the joy_gamecontroller_config is updated.
//---------------------------------------------------------------------------------------
void joy_gamecontroller_config_changed_f( IConVar *var, const char *pOldValue, float )
{
	CInputSystem *pInputSystem = (CInputSystem *)g_pInputSystem;
	if ( pInputSystem && SDL_WasInit(SDL_INIT_GAMEPAD) )
	{
		bool oldValuePresent = pOldValue && !Q_isempty( pOldValue );
		bool newValuePresent = !Q_isempty( joy_gamecontroller_config.GetString() );
		if ( !oldValuePresent && !newValuePresent )
		{
			return;
		}

		// We need to reinitialize the whole thing (i.e. undo CInputSystem::InitializeJoysticks and then call it again)
		// due to SDL_Gamepad only reading the SDL_HINT_GAMECONTROLLERCONFIG on init.
		pInputSystem->ShutdownJoysticks();
		pInputSystem->InitializeJoysticks();
	}
}

//-----------------------------------------------------------------------------
// Handle the events coming from the GameController SDL subsystem.
//-----------------------------------------------------------------------------
SDL_bool SDLCALL JoystickSDLWatcher( void *userInfo, SDL_Event *event )
{
	CInputSystem *pInputSystem = (CInputSystem *)userInfo;
	Assert(pInputSystem != NULL);
	Assert(event != NULL);

	if ( event == NULL || pInputSystem == NULL )
	{
		Warning("inputsystem: No input system\n");
		return true;
	}

	switch ( event->type )
	{
		case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
		case SDL_EVENT_GAMEPAD_BUTTON_UP:
		case SDL_EVENT_GAMEPAD_ADDED:
		case SDL_EVENT_GAMEPAD_REMOVED:
			break;
		default:
			return true;
	}

	// This is executed on the same thread as SDL_PollEvent, as PollEvent
	// updates the joystick subsystem, which then calls SDL_PushEvent for
	// the various events below.  PushEvent invokes this callback.
	// SDL_PollEvent is called in PumpWindowsMessageLoop which is coming
	// from PollInputState_Linux, so there's no worry about calling
	// PostEvent (which doesn't seem to be thread safe) from other threads.
	Assert(ThreadInMainThread());

	switch ( event->type )
	{
		case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		{
			pInputSystem->JoystickAxisMotion(event->gaxis.which, event->gaxis.axis, event->gaxis.value);
			break;
		}

		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
			pInputSystem->JoystickButtonPress(event->gbutton.which, event->gbutton.button);
			break;
		case SDL_EVENT_GAMEPAD_BUTTON_UP:
			pInputSystem->JoystickButtonRelease(event->gbutton.which, event->gbutton.button);
			break;

		case SDL_EVENT_GAMEPAD_ADDED:
			pInputSystem->JoystickHotplugAdded(event->gdevice.which);
			break;
		case SDL_EVENT_GAMEPAD_REMOVED:
			pInputSystem->JoystickHotplugRemoved(event->gdevice.which);
			SearchForDevice();
			break;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Initialize all joysticks
//-----------------------------------------------------------------------------
void CInputSystem::InitializeJoysticks( void )
{
	if ( m_bJoystickInitialized )
	{
		ShutdownJoysticks();
	}

	// assume no joystick
	m_nJoystickCount = 0;
	memset( m_pJoystickInfo, 0, sizeof( m_pJoystickInfo ) );
	for ( auto &joy : m_pJoystickInfo )
	{
		joy.m_nDeviceId = std::numeric_limits<unsigned>::max();
	}

	// abort startup if user requests no joystick
	if ( CommandLine()->FindParm("-nojoy") ) return;

	const char *controllerConfig = joy_gamecontroller_config.GetString();
	if ( !Q_isempty(controllerConfig) )
	{
		DevMsg("inputsystem: Passing joy_gamecontroller_config to SDL ('%s').\n", controllerConfig);
		// We need to pass this hint to SDL *before* we init the gamecontroller subsystem, otherwise it gets ignored.
		SDL_SetHint(SDL_HINT_GAMECONTROLLERCONFIG, controllerConfig);
	}

	if ( SDL_InitSubSystem( SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC ) == false )
	{
		Warning("inputsystem: Joystick init failed -- SDL_Init(SDL_INIT_GAMEPAD|SDL_INIT_HAPTIC) failed: %s.\n", SDL_GetError());
		return;
	}

	m_bJoystickInitialized = true;

	SDL_AddEventWatch(JoystickSDLWatcher, this);
	
	int totalSticks;
	SDL_JoystickID *joystickIds = SDL_GetJoysticks(&totalSticks);
	for ( int i = 0; i < totalSticks; i++ )
	{
		SDL_JoystickID joystickId = *(joystickIds + i);

		if ( SDL_IsGamepad(joystickId) )
		{
			JoystickHotplugAdded(joystickId);
		} 
		else
		{
			SDL_GUID joyGUID = SDL_GetJoystickGUIDForID(joystickId);

			char szGUID[sizeof(joyGUID.data)*2 + 1];
			SDL_GUIDToString(joyGUID, szGUID, sizeof(szGUID));

			Msg("inputsystem: Found joystick '%s' (%s), but no recognized controller configuration for it.\n", SDL_GetJoystickNameForID(joystickId), szGUID);
		}
	}
	SDL_free(joystickIds);

	if ( totalSticks < 1 )
	{
		Msg("inputsystem: Did not detect any valid joysticks. %s\n", SDL_GetError());
	}
}

void CInputSystem::ShutdownJoysticks()
{
	if ( !m_bJoystickInitialized )
	{
		return;
	}

	SDL_RemoveEventWatch( JoystickSDLWatcher, this );
	if ( m_pJoystickInfo[ 0 ].m_pDevice != NULL )
	{
		JoystickHotplugRemoved( m_pJoystickInfo[ 0 ].m_nDeviceId );
	}
	SDL_QuitSubSystem( SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC );

	m_bJoystickInitialized = false;
}

// Update the joy_xcontroller_found convar to force CInput::JoyStickMove to re-exec 360controller-linux.cfg
static void SetJoyXControllerFound( bool found )
{
	static ConVarRef xcontrollerVar( "joy_xcontroller_found" );
	static ConVarRef joystickVar( "joystick" );
	if ( xcontrollerVar.IsValid() )
	{
		xcontrollerVar.SetValue(found);
	}

	if ( found && joystickVar.IsValid() )
	{
		joystickVar.SetValue(true);
	}
}

void CInputSystem::JoystickHotplugAdded( unsigned joystickId )
{
	int joystickCount;
	SDL_JoystickID *joystickIds = SDL_GetJoysticks(&joystickCount);
	if (!joystickIds)
	{
		Warning("inputsystem: Unable to get joysticks. %s\n", SDL_GetError());
		return;
	}
	SDL_free(joystickIds);

	if ( !SDL_IsGamepad(joystickId) )
	{
		Warning("inputsystem: Joystick %u is not recognized by the game controller system. You can configure the controller in Steam Big Picture mode.\n", joystickId );
		return;
	}

	SDL_Joystick *joystick = SDL_OpenJoystick(joystickId);
	if ( joystick == NULL )
	{
		Warning("inputsystem: Could not open joystick %u: %s", joystickId, SDL_GetError());
		return;
	}
	SDL_CloseJoystick(joystick);

	unsigned activeJoystick = static_cast<unsigned>(joy_active.GetInt());
	JoystickInfo_t& info = m_pJoystickInfo[ 0 ];
	if ( activeJoystick == std::numeric_limits<unsigned>::max() )
	{
		// Only opportunistically open devices if we don't have one open already.
		if ( info.m_nDeviceId != std::numeric_limits<unsigned>::max() )
		{
			Msg("inputsystem: Detected supported joystick %u '%s'. Currently active joystick is %u.\n", joystickId, SDL_GetJoystickNameForID(joystickId), info.m_nDeviceId);
			return;
		}
	}
	else if ( activeJoystick != joystickId )
	{
		Msg("inputsystem: Detected supported joystick %u '%s'. Currently active joystick is %u.\n", joystickId, SDL_GetJoystickNameForID(joystickId), activeJoystick);
		return;
	}

	if ( info.m_nDeviceId != std::numeric_limits<unsigned>::max() )
	{
		// Don't try to open the device we already have open.
		if ( info.m_nDeviceId == joystickId )
		{
			return;
		}

		DevMsg("inputsystem: Joystick %u already initialized, removing it first.\n", info.m_nDeviceId);
		JoystickHotplugRemoved(info.m_nDeviceId);
	}

	Msg("inputsystem: Initializing joystick %u and making it active.\n", joystickId);

	SDL_Gamepad *controller = SDL_OpenGamepad(joystickId);
	if ( controller == NULL )
	{
		Warning("inputsystem: Failed to open joystick %u: %s\n", joystickId, SDL_GetError());
		return;
	}

	// XXX: This will fail if this is a *real* hotplug event (and not coming from the initial InitializeJoysticks call).
	// That's because the SDL haptic subsystem currently doesn't do hotplugging. Everything but haptics will work fine.
	SDL_Haptic *haptic = SDL_OpenHapticFromJoystick(SDL_GetGamepadJoystick(controller));
	if ( haptic == NULL || SDL_InitHapticRumble(haptic) != 0 )
	{
		Warning("inputsystem: Unable to initialize rumble for joystick %u: %s\n", joystickId, SDL_GetError());
		haptic = NULL;
	}

	info.m_pDevice = controller;
	info.m_pHaptic = haptic;
	info.m_nDeviceId = SDL_GetJoystickID(SDL_GetGamepadJoystick(controller));
	info.m_nButtonCount = SDL_GAMEPAD_BUTTON_MAX;
	info.m_bRumbleEnabled = false;

	SetJoyXControllerFound(true);
	EnableJoystickInput(0, true);
	m_nJoystickCount = 1;
	m_bXController =  true;

	// We reset joy_active to -1 because joystick ids are never reused - until you restart.
	// Setting it to -1 means that you get expected hotplugging behavior if you disconnect the current joystick.
	joy_active.SetValue(-1);
}

void CInputSystem::JoystickHotplugRemoved( unsigned joystickId )
{
	JoystickInfo_t& info = m_pJoystickInfo[ 0 ];
	if ( info.m_nDeviceId != joystickId )
	{
		DevMsg("inputsystem: Ignoring hotplug remove for %u, active joystick is %u.\n", joystickId, info.m_nDeviceId);
		return;
	}

	if ( info.m_pDevice == NULL )
	{
		info.m_nDeviceId = std::numeric_limits<unsigned>::max();
		DevMsg("inputsystem: Got hotplug remove event for removed joystick %u, ignoring.\n", joystickId);
		return;
	}

	m_nJoystickCount = 0;
	m_bXController =  false;
	EnableJoystickInput(0, false);
	SetJoyXControllerFound(false);

	SDL_CloseHaptic((SDL_Haptic *)info.m_pHaptic);
	SDL_CloseGamepad((SDL_Gamepad *)info.m_pDevice);

	info.m_pHaptic = NULL;
	info.m_pDevice = NULL;
	info.m_nButtonCount = 0;
	info.m_nDeviceId = std::numeric_limits<unsigned>::max();
	info.m_bRumbleEnabled = false;

	Msg("inputsystem: Joystick %u removed.\n", joystickId);
}

void CInputSystem::JoystickButtonPress( unsigned joystickId, int button )
{
	JoystickInfo_t& info = m_pJoystickInfo[ 0 ];
	if ( info.m_nDeviceId != joystickId )
	{
		Warning("inputsystem: Not active device input system (%u x %u)\n", info.m_nDeviceId, joystickId);
		return;
	}

	ButtonCode_t buttonCode = ControllerButtonToButtonCode((SDL_GamepadButton)button);
	PostButtonPressedEvent(IE_ButtonPressed, m_nLastSampleTick, buttonCode, buttonCode);
}

void CInputSystem::JoystickButtonRelease( unsigned joystickId, int button )
{
	JoystickInfo_t& info = m_pJoystickInfo[ 0 ];
	if ( info.m_nDeviceId != joystickId )
	{
		return;
	}

	ButtonCode_t buttonCode = ControllerButtonToButtonCode((SDL_GamepadButton)button);
	PostButtonReleasedEvent(IE_ButtonReleased, m_nLastSampleTick, buttonCode, buttonCode);
}


void CInputSystem::JoystickAxisMotion( unsigned joystickId, uint8 axis, int16 value )
{
	JoystickInfo_t& info = m_pJoystickInfo[ 0 ];
	if ( info.m_nDeviceId != joystickId )
	{
		return;
	}

	AnalogCode_t code = ControllerAxisToAnalogCode((SDL_GamepadAxis)axis);
	if ( code == ANALOG_CODE_INVALID )
	{
		Warning("inputsystem: Invalid code for axis %hhu\n", axis);
		return;
	}

	ButtonCode_t buttonCode = BUTTON_CODE_NONE;
	switch ( axis )
	{
		case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
			buttonCode = KEY_XBUTTON_RTRIGGER;
			break;
		case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
			buttonCode = KEY_XBUTTON_LTRIGGER;
			break;
	}

	if ( buttonCode != BUTTON_CODE_NONE )
	{
		int pressThreshold = static_cast<int>(joy_axisbutton_threshold.GetFloat() * 32767);
		int keyIndex = buttonCode - KEY_XBUTTON_LTRIGGER;
		Assert( keyIndex < ssize( m_appXKeys[0] ) && keyIndex >= 0 );

		appKey_t &key = m_appXKeys[0][keyIndex];
		if ( value > pressThreshold )
		{
			if ( key.repeats < 1 )
			{
				PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, buttonCode, buttonCode );
			}
			key.repeats++;
		}
		else
		{
			PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, buttonCode, buttonCode );
			key.repeats = 0;
		}
	}

	int minValue = static_cast<int>(joy_axis_deadzone.GetFloat() * 32767);
	if ( abs(value) < minValue )
	{
		value = 0;
	}

	InputState_t& state = m_InputState[ m_bIsPolling ];
	state.m_pAnalogDelta[ code ] = value - state.m_pAnalogValue[ code ];
	state.m_pAnalogValue[ code ] = value;
	if ( state.m_pAnalogDelta[ code ] != 0 )
	{
		PostEvent(IE_AnalogValueChanged, m_nLastSampleTick, code, value, 0);
	}
}

//-----------------------------------------------------------------------------
//	Process the event
//-----------------------------------------------------------------------------
void CInputSystem::JoystickButtonEvent( ButtonCode_t, int )
{
	// Not used - we post button events from JoystickButtonPress/Release.
}


//-----------------------------------------------------------------------------
// Update the joystick button state
//-----------------------------------------------------------------------------
void CInputSystem::UpdateJoystickButtonState( int )
{
	// We don't sample - we get events posted by SDL_Gamepad in JoystickSDLWatcher
}


//-----------------------------------------------------------------------------
// Update the joystick POV control
//-----------------------------------------------------------------------------
void CInputSystem::UpdateJoystickPOVControl( int )
{
	// SDL GameController does not support joystick POV. Should we poll?
}


//-----------------------------------------------------------------------------
// Purpose: Sample the joystick
//-----------------------------------------------------------------------------
void CInputSystem::PollJoystick( void )
{
	// We only pump the SDL event loop if we're not an SDL app, since otherwise PollInputState_Platform calls into CSDLMgr to pump it.
	// Our state updates happen in events posted by SDL_Gamepad in JoystickSDLWatcher, so the loop is empty.
#if !defined( USE_SDL )
	SDL_Event event;
	int nEventsProcessed = 0;

	SDL_PumpEvents();
	while ( SDL_PollEvent( &event ) && nEventsProcessed < 100 )
	{
		nEventsProcessed++;
	}
#endif
}

void CInputSystem::SetXDeviceRumble( float fLeftMotor, float fRightMotor, int userId )
{
	JoystickInfo_t& info = m_pJoystickInfo[ 0 ];
	if ( info.m_nDeviceId == std::numeric_limits<unsigned>::max() || info.m_pHaptic == NULL )
	{
		return;
	}

	float strength = (fLeftMotor + fRightMotor) / 2.f;
	static ConVarRef joystickVar( "joystick" );

	// 0f means "stop".
	bool shouldStop = ( strength < 0.01f );
	// If they've disabled the gamecontroller in settings, never rumble.
	if ( !joystickVar.IsValid() || !joystickVar.GetBool() )
	{
		shouldStop = true;
	}

	if ( shouldStop )
	{
		if ( info.m_bRumbleEnabled )
		{
			SDL_StopHapticRumble( (SDL_Haptic *)info.m_pHaptic );
			info.m_bRumbleEnabled = false;
			info.m_fCurrentRumble = 0.0f;
		}

		return;
	}

	// If there's little change, then don't change the rumble strength.
	if ( info.m_bRumbleEnabled && abs(info.m_fCurrentRumble - strength) < 0.01f )
	{
		return;
	}

	info.m_bRumbleEnabled = true;
	info.m_fCurrentRumble = strength;

	if ( SDL_PlayHapticRumble((SDL_Haptic *)info.m_pHaptic, strength, SDL_HAPTIC_INFINITY) != 0 )
	{
		Warning("inputsystem: Couldn't play rumble (strength %.1f): %s\n", strength, SDL_GetError());
	}
}

ButtonCode_t ControllerButtonToButtonCode( SDL_GamepadButton button )
{
	switch ( button )
	{
		case SDL_GAMEPAD_BUTTON_SOUTH: // KEY_XBUTTON_A
		case SDL_GAMEPAD_BUTTON_EAST: // KEY_XBUTTON_B
		case SDL_GAMEPAD_BUTTON_WEST: // KEY_XBUTTON_X
		case SDL_GAMEPAD_BUTTON_NORTH: // KEY_XBUTTON_Y
			return JOYSTICK_BUTTON(0, button);

		case SDL_GAMEPAD_BUTTON_BACK:
			return KEY_XBUTTON_BACK;
		case SDL_GAMEPAD_BUTTON_START:
			return KEY_XBUTTON_START;

		case SDL_GAMEPAD_BUTTON_GUIDE:
			return KEY_XBUTTON_BACK; // XXX: How are we supposed to handle this? Steam overlay etc.

		case SDL_GAMEPAD_BUTTON_LEFT_STICK:
			return KEY_XBUTTON_STICK1;
		case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
			return KEY_XBUTTON_STICK2;
		case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
			return KEY_XBUTTON_LEFT_SHOULDER;
		case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
			return KEY_XBUTTON_RIGHT_SHOULDER;

		case SDL_GAMEPAD_BUTTON_DPAD_UP:
			return KEY_XBUTTON_UP;
		case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
			return KEY_XBUTTON_DOWN;
		case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
			return KEY_XBUTTON_LEFT;
		case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
			return KEY_XBUTTON_RIGHT;
	}

	return BUTTON_CODE_NONE;
}

AnalogCode_t ControllerAxisToAnalogCode( SDL_GamepadAxis axis )
{
	switch ( axis )
	{
		case SDL_GAMEPAD_AXIS_LEFTX:
			return JOYSTICK_AXIS(0, JOY_AXIS_X);
		case SDL_GAMEPAD_AXIS_LEFTY:
			return JOYSTICK_AXIS(0, JOY_AXIS_Y);

		case SDL_GAMEPAD_AXIS_RIGHTX:
			return JOYSTICK_AXIS(0, JOY_AXIS_U);
		case SDL_GAMEPAD_AXIS_RIGHTY:
			return JOYSTICK_AXIS(0, JOY_AXIS_R);

		case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
		case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
			return JOYSTICK_AXIS(0, JOY_AXIS_Z);
	}

	return ANALOG_CODE_INVALID;
}
