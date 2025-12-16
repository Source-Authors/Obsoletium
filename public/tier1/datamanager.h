//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef RESOURCEMANAGER_H
#define RESOURCEMANAGER_H

#include "tier0/threadtools.h"
#include "utlmultilist.h"
#include "utlvector.h"

FORWARD_DECLARE_HANDLE( memhandle_t );

#define INVALID_MEMHANDLE ((memhandle_t)-1)

class CDataManagerBase
{
public:

	// public API
	// -----------------------------------------------------------------------------
	// memhandle_t			CreateResource( params ) // implemented by derived class
	void					DestroyResource( memhandle_t handle );

	// type-safe implementation in derived class
	//void					*LockResource( memhandle_t handle );
	int						UnlockResource( memhandle_t handle );
	void					TouchResource( memhandle_t handle );
	void					MarkAsStale( memhandle_t handle );		// move to head of LRU

	[[nodiscard]] int						LockCount( memhandle_t handle );
	int						BreakLock( memhandle_t handle );
	int						BreakAllLocks();

	// HACKHACK: For convenience - offers no lock protection 
	// type-safe implementation in derived class
	//void					*GetResource_NoLock( memhandle_t handle );

	[[nodiscard]] size_t					TargetSize() const;
	[[nodiscard]] size_t					AvailableSize() const;
	[[nodiscard]] size_t					UsedSize() const;

	void					NotifySizeChanged( memhandle_t handle, size_t oldSize, size_t newSize );

	void					SetTargetSize( size_t targetSize );

	// NOTE: flush is equivalent to Destroy
	size_t					FlushAllUnlocked();
	size_t					FlushToTargetSize();
	size_t					FlushAll();
	size_t					Purge( size_t nBytesToPurge );
	size_t					EnsureCapacity( size_t size );

	// Thread lock
	virtual void			Lock() {}
	[[nodiscard]] virtual bool			TryLock() { return true; }
	virtual void			Unlock() {}

	// Iteration

	// -----------------------------------------------------------------------------

	// Debugging only!!!!
	void					GetLRUHandleList( CUtlVector< memhandle_t >& list );
	void					GetLockHandleList( CUtlVector< memhandle_t >& list );


protected:
	// derived class must call these to implement public API
	unsigned short			CreateHandle( bool bCreateLocked );
	memhandle_t				StoreResourceInHandle( unsigned short memoryIndex, void *pStore, size_t realSize );
	void					*GetResource_NoLock( memhandle_t handle );
	void					*GetResource_NoLockNoLRUTouch( memhandle_t handle );
	void					*LockResource( memhandle_t handle );

	// NOTE: you must call this from the destructor of the derived class! (will assert otherwise)
	void					FreeAllLists()	{ FlushAll(); m_listsAreFreed = true; }

	explicit				CDataManagerBase( size_t maxSize );
	virtual					~CDataManagerBase();
	
	
	[[nodiscard]] inline size_t			MemTotal_Inline() const { return m_targetMemorySize; }
	[[nodiscard]] inline size_t			MemAvailable_Inline() const { return m_targetMemorySize - m_memUsed; }
	[[nodiscard]] inline size_t			MemUsed_Inline() const { return m_memUsed; }

// Implemented by derived class:
	virtual void			DestroyResourceStorage( void * ) = 0;
	virtual size_t			GetRealSize( void * ) = 0;

	memhandle_t				ToHandle( unsigned short index );
	unsigned short			FromHandle( memhandle_t handle );
	
	void					TouchByIndex( unsigned short memoryIndex );
	void *					GetForFreeByIndex( unsigned short memoryIndex );

	// One of these is stored per active allocation
	struct resource_lru_element_t
	{
		resource_lru_element_t()
		{
			lockCount = 0;
			serial = 1;
			pStore = nullptr;
		}

		unsigned short lockCount;
		unsigned short serial;
		void	*pStore;
	};

	size_t m_targetMemorySize;
	size_t m_memUsed;

	CUtlMultiList< resource_lru_element_t, unsigned short >  m_memoryLists;
	
	unsigned short m_lruList;
	unsigned short m_lockList;
	unsigned short m_freeList;
	unsigned short m_listsAreFreed : 1;
	unsigned short m_unused : 15;

};

template< class STORAGE_TYPE, class CREATE_PARAMS, class LOCK_TYPE = STORAGE_TYPE *, class MUTEX_TYPE = CThreadNullMutex>
class CDataManager : public CDataManagerBase
{
	using BaseClass = CDataManagerBase;
public:

	CDataManager<STORAGE_TYPE, CREATE_PARAMS, LOCK_TYPE, MUTEX_TYPE>( size_t size = std::numeric_limits<size_t>::max() ) : BaseClass(size) {}
	

	~CDataManager<STORAGE_TYPE, CREATE_PARAMS, LOCK_TYPE, MUTEX_TYPE>() override
	{
		// NOTE: This must be called in all implementations of CDataManager
		FreeAllLists();
	}

	// Use GetData() to translate pointer to LOCK_TYPE
	LOCK_TYPE LockResource( memhandle_t hMem )
	{
		void *pLock = BaseClass::LockResource( hMem );
		if ( pLock )
		{
			return StoragePointer(pLock)->GetData();
		}

		return nullptr;
	}

	// Use GetData() to translate pointer to LOCK_TYPE
	LOCK_TYPE GetResource_NoLock( memhandle_t hMem )
	{
		void *pLock = const_cast<void *>(BaseClass::GetResource_NoLock( hMem ));
		if ( pLock )
		{
			return StoragePointer(pLock)->GetData();
		}
		return nullptr;
	}

	// Use GetData() to translate pointer to LOCK_TYPE
	// Doesn't touch the memory LRU
	LOCK_TYPE GetResource_NoLockNoLRUTouch( memhandle_t hMem )
	{
		void *pLock = const_cast<void *>(BaseClass::GetResource_NoLockNoLRUTouch( hMem ));
		if ( pLock )
		{
			return StoragePointer(pLock)->GetData();
		}
		return nullptr;
	}

	// Wrapper to match implementation of allocation with typed storage & alloc params.
	memhandle_t CreateResource( const CREATE_PARAMS &createParams, bool bCreateLocked = false )
	{
		BaseClass::EnsureCapacity(STORAGE_TYPE::EstimatedSize(createParams));
		unsigned short memoryIndex = BaseClass::CreateHandle( bCreateLocked );
		STORAGE_TYPE *pStore = STORAGE_TYPE::CreateResource( createParams );
		return BaseClass::StoreResourceInHandle( memoryIndex, pStore, pStore->Size() );
	}

	// Iteration. Must lock first
	[[nodiscard]] memhandle_t GetFirstUnlocked()
	{
		auto node = m_memoryLists.Head(m_lruList);
		if ( node == m_memoryLists.InvalidIndex() )
		{
			return INVALID_MEMHANDLE;
		}
		return ToHandle( node );
	}

	[[nodiscard]] memhandle_t GetFirstLocked()
	{
		auto node = m_memoryLists.Head(m_lockList);
		if ( node == m_memoryLists.InvalidIndex() )
		{
			return INVALID_MEMHANDLE;
		}
		return ToHandle( node );
	}

	[[nodiscard]] memhandle_t GetNext( memhandle_t hPrev )
	{
		if ( hPrev == INVALID_MEMHANDLE )
		{
			return INVALID_MEMHANDLE;
		}

		auto iNext = m_memoryLists.Next( FromHandle( hPrev ) );
		if ( iNext == m_memoryLists.InvalidIndex() ) 
		{
			return INVALID_MEMHANDLE;
		}

		return ToHandle( iNext );
	}

	[[nodiscard]] MUTEX_TYPE &AccessMutex()	{ return m_mutex; }
	void Lock() override { m_mutex.Lock(); }
	[[nodiscard]] bool TryLock() override { return m_mutex.TryLock(); }
	void Unlock() override { m_mutex.Unlock(); }

private:
	STORAGE_TYPE *StoragePointer( void *pMem )
	{
		return static_cast<STORAGE_TYPE *>(pMem);
	}

	void DestroyResourceStorage( void *pStore ) override
	{
		StoragePointer(pStore)->DestroyResource();
	}
	
	size_t GetRealSize( void *pStore ) override
	{
		return StoragePointer(pStore)->Size();
	}

	MUTEX_TYPE m_mutex;
};

//-----------------------------------------------------------------------------

inline unsigned short CDataManagerBase::FromHandle( memhandle_t handle )
{
	Assert((uintp)handle <= std::numeric_limits<unsigned>::max() || handle == INVALID_MEMHANDLE);

	auto fullWord = (unsigned int)(uintp)handle; //-V221
	unsigned short serial = fullWord>>16;
	unsigned short index = fullWord & 0xFFFF;

	index--;

	if ( m_memoryLists.IsValidIndex(index) && m_memoryLists[index].serial == serial )
		return index;

	return m_memoryLists.InvalidIndex();
}

inline int CDataManagerBase::LockCount( memhandle_t handle )
{
	AUTO_LOCK( *this );
	int result = 0;
	unsigned short memoryIndex = FromHandle(handle);
	if ( memoryIndex != m_memoryLists.InvalidIndex() )
	{
		result = m_memoryLists[memoryIndex].lockCount;
	}
	return result;
}


#endif // RESOURCEMANAGER_H
