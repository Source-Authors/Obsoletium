// Copyright Valve Corporation, All rights reserved.
//

#ifndef SE_COMMON_WINDOWS_SCOPED_PROCESS_ERROR_MODE_H_
#define SE_COMMON_WINDOWS_SCOPED_PROCESS_ERROR_MODE_H_

extern "C" {

__declspec(dllimport) unsigned __stdcall GetErrorMode();
__declspec(dllimport) unsigned __stdcall SetErrorMode(unsigned new_mode);

}  // extern "C"

namespace se::common::windows {

// Scoped process error mode.
class ScopedProcessErrorMode {
 public:
  explicit ScopedProcessErrorMode(unsigned new_mode) noexcept
      : old_mode_{::SetErrorMode(::GetErrorMode() | new_mode)},
        new_mode_{new_mode} {}
  ~ScopedProcessErrorMode() noexcept {
    const unsigned current_mode{::GetErrorMode()};

    // Nobody changed mode between, reset.
    if (current_mode == new_mode_) ::SetErrorMode(old_mode_);
  }

  ScopedProcessErrorMode(const ScopedProcessErrorMode&) = delete;
  ScopedProcessErrorMode(ScopedProcessErrorMode&&) = delete;
  ScopedProcessErrorMode& operator=(const ScopedProcessErrorMode&) = delete;
  ScopedProcessErrorMode& operator=(ScopedProcessErrorMode&&) = delete;

 private:
  unsigned old_mode_, new_mode_;
};

}  // namespace se::common::windows

#endif  // !SE_COMMON_WINDOWS_SCOPED_PROCESS_ERROR_MODE_H_
