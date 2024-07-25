//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//===========================================================================//

#ifndef DATACACHE_H
#define DATACACHE_H

#ifdef _WIN32
#pragma once
#endif

#include "datamanager.h"
#include "utlhash.h"
#include "mempool.h"
#include "tier0/tslist.h"
#include "datacache_common.h"
#include "tier3/tier3.h"


//-----------------------------------------------------------------------------
//
// Data Cache class declarations
//
//-----------------------------------------------------------------------------
class CDataCache;
class CDataCacheSection;

//-----------------------------------------------------------------------------

struct DataCacheItemData_t
{
	const void *		pItemData;
	size_t				size;
	DataCacheClientID_t	clientId;
	CDataCacheSection *	pSection;
};

//-------------------------------------

#define DC_NO_NEXT_LOCKED ((DataCacheItem_t *)(ptrdiff_t)-1)
#define DC_MAX_THREADS_FRAMELOCKED 4

struct DataCacheItem_t : DataCacheItemData_t
{
	DataCacheItem_t( const DataCacheItemData_t &data ) 
	  : DataCacheItemData_t( data ),
		hLRU( INVALID_MEMHANDLE )
	{
		memset( pNextFrameLocked, 0xff, sizeof(pNextFrameLocked) );
	}

	static DataCacheItem_t *CreateResource( const DataCacheItemData_t &data )	{ return new DataCacheItem_t(data); }
	static size_t EstimatedSize( const DataCacheItemData_t &data )		{ return data.size; }
	void DestroyResource();
	DataCacheItem_t *GetData()													{ return this; }
	size_t Size() const															{ return size; }

	memhandle_t		 hLRU;
	DataCacheItem_t *pNextFrameLocked[DC_MAX_THREADS_FRAMELOCKED];

	DECLARE_FIXEDSIZE_ALLOCATOR_MT(DataCacheItem_t);
};

//-------------------------------------

typedef CDataManager<DataCacheItem_t, DataCacheItemData_t, DataCacheItem_t *, CThreadFastMutex> CDataCacheLRU;

//-----------------------------------------------------------------------------
// CDataCacheSection
//
// Purpose: Implements a sub-section of the global cache. Subsections are
//			areas of the cache with thier own memory constraints and common
//			management.
//-----------------------------------------------------------------------------
class CDataCacheSection : public CAlignedNewDelete<TSLIST_HEAD_ALIGNMENT, IDataCacheSection>
{
public:
	CDataCacheSection( CDataCache *pSharedCache, IDataCacheClient *pClient, const char *pszName );
	virtual ~CDataCacheSection();

	IDataCache *GetSharedCache() override;
	IDataCacheClient *GetClient()	{ return m_pClient; }
	const char *GetName() override			{ return szName;	}

	//--------------------------------------------------------
	// IDataCacheSection methods
	//--------------------------------------------------------
	void SetLimits( const DataCacheLimits_t &limits ) override;
	const DataCacheLimits_t &GetLimits();
	void SetOptions( unsigned options ) override;
	void GetStatus( DataCacheStatus_t *pStatus, DataCacheLimits_t *pLimits = NULL ) override;

	inline size_t GetNumBytes() const			{ return m_status.nBytes; }
	inline size_t GetNumItems() const			{ return m_status.nItems; }

	inline size_t GetNumBytesLocked() const		{ return m_status.nBytesLocked; }
	inline size_t GetNumItemsLocked() const		{ return m_status.nItemsLocked; }

	inline size_t GetNumBytesUnlocked() const	{ return m_status.nBytes - m_status.nBytesLocked; }
	inline size_t GetNumItemsUnlocked() const	{ return m_status.nItems - m_status.nItemsLocked; }

	void EnsureCapacity( size_t nBytes, size_t nItems = 1 ) override;

	//--------------------------------------------------------

	bool Add( DataCacheClientID_t clientId, const void *pItemData, size_t size, DataCacheHandle_t *pHandle ) override;
	bool AddEx( DataCacheClientID_t clientId, const void *pItemData, size_t size, unsigned flags, DataCacheHandle_t *pHandle ) override;
	DataCacheHandle_t Find( DataCacheClientID_t clientId ) override;
	DataCacheRemoveResult_t Remove( DataCacheHandle_t handle, const void **ppItemData = NULL, size_t *pItemSize = NULL, bool bNotify = false ) override;
	bool IsPresent( DataCacheHandle_t handle ) override;

	//--------------------------------------------------------

	void *Lock( DataCacheHandle_t handle ) override;
	int Unlock( DataCacheHandle_t handle ) override;
	void *Get( DataCacheHandle_t handle, bool bFrameLock = false ) override;
	void *GetNoTouch( DataCacheHandle_t handle, bool bFrameLock = false ) override;
	void LockMutex() override;
	void UnlockMutex() override;

	//--------------------------------------------------------

	int BeginFrameLocking() override;
	bool IsFrameLocking() override;
	void *FrameLock( DataCacheHandle_t handle ) override;
	int EndFrameLocking() override;

	//--------------------------------------------------------

	int GetLockCount( DataCacheHandle_t handle ) override;
	int BreakLock( DataCacheHandle_t handle ) override;

	//--------------------------------------------------------

	int *GetFrameUnlockCounterPtr() override;
	int			m_nFrameUnlockCounter;

	//--------------------------------------------------------

	bool Touch( DataCacheHandle_t handle ) override;
	bool Age( DataCacheHandle_t handle ) override;

	//--------------------------------------------------------

	size_t Flush(bool bUnlockedOnly = true, bool bNotify = true) override;
	size_t Purge( size_t nBytes ) override;
	size_t PurgeItems( size_t nItems );

	//--------------------------------------------------------

	void OutputReport( DataCacheReportType_t reportType = DC_SUMMARY_REPORT ) override;

	void UpdateSize( DataCacheHandle_t handle, size_t nNewSize ) override;

private:
	friend void DataCacheItem_t::DestroyResource();

	virtual void OnAdd( DataCacheClientID_t, DataCacheHandle_t ) {}
	virtual DataCacheHandle_t DoFind( DataCacheClientID_t clientId );
	virtual void OnRemove( DataCacheClientID_t ) {}

	memhandle_t GetFirstUnlockedItem();
	memhandle_t GetFirstLockedItem();
	memhandle_t GetNextItem( memhandle_t );
	DataCacheItem_t *AccessItem( memhandle_t hCurrent );
	bool DiscardItem( memhandle_t hItem, DataCacheNotificationType_t type );
	bool DiscardItemData( DataCacheItem_t *pItem, DataCacheNotificationType_t type );
	void NoteAdd( size_t size );
	void NoteRemove( size_t size );
	void NoteLock( size_t size );
	void NoteUnlock( size_t size );
	void NoteSizeChanged( size_t oldSize, size_t newSize );

	struct TSLIST_NODE_ALIGN FrameLock_t : public CAlignedNewDelete<TSLIST_NODE_ALIGNMENT>
	{
		//$ WARNING: This needs a TSLNodeBase_t as the first item in here.
		TSLNodeBase_t	base;
		int				m_iLock;
		DataCacheItem_t *m_pFirst;
		int				m_iThread;
	};
	typedef CThreadLocal<FrameLock_t *> CThreadFrameLock;

	CDataCacheLRU &		m_LRU;
	CThreadFrameLock	m_ThreadFrameLock;
	DataCacheStatus_t	m_status;
	DataCacheLimits_t	m_limits;
	IDataCacheClient *	m_pClient;
	unsigned			m_options;
	CDataCache *		m_pSharedCache;
	char				szName[DC_MAX_CLIENT_NAME + 1];
	CTSSimpleList<FrameLock_t> m_FreeFrameLocks;

protected:
	CThreadFastMutex &	m_mutex;
};


//-----------------------------------------------------------------------------
// CDataCacheSectionFastFind
//
// Purpose: A section variant that allows clients to have cache support tracking
//			efficiently (a true cache, not just an LRU)
//-----------------------------------------------------------------------------
class CDataCacheSectionFastFind : public CDataCacheSection
{
public:
	CDataCacheSectionFastFind(CDataCache *pSharedCache, IDataCacheClient *pClient, const char *pszName )
		: CDataCacheSection( pSharedCache, pClient, pszName )
	{
		m_Handles.Init( 1024 );
	}

private:
	virtual DataCacheHandle_t DoFind( DataCacheClientID_t clientId );
	virtual void OnAdd( DataCacheClientID_t clientId, DataCacheHandle_t hCacheItem );
	virtual void OnRemove( DataCacheClientID_t clientId );

	CUtlHashFast<DataCacheHandle_t> m_Handles;
};


//-----------------------------------------------------------------------------
// CDataCache
//
// Purpose: The global shared cache. Manages sections and overall budgets.
//
//-----------------------------------------------------------------------------
class CDataCache : public CTier3AppSystem< IDataCache >
{
	typedef CTier3AppSystem< IDataCache > BaseClass;

public:
	CDataCache();

	//--------------------------------------------------------
	// IAppSystem methods
	//--------------------------------------------------------
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	//--------------------------------------------------------
	// IDataCache methods
	//--------------------------------------------------------

	virtual void SetSize( int nMaxBytes );
	virtual void SetOptions( unsigned options );
	virtual void SetSectionLimits( const char *pszSectionName, const DataCacheLimits_t &limits );
	virtual void GetStatus( DataCacheStatus_t *pStatus, DataCacheLimits_t *pLimits = NULL );

	//--------------------------------------------------------

	virtual IDataCacheSection *AddSection( IDataCacheClient *pClient, const char *pszSectionName, const DataCacheLimits_t &limits = DataCacheLimits_t(), bool bSupportFastFind = false );
	virtual void RemoveSection( const char *pszClientName, bool bCallFlush = true );
	virtual IDataCacheSection *FindSection( const char *pszClientName );

	//--------------------------------------------------------

	void EnsureCapacity( unsigned nBytes );
	virtual unsigned Purge( unsigned nBytes );
	virtual unsigned Flush( bool bUnlockedOnly = true, bool bNotify = true );

	//--------------------------------------------------------

	virtual void OutputReport( DataCacheReportType_t reportType = DC_SUMMARY_REPORT, const char *pszSection = NULL );

	//--------------------------------------------------------

	inline unsigned GetNumBytes()			{ return m_status.nBytes; }
	inline unsigned GetNumItems()			{ return m_status.nItems; }

	inline unsigned GetNumBytesLocked()		{ return m_status.nBytesLocked; }
	inline unsigned GetNumItemsLocked()		{ return m_status.nItemsLocked; }

	inline unsigned GetNumBytesUnlocked()	{ return m_status.nBytes - m_status.nBytesLocked; }
	inline unsigned GetNumItemsUnlocked()	{ return m_status.nItems - m_status.nItemsLocked; }

private:
	//-----------------------------------------------------

	friend class CDataCacheSection;

	//-----------------------------------------------------

	DataCacheItem_t *AccessItem( memhandle_t hCurrent );

	bool IsInFlush()						{ return m_bInFlush; }
	int FindSectionIndex( const char *pszSection );

	// Utilities used by the data cache report
	void OutputItemReport( memhandle_t hItem );
	static bool SortMemhandlesBySizeLessFunc( const memhandle_t& lhs, const memhandle_t& rhs );

	//-----------------------------------------------------

	CDataCacheLRU					m_LRU;
	DataCacheStatus_t				m_status;
	CUtlVector<CDataCacheSection *>	m_Sections;
	bool							m_bInFlush;
	CThreadFastMutex &				m_mutex;
};

//---------------------------------------------------------

extern CDataCache g_DataCache;

//-----------------------------------------------------------------------------

inline DataCacheItem_t *CDataCache::AccessItem( memhandle_t hCurrent ) 
{ 
	return m_LRU.GetResource_NoLockNoLRUTouch( hCurrent ); 
}

//-----------------------------------------------------------------------------

inline IDataCache *CDataCacheSection::GetSharedCache()	
{ 
	return m_pSharedCache; 
}

inline DataCacheItem_t *CDataCacheSection::AccessItem( memhandle_t hCurrent ) 
{ 
	return m_pSharedCache->AccessItem( hCurrent ); 
}

// Note: if status updates are moved out of a mutexed section, will need to change these to use interlocked instructions

inline void CDataCacheSection::NoteSizeChanged( size_t oldSize, size_t newSize )
{
	intp nBytes = ( newSize - oldSize );

	m_status.nBytes += nBytes;
	m_status.nBytesLocked += nBytes;
	ThreadInterlockedExchangeAdd( &m_pSharedCache->m_status.nBytes, nBytes );
	ThreadInterlockedExchangeAdd( &m_pSharedCache->m_status.nBytesLocked, nBytes );
}

inline void CDataCacheSection::NoteAdd( size_t size )
{
	m_status.nBytes += size;
	m_status.nItems++;

	ThreadInterlockedExchangeAdd( &m_pSharedCache->m_status.nBytes, size );
	ThreadInterlockedIncrement( &m_pSharedCache->m_status.nItems );
}

inline void CDataCacheSection::NoteRemove( size_t size )
{
	m_status.nBytes -= size;
	m_status.nItems--;

	ThreadInterlockedExchangeAdd( (intp*)&m_pSharedCache->m_status.nBytes, -static_cast<intp>(size) );
	ThreadInterlockedDecrement( &m_pSharedCache->m_status.nItems );
}

inline void CDataCacheSection::NoteLock( size_t size )
{
	m_status.nBytesLocked += size;
	m_status.nItemsLocked++;

	ThreadInterlockedExchangeAdd( &m_pSharedCache->m_status.nBytesLocked, size );
	ThreadInterlockedIncrement( &m_pSharedCache->m_status.nItemsLocked );
}

inline void CDataCacheSection::NoteUnlock( size_t size )
{
	m_status.nBytesLocked -= size;
	m_status.nItemsLocked--;

	ThreadInterlockedExchangeAdd( (intp*)&m_pSharedCache->m_status.nBytesLocked, -static_cast<intp>(size) );
	ThreadInterlockedDecrement( &m_pSharedCache->m_status.nItemsLocked );

	// something has been unlocked, assume cached pointers are now invalid
	m_nFrameUnlockCounter++;
}

//-----------------------------------------------------------------------------

#endif // DATACACHE_H
