// Copyright Valve Corporation, All rights reserved.
//
// Purpose: A redirection tool that allows the DLLs to reside elsewhere.

#ifndef SRC_COMMON_SCOPED_DLL_H_
#define SRC_COMMON_SCOPED_DLL_H_

#include <type_traits>
#include <system_error>

#include "tier0/basetypes.h"

#ifdef _WIN32

#include <sal.h>

FORWARD_DECLARE_HANDLE(HINSTANCE);
using HMODULE = HINSTANCE;

using FARPROC = ptrdiff_t(__stdcall *)();

extern "C" {

__declspec(dllimport) _Ret_maybenull_ HMODULE
    __stdcall LoadLibraryExA(_In_ const char *lpLibFileName,
                             _Reserved_ void *hFile,
                             _In_ unsigned long dwFlags);
__declspec(dllimport) int __stdcall FreeLibrary(_In_ HMODULE hLibModule);
__declspec(dllimport) FARPROC
    __stdcall GetProcAddress(_In_ HMODULE hModule, _In_ const char *lpProcName);

__declspec(dllimport) _Check_return_ unsigned long __stdcall GetLastError();

}  // extern "C"

#else

#include <cstdio>
#include <dlfcn.h>
#include <unistd.h>

#endif

namespace source {

// Is function pointer?
template <typename F>
constexpr bool is_function_pointer_v =
    std::is_pointer_v<F> && std::is_function_v<std::remove_pointer_t<F>>;

// Scoped DLL.
class ScopedDll {
 public:
  // Create scoped DLL from dll path and load flags.
  ScopedDll(const char *dll_path, unsigned int load_flags) noexcept
#ifdef _WIN32
      : dll_{::LoadLibraryExA(dll_path, nullptr, load_flags)},
        rc_{dll_ ? 0 : static_cast<int>(GetLastError()), std::system_category()}
#else
      : dll_{::dlopen(dll_path, load_flags)},
        rc_{dll_ ? 0 : EINVAL}
#endif
#ifndef _WIN32
        ,
        dll_path_{dll_path}
#endif
  {
  }

  ScopedDll(const ScopedDll &) = delete;
  ScopedDll(ScopedDll &&d) noexcept : dll_{d.dll_} {
    d.dll_ = nullptr;
#ifndef _WIN32
    std::memset(d.dll_path_, 0, sizeoof(d.dll_path_));
#endif;
  }
  ScopedDll &operator=(ScopedDll &) = delete;
  ScopedDll &operator=(ScopedDll &&d) noexcept {
    std::swap(d.dll_, dll_);
#ifndef _WIN32
    std::swap(dll_path_, d.dll_path_);
#endif
    return *this;
  }

  ~ScopedDll() noexcept {
    if (dll_) {
#ifdef _WIN32
      // Force exit when unload failure.
      if (!FreeLibrary(dll_)) exit(1);
#else
      // Force exit when unload failure.
      if (::dlclose(dll_)) {
        fprintf(stderr, "Failed to close the %s: %s.\n", dll_path_,
                ::dlerror());
        exit(1);
      }
#endif
    }
  }

  // Get DLL function by name.
  template <typename F>
  std::tuple<std::enable_if_t<is_function_pointer_v<F>, F>, std::error_code>
  GetFunction(const char *name) const noexcept {
#ifdef _WIN32
    F f = reinterpret_cast<F>(::GetProcAddress(dll_, name));
#else
    F f = reinterpret_cast<F>(::dlsym(dll_, name));
#endif

    if (f == nullptr) {
      return {nullptr, std::error_code{static_cast<int>(GetLastError()),
                                       std::system_category()}};
    }

    return {f, std::error_code{}};
  }

  // Check DLL loaded.
  [[nodiscard]] bool operator!() const noexcept { return !dll_; }

  // Last error code for DLL.
  [[nodiscard]] std::error_code error_code() const noexcept { return rc_; }

 private:
#ifdef _WIN32
  HMODULE dll_;
#else
  void *dll_;
#endif

  std::error_code rc_;

#ifndef _WIN32
  [[maybe_unused]] const char *dll_path_;
#endif
};

}  // namespace source

#endif  // !SRC_COMMON_SCOPED_DLL_H_
