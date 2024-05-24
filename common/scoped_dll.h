// Copyright Valve Corporation, All rights reserved.
//
// Purpose: A redirection tool that allows the DLLs to reside elsewhere.

#ifndef SRC_COMMON_SCOPED_DLL_H_
#define SRC_COMMON_SCOPED_DLL_H_

#include <type_traits>

#include "tier0/basetypes.h"

#ifdef _WIN32

FORWARD_DECLARE_HANDLE(HINSTANCE);
using HMODULE = HINSTANCE;

using FARPROC = ptrdiff_t(__stdcall *)();

extern "C" {

__declspec(dllimport) HMODULE
    __stdcall LoadLibraryExA(const char *lpLibFileName, void *hFile,
                             unsigned long dwFlags);
__declspec(dllimport) int __stdcall FreeLibrary(HMODULE hLibModule);
__declspec(dllimport) FARPROC
    __stdcall GetProcAddress(HMODULE hModule, const char *lpProcName);

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
#else
      : dll_{::dlopen(dll_path, load_flags)}
#endif
        dll_path_{dll_path} {
  }

  ScopedDll(const ScopedDll &) = delete;
  ScopedDll(const ScopedDll &&) = delete;
  ScopedDll &operator=(ScopedDll &) = delete;
  ScopedDll &operator=(ScopedDll &&) = delete;

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
  std::enable_if_t<is_function_pointer_v<F>, F> GetFunction(
      const char *name) const noexcept {
#ifdef _WIN32
    return reinterpret_cast<F>(::GetProcAddress(dll_, name));
#else
    return reinterpret_cast<F>(::dlsym(dll_, name));
#endif
  }

  // Check DLL loaded.
  [[nodiscard]] bool operator!() const noexcept { return !dll_; }

 private:
#ifdef _WIN32
  HMODULE dll_;
#else
  void *dll_;
#endif

  const char *dll_path_;
};

}  // namespace source

#endif  // !SRC_COMMON_SCOPED_DLL_H_