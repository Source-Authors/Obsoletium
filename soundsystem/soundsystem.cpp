//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: DLL interface for low-level sound utilities
//
//===========================================================================//

#include "soundsystem/isoundsystem.h"

#include "tier1/convar.h"
#include "tier1/strtools.h"
#include "tier1/utldict.h"
#include "mathlib/mathlib.h"
#include "soundsystem/snd_device.h"
#include "datacache/idatacache.h"
#include "tier2/tier2.h"
#include "filesystem.h"
#include "soundchars.h"
#include "snd_wave_source.h"
#include "snd_dev_wave.h"


//-----------------------------------------------------------------------------
// External interfaces
//-----------------------------------------------------------------------------
IAudioDevice *g_pAudioDevice = nullptr;
ISoundSystem *g_pSoundSystem = nullptr;
IDataCache *g_pDataCache = nullptr;


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
int g_nSoundFrameCount = 0;


//-----------------------------------------------------------------------------
// Purpose: DLL interface for low-level sound utilities
//-----------------------------------------------------------------------------
class CSoundSystem : public CTier2AppSystem< ISoundSystem >
{
	using BaseClass = CTier2AppSystem< ISoundSystem >;

public:
	// Inherited from IAppSystem
	bool Connect( CreateInterfaceFn factory ) override;
	void Disconnect() override;
	void *QueryInterface( const char *pInterfaceName ) override;
	InitReturnVal_t Init() override;
	void Shutdown() override;

	void		Update( float dt ) override;
	void		Flush( void ) override;

	CAudioSource *FindOrAddSound( const char *filename ) override;
	CAudioSource *LoadSound( const char *wavfile ) override;
	void		PlaySound( CAudioSource *source, float volume, CAudioMixer **ppMixer ) override;

	bool		IsSoundPlaying( CAudioMixer *pMixer ) override;
	CAudioMixer *FindMixer( CAudioSource *source ) override;

	void		StopAll( void ) override;
	void		StopSound( CAudioMixer *mixer ) override;

private:
	struct CSoundFile
	{
		char				filename[ 512 ];
		CAudioSource		*source;
		time_t				filetime;
	};

	IAudioDevice *m_pAudioDevice;
	float		m_flElapsedTime;
	CUtlVector < CSoundFile > m_ActiveSounds;
};


//-----------------------------------------------------------------------------
// Singleton interface
//-----------------------------------------------------------------------------
static CSoundSystem s_SoundSystem;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CSoundSystem, ISoundSystem, SOUNDSYSTEM_INTERFACE_VERSION, s_SoundSystem );


//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CSoundSystem::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;

	g_pDataCache = (IDataCache*)factory( DATACACHE_INTERFACE_VERSION, nullptr );
	g_pSoundSystem = this;

	return g_pFullFileSystem && g_pDataCache;
}

void CSoundSystem::Disconnect()
{
	g_pSoundSystem = nullptr;
	g_pDataCache = nullptr;
	BaseClass::Disconnect();
}


//-----------------------------------------------------------------------------
// Query interface
//-----------------------------------------------------------------------------
void *CSoundSystem::QueryInterface( const char *pInterfaceName )
{
	if (!Q_strncmp(	pInterfaceName, SOUNDSYSTEM_INTERFACE_VERSION, Q_strlen(SOUNDSYSTEM_INTERFACE_VERSION) + 1))
		return (ISoundSystem*)this;

	return nullptr;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
InitReturnVal_t CSoundSystem::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2 );

	m_flElapsedTime = 0.0f;
	m_pAudioDevice = Audio_CreateWaveDevice();
	if ( !m_pAudioDevice->Init() )
		return INIT_FAILED;

	return INIT_OK;
}

void CSoundSystem::Shutdown()
{
	Msg( "Removing %zd sounds\n", m_ActiveSounds.Count() );
	for ( auto &s : m_ActiveSounds )
	{
		Msg( "Removing sound:  %s\n", s.filename );
		delete s.source;
	}

	m_ActiveSounds.RemoveAll();

	if ( m_pAudioDevice )
	{
		m_pAudioDevice->Shutdown();
		delete m_pAudioDevice;
	}

	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAudioSource *CSoundSystem::FindOrAddSound( const char *filename )
{
	CSoundFile *s;

	intp i;
	for ( i = 0; i < m_ActiveSounds.Count(); i++ )
	{
		s = &m_ActiveSounds[ i ];
		Assert( s );
		if ( !stricmp( s->filename, filename ) )
		{
			time_t filetime = g_pFullFileSystem->GetFileTime( filename );
			if ( filetime != s->filetime )
			{
				Msg( "Reloading sound %s.\n", filename );

				delete s->source;
				s->source = LoadSound( filename );
				s->filetime = filetime;
			}
			return s->source;
		}
	}

	i = m_ActiveSounds.AddToTail();
	s = &m_ActiveSounds[ i ];
	V_strcpy_safe( s->filename, filename );
	s->source = LoadSound( filename );
	s->filetime = g_pFullFileSystem->GetFileTime( filename );

	return s->source;
}

CAudioSource *CSoundSystem::LoadSound( const char *wavfile )
{
	if ( !m_pAudioDevice )
		return nullptr;

	CAudioSource *wave = AudioSource_Create( wavfile );
	return wave;
}

void CSoundSystem::PlaySound( CAudioSource *source, float volume, CAudioMixer **ppMixer )
{
	if ( ppMixer )
	{
		*ppMixer = nullptr;
	}

	if ( m_pAudioDevice )
	{
		CAudioMixer *mixer = source->CreateMixer();
		if ( ppMixer )
		{
			*ppMixer = mixer;
		}
		mixer->SetVolume( volume );
		m_pAudioDevice->AddSource( mixer );
	}
}

void CSoundSystem::Update( float dt )
{
//	closecaptionmanager->PreProcess( g_nSoundFrameCount );

	if ( m_pAudioDevice )
	{
		m_pAudioDevice->Update( m_flElapsedTime );
	}

//	closecaptionmanager->PostProcess( g_nSoundFrameCount, dt );

	m_flElapsedTime += dt;
	g_nSoundFrameCount++;
}

void CSoundSystem::Flush( void )
{
	if ( m_pAudioDevice )
	{
		m_pAudioDevice->Flush();
	}
}

void CSoundSystem::StopAll( void )
{
	if ( m_pAudioDevice )
	{
		m_pAudioDevice->StopSounds();
	}
}

void CSoundSystem::StopSound( CAudioMixer *mixer )
{
	int idx = m_pAudioDevice->FindSourceIndex( mixer );
	if ( idx != -1 )
	{
		m_pAudioDevice->FreeChannel( idx );
	}
}

bool CSoundSystem::IsSoundPlaying( CAudioMixer *pMixer )
{
	if ( !m_pAudioDevice || !pMixer )
		return false;

	//
	int index = m_pAudioDevice->FindSourceIndex( pMixer );
	if ( index != -1 )
		return true;

	return false;
}

CAudioMixer *CSoundSystem::FindMixer( CAudioSource *source )
{
	if ( !m_pAudioDevice )
		return nullptr;

	return m_pAudioDevice->GetMixerForSource( source );
}