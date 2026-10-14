// Copyright Valve Corporation, All rights reserved.

#ifndef SE_COMMON_WINDOWS_SCOPED_THREAD_EXECUTION_STATE_H_
#define SE_COMMON_WINDOWS_SCOPED_THREAD_EXECUTION_STATE_H_

#include "tier0/annotations.h"
#include "tier0/commonmacros.h"
#include "tier0/dbg.h"

using EXECUTION_STATE = unsigned long;

extern "C" __declspec(dllimport) EXECUTION_STATE __stdcall
SetThreadExecutionState(IN_P EXECUTION_STATE esFlags);

namespace s1::win {

/**
 * @brief Thread execution requirements.
 */
enum class ThreadExecutionState : unsigned long {
  /**
   * @brief Error if set thread execution state is failed.
   */
  Error = 0x00000000,
  /**
   * @brief Informs the system that the state being set should remain in effect
   * until the next call that uses Continuous and one of the other state flags
   * is cleared.
   */
  Continuous = 0x80000000,
  /**
   * @brief Forces the system to be in the working state by resetting the system
   * idle timer.
   */
  SystemRequired = 0x00000001,
  /**
   * @brief Forces the display to be on by resetting the display idle timer.
   */
  DisplayRequired = 0x00000002,
  /**
   * @brief Enables away mode.  This value must be specified with Continuous.
   * Away mode should be used only by media-recording and media-distribution
   * applications that must perform critical background processing on desktop
   * computers while the computer appears to be sleeping.  See
   * https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setthreadexecutionstate#remarks
   */
  AwayModeRequired = 0x00000040
};

[[nodiscard]] constexpr inline ThreadExecutionState operator|(
    ThreadExecutionState left, ThreadExecutionState right) noexcept {
  return static_cast<ThreadExecutionState>(to_underlying(left) |
                                           to_underlying(right));
}

[[nodiscard]] constexpr inline ThreadExecutionState operator&(
    ThreadExecutionState left, ThreadExecutionState right) noexcept {
  return static_cast<ThreadExecutionState>(to_underlying(left) &
                                           to_underlying(right));
}

/**
 * @brief Scoped enablement for an application to inform the system that it is
 * in use, thereby preventing the system from entering sleep or turning off the
 * display while the application is running.
 *
 * It cannot be used to prevent the user from putting the computer to sleep.  It
 * does not stop the screen saver from executing.
 */
class ScopedThreadExecutionState {
 public:
  explicit ScopedThreadExecutionState(ThreadExecutionState new_state) noexcept
      : old_state_{static_cast<ThreadExecutionState>(::SetThreadExecutionState(
            to_underlying(ThreadExecutionState::Continuous | new_state)))},
        new_state_{new_state} {
    AssertMsg(old_state_ != ThreadExecutionState::Error,
              "SetThreadExecutionState(0x%lx) failed.",
              to_underlying(new_state));
  }
  ~ScopedThreadExecutionState() noexcept {
    if (old_state_ != ThreadExecutionState::Error) {
      const auto expected_old_state =
          to_underlying(ThreadExecutionState::Continuous | new_state_);
      const auto actual_old_state = ::SetThreadExecutionState(
          to_underlying(ThreadExecutionState::Continuous | old_state_));
      AssertMsg(actual_old_state == expected_old_state,
                "Unbalanced SetThreadExecutionState, smth changed it from "
                "expected 0x%lu to actual 0x%lu.",
                expected_old_state, actual_old_state);
    }
  }

  [[nodiscard]] bool is_success() const noexcept {
    return old_state_ != ThreadExecutionState::Error;
  }

  ScopedThreadExecutionState(ScopedThreadExecutionState&) = delete;
  ScopedThreadExecutionState& operator=(ScopedThreadExecutionState&) = delete;
  ScopedThreadExecutionState(ScopedThreadExecutionState&&) = delete;
  ScopedThreadExecutionState& operator=(ScopedThreadExecutionState&&) = delete;

 private:
  const ThreadExecutionState old_state_, new_state_;
};

}  // namespace s1::win

#endif  // !SE_COMMON_WINDOWS_SCOPED_THREAD_EXECUTION_STATE_H_
