//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "vstdlib/IKeyValuesSystem.h"
#include "tier1/KeyValues.h"
#include "tier1/mempool.h"
#include "tier1/utlsymbol.h"
#include "tier0/threadtools.h"
#include "tier1/memstack.h"
#include "tier1/utlmap.h"
#include "tier1/utlstring.h"
#include "tier1/fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#ifdef NO_SBH // no need to pool if using tier0 small block heap
#define KEYVALUES_USE_POOL 1
#endif

//-----------------------------------------------------------------------------
// Purpose: Central storage point for KeyValues memory and symbols
//-----------------------------------------------------------------------------
class CKeyValuesSystem final : public IKeyValuesSystem
{
public:
	CKeyValuesSystem();
	~CKeyValuesSystem();

	// registers the size of the KeyValues in the specified instance
	// so it can build a properly sized memory pool for the KeyValues objects
	// the sizes will usually never differ but this is for versioning safety
	void RegisterSizeofKeyValues(intp size) override;

	// allocates/frees a KeyValues object from the shared mempool
	void *AllocKeyValuesMemory(intp size) override;
	void FreeKeyValuesMemory(void *pMem) override;

	// symbol table access (used for key names)
	HKeySymbol GetSymbolForString( const char *name, bool bCreate ) override;
	const char *GetStringForSymbol(HKeySymbol symbol) override;

	// returns the wide version of ansi, also does the lookup on #'d strings
	void GetLocalizedFromANSI( const char *ansi, wchar_t *outBuf, int unicodeBufferSizeInBytes);
	void GetANSIFromLocalized( const wchar_t *wchar, char *outBuf, int ansiBufferSizeInBytes );

	// for debugging, adds KeyValues record into global list so we can track memory leaks
	void AddKeyValuesToMemoryLeakList(void *pMem, HKeySymbol name) override;
	void RemoveKeyValuesFromMemoryLeakList(void *pMem) override;

	// maintain a cache of KeyValues we load from disk. This saves us quite a lot of time on app startup. 
	void AddFileKeyValuesToCache( const KeyValues* _kv, const char *resourceName, const char *pathID ) override;
	bool LoadFileKeyValuesFromCache( KeyValues* outKv, const char *resourceName, const char *pathID, IBaseFileSystem *filesystem ) const override;
	void InvalidateCache() override;
	void InvalidateCacheForFile( const char *resourceName, const char *pathID ) override;

private:
#ifdef KEYVALUES_USE_POOL
	CUtlMemoryPool *m_pMemPool;
#endif
	intp m_iMaxKeyValuesSize;

	// string hash table
	CMemoryStack m_Strings;
	struct hash_item_t
	{
		HKeySymbol stringIndex;
		hash_item_t *next;
	};
	CUtlMemoryPool m_HashItemMemPool;
	CUtlVector<hash_item_t> m_HashTable;
	intp CaseInsensitiveHash(const char *string, intp iBounds);

	void DoInvalidateCache();

	struct MemoryLeakTracker_t
	{
		HKeySymbol nameIndex;
		void *pMem;
	};
	static bool MemoryLeakTrackerLessFunc( const MemoryLeakTracker_t &lhs, const MemoryLeakTracker_t &rhs )
	{
		return lhs.pMem < rhs.pMem;
	}
	CUtlRBTree<MemoryLeakTracker_t, intp> m_KeyValuesTrackingList;

	CThreadFastMutex m_mutex;

	CUtlMap<CUtlString, KeyValues*> m_KeyValueCache;
};

// EXPOSE_SINGLE_INTERFACE(CKeyValuesSystem, IKeyValuesSystem, KEYVALUES_INTERFACE_VERSION);

//-----------------------------------------------------------------------------
// Instance singleton and expose interface to rest of code
//-----------------------------------------------------------------------------
static CKeyValuesSystem g_KeyValuesSystem;

IKeyValuesSystem *KeyValuesSystem()
{
	return &g_KeyValuesSystem;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CKeyValuesSystem::CKeyValuesSystem() 
: m_HashItemMemPool(sizeof(hash_item_t), 64, UTLMEMORYPOOL_GROW_FAST, "CKeyValuesSystem::m_HashItemMemPool")
, m_KeyValuesTrackingList(0, 0, MemoryLeakTrackerLessFunc)
, m_KeyValueCache( UtlStringLessFunc )
{
	// initialize hash table
	m_HashTable.AddMultipleToTail(2047);
	for ( auto &t : m_HashTable )
	{
		t.stringIndex = 0;
		t.next = NULL;
	}

	constexpr size_t size = 4u * 1024 * 1024; //-V112
#ifdef PLATFORM_64BITS
	if ( !m_Strings.Init( size, 64u*1024, 0, 8 ) ) //-V112
#else
	if ( !m_Strings.Init( size, 64u*1024, 0, 4 ) ) //-V112
#endif
	{
		// dimhotepus: Handle allocator failure.
		Error( "KeyValues allocator unable to allocate %zu virtual bytes.\n", size );
	}

	char *pszEmpty = ((char *)m_Strings.Alloc(1));
	*pszEmpty = 0;

#ifdef KEYVALUES_USE_POOL
	m_pMemPool = NULL;
#endif
	m_iMaxKeyValuesSize = sizeof(KeyValues);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CKeyValuesSystem::~CKeyValuesSystem()
{
#ifdef KEYVALUES_USE_POOL
#ifdef _DEBUG
	// display any memory leaks
	if (m_pMemPool && m_pMemPool->Count() > 0)
	{
		DevMsg("Leaked KeyValues blocks: %zd\n", m_pMemPool->Count());
	}

	// iterate all the existing keyvalues displaying their names
	for (intp i = 0; i < m_KeyValuesTrackingList.MaxElement(); i++)
	{
		if (m_KeyValuesTrackingList.IsValidIndex(i))
		{
			DevMsg("\tleaked KeyValues(%s)\n", &m_Strings[m_KeyValuesTrackingList[i].nameIndex]);
		}
	}
#endif

	delete m_pMemPool;
#endif

	DoInvalidateCache();
}

//-----------------------------------------------------------------------------
// Purpose: registers the size of the KeyValues in the specified instance
//			so it can build a properly sized memory pool for the KeyValues objects
//			the sizes will usually never differ but this is for versioning safety
//-----------------------------------------------------------------------------
void CKeyValuesSystem::RegisterSizeofKeyValues(intp size)
{
	if (size > m_iMaxKeyValuesSize)
	{
		m_iMaxKeyValuesSize = size;
	}
}

#ifdef KEYVALUES_USE_POOL
static void KVLeak( char const *fmt, ... )
{
	va_list argptr; 
    char data[1024];
    
    va_start(argptr, fmt);
    V_vsprintf_safe(data, fmt, argptr);
    va_end(argptr);

	Msg( "%s", data );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: allocates a KeyValues object from the shared mempool
//-----------------------------------------------------------------------------
void *CKeyValuesSystem::AllocKeyValuesMemory(intp size)
{
#ifdef KEYVALUES_USE_POOL
	// allocate, if we don't have one yet
	if (!m_pMemPool)
	{
		m_pMemPool = new CUtlMemoryPool(m_iMaxKeyValuesSize, 1024, UTLMEMORYPOOL_GROW_FAST, "CKeyValuesSystem::m_pMemPool" );
		m_pMemPool->SetErrorReportFunc( KVLeak );
	}

	return m_pMemPool->Alloc(size);
#else
	return malloc( size );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: frees a KeyValues object from the shared mempool
//-----------------------------------------------------------------------------
void CKeyValuesSystem::FreeKeyValuesMemory(void *pMem)
{
#ifdef KEYVALUES_USE_POOL
	m_pMemPool->Free(pMem);
#else
	free( pMem );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: symbol table access (used for key names)
//-----------------------------------------------------------------------------
HKeySymbol CKeyValuesSystem::GetSymbolForString( const char *name, bool bCreate )
{
	if ( !name )
	{
		return (-1);
	}

	AUTO_LOCK( m_mutex );

	intp hash = CaseInsensitiveHash(name, m_HashTable.Count());
	hash_item_t *item = &m_HashTable[hash];
	while (1)
	{
		if (!stricmp(name, (char *)m_Strings.GetBase() + item->stringIndex ))
		{
			return (HKeySymbol)item->stringIndex;
		}

		if (item->next == NULL)
		{
			if ( !bCreate )
			{
				// not found
				return -1;
			}

			// we're not in the table
			if (item->stringIndex != 0)
			{
				// first item is used, an new item
				item->next = (hash_item_t *)m_HashItemMemPool.Alloc(sizeof(hash_item_t));
				item = item->next;
			}

			// build up the new item
			item->next = NULL;
			const intp stringSize = V_strlen(name) + 1;
			char *pString = static_cast<char *>(m_Strings.Alloc( stringSize ));
			if ( !pString )
			{
				Error( "Out of keyvalue string space" );
				return -1;
			}
			item->stringIndex = pString - static_cast<char *>(m_Strings.GetBase());
			V_strncpy(pString, name, stringSize);
			return item->stringIndex;
		}

		item = item->next;
	}

	// shouldn't be able to get here
	Assert(0);
	return (-1);
}

//-----------------------------------------------------------------------------
// Purpose: symbol table access
//-----------------------------------------------------------------------------
const char *CKeyValuesSystem::GetStringForSymbol(HKeySymbol symbol)
{
	if ( symbol == -1 )
	{
		return "";
	}
	return ((char *)m_Strings.GetBase() + (size_t)symbol);
}

//-----------------------------------------------------------------------------
// Purpose: adds KeyValues record into global list so we can track memory leaks
//-----------------------------------------------------------------------------
void CKeyValuesSystem::AddKeyValuesToMemoryLeakList(void *pMem, HKeySymbol name)
{
#ifdef _DEBUG
	// only track the memory leaks in debug builds
	MemoryLeakTracker_t item = { name, pMem };
	m_KeyValuesTrackingList.Insert(item);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: used to track memory leaks
//-----------------------------------------------------------------------------
void CKeyValuesSystem::RemoveKeyValuesFromMemoryLeakList(void *pMem)
{
#ifdef _DEBUG
	// only track the memory leaks in debug builds
	MemoryLeakTracker_t item = { 0, pMem };
	intp index = m_KeyValuesTrackingList.Find(item);
	m_KeyValuesTrackingList.RemoveAt(index);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Removes a particular key value file (from a particular source) from the cache if present.
//-----------------------------------------------------------------------------
void CKeyValuesSystem::InvalidateCacheForFile(const char *resourceName, const char *pathID)
{
	CUtlString identString( CFmtStr( "%s::%s", resourceName ? resourceName : "", pathID ? pathID : "" ) );

	CUtlMap<CUtlString, KeyValues*>::IndexType_t index = m_KeyValueCache.Find( identString );
	if ( m_KeyValueCache.IsValidIndex( index ) )
	{
		m_KeyValueCache[ index ]->deleteThis();
		m_KeyValueCache.RemoveAt( index );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Adds a particular key value file (from a particular source) to the cache if not already present.
//-----------------------------------------------------------------------------
void CKeyValuesSystem::AddFileKeyValuesToCache(const KeyValues* _kv, const char *resourceName, const char *pathID)
{
	CUtlString identString( CFmtStr( "%s::%s", resourceName ? resourceName : "", pathID ? pathID : "" ) );
	// Some files actually have multiple roots, and if you use regular MakeCopy (without passing true), those 
	// will be missed. This caused a bug in soundscapes on dedicated servers.
	m_KeyValueCache.Insert( identString, _kv->MakeCopy( true ) );
}

//-----------------------------------------------------------------------------
// Purpose: Fetches a particular keyvalue from the cache, and copies into _outKv (clearing anything there already). 
//-----------------------------------------------------------------------------
bool CKeyValuesSystem::LoadFileKeyValuesFromCache(KeyValues* outKv, const char *resourceName, const char *pathID, IBaseFileSystem *) const
{
	Assert( outKv );
	Assert( resourceName );

	COM_TimestampedLog("CKeyValuesSystem::LoadFileKeyValuesFromCache(%s%s%s): Begin", pathID ? pathID : "", pathID && resourceName ? "/" : "", resourceName ? resourceName : "");

	CUtlString identString(CFmtStr("%s::%s", resourceName ? resourceName : "", pathID ? pathID : ""));

	CUtlMap<CUtlString, KeyValues*>::IndexType_t index = m_KeyValueCache.Find( identString );

	if ( m_KeyValueCache.IsValidIndex( index ) ) {
		(*outKv) = ( *m_KeyValueCache[ index ] );
		COM_TimestampedLog("CKeyValuesSystem::LoadFileKeyValuesFromCache(%s%s%s): End / Hit", pathID ? pathID : "", pathID && resourceName ? "/" : "", resourceName ? resourceName : "");
		return true;
	}

	COM_TimestampedLog("CKeyValuesSystem::LoadFileKeyValuesFromCache(%s%s%s): End / Miss", pathID ? pathID : "", pathID && resourceName ? "/" : "", resourceName ? resourceName : "");

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Evicts everything from the cache, cleans up the memory used.
//-----------------------------------------------------------------------------
void CKeyValuesSystem::InvalidateCache()
{
	DoInvalidateCache();
}

//-----------------------------------------------------------------------------
// Purpose: generates a simple hash value for a string
//-----------------------------------------------------------------------------
intp CKeyValuesSystem::CaseInsensitiveHash(const char *string, intp iBounds)
{
	uintp hash = 0;

	for ( ; *string != 0; string++ )
	{
		if (*string >= 'A' && *string <= 'Z')
		{
			hash = (hash << 1) + (*string - 'A' + 'a');
		}
		else
		{
			hash = (hash << 1) + *string;
		}
	}
	  
	return hash % iBounds;
}

//-----------------------------------------------------------------------------
// Purpose: Evicts everything from the cache, cleans up the memory used.
//-----------------------------------------------------------------------------
void CKeyValuesSystem::DoInvalidateCache()
{
	// Cleanup the cache.
	FOR_EACH_MAP_FAST( m_KeyValueCache, mapIndex )
	{
		m_KeyValueCache[mapIndex]->deleteThis();
	}

	// Apparently you cannot call RemoveAll on a map without also purging the contents because... ?
	// If you do and you continue to use the map, you will eventually wind up in a case where you
	// have an empty map but it still iterates over elements. Awesome?
	m_KeyValueCache.Purge();
}

