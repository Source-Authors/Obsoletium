// Copyright Valve Corporation, All rights reserved.

#ifndef SE_COMMON_WINDOWS_SCOPED_COM_H_
#define SE_COMMON_WINDOWS_SCOPED_COM_H_

#include <sal.h>
#include <objbase.h>

using HRESULT = _Return_type_success_(return >= 0) long;

extern "C" {

__declspec(dllimport) _Check_return_ HRESULT
    __stdcall CoInitializeEx(_In_opt_ void *pvReserved,
                             _In_ unsigned long dwCoInit);
__declspec(dllimport) void __stdcall CoUninitialize();
}

namespace se::common::windows {

// Scoped COM.
class ScopedCom {
 public:
  explicit ScopedCom(COINIT com_type) noexcept
      : errc_{CoInitializeEx(nullptr, com_type)} {}

  ScopedCom(const ScopedCom &) = delete;
  ScopedCom(ScopedCom &&) = delete;
  ScopedCom &operator=(const ScopedCom &) = delete;
  ScopedCom &operator=(ScopedCom &&) = delete;

  [[nodiscard]] HRESULT errc() const noexcept { return errc_; }

  ~ScopedCom() noexcept {
    if (SUCCEEDED(errc_)) CoUninitialize();
  }

 private:
  const HRESULT errc_;
};

}  // namespace se::common::windows

#endif  // !SE_COMMON_WINDOWS_SCOPED_COM_H_
