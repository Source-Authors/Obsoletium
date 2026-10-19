// Copyright Valve Corporation, All rights reserved.
//
// Defines the entry point for the application.

#ifndef _WIN32
#include <unistd.h>  // chdir
#endif

#include <atomic>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <system_error>
#include <thread>

#include "app_version_config.h"
#include "../launcher_main/resource.h"

#include "scoped_app_locale.h"
#include "scoped_app_multirun.h"
#include "scoped_app_relaunch.h"
#include "scoped_heap_leak_dumper.h"

#ifdef _WIN32
#include "winlite.h"

#include <direct.h>          // _chdir
#include <comdef.h>          // _com_error
#include <VersionHelpers.h>  // IsWindows10OrGreater
#include <WinSock2.h>

#include "windows/scoped_com.h"
#include "windows/scoped_console_ctrl_handler.h"
#include "windows/scoped_floating_point_exception_mode.h"
#include "windows/scoped_timer_resolution.h"
#include "windows/scoped_winsock_initializer.h"
#endif

#include "appframework/AppFramework.h"
#include "appframework/ilaunchermgr.h"

#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "tier0/scoped_breakpad_handler.h"

#include "boot_app_system_group.h"
#include "file_logger.h"
#include "vcr_helpers.h"

#ifndef NO_STEAM
#define VERSION_SAFE_STEAM_API_INTERFACES
#include "steam/steam_api.h"
#endif

#ifdef __GLIBC__
#include <gnu/libc-version.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined(USE_SDL)
extern void* CreateSDLMgr();
#endif

namespace {

// Exit codes for startup failures.
#ifdef _WIN32
constexpr int kExitCodeUnsupportedCpu{
    static_cast<int>(STATUS_ILLEGAL_INSTRUCTION)};
constexpr int kExitCodeAlreadyRunning{ERROR_SINGLE_INSTANCE_APP};
#else
constexpr int kExitCodeUnsupportedCpu{ENOTSUP};
constexpr int kExitCodeAlreadyRunning{EEXIST};
#endif

template <size_t out_size>
const char* PrefixMessageGroup(OUT_Z_ARRAY char (&out)[out_size],
                               IN_Z const char* group,
                               IN_Z const char* message) {
  const char* out_group{GetSpewOutputGroup()};

  out_group = !Q_isempty(out_group) ? out_group : group;

  const size_t length{strlen(message)};
  if (length > 1 && message[length - 1] == '\n' &&
      !V_isuniform(message, '\n')) {
    V_sprintf_safe(out, "[%.3f][%s] %s", Plat_FloatTime(), out_group, message);
  } else {
    V_strcpy_safe(out, message);
  }

  return out;
}

// Spew function!
SpewRetval_t DefaultSpew(SpewType_t spew_type, const char* raw) {
  char message[4096];
  Plat_DebugString(PrefixMessageGroup(message, "launcher", raw));

  switch (spew_type) {
    case SPEW_MESSAGE:
    case SPEW_LOG:
      return SPEW_CONTINUE;

    case SPEW_WARNING:
      if (V_strieq(GetSpewOutputGroup(), "init")) {
        Plat_MessageBox(V_STRINGIFY(VPCGAME) " - Warning", "Warning occurred",
                        message, Plat_MessageBoxButton::Ok,
                        Plat_MessageBoxIcon::Warning);
      }
      return SPEW_CONTINUE;

    case SPEW_ASSERT:
      if (!ShouldUseNewAssertDialog()) {
        Plat_MessageBox(V_STRINGIFY(VPCGAME) " - Assert", "Assert occurred",
                        message, Plat_MessageBoxButton::Ok,
                        Plat_MessageBoxIcon::Information);
      }
      return SPEW_DEBUGGER;

    case SPEW_ERROR:
    default: {
      // exit() runs atexit handlers and static destructors. If one of them (or
      // another thread) reports an error meanwhile we would call exit() again,
      // which is undefined behavior: terminate immediately instead.
      static std::atomic_flag is_exiting{ATOMIC_FLAG_INIT};
      if (is_exiting.test_and_set()) {
        std::_Exit(ENOTRECOVERABLE);
      }

      Plat_MessageBox(V_STRINGIFY(VPCGAME) " - Error", "Error occurred",
                      message, Plat_MessageBoxButton::Ok,
                      Plat_MessageBoxIcon::Error);
      // dimhotepus: Try to run destructors! 1 -> ENOTRECOVERABLE
      std::exit(ENOTRECOVERABLE);
    }
  }
}

// Gets the full path of the executable.
template <int out_size>
bool GetExecutableName(char (&out)[out_size]) {
  static_assert(out_size > 1);
  return Plat_GetModuleFilename(out, out_size);
}

// Last path separator or nullptr.
[[nodiscard]] char* FindLastPathSeparator(char* path) {
  char* last{nullptr};
  for (char* it{path}; *it; ++it) {
    if (*it == CORRECT_PATH_SEPARATOR) last = it;
  }
  return last;
}

// Determine the directory where this .exe is running from
void GetBaseDirectory(ICommandLine* command_line,
                      OUT_Z_ARRAY char (&base_directory)[MAX_PATH]) {
  base_directory[0] = '\0';

  if (GetExecutableName(base_directory)) {
    // Strip the file name and the separator before it, but keep the separator
    // of a root directory: "C:" or "" would not mean the root.
    if (char* separator{FindLastPathSeparator(base_directory)}) {
      const bool is_root{separator == base_directory || separator[-1] == ':'};
      separator[is_root ? 1 : 0] = '\0';
    }
  }

  const char* override_directory{command_line->CheckParm("-basedir")};
  if (override_directory) V_strcpy_safe(base_directory, override_directory);

#ifdef WIN32
  Q_strlower(base_directory);
#endif
  Q_FixSlashes(base_directory);
}

#ifdef WIN32
BOOL WINAPI ConsoleCtrlHandler([[maybe_unused]] DWORD ctrl_type) {
  // dimhotepus: 2 -> ERROR_CONTROL_C_EXIT
  TerminateProcess(GetCurrentProcess(), ERROR_CONTROL_C_EXIT);
  return TRUE;
}
#endif

// Reports a startup failure and returns exit_code.
//
// Unlike Error(), which ends in exit() from DefaultSpew, the caller can return
// a specific exit code and the scoped objects already constructed in
// LauncherMain are destroyed normally.
[[nodiscard]] int FatalError(int exit_code, const char* message) {
  Warning("%s\n", message);
  Plat_MessageBox(V_STRINGIFY(VPCGAME) " - Error", "Unable to start the game",
                  message, Plat_MessageBoxButton::Ok,
                  Plat_MessageBoxIcon::Error);
  return exit_code;
}

// Figure out if Steam is running, then load the GameOverlayRenderer<64>.dll
void TryToLoadSteamOverlayDLL() {
// dimhotepus: NO_STEAM
#if defined(WIN32) && !defined(NO_STEAM)
  // The overlay DLL is architecture-specific.
#ifdef _WIN64
  constexpr char kOverlayDll[]{"GameOverlayRenderer64" DLL_EXT_STRING};
#else
  constexpr char kOverlayDll[]{"GameOverlayRenderer" DLL_EXT_STRING};
#endif

  // First, check if the module is already loaded, perhaps because we were run
  // from Steam directly
  HMODULE module{GetModuleHandle(kOverlayDll)};
  if (module) return;

  if (0 == GetEnvironmentVariableW(L"SteamGameId", nullptr, 0)) {
    // Initializing the Steam client API has the side effect of setting up the
    // AppId which is immediately queried in GameOverlayRenderer.dll's DllMain
    // entry point
    if (SteamAPI_InitSafe()) {
      RunCodeAtScopeExit(SteamAPI_Shutdown());

      const char* steam_install_path{SteamAPI_GetSteamInstallPath()};
      if (steam_install_path) {
        char full_dll_path[MAX_PATH];
        V_ComposeFileName(steam_install_path, kOverlayDll, full_dll_path);

        // This could fail, but we can't fix it if it does so just ignore
        // failures
        LoadLibrary(full_dll_path);
      }
    }
  }
#endif
}

// Remove all but the last -game parameter.  This is for mods based off
// something other than Half-Life 2 (like HL2MP mods).  The Steam UI does 'steam
// -applaunch 320 -game c:\steam\steamapps\sourcemods\modname', but applaunch
// inserts its own -game parameter, which would supercede the one we really want
// if we didn't intercede here.
void RemoveDuplicatedGameParameters(ICommandLine* command_line) {
  // Find the last -game parameter.
  int game_args_count = 0;
  char last_game_arg[MAX_PATH];

  for (int i = 0; i < command_line->ParmCount() - 1; i++) {
    if (V_strieq(command_line->GetParm(i), "-game")) {
      Q_snprintf(last_game_arg, sizeof(last_game_arg), "\"%s\"",
                 command_line->GetParm(i + 1));

      ++game_args_count;
      ++i;
    }
  }

  // We only care if > 1 was specified.
  if (game_args_count > 1) {
    command_line->RemoveParm("-game");
    command_line->AppendParm("-game", last_game_arg);
  }
}

void RemoveParametersOverrides(ICommandLine* command_line) {
  // Remove any overrides in case settings changed
  command_line->RemoveParm("-w");
  command_line->RemoveParm("-h");
  command_line->RemoveParm("-width");
  command_line->RemoveParm("-height");
  command_line->RemoveParm("-sw");
  command_line->RemoveParm("-startwindowed");
  command_line->RemoveParm("-windowed");
  command_line->RemoveParm("-window");
  // dimhotepus: Remove borderless override.
  command_line->RemoveParm("-noborder");
  command_line->RemoveParm("-full");
  command_line->RemoveParm("-fullscreen");
  command_line->RemoveParm("-dxlevel");
  command_line->RemoveParm("-autoconfig");
  command_line->RemoveParm("+mat_hdr_level");
}

#ifdef WIN32
[[nodiscard]] std::error_code ApplyProcessPriorityClass(
    const ICommandLine* command_line) {
  // Make low priority?
  if (command_line->HasParm("-low")) {
    return SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS)
               ? std::error_code{}
               : std::error_code{static_cast<int>(::GetLastError()),
                                 std::system_category()};
  }

  if (command_line->HasParm("-high")) {
    return SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS)
               ? std::error_code{}
               : std::error_code{static_cast<int>(::GetLastError()),
                                 std::system_category()};
  }

  if (!command_line->HasParm("-normal")) {
    // dimhotepus: Above normal by default.
    return SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS)
               ? std::error_code{}
               : std::error_code{static_cast<int>(::GetLastError()),
                                 std::system_category()};
  }

  return {};
}
#endif

// Runs the app, restarting it as requested.  Returns the exit code of the last
// run.
int RunApp(ICommandLine* command_line, const char (&base_directory)[MAX_PATH],
           bool is_text_mode) {
  int rc{0};
  bool need_restart{false};

  do {
    s1::launcher::BootAppSystemGroup systems{command_line, base_directory,
                                             is_text_mode};
    CSteamApplication app{&systems};

    rc = app.Run();

    need_restart = (app.GetErrorStage() ==
                        s1::launcher::BootAppSystemGroup::INITIALIZATION &&
                    rc == INIT_RESTART) ||
                   rc == RUN_RESTART;

    const bool is_reslist_gen{!need_restart &&
                              systems.ShouldContinueGenerateReslist()};

    if (!need_restart) need_restart = is_reslist_gen;

    if (!is_reslist_gen) RemoveParametersOverrides(command_line);
  } while (need_restart);

  return rc == RUN_OK ? 0 : 1;
}

void DumpAppInformation(
#ifdef _WIN32
    LPSTR cmd_line
#else
    int argc, char** argv
#endif
) {
#if defined(POSIX)
#if defined(__clang__)
  const std::string kCompilerVersion{"Clang " __clang_version__};
#elif defined(__GNUC__)
  const std::string kCompilerVersion{"GCC " + std::to_string(__GNUC__) + "." +
                                     std::to_string(__GNUC_MINOR__) + "." +
                                     std::to_string(__GNUC_PATCHLEVEL__)};
#else
#error "Please, add your compiler build version here."
#endif

#ifdef __GLIBCXX__
  Msg("%s v.%s build with %s on libstdc++ v.%d (%d).\n",
      SE_PRODUCT_FILE_DESCRIPTION_STRING, SE_PRODUCT_FILE_VERSION_INFO_STRING,
      kCompilerVersion.c_str(), _GLIBCXX_RELEASE, __GLIBCXX__);
#endif

#ifdef _LIBCPP_VERSION
  Msg("%s v.%s build with %s on libc++ v.%d, ABI v.%d.\n",
      SE_PRODUCT_FILE_DESCRIPTION_STRING, SE_PRODUCT_FILE_VERSION_INFO_STRING,
      kCompilerVersion.c_str(), _LIBCPP_VERSION, _LIBCPP_ABI_VERSION);
#endif

#ifdef __GLIBC__
  Msg("glibc v.%d.%d [compiled], %s [runtime].\n", __GLIBC__, __GLIBC_MINOR__,
      gnu_get_libc_version());
#endif
#endif  // POSIX

#ifdef _WIN32
  Msg("%s v.%s build with MSVC %u.%u\n", SE_PRODUCT_FILE_DESCRIPTION_STRING,
      SE_PRODUCT_FILE_VERSION_INFO_STRING, _MSC_FULL_VER, _MSC_BUILD);
  Msg("%s started with command line '%s'\n", SE_PRODUCT_FILE_DESCRIPTION_STRING,
      cmd_line);
#else
  Msg("%s started with command line args:\n",
      SE_PRODUCT_FILE_DESCRIPTION_STRING);
  for (int i{0}; i < argc; ++i) {
    Msg("  %s\n", argv[i]);
  }
#endif

#ifdef PLATFORM_HAS_ASAN
  Msg("%s running under AddressSanitizer.\n",
      SE_PRODUCT_FILE_DESCRIPTION_STRING);
#endif
}

#ifdef _WIN32
[[nodiscard]] s1::win::FloatingPointExceptionMode GetFloatingPointExceptionMode(
    ICommandLine* command_line) {
  s1::win::FloatingPointExceptionMode mode{
      s1::win::FloatingPointExceptionMode::None};

  if (command_line->HasParm("-check-float-underflow")) {
    mode = mode | s1::win::FloatingPointExceptionMode::Underflow;
  }

  if (command_line->HasParm("-check-float-overflow")) {
    mode = mode | s1::win::FloatingPointExceptionMode::Overflow;
  }

  if (command_line->HasParm("-check-float-zerodivide")) {
    mode = mode | s1::win::FloatingPointExceptionMode::ZeroDivide;
  }

  if (command_line->HasParm("-check-float-invalid")) {
    mode = mode | s1::win::FloatingPointExceptionMode::Invalid;
  }

  return mode;
}

// Points a CRT standard stream at the console.
//
// The stream stays owned by the CRT and must not be fclose()d: spew, atexit
// handlers and static destructors keep writing to it after LauncherMain
// returns.
[[nodiscard]] int RedirectStdStream(FILE* stream, const char* device,
                                    const char* mode, const char* name) {
  FILE* reopened{nullptr};
  if (const errno_t rc{freopen_s(&reopened, device, mode, stream)}; rc != 0) {
    Warning("Unable to redirect %s to console: %s.\n", name,
            std::generic_category().message(rc).c_str());
    return rc;
  }
  return 0;
}
#endif

}  // namespace

// Entry point for the application.
#ifdef WIN32
DLL_EXPORT int LauncherMain(HINSTANCE instance, HINSTANCE, LPSTR cmd_line,
                            [[maybe_unused]] int window_show_flags)
#else
DLL_EXPORT int LauncherMain(int argc, char** argv)
#endif
{
  // Handle crashes and write dumps.
  [[maybe_unused]] auto& scoped_breakpad_handler =
      s1::breakpad::ScopedBreakpadHandler::Instance();

#ifdef WIN32
  SetAppInstance(instance);
#endif

  // Hook the debug output stuff.
  const ScopedSpewOutputFunc scoped_spew_output{DefaultSpew};

  // Dump compiler / libs / app versions.
#ifdef WIN32
  DumpAppInformation(cmd_line);
#else
  DumpAppInformation(argc, argv);
#endif

  // Printf/sscanf functions expect en_US UTF8 localization.  Mac OSX also sets
  // LANG to en_US.UTF-8 before starting up (in info.plist I believe).
  //
  // Starting in Windows 10 version 1803 (10.0.17134.0), the Universal C Runtime
  // supports using a UTF-8 code page.
  //
  // Need to double check that localization for libcef is handled correctly when
  // slam things to en_US.UTF-8.
  constexpr char kEnUsUtf8Locale[]{"en_US.UTF-8"};

  const s1::ScopedAppLocale scoped_app_locale{kEnUsUtf8Locale};
  if (!scoped_app_locale.IsSuccess()) {
    Warning("setlocale('%s') failed, current locale is '%s'.\n",
            kEnUsUtf8Locale, s1::ScopedAppLocale::GetCurrentLocale());
  }

#ifdef POSIX
  // Store off command line for argument searching.
  Plat_SetCommandLine(BuildCmdLine(argc, argv, false));
#endif

  ICommandLine* command_line{CommandLine()};

#ifdef WIN32
  command_line->CreateCmdLine(IsPC() ? VCRHook_GetCommandLine() : cmd_line);
#else
  command_line->CreateCmdLine(argc, argv);
#endif

  if (command_line->HasParm("-sleepatstartup")) {
    // When launching from Steam, it can be difficult to get a debugger attached
    // when you're crashing quickly at startup.
    //
    // Add a -sleepatstartup command line and sleep for 5 seconds which should
    // allow time to attach a debugger.
    ThreadSleep(5000);
  }

#ifdef _WIN32
  // dimhotepus: Allow to catch floating point exceptions which are usually bugs
  // in code.
  const s1::win::ScopedFloatingPointExceptionMode
      scoped_floating_point_exception_mode{
          GetFloatingPointExceptionMode(command_line)};
#endif

  // SSE / RDTSC requirements only apply to x86 builds.
#ifdef PLATFORM_INTEL
  const CPUInformation* cpu_info{GetCPUInformation()};
  if (!cpu_info->m_bSSE || !cpu_info->m_bSSE2 || !cpu_info->m_bSSE3 ||
      !cpu_info->m_bSSE41 || !cpu_info->m_bSSE42) {
    return FatalError(
        kExitCodeUnsupportedCpu,
        "Sorry, your CPU missed SSE / SSE2 / SSE3 / SSE4.1 / SSE4.2 "
        "instructions required for the game.\n\nPlease upgrade your CPU.");
  }

  if (!cpu_info->m_bRDTSC) {
    return FatalError(kExitCodeUnsupportedCpu,
                      "Sorry, your CPU missed RDTSC instruction required for "
                      "the game.\n\nPlease upgrade your CPU.");
  }
#endif

#ifdef WIN32
  if (!IsWindows10OrGreater()) {
    return FatalError(ERROR_OLD_WIN_VERSION,
                      "Sorry, Windows 10+ required to run the game.\n\nPlease "
                      "update your operating system.");
  }
#endif

  // Can only run one windowed source app at a time.
  const s1::launcher::ScopedAppMultiRun scoped_app_multi_run{command_line};
  if (!scoped_app_multi_run.is_single_run()) {
    // Allow the user to explicitly say they want to be able to run multiple
    // instances of the source mutex.  Useful for side-by-side comparisons of
    // different renderers.
    const bool allow_multirun{command_line->HasParm("-multirun")};
    if (!allow_multirun) {
      return FatalError(kExitCodeAlreadyRunning,
                        "Oops, the game is already launched.\n\nSorry, but "
                        "only single game can run at the same time.");
    }
  }

#ifdef WIN32
  // COM is required.
  const s1::win::ScopedCom scoped_com{
      static_cast<COINIT>(COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE |
                          COINIT_SPEED_OVER_MEMORY)};
  if (FAILED(scoped_com.error_code())) {
    const _com_error com_error{scoped_com.error_code()};

    char message[512];
    V_sprintf_safe(message, "Unable to initialize COM (0x%lx): %s.",
                   static_cast<unsigned long>(scoped_com.error_code()),
                   com_error.ErrorMessage());
    return FatalError(static_cast<int>(scoped_com.error_code()), message);
  }

  using namespace std::chrono_literals;
  constexpr std::chrono::milliseconds kSystemTimerResolution{2ms};

  // System timer precision affects Sleep & friends performance.
  const s1::win::ScopedTimerResolution scoped_timer_resolution{
      kSystemTimerResolution};
  if (!scoped_timer_resolution) {
    Warning(
        "Unable to set Windows timer resolution to %lld ms. Will use default "
        "one.\n",
        static_cast<long long>(kSystemTimerResolution.count()));
  }

  const s1::win::ScopedWinsockInitializer scoped_winsock{WINSOCK_VERSION};
  if (const auto rc = scoped_winsock.error_code(); rc) {
    Warning("Windows sockets 2.2+ unavailable (%d): %s.\n", rc.value(),
            rc.message().c_str());
    return FatalError(rc.value(), "Unable to initialize Windows sockets 2.2+.");
  }

  if (const auto rc = ApplyProcessPriorityClass(command_line); rc) {
    Warning("Unable to change game process priority. Will run as is: %s.\n",
            rc.message().c_str());
  }
#endif

  // dimhotepus: Remove Plat_VerifyHardwareKeyPrompt call as it is empty.

  // No -dxlevel or +mat_hdr_level allowed on POSIX.
#ifdef POSIX
  command_line->RemoveParm("-dxlevel");
  command_line->RemoveParm("+mat_hdr_level");
  command_line->RemoveParm("+mat_dxlevel");
#endif

  // If we're using -default command line parameters, get rid of DX8 settings.
  if (command_line->HasParm("-default")) {
    command_line->RemoveParm("-dxlevel");
    command_line->RemoveParm("-maxdxlevel");
    command_line->RemoveParm("+mat_dxlevel");
  }

  char base_directory[MAX_PATH];
  // Figure out the directory the executable is running from.
  GetBaseDirectory(command_line, base_directory);

  // Make the executable's directory the current working directory.
#ifdef _WIN32
  const int chdir_rc{_chdir(base_directory)};
#else
  const int chdir_rc{chdir(base_directory)};
#endif
  if (chdir_rc != 0) {
    const int rc{errno};
    Warning("Unable to change current directory to '%s': %s.\n", base_directory,
            std::generic_category().message(rc).c_str());
    return rc;
  }

  // This call is to emulate steam's injection of the GameOverlay DLL into our
  // process if we are running from the command line directly, this allows the
  // same experience the user gets to be present when running from perforce, the
  // call has no effect on X360
  TryToLoadSteamOverlayDLL();

  auto [vcr_helpers, rc] = s1::launcher::CreateVcrHelpers(command_line);
  if (rc != 0) return rc;

  // See the function for why we do this.
  RemoveDuplicatedGameParameters(command_line);

  // If game is not run from Steam then add -insecure in order to avoid client
  // timeout message.
  if (!command_line->HasParm("-steam")) {
    command_line->AppendParm("-insecure", nullptr);
  }

  // Relaunch app if needed.
  const s1::launcher::ScopedAppRelaunch scoped_app_relaunch;
  // Dump heap leaks if needed.
  const s1::launcher::ScopedHeapLeakDumper scoped_heap_leak_dumper{
      command_line->HasParm("-leakcheck")};

  // Run in text mode (no graphics & sound)?
  const bool is_text_mode{command_line->HasParm("-textmode")};
  if (!is_text_mode) {
    return RunApp(command_line, base_directory, is_text_mode);
  }

#ifdef WIN32
  // ERROR_ACCESS_DENIED means the process already has a console: use it.
  const bool is_console_owned{::AllocConsole() != FALSE};
  if (!is_console_owned) {
    if (const DWORD console_rc{::GetLastError()};
        console_rc != ERROR_ACCESS_DENIED) {
      Warning("Unable to create console for text mode: %s.\n",
              std::system_category().message(console_rc).c_str());
      return static_cast<int>(console_rc);
    }
  }
  RunCodeAtScopeExitOpt(is_console_owned, FreeConsole());

  const s1::win::ScopedConsoleCtrlHandler scoped_console_ctrl_handler{
      ConsoleCtrlHandler};
  if (!scoped_console_ctrl_handler.is_success()) {
    Warning(
        "Unable to set text mode console break handler, run without one.\n");
    // Continue, not fatal.
  }

  if (rc = RedirectStdStream(stdin, "CONIN$", "r", "stdin"); rc != 0) {
    return rc;
  }
  if (rc = RedirectStdStream(stdout, "CONOUT$", "w", "stdout"); rc != 0) {
    return rc;
  }
  if (rc = RedirectStdStream(stderr, "CONOUT$", "w", "stderr"); rc != 0) {
    return rc;
  }
#endif

  return RunApp(command_line, base_directory, is_text_mode);
}