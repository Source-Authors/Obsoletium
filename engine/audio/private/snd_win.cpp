//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Sound device dispatch and initialization.

#include "audio_pch.h"

#if defined( USE_SDL )
#include "snd_dev_sdl.h"
#endif
#ifdef OSX
#include "snd_dev_openal.h"
#include "snd_dev_mac_audioqueue.h"

ConVar snd_audioqueue( "snd_audioqueue", "1" );

#endif

#include "snd_dev_xaudio.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// dimhotepus: Allow to override sound device.
ConVar snd_device_override( "snd_device_override", "0", FCVAR_ARCHIVE,
	"Allow to override used sound device.\nPossible values:\n"
	"0\tAutodetection\n"
#ifdef PLATFORM_WINDOWS
	"1\tXAudio 2 [default]\n"
	"2\tDirect Sound 8\n",
#elif defined(OSX)
	"1\tMac Audio Queue [default]\n"
	"2\tOpen AL\n",
#elif defined(USE_SDL)
	"1\tSDL [default]",
#endif
	true,
	0,
	true,
#if defined(PLATFORM_WINDOWS)
	2
#elif defined(OSX)
	2
#elif defined(USE_SDL)
	1
#endif
);

/**
 * @brief Audio device kind.
 */
enum class AudioDeviceKind {
	/**
	 * @brief Autodetect.
	 */
	kAutodetect = 0
#ifdef PLATFORM_WINDOWS
	, kXAudio2 = 1
	, kDirectSound8 = 2
#elif defined(OSX)
	, kMacAudioQueue = 1
	, kOpenAL = 2
#elif defined(USE_SDL)
	, kSDL = 2
#endif
};

bool snd_firsttime = true;

/* 
 * Global variables. Must be visible to window-procedure function 
 *  so it can unlock and free the data block after it has been played. 
 */ 
IAudioDevice *g_AudioDevice = nullptr;

void S_BlockSound()
{
	if ( !g_AudioDevice )
		return;

	g_AudioDevice->Pause();
}

void S_UnblockSound()
{
	if ( !g_AudioDevice )
		return;

	g_AudioDevice->UnPause();
}

/**
 * @brief Autodetect and find sound device to mix on.
 * @param Unused.
 * @return Audio device to mix on. Null device when no sound.
 */
IAudioDevice *IAudioDevice::AutoDetectInit()
{
	IAudioDevice *pDevice = nullptr;

#if defined( WIN32 ) && !defined( USE_SDL )
	if ( snd_firsttime )
	{
		const auto device_override =
			static_cast<AudioDeviceKind>(snd_device_override.GetInt());

		switch (device_override) {
			default:
				Warning( "Unknown 'snd_device_override' con var value %d. Using autodetection.\n",
					to_underlying(device_override) );
				[[fallthrough]];

			case AudioDeviceKind::kAutodetect:
			case AudioDeviceKind::kXAudio2:
				{
					pDevice = Audio_CreateXAudioDevice();
					if ( pDevice )
					{
						// XAudio2 requires threaded mixing.
						S_EnableThreadedMixing( true );
					}
				}
				break;

			case AudioDeviceKind::kDirectSound8:
				pDevice = Audio_CreateDirectSoundDevice();
				break;
		}
	}
#elif defined(OSX)
	const auto device_override =
		static_cast<AudioDeviceKind>(snd_device_override.GetInt());

	switch (device_override) {
		default:
			Warning( "Unknown 'snd_device_override' con var value %d. Using autodetection.\n",
				to_underlying(device_override) );
			[[fallthrough]];

		case AudioDeviceKind::kAutodetect:
		case AudioDeviceKind::kMacAudioQueue:
			pDevice = Audio_CreateMacAudioQueueDevice();
			break;

		case AudioDeviceKind::kOpenAL:
			pDevice = Audio_CreateOpenALDevice();
			break;
	}

	if ( !pDevice )
	{
		// fall back to openAL if the audio queue fails
		DevMsg( "Using OpenAL Interface\n" );
		pDevice = Audio_CreateOpenALDevice();
	}
#elif defined( USE_SDL )
	DevMsg( "Trying SDL Audio Interface\n" );
	pDevice = Audio_CreateSDLAudioDevice();
#else
#error "Please define your platform"
#endif

	if ( !pDevice )
	{
		if ( snd_firsttime )
			DevMsg( "No sound device initialized.\n" );

		return Audio_GetNullDevice();
	}

	snd_firsttime = false;

	return pDevice;
}

void SNDDMA_Shutdown()
{
	if ( g_AudioDevice != Audio_GetNullDevice() )
	{
		if ( g_AudioDevice )
		{
			g_AudioDevice->Shutdown();
			delete g_AudioDevice;
		}

		// the NULL device is always valid
		g_AudioDevice = Audio_GetNullDevice();
	}
}
