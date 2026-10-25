// Copyright Valve Corporation, All rights reserved.

#ifndef SE_COMMON_SCOPED_INVALID_PARAMETER_HANDLER_H_
#define SE_COMMON_SCOPED_INVALID_PARAMETER_HANDLER_H_

#include <cstdlib>
#include <tier0/dbg.h>

namespace s1::win
{

/**
 * @brief Registers an invalid-parameter handler for this object's lifetime.
 *
 * Restores the previously registered handler when the object is destroyed.
 */
class ScopedInvalidParameterHandler
{
public:
	explicit ScopedInvalidParameterHandler(
	_invalid_parameter_handler handler ) noexcept
		:
		old_handler_{ _set_invalid_parameter_handler( handler ) },
		new_handler_{ handler }
	{
	}

	~ScopedInvalidParameterHandler() noexcept
	{
		if ( _get_invalid_parameter_handler() == new_handler_ )
		{
			_set_invalid_parameter_handler( old_handler_ );
		}
		else
		{
			Warning(
			"Somebody already replaced _set_invalid_parameter_handler. Old "
			"handler not restored.\n" );
		}
	}

	ScopedInvalidParameterHandler( const ScopedInvalidParameterHandler & ) = delete;
	ScopedInvalidParameterHandler &operator=(
	const ScopedInvalidParameterHandler & ) = delete;
	ScopedInvalidParameterHandler( ScopedInvalidParameterHandler && ) = delete;
	ScopedInvalidParameterHandler &operator=( ScopedInvalidParameterHandler && ) =
	delete;

private:
	const _invalid_parameter_handler old_handler_, new_handler_;
};

} // namespace s1::win

#endif // SE_COMMON_SCOPED_INVALID_PARAMETER_HANDLER_H_
