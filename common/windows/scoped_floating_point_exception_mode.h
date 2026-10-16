// Copyright Valve Corporation, All rights reserved.

#ifndef SE_COMMON_WINDOWS_SCOPED_FLOATING_EXCEPTION_POINT_MODE_H_
#define SE_COMMON_WINDOWS_SCOPED_FLOATING_EXCEPTION_POINT_MODE_H_

#include "tier0/annotations.h"
#include "tier0/commonmacros.h"
#include "tier0/dbg.h"

extern "C" unsigned int __cdecl _clearfp(void);
extern "C" int __cdecl _controlfp_s(OUT_OPT_ARG unsigned int* _CurrentState,
                                    IN_P unsigned int _NewValue,
                                    IN_P unsigned int _Mask);

namespace s1::win {

/**
 * @brief Floating-point exceptions to enable within a scope.
 */
enum class FloatingPointExceptionMode : unsigned {
  /** @brief Select no exception types. Not supported by the scoped mode. */
  None = 0x0,
  /** @brief Enable the inexact-result exception. */
  Inexact = 0x0000'0001,
  /** @brief Enable the underflow exception. */
  Underflow = 0x0000'0002,
  /** @brief Enable the overflow exception. */
  Overflow = 0x0000'0004,
  /** @brief Enable the division-by-zero exception. */
  ZeroDivide = 0x0000'0008,
  /** @brief Enable the invalid-operation exception. */
  Invalid = 0x0000'0010,
  /** @brief Enable all supported exception types. */
  All = 0x1 | 0x2 | 0x4 | 0x8 | 0x10
};


[[nodiscard]] constexpr FloatingPointExceptionMode operator|(
    FloatingPointExceptionMode left,
    FloatingPointExceptionMode right) noexcept {
  return static_cast<FloatingPointExceptionMode>(to_underlying(left) |
                                                 to_underlying(right));
}

[[nodiscard]] constexpr FloatingPointExceptionMode operator&(
    FloatingPointExceptionMode left,
    FloatingPointExceptionMode right) noexcept {
  return static_cast<FloatingPointExceptionMode>(to_underlying(left) &
                                                 to_underlying(right));
}

/**
 * @brief Apply floating point exception mode in scope and restore back out of
 * scope.  Note it clears floating point status flags, too.
 */
class ScopedFloatingPointExceptionMode {
 public:
  explicit ScopedFloatingPointExceptionMode(
      FloatingPointExceptionMode new_mode) noexcept
      // Retrieve the current state of the exception control mode.  This must be
      // done before changing them.
      : old_mode_{0},
        new_mode_{to_underlying(new_mode) & _MCW_EM},
        rc_{_controlfp_s(&old_mode_, 0, 0)} {
    if (rc_) {
      Warning("Failed to get floating point control mode: %s.\n",
              strerror(rc_));
      return;
    }

    if (new_mode == FloatingPointExceptionMode::None) {
      Warning("None floating point exception mode is not supported.\n");
      rc_ = ENOTSUP;
      return;
    }

    if ((to_underlying(new_mode) & ~to_underlying(FloatingPointExceptionMode::All)) != 0) {
      Warning("Unknown floating point exception mode 0x%x.\n",
              to_underlying(new_mode));
      rc_ = EINVAL;
      return;
    }

    // Clear any pending FP exceptions.  This must be done
    // prior to enabling FP exceptions since otherwise there
    // may be a 'deferred crash' as soon the exceptions are
    // enabled.
    _clearfp();

    // Zero out the specified bits, leaving other bits alone.
    rc_ = _controlfp_s(nullptr, ~new_mode_, new_mode_);
    new_mode_ = old_mode_ & ~new_mode_;
    if (rc_) {
      Warning("Failed to set floating point control mode 0x%x -> 0x%x: %s.\n",
              old_mode_, new_mode_, strerror(rc_));
      return;
    }
  }

  ~ScopedFloatingPointExceptionMode() noexcept {
    if (rc_) return;

    unsigned int actual_new_mode{0};
    rc_ = _controlfp_s(&actual_new_mode, 0, 0);
    if (rc_) {
      Warning("Failed to get floating point control mode: %s.\n",
              strerror(rc_));
      return;
    }

    if ((new_mode_ & _MCW_EM) != (actual_new_mode & _MCW_EM)) {
      Warning(
          "Somebody changed floating point exception control mode between "
          "calls. Expected 0x%x, got 0x%x.\n",
          new_mode_ & _MCW_EM, actual_new_mode & _MCW_EM);
      return;
    }

    // Reset the exception state.
    rc_ = _controlfp_s(0, old_mode_, _MCW_EM);
    if (rc_) {
      Warning("Failed to reset floating point control mode 0x%x -> 0x%x: %s.\n",
              new_mode_, old_mode_, strerror(rc_));
      return;
    }
  }

  ScopedFloatingPointExceptionMode(ScopedFloatingPointExceptionMode&) = delete;
  ScopedFloatingPointExceptionMode& operator=(
      ScopedFloatingPointExceptionMode&) = delete;
  ScopedFloatingPointExceptionMode(ScopedFloatingPointExceptionMode&&) = delete;
  ScopedFloatingPointExceptionMode& operator=(
      ScopedFloatingPointExceptionMode&&) = delete;

 private:
  unsigned int old_mode_, new_mode_;
  int rc_;
};

}  // namespace s1::win

#endif  // !SE_COMMON_WINDOWS_SCOPED_FLOATING_EXCEPTION_POINT_MODE_H_
