//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds the CGCClient class
//
//=============================================================================

#ifndef GCCLIENT_H
#define GCCLIENT_H
#ifdef _WIN32
#pragma once
#endif

#include "steam/steam_api.h"
#include "jobmgr.h"
#include "sharedobject.h"

class ISteamGameCoordinator;
struct GCMessageAvailable_t;
class CTestEvent;

namespace GCSDK
{


//-----------------------------------------------------------------------------
// Purpose: base for CGCMsgHandler
//			used only by CGCMsgHandler, shouldn't be used directly
//-----------------------------------------------------------------------------
class CGCClient
{
public:
	CGCClient( ISteamGameCoordinator *pSteamGameCoordinator = NULL, bool bGameserver = false );
	virtual ~CGCClient( );

	bool BInit( ISteamGameCoordinator *pSteamGameCoordinator );
	void Uninit( );
	bool BMainLoop( uint64 ulLimitMicroseconds, uint64 ulFrameTimeMicroseconds = 0 );

	CJobMgr &GetJobMgr() { return m_JobMgr; }

	bool BSendMessage( uint32 unMsgType, const uint8 *pubData, uint32 cubData );
	bool BSendMessage( const CGCMsgBase& msg );
	bool BSendMessage( const CProtoBufMsgBase& msg );

	/// Locate a given shared object from the cache
	CSharedObject *FindSharedObject( const CSteamID & ownerID, const CSharedObject & soIndex );

	/// Find a shared object cache for the specified user.  Optionally, the cache will be
	/// created if it doesn't not currently exist.
	CGCClientSharedObjectCache *FindSOCache( const CSteamID & steamID, bool bCreateIfMissing = true );

	/// Adds a listener to the shared object cache for the specified Steam ID.
	///
	/// @see CGCClientSharedObjectCache::AddListener
	void AddSOCacheListener( const CSteamID &ownerID, ISharedObjectListener *pListener );

	/// Removes a listener for the shared object cache for the specified Steam ID.
	/// Returns true if we were listening and were successfully removed, false
	/// otherwise
	///
	/// @see CGCClientSharedObjectCache::RemoveListener
	bool RemoveSOCacheListener( const CSteamID &ownerID, ISharedObjectListener *pListener );

	void OnGCMessageAvailable( GCMessageAvailable_t *pCallback );
	ISteamGameCoordinator *GetSteamGameCoordinator() { return m_pSteamGameCoordinator; }

	virtual void Test_AddEvent( CTestEvent * )	{}
	virtual void Test_CacheSubscribed( const CSteamID & ) {}

	void NotifySOCacheUnsubscribed( const CSteamID & ownerID );

	void Dump();

#ifdef DBGFLAG_VALIDATE
	static void ValidateStatics( CValidator &validator );
#endif
protected:

	ISteamGameCoordinator *m_pSteamGameCoordinator;
	CUtlMemory<uint8> m_memMsg;

	// local job handling
	CJobMgr m_JobMgr;

	// Shared object caches
	CUtlMap<CSteamID, CGCClientSharedObjectCache *> m_mapSOCache;

	// Steam callback for getting notified about messages available. Not part of the class
	// in Steam builds because we use the TestClientManager instead of steam_api.dll in Steam 
#ifndef STEAM
	CCallback< CGCClient, GCMessageAvailable_t, false > m_callbackGCMessageAvailable;
#endif

};


} // namespace GCSDK

#endif // GCCLIENT_H