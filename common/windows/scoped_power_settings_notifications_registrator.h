// Copyright Valve Corporation, All rights reserved.

#ifndef SE_COMMON_WINDOWS_SCOPED_POWER_SETTINGS_NOTIFICATIONS_REGISTRATOR_H_
#define SE_COMMON_WINDOWS_SCOPED_POWER_SETTINGS_NOTIFICATIONS_REGISTRATOR_H_

#include <system_error>

#include "tier0/dbg.h"

namespace s1::win {

/**
 * @brief Allows to register WM_POWERBROADCAST notifications for specific power
 * settings.
 */
class ScopedPowerSettingNotificationsRegistrator {
 public:
  ScopedPowerSettingNotificationsRegistrator(HWND recipient,
                                             const GUID& setting_guid) noexcept
      : handle_{::RegisterPowerSettingNotification(
            recipient, &setting_guid, DEVICE_NOTIFY_WINDOW_HANDLE)},
        rc_{handle_ ? std::error_code{}
                    : std::error_code{static_cast<int>(::GetLastError()),
                                      std::system_category()}} {}

  ScopedPowerSettingNotificationsRegistrator(
      const ScopedPowerSettingNotificationsRegistrator&) = delete;
  ScopedPowerSettingNotificationsRegistrator(
      ScopedPowerSettingNotificationsRegistrator&&) = delete;

  ScopedPowerSettingNotificationsRegistrator& operator=(
      const ScopedPowerSettingNotificationsRegistrator&) = delete;
  ScopedPowerSettingNotificationsRegistrator& operator=(
      ScopedPowerSettingNotificationsRegistrator&&) = delete;

  ~ScopedPowerSettingNotificationsRegistrator() noexcept {
    if (handle_) {
      [[maybe_unused]] const bool ok{
          ::UnregisterPowerSettingNotification(handle_) != FALSE};
#ifndef NDEBUG
      const auto message = std::error_code{static_cast<int>(::GetLastError()),
                                           std::system_category()}
                               .message();
      if (!ok) {
        Warning("UnregisterPowerSettingNotification failed w/e %s.\n",
                message.c_str());
      }
#endif
    }
  }

  [[nodiscard]] std::error_code error_code() const noexcept { return rc_; }

 private:
  HPOWERNOTIFY handle_;
  const std::error_code rc_;
};

}  // namespace s1::win

#endif  // !SE_COMMON_WINDOWS_SCOPED_POWER_SETTINGS_NOTIFICATIONS_REGISTRATOR_H_
