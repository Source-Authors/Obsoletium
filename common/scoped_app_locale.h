// Copyright Valve Corporation, All rights reserved.

#ifndef SE_COMMON_SCOPED_APP_LOCALE_H_
#define SE_COMMON_SCOPED_APP_LOCALE_H_

#include <clocale>
#include <string>

#include "tier1/strtools.h"

namespace se {

// Scoped app locale.
class ScopedAppLocale {
 public:
  explicit ScopedAppLocale(const char *new_locale) noexcept
      : old_locale_{GetCurrentLocale()}, new_locale_{new_locale} {
#ifdef LINUX
    setenv("LC_ALL", new_locale, 1);
#endif

    if (Q_strcmp(new_locale, old_locale_.c_str())) {
      std::setlocale(LC_ALL, new_locale);
    }
  }

  ScopedAppLocale(const ScopedAppLocale &) = delete;
  ScopedAppLocale(ScopedAppLocale &&) = delete;
  ScopedAppLocale &operator=(const ScopedAppLocale &) = delete;
  ScopedAppLocale &operator=(ScopedAppLocale &&) = delete;

  ~ScopedAppLocale() noexcept {
    const char *locale{GetCurrentLocale()};
    if (!Q_strcmp(locale, new_locale_.c_str())) {
      std::setlocale(LC_ALL, old_locale_.c_str());
    }
  }

  static const char *GetCurrentLocale() noexcept {
    const char *locale{std::setlocale(LC_ALL, nullptr)};
    return locale ? locale : kEmptyLocale;
  }

 private:
  std::string old_locale_, new_locale_;

  static constexpr char kEmptyLocale[1]{""};
};

}  // namespace se

#endif  // SE_COMMON_SCOPED_APP_LOCALE_H_
