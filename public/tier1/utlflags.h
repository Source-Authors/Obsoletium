#ifndef UTLFLAGS_H
#define UTLFLAGS_H

#include "tier0/dbg.h"

#include <limits>
#include <type_traits>

//-----------------------------------------------------------------------------
// Simple class to make it easier to deal with flags
//-----------------------------------------------------------------------------
template< class T >
class CUtlFlags
{
	static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>,
		"CUtlFlags requires a non-bool integral type");

public:
	explicit CUtlFlags( int nInitialFlags = 0 )
		: m_nFlags{}
	{
		T flags{};
		// dimhotepus: Check int -> T works.
		if ( !TryConvert( nInitialFlags, flags ) )
		{
			Assert(false);
			return;
		}
		m_nFlags = flags;
	}

	void SetFlag( int nFlagMask )
	{
		T mask{};
		// dimhotepus: Check int -> T works.
		if ( !TryConvert( nFlagMask, mask ) )
		{
			Assert(false);
			return;
		}
		m_nFlags = static_cast<T>( m_nFlags | mask );
	}

	void SetFlag( int nFlagMask, bool bEnable )
	{
		T mask{};
		// dimhotepus: Check int -> T works.
		if ( !TryConvert( nFlagMask, mask ) )
		{
			Assert(false);
			return;
		}

		if ( bEnable )
			m_nFlags = static_cast<T>( m_nFlags | mask );
		else
			m_nFlags = static_cast<T>( m_nFlags & static_cast<T>( ~mask ) );
	}

	void ClearFlag( int nFlagMask )
	{
		T mask{};
		// dimhotepus: Check int -> T works.
		if ( !TryConvert( nFlagMask, mask ) )
		{
			Assert(false);
			return;
		}
		m_nFlags = static_cast<T>( m_nFlags & static_cast<T>( ~mask ) );
	}

	void ClearAllFlags()
	{
		m_nFlags = T{};
	}

	// true if any requested bit is set.
	[[nodiscard]] bool IsFlagSet( int nFlagMask ) const
	{
		T mask{};
		// dimhotepus: Check int -> T works.
		if ( !TryConvert( nFlagMask, mask ) )
		{
			Assert(false);
			return false;
		}
		return (m_nFlags & mask) != 0;
	}

	// True only if every requested bit is set. A zero mask is vacuously true.
	[[nodiscard]] bool AreAllFlagsSet( int nFlagMask ) const
	{
		T mask{};
		// dimhotepus: Check int -> T works.
		if ( !TryConvert( nFlagMask, mask ) )
		{
			Assert(false);
			return false;
		}
		return (m_nFlags & mask) == mask;
	}

	[[nodiscard]] bool IsAnyFlagSet() const
	{
		return m_nFlags != T{};
	}

private:
	// dimhotepus: Check int -> T works.
	[[nodiscard]] static bool TryConvert( int value, T &result )
	{
		if constexpr (std::is_signed_v<T>)
		{
			if constexpr (std::numeric_limits<T>::digits <
						  std::numeric_limits<int>::digits)
			{
				if ( value < static_cast<int>( std::numeric_limits<T>::min() ) ||
					 value > static_cast<int>( std::numeric_limits<T>::max() ) )
					return false;
			}
		}
		else
		{
			if ( value < 0 )
				return false;

			if constexpr (std::numeric_limits<T>::digits <
						  std::numeric_limits<int>::digits)
			{
				if ( value > static_cast<int>( std::numeric_limits<T>::max() ) )
					return false;
			}
		}

		result = static_cast<T>( value );
		return true;
	}

	T m_nFlags;
};

#endif // UTLFLAGS_H
