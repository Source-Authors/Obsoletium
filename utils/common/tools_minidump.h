// Copyright Valve Corporation, All rights reserved.
//
// Tools minidump stuff.

#ifndef SE_UTILS_COMMON_TOOLS_MINIDUMP_H_
#define SE_UTILS_COMMON_TOOLS_MINIDUMP_H_

namespace se::utils::common {

/**
 * @brief Defaults to small minidumps.
 * @param enable If true, it'll write larger minidump files with the contents of
 * global variables and following pointers from where the crash occurred.
 */
// void EnableFullMinidumps(bool enable);

/**
 * @brief SEH handler.  Code is SEH exception code, info is struct
 * EXCEPTION_POINTERS if applicable.
 */
using ToolsExceptionHandler = void (*)(unsigned long code, void *info);

/**
 * @brief Enable / disable writing full dumps instead of normal ones.
 * @param enable true to write full dumps, false otherwise.
 * @return Old full minidumps enable state.
 */
bool EnableFullMinidumps(bool enable);

/**
 * @brief Catches any crash, writes a minidump, and runs the default system
 * crash handler (which usually shows a dialog).
 * @return Old exception handler.  nullptr if none.
 */
ToolsExceptionHandler SetupDefaultToolsMinidumpHandler();

/**
 * @brief Set exception handler.
 * @param new_handler New exception handler.
 * @return Old exception handler.  nullptr if none.
 */
ToolsExceptionHandler SetupToolsMinidumpHandler(
    ToolsExceptionHandler new_handler);

/**
 * @brief Set minidump handler for scope.
 */
class ScopedMinidumpHandler {
 public:
  ScopedMinidumpHandler(ToolsExceptionHandler handler) noexcept
      : old_handler_{SetupToolsMinidumpHandler(handler)} {}
  ~ScopedMinidumpHandler() noexcept { SetupToolsMinidumpHandler(old_handler_); }

  ScopedMinidumpHandler(ScopedMinidumpHandler &) = delete;
  ScopedMinidumpHandler &operator=(ScopedMinidumpHandler &) = delete;

 private:
  const ToolsExceptionHandler old_handler_;
};

/**
 * @brief Set default minidump handler for scope.
 */
class ScopedDefaultMinidumpHandler {
 public:
  ScopedDefaultMinidumpHandler() noexcept
      : handler_{SetupDefaultToolsMinidumpHandler()} {}
  ~ScopedDefaultMinidumpHandler() noexcept = default;

  ScopedDefaultMinidumpHandler(ScopedDefaultMinidumpHandler &) = delete;
  ScopedDefaultMinidumpHandler &operator=(ScopedDefaultMinidumpHandler &) =
      delete;

 private:
  const ScopedMinidumpHandler handler_;
};

}  // namespace se::utils::common

#endif  // !SE_UTILS_COMMON_TOOLS_MINIDUMP_H_
