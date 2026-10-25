// Copyright Valve Corporation, All rights reserved.

#ifndef SE_COMMON_SCOPED_PURECALL_HANDLER_H_
#define SE_COMMON_SCOPED_PURECALL_HANDLER_H_

#include <cstdlib>
#include <tier0/dbg.h>

namespace s1::win {

/**
 * @brief Registers a pure-call handler for this object's lifetime.
 *
 * Restores the previously registered handler when the object is destroyed.
 */
class ScopedPurecallHandler {
 public:
  explicit ScopedPurecallHandler(_purecall_handler handler) noexcept
      : old_handler_{_set_purecall_handler(handler)}, new_handler_{handler} {}

  ~ScopedPurecallHandler() noexcept {
    if (_get_purecall_handler() == new_handler_) {
      _set_purecall_handler(old_handler_);
    } else {
      Warning(
          "Somebody already replaced _set_purecall_handler. Old handler not "
          "restored.\n");
    }
  }

  ScopedPurecallHandler(const ScopedPurecallHandler&) = delete;
  ScopedPurecallHandler& operator=(const ScopedPurecallHandler&) = delete;
  ScopedPurecallHandler(ScopedPurecallHandler&&) = delete;
  ScopedPurecallHandler& operator=(ScopedPurecallHandler&&) = delete;

 private:
  const _purecall_handler old_handler_, new_handler_;
};

}  // namespace s1::win

#endif  // SE_COMMON_SCOPED_PURECALL_HANDLER_H_
