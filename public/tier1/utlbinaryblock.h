//====== Copyright ?1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef UTLBINARYBLOCK_H
#define UTLBINARYBLOCK_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlmemory.h"
#include "tier1/strtools.h"
#include <climits>

//-----------------------------------------------------------------------------
// Base class, containing simple memory management
//-----------------------------------------------------------------------------
class CUtlBinaryBlock
{
public:
	CUtlBinaryBlock( intp growSize = 0, intp initSize = 0 );

	// NOTE: nInitialLength indicates how much of the buffer starts full
	CUtlBinaryBlock( void* pMemory, intp nSizeInBytes, intp nInitialLength );
	CUtlBinaryBlock( const void* pMemory, intp nSizeInBytes );
	CUtlBinaryBlock( const CUtlBinaryBlock& src );

	void		Get( void *pValue, intp nMaxLen ) const;
	void		Set( const void *pValue, intp nLen );
	const void	*Get( ) const;
	void		*Get( );

	// STL compatible member functions. These allow easier use of std::sort
	// and they are forward compatible with the C++ 11 range-based for loops.
	unsigned char* begin()						{ return static_cast<unsigned char*>(Get()); }
	const unsigned char* begin() const			{ return static_cast<const unsigned char*>(Get()); }
	unsigned char* end()						{ return static_cast<unsigned char*>(Get()) + Length(); }
	const unsigned char* end() const			{ return static_cast<const unsigned char*>(Get()) + Length(); }

	unsigned char& operator[]( intp i );
	const unsigned char& operator[]( intp i ) const;

	intp		Length() const;
	void		SetLength( intp nLength );	// Undefined memory will result
	bool		IsEmpty() const;
	void		Clear();
	void		Purge();

	bool		IsReadOnly() const;

	CUtlBinaryBlock &operator=( const CUtlBinaryBlock &src );

	// Test for equality
	bool operator==( const CUtlBinaryBlock &src ) const;

private:
	CUtlMemory<unsigned char> m_Memory;
	intp m_nActualLength;
};


//-----------------------------------------------------------------------------
// class inlines
//-----------------------------------------------------------------------------
inline const void *CUtlBinaryBlock::Get( ) const
{
	return m_Memory.Base();
}

inline void *CUtlBinaryBlock::Get( )
{
	return m_Memory.Base();
}

inline intp CUtlBinaryBlock::Length() const
{
	return m_nActualLength;
}

inline unsigned char& CUtlBinaryBlock::operator[]( intp i )
{
	return m_Memory[i];
}

inline const unsigned char& CUtlBinaryBlock::operator[]( intp i ) const
{
	return m_Memory[i];
}

inline bool CUtlBinaryBlock::IsReadOnly() const
{
	return m_Memory.IsReadOnly();
}

inline bool CUtlBinaryBlock::IsEmpty() const
{
	return Length() == 0;
}

inline void CUtlBinaryBlock::Clear()
{
	SetLength( 0 );
}

inline void CUtlBinaryBlock::Purge()
{
	SetLength( 0 );
	m_Memory.Purge();
}

#endif // UTLBINARYBLOCK_H

