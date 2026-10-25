// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"
#include "tier0/scoped_breakpad_handler.h"

#if defined(_WIN32)
#include "client/windows/handler/exception_handler.h"
#elif defined(_LINUX)
#include "client/linux/handler/exception_handler.h"
#elif defined(OSX)
#include "client/mac/handler/exception_handler.h"
#else
#error "Please define your target os."
#endif

#include "tier0/dbg.h"

#include "tier0/memdbgon.h"

namespace s1::breakpad {

class ScopedBreakpadHandler::Impl final {
 public:
  explicit Impl(std::string_view dump_path)
      :
#if defined(_WIN32)
        exception_handler_{
            UTF8ToWide(std::string{dump_path}), nullptr, MinidumpCallback,
            nullptr,
            // Exception, invalid parameter and pure call.
            google_breakpad::ExceptionHandler::HANDLER_ALL,
            static_cast<MINIDUMP_TYPE>(
                MiniDumpNormal | MiniDumpWithDataSegs | MiniDumpWithHandleData |
                MiniDumpWithThreadInfo | MiniDumpWithProcessThreadData |
                MiniDumpWithUnloadedModules |
                MiniDumpWithIndirectlyReferencedMemory),
            L"", nullptr}
#elif defined(_LINUX)
        minidump_descriptor_{dump_path},
        exception_handler_{descriptor, nullptr, MinidumpCallback,
                           nullptr,    true,    -1}
#elif defined(OSX)
        exception_handler_{dump_path, nullptr, MinidumpCallback,
                           nullptr,   true,    nullptr}
#else
#error "Please define your target os."
#endif
  {
  }

  [[nodiscard]] bool WriteMinidump() {
    return exception_handler_.WriteMinidump();
  }

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(Impl&&) = delete;

 private:
#if defined(_WIN32)
  google_breakpad::ExceptionHandler exception_handler_;

  static bool MinidumpCallback(const wchar_t* dump_path,
                               const wchar_t* minidump_id, void* context,
                               EXCEPTION_POINTERS* exinfo,
                               MDRawAssertionInfo* assertion, bool succeeded) {
    if (succeeded) {
      printf("Created minidump at %S\\%S.dmp\n", dump_path, minidump_id);
    } else {
      fprintf(stderr, "Failed to create minidump.\n");
    }

    return succeeded;
  }

  [[nodiscard]] std::wstring UTF8ToWide(const std::string& utf8) {
    if (utf8.length() == 0) {
      return std::wstring();
    }

    // compute the length of the buffer we'll need
    int charcount =
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);

    if (charcount == 0) {
      return std::wstring();
    }

    // convert
    wchar_t* buf = new wchar_t[charcount];
    RunCodeAtScopeExit(delete[] buf);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, buf, charcount);
    return std::wstring{buf};
  }
#elif defined(_LINUX)
  google_breakpad::MinidumpDescriptor minidump_descriptor_;
  google_breakpad::ExceptionHandler exception_handler_;

  static bool MinidumpCallback(
      const google_breakpad::MinidumpDescriptor& descriptor, void* context,
      bool succeeded) {
    if (succeeded) {
      printf("Created minidump at %s.\n", descriptor.path());
    } else {
      fprintf(stderr, "Failed to create minidump.\n");
    }

    return succeeded;
  }
#elif defined(OSX)
  google_breakpad::ExceptionHandler exception_handler_;

  static bool MinidumpCallback(const char* dump_dir, const char* minidump_id,
                               void* context, bool succeeded) {
    if (succeeded) {
      printf("Created minidump at %s/%s.dmp\n", dump_dir, minidump_id);
    } else {
      fprintf(stderr, "Failed to create minidump.\n");
    }

    return succeeded;
  }
#else
#error "Please define your target os."
#endif
};

ScopedBreakpadHandler::~ScopedBreakpadHandler() = default;

[[nodiscard]] ScopedBreakpadHandler& ScopedBreakpadHandler::Instance() {
  static ScopedBreakpadHandler instance;
  return instance;
}

[[nodiscard]] bool ScopedBreakpadHandler::WriteMinidump() {
  return impl_->WriteMinidump();
}

ScopedBreakpadHandler::ScopedBreakpadHandler(std::string_view dump_path)
    : impl_{std::make_unique<Impl>(dump_path)} {}

}  // namespace s1::breakpad