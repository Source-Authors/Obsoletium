// Copyright Valve Corporation, All rights reserved.

#ifndef SE_COMMON_WINDOWS_SCOPED_FLOATING_EXCEPTION_POINT_MODE_H_
#define SE_COMMON_WINDOWS_SCOPED_FLOATING_EXCEPTION_POINT_MODE_H_

#include <float.h>

#include <cfenv>
#include <cerrno>  // errno codes
#include <cstring> // strerror, std::memcmp

#include "tier0/annotations.h"
#include "tier0/commonmacros.h"
#include "tier0/dbg.h"

namespace s1::win
{

/**
 * @brief Floating-point exceptions to enable within a scope.
 */
enum class FloatingPointExceptionMode : unsigned
{
	/** @brief Select no exception types. Not supported by the scoped mode. */
	None = 0x0,
	/** @brief Enable the inexact-result exception. */
	Inexact = _EM_INEXACT,
	/** @brief Enable the underflow exception. */
	Underflow = _EM_UNDERFLOW,
	/** @brief Enable the overflow exception. */
	Overflow = _EM_OVERFLOW,
	/** @brief Enable the division-by-zero exception. */
	ZeroDivide = _EM_ZERODIVIDE,
	/** @brief Enable the invalid-operation exception. */
	Invalid = _EM_INVALID,
	/** @brief Enable all exception types supported by us. */
	All =
	_EM_INEXACT | _EM_UNDERFLOW | _EM_OVERFLOW | _EM_ZERODIVIDE | _EM_INVALID
};

[[nodiscard]] constexpr FloatingPointExceptionMode operator|(
FloatingPointExceptionMode left,
FloatingPointExceptionMode right ) noexcept
{
	return static_cast< FloatingPointExceptionMode >( to_underlying( left ) |
	                                                  to_underlying( right ) );
}

[[nodiscard]] constexpr FloatingPointExceptionMode operator&(
FloatingPointExceptionMode left,
FloatingPointExceptionMode right ) noexcept
{
	return static_cast< FloatingPointExceptionMode >( to_underlying( left ) &
	                                                  to_underlying( right ) );
}

/**
 * @brief Clears float environment exception flags while the scope is active,
 * then restores the original environment on exit.
 */
class ScopedFloatingPointExceptionMode
{
public:
	explicit ScopedFloatingPointExceptionMode(
	FloatingPointExceptionMode new_mode ) noexcept
		:
		old_saved_env_{},
		rc_{ 0 }
	{
		// It is ok, we just skip if none for brevity.
		if ( new_mode == FloatingPointExceptionMode::None )
		{
			rc_ = ENOTSUP;
			return;
		}

		if ( ( to_underlying( new_mode ) &
		       ~to_underlying( FloatingPointExceptionMode::All ) ) != 0 )
		{
			Warning( "Unknown floating point exception mode 0x%x.\n",
			         to_underlying( new_mode ) );
			rc_ = EINVAL;
			return;
		}

		if ( rc_ = std::fegetenv( &old_saved_env_ ); rc_ != 0 )
		{
			Warning( "fegetenv failed w/e %d.\n", rc_ );
			return;
		}

		// Clear pending flags before enabling traps. If anything fails below,
		// restore the original environment before returning.
		if ( rc_ = std::feclearexcept( FE_ALL_EXCEPT ); rc_ != 0 )
		{
			Warning( "feclearexcept(FE_ALL_EXCEPT) failed w/e %d.\n", rc_ );

			rc_ = std::fesetenv( &old_saved_env_ );
			if ( rc_ )
			{
				Warning( "fesetenv failed w/e %d. Old fenv may not be restored.\n", rc_ );
			}
			rc_ = EINVAL;

			return;
		}

		// Zero out the specified bits, leaving other bits alone.
		rc_ = _controlfp_s( nullptr, ~to_underlying( new_mode ),
		                    to_underlying( new_mode ) );
		if ( rc_ )
		{
			Warning( "_controlfp_s(0x%x) failed w/e %s.\n", to_underlying( new_mode ),
			         strerror( rc_ ) );

			if ( rc_ = std::fesetenv( &old_saved_env_ ); rc_ != 0 )
			{
				Warning( "fesetenv failed w/e %d. Old fenv may not be restored.\n", rc_ );
			}
		}
	}

	~ScopedFloatingPointExceptionMode() noexcept
	{
		if ( rc_ )
			return;

		if ( const int rc{ std::fesetenv( &old_saved_env_ ) } )
		{
			Warning( "fesetenv failed w/e %d. Old fenv may not be restored.\n", rc );
		}
	}

	ScopedFloatingPointExceptionMode( ScopedFloatingPointExceptionMode & ) = delete;
	ScopedFloatingPointExceptionMode &operator=(
	ScopedFloatingPointExceptionMode & ) = delete;
	ScopedFloatingPointExceptionMode( ScopedFloatingPointExceptionMode && ) = delete;
	ScopedFloatingPointExceptionMode &operator=(
	ScopedFloatingPointExceptionMode && ) = delete;

private:
	std::fenv_t old_saved_env_;
	int         rc_;
};

} // namespace s1::win

#endif // !SE_COMMON_WINDOWS_SCOPED_FLOATING_EXCEPTION_POINT_MODE_H_
