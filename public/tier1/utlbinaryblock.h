//====== Copyright ?1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef UTLBINARYBLOCK_H
#define UTLBINARYBLOCK_H

#include "utlmemory.h"
#include "strtools.h"
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
	[[nodiscard]] const void	*Get( ) const;
	[[nodiscard]] void		*Get( );

	// STL compatible member functions. These allow easier use of std::sort
	// and they are forward compatible with the C++ 11 range-based for loops.
	[[nodiscard]] unsigned char* begin()					{ return static_cast<unsigned char*>(Get()); }
	[[nodiscard]] const unsigned char* begin() const		{ return static_cast<const unsigned char*>(Get()); }
	[[nodiscard]] unsigned char* end()						{ return static_cast<unsigned char*>(Get()) + Length(); }
	[[nodiscard]] const unsigned char* end() const			{ return static_cast<const unsigned char*>(Get()) + Length(); }

	[[nodiscard]] unsigned char& operator[]( intp i );
	[[nodiscard]] const unsigned char& operator[]( intp i ) const;

	[[nodiscard]] intp		Length() const;
	void		SetLength( intp nLength );	// Undefined memory will result
	[[nodiscard]] bool		IsEmpty() const;
	void		Clear();
	void		Purge();

	[[nodiscard]] bool		IsReadOnly() const;

	CUtlBinaryBlock &operator=( const CUtlBinaryBlock &src );

	// Test for equality
	[[nodiscard]] bool operator==( const CUtlBinaryBlock &src ) const;

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

