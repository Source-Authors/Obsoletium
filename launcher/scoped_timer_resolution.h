// Copyright Valve Corporation, All rights reserved.

#ifndef SRC_LAUNCHER_SCOPED_TIMER_RESOLUTION_H
#define SRC_LAUNCHER_SCOPED_TIMER_RESOLUTION_H

#include <chrono>
#include <sal.h>

#pragma comment(lib, "winmm.lib")

#include "tier0/dbg.h"

typedef _Return_type_success_(return == 0) unsigned MMRESULT;

extern "C" {

__declspec(dllimport) MMRESULT __stdcall timeBeginPeriod(_In_ unsigned uPeriod);
__declspec(dllimport) MMRESULT __stdcall timeEndPeriod(_In_ unsigned uPeriod);

}  // extern "C"

namespace se::launcher {

// Changes minimum resolution for periodic timers and reverts back when
// out of scope.
//
// "Prior to Windows 10, version 2004, this function affects a global Windows
// setting.  For all processes Windows uses the lowest value (that is, highest
// resolution) requested by any process.  Starting with Windows 10, version
// 2004, this function no longer affects global timer resolution.  For processes
// which call this function, Windows uses the lowest value (that is, highest
// resolution) requested by any process.  For processes which have not called
// this function, Windows does not guarantee a higher resolution than the
// default system resolution.
//
// Starting with Windows 11, if a window-owning process becomes fully occluded,
// minimized, or otherwise invisible or inaudible to the end user, Windows does
// not guarantee a higher resolution than the default system resolution.  See
// SetProcessInformation for more information on this behavior.
//
// Setting a higher resolution can improve the accuracy of time-out intervals in
// wait functions.  However, it can also reduce overall system performance,
// because the thread scheduler switches tasks more often.  High resolutions can
// also prevent the CPU power management system from entering power-saving
// modes.  Setting a higher resolution does not improve the accuracy of the
// high-resolution performance counter."
//
// See
// https://docs.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod
// dimhotepus: Set timer resolution in a single place.
class ScopedTimerResolution {
 public:
  // Changes minimum resolution for periodic timers.  |resolution_ms| Minimum
  // timers resolution in milliseconds to request.
  explicit ScopedTimerResolution(
      std::chrono::milliseconds resolution_ms) noexcept
      : new_resolution_{resolution_ms},
        error_code_{
            ::timeBeginPeriod(static_cast<unsigned>(resolution_ms.count()))} {
    AssertMsg(!operator!(), "Unable to set windows timer resolution.");
  }

  // Restores previous minimum timer resolution.
  ~ScopedTimerResolution() noexcept {
    if (!operator!()) {
      [[maybe_unused]] const bool is_ok{
          ::timeEndPeriod(static_cast<unsigned>(new_resolution_.count())) == 0};
      AssertMsg(is_ok, "Unable to restore windows timer resolution.");
    }
  }

  // Is set minimum timers resolution succeeded?
  [[nodiscard]] bool operator!() const noexcept { return error_code_ != 0; }

 private:
  // New minimum timer resolution in ms.
  std::chrono::milliseconds new_resolution_;
  // Minimum timer resolution creation error_code.
  unsigned error_code_;
};

}  // namespace se::launcher

#endif  // SRC_LAUNCHER_SCOPED_TIMER_RESOLUTION_H
