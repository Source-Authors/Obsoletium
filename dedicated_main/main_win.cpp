// Copyright Valve Corporation, All rights reserved.
//
// Purpose: A redirection tool that allows the DLLs to reside elsewhere.

#include <cstdio>
#include <cstdlib>

#include <system_error>

#include "winlite.h"

namespace {

// Purpose: Return the directory where this .exe is running from
template <size_t buffer_size>
[[nodiscard]] const char *GetBaseDirectory(
    const char (&buffer)[buffer_size],
    char (&out_base_directory)[buffer_size]) {
  strcpy_s(out_base_directory, buffer);

  char *separator{strrchr(out_base_directory, '\\')};
  if (separator) *(separator + 1) = L'\0';

  const size_t size{strlen(out_base_directory)};
  if (size > 0) {
    char &lastChar{out_base_directory[size - 1]};

    if (lastChar == '\\' || lastChar == '/') {
      lastChar = '\0';
    }
  }

  return out_base_directory;
}

// Purpose: Shows error box and returns error code.
[[nodiscard]] int ShowErrorBoxAndExitWithCode(const char *error_message,
                                              int exit_code) {
  const auto system_error = std::system_category().message(exit_code);

  char entire_error_message[2048];
  _snprintf_s(entire_error_message, _TRUNCATE, "%s\n\n%s", error_message,
              system_error.c_str());

  MessageBoxA(nullptr, entire_error_message,
              "OrangeBox Dedicated Server - Error",
              MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
  return exit_code;
}

}  // namespace

#define VALVE_OB_STRINGIFY(x) #x
#define VALVE_OB_TOSTRING(x) VALVE_OB_STRINGIFY(x)

int APIENTRY WinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE old_instance,
                     _In_ LPSTR cmd_line, _In_ int window_flags) {
  // Use the .exe name to determine the base directory.
  char module_name[MAX_PATH];
  if (!::GetModuleFileNameA(instance, module_name, MAX_PATH)) {
    return ShowErrorBoxAndExitWithCode(
        "Please check game installed in the folder with less "
        "than " VALVE_OB_TOSTRING(MAX_PATH) " chars deep.\n\n"
        "Unable to get module file name from GetModuleFileName",
        ::GetLastError());
  }

  // Get the base directory the .exe is in.
  char base_directory_path[MAX_PATH], dedicated_dll_path[MAX_PATH];
  // Assemble the full path to our "dedicated.dll".
  _snprintf_s(dedicated_dll_path, _TRUNCATE, "%s\\bin\\dedicated.dll",
              GetBaseDirectory(module_name, base_directory_path));

  // STEAM OK ... filesystem not mounted yet.
  HMODULE dedicated_dll{::LoadLibraryExA(dedicated_dll_path, nullptr,
                                         LOAD_WITH_ALTERED_SEARCH_PATH)};
  if (!dedicated_dll) [[unlikely]] {
    const auto rc = ::GetLastError();
    char user_error[1024];
    _snprintf_s(user_error, _TRUNCATE,
                "Please check game installed in the folder with less "
                "than " VALVE_OB_TOSTRING(MAX_PATH) " chars deep.\n\n"
                "Unable to load the dedicated DLL from %s",
                dedicated_dll_path);

    return ShowErrorBoxAndExitWithCode(user_error, rc);
  }

  using DedicatedMainFunction = int (*)(HINSTANCE, HINSTANCE, LPSTR, int);

  const auto dedicated_main = reinterpret_cast<DedicatedMainFunction>(
      ::GetProcAddress(dedicated_dll, "DedicatedMain"));
  if (dedicated_main) [[likely]] {
    const auto rc =
        dedicated_main(instance, old_instance, cmd_line, window_flags);

    ::FreeLibrary(dedicated_dll);

    return rc;
  }

  {
    const auto rc = ::GetLastError();
    char user_error[1024];
    _snprintf_s(user_error, _TRUNCATE,
                "Please check game installed correctly.\n\nUnable to find "
                "DedicatedMain "
                "entry point in the dedicated DLL %s",
                dedicated_dll_path);

    return ShowErrorBoxAndExitWithCode(user_error, rc);
  }
}
