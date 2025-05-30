//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface to Xbox 360 system functions. Helps deal with the async system and Live
//			functions by either providing a handle for the caller to check results or handling
//			automatic cleanup of the async data when the caller doesn't care about the results.
//
//=====================================================================================//

#include "host.h"
#include "tier3/tier3.h"
#include "vgui/ILocalize.h"
#include "ixboxsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static wchar_t g_szModSaveContainerDisplayName[XCONTENT_MAX_DISPLAYNAME_LENGTH] = L"";
static char g_szModSaveContainerName[XCONTENT_MAX_FILENAME_LENGTH] = "";

//-----------------------------------------------------------------------------
// Implementation of IXboxSystem interface
//-----------------------------------------------------------------------------
class CXboxSystem : public IXboxSystem
{
public:
	CXboxSystem( void );

	virtual	~CXboxSystem( void );

	virtual AsyncHandle_t	CreateAsyncHandle( void );
	virtual void			ReleaseAsyncHandle( AsyncHandle_t handle );
	virtual int				GetOverlappedResult( AsyncHandle_t handle, uint *pResultCode, bool bWait );
	virtual void			CancelOverlappedOperation( AsyncHandle_t handle );

	// Save/Load
	virtual void			GetModSaveContainerNames( const char *pchModName, const wchar_t **ppchDisplayName, const char **ppchName );
	virtual uint			GetContainerRemainingSpace( void );
	virtual bool			DeviceCapacityAdequate( DWORD nDeviceID, const char *pModName );
	virtual DWORD			DiscoverUserData( DWORD nUserID, const char *pModName );

	// XUI
	virtual bool			ShowDeviceSelector( bool bForce, uint *pStorageID, AsyncHandle_t *pHandle );
	virtual void			ShowSigninUI( uint nPanes, uint nFlags );

	// Rich Presence and Matchmaking
	virtual int				UserSetContext( uint nUserIdx, uint nContextID, uint nContextValue, bool bAsync, AsyncHandle_t *pHandle);
	virtual int				UserSetProperty( uint nUserIndex, uint nPropertyId, uint nBytes, const void *pvValue, bool bAsync, AsyncHandle_t *pHandle );

	// Matchmaking
	virtual int				CreateSession( uint nFlags, uint nUserIdx, uint nMaxPublicSlots, uint nMaxPrivateSlots, uint64 *pNonce, void *pSessionInfo, XboxHandle_t *pSessionHandle, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual uint			DeleteSession( XboxHandle_t hSession, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL );
	virtual uint			SessionSearch( uint nProcedureIndex, uint nUserIndex, uint nNumResults, uint nNumUsers, uint nNumProperties, uint nNumContexts, XUSER_PROPERTY *pSearchProperties, XUSER_CONTEXT *pSearchContexts, uint *pcbResultsBuffer, XSESSION_SEARCHRESULT_HEADER *pSearchResults, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual uint			SessionStart( XboxHandle_t hSession, uint nFlags, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual uint			SessionEnd( XboxHandle_t hSession, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				SessionJoinLocal( XboxHandle_t hSession, uint nUserCount, const uint *pUserIndexes, const bool *pPrivateSlots, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				SessionJoinRemote( XboxHandle_t hSession, uint nUserCount, const XUID *pXuids, const bool *pPrivateSlot, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				SessionLeaveLocal( XboxHandle_t hSession, uint nUserCount, const uint *pUserIndexes, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				SessionLeaveRemote( XboxHandle_t hSession, uint nUserCount, const XUID *pXuids, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				SessionMigrate( XboxHandle_t hSession, uint nUserIndex, void *pSessionInfo, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual int				SessionArbitrationRegister( XboxHandle_t hSession, uint nFlags, uint64 nonce, uint *pBytes, void *pBuffer, bool bAsync, AsyncHandle_t *pAsyncHandle );

	// Stats
	virtual int				WriteStats( XboxHandle_t hSession, XUID xuid, uint nViews, void* pViews, bool bAsync, AsyncHandle_t *pAsyncHandle );

	// Achievements
	virtual int				EnumerateAchievements( uint nUserIdx, uint64 xuid, uint nStartingIdx, uint nCount, void *pBuffer, uint nBufferBytes, bool bAsync, AsyncHandle_t *pAsyncHandle );
	virtual void			AwardAchievement( uint nUserIdx, uint nAchievementId );
	
	virtual void			FinishContainerWrites( void );
	virtual uint			GetContainerOpenResult( void );
	virtual uint			OpenContainers( void );
	virtual void			CloseContainers( void );

private:
	virtual uint			CreateSavegameContainer( uint nCreationFlags );
	virtual uint			CreateUserSettingsContainer( uint nCreationFlags );

	uint					m_OpenContainerResult;
};

static CXboxSystem s_XboxSystem;
IXboxSystem *g_pXboxSystem = &s_XboxSystem;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CXboxSystem, IXboxSystem, XBOXSYSTEM_INTERFACE_VERSION, s_XboxSystem );

// Stubbed interface for win32
CXboxSystem::~CXboxSystem( void ) {}
CXboxSystem::CXboxSystem(void) : m_OpenContainerResult{0} {}

AsyncHandle_t	CXboxSystem::CreateAsyncHandle( void ) { return NULL; }
void			CXboxSystem::ReleaseAsyncHandle( AsyncHandle_t handle ) {}
int				CXboxSystem::GetOverlappedResult( AsyncHandle_t handle, uint *pResultCode, bool bWait ) { return 0; }
void			CXboxSystem::CancelOverlappedOperation( AsyncHandle_t handle ) {};
void			CXboxSystem::GetModSaveContainerNames( const char *pchModName, const wchar_t **ppchDisplayName, const char **ppchName ) 
{
	*ppchDisplayName = g_szModSaveContainerDisplayName;
	*ppchName = g_szModSaveContainerName;
}
DWORD			CXboxSystem::DiscoverUserData( DWORD nUserID, const char *pModName ) { return ((DWORD)-1); }
bool			CXboxSystem::DeviceCapacityAdequate( DWORD nDeviceID, const char *pModName ) { return true; }
uint			CXboxSystem::GetContainerRemainingSpace( void ) { return 0; }
bool			CXboxSystem::ShowDeviceSelector( bool bForce, uint *pStorageID, AsyncHandle_t *pHandle  ) { return false; }
void			CXboxSystem::ShowSigninUI( uint nPanes, uint nFlags ) {}
int				CXboxSystem::UserSetContext( uint nUserIdx, uint nContextID, uint nContextValue, bool bAsync, AsyncHandle_t *pHandle) { return 0; }
int				CXboxSystem::UserSetProperty( uint nUserIndex, uint nPropertyId, uint nBytes, const void *pvValue, bool bAsync, AsyncHandle_t *pHandle ) { return 0; }
int				CXboxSystem::CreateSession( uint nFlags, uint nUserIdx, uint nMaxPublicSlots, uint nMaxPrivateSlots, uint64 *pNonce, void *pSessionInfo, XboxHandle_t *pSessionHandle, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
uint			CXboxSystem::DeleteSession( XboxHandle_t hSession, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
uint			CXboxSystem::SessionSearch( uint nProcedureIndex, uint nUserIndex, uint nNumResults, uint nNumUsers, uint nNumProperties, uint nNumContexts, XUSER_PROPERTY *pSearchProperties, XUSER_CONTEXT *pSearchContexts, uint *pcbResultsBuffer, XSESSION_SEARCHRESULT_HEADER *pSearchResults, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
uint			CXboxSystem::SessionStart( XboxHandle_t hSession, uint nFlags, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; };
uint			CXboxSystem::SessionEnd( XboxHandle_t hSession, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; };
int				CXboxSystem::SessionJoinLocal( XboxHandle_t hSession, uint nUserCount, const uint *pUserIndexes, const bool *pPrivateSlots, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
int				CXboxSystem::SessionJoinRemote( XboxHandle_t hSession, uint nUserCount, const XUID *pXuids, const bool *pPrivateSlot, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
int				CXboxSystem::SessionLeaveLocal( XboxHandle_t hSession, uint nUserCount, const uint *pUserIndexes, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
int				CXboxSystem::SessionLeaveRemote( XboxHandle_t hSession, uint nUserCount, const XUID *pXuids, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
int				CXboxSystem::SessionMigrate( XboxHandle_t hSession, uint nUserIndex, void *pSessionInfo, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
int				CXboxSystem::SessionArbitrationRegister( XboxHandle_t hSession, uint nFlags, uint64 nonce, uint *pBytes, void *pBuffer, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
int				CXboxSystem::WriteStats( XboxHandle_t hSession, XUID xuid, uint nViews, void* pViews, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
int				CXboxSystem::EnumerateAchievements( uint nUserIdx, uint64 xuid, uint nStartingIdx, uint nCount, void *pBuffer, uint nBufferBytes, bool bAsync, AsyncHandle_t *pAsyncHandle ) { return 0; }
void			CXboxSystem::AwardAchievement( uint nUserIdx, uint nAchievementId ) {}
void			CXboxSystem::FinishContainerWrites( void ) {}
uint			CXboxSystem::GetContainerOpenResult( void ) { return 0; }
uint			CXboxSystem::OpenContainers( void ) { return 0; }
void			CXboxSystem::CloseContainers( void ) {}
uint			CXboxSystem::CreateSavegameContainer( uint nCreationFlags ) { return 0; }
uint			CXboxSystem::CreateUserSettingsContainer( uint nCreationFlags ) { return 0; }
