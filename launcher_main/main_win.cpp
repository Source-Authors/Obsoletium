// Copyright Valve Corporation, All rights reserved.
//
// Purpose: A redirection tool that allows the DLLs to reside elsewhere.

#include <cstdio>
#include <cstdlib>

#include <system_error>

#include "scoped_dll.h"
#include "scoped_process_error_mode.h"
#include "winlite.h"

extern "C" {

// Starting with the Release 302 drivers, application developers can direct the
// Nvidia Optimus driver at runtime to use the High Performance Graphics to
// render any application - even those applications for which there is no
// existing application profile.
//
// See
// https://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;

// This will select the high performance AMD GPU as long as no profile exists
// that assigns the application to another GPU.  Please make sure to use a 13.35
// or newer driver.  Older drivers do not support this.
//
// See
// https://community.amd.com/t5/firepro-development/can-an-opengl-app-default-to-the-discrete-gpu-on-an-enduro/td-p/279440
__declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001;

}  // extern "C"

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
                                              std::error_code exit_code) {
  const auto system_error = exit_code.message();

  char entire_error_message[2048];
  _snprintf_s(entire_error_message, _TRUNCATE, "%s\n\n%s", error_message,
              system_error.c_str());

  MessageBoxA(nullptr, entire_error_message, "Source Launcher - Error",
              MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
  return exit_code.value();
}

[[nodiscard]] std::error_code GetLastErrorCode(DWORD errc = GetLastError()) {
  return std::error_code{static_cast<int>(errc), std::system_category()};
}

// Purpose: Apply process mitigation policies.
[[nodiscard]] int ApplyProcessMitigations() {
  // Disable loading DLLs from current directory to protect against DLL preload
  // attacks.
  if (!::SetDllDirectoryA("")) {
    return ShowErrorBoxAndExitWithCode(
        "Please contact publisher, very likely bug is detected.\n\n"
        "Unable to remove current directory from DLL search order.",
        GetLastErrorCode());
  }

  // Enable heap corruption detection & app termination.
  if (!HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr,
                          0)) {
    return ShowErrorBoxAndExitWithCode(
        "Please, contact publisher. Failed to enable termination on heap "
        "corruption feature for your environment.",
        GetLastErrorCode());
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
        GetLastErrorCode());
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
          GetLastErrorCode());
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
          GetLastErrorCode());
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
          GetLastErrorCode());
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
          GetLastErrorCode());
    }
  }

  return 0;
}

#define VALVE_OB_STRINGIFY(x) #x
#define VALVE_OB_TOSTRING(x) VALVE_OB_STRINGIFY(x)

int Run(_In_ HINSTANCE instance, _In_opt_ HINSTANCE old_instance,
        _In_ LPSTR cmd_line, _In_ int window_flags) {
  // Game uses features of Windows 10.
  if (!IsWindows10OrGreater()) {
    return ShowErrorBoxAndExitWithCode(
        "Unfortunately, your environment is not supported."
        "\n\nApp requires at least Windows 10 to survive.",
        GetLastErrorCode(ERROR_EXE_MACHINE_TYPE_MISMATCH));
  }

  // Do not show fault error boxes, etc.
  const source::ScopedProcessErrorMode scoped_process_error_mode{
#ifdef NDEBUG
      // The system does not display the critical-error-handler message box.
      // Instead, the system sends the error to the calling process.
      // Enable only in Release to detect critical errors during debug builds.
      SEM_FAILCRITICALERRORS |
#endif
      // The system automatically fixes memory alignment faults and makes them
      // invisible to the application.  It does this for the calling process
      // and any descendant processes.
      SEM_NOALIGNMENTFAULTEXCEPT |
      // The system does not display the Windows Error Reporting dialog.
      SEM_NOGPFAULTERRORBOX};

  {
    // Apply process mitigations.
    const int rc{ApplyProcessMitigations()};
    if (rc) return static_cast<int>(rc);
  }

  // Use the .exe name to determine the base directory.
  char module_name[MAX_PATH];
  if (!::GetModuleFileNameA(instance, module_name, MAX_PATH)) {
    return ShowErrorBoxAndExitWithCode(
        "Please check game installed in the folder with less than "
        VALVE_OB_TOSTRING(MAX_PATH) " chars deep.\n\nUnable to get "
        "module file name from GetModuleFileName.",
        GetLastErrorCode());
  }

  // Get the base directory the .exe is in.
  char base_directory_path[MAX_PATH], launcher_dll_path[MAX_PATH];
  // Assemble the full path to our "launcher.dll".
  _snprintf_s(launcher_dll_path, _TRUNCATE,
#if !defined(_WIN64)
              "%s\\bin\\launcher.dll",
#else
              "%s\\bin\\x64\\launcher.dll",
#endif
              GetBaseDirectory(module_name, base_directory_path));

  char user_error[1024];
  // STEAM OK ... filesystem not mounted yet.
  const source::ScopedDll launcher_dll{launcher_dll_path,
                                       LOAD_WITH_ALTERED_SEARCH_PATH};
  if (!launcher_dll) {
    _snprintf_s(user_error, _TRUNCATE,
                "Please check game installed in the folder with less "
                "than " VALVE_OB_TOSTRING(MAX_PATH) " chars deep.\n\n"
                "Unable to load the launcher DLL from %s.",
                launcher_dll_path);

    return ShowErrorBoxAndExitWithCode(user_error, launcher_dll.error_code());
  }

  using LauncherMainFunction = int (*)(HINSTANCE, HINSTANCE, LPSTR, int);
  constexpr char launcher_main_function_name[]{"LauncherMain"};

  const auto [launcher_main, errc] =
      launcher_dll.GetFunction<LauncherMainFunction>(
          launcher_main_function_name);
  if (!launcher_main) {
    _snprintf_s(user_error, _TRUNCATE,
                "Please check game installed correctly.\n\nUnable to find %s "
                "entry point in the launcher DLL %s.",
                launcher_main_function_name, launcher_dll_path);

    return ShowErrorBoxAndExitWithCode(user_error, errc);
  }

  return launcher_main(instance, old_instance, cmd_line, window_flags);
}

}  // namespace

int APIENTRY WinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE old_instance,
                     _In_ LPSTR cmd_line, _In_ int window_flags) {
  const auto rc = Run(instance, old_instance, cmd_line, window_flags);

  // Prevent tail call optimization and incorrect stack traces.
  exit(rc); //-V2014
}
