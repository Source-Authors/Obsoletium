//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "audio_pch.h"

#include <dsound.h>
#include <ks.h>
#include <ksmedia.h>

#include <system_error>

#include "com_ptr.h"
#include "iprediction.h"
#include "eax.h"
#include "tier0/icommandline.h"
#include "video//ivideoservices.h"
#include "../../sys_dll.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern bool snd_firsttime;

extern void DEBUG_StartSoundMeasure(int type, int samplecount );
extern void DEBUG_StopSoundMeasure(int type, int samplecount );

// legacy support
extern ConVar sxroom_off;
extern ConVar sxroom_type;
extern ConVar sxroomwater_type;
extern float sxroom_typeprev;

extern HWND* pmainwindow;

typedef enum {SIS_SUCCESS, SIS_FAILURE, SIS_NOTAVAIL} sndinitstat;

#define SECONDARY_BUFFER_SIZE			0x10000		// output buffer size in bytes
#define SECONDARY_BUFFER_SIZE_SURROUND	0x04000		// output buffer size in bytes, one per channel

extern void ReleaseSurround();
extern bool MIX_ScaleChannelVolume( paintbuffer_t *ppaint, channel_t *pChannel, int volume[CCHANVOLUMES], int mixchans );

void OnSndSurroundCvarChanged( IConVar *var, const char *pOldString, float flOldValue );
void OnSndSurroundLegacyChanged( IConVar *var, const char *pOldString, float flOldValue );
void OnSndVarChanged( IConVar *var, const char *pOldString, float flOldValue );

static LPDIRECTSOUND8 pDS = nullptr;
static LPDIRECTSOUNDBUFFER pDSBuf = nullptr;
// For the primary buffer, you must use the IDirectSoundBuffer interface; IDirectSoundBuffer8 is not available.
// See https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee418055(v=vs.85)
static LPDIRECTSOUNDBUFFER pDSPBuf = nullptr;

static GUID IID_IDirectSound3DBufferDef = {0x279AFA86, 0x4981, 0x11CE, {0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60}};
static ConVar windows_speaker_config("windows_speaker_config", "-1", FCVAR_ARCHIVE);
static DWORD g_ForcedSpeakerConfig = 0;

extern ConVar snd_mute_losefocus;

[[nodiscard]] constexpr short AdjustSampleVolume(int sample,
                                                 int volume_factor) noexcept {
  return CLIP((sample * volume_factor) >> 8);
}

//-----------------------------------------------------------------------------
// Purpose: Implementation of direct sound
//-----------------------------------------------------------------------------
class CAudioDirectSound : public CAudioDeviceBase
{
public:
	~CAudioDirectSound( void );
	bool		IsActive( void ) { return true; }
	bool		Init( void );
	void		Shutdown( void );
	void		Pause( void );
	void		UnPause( void );
	float		MixDryVolume( void );
	bool		Should3DMix( void );
	void		StopAllSounds( void );

	int			PaintBegin( float mixAheadTime, int soundtime, int paintedtime );
	void		PaintEnd( void );

	int			GetOutputPosition( void );
	void		ClearBuffer( void );
	void		UpdateListener( const Vector& position, const Vector& forward, const Vector& right, const Vector& up );

	void		ChannelReset( int entnum, int channelIndex, float distanceMod );
	void		TransferSamples( int end );

	const char *DeviceName( void );
	int			DeviceChannels( void )		{ return m_deviceChannels; }
	int			DeviceSampleBits( void )	{ return m_deviceSampleBits; }
	int			DeviceSampleBytes( void )	{ return m_deviceSampleBits/8; }
	int			DeviceDmaSpeed( void )		{ return m_deviceDmaSpeed; }
	int			DeviceSampleCount( void )	{ return m_deviceSampleCount; }

	bool		IsInterleaved() { return m_isInterleaved; }

	// Singleton object
	static		CAudioDirectSound *m_pSingleton;

private:
	void		DetectWindowsSpeakerSetup();
	bool		LockDSBuffer( LPDIRECTSOUNDBUFFER pBuffer, DWORD **pdwWriteBuffer, DWORD *pdwSizeBuffer, const char *pBufferName, int lockFlags = 0 );
	bool		IsUsingBufferPerSpeaker();

	sndinitstat SNDDMA_InitDirect( void );
	bool		SNDDMA_InitInterleaved( LPDIRECTSOUND8 lpDS, WAVEFORMATEX* lpFormat, int channelCount );
	bool		SNDDMA_InitSurround(LPDIRECTSOUND8 lpDS, WAVEFORMATEX* lpFormat, DSBCAPS* lpdsbc, int cchan);
	void		S_TransferSurround16( portable_samplepair_t *pfront, portable_samplepair_t *prear, portable_samplepair_t *pcenter, int lpaintedtime, int endtime, int cchan);
	void		S_TransferSurround16Interleaved( portable_samplepair_t *pfront, portable_samplepair_t *prear, portable_samplepair_t *pcenter, int lpaintedtime, int endtime);
	void		S_TransferSurround16Interleaved_FullLock( const portable_samplepair_t *pfront, const portable_samplepair_t *prear, const portable_samplepair_t *pcenter, int lpaintedtime, int endtime);

	int			m_deviceChannels;					// channels per hardware output buffer (1 for quad/5.1, 2 for stereo)
	int			m_deviceSampleBits;					// bits per sample (16)
	int			m_deviceSampleCount;				// count of mono samples in output buffer
	int			m_deviceDmaSpeed;					// samples per second per output buffer
	int			m_bufferSizeBytes;					// size of a single hardware output buffer, in bytes
	
	DWORD		m_outputBufferStartOffset;						// output buffer playback starting byte offset
	HINSTANCE	m_hInstDS;
	bool		m_isInterleaved;
};

CAudioDirectSound *CAudioDirectSound::m_pSingleton = NULL;

LPDIRECTSOUNDBUFFER8 pDSBufFL = NULL;
LPDIRECTSOUNDBUFFER8 pDSBufFR = NULL;
LPDIRECTSOUNDBUFFER8 pDSBufRL = NULL;
LPDIRECTSOUNDBUFFER8 pDSBufRR = NULL;
LPDIRECTSOUNDBUFFER8 pDSBufFC = NULL;
LPDIRECTSOUND3DBUFFER8 pDSBuf3DFL = NULL;
LPDIRECTSOUND3DBUFFER8 pDSBuf3DFR = NULL;
LPDIRECTSOUND3DBUFFER8 pDSBuf3DRL = NULL;
LPDIRECTSOUND3DBUFFER8 pDSBuf3DRR = NULL;
LPDIRECTSOUND3DBUFFER8 pDSBuf3DFC = NULL;

// ----------------------------------------------------------------------------- //
// Helpers.
// ----------------------------------------------------------------------------- //


CAudioDirectSound::~CAudioDirectSound( void )
{
	m_pSingleton = NULL;
}

bool CAudioDirectSound::Init( void )
{
	m_hInstDS = NULL;

	static bool first = true;
	if ( first )
	{
		snd_surround.InstallChangeCallback( &OnSndSurroundCvarChanged );
		snd_legacy_surround.InstallChangeCallback( &OnSndSurroundLegacyChanged );
		snd_mute_losefocus.InstallChangeCallback( &OnSndVarChanged );
		first = false;
	}

	if ( SNDDMA_InitDirect() == SIS_SUCCESS )
	{
		if ( g_pVideo )
		{
			g_pVideo->SoundDeviceCommand( VideoSoundDeviceOperation::SET_DIRECT_SOUND_DEVICE, pDS );
		}

		return true;
	}

	return false;
}

void CAudioDirectSound::Shutdown()
{
	// dimhotepus: Notify video we shut down sound device.
	if ( g_pVideo )
	{
		g_pVideo->SoundDeviceCommand( VideoSoundDeviceOperation::SET_DIRECT_SOUND_DEVICE, nullptr );
	}

	ReleaseSurround();

	if (pDSBuf)
	{
		pDSBuf->Stop();
		pDSBuf->Release();
	}

	// only release primary buffer if it's not also the mixing buffer we just released
	if (pDSPBuf && (pDSBuf != pDSPBuf))
	{
		pDSPBuf->Release();
	}

	if (pDS)
	{
		pDS->SetCooperativeLevel(*pmainwindow, DSSCL_NORMAL);
		pDS->Release();
	}

	pDS = NULL;
	pDSBuf = NULL;
	pDSPBuf = NULL;

	if ( m_hInstDS )
	{
		FreeLibrary( m_hInstDS );
		m_hInstDS = NULL;
	}

	if ( this == CAudioDirectSound::m_pSingleton )
	{
		CAudioDirectSound::m_pSingleton = NULL;
	}
}

// Total number of samples that have played out to hardware
// for current output buffer (ie: from buffer offset start).
// return playback position within output playback buffer:
// the output units are dependant on the device channels
// so the ouput units for a 2 channel device are as 16 bit LR pairs
// and the output unit for a 1 channel device are as 16 bit mono samples.
// take into account the original start position within the buffer, and 
// calculate difference between current position (with buffer wrap) and 
// start position.
int	CAudioDirectSound::GetOutputPosition( void )
{
	int start, current;
	DWORD dwCurrent;

	// get size in bytes of output buffer
	const int size_bytes = m_bufferSizeBytes; 
	if ( IsUsingBufferPerSpeaker() )
	{
		// mono output buffers
		// get byte offset of playback cursor in Front Left output buffer
		pDSBufFL->GetCurrentPosition(&dwCurrent, NULL);

		start = (int) m_outputBufferStartOffset;
		current = (int) dwCurrent;
	} 
	else
	{
		// multi-channel interleavd output buffer 
		// get byte offset of playback cursor in output buffer
		pDSBuf->GetCurrentPosition(&dwCurrent, NULL);

		start = (int) m_outputBufferStartOffset;
		current = (int) dwCurrent;
	}
	
	int samp16;
	// get 16 bit samples played, relative to buffer starting offset
	if (current > start)
	{
		// get difference & convert to 16 bit mono samples
		samp16 = (current - start) >> SAMPLE_16BIT_SHIFT;
	}
	else
	{
		// get difference (with buffer wrap) convert to 16 bit mono samples
		samp16 = ((size_bytes - start) + current) >> SAMPLE_16BIT_SHIFT;
	}

	int outputPosition = samp16 / DeviceChannels();

	return outputPosition;
}

void CAudioDirectSound::Pause( void )
{
	if (pDSBuf)
	{
		pDSBuf->Stop();
	}

	if ( pDSBufFL ) pDSBufFL->Stop(); 
	if ( pDSBufFR ) pDSBufFR->Stop(); 
	if ( pDSBufRL ) pDSBufRL->Stop(); 
	if ( pDSBufRR ) pDSBufRR->Stop(); 
	if ( pDSBufFC ) pDSBufFC->Stop(); 
}


void CAudioDirectSound::UnPause( void )
{
	if (pDSBuf)
		pDSBuf->Play(0, 0, DSBPLAY_LOOPING);

	if (pDSBufFL) pDSBufFL->Play(0, 0, DSBPLAY_LOOPING); 
	if (pDSBufFR) pDSBufFR->Play(0, 0, DSBPLAY_LOOPING); 
	if (pDSBufRL) pDSBufRL->Play(0, 0, DSBPLAY_LOOPING); 
	if (pDSBufRR) pDSBufRR->Play( 0, 0, DSBPLAY_LOOPING); 
	if (pDSBufFC) pDSBufFC->Play( 0, 0, DSBPLAY_LOOPING); 
}


float CAudioDirectSound::MixDryVolume( void )
{
	return 0;
}


bool CAudioDirectSound::Should3DMix( void )
{
	return m_bSurround;
}


IAudioDevice *Audio_CreateDirectSoundDevice( void )
{
	if ( !CAudioDirectSound::m_pSingleton )
		CAudioDirectSound::m_pSingleton = new CAudioDirectSound;

	if ( CAudioDirectSound::m_pSingleton->Init() )
	{
		if (snd_firsttime)
			DevMsg ("DirectSound initialized\n");

		return CAudioDirectSound::m_pSingleton;
	}

	DevMsg ("DirectSound failed to init\n");

	delete CAudioDirectSound::m_pSingleton;
	CAudioDirectSound::m_pSingleton = NULL;

	return NULL;
}

int CAudioDirectSound::PaintBegin( float mixAheadTime, int soundtime, int lpaintedtime )
{
	//  soundtime - total full samples that have been played out to hardware at dmaspeed
	//  paintedtime - total full samples that have been mixed at speed
	//  endtime - target for full samples in mixahead buffer at speed
	//  samps - size of output buffer in full samples
	
	int mixaheadtime = mixAheadTime * DeviceDmaSpeed();
	int endtime = soundtime + mixaheadtime;

	if ( endtime <= lpaintedtime )
		return endtime;

	uint nSamples = endtime - lpaintedtime;
	if ( nSamples & 0x3 )
	{
		// The difference between endtime and painted time should align on 
		// boundaries of 4 samples.  This is important when upsampling from 11khz -> 44khz.
		nSamples += (4 - (nSamples & 3));
	}
	// clamp to min 512 samples per mix
	if ( nSamples > 0 && nSamples < 512 )
	{
		nSamples = 512;
	}
	endtime = lpaintedtime + nSamples;

	int fullsamps = DeviceSampleCount() / DeviceChannels();
	if ( (endtime - soundtime) > fullsamps)
	{
		endtime = soundtime + fullsamps;
		endtime += (4 - (endtime & 3));
	}

	DWORD	dwStatus;

	// If using surround, there are 4 or 5 different buffers being used and the pDSBuf is NULL.
	if ( IsUsingBufferPerSpeaker() ) 
	{
		if (pDSBufFL->GetStatus(&dwStatus) != DS_OK)
			Msg ("Couldn't get SURROUND FL sound buffer status\n");
		
		if (dwStatus & DSBSTATUS_BUFFERLOST)
			pDSBufFL->Restore();
		
		if (!(dwStatus & DSBSTATUS_PLAYING))
			pDSBufFL->Play(0, 0, DSBPLAY_LOOPING);

		if (pDSBufFR->GetStatus(&dwStatus) != DS_OK)
			Msg ("Couldn't get SURROUND FR sound buffer status\n");
		
		if (dwStatus & DSBSTATUS_BUFFERLOST)
			pDSBufFR->Restore();
		
		if (!(dwStatus & DSBSTATUS_PLAYING))
			pDSBufFR->Play(0, 0, DSBPLAY_LOOPING);

		if (pDSBufRL->GetStatus(&dwStatus) != DS_OK)
			Msg ("Couldn't get SURROUND RL sound buffer status\n");
		
		if (dwStatus & DSBSTATUS_BUFFERLOST)
			pDSBufRL->Restore();
		
		if (!(dwStatus & DSBSTATUS_PLAYING))
			pDSBufRL->Play(0, 0, DSBPLAY_LOOPING);

		if (pDSBufRR->GetStatus(&dwStatus) != DS_OK)
			Msg ("Couldn't get SURROUND RR sound buffer status\n");
		
		if (dwStatus & DSBSTATUS_BUFFERLOST)
			pDSBufRR->Restore();
		
		if (!(dwStatus & DSBSTATUS_PLAYING))
			pDSBufRR->Play(0, 0, DSBPLAY_LOOPING);

		if ( m_bSurroundCenter )
		{
			if (pDSBufFC->GetStatus(&dwStatus) != DS_OK)
				Msg ("Couldn't get SURROUND FC sound buffer status\n");
			
			if (dwStatus & DSBSTATUS_BUFFERLOST)
				pDSBufFC->Restore();
			
			if (!(dwStatus & DSBSTATUS_PLAYING))
				pDSBufFC->Play(0, 0, DSBPLAY_LOOPING);
		}
	}
	else if (pDSBuf)
	{
		if ( pDSBuf->GetStatus (&dwStatus) != DS_OK )
			Msg("Couldn't get sound buffer status\n");

		if ( dwStatus & DSBSTATUS_BUFFERLOST )
			pDSBuf->Restore();

		if ( !(dwStatus & DSBSTATUS_PLAYING) )
			pDSBuf->Play(0, 0, DSBPLAY_LOOPING);
	}

	return endtime;
}


void CAudioDirectSound::PaintEnd( void )
{
}


void CAudioDirectSound::ClearBuffer( void )
{
	int		clear;

	DWORD	dwSizeFL, dwSizeFR, dwSizeRL, dwSizeRR, dwSizeFC;
	char	*pDataFL, *pDataFR, *pDataRL, *pDataRR, *pDataFC;

	dwSizeFC = 0;		// compiler warning
	pDataFC = NULL;

	if ( IsUsingBufferPerSpeaker() )
	{
		int		SURROUNDreps;
		HRESULT	SURROUNDhresult;
		SURROUNDreps = 0;

		if ( !pDSBufFL && !pDSBufFR && !pDSBufRL && !pDSBufRR && !pDSBufFC )
			return;

		while ((SURROUNDhresult = pDSBufFL->Lock(0, m_bufferSizeBytes, (void**)&pDataFL, &dwSizeFL, NULL, NULL, 0)) != DS_OK)
		{
			if (SURROUNDhresult != DSERR_BUFFERLOST)
			{
				Msg ("S_ClearBuffer: DS::Lock FL Sound Buffer Failed\n");
				S_Shutdown ();
				return;
			}

			if (++SURROUNDreps > 10000)
			{
				Msg ("S_ClearBuffer: DS: couldn't restore FL buffer\n");
				S_Shutdown ();
				return;
			}
		}

		SURROUNDreps = 0;
		while ((SURROUNDhresult = pDSBufFR->Lock(0, m_bufferSizeBytes, (void**)&pDataFR, &dwSizeFR, NULL, NULL, 0)) != DS_OK)
		{
			if (SURROUNDhresult != DSERR_BUFFERLOST)
			{
				Msg ("S_ClearBuffer: DS::Lock FR Sound Buffer Failed\n");
				S_Shutdown ();
				return;
			}

			if (++SURROUNDreps > 10000)
			{
				Msg ("S_ClearBuffer: DS: couldn't restore FR buffer\n");
				S_Shutdown ();
				return;
			}
		}

		SURROUNDreps = 0;
		while ((SURROUNDhresult = pDSBufRL->Lock(0, m_bufferSizeBytes, (void**)&pDataRL, &dwSizeRL, NULL, NULL, 0)) != DS_OK)
		{
			if (SURROUNDhresult != DSERR_BUFFERLOST)
			{
				Msg ("S_ClearBuffer: DS::Lock RL Sound Buffer Failed\n");
				S_Shutdown ();
				return;
			}

			if (++SURROUNDreps > 10000)
			{
				Msg ("S_ClearBuffer: DS: couldn't restore RL buffer\n");
				S_Shutdown ();
				return;
			}
		}

		SURROUNDreps = 0;
		while ((SURROUNDhresult = pDSBufRR->Lock(0, m_bufferSizeBytes, (void**)&pDataRR, &dwSizeRR, NULL, NULL, 0)) != DS_OK)
		{
			if (SURROUNDhresult != DSERR_BUFFERLOST)
			{
				Msg ("S_ClearBuffer: DS::Lock RR Sound Buffer Failed\n");
				S_Shutdown ();
				return;
			}

			if (++SURROUNDreps > 10000)
			{
				Msg ("S_ClearBuffer: DS: couldn't restore RR buffer\n");
				S_Shutdown ();
				return;
			}
		}

		if (m_bSurroundCenter)
		{
			SURROUNDreps = 0;
			while ((SURROUNDhresult = pDSBufFC->Lock(0, m_bufferSizeBytes, (void**)&pDataFC, &dwSizeFC, NULL, NULL, 0)) != DS_OK)
			{
				if (SURROUNDhresult != DSERR_BUFFERLOST)
				{
					Msg ("S_ClearBuffer: DS::Lock FC Sound Buffer Failed\n");
					S_Shutdown ();
					return;
				}

				if (++SURROUNDreps > 10000)
				{
					Msg ("S_ClearBuffer: DS: couldn't restore FC buffer\n");
					S_Shutdown ();
					return;
				}
			}
		}

		Q_memset(pDataFL, 0, m_bufferSizeBytes);
		Q_memset(pDataFR, 0, m_bufferSizeBytes);
		Q_memset(pDataRL, 0, m_bufferSizeBytes);
		Q_memset(pDataRR, 0, m_bufferSizeBytes);

		if (m_bSurroundCenter)
			Q_memset(pDataFC, 0, m_bufferSizeBytes);

		pDSBufFL->Unlock(pDataFL, dwSizeFL, NULL, 0);
		pDSBufFR->Unlock(pDataFR, dwSizeFR, NULL, 0);
		pDSBufRL->Unlock(pDataRL, dwSizeRL, NULL, 0);
		pDSBufRR->Unlock(pDataRR, dwSizeRR, NULL, 0);

		if (m_bSurroundCenter)
			pDSBufFC->Unlock(pDataFC, dwSizeFC, NULL, 0);

		return;
	}
		
	if ( !pDSBuf )
		return;

	if ( DeviceSampleBits() == 8 )
		clear = 0x80;
	else
		clear = 0;

	DWORD	dwSize;
	DWORD	*pData;
	int		reps;
	HRESULT	hresult;

	reps = 0;
	while ((hresult = pDSBuf->Lock(0, m_bufferSizeBytes, (void**)&pData, &dwSize, NULL, NULL, 0)) != DS_OK)
	{
		if (hresult != DSERR_BUFFERLOST)
		{
			Msg("S_ClearBuffer: DS::Lock Sound Buffer Failed\n");
			S_Shutdown();
			return;
		}

		if (++reps > 10000)
		{
			Msg("S_ClearBuffer: DS: couldn't restore buffer\n");
			S_Shutdown();
			return;
		}
	}

	Q_memset(pData, clear, dwSize);

	pDSBuf->Unlock(pData, dwSize, NULL, 0);
}

void CAudioDirectSound::StopAllSounds( void )
{
}

bool CAudioDirectSound::SNDDMA_InitInterleaved( LPDIRECTSOUND8 lpDS, WAVEFORMATEX* lpFormat, int channelCount )
{
	WAVEFORMATEXTENSIBLE    wfx = {};     // DirectSoundBuffer wave format (extensible)

    // set the channel mask and number of channels based on the command line parameter
    if(channelCount == 2)
    {
        wfx.Format.nChannels = 2;
        wfx.dwChannelMask = KSAUDIO_SPEAKER_STEREO;   // SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    }
    else if(channelCount == 4)
    {
        wfx.Format.nChannels = 4;
        wfx.dwChannelMask = KSAUDIO_SPEAKER_QUAD;     // SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
    }
    else if(channelCount == 6)
    {
        wfx.Format.nChannels = 6;
        wfx.dwChannelMask = KSAUDIO_SPEAKER_5POINT1;  // SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
    }
    else
    {
        return false;
    }

    // setup the extensible structure
    wfx.Format.wFormatTag             = WAVE_FORMAT_EXTENSIBLE; 
  //wfx.Format.nChannels              = SET ABOVE 
    wfx.Format.nSamplesPerSec         = lpFormat->nSamplesPerSec;
    wfx.Format.wBitsPerSample         = lpFormat->wBitsPerSample; 
    wfx.Format.nBlockAlign            = wfx.Format.wBitsPerSample / 8 * wfx.Format.nChannels;
    wfx.Format.nAvgBytesPerSec        = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
    wfx.Format.cbSize                 = 22; // size from after this to end of extensible struct. sizeof(WORD + DWORD + GUID)
    wfx.Samples.wValidBitsPerSample   = lpFormat->wBitsPerSample;
  //wfx.dwChannelMask                 = SET ABOVE BASED ON COMMAND LINE PARAMETERS
    wfx.SubFormat                     = KSDATAFORMAT_SUBTYPE_PCM;

    // setup the DirectSound
    DSBUFFERDESC            dsbdesc = {};  // DirectSoundBuffer descriptor	
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = 0;

    dsbdesc.dwBufferBytes = SECONDARY_BUFFER_SIZE_SURROUND * channelCount;
 
    dsbdesc.lpwfxFormat = (WAVEFORMATEX*)&wfx;
	bool bSuccess = false;
	for ( int i = 0; i < 3; i++ )
	{
		switch(i)
		{
		case 0:
			dsbdesc.dwFlags = DSBCAPS_LOCHARDWARE;
			break;
		case 1:
			dsbdesc.dwFlags = DSBCAPS_LOCSOFTWARE;
			break;
		case 2:
			dsbdesc.dwFlags = 0;
			break;
		}
		
		dsbdesc.dwFlags |= DSBCAPS_GLOBALFOCUS;

		se::win::com::com_ptr<IDirectSoundBuffer, &IID_IDirectSoundBuffer> buffer;
		if(!FAILED(lpDS->CreateSoundBuffer(&dsbdesc, &buffer, NULL)) &&
		   !FAILED(buffer.QueryInterface(IID_IDirectSoundBuffer8, &pDSBuf)))
		{
				bSuccess = true;
				break;
		}
	}
	if ( !bSuccess )
		return false;

	DWORD dwSize = 0, dwWrite;
	DWORD *pBuffer = 0;
	if ( !LockDSBuffer( pDSBuf, &pBuffer, &dwSize, "DS_INTERLEAVED", DSBLOCK_ENTIREBUFFER ) )
		return false;

	m_deviceChannels = wfx.Format.nChannels;
	m_deviceSampleBits = wfx.Format.wBitsPerSample;
	m_deviceDmaSpeed = wfx.Format.nSamplesPerSec;
	m_bufferSizeBytes = dsbdesc.dwBufferBytes;
	m_isInterleaved = true;

	Q_memset( pBuffer, 0, dwSize );

	pDSBuf->Unlock(pBuffer, dwSize, NULL, 0);
	
	// Make sure mixer is active (this was moved after the zeroing to avoid popping on startup -- at least when using the dx9.0b debug .dlls)
	pDSBuf->Play(0, 0, DSBPLAY_LOOPING);

	pDSBuf->Stop();
	pDSBuf->GetCurrentPosition(&m_outputBufferStartOffset, &dwWrite);

	pDSBuf->Play(0, 0, DSBPLAY_LOOPING);

	return true;
}

/*
==================
SNDDMA_InitDirect

Direct-Sound support
==================
*/
sndinitstat CAudioDirectSound::SNDDMA_InitDirect( void )
{
	if (!m_hInstDS)
	{
		m_hInstDS = LoadLibraryExA( "dsound.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 );
		if (!m_hInstDS)
		{
			Warning( "dsound [init]: Unable to load 'dsound.dll': %s.\n",
				std::system_category().message( GetLastError() ).c_str() );
			return SIS_FAILURE;
		}
	}
	
	using DirectSoundCreate8Function = decltype(&DirectSoundCreate8);
	const auto direct_sound_create8 =
		reinterpret_cast<DirectSoundCreate8Function>(
			GetProcAddress( m_hInstDS, V_STRINGIFY(DirectSoundCreate8) ) );
	if (!direct_sound_create8)
	{
		Warning( "dsound [init]: Unable to get '%s' from 'dsound.dll': %s.\n",
			V_STRINGIFY(DirectSoundCreate8),
			std::system_category().message( GetLastError() ).c_str() );
		return SIS_FAILURE;
	}

	HRESULT hr;
	if (FAILED(hr = direct_sound_create8(nullptr, &pDS, nullptr)))
	{
		// The request failed because resources, such as a priority level,
		// were already in use by another caller.
		if (hr != DSERR_ALLOCATED)
		{
			DevMsg( "dsound [init]: DirectSoundCreate8 failed w/e 0x%8x.\n", hr );
			return SIS_FAILURE;
		}

		return SIS_NOTAVAIL;
	}

	DWORD is_certified{DS_UNCERTIFIED};
	hr = pDS->VerifyCertification( &is_certified );
	if ( SUCCEEDED(hr) && is_certified == DS_CERTIFIED )
	{
		Msg( "dsound [init]: DirectSound8 card driver is certified.\n" );
	}

	// get snd_surround value from window settings
	DetectWindowsSpeakerSetup();

	m_bSurround = false;
	m_bSurroundCenter = false;
	m_bHeadphone = false;
	m_isInterleaved = false;
	
	unsigned short pri_channels = 2;

	switch ( snd_surround.GetInt() )
	{
	case 0:
		m_bHeadphone = true;	// stereo headphone
		pri_channels = 2;		// primary buffer mixes stereo input data
		break;
	default:
	case 2:
		pri_channels = 2;		// primary buffer mixes stereo input data
		break;					// no surround
	case 4:
		m_bSurround = true;		// quad surround
		pri_channels = 1;		// primary buffer mixes 3d mono input data
		break;
	case 5:
	case 7:
		m_bSurround = true;		// 5.1 / 7.1 surround
		m_bSurroundCenter = true;
		pri_channels = 1;		// primary buffer mixes 3d mono input data
		break;
	}

	m_deviceChannels   = pri_channels;		// secondary buffers should have same # channels as primary
	m_deviceSampleBits = 16;				// hardware bits per sample
	m_deviceDmaSpeed   = SOUND_DMA_SPEED;	// hardware playback rate
	
	WAVEFORMATEX primary_format = {};
	primary_format.wFormatTag		= WAVE_FORMAT_PCM;
    primary_format.nChannels		= pri_channels;
    primary_format.wBitsPerSample	= m_deviceSampleBits;
    primary_format.nSamplesPerSec	= m_deviceDmaSpeed;
    primary_format.nBlockAlign		= primary_format.nChannels * primary_format.wBitsPerSample / 8;
    primary_format.cbSize			= 0;
    primary_format.nAvgBytesPerSec	= primary_format.nSamplesPerSec * primary_format.nBlockAlign; 

	DSCAPS device_caps = {};
	device_caps.dwSize = sizeof(device_caps);
	if (FAILED(hr = pDS->GetCaps(&device_caps)))
	{
		Warning( "dsound [init]: Unable to query DirectSound8 device capabilities w/e 0x%8x.\n", hr );
		Shutdown();
		return SIS_FAILURE;
	}

	if (device_caps.dwFlags & DSCAPS_EMULDRIVER)
	{
		Warning( "dsound [init]: DirectSound8 driver is not installed.\n");
		Shutdown();
		return SIS_FAILURE;
	}

	if (FAILED(hr = pDS->SetCooperativeLevel(*pmainwindow, DSSCL_PRIORITY)))
	{
		Warning( "dsound [init]: DirectSound8 device apply cooperative level DSSCL_PRIORITY failed w/e 0x%8x.\n", hr );
		Shutdown();
		return SIS_FAILURE;
	}

	// get access to the primary buffer, if possible, so we can set the
	// sound hardware format
	DSBUFFERDESC buffer_desc = {};
	buffer_desc.dwSize = sizeof(DSBUFFERDESC);
	buffer_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
	if ( snd_legacy_surround.GetBool() || m_bSurround )
	{
		buffer_desc.dwFlags |= DSBCAPS_CTRL3D;
	}
	buffer_desc.dwBufferBytes = 0;
	buffer_desc.lpwfxFormat = nullptr;
	
	DSBCAPS buffer_caps = {};
	buffer_caps.dwSize = sizeof(buffer_caps);
	
	bool primary_format_set = false;
	if ( !CommandLine()->CheckParm("-snoforceformat"))
	{
		if (SUCCEEDED(hr = pDS->CreateSoundBuffer(&buffer_desc, &pDSPBuf, nullptr)))
		{
			WAVEFORMATEX format = primary_format;
			if (FAILED(pDSPBuf->SetFormat(&format)))
			{
				if (snd_firsttime)
					DevMsg ("dsound: Set primary sound buffer format: no\n");
			}
			else
			{
				if (snd_firsttime)
					DevMsg ("dsound: Set primary sound buffer format: yes\n");

				primary_format_set = true;
			}
		}
		else
		{
			Warning( "dsound [init]: DirectSound8 device unable to create sound buffer w/e 0x%8x.\n", hr );
			Shutdown();
			return SIS_FAILURE;
		}
	}

	if ( m_bSurround )
	{
		// try to init surround
		m_bSurround = false;
		if ( snd_legacy_surround.GetBool() )
		{
			if (snd_surround.GetInt() == 4) 
			{
				// attempt to init 4 channel surround
				m_bSurround = SNDDMA_InitSurround(pDS, &primary_format, &buffer_caps, 4);
			}
			else if (snd_surround.GetInt() == 5 || snd_surround.GetInt() == 7) 
			{
				// attempt to init 5 channel surround
				m_bSurroundCenter = SNDDMA_InitSurround(pDS, &primary_format, &buffer_caps, 5);
				m_bSurround = m_bSurroundCenter;
			}
		}
		if ( !m_bSurround )
		{
			pri_channels = 6;
			if ( snd_surround.GetInt() < 5 )
			{
				pri_channels = 4;
			}
	
			m_bSurround = SNDDMA_InitInterleaved( pDS, &primary_format, pri_channels );
		}
	}

	if ( !m_bSurround )
	{
		if ( !primary_format_set || !CommandLine()->CheckParm ("-primarysound") )
		{
			// create the secondary buffer we'll actually work with
			BitwiseClear( buffer_desc );
			buffer_desc.dwSize = sizeof(DSBUFFERDESC);
			buffer_desc.dwFlags = DSBCAPS_LOCSOFTWARE;		// NOTE: don't use CTRLFREQUENCY (slow)
			buffer_desc.dwBufferBytes = SECONDARY_BUFFER_SIZE;
			buffer_desc.lpwfxFormat = &primary_format;
			buffer_desc.dwFlags |= DSBCAPS_GLOBALFOCUS;

			se::win::com::com_ptr<IDirectSoundBuffer, &IID_IDirectSoundBuffer> buffer;
			if (FAILED(hr = pDS->CreateSoundBuffer(&buffer_desc, &buffer, nullptr)) || 
				FAILED(hr = buffer.QueryInterface(IID_IDirectSoundBuffer8, &pDSBuf)))
			{
				Warning( "dsound [init]: DirectSound8 device unable to create sound buffer 8 w/e 0x%8x.\n", hr );
				Shutdown();
				return SIS_FAILURE;
			}

			m_deviceChannels   = primary_format.nChannels;
			m_deviceSampleBits = primary_format.wBitsPerSample;
			m_deviceDmaSpeed   = primary_format.nSamplesPerSec;

			BitwiseClear( buffer_caps );
			buffer_caps.dwSize = sizeof(buffer_caps);

			if (DS_OK != pDSBuf->GetCaps( &buffer_caps ))
			{
				Warning( "dsound [init]: Unable to query DirectSound8 buffer capabilities w/e 0x%8x.\n", hr );
				Shutdown();
				return SIS_FAILURE;
			}

			if ( snd_firsttime )
				DevMsg ("dsound [init]: Using secondary sound buffer.\n");
		}
		else
		{
			if (FAILED(pDS->SetCooperativeLevel(*pmainwindow, DSSCL_WRITEPRIMARY)))
			{
				Warning( "dsound [init]: DirectSound8 device apply cooperative level DSSCL_WRITEPRIMARY failed w/e 0x%8x.\n", hr );
				Shutdown();
				return SIS_FAILURE;
			}

			BitwiseClear( buffer_caps );
			buffer_caps.dwSize = sizeof(buffer_caps);
			if (FAILED(pDSPBuf->GetCaps(&buffer_caps)))
			{
				Warning( "dsound [init]: Unable to query DirectSound8 buffer capabilities w/e 0x%8x.\n", hr );
				Shutdown();
				return SIS_FAILURE;
			}

			pDSBuf = pDSPBuf;
			DevMsg ("dsound [init]: Using primary sound buffer\n");
		}

		if ( snd_firsttime )
		{
			DevMsg("   %d channel(s) |  %d bits/sample |  %d Hz\n",
				DeviceChannels(), DeviceSampleBits(), DeviceDmaSpeed());
		}

		// initialize the buffer
		m_bufferSizeBytes = buffer_caps.dwBufferBytes;

		DWORD data_size;
		void *buffer_data = nullptr;
		unsigned retries_count = 0;
		while (FAILED(hr = pDSBuf->Lock(0, m_bufferSizeBytes, &buffer_data, &data_size, nullptr, nullptr, 0)))
		{
			if (hr != DSERR_BUFFERLOST)
			{
				Warning( "dsound [init]: Unable to lock DirectSound8 buffer w/e 0x%8x.\n", hr );
				Shutdown();
				return SIS_FAILURE;
			}

			if (++retries_count > 10000u)
			{
				Warning( "dsound [init]: Unable to restore DirectSound8 buffer w/e 0x%8x.\n", hr );
				Shutdown();
				return SIS_FAILURE;
			}
		}

		Q_memset( buffer_data, 0, data_size );
		hr = pDSBuf->Unlock(buffer_data, data_size, NULL, 0);
		if (FAILED(hr))
		{
			Warning( "dsound [init]: Unable to lock DirectSound8 buffer w/e 0x%8x.\n", hr );
			Shutdown();
			return SIS_FAILURE;
		}
		
		// Make sure mixer is active (this was moved after the zeroing to avoid
		// popping on startup -- at least when using the dx9.0b debug .dlls)
		hr = pDSBuf->Play(0, 0, DSBPLAY_LOOPING);
		if (FAILED(hr))
		{
			Warning( "dsound [init]: Unable to play DirectSound8 buffer w/e 0x%8x.\n", hr );
			Shutdown();
			return SIS_FAILURE;
		}

		// we don't want anyone to access the buffer directly w/o locking it first.
		buffer_data = NULL;

		hr = pDSBuf->Stop();
		if (FAILED(hr))
		{
			Warning( "dsound [init]: Unable to stop DirectSound8 buffer w/e 0x%8x.\n", hr );
			Shutdown();
			return SIS_FAILURE;
		}
		
		DWORD write_buffer_pos;
		hr = pDSBuf->GetCurrentPosition(&m_outputBufferStartOffset, &write_buffer_pos);
		if (FAILED(hr))
		{
			Warning( "dsound [init]: Unable to get current position for DirectSound8 buffer w/e 0x%8x.\n", hr );
			Shutdown();
			return SIS_FAILURE;
		}

		hr = pDSBuf->Play(0, 0, DSBPLAY_LOOPING);
		if (FAILED(hr))
		{
			Warning( "dsound [init]: Unable to play DSBPLAY_LOOPING DirectSound8 buffer w/e 0x%8x.\n", hr );
			Shutdown();
			return SIS_FAILURE;
		}
	}

	// number of mono samples output buffer may hold
	m_deviceSampleCount = m_bufferSizeBytes/ DeviceSampleBytes();

	return SIS_SUCCESS;

}

static DWORD GetSpeakerConfigForSurroundMode( int surroundMode, const char **pConfigDesc )
{
	DWORD newSpeakerConfig = DSSPEAKER_STEREO;
	const char *speakerConfigDesc = "";

	switch ( surroundMode )
	{
	case 0:
		newSpeakerConfig = DSSPEAKER_HEADPHONE;
		speakerConfigDesc = "Headphone";
		break;

	case 2:
	default:
		newSpeakerConfig = DSSPEAKER_STEREO;
		speakerConfigDesc = "Stereo speaker";
		break;

	case 4:
		newSpeakerConfig = DSSPEAKER_QUAD;
		speakerConfigDesc = "Quad speaker";
		break;

	case 5:
		newSpeakerConfig = DSSPEAKER_5POINT1_BACK;
		speakerConfigDesc = "5.1 speaker";
		break;

	case 7:
		newSpeakerConfig = DSSPEAKER_7POINT1_WIDE;
		speakerConfigDesc = "7.1 speaker";
		break;
	}
	if ( pConfigDesc )
	{
		*pConfigDesc = speakerConfigDesc;
	}
	return newSpeakerConfig;
}

// Read the speaker config from windows
static DWORD GetWindowsSpeakerConfig()
{
	DWORD speaker_config = windows_speaker_config.GetInt();
	if ( windows_speaker_config.GetInt() < 0 )
	{
		speaker_config = DSSPEAKER_STEREO;
		if (DS_OK == pDS->GetSpeakerConfig( &speaker_config ))
		{
			// split out settings
			speaker_config = DSSPEAKER_CONFIG(speaker_config);
			if ( speaker_config == DSSPEAKER_7POINT1_SURROUND )
				speaker_config = DSSPEAKER_7POINT1_WIDE;
			if ( speaker_config == DSSPEAKER_5POINT1_SURROUND)
				speaker_config = DSSPEAKER_5POINT1_BACK;
		}
		windows_speaker_config.SetValue((int)speaker_config);
	}

	return speaker_config;
}

// Writes snd_surround convar given a directsound speaker config
static void SetSurroundModeFromSpeakerConfig( DWORD speakerConfig )
{
	// set the cvar to be the windows setting
	switch (speakerConfig)
	{
	case DSSPEAKER_HEADPHONE:
		snd_surround.SetValue(0);
		break;

	case DSSPEAKER_MONO:
	case DSSPEAKER_STEREO:
	default:
		snd_surround.SetValue( 2 );
		break;

	case DSSPEAKER_QUAD:
		snd_surround.SetValue(4);
		break;

	case DSSPEAKER_5POINT1:
		snd_surround.SetValue(5);
		break;

	case DSSPEAKER_7POINT1:
		snd_surround.SetValue(7);
		break;
	}
}
/*
 Sets the snd_surround_speakers cvar based on the windows setting
*/

void CAudioDirectSound::DetectWindowsSpeakerSetup()
{
	// detect speaker settings from windows
	DWORD speaker_config = GetWindowsSpeakerConfig();
	SetSurroundModeFromSpeakerConfig(speaker_config);

	// DEBUG
	if (speaker_config == DSSPEAKER_MONO)
		DevMsg( "dsound: Mono configuration detected.\n");

	if (speaker_config == DSSPEAKER_HEADPHONE)
		DevMsg( "dsound: Headphone configuration detected.\n");

	if (speaker_config == DSSPEAKER_STEREO)
		DevMsg( "dsound: Stereo speaker configuration detected.\n");

	if (speaker_config == DSSPEAKER_QUAD)
		DevMsg( "dsound: Quad speaker configuration detected.\n");

	if (speaker_config == DSSPEAKER_SURROUND)
		DevMsg( "dsound: Surround speaker configuration detected.\n");

	if (speaker_config == DSSPEAKER_5POINT1)
		DevMsg( "dsound: 5.1 speaker configuration detected.\n");

	if (speaker_config == DSSPEAKER_7POINT1)
		DevMsg( "dsound: 7.1 speaker configuration detected.\n");
}

/*
 Updates windows settings based on snd_surround_speakers cvar changing
 This should only happen if the user has changed it via the console or the UI
 Changes won't take effect until the engine has restarted
*/
void OnSndSurroundCvarChanged( IConVar *pVar, const char *pOldString, float flOldValue )
{
	// if the old value is -1, we're setting this from the detect routine for the first time
	// no need to reset the device
	if (!pDS || flOldValue == -1 )
		return;

	// get the user's previous speaker config
	DWORD speaker_config = GetWindowsSpeakerConfig();

	// get the new config
	DWORD newSpeakerConfig = 0;
	const char *speakerConfigDesc = "";

	ConVarRef var( pVar );
	newSpeakerConfig = GetSpeakerConfigForSurroundMode( var.GetInt(), &speakerConfigDesc );
	// make sure the config has changed
	if (newSpeakerConfig == speaker_config)
		return;

	// set new configuration
	windows_speaker_config.SetValue( (int)newSpeakerConfig );

	Msg("dsound: Audio configuration has been changed to '%s'.\n", speakerConfigDesc);

	// restart sound system so it takes effect
	g_pSoundServices->RestartSoundSystem();
}

void OnSndSurroundLegacyChanged( IConVar *pVar, const char *pOldString, float flOldValue )
{
	if ( pDS && CAudioDirectSound::m_pSingleton )
	{
		ConVarRef var( pVar );
		// should either be interleaved or have legacy surround set, not both
		if ( CAudioDirectSound::m_pSingleton->IsInterleaved() == var.GetBool() )
		{
			Msg( "dsound: Legacy Surround %s.\n", var.GetBool() ? "enabled" : "disabled" );
			// restart sound system so it takes effect
			g_pSoundServices->RestartSoundSystem();
		}
	}
}

void OnSndVarChanged( IConVar *pVar, const char *pOldString, float flOldValue )
{
	ConVarRef var(pVar);
	// restart sound system so the change takes effect
	if ( var.GetInt() != int(flOldValue) )
	{
		g_pSoundServices->RestartSoundSystem();
	}
}

/*
 Release all Surround buffer pointers
*/
void ReleaseSurround(void)
{
	if ( pDSBuf3DFL != NULL )
	{
		pDSBuf3DFL->Release();
		pDSBuf3DFL = NULL;
	}

	if ( pDSBuf3DFR != NULL)
	{
		pDSBuf3DFR->Release();
		pDSBuf3DFR = NULL;
	}

	if ( pDSBuf3DRL != NULL )
	{
		pDSBuf3DRL->Release();
		pDSBuf3DRL = NULL;
	}

	if ( pDSBuf3DRR != NULL )
	{	
		pDSBuf3DRR->Release();
		pDSBuf3DRR = NULL;
	}

	if ( pDSBufFL != NULL )
	{
		pDSBufFL->Release();
		pDSBufFL = NULL;
	}

	if ( pDSBufFR != NULL )
	{
		pDSBufFR->Release();
		pDSBufFR = NULL;
	}

	if ( pDSBufRL != NULL )
	{
		pDSBufRL->Release();
		pDSBufRL = NULL;
	}

	if ( pDSBufRR != NULL )
	{
		pDSBufRR->Release();
		pDSBufRR = NULL;
	}

	if ( pDSBufFC != NULL )
	{
		pDSBufFC->Release();
		pDSBufFC = NULL;
	}
}

void DEBUG_DS_FillSquare( void *lpData, DWORD dwSize )
{
	short *lpshort = (short *)lpData;
	DWORD j = min((DWORD)10000, dwSize/2);
 
	for (DWORD i = 0; i < j; i++)
		lpshort[i] = 8000;
}

void DEBUG_DS_FillSquare2( void *lpData, DWORD dwSize )
{
	short *lpshort = (short *)lpData;
	DWORD j = min((DWORD)1000, dwSize/2);
 
	for (DWORD i = 0; i < j; i++)
		lpshort[i] = 16000;
}

// helper to set default buffer params
void DS3D_SetBufferParams( LPDIRECTSOUND3DBUFFER pDSBuf3D, D3DVECTOR *pbpos, D3DVECTOR *pbdir )
{
	DS3DBUFFER bparm;
	D3DVECTOR bvel;
	D3DVECTOR bpos, bdir;
	HRESULT hr;
	
	bvel.x = 0.0f; bvel.y = 0.0f; bvel.z = 0.0f;
	bpos = *pbpos;
	bdir = *pbdir;

	bparm.dwSize = sizeof(DS3DBUFFER);

	hr = pDSBuf3D->GetAllParameters( &bparm );

	bparm.vPosition = bpos;
	bparm.vVelocity = bvel;
	bparm.dwInsideConeAngle = 5;						// narrow cones for each speaker
	bparm.dwOutsideConeAngle = 10;	
	bparm.vConeOrientation = bdir;
	bparm.lConeOutsideVolume = DSBVOLUME_MIN;
	bparm.flMinDistance = 100.0;		// no rolloff (until > 2.0 meter distance)
	bparm.flMaxDistance = DS3D_DEFAULTMAXDISTANCE;
	bparm.dwMode = DS3DMODE_NORMAL;

	hr = pDSBuf3D->SetAllParameters( &bparm, DS3D_DEFERRED );
}

// Initialization for Surround sound support (4 channel or 5 channel). 
// Creates 4 or 5 mono 3D buffers to be used as Front Left, (Front Center), Front Right, Rear Left, Rear Right
bool CAudioDirectSound::SNDDMA_InitSurround(LPDIRECTSOUND8 lpDS, WAVEFORMATEX* lpFormat, DSBCAPS* lpdsbc, int cchan)
{
	DSBUFFERDESC	dsbuf;
	WAVEFORMATEX wvex;
	DWORD dwSize, dwWrite;
	int reps;
	HRESULT hresult;
	void			*lpData = NULL;

	if ( lpDS == NULL ) return FALSE;
 
	// Force format to mono channel

	memcpy(&wvex, lpFormat, sizeof(WAVEFORMATEX));
	wvex.nChannels = 1;
	wvex.nBlockAlign = wvex.nChannels * wvex.wBitsPerSample / 8;
	wvex.nAvgBytesPerSec = wvex.nSamplesPerSec	* wvex.nBlockAlign; 

	BitwiseClear(dsbuf);
	dsbuf.dwSize = sizeof(DSBUFFERDESC);
														 // NOTE: LOCHARDWARE causes SB AWE64 to crash in it's DSOUND driver
	dsbuf.dwFlags = DSBCAPS_CTRL3D;						 // don't use CTRLFREQUENCY (slow)
	dsbuf.dwFlags |= DSBCAPS_GLOBALFOCUS;

	// reserve space for each buffer

	dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE_SURROUND;	

	dsbuf.lpwfxFormat = &wvex;

	se::win::com::com_ptr<IDirectSoundBuffer, &IID_IDirectSoundBuffer> buffer;
	
	// create 4 mono buffers FL, FR, RL, RR
	if (DS_OK != lpDS->CreateSoundBuffer(&dsbuf, &buffer, NULL) ||
        FAILED(buffer.QueryInterface(IID_IDirectSoundBuffer8, &pDSBufFL)))
	{
		Warning( "dsound [surround]: CreateSoundBuffer for 3d front left failed");
		ReleaseSurround();
		return FALSE;
	}

	if (DS_OK != lpDS->CreateSoundBuffer(&dsbuf, &buffer, NULL) ||
        FAILED(buffer.QueryInterface(IID_IDirectSoundBuffer8, &pDSBufFR)))
	{
		Warning( "dsound [surround]: CreateSoundBuffer for 3d front right failed");
		ReleaseSurround();
		return FALSE;
	}

	if (DS_OK != lpDS->CreateSoundBuffer(&dsbuf, &buffer, NULL) ||
        FAILED(buffer.QueryInterface(IID_IDirectSoundBuffer8, &pDSBufRL)))
	{
		Warning( "dsound [surround]: CreateSoundBuffer for 3d rear left failed");
		ReleaseSurround();
		return FALSE;
	}

	if (DS_OK != lpDS->CreateSoundBuffer(&dsbuf, &buffer, NULL) ||
        FAILED(buffer.QueryInterface(IID_IDirectSoundBuffer8, &pDSBufRR)))
	{
		Warning( "dsound [surround]: CreateSoundBuffer for 3d rear right failed");
		ReleaseSurround();
		return FALSE;
	}

	// create center channel

	if (cchan == 5)
	{
		if (DS_OK != lpDS->CreateSoundBuffer(&dsbuf, &buffer, NULL) ||
			FAILED(buffer.QueryInterface(IID_IDirectSoundBuffer8, &pDSBufFC)))
		{
			Warning( "dsound [surround]: CreateSoundBuffer for 3d front center failed");
			ReleaseSurround();
			return FALSE;
		}
	}

	// Try to get 4 or 5 3D buffers from the mono DS buffers

	if (DS_OK != pDSBufFL->QueryInterface(IID_IDirectSound3DBufferDef, (void**)&pDSBuf3DFL))
	{
		Warning( "dsound [surround]: Query 3DBuffer for 3d front left failed");
		ReleaseSurround();
		return FALSE;
	}

	if (DS_OK != pDSBufFR->QueryInterface(IID_IDirectSound3DBufferDef, (void**)&pDSBuf3DFR))
	{
		Warning( "dsound [surround]: Query 3DBuffer for 3d front right failed");
		ReleaseSurround();
		return FALSE;
	}

	if (DS_OK != pDSBufRL->QueryInterface(IID_IDirectSound3DBufferDef, (void**)&pDSBuf3DRL))
	{
		Warning( "dsound [surround]: Query 3DBuffer for 3d rear left failed");
		ReleaseSurround();
		return FALSE;
	}

	if (DS_OK != pDSBufRR->QueryInterface(IID_IDirectSound3DBufferDef, (void**)&pDSBuf3DRR))
	{
		Warning( "dsound [surround]: Query 3DBuffer for 3d rear right failed");
		ReleaseSurround();
		return FALSE;
	}

	if (cchan == 5)
	{
		if (DS_OK != pDSBufFC->QueryInterface(IID_IDirectSound3DBufferDef, (void**)&pDSBuf3DFC)) 
		{
			Warning( "dsound [surround]: Query 3DBuffer for 3d front center failed");
			ReleaseSurround();
			return FALSE;
		}
	}

	// set listener position & orientation.
	// DS uses left handed coord system: +x is right, +y is up, +z is forward

	HRESULT hr;

	IDirectSound3DListener *plistener = NULL;
	
	hr = pDSPBuf->QueryInterface(IID_IDirectSound3DListener, (void**)&plistener);
	if (plistener)
	{
		DS3DLISTENER lparm;
		lparm.dwSize = sizeof(DS3DLISTENER);

		hr = plistener->GetAllParameters( &lparm );	

		hr = plistener->SetOrientation( 0.0f,0.0f,1.0f, 0.0f,1.0f,0.0f, DS3D_IMMEDIATE); // frontx,y,z topx,y,z
		hr = plistener->SetPosition(0.0f, 0.0f, 0.0f, DS3D_IMMEDIATE);	
	}
	else
	{
		Warning( "dsound [surround]: Failed to get 3D listener interface.");
		ReleaseSurround();
		return FALSE;
	}

	// set 3d buffer position and orientation params

	D3DVECTOR bpos, bdir;

	bpos.x = -1.0; bpos.y = 0.0; bpos.z = 1.0;				// FL
	bdir.x =  1.0; bdir.y = 0.0; bdir.z = -1.0;
	
	DS3D_SetBufferParams( pDSBuf3DFL, &bpos, &bdir );
	
	bpos.x = 1.0; bpos.y = 0.0; bpos.z = 1.0;				// FR
	bdir.x = -1.0; bdir.y = 0.0; bdir.z = -1.0;
	
	DS3D_SetBufferParams( pDSBuf3DFR, &bpos, &bdir );

	bpos.x = -1.0; bpos.y = 0.0; bpos.z = -1.0;				// RL
	bdir.x = 1.0; bdir.y = 0.0; bdir.z = 1.0;
	
	DS3D_SetBufferParams( pDSBuf3DRL, &bpos, &bdir );

	bpos.x = 1.0; bpos.y = 0.0; bpos.z = -1.0;				// RR
	bdir.x = -1.0; bdir.y = 0.0; bdir.z = 1.0;
	
	DS3D_SetBufferParams( pDSBuf3DRR, &bpos, &bdir );

	if (cchan == 5)
	{
		bpos.x = 0.0; bpos.y = 0.0; bpos.z = 1.0;			// FC
		bdir.x = 0.0; bdir.y = 0.0; bdir.z = -1.0;
	
		DS3D_SetBufferParams( pDSBuf3DFC, &bpos, &bdir );
	}

	// commit all buffer param settings

	hr = plistener->CommitDeferredSettings();

	m_deviceChannels = 1;				// 1 mono 3d output buffer
	m_deviceSampleBits = lpFormat->wBitsPerSample;
	m_deviceDmaSpeed = lpFormat->nSamplesPerSec;

	BitwiseClear(*lpdsbc);
	lpdsbc->dwSize = sizeof(DSBCAPS);

	if (DS_OK != pDSBufFL->GetCaps (lpdsbc))
	{
		Warning( "dsound [surround]: GetCaps failed for 3d sound buffer\n");
		ReleaseSurround();
		return FALSE;
	}

	pDSBufFL->Play(0, 0, DSBPLAY_LOOPING);
	pDSBufFR->Play(0, 0, DSBPLAY_LOOPING);
	pDSBufRL->Play(0, 0, DSBPLAY_LOOPING);
	pDSBufRR->Play(0, 0, DSBPLAY_LOOPING);

	if (cchan == 5)
		pDSBufFC->Play(0, 0, DSBPLAY_LOOPING);

	if (snd_firsttime)
		DevMsg("   %d channel(s) |  %d bits/sample |  %d Hz\n",
					cchan, DeviceSampleBits(), DeviceDmaSpeed());

	m_bufferSizeBytes = lpdsbc->dwBufferBytes;

	// Test everything just like in the normal initialization.
	if (cchan == 5)
	{
		reps = 0;
		while ((hresult = pDSBufFC->Lock(0, lpdsbc->dwBufferBytes, (void**)&lpData, &dwSize, NULL, NULL, 0)) != DS_OK)
		{
			if (hresult != DSERR_BUFFERLOST)
			{
				Warning( "dsound [surround]: Lock Sound Buffer Failed for FC\n");
				ReleaseSurround();
				return FALSE;
			}

			if (++reps > 10000)
			{
				Warning( "dsound [surround]: Couldn't restore buffer for FC\n");
				ReleaseSurround();
				return FALSE;
			}
		}

		memset(lpData, 0, dwSize);

		pDSBufFC->Unlock(lpData, dwSize, NULL, 0);
	}

	reps = 0;
	while ((hresult = pDSBufFL->Lock(0, lpdsbc->dwBufferBytes, (void**)&lpData, &dwSize, NULL, NULL, 0)) != DS_OK)
	{
		if (hresult != DSERR_BUFFERLOST)
		{
			Warning( "dsound [surround]: Lock Sound Buffer Failed for 3d FL\n");
			ReleaseSurround();
			return FALSE;
		}

		if (++reps > 10000)
		{
			Warning( "dsound [surround]: Couldn't restore buffer for 3d FL\n");
			ReleaseSurround();
			return FALSE;
		}
	}
	memset(lpData, 0, dwSize);
//	DEBUG_DS_FillSquare( lpData, dwSize );
	pDSBufFL->Unlock(lpData, dwSize, NULL, 0);

	reps = 0;
	while ((hresult = pDSBufFR->Lock(0, lpdsbc->dwBufferBytes, (void**)&lpData, &dwSize, NULL, NULL, 0)) != DS_OK)
	{
		if (hresult != DSERR_BUFFERLOST)
		{
			Warning( "dsound [surround]: Lock Sound Buffer Failed for 3d FR\n");
			ReleaseSurround();
			return FALSE;
		}

		if (++reps > 10000)
		{
			Warning( "dsound [surround]: Couldn't restore buffer for FR\n");
			ReleaseSurround();
			return FALSE;
		}
	}
	memset(lpData, 0, dwSize);
//	DEBUG_DS_FillSquare( lpData, dwSize );
	pDSBufFR->Unlock(lpData, dwSize, NULL, 0);

	reps = 0;
	while ((hresult = pDSBufRL->Lock(0, lpdsbc->dwBufferBytes, (void**)&lpData, &dwSize, NULL, NULL, 0)) != DS_OK)
	{
		if (hresult != DSERR_BUFFERLOST)
		{
			Warning( "dsound [surround]: Lock Sound Buffer Failed for RL\n");
			ReleaseSurround();
			return FALSE;
		}

		if (++reps > 10000)
		{
			Warning( "dsound [surround]: Couldn't restore buffer for RL\n");
			ReleaseSurround();
			return FALSE;
		}
	}
	memset(lpData, 0, dwSize);
//	DEBUG_DS_FillSquare( lpData, dwSize );
	pDSBufRL->Unlock(lpData, dwSize, NULL, 0);

	reps = 0;
	while ((hresult = pDSBufRR->Lock(0, lpdsbc->dwBufferBytes, (void**)&lpData, &dwSize, NULL, NULL, 0)) != DS_OK)
	{
		if (hresult != DSERR_BUFFERLOST)
		{
			Warning( "dsound [surround]: Lock Sound Buffer Failed for RR\n");
			ReleaseSurround();
			return FALSE;
		}

		if (++reps > 10000)
		{
			Warning( "dsound [surround]: couldn't restore buffer for RR\n");
			ReleaseSurround();
			return FALSE;
		}
	}
	memset(lpData, 0, dwSize);
//	DEBUG_DS_FillSquare( lpData, dwSize );
	pDSBufRR->Unlock(lpData, dwSize, NULL, 0);

	lpData = NULL; // this is invalid now

	// OK Stop and get our positions and were good to go.
	pDSBufFL->Stop();
	pDSBufFR->Stop();
	pDSBufRL->Stop();
	pDSBufRR->Stop();
	if (cchan == 5) 
		pDSBufFC->Stop();

	// get hardware playback position, store it, syncronize all buffers to FL

	pDSBufFL->GetCurrentPosition(&m_outputBufferStartOffset, &dwWrite);
	pDSBufFR->SetCurrentPosition(m_outputBufferStartOffset);
	pDSBufRL->SetCurrentPosition(m_outputBufferStartOffset);
	pDSBufRR->SetCurrentPosition(m_outputBufferStartOffset);
	if (cchan == 5) 
		pDSBufFC->SetCurrentPosition(m_outputBufferStartOffset);

	pDSBufFL->Play(0, 0, DSBPLAY_LOOPING);
	pDSBufFR->Play(0, 0, DSBPLAY_LOOPING);
	pDSBufRL->Play(0, 0, DSBPLAY_LOOPING);
	pDSBufRR->Play(0, 0, DSBPLAY_LOOPING);
	if (cchan == 5) 
		pDSBufFC->Play(0, 0, DSBPLAY_LOOPING);

	if (snd_firsttime)
		Msg( "dsound [surround]: 3d surround sound initialization successful\n");

	return TRUE;
}

void CAudioDirectSound::UpdateListener( const Vector& position, const Vector& forward, const Vector& right, const Vector& up )
{
}

void CAudioDirectSound::ChannelReset( int entnum, int channelIndex, float distanceMod )
{
}

const char *CAudioDirectSound::DeviceName( void )
{
	switch (snd_surround.GetInt())
	{
	case 0:
		return "Direct Sound Headphones";

	case 2:
		return "Direct Sound Speakers";

	case 4:
		return "Direct Sound 4 Channel Surround";

	case 5:
		return "Direct Sound 5.1 Channel Surround";

	case 7:
		return "Direct Sound 7.1 Channel Surround";
	}

	return "Direct Sound";
}

// use the partial buffer locking code in stereo as well - not available when recording a movie
ConVar snd_lockpartial("snd_lockpartial","1");

// Transfer up to a full paintbuffer (PAINTBUFFER_SIZE) of stereo samples
// out to the directsound secondary buffer(s).
// For 4 or 5 ch surround, there are 4 or 5 mono 16 bit secondary DS streaming buffers.
// For stereo speakers, there is one stereo 16 bit secondary DS streaming buffer.

void CAudioDirectSound::TransferSamples( int end )
{
	int		lpaintedtime = g_paintedtime;
	int		endtime = end;
	
	// When Surround is enabled, divert to 4 or 5 chan xfer scheme.
	if ( m_bSurround )
	{		
		if ( m_isInterleaved )
		{
			S_TransferSurround16Interleaved( PAINTBUFFER, REARPAINTBUFFER, CENTERPAINTBUFFER, lpaintedtime, endtime);
		}
		else
		{
			int	cchan = ( m_bSurroundCenter ? 5 : 4);

			S_TransferSurround16( PAINTBUFFER, REARPAINTBUFFER, CENTERPAINTBUFFER, lpaintedtime, endtime, cchan);
		}
		return;
	}
	else if ( snd_lockpartial.GetBool() && DeviceChannels() == 2 && DeviceSampleBits() == 16 && !SND_IsRecording() )
	{
		S_TransferSurround16Interleaved( PAINTBUFFER, NULL, NULL, lpaintedtime, endtime );
	}
	else
	{
		DWORD *pBuffer = NULL;
		DWORD dwSize = 0;
		if ( !LockDSBuffer( pDSBuf, &pBuffer, &dwSize, "DS_STEREO" ) )
		{
			S_Shutdown();
			S_Startup();
			return;
		}
		if ( pBuffer )
		{
			if ( DeviceChannels() == 2 && DeviceSampleBits() == 16 )
			{
				S_TransferStereo16( pBuffer, PAINTBUFFER, lpaintedtime, endtime );
			}
			else
			{
				// UNDONE: obsolete - no 8 bit mono output supported
				S_TransferPaintBuffer( pBuffer, PAINTBUFFER, lpaintedtime, endtime );
			}
			pDSBuf->Unlock( pBuffer, dwSize, NULL, 0 );
		}
	}
}

bool CAudioDirectSound::IsUsingBufferPerSpeaker() 
{ 
	return m_bSurround && !m_isInterleaved; 
}

bool CAudioDirectSound::LockDSBuffer( LPDIRECTSOUNDBUFFER pBuffer, DWORD **pdwWriteBuffer, DWORD *pdwSizeBuffer, const char *pBufferName, int lockFlags )
{
	if ( !pBuffer )
		return false;

	HRESULT hr;
	int reps = 0;
	while ((hr = pBuffer->Lock(0, m_bufferSizeBytes, (void**)pdwWriteBuffer, pdwSizeBuffer, 
		NULL, NULL, lockFlags)) != DS_OK)
	{
		if (hr != DSERR_BUFFERLOST)
		{
			Msg ("dsound: Lock Sound Buffer Failed %s\n", pBufferName);
			return false;
		}

		if (++reps > 10000)
		{
			Msg ("dsound: Couldn't restore buffer %s\n", pBufferName);
			return false;
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Given front, rear and center stereo paintbuffers, split samples into 4 or 5 mono directsound buffers (FL, FC, FR, RL, RR)
void CAudioDirectSound::S_TransferSurround16( portable_samplepair_t *pfront, portable_samplepair_t *prear, portable_samplepair_t *pcenter, int lpaintedtime, int endtime, int cchan)
{
	int		lpos;
	DWORD *pdwWriteFL=NULL, *pdwWriteFR=NULL, *pdwWriteRL=NULL, *pdwWriteRR=NULL, *pdwWriteFC=NULL;
	DWORD dwSizeFL=0, dwSizeFR=0, dwSizeRL=0, dwSizeRR=0, dwSizeFC=0;

	int *snd_p, *snd_rp, *snd_cp;
	short	*snd_out_fleft, *snd_out_fright, *snd_out_rleft, *snd_out_rright, *snd_out_fcenter;

	pdwWriteFC = NULL;			// compiler warning
	dwSizeFC = 0;
	snd_out_fcenter = NULL;

	int volumeFactor = S_GetMasterVolume() * 256;

	// lock all 4 or 5 mono directsound buffers FL, FR, RL, RR, FC
	if ( !LockDSBuffer( pDSBufFL, &pdwWriteFL, &dwSizeFL, "FL" ) ||
		!LockDSBuffer( pDSBufFR, &pdwWriteFR, &dwSizeFR, "FR" ) ||
		!LockDSBuffer( pDSBufRL, &pdwWriteRL, &dwSizeRL, "RL" ) ||
		!LockDSBuffer( pDSBufRR, &pdwWriteRR, &dwSizeRR, "RR" ) )
	{
		S_Shutdown();
		S_Startup();
		return;
	}

	if (cchan == 5 && !LockDSBuffer( pDSBufFC, &pdwWriteFC, &dwSizeFC, "FC" ))
	{
		S_Shutdown ();
		S_Startup ();
		return;
	}

	// take stereo front and rear paintbuffers, and center paintbuffer if provided,
	// and copy samples into the 4 or 5 mono directsound buffers

	snd_rp = (int *)prear;
	snd_cp = (int *)pcenter;
	snd_p = (int *)pfront;
	
	int linearCount;							 // space in output buffer for linearCount mono samples
	int sampleMonoCount = DeviceSampleCount();	 // number of mono samples per output buffer (was;(DeviceSampleCount()>>1))
	int sampleMask = sampleMonoCount - 1;
	
	// paintedtime - number of full samples that have played since start
	// endtime - number of full samples to play to - endtime is g_soundtime + mixahead samples

	while (lpaintedtime < endtime)
	{
		lpos = lpaintedtime & sampleMask;		// lpos is next output position in output buffer

		linearCount = sampleMonoCount - lpos;

		// limit output count to requested number of samples

		if (linearCount > endtime - lpaintedtime)
			linearCount = endtime - lpaintedtime;

		snd_out_fleft = (short *)pdwWriteFL + lpos;
		snd_out_fright = (short *)pdwWriteFR + lpos;
		snd_out_rleft = (short *)pdwWriteRL + lpos;
		snd_out_rright = (short *)pdwWriteRR + lpos;

		if (cchan == 5)
			snd_out_fcenter = (short *)pdwWriteFC + lpos;

		// for 16 bit sample in the front and rear stereo paintbuffers, copy
		// into the 4 or 5 FR, FL, RL, RR, FC directsound paintbuffers

		for (int i=0, j= 0 ; i<linearCount ; i++, j+=2)
		{
			snd_out_fleft[i]  = AdjustSampleVolume(snd_p[j], volumeFactor);
			snd_out_fright[i] = AdjustSampleVolume(snd_p[j + 1], volumeFactor);
			snd_out_rleft[i]  = AdjustSampleVolume(snd_rp[j], volumeFactor);
			snd_out_rright[i] = AdjustSampleVolume(snd_rp[j + 1], volumeFactor);
		}

		// copy front center buffer (mono) data to center chan directsound paintbuffer

		if (cchan == 5)
		{
			for (int i=0, j=0 ; i<linearCount ; i++, j+=2)
			{
				snd_out_fcenter[i] = AdjustSampleVolume(snd_cp[j], volumeFactor);
			}
		}

		snd_p += linearCount << 1;
		snd_rp += linearCount << 1;
		snd_cp += linearCount << 1;

		lpaintedtime += linearCount;
	}

	pDSBufFL->Unlock(pdwWriteFL, dwSizeFL, NULL, 0);
	pDSBufFR->Unlock(pdwWriteFR, dwSizeFR, NULL, 0);
	pDSBufRL->Unlock(pdwWriteRL, dwSizeRL, NULL, 0);
	pDSBufRR->Unlock(pdwWriteRR, dwSizeRR, NULL, 0);

	if (cchan == 5) 
		pDSBufFC->Unlock(pdwWriteFC, dwSizeFC, NULL, 0);
}

struct surround_transfer_t
{
	int paintedtime;
	int	linearCount;
	int sampleMask;
	int channelCount;
	int	*snd_p;
	int	*snd_rp;
	int *snd_cp;
	short *pOutput;
};

static void TransferSamplesToSurroundBuffer( int outputCount, surround_transfer_t &transfer )
{
	int volumeFactor = S_GetMasterVolume() * 256;

	if ( transfer.channelCount == 2 )
	{
		for (int i=0, j=0; i<outputCount ; i++, j+=2)
		{
			transfer.pOutput[0] = AdjustSampleVolume(transfer.snd_p[j], volumeFactor);		// FL
			transfer.pOutput[1] = AdjustSampleVolume(transfer.snd_p[j + 1], volumeFactor);	// FR

			transfer.pOutput += 2;
		}
	}
	// no center channel, 4 channel surround
	else if ( transfer.channelCount == 4 )
	{
		Assert(transfer.snd_rp);

		for (int i=0, j=0; i<outputCount ; i++, j+=2)
		{
			transfer.pOutput[0] = AdjustSampleVolume(transfer.snd_p[j], volumeFactor);		// FL
			transfer.pOutput[1] = AdjustSampleVolume(transfer.snd_p[j + 1], volumeFactor);	// FR
			transfer.pOutput[2] = AdjustSampleVolume(transfer.snd_rp[j], volumeFactor);		// RL
			transfer.pOutput[3] = AdjustSampleVolume(transfer.snd_rp[j + 1], volumeFactor);	// RR

			transfer.pOutput += 4;
		}
	}
	else
	{
		Assert(transfer.snd_cp);
		Assert(transfer.snd_rp);

		// 6 channel / 5.1
		for (int i=0, j=0 ; i<outputCount ; i++, j+=2)
		{
			transfer.pOutput[0] = AdjustSampleVolume(transfer.snd_p[j], volumeFactor);		// FL
			transfer.pOutput[1] = AdjustSampleVolume(transfer.snd_p[j + 1], volumeFactor);	// FR

			transfer.pOutput[2] = AdjustSampleVolume(transfer.snd_cp[j], volumeFactor);		// Center
			transfer.pOutput[3] = 0;

			transfer.pOutput[4] = AdjustSampleVolume(transfer.snd_rp[j], volumeFactor);		// RL
			transfer.pOutput[5] = AdjustSampleVolume(transfer.snd_rp[j + 1], volumeFactor);	// RR
			
			transfer.pOutput += 6;
		}
	}

	transfer.snd_p += outputCount << 1;
	if ( transfer.snd_rp )
	{
		transfer.snd_rp += outputCount << 1;
	}
	if ( transfer.snd_cp )
	{
		transfer.snd_cp += outputCount << 1;
	}

	transfer.paintedtime += outputCount;
	transfer.linearCount -= outputCount;
}

void CAudioDirectSound::S_TransferSurround16Interleaved_FullLock( const portable_samplepair_t *pfront, const portable_samplepair_t *prear, const portable_samplepair_t *pcenter, int lpaintedtime, int endtime )
{
	int volumeFactor = S_GetMasterVolume() * 256;
	int channelCount = m_bSurroundCenter ? 5 : 4;
	if ( DeviceChannels() == 2 )
	{
		channelCount = 2;
	}
	
	DWORD *write_buffer = nullptr;
	DWORD buffer_size = 0;
	// lock single interleaved buffer
	if ( !LockDSBuffer( pDSBuf, &write_buffer, &buffer_size, "DS_INTERLEAVED" ) )
	{
		S_Shutdown ();
		S_Startup ();
		return;
	}

	// take stereo front and rear paintbuffers, and center paintbuffer if provided,
	// and copy samples into the 4 or 5 mono directsound buffers

	int *snd_rp = (int *)prear;
	int *snd_cp = (int *)pcenter;
	int *snd_p = (int *)pfront;

	int linearCount;							 // space in output buffer for linearCount mono samples
	int sampleMonoCount = m_bufferSizeBytes/(DeviceSampleBytes()*DeviceChannels());	 // number of mono samples per output buffer (was;(DeviceSampleCount()>>1))
	int sampleMask = sampleMonoCount - 1;

	// paintedtime - number of full samples that have played since start
	// endtime - number of full samples to play to - endtime is g_soundtime + mixahead samples
	int		lpos;
	
	short *pOutput = (short *)write_buffer;
	while (lpaintedtime < endtime)
	{
		lpos = lpaintedtime & sampleMask;		// lpos is next output position in output buffer

		linearCount = sampleMonoCount - lpos;

		// limit output count to requested number of samples

		if (linearCount > endtime - lpaintedtime)
			linearCount = endtime - lpaintedtime;

		if ( channelCount == 4 )
		{
			int baseOffset = lpos * channelCount;
			for (int i=0, j= 0 ; i<linearCount ; i++, j+=2)
			{
				pOutput[baseOffset+0] = AdjustSampleVolume(snd_p[j], volumeFactor);		 // FL
				pOutput[baseOffset+1] = AdjustSampleVolume(snd_p[j + 1], volumeFactor);	 // FR
				pOutput[baseOffset+2] = AdjustSampleVolume(snd_rp[j], volumeFactor);	 // RL
				pOutput[baseOffset+3] = AdjustSampleVolume(snd_rp[j + 1], volumeFactor); // RR
				baseOffset += 4;
			}
		}
		else
		{
			Assert(channelCount==5); // 6 channel / 5.1
			int baseOffset = lpos * 6;
			for (int i=0, j= 0 ; i<linearCount ; i++, j+=2)
			{
				pOutput[baseOffset+0] = AdjustSampleVolume(snd_p[j], volumeFactor);		 // FL
				pOutput[baseOffset+1] = AdjustSampleVolume(snd_p[j + 1], volumeFactor);	 // FR

				pOutput[baseOffset+2] = AdjustSampleVolume(snd_cp[j], volumeFactor);	 // Center
				// NOTE: Let the hardware mix the sub from the main channels since
				//		 we don't have any sub-specific sounds, or direct sub-addressing
				pOutput[baseOffset+3]  = 0;

				pOutput[baseOffset+4] = AdjustSampleVolume(snd_rp[j], volumeFactor);	 // RL
				pOutput[baseOffset+5] = AdjustSampleVolume(snd_rp[j + 1], volumeFactor); // RR

				baseOffset += 6;
			}
		}

		snd_p += linearCount << 1;
		snd_rp += linearCount << 1;
		snd_cp += linearCount << 1;

		lpaintedtime += linearCount;
	}

	HRESULT hr = pDSBuf->Unlock(write_buffer, buffer_size, nullptr, 0);
	if (FAILED(hr))
	{
		Warning( "dsound: Unable to unlock DirectSound8 buffer w/e 0x%8x.\n", hr );
	}
}

void CAudioDirectSound::S_TransferSurround16Interleaved( portable_samplepair_t *pfront, portable_samplepair_t *prear, portable_samplepair_t *pcenter, int lpaintedtime, int endtime )
{
	if ( !pDSBuf )
		return;

	if ( !snd_lockpartial.GetBool() )
	{
		S_TransferSurround16Interleaved_FullLock( pfront, prear, pcenter, lpaintedtime, endtime );
		return;
	}
	// take stereo front and rear paintbuffers, and center paintbuffer if provided,
	// and copy samples into the 4 or 5 mono directsound buffers

	surround_transfer_t transfer;
	transfer.snd_rp = &(prear->left);
	transfer.snd_cp = &(pcenter->left);
	transfer.snd_p = &(pfront->left);
	
	int sampleMonoCount = DeviceSampleCount()/DeviceChannels();	 // number of full samples per output buffer
	Assert(IsPowerOfTwo(sampleMonoCount));

	transfer.sampleMask = sampleMonoCount - 1;
	transfer.paintedtime = lpaintedtime;
	transfer.linearCount = endtime - lpaintedtime;

	// paintedtime - number of full samples that have played since start
	// endtime - number of full samples to play to - endtime is g_soundtime + mixahead samples
	int channelCount = m_bSurroundCenter ? 6 : 4;
	if ( DeviceChannels() == 2 )
	{
		channelCount = 2;
	}

	transfer.channelCount = channelCount;

	// lpos is next output position in output buffer
	const int next_pos = transfer.paintedtime & transfer.sampleMask;
	const int offset = next_pos * 2 * channelCount;
	const int lock_size = transfer.linearCount * 2 * channelCount;

	unsigned retries_count = 0;
	HRESULT hr;

	void *buffer_0_data = nullptr, *buffer_1_data = nullptr;
	DWORD buffer_0_size, buffer_1_size;
	while ( FAILED(hr = pDSBuf->Lock( offset, lock_size, &buffer_0_data, &buffer_0_size, &buffer_1_data, &buffer_1_size, 0 )) )
	{
		if ( hr == DSERR_BUFFERLOST )
		{
			if ( ++retries_count < 10000u )
				continue;
		}
		
		Warning( "dsound: Unable to lock DirectSound8 buffer w/e 0x%8x.\n", hr );
		return;
	}

	if ( buffer_0_data )
	{
		transfer.pOutput = static_cast<short *>(buffer_0_data);
		TransferSamplesToSurroundBuffer( buffer_0_size / (channelCount * 2), transfer );
	}

	if ( buffer_1_data )
	{
		transfer.pOutput = static_cast<short *>(buffer_1_data);
		TransferSamplesToSurroundBuffer( buffer_1_size / (channelCount * 2), transfer );
	}

	hr = pDSBuf->Unlock(buffer_0_data, buffer_0_size, buffer_1_data, buffer_1_size);
	if (FAILED(hr))
	{
		Warning( "dsound: Unable to unlock DirectSound8 buffer w/e 0x%8x.\n", hr );
	}
}
