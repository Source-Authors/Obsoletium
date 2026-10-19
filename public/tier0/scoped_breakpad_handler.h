// Copyright Valve Corporation, All rights reserved.

#ifndef SE_TIER0_SCOPED_BREAKPAD_HANDLER_H_
#define SE_TIER0_SCOPED_BREAKPAD_HANDLER_H_

#include <memory>
#include <string_view>

#include "tier0/platform.h"

namespace s1::breakpad {

/**
 * @brief Registers a Google Breakpad for this object's lifetime.
 */
class PLATFORM_CLASS ScopedBreakpadHandler final {
 public:
  [[nodiscard]] static ScopedBreakpadHandler& Instance();
  [[nodiscard]] bool WriteMinidump();

  ~ScopedBreakpadHandler();
  ScopedBreakpadHandler(const ScopedBreakpadHandler&) = delete;
  ScopedBreakpadHandler& operator=(const ScopedBreakpadHandler&) = delete;
  ScopedBreakpadHandler(ScopedBreakpadHandler&&) = delete;
  ScopedBreakpadHandler& operator=(ScopedBreakpadHandler&&) = delete;

 private:
  explicit ScopedBreakpadHandler(std::string_view dump_path = ".");

  class Impl;

  MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()
  // DLL export looks safe.
  MSVC_DISABLE_WARNING(4251)
  std::unique_ptr<Impl> impl_;
  MSVC_END_WARNING_OVERRIDE_SCOPE()
};

}  // namespace s1::breakpad

#endif  // !SE_TIER0_SCOPED_BREAKPAD_HANDLER_H_
