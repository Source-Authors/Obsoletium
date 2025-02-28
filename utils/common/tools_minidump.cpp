// Copyright Valve Corporation, All rights reserved.
//
//

#include "tools_minidump.h"

#include "winlite.h"

#include <atomic>
#include <dbghelp.h>

#include "tier0/minidump.h"

namespace {

std::atomic_bool g_bToolsWriteFullMinidumps{false};
std::atomic<se::utils::common::ToolsExceptionHandler> g_pCustomExceptionHandler{
    nullptr};

LONG __stdcall DefaultToolsExceptionFilter(EXCEPTION_POINTERS *ptrs) {
  // Non VMPI workers write a minidump and show a crash dialog like normal.
  MINIDUMP_TYPE minidump_type{MiniDumpNormal};

  if (g_bToolsWriteFullMinidumps.load(
          std::memory_order::memory_order_relaxed)) {
    minidump_type = static_cast<MINIDUMP_TYPE>(
        MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory);
  }

  return WriteMiniDumpUsingExceptionInfo(ptrs->ExceptionRecord->ExceptionCode,
                                         ptrs, minidump_type)
             ? EXCEPTION_CONTINUE_SEARCH
             : EXCEPTION_EXECUTE_HANDLER;
}

LONG __stdcall CallbackToolsExceptionFilter(EXCEPTION_POINTERS *ptrs) {
  // Run their custom handler.
  const auto handler =
      g_pCustomExceptionHandler.load(std::memory_order::memory_order_relaxed);
  if (handler) handler(ptrs->ExceptionRecord->ExceptionCode, ptrs);

  return EXCEPTION_EXECUTE_HANDLER;  // (never gets here anyway)
}

}  // namespace

namespace se::utils::common {

bool EnableFullMinidumps(bool enable) {
  return g_bToolsWriteFullMinidumps.exchange(
      enable, std::memory_order::memory_order_relaxed);
}

ToolsExceptionHandler SetupDefaultToolsMinidumpHandler() {
  const ToolsExceptionHandler old_handler{
      std::atomic_exchange(&g_pCustomExceptionHandler, nullptr)};

  ::SetUnhandledExceptionFilter(DefaultToolsExceptionFilter);

  return old_handler;
}

ToolsExceptionHandler SetupToolsMinidumpHandler(ToolsExceptionHandler fn) {
  const ToolsExceptionHandler old_handler{
      std::atomic_exchange(&g_pCustomExceptionHandler, fn)};

  ::SetUnhandledExceptionFilter(CallbackToolsExceptionFilter);

  return old_handler;
}

}  // namespace se::utils::common