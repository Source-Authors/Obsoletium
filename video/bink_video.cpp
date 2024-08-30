
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "bink_video.h"
#include "video_macros.h"

#include "filesystem.h"
#include "tier0/icommandline.h"
#include "tier1/strtools.h"
#include "tier1/utllinkedlist.h"
#include "tier1/KeyValues.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/itexture.h"
#include "vtf/vtf.h"
#include "pixelwriter.h"
#include "tier2/tier2.h"
#include "platform.h"
#include "bink_material.h"

#include "bink/include/bink.h"

#ifdef _WIN32
#include "winlite.h"
#include <combaseapi.h>
#endif

#include "tier0/memdbgon.h"

// ===========================================================================
// Singleton to expose Bink video subsystem
// ===========================================================================
static CBinkVideoSubSystem g_BinkSystem;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CBinkVideoSubSystem, IVideoSubSystem, VIDEO_SUBSYSTEM_INTERFACE_VERSION, g_BinkSystem );


// ===========================================================================
// List of file extensions and features supported by this subsystem
// ===========================================================================
VideoFileExtensionInfo_t s_BinkExtensions[] = 
{
	{ ".bik", VideoSystem::BINK,  VideoSystemFeature::PLAY_VIDEO_FILE_IN_MATERIAL | VideoSystemFeature::PLAY_VIDEO_FILE_FULL_SCREEN },
};

constexpr ptrdiff_t s_BinkExtensionCount = ssize( s_BinkExtensions );
constexpr VideoSystemFeature_t	CBinkVideoSubSystem::DEFAULT_FEATURE_SET = VideoSystemFeature::PLAY_VIDEO_FILE_IN_MATERIAL;

// ===========================================================================
// CBinkVideoSubSystem class
// ===========================================================================
CBinkVideoSubSystem::CBinkVideoSubSystem() :
	m_bBinkInitialized( false ),
	m_LastResult( VideoResult::SUCCESS ),
	m_CurrentStatus( VideoSystemStatus::NOT_INITIALIZED ),
	m_AvailableFeatures( CBinkVideoSubSystem::DEFAULT_FEATURE_SET ),
	m_pCommonServices( nullptr )
{

}

CBinkVideoSubSystem::~CBinkVideoSubSystem()
{
	ShutdownBink();		// Super redundant safety check
}

// ===========================================================================
// IAppSystem methods
// ===========================================================================
bool CBinkVideoSubSystem::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
	{
		return false;
	}

	if ( g_pFullFileSystem == nullptr || materials == nullptr ) 
	{
		Msg( "Bink video subsystem failed to connect to missing a required system\n" );
		return false;
	}
	return true;
}

void CBinkVideoSubSystem::Disconnect()
{
	BaseClass::Disconnect();
}

void* CBinkVideoSubSystem::QueryInterface( const char *pInterfaceName )
{

	if ( IS_NOT_EMPTY( pInterfaceName ) )
	{
		if ( V_strncmp(	pInterfaceName, VIDEO_SUBSYSTEM_INTERFACE_VERSION, ssize( VIDEO_SUBSYSTEM_INTERFACE_VERSION ) ) == STRINGS_MATCH )
		{
			return (IVideoSubSystem*) this;
		}
	}

	return nullptr;
}

static void* RADLINK BinkAlloc( U32 bytes )
{
	return MemAlloc_AllocAligned( bytes, 16 );
}

static void RADLINK BinkFree( void *mem )
{
	return MemAlloc_FreeAligned( mem );
}

InitReturnVal_t CBinkVideoSubSystem::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
	{
		return nRetVal;
	}

	Msg( "Start up Bink Video %s (%u.%u.%u)\n",
		BINKVERSION, BINKMAJORVERSION, BINKMINORVERSION, BINKSUBVERSION );

	// dimhotepus: Use our allocators (ASAN happier).
	BinkSetMemory( BinkAlloc, BinkFree );

	return INIT_OK;
}

void CBinkVideoSubSystem::Shutdown()
{
	// Make sure we shut down Bink
	ShutdownBink();

	Msg("Shut down Bink Video %s (%u.%u.%u)\n",
		BINKVERSION, BINKMAJORVERSION, BINKMINORVERSION, BINKSUBVERSION);

	BaseClass::Shutdown();
}


// ===========================================================================
// IVideoSubSystem identification methods  
// ===========================================================================
VideoSystem_t CBinkVideoSubSystem::GetSystemID()
{
	return VideoSystem::BINK;
}


VideoSystemStatus_t CBinkVideoSubSystem::GetSystemStatus()
{
	return m_CurrentStatus;
}


VideoSystemFeature_t CBinkVideoSubSystem::GetSupportedFeatures()
{
	return m_AvailableFeatures;
}


const char* CBinkVideoSubSystem::GetVideoSystemName()
{
	return "BINK";
}


// ===========================================================================
// IVideoSubSystem setup and shutdown services
// ===========================================================================
bool CBinkVideoSubSystem::InitializeVideoSystem( IVideoCommonServices *pCommonServices )
{
	m_AvailableFeatures = DEFAULT_FEATURE_SET;			// Put here because of issue with static const int, binary OR and DEBUG builds

	AssertPtr( pCommonServices );
	m_pCommonServices = pCommonServices;

	return ( m_bBinkInitialized ) ? true : SetupBink();
}


bool CBinkVideoSubSystem::ShutdownVideoSystem()
{
	return (  m_bBinkInitialized ) ? ShutdownBink() : true;
}


VideoResult_t CBinkVideoSubSystem::VideoSoundDeviceCMD( VideoSoundDeviceOperation_t operation, void *pDevice, void * )
{
	switch ( operation ) 
	{
		case VideoSoundDeviceOperation::SET_DIRECT_SOUND_DEVICE:
		{
#ifdef __RADWIN__
			return SetResult( BinkSoundUseDirectSound( pDevice ) ? VideoResult::SUCCESS : VideoResult::AUDIO_ERROR_OCCURED );
#else
			// On any other OS, we don't support this operation
			return SetResult( VideoResult::OPERATION_NOT_SUPPORTED );
#endif
		}

		case VideoSoundDeviceOperation::SET_MILES_SOUND_DEVICE:
		{
#ifdef __RADWIN__
			return SetResult( BinkSoundUseMiles( pDevice ) ? VideoResult::SUCCESS : VideoResult::AUDIO_ERROR_OCCURED );
#else
			// On any other OS, we don't support this operation
			return SetResult( VideoResult::OPERATION_NOT_SUPPORTED );
#endif
		}

		case VideoSoundDeviceOperation::HOOK_X_AUDIO:
		{
#ifdef __RADXENON__
			BinkSoundUseXAudio();
			return SetResult( VideoResult::SUCCESS );
#else
			return SetResult( VideoResult::OPERATION_NOT_SUPPORTED );
#endif
		}

		case VideoSoundDeviceOperation::SET_SOUND_MANAGER_DEVICE:
		{
#ifdef __RADMAC__
			return SetResult( BinkSoundUseSoundManager() ? VideoResult::SUCCESS : VideoResult::AUDIO_ERROR_OCCURED );
#else
			return SetResult( VideoResult::OPERATION_NOT_SUPPORTED );
#endif
		}

		case VideoSoundDeviceOperation::SET_SDL_SOUND_DEVICE:
		{
#ifdef __RADLINUX__
			return SetResult( BinkSoundUseSDLMixer() ? VideoResult::SUCCESS : VideoResult::AUDIO_ERROR_OCCURED );
#else
			return SetResult( VideoResult::OPERATION_NOT_SUPPORTED );
#endif
		}

		default:
		{
			return SetResult( VideoResult::UNKNOWN_OPERATION );
		}
	}
}


// ===========================================================================
// IVideoSubSystem supported extensions & features
// ===========================================================================
int CBinkVideoSubSystem::GetSupportedFileExtensionCount()
{
	return s_BinkExtensionCount;
}

 
const char* CBinkVideoSubSystem::GetSupportedFileExtension( int num )
{
	return ( num < 0 || num >= s_BinkExtensionCount ) ? nullptr : s_BinkExtensions[num].m_FileExtension;
}

 
VideoSystemFeature_t CBinkVideoSubSystem::GetSupportedFileExtensionFeatures( int num )
{
	 return ( num < 0 || num >= s_BinkExtensionCount ) ? VideoSystemFeature::NO_FEATURES : s_BinkExtensions[num].m_VideoFeatures;
}


// ===========================================================================
// IVideoSubSystem Video Playback and Recording Services
// ===========================================================================
VideoResult_t CBinkVideoSubSystem::PlayVideoFileFullScreen( const char *filename, void *mainWindow, int windowWidth, int windowHeight, int desktopWidth, int desktopHeight, bool windowed, float forcedMinTime, VideoPlaybackFlags_t playbackFlags )
{
	// See if the caller is asking for a feature we can not support....
	VideoPlaybackFlags_t unsupportedFeatures = VideoPlaybackFlags::PRELOAD_VIDEO | VideoPlaybackFlags::LOOP_VIDEO;
	if ( playbackFlags & unsupportedFeatures )
	{
		return SetResult( VideoResult::FEATURE_NOT_AVAILABLE );
	}

	AssertExitV( m_CurrentStatus == VideoSystemStatus::OK, SetResult( VideoResult::SYSTEM_NOT_AVAILABLE ) );

#if defined ( OSX )
	SystemUIMode oldMode;
	SystemUIOptions oldOptions;
	GetSystemUIMode( &oldMode, &oldOptions );

	if ( !windowed )
	{
		status = SetSystemUIMode( kUIModeAllHidden, (SystemUIOptions) 0 );
		Assert( status == noErr );
	}
	SetGWorld( nil, nil );
#endif

	// Check to see if we should include audio playback
	bool enableMovieAudio = !BITFLAGS_SET( playbackFlags, VideoPlaybackFlags::NO_AUDIO );

	// Open the bink file with audio
	HBINK hBINK = BinkOpen( filename, enableMovieAudio ? BINKSNDTRACK : 0 );
	if ( hBINK == 0 )
	{
		Warning( "Bink video %s open error: %s.", filename, BinkGetError() );
		return SetResult( VideoResult::FILE_ERROR_OCCURED );
	}

	// what size do we set the output rect to?
	// Integral scaling is much faster, so always scale the video as such
	int nNewWidth  = (int) hBINK->Width;
	int nNewHeight = (int) hBINK->Height;

	// Determine the window we are rendering video into
	int displayWidth  = windowWidth;
	int displayHeight = windowHeight;

	// on mac OSX, if we are fullscreen, bink is bypassing our targets and going to the display directly, so use its dimensions
	if ( IsOSX() && windowed == false )
	{
		displayWidth = desktopWidth;
		displayHeight = desktopHeight;
	}

	// get the size of the target video output
	int	nBufferWidth = nNewWidth;
	int nBufferHeight = nNewHeight;
	
	int displayXOffset = 0;
	int displayYOffset = 0;

	if ( !m_pCommonServices->CalculateVideoDimensions( nNewWidth, nNewHeight, displayWidth, displayHeight, playbackFlags, &nBufferWidth, &nBufferHeight, &displayXOffset, &displayYOffset ) )
	{
#if defined ( OSX )
		SetSystemUIMode( oldMode, oldOptions );
#endif
		BinkClose( hBINK );

		return SetResult( VideoResult::VIDEO_ERROR_OCCURED );
	}

	// Create a buffer to decompress to
	// NOTE: The DIB version is the only one we can call on without DirectDraw
	HBINKBUFFER hBINKBuffer = BinkBufferOpen(
		mainWindow, hBINK->Width, hBINK->Height,
		BINKBUFFERDIBSECTION | BINKBUFFERSTRETCHXINT | BINKBUFFERSTRETCHYINT |
			BINKBUFFERSHRINKXINT | BINKBUFFERSHRINKYINT );
	if ( hBINKBuffer == 0 )
	{
		BinkClose( hBINK );
		
		Warning( "Bink video %s buffer open error: %s.", filename, BinkGetError() );
		return SetResult( VideoResult::FILE_ERROR_OCCURED );
	}

	// Scale if we need to
	BinkBufferSetScale( hBINKBuffer, nBufferWidth, nBufferHeight );

	// Offset to the middle of the screen
	BinkBufferSetOffset( hBINKBuffer, displayXOffset, displayYOffset );

	// Setup the keyboard and message handler for fullscreen playback
	if ( SetResult( m_pCommonServices->InitFullScreenPlaybackInputHandler( playbackFlags, forcedMinTime, windowed ) ) != VideoResult::SUCCESS )
	{
#if defined ( OSX )
		SetSystemUIMode( oldMode, oldOptions );
#endif

		BinkBufferClose( hBINKBuffer );
		BinkClose( hBINK );

		return GetLastResult();
	}

	// Other Movie playback	state init
	bool bPaused = false;

	while ( true ) {
		bool bAbortEvent, bPauseEvent, bQuitEvent;
	
		if ( m_pCommonServices->ProcessFullScreenInput( bAbortEvent, bPauseEvent, bQuitEvent ) )
		{
			// check for aborting the movie
			if ( bAbortEvent || bQuitEvent )
			{
				goto abort_playback;
			}

			// check for pausing the movie?
			if ( bPauseEvent )
			{
				if ( bPaused == false )		// pausing the movie?
				{
					BinkPause( hBINK, 1 );
					bPaused = true;
				}
				else	// unpause the movie
				{
					BinkPause( hBINK, 0 );
					bPaused = false;
				}
			}
		}

		// if the movie is paused, sleep for 1ms to keeps the CPU from spinning so hard
		if ( bPaused )
		{
			ThreadSleep( 1 );
		}
		else
		{
			// Decompress this frame
			BinkDoFrame( hBINK );

			// Lock the buffer for writing
			if ( BinkBufferLock( hBINKBuffer ) ) {
				// Copy the decompressed frame into the BinkBuffer
				BinkCopyToBuffer( hBINK, hBINKBuffer->Buffer, hBINKBuffer->BufferPitch,
									hBINKBuffer->Height, 0, 0, hBINKBuffer->SurfaceType );

				// Unlock the buffer
				BinkBufferUnlock( hBINKBuffer );
			}

			// Blit the pixels to the screen
			BinkBufferBlit( hBINKBuffer, hBINK->FrameRects,
							BinkGetRects( hBINK, hBINKBuffer->SurfaceType ) );

			// Wait until the next frame is ready
			while ( BinkWait( hBINK ) ) {
				ThreadSleep( 1 );
			}

			// Check for video being complete
			if ( hBINK->FrameNum == hBINK->Frames ) break;

			// Move on
			BinkNextFrame( hBINK );
		}
	}

	// Close it all down
abort_playback:
	// Close it all down
	BinkBufferClose( hBINKBuffer );
	BinkClose( hBINK );

	return SetResult( VideoResult::SUCCESS );
}


// ===========================================================================
// IVideoSubSystem Video Material Services
//   note that the filename is absolute and has already resolved any paths
// ===========================================================================
IVideoMaterial* CBinkVideoSubSystem::CreateVideoMaterial( const char *pMaterialName, const char *pVideoFileName, VideoPlaybackFlags_t flags )
{
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitN( m_CurrentStatus == VideoSystemStatus::OK && IS_NOT_EMPTY( pMaterialName ) || IS_NOT_EMPTY( pVideoFileName ) );

	CBinkMaterial *pVideoMaterial = new CBinkMaterial();
	if ( pVideoMaterial == nullptr || pVideoMaterial->Init( pMaterialName, pVideoFileName, flags ) == false )
	{
		SAFE_DELETE( pVideoMaterial );
		SetResult( VideoResult::VIDEO_ERROR_OCCURED );
		return nullptr;
	}

	IVideoMaterial	*pInterface = (IVideoMaterial*) pVideoMaterial;
	m_MaterialList.AddToTail( pInterface );

	SetResult( VideoResult::SUCCESS );
	return pInterface;
}


VideoResult_t CBinkVideoSubSystem::DestroyVideoMaterial( IVideoMaterial *pVideoMaterial )
{
	AssertExitV( m_CurrentStatus == VideoSystemStatus::OK, SetResult( VideoResult::SYSTEM_NOT_AVAILABLE ) );
	AssertPtrExitV( pVideoMaterial, SetResult( VideoResult::BAD_INPUT_PARAMETERS ) );

	if ( m_MaterialList.Find( pVideoMaterial ) != -1 )
	{
		CBinkMaterial *pObject = (CBinkMaterial*) pVideoMaterial;
		pObject->Shutdown();
		delete pObject;

		m_MaterialList.FindAndFastRemove( pVideoMaterial );

		return SetResult( VideoResult::SUCCESS );
	}

	return SetResult (VideoResult::MATERIAL_NOT_FOUND );
}


// ===========================================================================
// IVideoSubSystem Video Recorder Services
// ===========================================================================
IVideoRecorder* CBinkVideoSubSystem::CreateVideoRecorder()
{
	SetResult( VideoResult::FEATURE_NOT_AVAILABLE );
	return nullptr;
}


VideoResult_t CBinkVideoSubSystem::DestroyVideoRecorder( IVideoRecorder * )
{
	return SetResult( VideoResult::FEATURE_NOT_AVAILABLE );
}

VideoResult_t CBinkVideoSubSystem::CheckCodecAvailability( VideoEncodeCodec_t codec )
{
	AssertExitV( m_CurrentStatus == VideoSystemStatus::OK, SetResult( VideoResult::SYSTEM_NOT_AVAILABLE ) );
	AssertExitV( codec >= VideoEncodeCodec::DEFAULT_CODEC && codec < VideoEncodeCodec::CODEC_COUNT, SetResult( VideoResult::BAD_INPUT_PARAMETERS ) );

	return SetResult( VideoResult::FEATURE_NOT_AVAILABLE );
}


// ===========================================================================
// Status support
// ===========================================================================
VideoResult_t CBinkVideoSubSystem::GetLastResult()
{
	return m_LastResult;
}


VideoResult_t CBinkVideoSubSystem::SetResult( VideoResult_t status )
{
	m_LastResult = status;
	return status;
}


// ===========================================================================
// Bink Initialization & Shutdown
// ===========================================================================
bool CBinkVideoSubSystem::SetupBink()
{
	SetResult( VideoResult::INITIALIZATION_ERROR_OCCURED);
	AssertExitF( m_bBinkInitialized == false );

	// This is set early to indicate we have already been through here, even if we error out for some reason
	m_bBinkInitialized = true;
	m_CurrentStatus = VideoSystemStatus::OK;
	m_AvailableFeatures = DEFAULT_FEATURE_SET;
	// $$INIT CODE HERE$$

	// Bink needs COM.
#ifdef _WIN32
	if ( SUCCEEDED( ::CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY ) ) )
	{
		// Note that we are now open for business....	
		m_bBinkInitialized = true;
		SetResult( VideoResult::SUCCESS );
	}
	else
	{
		SetResult( VideoResult::INITIALIZATION_ERROR_OCCURED );
	}
#else
	// Note that we are now open for business....	
	m_bBinkInitialized = true;
	SetResult( VideoResult::SUCCESS );
#endif

	return true;
}


bool CBinkVideoSubSystem::ShutdownBink()
{
	if ( m_bBinkInitialized && m_CurrentStatus == VideoSystemStatus::OK )
	{
#ifdef _WIN32
		::CoUninitialize();
#endif
	}

	m_bBinkInitialized = false;
	m_CurrentStatus = VideoSystemStatus::NOT_INITIALIZED;
	m_AvailableFeatures = VideoSystemFeature::NO_FEATURES;
	SetResult( VideoResult::SUCCESS );

	return true;
}

