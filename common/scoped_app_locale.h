// Copyright Valve Corporation, All rights reserved.

#ifndef SE_COMMON_SCOPED_APP_LOCALE_H_
#define SE_COMMON_SCOPED_APP_LOCALE_H_

#include <clocale>
#include <string>

#include "tier0/annotations.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"

namespace s1 {

/**
 * @brief Set locale for process in scope and revert back out of scope.
 */
class ScopedAppLocale {
 public:
  explicit ScopedAppLocale(IN_P const char* new_locale)
      : old_locale_{GetCurrentLocale()},
        new_locale_{new_locale ? SetCurrentLocale(new_locale) : kEmptyLocale} {}

  ~ScopedAppLocale() noexcept {
    if (!new_locale_.empty()) {
      const char* current_locale{GetCurrentLocale()};
      if (V_streq(current_locale, new_locale_.c_str())) {
        const char* actual_locale{std::setlocale(LC_ALL, old_locale_.c_str())};
        if (V_isempty(actual_locale)) {
          Warning("Failed to reset locale from %s to %s.\n", current_locale,
                  old_locale_.c_str());
        }
      }
    }
  }

  [[nodiscard]] bool IsSuccess() const noexcept { return !new_locale_.empty(); }

  [[nodiscard]] static const char* GetCurrentLocale() noexcept {
    const char* locale{std::setlocale(LC_ALL, nullptr)};
    return locale ? locale : kEmptyLocale;
  }

  ScopedAppLocale(const ScopedAppLocale&) = delete;
  ScopedAppLocale(ScopedAppLocale&&) = delete;
  ScopedAppLocale& operator=(const ScopedAppLocale&) = delete;
  ScopedAppLocale& operator=(ScopedAppLocale&&) = delete;

 private:
  std::string old_locale_, new_locale_;

  [[nodiscard]] static const char* SetCurrentLocale(
      const char* locale) noexcept {
    const char* actual_locale{std::setlocale(LC_ALL, locale)};
    return actual_locale ? actual_locale : kEmptyLocale;
  }

  static constexpr char kEmptyLocale[1]{""};
};

}  // namespace s1

#endif  // !SE_COMMON_SCOPED_APP_LOCALE_H_
