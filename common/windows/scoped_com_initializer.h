// Copyright Valve Corporation, All rights reserved.

#ifndef SE_COMMON_WINDOWS_SCOPED_COM_INITIALIZER_H_
#define SE_COMMON_WINDOWS_SCOPED_COM_INITIALIZER_H_

#include <sal.h>
#include <objbase.h>

#include "com_error_category.h"
#include "tier0/commonmacros.h"

using HRESULT = _Return_type_success_( return >= 0 ) long;

extern "C"
{
__declspec( dllimport ) _Check_return_ HRESULT __stdcall CoInitializeEx(
_In_opt_ void *pvReserved, _In_ unsigned long dwCoInit );
__declspec( dllimport ) void __stdcall CoUninitialize();

} // extern "C"

namespace s1::win
{

/**
 * @brief Initializes COM for this scope and uninitializes it on successful
 * setup.  Construct and destroy this object on the same thread.
 */
class ScopedComInitializer
{
public:
	explicit ScopedComInitializer( COINIT com_type ) noexcept
		:
		error_code_{
			static_cast< int >( CoInitializeEx( nullptr, to_underlying( com_type ) ) ),
			s1::win::com::com_error_category()
		}
	{
	}

	ScopedComInitializer( const ScopedComInitializer & ) = delete;
	ScopedComInitializer( ScopedComInitializer && ) = delete;
	ScopedComInitializer &operator=( const ScopedComInitializer & ) = delete;
	ScopedComInitializer &operator=( ScopedComInitializer && ) = delete;

	[[nodiscard]] std::error_code error_code() const noexcept
	{
		return error_code_;
	}

	~ScopedComInitializer() noexcept
	{
		if ( !error_code_ )
			CoUninitialize();
	}

private:
	const std::error_code error_code_;
};

} // namespace s1::win

#endif // !SE_COMMON_WINDOWS_SCOPED_COM_INITIALIZER_H_
