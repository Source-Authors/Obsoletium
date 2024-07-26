// Copyright Valve Corporation, All rights reserved.
//
//

#include "tools_minidump.h"

#include "winlite.h"

#include <atomic>
#include <dbghelp.h>

#include "tier0/minidump.h"

namespace {

// bool g_bToolsWriteFullMinidumps = false;
std::atomic<se::utils::common::ToolsExceptionHandler>
    g_pCustomExceptionHandler = nullptr;

LONG __stdcall ToolsExceptionFilter(EXCEPTION_POINTERS *ptrs) {
  // Non VMPI workers write a minidump and show a crash dialog like normal.
  MINIDUMP_TYPE iType = MiniDumpNormal;

  /*if (g_bToolsWriteFullMinidumps)
    iType = static_cast<MINIDUMP_TYPE>(MiniDumpWithDataSegs |
                                       MiniDumpWithIndirectlyReferencedMemory);*/

  WriteMiniDumpUsingExceptionInfo(ptrs->ExceptionRecord->ExceptionCode, ptrs,
                                  iType);

  return EXCEPTION_CONTINUE_SEARCH;
}

LONG __stdcall ToolsExceptionFilter_Custom(EXCEPTION_POINTERS *ptrs) {
  // Run their custom handler.
  g_pCustomExceptionHandler.load()(ptrs->ExceptionRecord->ExceptionCode, ptrs);

  return EXCEPTION_EXECUTE_HANDLER;  // (never gets here anyway)
}

}  // namespace

namespace se::utils::common {

// void EnableFullMinidumps(bool bFull) { g_bToolsWriteFullMinidumps = bFull; }

ToolsExceptionHandler SetupDefaultToolsMinidumpHandler() {
  ToolsExceptionHandler old_handler{
      std::atomic_exchange(&g_pCustomExceptionHandler, nullptr)};

  SetUnhandledExceptionFilter(ToolsExceptionFilter);

  return old_handler;
}

ToolsExceptionHandler SetupToolsMinidumpHandler(ToolsExceptionHandler fn) {
  ToolsExceptionHandler old_handler{
      std::atomic_exchange(&g_pCustomExceptionHandler, fn)};

  SetUnhandledExceptionFilter(ToolsExceptionFilter_Custom);

  return old_handler;
}

}  // namespace se::utils::common