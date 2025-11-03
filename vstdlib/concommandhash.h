//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Special case hash table for console commands
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef CONCOMMANDHASH_H
#define CONCOMMANDHASH_H

#include "tier1/utllinkedlist.h"
#include "tier1/generichash.h"

// This is a hash table class very similar to the CUtlHashFast, but
// modified specifically so that we can look up ConCommandBases
// by string names without having to actually store those strings in
// the dictionary, and also iterate over all of them. 
// It uses separate chaining: each key hashes to a bucket, each
// bucket is a linked list of hashed commands. We store the hash of 
// the command's string name as well as its pointer, so we can do 
// the linked list march part of the Find() operation more quickly.
class CConCommandHash
{
public:
	using CCommandHashHandle_t = intp;
	using HashKey_t = unsigned int;

	// Constructor/Deconstructor.
	CConCommandHash();
	~CConCommandHash();

	// Memory.
	void Purge( bool bReinitialize );

	// Invalid handle.
	[[nodiscard]] static constexpr inline CCommandHashHandle_t InvalidHandle()	{ return ( CCommandHashHandle_t )~0; }
	[[nodiscard]] inline bool IsValidHandle( CCommandHashHandle_t hHash ) const;

	/// Initialize.
	void Init(); // bucket count is hardcoded in enum below.

	/// Get hash value for a concommand
	[[nodiscard]] static inline HashKey_t Hash( const ConCommandBase *cmd );

	// Size not available; count is meaningless for multilists.
	// intp Count() const;

	// Insertion.
	CCommandHashHandle_t Insert( ConCommandBase *cmd );
	CCommandHashHandle_t FastInsert( ConCommandBase *cmd );

	// Removal.
	void Remove( CCommandHashHandle_t hHash ) RESTRICT;
	void RemoveAll();

	// Retrieval.
	[[nodiscard]] inline CCommandHashHandle_t Find( const char *name ) const;
	[[nodiscard]] CCommandHashHandle_t Find( const ConCommandBase *cmd ) const RESTRICT;
	// A convenience version of Find that skips the handle part
	// and returns a pointer to a concommand, or nullptr if none was found.
	[[nodiscard]] inline ConCommandBase * FindPtr( const char *name ) const;

	[[nodiscard]] inline ConCommandBase * &operator[]( CCommandHashHandle_t hHash );
	[[nodiscard]] inline ConCommandBase *const &operator[]( CCommandHashHandle_t hHash ) const;

#ifdef _DEBUG
	// Dump a report to MSG
	void Report();
#endif

	// Iteration
	struct CCommandHashIterator_t
	{
		intp bucket;
		CCommandHashHandle_t handle;

		CCommandHashIterator_t(intp _bucket, const CCommandHashHandle_t &_handle) 
			: bucket(_bucket), handle(_handle) {};
	};
	[[nodiscard]] inline CCommandHashIterator_t First() const;
	[[nodiscard]] inline CCommandHashIterator_t Next( const CCommandHashIterator_t &hHash ) const;
	[[nodiscard]] inline bool IsValidIterator( const CCommandHashIterator_t &iter ) const;
	[[nodiscard]] inline ConCommandBase * &operator[]( const CCommandHashIterator_t &iter ) { return (*this)[iter.handle];  }
	[[nodiscard]] inline ConCommandBase * const &operator[]( const CCommandHashIterator_t &iter ) const { return (*this)[iter.handle];  }
private:
	// a find func where we've already computed the hash for the string.
	// (hidden private in case we decide to invent a custom string hash func
	//  for this class)
	CCommandHashHandle_t Find( const char *name, HashKey_t hash) const RESTRICT;

protected:
	enum 
	{
		kNUM_BUCKETS = 256,
		kBUCKETMASK  = kNUM_BUCKETS - 1,
	};

	struct HashEntry_t
	{
		HashKey_t m_uiKey;
		ConCommandBase *m_Data;

		HashEntry_t(unsigned int _hash, ConCommandBase * _cmd)
			: m_uiKey(_hash), m_Data(_cmd) {};

		HashEntry_t() : HashEntry_t{0, nullptr} {}
	};

	using datapool_t = CUtlFixedLinkedList<HashEntry_t>;

	CUtlVector<CCommandHashHandle_t>	m_aBuckets;
	datapool_t							m_aDataPool;
};

inline bool CConCommandHash::IsValidHandle( CCommandHashHandle_t hHash ) const
{
	return m_aDataPool.IsValidIndex(hHash);
}


inline CConCommandHash::CCommandHashHandle_t CConCommandHash::Find( const char *name ) const
{
	return Find( name, HashStringCaseless(name) );
}

inline ConCommandBase * &CConCommandHash::operator[]( CCommandHashHandle_t hHash )
{
	return ( m_aDataPool[hHash].m_Data );
}

inline ConCommandBase *const &CConCommandHash::operator[]( CCommandHashHandle_t hHash ) const
{
	return ( m_aDataPool[hHash].m_Data );
}

//-----------------------------------------------------------------------------
// Purpose: For iterating over the whole hash, return the index of the first element
//-----------------------------------------------------------------------------
CConCommandHash::CCommandHashIterator_t CConCommandHash::First() const
{
	// walk through the buckets to find the first one that has some data
	intp bucketCount = m_aBuckets.Count();
	constexpr CCommandHashHandle_t invalidIndex = datapool_t::InvalidIndex();
	for ( intp bucket = 0 ; bucket < bucketCount ; ++bucket )
	{
		CCommandHashHandle_t iElement = m_aBuckets[bucket]; // get the head of the bucket
		if ( iElement != invalidIndex )
			return CCommandHashIterator_t( bucket, iElement );
	}

	// if we are down here, the list is empty 
	return CCommandHashIterator_t( -1, invalidIndex );
}

//-----------------------------------------------------------------------------
// Purpose: For iterating over the whole hash, return the next element after
// the param one. Or an invalid iterator.
//-----------------------------------------------------------------------------
CConCommandHash::CCommandHashIterator_t 
CConCommandHash::Next( const CConCommandHash::CCommandHashIterator_t &iter ) const
{
	// look for the next entry in the current bucket
	constexpr CCommandHashHandle_t invalidIndex = datapool_t::InvalidIndex();
	if ( CCommandHashHandle_t next = m_aDataPool.Next(iter.handle); next != invalidIndex )
	{
		// this bucket still has more elements in it
		return CCommandHashIterator_t(iter.bucket, next);
	}

	// otherwise look for the next bucket with data
	intp bucketCount = m_aBuckets.Count();
	for ( intp bucket = iter.bucket+1 ; bucket < bucketCount ; ++bucket )
	{
		CCommandHashHandle_t next = m_aBuckets[bucket]; // get the head of the bucket
		if (next != invalidIndex)
			return CCommandHashIterator_t( bucket, next );
	}

	// if we're here, there's no more data to be had
	return CCommandHashIterator_t(-1, invalidIndex);
}

bool CConCommandHash::IsValidIterator( const CCommandHashIterator_t &iter ) const
{
	return ( (iter.bucket >= 0) && (m_aDataPool.IsValidIndex(iter.handle)) );
}

inline CConCommandHash::HashKey_t CConCommandHash::Hash( const ConCommandBase *cmd )
{
	return HashStringCaseless( cmd->GetName() );
}

inline ConCommandBase * CConCommandHash::FindPtr( const char *name ) const
{
	CCommandHashHandle_t handle = Find(name);
	if (handle == InvalidHandle())
	{
		return nullptr;
	}
	else
	{
		return (*this)[handle];
	}
}

#endif