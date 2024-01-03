//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Toggles left/right Windows keys.
// See https://docs.microsoft.com/en-us/windows/desktop/dxtecharts/disabling-shortcut-keys-in-games#disable-the-windows-key-with-a-keyboard-hook
//
//========================================================================//

#ifndef ENGINE_WINDOWS_SHORTCUT_KEYS_TOGGLER_H_
#define ENGINE_WINDOWS_SHORTCUT_KEYS_TOGGLER_H_
#ifdef _WIN32
#pragma once
#endif

#include <cstdint>
#include <system_error>

#include "scoped_windows_hook.h"

FORWARD_DECLARE_HANDLE(HINSTANCE);

#if defined(_WIN64)
using LONG_PTR = __int64;
#else
using LONG_PTR = _W64 long;
#endif

namespace source::engine::win {

// Should we enable window shortcut keys or not.
using ShouldEnableWindowsShortcutKeysFunction = bool (*)() noexcept;

// Toggles left/right Windows keys.  See
// https://docs.microsoft.com/en-us/windows/desktop/dxtecharts/disabling-shortcut-keys-in-games#disable-the-windows-key-with-a-keyboard-hook
class WindowsShortcutKeysToggler
{
public:
	// Creates Windows keys toggler by hooking module instance and function to control Windows keys toggle.
	WindowsShortcutKeysToggler( HINSTANCE hook_module,
		ShouldEnableWindowsShortcutKeysFunction should_enable_windows_shortcut_keys_fn ) noexcept;
	~WindowsShortcutKeysToggler() noexcept = default;

	WindowsShortcutKeysToggler( WindowsShortcutKeysToggler& ) = delete;
	WindowsShortcutKeysToggler( WindowsShortcutKeysToggler&& ) = delete;
	WindowsShortcutKeysToggler& operator=(WindowsShortcutKeysToggler& ) = delete;
	WindowsShortcutKeysToggler& operator=(WindowsShortcutKeysToggler&& ) = delete;

	// Gets toggler initialization errno code.
	[[nodiscard]] std::error_code errno_code() const noexcept
	{
		return keyboard_hook_.errno_code();
	}

private:
	// Keyboard hook to catch key up/down events.
	const ScopedWindowsHook keyboard_hook_;
	// Callback containing needed info to apply keyboard hooks.
	static ShouldEnableWindowsShortcutKeysFunction should_enable_windows_shortcut_keys_fn_;

	// Low level keyboard hook procedure.  Performs conditional Windows key filtering.
	static LONG_PTR __stdcall LowLevelKeyboardProc( int hook_code,
		uintptr_t wide_param,
		LONG_PTR low_param );
};

}  // namespace source::engine::win

#endif  // ENGINE_WINDOWS_SHORTCUT_KEYS_TOGGLER_H_
