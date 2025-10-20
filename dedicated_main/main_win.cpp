// Copyright Valve Corporation, All rights reserved.
//
// Purpose: A redirection tool that allows the DLLs to reside elsewhere.

#include <cstdio>
#include <cstdlib>

#include <system_error>

#include "tier0/platform.h"

#include "scoped_dll.h"
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
[[nodiscard]] int ShowErrorBoxAndExitWithCode(_In_z_ const char *error_message,
                                              _In_ std::error_code exit_code,
                                              _In_ bool is_console_mode) {
  const auto system_error = exit_code.message();

  char entire_error_message[2048];
  _snprintf_s(entire_error_message, _TRUNCATE, "%s\n\n%s", error_message,
              system_error.c_str());

  if (is_console_mode) {
    fprintf(stderr, "%s\n", entire_error_message);
    OutputDebugStringA(entire_error_message);
  } else {
    // Note, uses delay load to allow run in no Win32 mode.
    MessageBoxA(nullptr, entire_error_message, "SRCDS - Error",
                MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
  }

  return exit_code.value();
}

[[nodiscard]] std::error_code GetLastErrorCode(DWORD errc = GetLastError()) {
  return std::error_code{static_cast<int>(errc), std::system_category()};
}

// Purpose: Apply process mitigation policies.
[[nodiscard]] int ApplyProcessMitigations(bool is_console_mode) {
  // Disable loading DLLs from current directory to protect against DLL preload
  // attacks.
  if (!::SetDllDirectoryA("")) {
    return ShowErrorBoxAndExitWithCode(
        "Please contact publisher, very likely bug is detected.\n\n"
        "Unable to remove current directory from DLL search order.",
        GetLastErrorCode(), is_console_mode);
  }

  // Enable heap corruption detection & app termination.
  if (!HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr,
                          0)) {
    return ShowErrorBoxAndExitWithCode(
        "Please, contact publisher. Failed to enable termination on heap "
        "corruption feature for your environment.",
        GetLastErrorCode(), is_console_mode);
  }

#if !defined(_WIN64)
  // DEP is always enabled on 64-bit.
  const DWORD dep_flags =
      PROCESS_DEP_ENABLE | PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION;
  if (!::SetProcessDEPPolicy(dep_flags) &&
      ERROR_ACCESS_DENIED != ::GetLastError()) {
    return ShowErrorBoxAndExitWithCode(
        "Please, contact publisher. Failed to enable DEP policy for your "
        "environment.",
        GetLastErrorCode(), is_console_mode);
  }
#endif

  {
    // Enable ASLR policies.
    PROCESS_MITIGATION_ASLR_POLICY policy = {};
    policy.EnableForceRelocateImages = true;
    policy.DisallowStrippedImages = true;

    if (!SetProcessMitigationPolicy(ProcessASLRPolicy, &policy,
                                    sizeof(policy)) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return ShowErrorBoxAndExitWithCode(
          "Please, contact publisher. Failed to enable ASLR policy for your "
          "environment.",
          GetLastErrorCode(), is_console_mode);
    }
  }

  {
    // Enable strict handle policies.
    PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY policy = {};
    policy.HandleExceptionsPermanentlyEnabled =
        policy.RaiseExceptionOnInvalidHandleReference = true;

    if (!SetProcessMitigationPolicy(ProcessStrictHandleCheckPolicy, &policy,
                                    sizeof(policy)) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return ShowErrorBoxAndExitWithCode(
          "Please, contact publisher. Failed to enable Strict Handle policy "
          "for your environment.",
          GetLastErrorCode(), is_console_mode);
    }
  }

  {
    // Enable extension point policies.
    PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY policy = {};
    policy.DisableExtensionPoints = true;

    if (!SetProcessMitigationPolicy(ProcessExtensionPointDisablePolicy, &policy,
                                    sizeof(policy)) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return ShowErrorBoxAndExitWithCode(
          "Please, contact publisher. Failed to enable Extension Points policy "
          "for your environment.",
          GetLastErrorCode(), is_console_mode);
    }
  }

  {
    // Enable image load policies.
    PROCESS_MITIGATION_IMAGE_LOAD_POLICY policy = {};
    policy.NoRemoteImages = true;
    policy.NoLowMandatoryLabelImages = true;
    // PreferSystem32 is only supported on >= Anniversary.
    policy.PreferSystem32Images = true;

    if (!SetProcessMitigationPolicy(ProcessImageLoadPolicy, &policy,
                                    sizeof(policy)) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return ShowErrorBoxAndExitWithCode(
          "Please, contact publisher. Failed to harden Image Load policy for "
          "your environment.",
          GetLastErrorCode(), is_console_mode);
    }
  }

  // Address Sanitizer generates dynamic code.
#ifndef __SANITIZE_ADDRESS__
  {
    // Prohibit dynamic code generation as no shaders in dedicated server.
    PROCESS_MITIGATION_DYNAMIC_CODE_POLICY policy = {};
    // Set (0x1) to prevent the process from generating dynamic code or
    // modifying existing executable code; otherwise leave unset (0x0).
    policy.ProhibitDynamicCode = 1;
    // Set (0x1) to allow threads to opt out of the restrictions on dynamic code
    // generation by calling the SetThreadInformation function with the
    // ThreadInformation parameter set to ThreadDynamicCodePolicy; otherwise
    // leave unset (0x0).  You should not use the AllowThreadOptOut and
    // ThreadDynamicCodePolicy settings together to provide strong security.
    // These settings are only intended to enable applications to adapt their
    // code more easily for full dynamic code restrictions.
    policy.AllowThreadOptOut = 0;
    // Set (0x1) to allow non-AppContainer processes to modify all of the
    // dynamic code settings for the calling process, including relaxing dynamic
    // code restrictions after they have been set.
    policy.AllowRemoteDowngrade = 0;

    // The dynamic code policy of the process.  When turned on, the process
    // cannot generate dynamic code or modify existing executable code.
    if (!SetProcessMitigationPolicy(ProcessDynamicCodePolicy, &policy,
                                    sizeof(policy)) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return ShowErrorBoxAndExitWithCode(
          "Please, contact publisher. Failed to apply Dynamic Code "
          "Generation Prohibition policy for your environment.",
          GetLastErrorCode(), is_console_mode);
    }
  }
#endif

  {
    // Console mode, no NTUser/GDI calls allowed.
    if (is_console_mode) {
      PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY policy = {};
      // When set to 1, the process is not permitted to perform GUI system
      // calls.
      policy.DisallowWin32kSystemCalls = 0;
      // When set to 1, the process is not be able to successfully make
      // NtFsControlFile system calls, with the following FsControlCode
      // exceptions.
      //
      // FSCTL_IS_VOLUME_MOUNTED
      // FSCTL_PIPE_IMPERSONATE
      // FSCTL_PIPE_LISTEN
      // FSCTL_PIPE_DISCONNECT
      // FSCTL_PIPE_TRANSCEIVE
      // FSCTL_PIPE_WAIT
      // FSCTL_PIPE_GET_PIPE_ATTRIBUTE
      // FSCTL_PIPE_GET_CONNECTION_ATTRIBUTE
      // FSCTL_PIPE_GET_HANDLE_ATTRIBUTE
      // FSCTL_PIPE_PEEK
      // FSCTL_PIPE_EVENT_SELECT
      // FSCTL_PIPE_EVENT_ENUM
      policy.DisallowFsctlSystemCalls = 1;

      if (!SetProcessMitigationPolicy(ProcessSystemCallDisablePolicy, &policy,
                                      sizeof(policy)) &&
          ERROR_ACCESS_DENIED != ::GetLastError()) {
        return ShowErrorBoxAndExitWithCode(
            "Please, contact publisher. Failed to apply Win32k and Fsctl "
            "System Calls Prohibition policy for your environment.",
            GetLastErrorCode(), is_console_mode);
      }
    }
  }

  return 0;
}

/**
 * @brief Check app started in console mode.
 * @param command_line Command line.
 * @return true if app in console mode, false otherwise.
 */
bool IsConsoleMode(const char *command_line) {
  constexpr char kConsoleArgName[]{"-console"};

  const char *console_arg{strstr(command_line, kConsoleArgName)};
  if (!console_arg) return false;

  const char *next_arg{console_arg + std::size(kConsoleArgName) - 1};

  return isspace(*next_arg) != 0;
}

}  // namespace

#define VALVE_OB_STRINGIFY(x) #x
#define VALVE_OB_TOSTRING(x) VALVE_OB_STRINGIFY(x)

int APIENTRY WinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE old_instance,
                     _In_ LPSTR cmd_line, _In_ int window_flags) {
  const bool is_console_mode{IsConsoleMode(cmd_line)};

  // Game uses features of Windows 10.
  if (!IsWindows10OrGreater()) {
    return ShowErrorBoxAndExitWithCode(
        "Unfortunately, your environment is not supported."
        "\n\nApp requires at least Windows 10 to survive.",
        GetLastErrorCode(ERROR_EXE_MACHINE_TYPE_MISMATCH), is_console_mode);
  }

  // Do not show fault error boxes, etc.
  (void)::SetErrorMode(
#ifdef NDEBUG
      // The system does not display the critical-error-handler message box.
      // Instead, the system sends the error to the calling process.
      // Enable only in Release to detect critical errors during debug builds.
      SEM_FAILCRITICALERRORS |
#endif
      // The system automatically fixes memory alignment faults and makes them
      // invisible to the application.  It does this for the calling process and
      // any descendant processes.
      SEM_NOALIGNMENTFAULTEXCEPT |
      // The system does not display the Windows Error Reporting dialog.
      SEM_NOGPFAULTERRORBOX);

  {
    // Apply process mitigations.
    int rc = ApplyProcessMitigations(is_console_mode);
    if (rc) return static_cast<int>(rc);
  }

  // Use the .exe name to determine the base directory.
  char module_name[MAX_PATH];
  if (!::GetModuleFileNameA(instance, module_name, MAX_PATH)) {
    return ShowErrorBoxAndExitWithCode(
        "Please check game installed in the folder with less "
        "than " VALVE_OB_TOSTRING(MAX_PATH) " chars deep.\n\n"
        "Unable to get module file name from GetModuleFileName.",
        GetLastErrorCode(), is_console_mode);
  }

  // Get the base directory the .exe is in.
  char base_directory_path[MAX_PATH], dedicated_dll_path[MAX_PATH];
  // Assemble the full path to our "dedicated.dll".
  _snprintf_s(dedicated_dll_path, _TRUNCATE,
              "%s\\" PLATFORM_BIN_DIR "\\dedicated.dll",
              GetBaseDirectory(module_name, base_directory_path));

  char user_error[1024];
  // STEAM OK ... filesystem not mounted yet.
  const source::ScopedDll dedicated_dll{dedicated_dll_path,
                                        LOAD_WITH_ALTERED_SEARCH_PATH};
  if (!dedicated_dll) {
    _snprintf_s(user_error, _TRUNCATE,
                "Please check game installed in the folder with less "
                "than " VALVE_OB_TOSTRING(MAX_PATH) " chars deep.\n\n"
                "Unable to load the dedicated DLL from %s.",
                dedicated_dll_path);

    return ShowErrorBoxAndExitWithCode(user_error, dedicated_dll.error_code(),
                                       is_console_mode);
  }

  using DedicatedMainFunction = int (*)(HINSTANCE, HINSTANCE, LPSTR, int);

  const auto [dedicated_main, errc] =
      dedicated_dll.GetFunction<DedicatedMainFunction>("DedicatedMain");
  if (!dedicated_main) {
    _snprintf_s(user_error, _TRUNCATE,
                "Please check game installed correctly.\n\nUnable to find "
                "DedicatedMain entry point in the dedicated DLL %s.",
                dedicated_dll_path);

    return ShowErrorBoxAndExitWithCode(user_error, errc, is_console_mode);
  }

  const auto rc =
      dedicated_main(instance, old_instance, cmd_line, window_flags);

  // Prevent tail call optimization and incorrect stack traces.
  exit(rc);  //-V2014
}
