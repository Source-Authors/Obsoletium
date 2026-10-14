// Copyright Valve Corporation, All rights reserved.

#ifndef SE_COMMON_WINDOWS_SCOPED_CONSOLE_CTRL_HANDLER_H_
#define SE_COMMON_WINDOWS_SCOPED_CONSOLE_CTRL_HANDLER_H_

#include "tier0/annotations.h"
#include "tier0/basetypes.h"  // TRUE / FALSE
#include "tier0/dbg.h"

using ConsoleCtrlHandlerFn = int(__stdcall*)(IN_P unsigned long CtrlType);

extern "C" __declspec(dllimport) int __stdcall SetConsoleCtrlHandler(
    IN_OPT ConsoleCtrlHandlerFn HandlerRoutine, IN_P int Add);

namespace s1::win {

/**
 * @brief Adds or removes an application-defined function from the list of
 * handler functions for the calling process.  If no handler function is
 * specified, the function sets an inheritable attribute that determines whether
 * the calling process ignores CTRL+C signals.
 */
class ScopedConsoleCtrlHandler {
 public:
  /**
   * @brief Ctor.
   * @param handler If nullptr, causes the calling process to ignore CTRL+C
   * input and restores normal processing of CTRL+C input on out of scope.  This
   * attribute of ignoring or processing CTRL+C is inherited by child processes.
   * Note handler should be thread-safe as invoked in a new thread.
   */
  explicit ScopedConsoleCtrlHandler(
      IN_OPT ConsoleCtrlHandlerFn handler) noexcept
      : handler_{handler},
        is_success_{::SetConsoleCtrlHandler(handler, TRUE) != FALSE} {
    AssertMsg(is_success_,
              "Unable to set console ctrl handler 0x%p for process.",
              static_cast<void*>(handler));
  }
  ~ScopedConsoleCtrlHandler() noexcept {
    if (is_success_) {
      [[maybe_unused]] const bool ok{::SetConsoleCtrlHandler(handler_, FALSE) !=
                                     FALSE};
      AssertMsg(ok,
                "Unable to reset console ctrl handler from 0x%p for process.",
                static_cast<void*>(handler_));
    }
  }

  [[nodiscard]] bool is_success() const noexcept { return is_success_; }

  ScopedConsoleCtrlHandler(ScopedConsoleCtrlHandler&) = delete;
  ScopedConsoleCtrlHandler& operator=(ScopedConsoleCtrlHandler&) = delete;
  ScopedConsoleCtrlHandler(ScopedConsoleCtrlHandler&&) = delete;
  ScopedConsoleCtrlHandler& operator=(ScopedConsoleCtrlHandler&&) = delete;

 private:
  const ConsoleCtrlHandlerFn handler_;
  const bool is_success_;
};

}  // namespace s1::win

#endif  // !SE_COMMON_WINDOWS_SCOPED_CONSOLE_CTRL_HANDLER_H_
