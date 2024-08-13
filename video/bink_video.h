//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef BINK_VIDEO_H
#define BINK_VIDEO_H

#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IFileSystem;
class IMaterialSystem;

//-----------------------------------------------------------------------------
// Global interfaces - you already did the needed includes, right?
//-----------------------------------------------------------------------------
extern IFileSystem		*g_pFileSystem;
extern IMaterialSystem	*materials;

#include "video/ivideoservices.h"
#include "videosubsystem.h"

#include "utlvector.h"
#include "tier1/KeyValues.h"
#include "tier0/platform.h"

// -----------------------------------------------------------------------------
// CBinkVideoSubSystem - Implementation of IVideoSubSystem
// -----------------------------------------------------------------------------
class CBinkVideoSubSystem final : public CTier2AppSystem< IVideoSubSystem >
{
	typedef CTier2AppSystem< IVideoSubSystem > BaseClass;

	public:
		CBinkVideoSubSystem();
		~CBinkVideoSubSystem();

		// Inherited from IAppSystem 
		bool					Connect( CreateInterfaceFn factory ) override;
		void					Disconnect() override;
		void				   *QueryInterface( const char *pInterfaceName ) override;
		InitReturnVal_t			Init() override;
		void					Shutdown() override;

		// Inherited from IVideoSubSystem
		 
		// SubSystem Identification functions
		VideoSystem_t			GetSystemID() override;
		VideoSystemStatus_t		GetSystemStatus() override;
		VideoSystemFeature_t	GetSupportedFeatures() override;
		const char			   *GetVideoSystemName() override;
		
		// Setup & Shutdown Services
		bool					InitializeVideoSystem( IVideoCommonServices *pCommonServices ) override;
		bool					ShutdownVideoSystem() override;
		
		VideoResult_t			VideoSoundDeviceCMD( VideoSoundDeviceOperation_t operation, void *pDevice = nullptr, void *pData = nullptr ) override;
		
		// get list of file extensions and features we support
		int						GetSupportedFileExtensionCount() override;
		const char			   *GetSupportedFileExtension( int num ) override;
		VideoSystemFeature_t	GetSupportedFileExtensionFeatures( int num ) override;

		// Video Playback and Recording Services
		VideoResult_t			PlayVideoFileFullScreen( const char *filename, void *mainWindow, int windowWidth, int windowHeight, int desktopWidth, int desktopHeight, bool windowed, float forcedMinTime, VideoPlaybackFlags_t playbackFlags ) override;

		// Create/destroy a video material
		IVideoMaterial		   *CreateVideoMaterial( const char *pMaterialName, const char *pVideoFileName, VideoPlaybackFlags_t flags ) override;
		VideoResult_t			DestroyVideoMaterial( IVideoMaterial *pVideoMaterial ) override;

		// Create/destroy a video encoder		
		IVideoRecorder		   *CreateVideoRecorder() override;
		VideoResult_t			DestroyVideoRecorder( IVideoRecorder *pRecorder ) override;
		
		VideoResult_t			CheckCodecAvailability( VideoEncodeCodec_t codec ) override;
		
		VideoResult_t			GetLastResult() override;

	private:
	
		bool							SetupBink();
		bool							ShutdownBink();

		VideoResult_t					SetResult( VideoResult_t status );

		bool							m_bBinkInitialized;
		VideoResult_t					m_LastResult;
		
		VideoSystemStatus_t				m_CurrentStatus;
		VideoSystemFeature_t			m_AvailableFeatures;

		IVideoCommonServices		   *m_pCommonServices;
		
		CUtlVector< IVideoMaterial* >	m_MaterialList;
		CUtlVector< IVideoRecorder* >	m_RecorderList;
		
		static const VideoSystemFeature_t	DEFAULT_FEATURE_SET;
};

#endif // BINK_VIDEO_H
