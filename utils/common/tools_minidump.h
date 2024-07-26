// Copyright Valve Corporation, All rights reserved.
//
// Tools minidump stuff.

#ifndef SRC_UTILS_COMMON_TOOLS_MINIDUMP_H_
#define SRC_UTILS_COMMON_TOOLS_MINIDUMP_H_

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
ToolsExceptionHandler SetupToolsMinidumpHandler(ToolsExceptionHandler new_handler);

}  // namespace se::utils::common

#endif  // !SRC_UTILS_COMMON_TOOLS_MINIDUMP_H_
