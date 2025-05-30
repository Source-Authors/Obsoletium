//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef IENGINEREPLAY_H
#define IENGINEREPLAY_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "tier1/interface.h"

//----------------------------------------------------------------------------------------

constexpr inline char ENGINE_REPLAY_INTERFACE_VERSION[]{"EngineReplay001"};

#ifndef DEDICATED
constexpr inline char ENGINE_REPLAY_CLIENT_INTERFACE_VERSION[]{"EngineClientReplay001"};
#endif

//----------------------------------------------------------------------------------------

class IServer;
class INetChannel;
class IReplayServer;
class IClientEntityList;
class IClientReplay;
struct demoheader_t;
class CGlobalVarsBase;
class IDemoBuffer;
class CBaseRecordingSessionBlock;

//----------------------------------------------------------------------------------------

//
// Allows the replay, client & server DLL's to talk to the engine
//
class IEngineReplay : public IBaseInterface
{
public:
	[[nodiscard]] virtual bool			IsSupportedModAndPlatform() = 0;

	[[nodiscard]] virtual float			GetHostTime() = 0;
	[[nodiscard]] virtual int			GetHostTickCount() = 0;
	[[nodiscard]] virtual int			TimeToTicks( float flTime ) = 0;
	[[nodiscard]] virtual float			TicksToTime( int nTick ) = 0;

	virtual bool			ReadDemoHeader( const char *pFilename, demoheader_t &header ) = 0;
	[[nodiscard]] virtual const char		*GetGameDir() = 0;
	virtual void			Cbuf_AddText( const char *pCmd ) = 0;
	virtual void			Cbuf_Execute() = 0;
	virtual void			Host_Disconnect( bool bShowMainMenu ) = 0;
	virtual void			HostState_Shutdown() = 0;
	[[nodiscard]] virtual const char		*GetModDir() = 0;

	virtual bool			CopyFile( const char *pSource, const char *pDest ) = 0;

	virtual bool			LZSS_Compress( char *pDest, unsigned int *pDestLen, const char *pSource, unsigned int nSourceLen ) = 0;
	virtual bool			LZSS_Decompress( char *pDest, unsigned int *pDestLen, const char *pSource, unsigned int nSourceLen ) = 0;

	virtual bool			MD5_HashBuffer( unsigned char pDigest[16], const unsigned char *pBuffer, int nSize, unsigned int pSeed[4] ) = 0;

	// Server-specific
	[[nodiscard]] virtual IReplayServer	*GetReplayServer() = 0;
	[[nodiscard]] virtual IServer			*GetReplayServerAsIServer() = 0;
	[[nodiscard]] virtual IServer			*GetGameServer() = 0;
	virtual bool			GetSessionRecordBuffer( uint8 **ppSessionBuffer, int *pSize ) = 0;
	[[nodiscard]] virtual bool			IsDedicated() = 0;
	virtual void			ResetReplayRecordBuffer() = 0;
	[[nodiscard]] virtual demoheader_t	*GetReplayDemoHeader() = 0;
	virtual void			RecalculateTags() = 0;
	[[nodiscard]] virtual bool	NET_GetHostnameAsIP( const char *pHostname, char *pOut, intp nOutSize ) = 0;

	template<intp outSize>
	[[nodiscard]] bool NET_GetHostnameAsIP( const char *pHostname, char (&pOut)[outSize] )
	{
		return NET_GetHostnameAsIP( pHostname, pOut, outSize );
	}
};

//
// Allows the replay and client DLL's to talk to the engine
//
#if !defined( DEDICATED )
class IEngineClientReplay : public IBaseInterface
{
public:
	[[nodiscard]] virtual INetChannel		*GetNetChannel() = 0;
	[[nodiscard]] virtual bool			IsConnected() = 0;
	[[nodiscard]] virtual bool			IsListenServer() = 0;
	[[nodiscard]] virtual float			GetLastServerTickTime() = 0;
	[[nodiscard]] virtual const char		*GetLevelName() = 0;
	[[nodiscard]] virtual const char		*GetLevelNameShort() = 0;
	[[nodiscard]] virtual int				GetPlayerSlot() = 0;
	[[nodiscard]] virtual bool			IsPlayingReplayDemo() = 0;
	[[nodiscard]] virtual IClientEntityList	*GetClientEntityList() = 0;
	[[nodiscard]] virtual IClientReplay	*GetClientReplayInt() = 0;
	[[nodiscard]] virtual uint32			GetClientSteamID() = 0;
	virtual void			Con_NPrintf( int nPos, PRINTF_FORMAT_STRING const char *pFormat, ... ) = 0;
	[[nodiscard]] virtual CGlobalVarsBase	*GetClientGlobalVars() = 0;
	virtual void			VGui_PlaySound( const char *pSound ) = 0;
	virtual void			EngineVGui_ConfirmQuit() = 0;
	[[nodiscard]] virtual int			GetScreenWidth() = 0;
	[[nodiscard]] virtual int			GetScreenHeight() = 0;
	[[nodiscard]] virtual bool			IsDemoPlayingBack() = 0;
	[[nodiscard]] virtual bool			IsGamePathValidAndSafeForDownload( const char *pGamePath ) = 0;
	[[nodiscard]] virtual bool			IsInGame() = 0;

	virtual void			InitSoundRecord() = 0;

	virtual void			Wave_CreateTmpFile( const char *pFilename ) = 0;
	virtual void			Wave_AppendTmpFile( const char *pFilename, void *pBuffer, int nNumSamples ) = 0;
	virtual void			Wave_FixupTmpFile( const char *pFilename ) = 0;

};
#endif	// !defined( DEDICATED )

//----------------------------------------------------------------------------------------

#endif // IENGINEREPLAY_H