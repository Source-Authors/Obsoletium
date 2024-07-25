// Copyright Valve Corporation, All rights reserved.
//
// Defines the entry point for the application.

#if defined(_WIN32)
#include "winlite.h"

#include <direct.h>  // _chdir
#include <comdef.h>  // _com_error
#endif

#include <thread>

#include "app_version_config.h"
#include "scoped_app_locale.h"
#include "scoped_app_multirun.h"
#include "scoped_app_relaunch.h"
#include "scoped_com.h"
#include "scoped_heap_leak_dumper.h"

#ifdef _WIN32
#include "scoped_timer_resolution.h"
#include "scoped_winsock.h"
#endif

#include "appframework/AppFramework.h"
#include "appframework/ilaunchermgr.h"

#include "tier0/dbg.h"
#include "tier0/icommandline.h"

#include "boot_app_system_group.h"
#include "file_logger.h"
#include "vcr_helpers.h"

#define VERSION_SAFE_STEAM_API_INTERFACES
#include "steam/steam_api.h"

#include "include/SDL3/SDL.h"

#ifdef __GLIBC__
#include <gnu/libc-version.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined(USE_SDL)
extern void *CreateSDLMgr();
#endif

namespace {

// Spew function!
SpewRetval_t LauncherDefaultSpewFunc(SpewType_t spew_type, const char *raw) {
  char message[4096];

  constexpr char engineGroup[] = "launcher";
  const char *group = GetSpewOutputGroup();

  group = group && group[0] ? group : engineGroup;

  Q_snprintf(message, std::size(message), "[%s] %s", group, raw);

  Plat_DebugString(message);

  switch (spew_type) {
    case SPEW_MESSAGE:
    case SPEW_LOG:
      return SPEW_CONTINUE;

    case SPEW_WARNING:
      if (!stricmp(GetSpewOutputGroup(), "init")) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING,
                                 "Source Launcher - Warning", message, nullptr);
      }
      return SPEW_CONTINUE;

    case SPEW_ASSERT:
      if (!ShouldUseNewAssertDialog()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 "Source Launcher - Assert", message, nullptr);
      }
      return SPEW_DEBUGGER;

    case SPEW_ERROR:
    default:
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Source Launcher - Error",
                               message, nullptr);
      _exit(1);
  }
}

// Gets the executable name
template <DWORD out_size>
bool GetExecutableName(char (&out)[out_size]) {
#ifdef WIN32
  if (!::GetModuleFileName(GetModuleHandle(nullptr), out, out_size)) {
    return false;
  }
  return true;
#else
  return false;
#endif
}

// Determine the directory where this .exe is running from
void GetBaseDirectory(ICommandLine *command_line,
                      __out_z char (&base_directory)[MAX_PATH]) {
  base_directory[0] = '\0';

  if (!base_directory[0] && GetExecutableName(base_directory)) {
    char *buffer = strrchr(base_directory, '\\');

    if (buffer && *buffer) *(buffer + 1) = '\0';

    const size_t length{strlen(base_directory)};

    if (length > 0) {
      char &end = base_directory[length - 1];

      if (end == '\\' || end == '/') end = '\0';
    }
  }

  const char *override_directory{command_line->CheckParm("-basedir")};
  if (override_directory) V_strcpy_safe(base_directory, override_directory);

#ifdef WIN32
  Q_strlower(base_directory);
#endif
  Q_FixSlashes(base_directory);
}

#ifdef WIN32
BOOL WINAPI ConsoleCtrlHandler(DWORD ctrl_type) {
  TerminateProcess(GetCurrentProcess(), 2);
  return TRUE;
}
#endif

bool InitTextMode() {
  bool ok{true};

#ifdef WIN32
  ok = AllocConsole() != 0;
  if (ok) {
    ok = SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE) != 0;
  }

  // reopen stdin handle as console window input
  // reopen stout handle as console window output
  // reopen stderr handle as console window output
  ok = !!freopen("CONIN$", "rb", stdin) && !!freopen("CONOUT$", "wb", stdout) &&
       !!freopen("CONOUT$", "wb", stderr);
#endif

  return ok;
}

// Figure out if Steam is running, then load the GameOverlayRenderer.dll
void TryToLoadSteamOverlayDLL() {
// dimhotepus: NO_STEAM
#if defined(WIN32) && !defined(NO_STEAM)
  // First, check if the module is already loaded, perhaps because we were run
  // from Steam directly
  HMODULE hMod = GetModuleHandle("GameOverlayRenderer" DLL_EXT_STRING);
  if (hMod) {
    return;
  }

  if (0 == GetEnvironmentVariableA("SteamGameId", NULL, 0)) {
    // Initializing the Steam client API has the side effect of setting up the
    // AppId which is immediately queried in GameOverlayRenderer.dll's DllMain
    // entry point
    if (SteamAPI_InitSafe()) {
      const char *pchSteamInstallPath = SteamAPI_GetSteamInstallPath();
      if (pchSteamInstallPath) {
        char rgchSteamPath[MAX_PATH];
        V_ComposeFileName(pchSteamInstallPath,
                          "GameOverlayRenderer" DLL_EXT_STRING, rgchSteamPath,
                          ssize(rgchSteamPath));
        // This could fail, but we can't fix it if it does so just ignore
        // failures
        LoadLibrary(rgchSteamPath);
      }

      SteamAPI_Shutdown();
    }
  }

#endif
}

// Remove all but the last -game parameter.  This is for mods based off
// something other than Half-Life 2 (like HL2MP mods).  The Steam UI does 'steam
// -applaunch 320 -game c:\steam\steamapps\sourcemods\modname', but applaunch
// inserts its own -game parameter, which would supercede the one we really want
// if we didn't intercede here.
void RemoveDuplicatedGameParameters(ICommandLine *command_line) {
  // Find the last -game parameter.
  int game_args_count = 0;
  char last_game_arg[MAX_PATH];

  for (int i = 0; i < command_line->ParmCount() - 1; i++) {
    if (Q_stricmp(command_line->GetParm(i), "-game") == 0) {
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

void RemoveParametersOverrides(ICommandLine *command_line) {
  // Remove any overrides in case settings changed
  command_line->RemoveParm("-w");
  command_line->RemoveParm("-h");
  command_line->RemoveParm("-width");
  command_line->RemoveParm("-height");
  command_line->RemoveParm("-sw");
  command_line->RemoveParm("-startwindowed");
  command_line->RemoveParm("-windowed");
  command_line->RemoveParm("-window");
  command_line->RemoveParm("-full");
  command_line->RemoveParm("-fullscreen");
  command_line->RemoveParm("-dxlevel");
  command_line->RemoveParm("-autoconfig");
  command_line->RemoveParm("+mat_hdr_level");
}

#ifdef WIN32
bool ApplyProcessPriorityClass(ICommandLine *command_line) {
  // Make low priority?
  if (command_line->CheckParm("-low")) {
    return !!SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
  }

  if (command_line->CheckParm("-high")) {
    return !!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
  }

  if (!command_line->CheckParm("-normal")) {
    // dimhotepus: Above normal by default.
    return !!SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
  }

  return true;
}
#endif

int RunApp(ICommandLine *command_line, const char (&base_directory)[MAX_PATH],
           bool is_text_mode) {
  bool need_restart{false};

  do {
    se::launcher::BootAppSystemGroup systems{command_line, base_directory,
                                             is_text_mode};
    CSteamApplication app{&systems};

    const int rc{app.Run()};

    need_restart = (app.GetErrorStage() ==
                        se::launcher::BootAppSystemGroup::INITIALIZATION &&
                    rc == INIT_RESTART) ||
                   rc == RUN_RESTART;

    bool is_reslist_gen =
        !need_restart && systems.ShouldContinueGenerateReslist();

    if (!need_restart) need_restart = is_reslist_gen;

    if (!is_reslist_gen) RemoveParametersOverrides(command_line);
  } while (need_restart);

  return 0;
}

void DumpAppInformation(
#ifdef _WIN32
    LPSTR cmd_line
#else
    int argc, char **argv
#endif
) {
#if defined(POSIX)
#if defined(__clang__)
  const std::string kCompilerVersion{"Clang " __clang_version__};
#elif defined(__GCC__)
  const std::string kCompilerVersion{std::to_string(__GNUC__) + "." +
                                     std::to_string(__GNUC_MINOR__) + "." +
                                     std::to_string(__GNUC_PATCHLEVEL__)};
#else
#error "Please, add your compiler build version here."
#endif  // __GCC__

#ifdef __GLIBCXX__
  Msg("%s v.%s build with %s on glibc v.%u.%u [compiled], %s [runtime]. "
      "glibc++ v.%u, ABI v.%u.\n",
      SRC_PRODUCT_NAME_STRING, SRC_PRODUCT_FILE_VERSION_INFO_STRING,
      kCompilerVersion.c_str(), __GLIBC__, __GLIBC_MINOR__,
      gnu_get_libc_version(), _GLIBCXX_RELEASE, __GLIBCXX__);
#endif

#ifdef _LIBCPP_VERSION
  Msg("%s v.%s build with %s on libc++ v.%u, ABI v.%u.\n",
      SRC_PRODUCT_NAME_STRING, SRC_PRODUCT_FILE_VERSION_INFO_STRING,
      kCompilerVersion.c_str(), _LIBCPP_VERSION, _LIBCPP_ABI_VERSION);
#endif
#endif  // POSIX

#ifdef _WIN32
  Msg("%s v.%s build with MSVC %u.%u\n", SRC_PRODUCT_NAME_STRING,
      SRC_PRODUCT_FILE_VERSION_INFO_STRING, _MSC_FULL_VER, _MSC_BUILD);
  Msg("%s started with command line '%s'\n", SRC_PRODUCT_NAME_STRING, cmd_line);
#else
  Msg("%s started with command line args:\n", SRC_PRODUCT_NAME_STRING);
  for (int i{0}; i < argc; ++i) {
    Msg("  %s\n", argv[i]);
  }
#endif

#ifdef __SANITIZE_ADDRESS__
  Msg("%s running under AddressSanitizer.\n", SRC_PRODUCT_NAME_STRING);
#endif
}

}  // namespace

// Entry point for the application.
#ifdef WIN32
DLL_EXPORT int LauncherMain(HINSTANCE instance, HINSTANCE, LPSTR cmd_line,
                            [[maybe_unused]] int window_show_flags)
#else
DLL_EXPORT int LauncherMain(int argc, char **argv)
#endif
{
#ifdef WIN32
  SetAppInstance(instance);
#endif

  // Hook the debug output stuff.
  SpewOutputFunc(LauncherDefaultSpewFunc);

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

  const se::launcher::ScopedAppLocale scoped_app_locale{kEnUsUtf8Locale};
  if (Q_stricmp(se::launcher::ScopedAppLocale::GetCurrentLocale(),
                kEnUsUtf8Locale)) {
    Warning("setlocale('%s') failed, current locale is '%s'.\n",
            kEnUsUtf8Locale, se::launcher::ScopedAppLocale::GetCurrentLocale());
  }

#ifdef POSIX
  // Store off command line for argument searching.
  Plat_SetCommandLine(BuildCmdLine(argc, argv, false));
#endif

  ICommandLine *command_line{CommandLine()};

#ifdef WIN32
  command_line->CreateCmdLine(IsPC() ? VCRHook_GetCommandLine() : cmd_line);
#else
  command_line->CreateCmdLine(argc, argv);
#endif

  if (command_line->CheckParm("-sleepatstartup")) {
    // When launching from Steam, it can be difficult to get a debugger attached
    // when you're crashing quickly at startup.
    //
    // Add a -sleepatstartup command line and sleep for 5 seconds which should
    // allow time to attach a debugger.
    using namespace std::chrono_literals;

    std::this_thread::sleep_for(5000ms);
  }

  const CPUInformation *cpu_info{GetCPUInformation()};
  if (!cpu_info->m_bSSE || !cpu_info->m_bSSE2 || !cpu_info->m_bSSE3 ||
      !cpu_info->m_bSSE41 || !cpu_info->m_bSSE42) {
    Error(
        "Sorry, your CPU missed SSE / SSE2 / SSE3 / SSE4.1 / SSE4.2 "
        "instructions required for the game.\n\nPlease upgrade your CPU.");
    return STATUS_ILLEGAL_INSTRUCTION;
  }

  if (!cpu_info->m_bRDTSC) {
    Error(
        "Sorry, your CPU missed RDTSC instruction required for the "
        "game.\n\nPlease upgrade your CPU.");
    return STATUS_ILLEGAL_INSTRUCTION;
  }

#ifdef WIN32
  if (!IsWindows10OrGreater()) {
    Error(
        "Sorry, Windows 10+ required to run the game.\n\nPlease update your "
        "operating system.");
    return ERROR_OLD_WIN_VERSION;
  }
#endif

  // Can only run one windowed source app at a time.
  const se::launcher::ScopedAppMultiRun scoped_app_multi_run;
  if (!scoped_app_multi_run.is_single_run()) {
    // Allow the user to explicitly say they want to be able to run multiple
    // instances of the source mutex.  Useful for side-by-side comparisons of
    // different renderers.
    const bool allow_multirun{command_line->CheckParm("-multirun") != nullptr};
    if (!allow_multirun) {
      Error(
          "Oops, the game is already launched\n\nSorry, but only "
          "single game can run at the same time.");

#ifdef WIN32
      return ERROR_SINGLE_INSTANCE_APP;
#else
      return EEXIST;
#endif
    }
  }

#ifdef WIN32
  // COM is required.
  const se::launcher::ScopedCom scoped_com{
      static_cast<COINIT>(COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE |
                          COINIT_SPEED_OVER_MEMORY)};
  if (FAILED(scoped_com.errc())) {
    const _com_error com_error{scoped_com.errc()};
    Error("Unable to initialize COM (0x%x): %s.\n\n", scoped_com.errc(),
          com_error.ErrorMessage());
  }

  using namespace std::chrono_literals;
  constexpr std::chrono::milliseconds kSystemTimerResolution{2ms};

  // System timer precision affects Sleep & friends performance.
  const se::launcher::ScopedTimerResolution scoped_timer_resolution{
      kSystemTimerResolution};
  if (!scoped_timer_resolution) {
    Warning(
        "Unable to set Windows timer resolution to %lld ms. Will use default "
        "one.",
        static_cast<long long>(kSystemTimerResolution.count()));
  }

  const se::launcher::ScopedWinsock scoped_winsock{MAKEWORD(2, 0)};
  if (scoped_winsock.errc()) {
    Warning("Windows sockets 2.0 unavailable (%d): %s.\n",
            scoped_winsock.errc(),
            std::system_category().message(scoped_winsock.errc()).c_str());
  }

  if (!ApplyProcessPriorityClass(command_line)) {
    Warning("Unable to change game process priority. Will run as is: %s.",
            std::system_category().message(::GetLastError()).c_str());
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
  if (command_line->CheckParm("-default")) {
    command_line->RemoveParm("-dxlevel");
    command_line->RemoveParm("-maxdxlevel");
    command_line->RemoveParm("+mat_dxlevel");
  }

  char base_directory[MAX_PATH];
  // Figure out the directory the executable is running from.
  GetBaseDirectory(command_line, base_directory);

  // Figure out the directory the executable is running from and make that be
  // the current working directory.
#ifdef WIN32
  if (_chdir(base_directory)) {
#else
  if (chdir(base_directory)) {
#endif
    Warning("Unable to change current directory to %s: %s.", base_directory,
            std::generic_category().message(errno).c_str());
  }

  // This call is to emulate steam's injection of the GameOverlay DLL into our
  // process if we are running from the command line directly, this allows the
  // same experience the user gets to be present when running from perforce, the
  // call has no effect on X360
  TryToLoadSteamOverlayDLL();

  auto [vcr_helpers, rc] = se::launcher::CreateVcrHelpers(command_line);
  if (rc != 0) return rc;

  // See the function for why we do this.
  RemoveDuplicatedGameParameters(command_line);

  // If game is not run from Steam then add -insecure in order to avoid client
  // timeout message.
  if (!command_line->CheckParm("-steam")) {
    command_line->AppendParm("-insecure", nullptr);
  }

  // Relaunch app if needed.
  const se::launcher::ScopedAppRelaunch scoped_app_relaunch;
  // Dump heap leaks if needed.
  const se::launcher::ScopedHeapLeakDumper scoped_heap_leak_dumper{
      !!command_line->CheckParm("-leakcheck")};

  // Run in text mode (no graphics & sound)?
  const bool is_text_mode{command_line->CheckParm("-textmode") &&
                          InitTextMode()};

  rc = RunApp(command_line, base_directory, is_text_mode);

  SpewOutputFunc(nullptr);

  return rc;
}
