//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef UTLSTRINGMAP_H
#define UTLSTRINGMAP_H

#include "utlsymbol.h"

template <typename T>
class CUtlStringMap
{
public:
	CUtlStringMap( bool caseInsensitive = true ) : m_SymbolTable( 0, 32, caseInsensitive )
	{
	}

	// Get data by the string itself:
	[[nodiscard]] T& operator[]( const char *pString )
	{
		CUtlSymbol symbol = m_SymbolTable.AddString( pString );
		intp index = ( intp )( UtlSymId_t )symbol;
		if( m_Vector.Count() <= index )
		{
			m_Vector.EnsureCount( index + 1 );
		}
		return m_Vector[index];
	}

	// Get data by the string's symbol table ID - only used to retrieve a pre-existing symbol, not create a new one!
	[[nodiscard]] T& operator[]( UtlSymId_t n )
	{
		Assert( n >=0 && n <= m_Vector.Count() );
		return m_Vector[n];
	}

	[[nodiscard]] const T& operator[]( UtlSymId_t n ) const
	{
		Assert( n >=0 && n <= m_Vector.Count() );
		return m_Vector[n];
	}

	[[nodiscard]] bool Defined( const char *pString ) const
	{
		return m_SymbolTable.Find( pString ) != UTL_INVAL_SYMBOL;
	}

	[[nodiscard]] UtlSymId_t Find( const char *pString ) const
	{
		return m_SymbolTable.Find( pString );
	}

	[[nodiscard]] static constexpr UtlSymId_t InvalidIndex()
	{
		return UTL_INVAL_SYMBOL;
	}

	[[nodiscard]] unsigned short GetNumStrings( ) const
	{
		return m_SymbolTable.GetNumStrings();
	}

	[[nodiscard]] const char *String( UtlSymId_t n )	const
	{
		return m_SymbolTable.String( n );
	}

	// Clear all of the data from the map
	void Clear()
	{
		m_Vector.RemoveAll();
		m_SymbolTable.RemoveAll();
	}

	void Purge()
	{
		m_Vector.Purge();
		m_SymbolTable.RemoveAll();
	}

	void PurgeAndDeleteElements()
	{
		m_Vector.PurgeAndDeleteElements();
		m_SymbolTable.RemoveAll();
	}



private:
	CUtlVector<T> m_Vector;
	CUtlSymbolTable m_SymbolTable;
};

#endif // UTLSTRINGMAP_H
