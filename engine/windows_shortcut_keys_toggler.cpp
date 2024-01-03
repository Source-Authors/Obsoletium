//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Toggles left/right Windows keys.
// See https://docs.microsoft.com/en-us/windows/desktop/dxtecharts/disabling-shortcut-keys-in-games#disable-the-windows-key-with-a-keyboard-hook
//
//========================================================================//

#include "windows_shortcut_keys_toggler.h"

#include "winlite.h"
#include "tier0/dbg.h"

namespace source::engine::win {

// Creates Windows keys toggler by hooking module instance and function to control Windows keys toggle.
WindowsShortcutKeysToggler::WindowsShortcutKeysToggler(
	HINSTANCE hook_module,
	ShouldEnableWindowsShortcutKeysFunction should_enable_windows_shortcut_keys_fn ) noexcept
		: keyboard_hook_{ WH_KEYBOARD_LL, &LowLevelKeyboardProc, hook_module, 0 }
	{
		Assert(!!should_enable_windows_shortcut_keys_fn);

		should_enable_windows_shortcut_keys_fn_ = should_enable_windows_shortcut_keys_fn;
	}

// Low level keyboard hook procedure.  Performs conditional Windows key filtering.
LONG_PTR CALLBACK WindowsShortcutKeysToggler::LowLevelKeyboardProc( int hook_code,
	uintptr_t wide_param,
	LONG_PTR low_param )
{
	if (hook_code < 0 || hook_code != HC_ACTION)
	{
		// Do not process message.
		return ScopedWindowsHook::CallNextHookEx( hook_code, wide_param, low_param );
	}

	bool should_skip_key{ false };
	auto* keyboard_dll_hooks = reinterpret_cast<KBDLLHOOKSTRUCT*>( low_param );

	switch (wide_param)
	{
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			should_skip_key = (!should_enable_windows_shortcut_keys_fn_() &&
				((keyboard_dll_hooks->vkCode == VK_LWIN) ||
					(keyboard_dll_hooks->vkCode == VK_RWIN)));
			break;
		}
	}

	// Return 1 to ensure keyboard keys are handled and don't popup further.
	return !should_skip_key
		? ScopedWindowsHook::CallNextHookEx( hook_code, wide_param, low_param )
		: 1L;
}

ShouldEnableWindowsShortcutKeysFunction
	WindowsShortcutKeysToggler::should_enable_windows_shortcut_keys_fn_ =
	nullptr;

}  // namespace source::engine::win
