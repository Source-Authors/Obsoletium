//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A class to wrap data for transport over a boundary like a thread
//			or window.
//
//=============================================================================

#ifndef UTLENVELOPE_H
#define UTLENVELOPE_H

#include "tier0/basetypes.h"
#include "utlstring.h"

//-----------------------------------------------------------------------------

class CUtlDataEnvelope
{
public:
	CUtlDataEnvelope( const void *pData, intp nBytes );
	CUtlDataEnvelope( const CUtlDataEnvelope &from );
	// dimhotepus: Move support.
	CUtlDataEnvelope( CUtlDataEnvelope &&from ) noexcept;
	~CUtlDataEnvelope();

	CUtlDataEnvelope &operator=( const CUtlDataEnvelope &from );
	// dimhotepus: Move support.
	CUtlDataEnvelope &operator=( CUtlDataEnvelope &&from ) noexcept;

	[[nodiscard]] operator void *();
	[[nodiscard]] operator void *() const;

private:
	void Assign( const void *pData, intp nBytes );
	void Assign( const CUtlDataEnvelope &from );
	void Purge();

	// TODO: switch to a reference counted array?
	union
	{
		byte *m_pData;
		byte m_data[ sizeof(size_t) ];
	};
	intp m_nBytes;
};


//-----------------------------------------------------------------------------

template <typename T>
class CUtlEnvelope : protected CUtlDataEnvelope
{
public:
	CUtlEnvelope( const T *pData, intp nElems = 1 );
	CUtlEnvelope( const CUtlEnvelope<T> &from );
	// dimhotepus: Move support.
	CUtlEnvelope( CUtlEnvelope<T> &&from ) noexcept;

	CUtlEnvelope<T> &operator=( const CUtlEnvelope<T> &from );
	// dimhotepus: Move support.
	CUtlEnvelope<T> &operator=( CUtlEnvelope<T> &&from ) noexcept;

	[[nodiscard]] operator T *();
	[[nodiscard]] operator T *() const;

	[[nodiscard]] operator void *();
	[[nodiscard]] operator void *() const;
};

//-----------------------------------------------------------------------------

template <>
class CUtlEnvelope<const char *>
{
public:
	CUtlEnvelope( const char *pData )
	{
		m_string = pData;
	}

	CUtlEnvelope( const CUtlEnvelope<const char *> &from )
		: m_string{from.m_string}
	{
	}

	CUtlEnvelope( CUtlEnvelope<const char *> &&from ) noexcept
		: m_string{std::move(from.m_string)}
	{
	}

	CUtlEnvelope<const char *> &operator=( const CUtlEnvelope<const char *> &from )
	{
		m_string = from.m_string;
		return *this;
	}
	
	CUtlEnvelope<const char *> &operator=( CUtlEnvelope<const char *> &&from ) noexcept
	{
		using std::swap;

		swap( m_string, from.m_string );
		return *this;
	}

	[[nodiscard]] operator char *()
	{
		return (char *) m_string.Get();
	}

	[[nodiscard]] operator const char *() const
	{
		return m_string.Get();
	}

	[[nodiscard]] operator void *()
	{
		return (void *) m_string.Get();
	}

	[[nodiscard]] operator const void *() const
	{
		return m_string.Get();
	}

private:
	CUtlString m_string;
};

//-----------------------------------------------------------------------------

#include "tier0/memdbgon.h"

inline void CUtlDataEnvelope::Assign( const void *pData, intp nBytes )
{
	if ( pData )
	{
		m_nBytes = nBytes;
		if ( m_nBytes > ssize( m_data ) )
		{
			m_pData = new byte[nBytes];
			memcpy( m_pData, pData, nBytes );
		}
		else
		{
			memcpy( m_data, pData, nBytes );
		}
	}
	else
	{
		m_pData = nullptr;
		m_nBytes = 0;
	}
}

inline void CUtlDataEnvelope::Assign( const CUtlDataEnvelope &from )
{
	Assign( from.operator void *(), from.m_nBytes );
}

inline void CUtlDataEnvelope::Purge()
{
	if (m_nBytes > ssize( m_data ))
		delete [] m_pData;
	m_nBytes = 0;
}

inline CUtlDataEnvelope::CUtlDataEnvelope( const void *pData, intp nBytes )
{
	Assign( pData, nBytes );
}

inline CUtlDataEnvelope::CUtlDataEnvelope( const CUtlDataEnvelope &from )
{
	Assign( from );
}

inline CUtlDataEnvelope::CUtlDataEnvelope( CUtlDataEnvelope &&from ) noexcept
	: m_pData{std::move(from.m_pData)}, m_nBytes{std::move(from.m_nBytes)}
{
	from.m_pData = nullptr;
	from.m_nBytes = 0;
}

inline CUtlDataEnvelope::~CUtlDataEnvelope()
{
	Purge();
}

inline CUtlDataEnvelope &CUtlDataEnvelope::operator=( const CUtlDataEnvelope &from )
{
	Purge();
	Assign( from );
	return *this;
}

inline CUtlDataEnvelope &CUtlDataEnvelope::operator=( CUtlDataEnvelope &&from ) noexcept
{
	using std::swap;
	
	std::swap( m_pData, from.m_pData );
	std::swap( m_nBytes, from.m_nBytes );
	return *this;
}

inline CUtlDataEnvelope::operator void *()
{
	if ( !m_nBytes )
	{
		return nullptr;
	}

	return ( m_nBytes > ssize( m_data ) ) ? m_pData : m_data;
}

inline CUtlDataEnvelope::operator void *() const
{
	if ( !m_nBytes )
	{
		return nullptr;
	}

	return m_nBytes > ssize( m_data ) ? static_cast<void *>(m_pData) : static_cast<void *>(const_cast<byte*>(m_data));
}

//-----------------------------------------------------------------------------

template <typename T>
inline CUtlEnvelope<T>::CUtlEnvelope( const T *pData, intp nElems )
	: CUtlDataEnvelope( pData, sizeof(T) * nElems )
{
}

template <typename T>
inline CUtlEnvelope<T>::CUtlEnvelope( const CUtlEnvelope<T> &from )
	: CUtlDataEnvelope( from )
{
}

template <typename T>
inline CUtlEnvelope<T>::CUtlEnvelope( CUtlEnvelope<T> &&from ) noexcept
	: CUtlDataEnvelope( std::move( from ) )
{
}

template <typename T>
inline CUtlEnvelope<T> &CUtlEnvelope<T>::operator=( const CUtlEnvelope<T> &from )
{
	CUtlDataEnvelope::operator=( from );
	return *this;
}


template <typename T>
inline CUtlEnvelope<T> &CUtlEnvelope<T>::operator=( CUtlEnvelope<T> &&from ) noexcept
{
	CUtlDataEnvelope::operator=( std::move(from) );
	return *this;
}

template <typename T>
inline CUtlEnvelope<T>::operator T *()
{
	return (T *)CUtlDataEnvelope::operator void *();
}

template <typename T>
inline CUtlEnvelope<T>::operator T *() const
{
	return (T *)( (const_cast<CUtlEnvelope<T> *>(this))->operator T *() );
}

template <typename T>
inline CUtlEnvelope<T>::operator void *()
{
	return CUtlDataEnvelope::operator void *();
}

template <typename T>
inline CUtlEnvelope<T>::operator void *() const
{
	return ( (const_cast<CUtlEnvelope<T> *>(this))->operator void *() );
}

//-----------------------------------------------------------------------------

#include "tier0/memdbgoff.h"

#endif // UTLENVELOPE_H
