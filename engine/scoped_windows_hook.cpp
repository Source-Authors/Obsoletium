//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Scoped windows hook.
//
//========================================================================//

#include "scoped_windows_hook.h"

#include <system_error>

#include "winlite.h"
#include "tier0/dbg.h"

namespace source::engine::win {

// Creates scoped windows hook by hook type, hook procedure, instance and thread id.
ScopedWindowsHook::ScopedWindowsHook( int hook_type,
	HOOKPROC hook_proc,
	HINSTANCE instance,
	unsigned long thread_id ) noexcept
	: hook_{ ::SetWindowsHookExW( hook_type, hook_proc, instance, thread_id ) },
	  errno_code_{ std::error_code{ static_cast<int>(::GetLastError()), std::system_category() } }
{
}

// Destroys windows hook.
ScopedWindowsHook::~ScopedWindowsHook() noexcept
{
	if ( !errno_code_ )
	{
		if ( !::UnhookWindowsHookEx(hook_) )
		{
			const std::string lastErrorText{ std::system_category().message( ::GetLastError() ) };
			Warning( "Can't remove hook %p: %s\n", hook_, lastErrorText.c_str() );
		}
	}
}

// Calls next hook with hook code, wide_param and low_param.
[[nodiscard]] LONG_PTR ScopedWindowsHook::CallNextHookEx( int hook_code,
	uintptr_t wide_param,
	LONG_PTR low_param ) {
	// Hook parameter is ignored, see
	// https://docs.microsoft.com/en-us/windows/desktop/api/winuser/nf-winuser-callnexthookex
	return ::CallNextHookEx( nullptr, hook_code, wide_param, low_param );
}

}  // namespace source::engine::win
