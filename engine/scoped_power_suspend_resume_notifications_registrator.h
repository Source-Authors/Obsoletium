//========= Copyright Valve Corporation, All rights reserved. ============//
//

#ifndef ENGINE_SCOPED_POWER_SUSPEND_RESUME_NOTIFICATIONS_REGISTRATOR_H_
#define ENGINE_SCOPED_POWER_SUSPEND_RESUME_NOTIFICATIONS_REGISTRATOR_H_

#include <system_error>

#include "tier0/dbg.h"

namespace source::engine::win {

/**
 * @brief Allows to register WM_POWERBROADCAST notifications for specific power
 * settings.
 */
class ScopedPowerSuspendResumeNotificationsRegistrator {
 public:
  ScopedPowerSuspendResumeNotificationsRegistrator(HWND recipient) noexcept
      : handle_{::RegisterSuspendResumeNotification(
            recipient, DEVICE_NOTIFY_WINDOW_HANDLE)},
        rc_{handle_ ? std::error_code{}
                    : std::error_code{static_cast<int>(::GetLastError()),
                                      std::system_category()}} {}

  ScopedPowerSuspendResumeNotificationsRegistrator(
      const ScopedPowerSuspendResumeNotificationsRegistrator &) = delete;
  ScopedPowerSuspendResumeNotificationsRegistrator(
      ScopedPowerSuspendResumeNotificationsRegistrator &&) = delete;

  ScopedPowerSuspendResumeNotificationsRegistrator &operator=(
      const ScopedPowerSuspendResumeNotificationsRegistrator &) = delete;
  ScopedPowerSuspendResumeNotificationsRegistrator &operator=(
      ScopedPowerSuspendResumeNotificationsRegistrator &&) = delete;

  ~ScopedPowerSuspendResumeNotificationsRegistrator() noexcept {
    if (handle_) {
      [[maybe_unused]] const bool ok{
          ::UnregisterSuspendResumeNotification(handle_) != FALSE};
#ifdef _DEBUG
      const auto message = std::error_code{static_cast<int>(::GetLastError()),
                                           std::system_category()}
                               .message();
      AssertMsg(ok, "UnregisterSuspendResumeNotification failed w/e %s.\n",
                message.c_str());
#endif
    }
  }

  [[nodiscard]] std::error_code error_code() const noexcept { return rc_; }

 private:
  HPOWERNOTIFY handle_;
  const std::error_code rc_;
};

}  // namespace source::engine::win

#endif  // ENGINE_SCOPED_POWER_SUSPEND_RESUME_NOTIFICATIONS_REGISTRATOR_H_
