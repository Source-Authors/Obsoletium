//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef STRINGPOOL_H
#define STRINGPOOL_H

#if defined( _WIN32 )
#pragma once
#endif

#include "tier1/utlrbtree.h"
#include "tier1/utlvector.h"

//-----------------------------------------------------------------------------
// Purpose: Allocates memory for strings, checking for duplicates first,
//			reusing exising strings if duplicate found.
//-----------------------------------------------------------------------------

class CStringPool
{
public:
	CStringPool();
	~CStringPool();

	[[nodiscard]] size_t Count() const;

	[[nodiscard]] const char * Allocate( const char *pszValue );
	void FreeAll();

	// searches for a string already in the pool
	[[nodiscard]] const char * Find( const char *pszValue );

protected:
	typedef CUtlRBTree<const char *, unsigned short> CStrSet;

	CStrSet m_Strings;
};

//-----------------------------------------------------------------------------
// Purpose: A reference counted string pool.  
//
// Elements are stored more efficiently than in the conventional string pool, 
// quicker to look up, and storage is tracked via reference counts.  
//
// At some point this should replace CStringPool
//-----------------------------------------------------------------------------
class CCountedStringPool
{
public: // HACK, hash_item_t structure should not be public.

	struct hash_item_t
	{
		char*			pString;
		unsigned short	nNextElement;
		unsigned char	nReferenceCount;
		unsigned char	pad;
	};

	enum
	{
		INVALID_ELEMENT = 0,
		MAX_REFERENCE   = 0xFF,
		HASH_TABLE_SIZE = 1024
	};

	CUtlVector<unsigned short>	m_HashTable;	// Points to each element
	CUtlVector<hash_item_t>		m_Elements;
	unsigned short				m_FreeListStart;

public:
	CCountedStringPool();
	virtual ~CCountedStringPool();

	void			FreeAll();

	[[nodiscard]] char			*FindString( const char* pIntrinsic ); 
	[[nodiscard]] char			*ReferenceString( const char* pIntrinsic );
	void			DereferenceString( const char* pIntrinsic );

	// These are only reliable if there are less than 64k strings in your string pool
	[[nodiscard]] unsigned short	FindStringHandle( const char* pIntrinsic ); 
	[[nodiscard]] unsigned short	ReferenceStringHandle( const char* pIntrinsic );
	[[nodiscard]] char			*HandleToString( unsigned short handle );
	void			SpewStrings();
};

#endif // STRINGPOOL_H
