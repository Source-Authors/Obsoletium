// Copyright Valve Corporation, All rights reserved.

#ifndef SE_COMMON_WINDOWS_SCOPED_COM_H_
#define SE_COMMON_WINDOWS_SCOPED_COM_H_

#include <sal.h>
#include <objbase.h>

#include "tier0/commonmacros.h"

using HRESULT = _Return_type_success_(return >= 0) long;

extern "C" {

__declspec(dllimport) _Check_return_ HRESULT __stdcall CoInitializeEx(
    _In_opt_ void* pvReserved, _In_ unsigned long dwCoInit);
__declspec(dllimport) void __stdcall CoUninitialize();

}  // extern "C"

namespace s1::win {

/**
 * @brief Initializes COM for this scope and uninitializes it on successful
 * setup.  Construct and destroy this object on the same thread.
 */
class ScopedCom {
 public:
  explicit ScopedCom(COINIT com_type) noexcept
      : result_{CoInitializeEx(nullptr, to_underlying(com_type))} {}

  ScopedCom(const ScopedCom&) = delete;
  ScopedCom(ScopedCom&&) = delete;
  ScopedCom& operator=(const ScopedCom&) = delete;
  ScopedCom& operator=(ScopedCom&&) = delete;

  [[nodiscard]] HRESULT error_code() const noexcept { return result_; }

  ~ScopedCom() noexcept {
    if (SUCCEEDED(result_)) {
      CoUninitialize();
    }
  }

 private:
  const HRESULT result_;
};

}  // namespace s1::win

#endif  // !SE_COMMON_WINDOWS_SCOPED_COM_H_
