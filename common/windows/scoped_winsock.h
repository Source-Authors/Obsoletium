// Copyright Valve Corporation, All rights reserved.

#ifndef SE_COMMON_WINDOWS_SCOPED_WINSOCK_H_
#define SE_COMMON_WINDOWS_SCOPED_WINSOCK_H_

#include <sal.h>
#include <winsock.h>

#include <system_error>

#include "tier0/dbg.h"

namespace se::common::windows {

// Scoped Windows sockets initializer.
class ScopedWinsock {
 public:
  /**
   * @brief Startup Windows sockets version.
   * @param version Version.
   */
  explicit ScopedWinsock(unsigned short version)
      : errc_{::WSAStartup(version, &wsa_data_), std::system_category()},
        version_{version} {}

  /**
   * @brief Error code if any.
   * @return Error code.
   */
  [[nodiscard]] std::error_code errc() const noexcept { return errc_; }

  ScopedWinsock(const ScopedWinsock &) = delete;
  ScopedWinsock(ScopedWinsock &&) = delete;
  ScopedWinsock &operator=(const ScopedWinsock &) = delete;
  ScopedWinsock &operator=(ScopedWinsock &&) = delete;

  ~ScopedWinsock() noexcept {
    if (!errc_) {
      const int rc{::WSACleanup()};
      if (rc) {
        Warning("Windows sockets %hu shutdown failure (%d): %s.\n", version_,
                rc, std::system_category().message(rc).c_str());
      }
    }
  }

 private:
  WSAData wsa_data_;
  const std::error_code errc_;
  const unsigned short version_;
};

}  // namespace se::common::windows

#endif  // !SE_COMMON_WINDOWS_SCOPED_WINSOCK_H_
