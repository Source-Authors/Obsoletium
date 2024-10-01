//========= Copyright Valve Corporation, All rights reserved. ============//
//

#ifndef ENGINE_SCOPED_THREAD_EXECUTION_STATE_H_
#define ENGINE_SCOPED_THREAD_EXECUTION_STATE_H_

#include <system_error>

#include "tier0/dbg.h"

namespace source::engine::win {

/**
 * @brief Enables an application to inform the system that it is in use, thereby
 * preventing the system from entering sleep or turning off the display while
 * the application is running.
 */
class ScopedThreadExecutionState {
 public:
  explicit ScopedThreadExecutionState(
      EXECUTION_STATE new_execution_state) noexcept
      : old_execution_state_{
            ::SetThreadExecutionState(new_execution_state | ES_CONTINUOUS)} {
    AssertMsg(old_execution_state_, "SetThreadExecutionState(%lu) failed.\n",
              new_execution_state | ES_CONTINUOUS);
  }
  ~ScopedThreadExecutionState() noexcept {
    if (old_execution_state_ != 0) {
      [[maybe_unused]] const EXECUTION_STATE new_execution_state{
          ::SetThreadExecutionState(old_execution_state_ | ES_CONTINUOUS)};
      AssertMsg(new_execution_state, "SetThreadExecutionState(%lu) failed.\n",
                old_execution_state_ | ES_CONTINUOUS);
    }
  }

  ScopedThreadExecutionState(ScopedThreadExecutionState &) = delete;
  ScopedThreadExecutionState(ScopedThreadExecutionState &&) = delete;
  ScopedThreadExecutionState &operator=(ScopedThreadExecutionState &) = delete;
  ScopedThreadExecutionState &operator=(ScopedThreadExecutionState &&) = delete;

  [[nodiscard]] bool is_success() const noexcept {
    return old_execution_state_ != 0;
  }

 private:
  const EXECUTION_STATE old_execution_state_;
};

}  // namespace source::engine::win

#endif  // ENGINE_SCOPED_THREAD_EXECUTION_STATE_H_
