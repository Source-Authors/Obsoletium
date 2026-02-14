//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#include "datacache/idatacache.h"

#ifdef _LINUX
#include <malloc.h>
#endif

#include "tier0/vprof.h"
#include "tier0/basetypes.h"
#include "tier1/convar.h"
#include "tier1/interface.h"
#include "tier1/datamanager.h"
#include "tier1/utlrbtree.h"
#include "tier1/utlhash.h"
#include "tier1/utlmap.h"
#include "tier1/generichash.h"
#include "tier1/utlvector.h"
#include "tier1/fmtstr.h"
#include "filesystem.h"
#include "datacache.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
CDataCache g_DataCache;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CDataCache, IDataCache, DATACACHE_INTERFACE_VERSION, g_DataCache );


//-----------------------------------------------------------------------------
//
// Data Cache class implemenations
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Console commands
//-----------------------------------------------------------------------------
ConVar developer( "developer", "0", FCVAR_INTERNAL_USE );
static ConVar mem_force_flush( "mem_force_flush", "0", FCVAR_CHEAT, "Force cache flush of unlocked resources on every alloc" );
static std::atomic_int g_iDontForceFlush;

//-----------------------------------------------------------------------------
// DataCacheItem_t
//-----------------------------------------------------------------------------

DEFINE_FIXEDSIZE_ALLOCATOR_MT( DataCacheItem_t, 4096/sizeof(DataCacheItem_t), CUtlMemoryPool::GROW_SLOW );

void DataCacheItem_t::DestroyResource()
{ 
	if ( pSection )
	{
		pSection->DiscardItemData( this, DC_AGE_DISCARD );
	}
	delete this; 
}


//-----------------------------------------------------------------------------
// CDataCacheSection
//-----------------------------------------------------------------------------

CDataCacheSection::CDataCacheSection( CDataCache *pSharedCache, IDataCacheClient *pClient, const char *pszName )
  :	m_nFrameUnlockCounter( 0 ),
	m_LRU( pSharedCache->m_LRU ),
	m_pClient( pClient ),
	m_options( 0 ),
	m_pSharedCache( pSharedCache ),
	m_mutex( pSharedCache->m_mutex )
{
	BitwiseClear( m_status );
	AssertMsg1( strlen(pszName) <= DC_MAX_CLIENT_NAME, "Cache client name too long \"%s\"", pszName );
	V_strcpy_safe( szName, pszName );

	for ( int i = 0; i < DC_MAX_THREADS_FRAMELOCKED; i++ )
	{
		FrameLock_t *pFrameLock = new FrameLock_t;
		pFrameLock->m_iThread = i;
		m_FreeFrameLocks.Push( pFrameLock );
	}
}

CDataCacheSection::~CDataCacheSection()
{
	FrameLock_t *pFrameLock;
	while ( ( pFrameLock = m_FreeFrameLocks.Pop() ) != NULL )
	{
		delete pFrameLock;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Controls cache size.
//-----------------------------------------------------------------------------
void CDataCacheSection::SetLimits( const DataCacheLimits_t &limits )
{
	m_limits = limits;
	AssertMsg( m_limits.nMinBytes == 0 && m_limits.nMinItems == 0, "Cache minimums not yet implemented" );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const DataCacheLimits_t &CDataCacheSection::GetLimits()
{
	return m_limits;
}


//-----------------------------------------------------------------------------
// Purpose: Controls cache options.
//-----------------------------------------------------------------------------
void CDataCacheSection::SetOptions( unsigned options )
{
	m_options = options;
}


//-----------------------------------------------------------------------------
// Purpose: Get the current state of the section
//-----------------------------------------------------------------------------
void CDataCacheSection::GetStatus( DataCacheStatus_t *pStatus, DataCacheLimits_t *pLimits )
{
	if ( pStatus )
	{
		*pStatus = m_status;
	}

	if ( pLimits )
	{
		*pLimits = m_limits;
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CDataCacheSection::EnsureCapacity( size_t nBytes, size_t nItems )
{
	VPROF( "CDataCacheSection::EnsureCapacity" );

	if ( m_limits.nMaxItems != std::numeric_limits<size_t>::max() ||
		 m_limits.nMaxBytes != std::numeric_limits<size_t>::max() )
	{
		size_t nNewSectionBytes = GetNumBytes() + nBytes;

		if ( nNewSectionBytes > m_limits.nMaxBytes )
		{
			Purge( nNewSectionBytes - m_limits.nMaxBytes );
		}

		size_t nNewItems = GetNumItems() + nItems;

		if ( nNewItems > m_limits.nMaxItems )
		{
			PurgeItems( nNewItems - m_limits.nMaxItems );
		}
	}

	m_pSharedCache->EnsureCapacity( nBytes );
}

//-----------------------------------------------------------------------------
// Purpose: Add an item to the cache.  Purges old items if over budget, returns false if item was already in cache.
//-----------------------------------------------------------------------------
bool CDataCacheSection::Add( DataCacheClientID_t clientId, const void *pItemData, size_t size, DataCacheHandle_t *pHandle )
{
	return AddEx( clientId, pItemData, size, DCAF_DEFAULT, pHandle );
}

//-----------------------------------------------------------------------------
// Purpose: Add an item to the cache.  Purges old items if over budget, returns false if item was already in cache.
//-----------------------------------------------------------------------------
bool CDataCacheSection::AddEx( DataCacheClientID_t clientId, const void *pItemData, size_t size, unsigned flags, DataCacheHandle_t *pHandle )
{
	VPROF( "CDataCacheSection::Add" );

	if ( mem_force_flush.GetBool() )
	{
		m_pSharedCache->Flush();
	}

	if ( ( m_options & DC_VALIDATE ) && Find( clientId ) )
	{
		Error( "Duplicate add to data cache %zu\n", clientId );
		return false;
	}

	EnsureCapacity( size );

	DataCacheItemData_t itemData = 
	{
		pItemData,
		size,
		clientId,
		this
	};

	memhandle_t hMem = m_LRU.CreateResource( itemData, true );

	Assert( hMem != 0 && hMem != DC_INVALID_HANDLE );

	AccessItem( hMem )->hLRU = hMem;

	if ( pHandle )
	{
		*pHandle = hMem;
	}

	NoteAdd( size );
	// dimhotepus: resource created already locked (true).
	NoteLock( size );

	OnAdd( clientId, hMem );

	g_iDontForceFlush.fetch_add(1, std::memory_order::memory_order_relaxed);

	if ( flags & DCAF_LOCK )
	{
		Lock( hMem );
	}
	// Add implies a frame lock. A no-op if not in frame lock
	FrameLock( hMem );

	g_iDontForceFlush.fetch_sub(1, std::memory_order::memory_order_relaxed);

	// dimhotepus: unlock locked resource if needed and update status.
	Unlock( hMem );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Finds an item in the cache, returns NULL if item is not in cache.
//-----------------------------------------------------------------------------
DataCacheHandle_t CDataCacheSection::Find( DataCacheClientID_t clientId )
{
	VPROF( "CDataCacheSection::Find" );

	m_status.nFindRequests++;

	DataCacheHandle_t hResult = DoFind( clientId );

	if ( hResult != DC_INVALID_HANDLE )
	{
		m_status.nFindHits++;
	}

	return hResult;
}

//---------------------------------------------------------
DataCacheHandle_t CDataCacheSection::DoFind( DataCacheClientID_t clientId )
{
	AUTO_LOCK( m_mutex );
	memhandle_t hCurrent = GetFirstUnlockedItem();

	while ( hCurrent != INVALID_MEMHANDLE )
	{
		if ( AccessItem( hCurrent )->clientId == clientId )
		{
			m_status.nFindHits++;
			return hCurrent;
		}
		hCurrent = GetNextItem( hCurrent );
	}

	hCurrent = GetFirstLockedItem();

	while ( hCurrent != INVALID_MEMHANDLE )
	{
		if ( AccessItem( hCurrent )->clientId == clientId )
		{
			m_status.nFindHits++;
			return hCurrent;
		}
		hCurrent = GetNextItem( hCurrent );
	}

	return DC_INVALID_HANDLE;
}


//-----------------------------------------------------------------------------
// Purpose: Get an item out of the cache and remove it. No callbacks are executed.
//-----------------------------------------------------------------------------
DataCacheRemoveResult_t CDataCacheSection::Remove( DataCacheHandle_t handle, const void **ppItemData, size_t *pItemSize, bool bNotify )
{
	VPROF( "CDataCacheSection::Remove" );

	if ( handle != DC_INVALID_HANDLE )
	{
		memhandle_t lruHandle = handle;
		if ( m_LRU.LockCount( lruHandle ) > 0 )
		{
			return DC_LOCKED;
		}

		AUTO_LOCK( m_mutex );

		DataCacheItem_t *pItem = AccessItem( lruHandle );
		if ( pItem )
		{
			if ( ppItemData )
			{
				*ppItemData = pItem->pItemData;
			}

			if ( pItemSize )
			{
				*pItemSize = pItem->size;
			}

			DiscardItem( lruHandle, ( bNotify ) ? DC_REMOVED : DC_NONE );

			return DC_OK;
		}
	}

	return DC_NOT_FOUND;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CDataCacheSection::IsPresent( DataCacheHandle_t handle )
{
	return ( m_LRU.GetResource_NoLockNoLRUTouch( handle ) != NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Lock an item in the cache, returns NULL if item is not in the cache.
//-----------------------------------------------------------------------------
void *CDataCacheSection::Lock( DataCacheHandle_t handle )
{
	VPROF( "CDataCacheSection::Lock" );

	if ( mem_force_flush.GetBool() && !g_iDontForceFlush.load(std::memory_order::memory_order_relaxed))
		Flush();

	if ( handle != DC_INVALID_HANDLE )
	{
		DataCacheItem_t *pItem = m_LRU.LockResource( handle );
		if ( pItem )
		{
			if ( m_LRU.LockCount( handle ) == 1 )
			{
				NoteLock( pItem->size );
			}
			return const_cast<void *>(pItem->pItemData);
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Unlock a previous lock.
//-----------------------------------------------------------------------------
int CDataCacheSection::Unlock( DataCacheHandle_t handle )
{
	VPROF( "CDataCacheSection::Unlock" );

	int iNewLockCount = 0;
	if ( handle != DC_INVALID_HANDLE )
	{
		AssertMsg( AccessItem( handle ) != nullptr, "Attempted to unlock nonexistent cache entry" );
		size_t nBytesUnlocked = 0;

		{
			AUTO_LOCK(m_mutex);
			iNewLockCount = m_LRU.UnlockResource( handle );
			if ( iNewLockCount == 0 )
			{
				nBytesUnlocked = AccessItem( handle )->size;
			}
		}

		if ( nBytesUnlocked )
		{
			NoteUnlock( nBytesUnlocked );
			EnsureCapacity( 0 );
		}
	}
	return iNewLockCount;
}


//-----------------------------------------------------------------------------
// Purpose: Lock the mutex
//-----------------------------------------------------------------------------
void CDataCacheSection::LockMutex()
{
	m_mutex.Lock();
	g_iDontForceFlush.fetch_add(1, std::memory_order::memory_order_relaxed);
}


//-----------------------------------------------------------------------------
// Purpose: Unlock the mutex
//-----------------------------------------------------------------------------
void CDataCacheSection::UnlockMutex()
{
	g_iDontForceFlush.fetch_sub(1, std::memory_order::memory_order_relaxed);
	m_mutex.Unlock();
}

//-----------------------------------------------------------------------------
// Purpose: Get without locking
//-----------------------------------------------------------------------------
void *CDataCacheSection::Get( DataCacheHandle_t handle, bool bFrameLock )
{
	VPROF( "CDataCacheSection::Get" );

	if ( mem_force_flush.GetBool() && !g_iDontForceFlush.load(std::memory_order::memory_order_relaxed))
		Flush();

	if ( handle != DC_INVALID_HANDLE )
	{
		if ( bFrameLock && IsFrameLocking() )
			return FrameLock( handle );

		AUTO_LOCK( m_mutex );
		DataCacheItem_t *pItem = m_LRU.GetResource_NoLock( handle );
		if ( pItem )
		{
			return const_cast<void *>( pItem->pItemData );
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Get without locking
//-----------------------------------------------------------------------------
void *CDataCacheSection::GetNoTouch( DataCacheHandle_t handle, bool bFrameLock )
{
	VPROF( "CDataCacheSection::GetNoTouch" );

	if ( handle != DC_INVALID_HANDLE )
	{
		if ( bFrameLock && IsFrameLocking() )
			return FrameLock( handle );

		AUTO_LOCK( m_mutex );
		DataCacheItem_t *pItem = m_LRU.GetResource_NoLockNoLRUTouch( handle );
		if ( pItem )
		{
			return const_cast<void *>( pItem->pItemData );
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: "Frame locking" (not game frame). A crude way to manage locks over relatively 
//			short periods. Does not affect normal locks/unlocks
//-----------------------------------------------------------------------------
int CDataCacheSection::BeginFrameLocking()
{
	FrameLock_t *pFrameLock = m_ThreadFrameLock.Get();
	if ( pFrameLock )
	{
		pFrameLock->m_iLock++;
	}
	else
	{
		while ( ( pFrameLock = m_FreeFrameLocks.Pop() ) == NULL )
		{
			ThreadPause();
			ThreadSleep( 1 );
		}
		pFrameLock->m_iLock = 1;
		pFrameLock->m_pFirst = NULL;
		m_ThreadFrameLock.Set( pFrameLock );
	}
	return pFrameLock->m_iLock;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CDataCacheSection::IsFrameLocking()
{
	FrameLock_t *pFrameLock = m_ThreadFrameLock.Get();
	return ( pFrameLock != NULL );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void *CDataCacheSection::FrameLock( DataCacheHandle_t handle )
{
	VPROF( "CDataCacheSection::FrameLock" );

	if ( mem_force_flush.GetBool() && !g_iDontForceFlush.load(std::memory_order::memory_order_relaxed))
		Flush();

	void *pResult = NULL;
	FrameLock_t *pFrameLock = m_ThreadFrameLock.Get();
	if ( pFrameLock )
	{
		DataCacheItem_t *pItem = m_LRU.LockResource( handle );
		if ( pItem )
		{
			int iThread = pFrameLock->m_iThread;
			if ( pItem->pNextFrameLocked[iThread] == DC_NO_NEXT_LOCKED )
			{
				// dimhotepus: Fix status by incrementing locked on first time lock.
				// Lock below increments status stats on first lock (count = 1), but
				// LockResource above already incremented lock counter and second
				// LockResource inside Lock will increment it further (count = 2) 
				// even for first time lock.
				if ( m_LRU.LockCount( handle ) == 1 )
				{
					NoteLock( pItem->size );
				}

				pItem->pNextFrameLocked[iThread] = pFrameLock->m_pFirst;
				pFrameLock->m_pFirst = pItem;
				Lock( handle );
			}

			pResult = const_cast<void *>(pItem->pItemData);
			m_LRU.UnlockResource( handle );
		}
	}

	return pResult;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CDataCacheSection::EndFrameLocking()
{
	FrameLock_t *pFrameLock = m_ThreadFrameLock.Get();
	Assert( pFrameLock && pFrameLock->m_iLock > 0 );

	if ( pFrameLock->m_iLock == 1 )
	{
		VPROF( "CDataCacheSection::EndFrameLocking" );

		DataCacheItem_t *pItem = pFrameLock->m_pFirst;
		DataCacheItem_t *pNext;
		int iThread = pFrameLock->m_iThread;
		while ( pItem )
		{
			pNext = pItem->pNextFrameLocked[iThread];
			pItem->pNextFrameLocked[iThread] = DC_NO_NEXT_LOCKED;
			Unlock( pItem->hLRU );
			pItem = pNext;
		}

		m_FreeFrameLocks.Push( pFrameLock );
		m_ThreadFrameLock.Set( NULL );
		return 0;
	}
	else
	{
		pFrameLock->m_iLock--;
	}
	return pFrameLock->m_iLock;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int *CDataCacheSection::GetFrameUnlockCounterPtr()
{
	return &m_nFrameUnlockCounter;
}


//-----------------------------------------------------------------------------
// Purpose: Lock management, not for the feint of heart
//-----------------------------------------------------------------------------
int CDataCacheSection::GetLockCount( DataCacheHandle_t handle )
{
	return m_LRU.LockCount( handle );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CDataCacheSection::BreakLock( DataCacheHandle_t handle )
{
	return m_LRU.BreakLock( handle );
}


//-----------------------------------------------------------------------------
// Purpose: Explicitly mark an item as "recently used"
//-----------------------------------------------------------------------------
bool CDataCacheSection::Touch( DataCacheHandle_t handle )
{
	m_LRU.TouchResource( handle );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Explicitly mark an item as "least recently used". 
//-----------------------------------------------------------------------------
bool CDataCacheSection::Age( DataCacheHandle_t handle )
{
	m_LRU.MarkAsStale( handle );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Empty the cache. Returns bytes released, will remove locked items if force specified
//-----------------------------------------------------------------------------
size_t CDataCacheSection::Flush( bool bUnlockedOnly, bool bNotify )
{
	VPROF( "CDataCacheSection::Flush" );

	AUTO_LOCK( m_mutex );

	DataCacheNotificationType_t notificationType = ( bNotify )? DC_FLUSH_DISCARD : DC_NONE;

	size_t nBytesFlushed = 0;
	size_t nBytesCurrent = 0;

	memhandle_t hCurrent = GetFirstUnlockedItem();

	while ( hCurrent != INVALID_MEMHANDLE )
	{
		memhandle_t hNext = GetNextItem( hCurrent );
		nBytesCurrent = AccessItem( hCurrent )->size;

		if ( DiscardItem( hCurrent, notificationType ) )
		{
			nBytesFlushed += nBytesCurrent;
		}
		hCurrent = hNext;
	}

	if ( !bUnlockedOnly )
	{
		hCurrent = GetFirstLockedItem();

		while ( hCurrent != INVALID_MEMHANDLE )
		{
			memhandle_t hNext = GetNextItem( hCurrent );
			nBytesCurrent = AccessItem( hCurrent )->size;

			if ( DiscardItem( hCurrent, notificationType ) )
			{
				nBytesFlushed += nBytesCurrent;
			}
			hCurrent = hNext;
		}
	}

	return nBytesFlushed;
}


//-----------------------------------------------------------------------------
// Purpose: Dump the oldest items to free the specified amount of memory. Returns amount actually freed
//-----------------------------------------------------------------------------
size_t CDataCacheSection::Purge( size_t nBytes )
{
	VPROF( "CDataCacheSection::Purge" );

	AUTO_LOCK( m_mutex );

	size_t nBytesPurged = 0;
	size_t nBytesCurrent = 0;

	memhandle_t hCurrent = GetFirstUnlockedItem();
	while ( hCurrent != INVALID_MEMHANDLE && nBytes > 0 )
	{
		memhandle_t hNext = GetNextItem( hCurrent );
		nBytesCurrent = AccessItem( hCurrent )->size;

		if ( DiscardItem( hCurrent, DC_FLUSH_DISCARD  ) )
		{
			nBytesPurged += nBytesCurrent;
			nBytes -= min( nBytesCurrent, nBytes );
		}
		hCurrent = hNext;
	}

	return nBytesPurged;
}

//-----------------------------------------------------------------------------
// Purpose: Dump the oldest items to free the specified number of items. Returns number actually freed
//-----------------------------------------------------------------------------
size_t CDataCacheSection::PurgeItems( size_t nItems )
{
	AUTO_LOCK( m_mutex );

	size_t nPurged = 0;

	memhandle_t hCurrent = GetFirstUnlockedItem();
	while ( hCurrent != INVALID_MEMHANDLE && nItems )
	{
		memhandle_t hNext = GetNextItem( hCurrent );

		if ( DiscardItem( hCurrent, DC_FLUSH_DISCARD ) )
		{
			nItems--;
			nPurged++;
		}
		hCurrent = hNext;
	}

	return nPurged;
}


//-----------------------------------------------------------------------------
// Purpose: Output the state of the section
//-----------------------------------------------------------------------------
void CDataCacheSection::OutputReport( DataCacheReportType_t reportType )
{
	m_pSharedCache->OutputReport( reportType, GetName() );
}

//-----------------------------------------------------------------------------
// Purpose: Updates the size of a specific item
// Input  : handle - 
//			newSize - 
//-----------------------------------------------------------------------------
void CDataCacheSection::UpdateSize( DataCacheHandle_t handle, size_t nNewSize )
{
	DataCacheItem_t *pItem = m_LRU.LockResource( handle );
	if ( !pItem )
	{
		// If it's gone from memory, size is already irrelevant
		return;
	}

	size_t oldSize = pItem->size;
	
	if ( oldSize != nNewSize )
	{
		// Update the size
		pItem->size = nNewSize;

		// If change would grow cache size, then purge items until we have room
		if ( nNewSize > oldSize )
		{
			m_pSharedCache->EnsureCapacity( nNewSize - oldSize );
		}
		
		m_LRU.NotifySizeChanged( handle, oldSize, nNewSize );
		NoteSizeChanged( oldSize, nNewSize );
	}

	m_LRU.UnlockResource( handle );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
memhandle_t CDataCacheSection::GetFirstUnlockedItem()
{
	memhandle_t hCurrent = m_LRU.GetFirstUnlocked();

	while ( hCurrent != INVALID_MEMHANDLE )
	{
		if ( AccessItem( hCurrent )->pSection == this )
		{
			return hCurrent;
		}
		hCurrent = m_LRU.GetNext( hCurrent );
	}
	return INVALID_MEMHANDLE;
}


memhandle_t CDataCacheSection::GetFirstLockedItem()
{
	memhandle_t hCurrent = m_LRU.GetFirstLocked();

	while ( hCurrent != INVALID_MEMHANDLE )
	{
		if ( AccessItem( hCurrent )->pSection == this )
		{
			return hCurrent;
		}
		hCurrent = m_LRU.GetNext( hCurrent );
	}
	return INVALID_MEMHANDLE;
}


memhandle_t CDataCacheSection::GetNextItem( memhandle_t hCurrent )
{
	hCurrent = m_LRU.GetNext( hCurrent );

	while ( hCurrent != INVALID_MEMHANDLE )
	{
		if ( AccessItem( hCurrent )->pSection == this )
		{
			return hCurrent;
		}
		hCurrent = m_LRU.GetNext( hCurrent );
	}
	return INVALID_MEMHANDLE;
}

bool CDataCacheSection::DiscardItem( memhandle_t hItem, DataCacheNotificationType_t type )
{
	DataCacheItem_t *pItem = AccessItem( hItem );
	if ( DiscardItemData( pItem, type ) )
	{
		if ( m_LRU.LockCount( hItem ) )
		{
			m_LRU.BreakLock( hItem );
			NoteUnlock( pItem->size );
		}

		FrameLock_t *pFrameLock = m_ThreadFrameLock.Get();
		if ( pFrameLock )
		{
			int iThread = pFrameLock->m_iThread;
			if ( pItem->pNextFrameLocked[iThread] != DC_NO_NEXT_LOCKED )
			{
				if ( pFrameLock->m_pFirst == pItem )
				{
					pFrameLock->m_pFirst = pItem->pNextFrameLocked[iThread];
				}
				else
				{
					DataCacheItem_t *pCurrent = pFrameLock->m_pFirst;
					while ( pCurrent )
					{
						if ( pCurrent->pNextFrameLocked[iThread] == pItem )
						{
							pCurrent->pNextFrameLocked[iThread] = pItem->pNextFrameLocked[iThread];
							break;
						}
						pCurrent = pCurrent->pNextFrameLocked[iThread];
					}
				}
				pItem->pNextFrameLocked[iThread] = DC_NO_NEXT_LOCKED;
			}

		}

#ifdef _DEBUG
		for ( auto *c : pItem->pNextFrameLocked )
		{
			if ( c != DC_NO_NEXT_LOCKED )
			{
				DebuggerBreak(); // higher level code needs to handle better
			}
		}
#endif

		pItem->pSection = NULL; // inhibit callbacks from lower level resource system
		m_LRU.DestroyResource( hItem );
		return true;
	}
	return false;
}

bool CDataCacheSection::DiscardItemData( DataCacheItem_t *pItem, DataCacheNotificationType_t type )
{
	if ( pItem )
	{
		if ( type != DC_NONE )
		{
			Assert( type == DC_AGE_DISCARD || type == DC_FLUSH_DISCARD || type == DC_REMOVED );

			if ( type == DC_AGE_DISCARD && m_pSharedCache->IsInFlush() )
				type = DC_FLUSH_DISCARD;

			DataCacheNotification_t notification =
			{
				type,
				GetName(),
				pItem->clientId,
				pItem->pItemData,
				pItem->size
			};

			bool bResult = m_pClient->HandleCacheNotification( notification );
			AssertMsg( bResult, "Refusal of cache drop not yet implemented!" );

			if ( bResult )
			{
				NoteRemove( pItem->size );
			}

			return bResult;
		}

		OnRemove( pItem->clientId );

		pItem->pSection = NULL;
		pItem->pItemData = NULL;
		pItem->clientId = 0;

		NoteRemove( pItem->size );

		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// CDataCacheSectionFastFind
//-----------------------------------------------------------------------------
DataCacheHandle_t CDataCacheSectionFastFind::DoFind( DataCacheClientID_t clientId ) 
{ 
	AUTO_LOCK( m_mutex );
	UtlHashFastHandle_t hHash = m_Handles.Find( Hash4( &clientId ) );
	if( hHash != m_Handles.InvalidHandle() )
		return m_Handles[hHash];
	return DC_INVALID_HANDLE; 
}


void CDataCacheSectionFastFind::OnAdd( DataCacheClientID_t clientId, DataCacheHandle_t hCacheItem ) 
{
	AUTO_LOCK( m_mutex );
	Assert( m_Handles.Find( Hash4( &clientId ) ) == m_Handles.InvalidHandle());
	m_Handles.FastInsert( Hash4( &clientId ), hCacheItem );
}


void CDataCacheSectionFastFind::OnRemove( DataCacheClientID_t clientId ) 
{
	AUTO_LOCK( m_mutex );
	UtlHashFastHandle_t hHash = m_Handles.Find( Hash4( &clientId ) );
	Assert( hHash != m_Handles.InvalidHandle());
	if( hHash != m_Handles.InvalidHandle() )
		return m_Handles.Remove( hHash );
}


//-----------------------------------------------------------------------------
// CDataCache
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Convar callback to change data cache 
//-----------------------------------------------------------------------------
void DataCacheSize_f( IConVar *pConVar, [[maybe_unused]] const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	int nOldValue = (int)flOldValue;
	if ( var.GetInt() != nOldValue )
	{
		g_DataCache.SetSize( var.GetInt() * static_cast<size_t>(1024) * 1024 );
	}
}
// dimhotepus: Bump 64 -> 96 as Hammer with many models needs this.
ConVar datacachesize( "datacachesize", "96", FCVAR_INTERNAL_USE, "Data cache size in MiB.", true, 32, true, 512, DataCacheSize_f );

//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CDataCache::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;

	g_DataCache.SetSize( datacachesize.GetInt() * static_cast<size_t>(1024) * 1024 );
	g_pDataCache = this;

	return true;
}

void CDataCache::Disconnect()
{
	g_pDataCache = NULL;
	BaseClass::Disconnect();
}


//-----------------------------------------------------------------------------
// Init, Shutdown
//-----------------------------------------------------------------------------
InitReturnVal_t CDataCache::Init( void )
{
	return BaseClass::Init();
}

void CDataCache::Shutdown( void )
{
	Flush( false, false );
	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Query interface
//-----------------------------------------------------------------------------
void *CDataCache::QueryInterface( const char *pInterfaceName )
{
	// Loading the datacache DLL mounts *all* interfaces
	// This includes the backward-compatible interfaces + IStudioDataCache
	return Sys_GetFactoryThis()( pInterfaceName, nullptr );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CDataCache::CDataCache()
	: m_mutex( m_LRU.AccessMutex() )
{
	BitwiseClear( m_status );
	m_bInFlush = false;
}

//-----------------------------------------------------------------------------
// Purpose: Controls cache size.
//-----------------------------------------------------------------------------
void CDataCache::SetSize( size_t nMaxBytes )
{
	m_LRU.SetTargetSize( nMaxBytes );
	m_LRU.FlushToTargetSize();

	nMaxBytes /= static_cast<size_t>(1024) * 1024;

	// dimhotepus: Fix warning about size_t <-> int mismatch.
	if ( static_cast<size_t>(datacachesize.GetInt()) != nMaxBytes )
	{
		datacachesize.SetValue( static_cast<int>(nMaxBytes) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Controls cache options.
//-----------------------------------------------------------------------------
void CDataCache::SetOptions( unsigned options )
{
	for ( auto &s : m_Sections )
	{
		s->SetOptions( options );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Controls cache section size.
//-----------------------------------------------------------------------------
void CDataCache::SetSectionLimits( const char *pszSectionName, const DataCacheLimits_t &limits )
{
	IDataCacheSection *pSection = FindSection( pszSectionName );

	if ( !pSection )
	{
		DevMsg( "Cannot find requested cache section \"%s\"", pszSectionName );
		return;
	}

	pSection->SetLimits( limits );
}


//-----------------------------------------------------------------------------
// Purpose: Get the current state of the cache
//-----------------------------------------------------------------------------
void CDataCache::GetStatus( DataCacheStatus_t *pStatus, DataCacheLimits_t *pLimits )
{
	if ( pStatus )
	{
		*pStatus = m_status;
	}

	if ( pLimits )
	{
		Construct( pLimits );
		pLimits->nMaxBytes = m_LRU.TargetSize();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Add a section to the cache
//-----------------------------------------------------------------------------
IDataCacheSection *CDataCache::AddSection( IDataCacheClient *pClient, const char *pszSectionName, const DataCacheLimits_t &limits, bool bSupportFastFind )
{
	CDataCacheSection *pSection = (CDataCacheSection *)FindSection( pszSectionName );
	if ( pSection )
	{
		AssertMsg1( pSection->GetClient() == pClient, "Duplicate cache section name \"%s\"", pszSectionName );
		return pSection;
	}

	if ( !bSupportFastFind )
		pSection = new CDataCacheSection( this, pClient, pszSectionName );
	else
		pSection = new CDataCacheSectionFastFind( this, pClient, pszSectionName );

	pSection->SetLimits( limits );

	m_Sections.AddToTail( pSection );
	return pSection;
}


//-----------------------------------------------------------------------------
// Purpose: Remove a section from the cache
//-----------------------------------------------------------------------------
void CDataCache::RemoveSection( const char *pszClientName, bool bCallFlush )
{
	intp iSection = FindSectionIndex( pszClientName );

	if ( iSection != m_Sections.InvalidIndex() )
	{
		if ( bCallFlush )
		{
			m_Sections[iSection]->Flush( false );
		}
		delete m_Sections[iSection];
		m_Sections.FastRemove( iSection );
		return;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Find a section of the cache
//-----------------------------------------------------------------------------
IDataCacheSection *CDataCache::FindSection( const char *pszClientName )
{
	intp iSection = FindSectionIndex( pszClientName );

	if ( iSection != m_Sections.InvalidIndex() )
	{
		return m_Sections[iSection];
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CDataCache::EnsureCapacity( size_t nBytes )
{
	VPROF( "CDataCache::EnsureCapacity" );

	m_LRU.EnsureCapacity( nBytes );
}


//-----------------------------------------------------------------------------
// Purpose: Dump the oldest items to free the specified amount of memory. Returns amount actually freed
//-----------------------------------------------------------------------------
size_t CDataCache::Purge( size_t nBytes )
{
	VPROF( "CDataCache::Purge" );

	return m_LRU.Purge( nBytes );
}


//-----------------------------------------------------------------------------
// Purpose: Empty the cache. Returns bytes released, will remove locked items if force specified
//-----------------------------------------------------------------------------
size_t CDataCache::Flush( bool bUnlockedOnly, [[maybe_unused]] bool bNotify )
{
	VPROF( "CDataCache::Flush" );

	if ( m_bInFlush )
	{
		return 0;
	}

	m_bInFlush = true;

	size_t result = bUnlockedOnly ? m_LRU.FlushAllUnlocked() : m_LRU.FlushAll();

	m_bInFlush = false;

	return result;
}

//-----------------------------------------------------------------------------
// Purpose: Output the state of the cache
//-----------------------------------------------------------------------------
void CDataCache::OutputReport( DataCacheReportType_t reportType, const char *pszSection )
{
	AUTO_LOCK( m_mutex );
	size_t bytesUsed = m_LRU.UsedSize();
	size_t bytesTotal = m_LRU.TargetSize();

	float percent = 100.0f * (float)bytesUsed / (float)bytesTotal;

	CUtlVector<memhandle_t> lruList, lockedlist;

	m_LRU.GetLockHandleList( lockedlist );
	m_LRU.GetLRUHandleList( lruList );

	CDataCacheSection *pSection = NULL;
	if ( pszSection )
	{
		pSection = (CDataCacheSection *)FindSection( pszSection );
		if ( !pSection )
		{
			Msg( "Unknown cache section %s\n", pszSection );
			return;
		}
	}

	if ( reportType == DC_DETAIL_REPORT )
	{
		CUtlRBTree< memhandle_t, int >	sortedbysize( 0, 0, SortMemhandlesBySizeLessFunc );
		for ( auto &v : lockedlist )
		{
			if ( !pSection || AccessItem( v )->pSection == pSection )
				sortedbysize.Insert( v );
		}

		for ( auto &v : lruList )
		{
			if ( !pSection || AccessItem( v )->pSection == pSection )
				sortedbysize.Insert( v );
		}

		for ( auto i = sortedbysize.FirstInorder(); i != sortedbysize.InvalidIndex(); i = sortedbysize.NextInorder( i ) )
		{
			OutputItemReport( sortedbysize[ i ] );
		}
		OutputReport( DC_SUMMARY_REPORT, pszSection );
	}
	else if ( reportType == DC_DETAIL_REPORT_LRU )
	{
		for ( auto &v : lockedlist )
		{
			if ( !pSection || AccessItem( v )->pSection == pSection )
				OutputItemReport( v );
		}

		for ( auto &v : lruList )
		{
			if ( !pSection || AccessItem( v )->pSection == pSection )
				OutputItemReport( v );
		}
		OutputReport( DC_SUMMARY_REPORT, pszSection );
	}
	else if ( reportType == DC_SUMMARY_REPORT )
	{
		if ( !pszSection )
		{
			// summary for all of the sections
			for ( auto *s : m_Sections )
			{
				if ( s->GetName() )
				{
					OutputReport( DC_SUMMARY_REPORT, s->GetName() );
				}
			}
			Msg( "Summary: %zd resources total %s, %.2f %% of capacity\n", lockedlist.Count() + lruList.Count(), Q_pretifymem( bytesUsed, 2, true ), percent );
		}
		else
		{
			// summary for the specified section
			DataCacheItem_t *pItem;
			size_t sectionBytes = 0;
			intp sectionCount = 0;
			for ( auto &v : lockedlist )
			{
				if ( AccessItem( v )->pSection == pSection )
				{
					pItem = g_DataCache.m_LRU.GetResource_NoLockNoLRUTouch( v );
					sectionBytes += pItem->size;
					sectionCount++;
				}
			}
			for ( auto &v : lruList )
			{
				if ( AccessItem( v )->pSection == pSection )
				{
					pItem = g_DataCache.m_LRU.GetResource_NoLockNoLRUTouch( v );
					sectionBytes += pItem->size;
					sectionCount++;
				}
			}
			size_t sectionSize = 1;
			float sectionPercent;
			if ( pSection->GetLimits().nMaxBytes == (size_t)-1 )
			{
				// section unrestricted, base on total size
				sectionSize = bytesTotal;
			}
			else if ( pSection->GetLimits().nMaxBytes )
			{
				sectionSize = pSection->GetLimits().nMaxBytes;
			}
			sectionPercent = 100.0f * (float)sectionBytes/(float)sectionSize;
			Msg( "Section [%s]: %zd resources total %s, %.2f %% of limit (%s)\n", pszSection, sectionCount, Q_pretifymem( sectionBytes, 2, true ), sectionPercent, Q_pretifymem( sectionSize, 2, true ) );
		}
	}
}

//-------------------------------------

void CDataCache::OutputItemReport( memhandle_t hItem )
{
	AUTO_LOCK( m_mutex );
	DataCacheItem_t *pItem = m_LRU.GetResource_NoLockNoLRUTouch( hItem );
	if ( !pItem )
		return;

	CDataCacheSection *pSection = pItem->pSection;

	char name[DC_MAX_ITEM_NAME+1];
	name[0] = 0;

	pSection->GetClient()->GetItemName( pItem->clientId, pItem->pItemData, name, DC_MAX_ITEM_NAME );

	Msg( "\t%16.16s : %12s : 0x%08x, 0x%p, 0x%p : %s : %s\n", 
		Q_pretifymem( pItem->size, 2, true ), 
		pSection->GetName(), 
		pItem->clientId, pItem->pItemData, hItem,
		( name[0] ) ? name : "unknown",
		( m_LRU.LockCount( hItem ) ) ? CFmtStr( "Locked %d", m_LRU.LockCount( hItem ) ).operator const char*() : "" );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
intp CDataCache::FindSectionIndex( const char *pszSection )
{
	intp i = 0;
	for ( auto &s : m_Sections )
	{
		if ( stricmp( s->GetName(), pszSection ) == 0 )
			return i;

		++i;
	}
	return m_Sections.InvalidIndex();
}


//-----------------------------------------------------------------------------
// Sorting utility used by the data cache report
//-----------------------------------------------------------------------------
bool CDataCache::SortMemhandlesBySizeLessFunc( const memhandle_t& lhs, const memhandle_t& rhs )
{
	DataCacheItem_t *pItem1 = g_DataCache.m_LRU.GetResource_NoLockNoLRUTouch( lhs );
	DataCacheItem_t *pItem2 = g_DataCache.m_LRU.GetResource_NoLockNoLRUTouch( rhs );

	Assert( pItem1 );
	Assert( pItem2 );

	return pItem1->size < pItem2->size;
}


