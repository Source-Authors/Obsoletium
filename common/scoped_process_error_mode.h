// Copyright Valve Corporation, All rights reserved.
//

#ifndef SRC_COMMON_SCOPED_PROCESS_ERROR_MODE_H_
#define SRC_COMMON_SCOPED_PROCESS_ERROR_MODE_H_

#ifdef _WIN32

extern "C" {

__declspec(dllimport) unsigned __stdcall GetErrorMode();
__declspec(dllimport) unsigned __stdcall SetErrorMode(unsigned new_mode);

}  // extern "C"

#endif

namespace source {

// Scoped process error mode.
class ScopedProcessErrorMode {
 public:
  ScopedProcessErrorMode(unsigned new_mode) noexcept
      : old_mode_{::SetErrorMode(::GetErrorMode() | new_mode)},
        new_mode_{new_mode} {}

  ScopedProcessErrorMode(const ScopedProcessErrorMode &) = delete;
  ScopedProcessErrorMode(ScopedProcessErrorMode &&) = delete;
  ScopedProcessErrorMode &operator=(const ScopedProcessErrorMode &) = delete;
  ScopedProcessErrorMode &operator=(ScopedProcessErrorMode &&) = delete;

  ~ScopedProcessErrorMode() noexcept {
    unsigned current_mode{::GetErrorMode()};

    // Nobody changed mode between, reset.
    if (current_mode == new_mode_) ::SetErrorMode(old_mode_);
  }

 private:
  unsigned old_mode_, new_mode_;
};

}  // namespace source

#endif  // !SRC_COMMON_SCOPED_PROCESS_ERROR_MODE_H_
