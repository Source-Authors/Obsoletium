//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Toggles accessibility shortcut keys.
// See https://docs.microsoft.com/en-us/windows/desktop/dxtecharts/disabling-shortcut-keys-in-games#disable-the-accessibility-shortcut-keys
//
//========================================================================//

#ifndef ENGINE_ACCESSIBILITY_SHORTCUT_KEYS_TOGGLER_H_
#define ENGINE_ACCESSIBILITY_SHORTCUT_KEYS_TOGGLER_H_
#ifdef _WIN32
#pragma once
#endif

#include <memory>

namespace source::engine::win {

// Toggles accessibility shortcut keys.  See
// https://docs.microsoft.com/en-us/windows/desktop/dxtecharts/disabling-shortcut-keys-in-games#disable-the-accessibility-shortcut-keys
class AccessibilityShortcutKeysToggler
{
public:
	// Creates accessibility shortcut keys toggler.
	AccessibilityShortcutKeysToggler() noexcept;

	AccessibilityShortcutKeysToggler( const AccessibilityShortcutKeysToggler & ) = delete;
	AccessibilityShortcutKeysToggler( AccessibilityShortcutKeysToggler && ) = delete;
	AccessibilityShortcutKeysToggler &operator=( const AccessibilityShortcutKeysToggler & ) = delete;
	AccessibilityShortcutKeysToggler &operator=( AccessibilityShortcutKeysToggler && ) = delete;

	// Restores accessibility shortcut keys to original system state.
	~AccessibilityShortcutKeysToggler() noexcept;

	// Toggle accessibility shortcut keys or not.
	void Toggle( bool toggle ) noexcept;

private:
	class Impl;

	std::unique_ptr<Impl> impl_;
};

}  // namespace source::engine::win

#endif  // ENGINE_ACCESSIBILITY_SHORTCUT_KEYS_TOGGLER_H_
