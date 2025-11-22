
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "tier1/datamanager.h"

#include "tier0/basetypes.h"

DECLARE_POINTER_HANDLE( memhandle_t );

CDataManagerBase::CDataManagerBase( size_t maxSize )
{
	m_targetMemorySize = maxSize;
	m_memUsed = 0;
	m_lruList = m_memoryLists.CreateList();
	m_lockList = m_memoryLists.CreateList();
	m_freeList = m_memoryLists.CreateList();
	m_listsAreFreed = 0;
}

CDataManagerBase::~CDataManagerBase() 
{
	Assert( m_listsAreFreed );
}

void CDataManagerBase::NotifySizeChanged( [[maybe_unused]] memhandle_t handle, size_t oldSize, size_t newSize )
{
	AUTO_LOCK(*this);

	if (newSize >= oldSize)
		m_memUsed += newSize - oldSize;
	else
		m_memUsed -= oldSize - newSize;
}

void CDataManagerBase::SetTargetSize( size_t targetSize )
{
	m_targetMemorySize = targetSize;
}

size_t CDataManagerBase::FlushAllUnlocked()
{
	Lock();

	auto nFlush = m_memoryLists.Count( m_lruList );
	void **pScratch = stackallocT( void *, nFlush );
	CUtlVector<void *> destroyList( pScratch, nFlush );

	size_t nBytesInitial = MemUsed_Inline();

	auto node = m_memoryLists.Head(m_lruList);
	while ( node != m_memoryLists.InvalidIndex() )
	{
		auto next = m_memoryLists.Next(node);
		m_memoryLists.Unlink( m_lruList, node );
		destroyList.AddToTail( GetForFreeByIndex( node ) );
		node = next;
	}

	Unlock();

	for ( decltype(nFlush) i = 0; i < nFlush; i++ )
	{
		DestroyResourceStorage( destroyList[i] );
	}

	return ( nBytesInitial - MemUsed_Inline() );
}

size_t CDataManagerBase::FlushToTargetSize()
{
	return EnsureCapacity(0);
}

// Frees everything!  The LRU AND the LOCKED items.  This is only used to forcibly free the resources,
// not to make space.
size_t CDataManagerBase::FlushAll()
{
	Lock();

	auto nFlush = m_memoryLists.Count( m_lruList ) + m_memoryLists.Count( m_lockList );
	void **pScratch = stackallocT( void*, nFlush );
	CUtlVector<void *> destroyList( pScratch, nFlush );

	size_t result = MemUsed_Inline();

	auto node = m_memoryLists.Head(m_lruList);
	while ( node != m_memoryLists.InvalidIndex() )
	{
		auto nextNode = m_memoryLists.Next(node);
		m_memoryLists.Unlink( m_lruList, node );
		destroyList.AddToTail( GetForFreeByIndex( node ) );
		node = nextNode;
	}

	node = m_memoryLists.Head(m_lockList);
	while ( node != m_memoryLists.InvalidIndex() )
	{
		auto nextNode = m_memoryLists.Next(node);
		m_memoryLists.Unlink( m_lockList, node );
		m_memoryLists[node].lockCount = 0;
		destroyList.AddToTail( GetForFreeByIndex( node ) );
		node = nextNode;
	}

	m_listsAreFreed = false;

	Unlock();

	for ( decltype(nFlush) i = 0; i < nFlush; i++ )
	{
		DestroyResourceStorage( destroyList[i] );
	}

	return result;
}

size_t CDataManagerBase::Purge( size_t nBytesToPurge )
{
	size_t nTargetSize = MemUsed_Inline() - nBytesToPurge;

	// Check for underflow
	if ( MemUsed_Inline() < nBytesToPurge )
		nTargetSize = 0;

	const size_t nImpliedCapacity = MemTotal_Inline() - nTargetSize;
	return EnsureCapacity( nImpliedCapacity );
}


void CDataManagerBase::DestroyResource( memhandle_t handle )
{
	void *p = nullptr;

	{
		AUTO_LOCK(*this);

		unsigned short index = FromHandle( handle );
		if ( !m_memoryLists.IsValidIndex(index) )
		{
			return;
		}
	
		Assert( m_memoryLists[index].lockCount == 0  );
		if ( m_memoryLists[index].lockCount )
			BreakLock( handle );
		m_memoryLists.Unlink( m_lruList, index );

		p = GetForFreeByIndex( index );
	}

	DestroyResourceStorage( p );
}


void *CDataManagerBase::LockResource( memhandle_t handle )
{
	AUTO_LOCK( *this );

	unsigned short memoryIndex = FromHandle(handle);
	if ( memoryIndex != m_memoryLists.InvalidIndex() )
	{
		if ( m_memoryLists[memoryIndex].lockCount == 0 )
		{
			m_memoryLists.Unlink( m_lruList, memoryIndex );
			m_memoryLists.LinkToTail( m_lockList, memoryIndex );
		}
		auto &mem = m_memoryLists[memoryIndex];
		Assert(mem.lockCount != (unsigned short)-1);
		mem.lockCount++;
		return mem.pStore;
	}

	return nullptr;
}

int CDataManagerBase::UnlockResource( memhandle_t handle )
{
	AUTO_LOCK( *this );

	unsigned short memoryIndex = FromHandle(handle);
	if ( memoryIndex != m_memoryLists.InvalidIndex() )
	{
		auto &mem = m_memoryLists[memoryIndex];
		Assert( mem.lockCount > 0 );
		if ( mem.lockCount > 0 )
		{
			--mem.lockCount;
			if ( mem.lockCount == 0 )
			{
				m_memoryLists.Unlink( m_lockList, memoryIndex );
				m_memoryLists.LinkToTail( m_lruList, memoryIndex );
			}
		}
		return m_memoryLists[memoryIndex].lockCount;
	}

	return 0;
}

void *CDataManagerBase::GetResource_NoLockNoLRUTouch( memhandle_t handle )
{
	AUTO_LOCK( *this );

	unsigned short memoryIndex = FromHandle(handle);
	if ( memoryIndex != m_memoryLists.InvalidIndex() )
	{
		return m_memoryLists[memoryIndex].pStore;
	}

	return nullptr;
}


void *CDataManagerBase::GetResource_NoLock( memhandle_t handle )
{
	AUTO_LOCK( *this );

	unsigned short memoryIndex = FromHandle(handle);
	if ( memoryIndex != m_memoryLists.InvalidIndex() )
	{
		TouchByIndex( memoryIndex );
		return m_memoryLists[memoryIndex].pStore;
	}

	return nullptr;
}

void CDataManagerBase::TouchResource( memhandle_t handle )
{
	AUTO_LOCK( *this );

	TouchByIndex( FromHandle(handle) );
}

void CDataManagerBase::MarkAsStale( memhandle_t handle )
{
	AUTO_LOCK( *this );

	unsigned short memoryIndex = FromHandle(handle);
	if ( memoryIndex != m_memoryLists.InvalidIndex() )
	{
		if ( m_memoryLists[memoryIndex].lockCount == 0 )
		{
			m_memoryLists.Unlink( m_lruList, memoryIndex );
			m_memoryLists.LinkToHead( m_lruList, memoryIndex );
		}
	}
}

int CDataManagerBase::BreakLock( memhandle_t handle )
{
	AUTO_LOCK( *this );

	unsigned short memoryIndex = FromHandle(handle);
	if ( memoryIndex != m_memoryLists.InvalidIndex() && m_memoryLists[memoryIndex].lockCount )
	{
		auto nBroken = m_memoryLists[memoryIndex].lockCount;
		m_memoryLists[memoryIndex].lockCount = 0;
		m_memoryLists.Unlink( m_lockList, memoryIndex );
		m_memoryLists.LinkToTail( m_lruList, memoryIndex );

		return nBroken;
	}
	return 0;
}

int CDataManagerBase::BreakAllLocks()
{
	AUTO_LOCK( *this );

	int nBroken = 0;

	auto node = m_memoryLists.Head(m_lockList);
	while ( node != m_memoryLists.InvalidIndex() )
	{
		nBroken++;

		auto nextNode = m_memoryLists.Next(node);
		m_memoryLists[node].lockCount = 0;
		m_memoryLists.Unlink( m_lockList, node );
		m_memoryLists.LinkToTail( m_lruList, node );
		node = nextNode;
	}

	return nBroken;

}

unsigned short CDataManagerBase::CreateHandle( bool bCreateLocked )
{
	AUTO_LOCK( *this );

	auto memoryIndex = m_memoryLists.Head(m_freeList);
	unsigned short list = ( bCreateLocked ) ? m_lockList : m_lruList;
	if ( memoryIndex != m_memoryLists.InvalidIndex() )
	{
		m_memoryLists.Unlink( m_freeList, memoryIndex );
		m_memoryLists.LinkToTail( list, memoryIndex );
	}
	else
	{
		memoryIndex = m_memoryLists.AddToTail( list );
	}

	if ( bCreateLocked )
	{
		m_memoryLists[memoryIndex].lockCount++;
	}

	return memoryIndex;
}

memhandle_t CDataManagerBase::StoreResourceInHandle( unsigned short memoryIndex, void *pStore, size_t realSize )
{
	AUTO_LOCK( *this );

	resource_lru_element_t &mem = m_memoryLists[memoryIndex];
	mem.pStore = pStore;
	m_memUsed += realSize;
	return ToHandle(memoryIndex);
}

void CDataManagerBase::TouchByIndex( unsigned short memoryIndex )
{
	if ( memoryIndex != m_memoryLists.InvalidIndex() )
	{
		if ( m_memoryLists[memoryIndex].lockCount == 0 )
		{
			m_memoryLists.Unlink( m_lruList, memoryIndex );
			m_memoryLists.LinkToTail( m_lruList, memoryIndex );
		}
	}
}

memhandle_t CDataManagerBase::ToHandle( unsigned short index )
{
	unsigned int hiword = m_memoryLists.Element(index).serial;
	hiword <<= 16;
	index++;
	return (memhandle_t)static_cast<uintp>( hiword|index );
}

size_t CDataManagerBase::TargetSize() const
{ 
	return MemTotal_Inline(); 
}

size_t CDataManagerBase::AvailableSize() const
{ 
	return MemAvailable_Inline(); 
}


size_t CDataManagerBase::UsedSize() const
{ 
	return MemUsed_Inline(); 
}

// free resources until there is enough space to hold "size"
size_t CDataManagerBase::EnsureCapacity( size_t size )
{
	size_t nBytesInitial = MemUsed_Inline();
	while ( MemUsed_Inline() > MemTotal_Inline() || MemAvailable_Inline() < size )
	{
		void *p = nullptr;
		{
			AUTO_LOCK(*this);

			auto lruIndex = m_memoryLists.Head( m_lruList );
			if ( lruIndex == m_memoryLists.InvalidIndex() )
			{
				break;
			}
			m_memoryLists.Unlink( m_lruList, lruIndex );
			p = GetForFreeByIndex( lruIndex );
		}

		DestroyResourceStorage( p );
	}
	return ( nBytesInitial - MemUsed_Inline() );
}

// free this resource and move the handle to the free list
void *CDataManagerBase::GetForFreeByIndex( unsigned short memoryIndex )
{
	void *p = nullptr;
	if ( memoryIndex != m_memoryLists.InvalidIndex() )
	{
		Assert( m_memoryLists[memoryIndex].lockCount == 0 );

		resource_lru_element_t &mem = m_memoryLists[memoryIndex];
		size_t size = GetRealSize( mem.pStore );
		if ( size > m_memUsed )
		{
			ExecuteOnce( Warning( "Data manager 'used' memory incorrect\n" ) );
			size = m_memUsed;
		}

		m_memUsed -= size;
		p = mem.pStore;
		mem.pStore = nullptr;
		mem.serial++;
		m_memoryLists.LinkToTail( m_freeList, memoryIndex );
	}
	return p;
}

// get a list of everything in the LRU
void CDataManagerBase::GetLRUHandleList( CUtlVector< memhandle_t >& list )
{
	for ( auto node = m_memoryLists.Tail(m_lruList);
			node != m_memoryLists.InvalidIndex();
			node = m_memoryLists.Previous(node) )
	{
		list.AddToTail( ToHandle( node ) );
	}
}

// get a list of everything locked
void CDataManagerBase::GetLockHandleList( CUtlVector< memhandle_t >& list )
{
	for ( auto node = m_memoryLists.Head(m_lockList);
			node != m_memoryLists.InvalidIndex();
			node = m_memoryLists.Next(node) )
	{
		list.AddToTail( ToHandle( node ) );
	}
}

