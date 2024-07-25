// Copyright Valve Corporation, All rights reserved.

#ifndef SRC_LAUNCHER_SCOPED_APP_RELAUNCH_H
#define SRC_LAUNCHER_SCOPED_APP_RELAUNCH_H

#ifdef _WIN32
#include <winerror.h>
#include <winreg.h>
#include <WinUser.h>
#include <shellapi.h>

#include <array>  // For std::size
#elif defined(LINUX) || defined(OSX)
#include <sys/stat.h>
#include <sys/types.h>

#include <cstdio>
#endif

namespace se::launcher {

// Scoped app relaunch.
class ScopedAppRelaunch {
 public:
  ScopedAppRelaunch() noexcept {
#ifndef WIN32
    struct stat st;
    if (!stat(kRelaunchFilePath, &st)) {
      unlink(kRelaunchFilePath);
    }
#endif
  }

  ScopedAppRelaunch(const ScopedAppRelaunch &) = delete;
  ScopedAppRelaunch(ScopedAppRelaunch &&) = delete;
  ScopedAppRelaunch &operator=(const ScopedAppRelaunch &) = delete;
  ScopedAppRelaunch &operator=(ScopedAppRelaunch &&) = delete;

  ~ScopedAppRelaunch() noexcept {
#ifndef WIN32
    struct stat st;
    if (stat(kRelaunchFilePath, &st)) {
      return;
    }

    FILE *fp{fopen(kRelaunchFilePath, "r")};
    if (fp) {
      char cmd[256];
      int chars_count = fread(cmd, 1, sizeof(cmd), fp);

      fclose(fp);

      if (chars_count > 0) {
        if (chars_count > (sizeof(cmd) - 1)) {
          chars_count = sizeof(cmd) - 1;
        }
        cmd[chars_count] = 0;
        char open_line[MAX_PATH];

#if defined(LINUX)
        Q_snprintf(open_line, sizeof(open_line), "xdg-open \"%s\"", cmd);
#else
        Q_snprintf(open_line, sizeof(open_line), "open \"%s\"", cmd);
#endif

        system(open_line);
      }
    }
#else
    // Now that the mutex has been released, check
    // HKEY_CURRENT_USER\Software\Valve\Source\Relaunch URL.  If there is a URL
    // here, exec it.  This supports the capability of immediately re-launching
    // the the game via Steam in a different audio language.
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Source", 0,
                      KEY_ALL_ACCESS, &key) == ERROR_SUCCESS) {
      wchar_t value[MAX_PATH];
      DWORD value_size = std::size(value);

      if (RegQueryValueExW(key, L"Relaunch URL", nullptr, nullptr,
                           (unsigned char *)value,
                           &value_size) == ERROR_SUCCESS) {
        ShellExecuteW(nullptr, L"open", value, nullptr, nullptr, SW_SHOW);
        RegDeleteValueW(key, L"Relaunch URL");
      }

      RegCloseKey(key);
    }
#endif
  }

 private:
#ifndef WIN32
  static constexpr char kRelaunchFilePath[18] = {"/tmp/hl2_relaunch"};
#endif
};

}  // namespace se::launcher

#endif  // SRC_LAUNCHER_SCOPED_APP_RELAUNCH_H
