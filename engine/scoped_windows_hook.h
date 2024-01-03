//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Scoped windows hook.
//
//========================================================================//

#ifndef ENGINE_SCOPED_WINDOWS_HOOK_H_
#define ENGINE_SCOPED_WINDOWS_HOOK_H_
#ifdef _WIN32
#pragma once
#endif

#include <cstdint>
#include <system_error>

#include "tier0/dbg.h"

FORWARD_DECLARE_HANDLE(HHOOK);
FORWARD_DECLARE_HANDLE(HINSTANCE);

#if defined(_WIN64)
using LONG_PTR = __int64;
#else
using LONG_PTR = _W64 long;
#endif

using HOOKPROC = LONG_PTR(__stdcall* )( int code, uintptr_t wParam, LONG_PTR lParam );

namespace source::engine::win {

// Scoped windows hook.
class ScopedWindowsHook
{
public:
	// Creates scoped windows hook by hook type, hook procedure, instance and thread id.
	ScopedWindowsHook( int hook_type,
		HOOKPROC hook_proc,
		HINSTANCE instance,
		unsigned long thread_id ) noexcept;
	~ScopedWindowsHook() noexcept;

	ScopedWindowsHook( ScopedWindowsHook& ) = delete;
	ScopedWindowsHook( ScopedWindowsHook&& ) = delete;
	ScopedWindowsHook& operator=( ScopedWindowsHook& ) = delete;
	ScopedWindowsHook& operator=( ScopedWindowsHook&& ) = delete;

	// Hook initialization errno code.
	[[nodiscard]] std::error_code errno_code() const noexcept {
		return errno_code_;
	}

	// Calls next hook with hook code, wide_param and low_param.
	[[nodiscard]] static LONG_PTR CallNextHookEx( int hook_code,
		uintptr_t wide_param,
		LONG_PTR low_param );

private:
	// Hook.
	HHOOK hook_;
	// Hook initialization error code.
	const std::error_code errno_code_;
};

}  // namespace source::engine::win

#endif  // ENGINE_SCOPED_WINDOWS_HOOK_H_
