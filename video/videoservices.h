//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
//=============================================================================

#ifndef VIDEOSERVICES_H
#define VIDEOSERVICES_H

#if defined ( WIN32 )
    #pragma once
#endif


#include "video/ivideoservices.h"

#include "videosubsystem.h"


struct CVideFileoExtInfo_t
{
	const char			   *m_pExtension;					// extension including "."
	VideoSystem_t			m_VideoSystemSupporting;
	VideoSystemFeature_t	m_VideoFeaturesSupporting;
};


struct CActiveVideoObjectRecord_t
{
	void   *m_pObject;
	int		m_VideoSystem;
};


//-----------------------------------------------------------------------------
// Main VIDEO_SERVICES interface
//-----------------------------------------------------------------------------

class CValveVideoServices final : public CTier3AppSystem< IVideoServices >
{
	typedef CTier3AppSystem< IVideoServices > BaseClass;

	public:
		CValveVideoServices();
		~CValveVideoServices();
	
		// Inherited from IAppSystem 
		bool					Connect( CreateInterfaceFn factory ) override;
		void					Disconnect() override;
		void				   *QueryInterface( const char *pInterfaceName ) override;
		InitReturnVal_t			Init() override;
		void					Shutdown() override;
		
		// Inherited from IVideoServices
	
		// Query the available video systems
		int						GetAvailableVideoSystemCount() override;
		VideoSystem_t			GetAvailableVideoSystem( int n ) override;
		
		bool					IsVideoSystemAvailable( VideoSystem_t videoSystem ) override;
		VideoSystemStatus_t		GetVideoSystemStatus( VideoSystem_t videoSystem ) override;
		VideoSystemFeature_t	GetVideoSystemFeatures( VideoSystem_t videoSystem ) override;
		const char			   *GetVideoSystemName( VideoSystem_t videoSystem ) override;

		VideoSystem_t			FindNextSystemWithFeature( VideoSystemFeature_t features, VideoSystem_t startAfter = VideoSystem::NONE ) override;
		
		VideoResult_t			GetLastResult() override;
		
		// deal with video file extensions and video system mappings
		int						GetSupportedFileExtensionCount( VideoSystem_t videoSystem ) override;		
		const char             *GetSupportedFileExtension( VideoSystem_t videoSystem, int extNum = 0 ) override;
		VideoSystemFeature_t    GetSupportedFileExtensionFeatures( VideoSystem_t videoSystem, int extNum = 0 ) override;


		VideoSystem_t			LocateVideoSystemForPlayingFile( const char *pFileName, VideoSystemFeature_t playMode = VideoSystemFeature::PLAY_VIDEO_FILE_IN_MATERIAL ) override;
		VideoResult_t			LocatePlayableVideoFile( const char *pSearchFileName, const char *pPathID, VideoSystem_t *pPlaybackSystem, char *pPlaybackFileName, int fileNameMaxLen, VideoSystemFeature_t playMode = VideoSystemFeature::FULL_PLAYBACK ) override;

		// Create/destroy a video material
		IVideoMaterial		   *CreateVideoMaterial( const char *pMaterialName, const char *pVideoFileName, const char *pPathID = nullptr, 
													 VideoPlaybackFlags_t playbackFlags = VideoPlaybackFlags::DEFAULT_MATERIAL_OPTIONS, 
													 VideoSystem_t videoSystem = VideoSystem::DETERMINE_FROM_FILE_EXTENSION, bool PlayAlternateIfNotAvailable = true ) override;
													 
		VideoResult_t			DestroyVideoMaterial( IVideoMaterial* pVideoMaterial ) override;
		int						GetUniqueMaterialID() override;

		// Create/destroy a video encoder
		VideoResult_t			IsRecordCodecAvailable( VideoSystem_t videoSystem, VideoEncodeCodec_t codec ) override;
		
		IVideoRecorder		   *CreateVideoRecorder( VideoSystem_t videoSystem ) override;
		VideoResult_t			DestroyVideoRecorder( IVideoRecorder *pVideoRecorder ) override;

		// Plays a given video file until it completes or the user presses ESC, SPACE, or ENTER
		VideoResult_t			PlayVideoFileFullScreen( const char *pFileName, const char *pPathID, void *mainWindow, int windowWidth, int windowHeight, int desktopWidth, int desktopHeight, bool windowed, float forcedMinTime, 
																 VideoPlaybackFlags_t playbackFlags = VideoPlaybackFlags::DEFAULT_FULLSCREEN_OPTIONS, 
																 VideoSystem_t videoSystem = VideoSystem::DETERMINE_FROM_FILE_EXTENSION, bool PlayAlternateIfNotAvailable = true ) override;

		// Sets the sound devices that the video will decode to
		VideoResult_t			SoundDeviceCommand( VideoSoundDeviceOperation_t operation, void *pDevice = nullptr, void *pData = nullptr, VideoSystem_t videoSystem = VideoSystem::ALL_VIDEO_SYSTEMS ) override;

		// Get the name of a codec as a string
		const wchar_t					*GetCodecName( VideoEncodeCodec_t nCodec ) override;

	private:
	
		VideoResult_t					ResolveToPlayableVideoFile( const char *pFileName, const char *pPathID, VideoSystem_t videoSystem, VideoSystemFeature_t requiredFeature, bool PlayAlternateIfNotAvailable, 
																	char *pResolvedFileName, int resolvedFileNameMaxLen, VideoSystem_t *pResolvedVideoSystem );
	
	
		VideoSystem_t					LocateSystemAndFeaturesForFileName( const char *pFileName, VideoSystemFeature_t *pFeatures = nullptr, VideoSystemFeature_t requiredFeatures = VideoSystemFeature::NO_FEATURES );
		
		bool							IsMatchAnyExtension( const char *pFileName );
	
		bool							ConnectVideoLibraries( CreateInterfaceFn factory );
		bool							DisconnectVideoLibraries();
		
		ptrdiff_t								DestroyAllVideoInterfaces();

		int								GetIndexForSystem( VideoSystem_t n );
		VideoSystem_t					GetSystemForIndex( int n );
		
		VideoResult_t					SetResult( VideoResult_t resultCode );
		
		const char					   *GetFileExtension( const char *pFileName );
		
		
		static const int				SYSTEM_NOT_FOUND = -1;
		
		VideoResult_t					m_LastResult;
		
		int								m_nInstalledSystems;
		bool							m_bInitialized;
		
		CSysModule					   *m_VideoSystemModule[VideoSystem::VIDEO_SYSTEM_COUNT];
		IVideoSubSystem				   *m_VideoSystems[VideoSystem::VIDEO_SYSTEM_COUNT];
		VideoSystem_t					m_VideoSystemType[VideoSystem::VIDEO_SYSTEM_COUNT];
		VideoSystemFeature_t			m_VideoSystemFeatures[VideoSystem::VIDEO_SYSTEM_COUNT];
		
		CUtlVector< VideoFileExtensionInfo_t >	m_ExtInfo;			// info about supported file extensions
		
		CUtlVector< CActiveVideoObjectRecord_t > m_RecorderList;
		CUtlVector< CActiveVideoObjectRecord_t > m_MaterialList;
		
		int								m_nMaterialCount;				
			
};


class CVideoCommonServices final : public IVideoCommonServices
{
	public:
	
		CVideoCommonServices();
		virtual ~CVideoCommonServices();
	
	
		bool			CalculateVideoDimensions( int videoWidth, int videoHeight, int displayWidth, int displayHeight, VideoPlaybackFlags_t playbackFlags, 
											  int *pOutputWidth, int *pOutputHeight, int *pXOffset, int *pYOffset ) override;

		float			GetSystemVolume() override;
											  
		VideoResult_t	InitFullScreenPlaybackInputHandler( VideoPlaybackFlags_t playbackFlags, float forcedMinTime, bool windowed ) override;
		
		bool			ProcessFullScreenInput( bool &bAbortEvent, bool &bPauseEvent, bool &bQuitEvent ) override;
		
		VideoResult_t	TerminateFullScreenPlaybackInputHandler() override;


	private:
		
		void					ResetInputHandlerState();
	
		bool					m_bInputHandlerInitialized;
	
		bool					m_bScanAll;
		bool					m_bScanEsc;
		bool					m_bScanReturn;
		bool					m_bScanSpace;
		bool					m_bPauseEnabled;
		bool					m_bAbortEnabled;
		bool					m_bEscLast;
		bool					m_bReturnLast;
		bool					m_bSpaceLast;
		bool					m_bForceMinPlayTime;
		
		bool					m_bWindowed;
		VideoPlaybackFlags_t	m_playbackFlags;
		float					m_forcedMinTime;
		
		double					m_StartTime;
		

};


#endif		// VIDEOSERVICES_H
