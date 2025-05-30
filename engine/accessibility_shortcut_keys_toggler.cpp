//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Toggles accessibility shortcut keys.
// See
// https://docs.microsoft.com/en-us/windows/desktop/dxtecharts/disabling-shortcut-keys-in-games#disable-the-accessibility-shortcut-keys
//
//========================================================================//

#include "accessibility_shortcut_keys_toggler.h"

#include <optional>
#include <type_traits>
#include <system_error>

#include "winlite.h"
#include "tier0/dbg.h"

namespace {

template<typename TSystemKey, typename TResult>
using is_system_key_t =
    std::enable_if_t<std::disjunction_v<std::is_same<TSystemKey, STICKYKEYS>,
                                        std::is_same<TSystemKey, TOGGLEKEYS>,
                                        std::is_same<TSystemKey, FILTERKEYS>>, TResult>;

template <typename TSystemKey>
is_system_key_t<TSystemKey, void> SystemKeysInfo(_In_ UINT action, _In_ TSystemKey &key) noexcept
{
  const BOOL is_succeeded{
      ::SystemParametersInfoW(action, sizeof(key), &key, 0)};

  if (!is_succeeded)
  {
    const auto lastErrorText = std::system_category().message(::GetLastError());

    Warning(
        "Can't toggle accessibility shortcut keys, please, do not press them "
        "in the game: %s\n",
        lastErrorText.c_str() );
  }
}

}  // namespace

namespace source::engine::win {

// Toggles accessibility shortcut keys.  See
// https://docs.microsoft.com/en-us/windows/desktop/dxtecharts/disabling-shortcut-keys-in-games#disable-the-accessibility-shortcut-keys
class AccessibilityShortcutKeysToggler::Impl
{
public:
	// Creates accessibility shortcut keys toggler.
	Impl() noexcept
			: is_toggled_{},
			startup_sticky_keys_{sizeof(decltype(startup_sticky_keys_)), 0},
			startup_toggle_keys_{sizeof(decltype(startup_toggle_keys_)), 0},
			startup_filter_keys_{sizeof(decltype(startup_filter_keys_)), 0, 0, 0, 0, 0}
	{
		// Save the current sticky/toggle/filter key settings so they can be
		// restored them later.
		SystemKeysInfo(SPI_GETSTICKYKEYS, startup_sticky_keys_);
		SystemKeysInfo(SPI_GETTOGGLEKEYS, startup_toggle_keys_);
		SystemKeysInfo(SPI_GETFILTERKEYS, startup_filter_keys_);
	}

	Impl(const Impl &) = delete;
	Impl(Impl &&) = delete;
	Impl &operator=(const Impl &) = delete;
	Impl &operator=(Impl &&) = delete;

	// Restores accessibility shortcut keys to original system state.
	~Impl() noexcept { Toggle(true); }

	// Toggle accessibility shortcut keys or not.
	void Toggle( bool toggle ) noexcept;

private:
    // Small wrapper for better semantics.
    using nullable_bool = std::optional<bool>;

    nullable_bool is_toggled_;

    STICKYKEYS startup_sticky_keys_;
    TOGGLEKEYS startup_toggle_keys_;
    FILTERKEYS startup_filter_keys_;
};

void AccessibilityShortcutKeysToggler::Impl::Toggle( bool toggle ) noexcept
{
  if (is_toggled_.has_value() && *is_toggled_ == toggle) return;

  if (toggle)
  {
    // Restore StickyKeys/etc to original state and enable Windows key.
    SystemKeysInfo(SPI_SETSTICKYKEYS, startup_sticky_keys_);
    SystemKeysInfo(SPI_SETTOGGLEKEYS, startup_toggle_keys_);
    SystemKeysInfo(SPI_SETFILTERKEYS, startup_filter_keys_);
  }
  else
  {
    // Disable StickyKeys/etc shortcuts but if the accessibility feature is
    // on, then leave the settings alone as its probably being usefully used.

    STICKYKEYS sticky_keys_off = startup_sticky_keys_;
    if ((sticky_keys_off.dwFlags & SKF_STICKYKEYSON) == 0)
    {
      // Disable the hotkey and the confirmation.
      sticky_keys_off.dwFlags &= ~SKF_HOTKEYACTIVE;
      sticky_keys_off.dwFlags &= ~SKF_CONFIRMHOTKEY;

      SystemKeysInfo(SPI_SETSTICKYKEYS, sticky_keys_off);
    }

    TOGGLEKEYS toggle_keys_off = startup_toggle_keys_;
    if ((toggle_keys_off.dwFlags & TKF_TOGGLEKEYSON) == 0)
    {
      // Disable the hotkey and the confirmation.
      toggle_keys_off.dwFlags &= ~TKF_HOTKEYACTIVE;
      toggle_keys_off.dwFlags &= ~TKF_CONFIRMHOTKEY;

      SystemKeysInfo(SPI_SETTOGGLEKEYS, toggle_keys_off);
    }

    FILTERKEYS filter_keys_off = startup_filter_keys_;
    if ((filter_keys_off.dwFlags & FKF_FILTERKEYSON) == 0)
    {
      // Disable the hotkey and the confirmation.
      filter_keys_off.dwFlags &= ~FKF_HOTKEYACTIVE;
      filter_keys_off.dwFlags &= ~FKF_CONFIRMHOTKEY;

      SystemKeysInfo(SPI_SETFILTERKEYS, filter_keys_off);
    }
  }

  is_toggled_ = toggle;
}

AccessibilityShortcutKeysToggler::AccessibilityShortcutKeysToggler() noexcept
    : impl_{ std::make_unique<Impl>() }
{
}

// Restores accessibility shortcut keys to original system state.
AccessibilityShortcutKeysToggler::~AccessibilityShortcutKeysToggler() noexcept
{
  Toggle(true);
}

void AccessibilityShortcutKeysToggler::Toggle( bool toggle ) noexcept
{
    impl_->Toggle( toggle );
}

}  // namespace source::engine::win
