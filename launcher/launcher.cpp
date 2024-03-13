// Copyright Valve Corporation, All rights reserved.
//
// Defines the entry point for the application.

#if defined(_WIN32)
#include "winlite.h"

#include <direct.h>  // _chdir
#include <comdef.h>  // _com_error
#endif

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
#include "tier0/vcrmode.h"

#include "boot_app_system_group.h"
#include "file_logger.h"

#define VERSION_SAFE_STEAM_API_INTERFACES
#include "steam/steam_api.h"

#include "include/SDL3/SDL.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined(USE_SDL)
extern void *CreateSDLMgr();
#endif

namespace {

// Spew function!
SpewRetval_t LauncherDefaultSpewFunc(SpewType_t spew_type,
                                     const char *message) {
  Plat_DebugString(message);

  switch (spew_type) {
    case SPEW_MESSAGE:
    case SPEW_LOG:
      return SPEW_CONTINUE;

    case SPEW_WARNING:
      if (!stricmp(GetSpewOutputGroup(), "init")) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Launcher - Warning",
                                 message, nullptr);
      }
      return SPEW_CONTINUE;

    case SPEW_ASSERT:
      if (!ShouldUseNewAssertDialog()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Launcher - Assert",
                                 message, nullptr);
      }
      return SPEW_DEBUGGER;

    case SPEW_ERROR:
    default:
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Launcher - Error",
                               message, nullptr);
      _exit(1);
  }
}

// Implementation of VCRHelpers.
class CVCRHelpers : public IVCRHelpers {
 public:
  void ErrorMessage(const char *message) override {
#if defined(WIN32) || defined(LINUX)
    NOVCR(SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "VCR Error", message,
                                   nullptr));
#endif
  }

  void *GetMainWindow() override { return nullptr; }
};

// Gets the executable name
template <DWORD outSize>
bool GetExecutableName(char (&out)[outSize]) {
#ifdef WIN32
  if (!::GetModuleFileName(GetModuleHandle(nullptr), out, outSize)) {
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

  if (IsPC()) {
    const char *override_directory{command_line->CheckParm("-basedir")};

    if (override_directory) strcpy(base_directory, override_directory);
  }

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
  (void)freopen("CONIN$", "rb", stdin);
  // reopen stout handle as console window output
  (void)freopen("CONOUT$", "wb", stdout);
  // reopen stderr handle as console window output
  (void)freopen("CONOUT$", "wb", stderr);
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
                          Q_ARRAYSIZE(rgchSteamPath));
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
void RemoveSpuriousGameParameters(ICommandLine *command_line) {
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

int RunApp(ICommandLine *command_line, const char (&base_directory)[MAX_PATH],
           bool is_text_mode) {
  bool need_restart{false};

  do {
    src::launcher::BootAppSystemGroup systems{command_line, base_directory,
                                              is_text_mode};
    CSteamApplication app{&systems};

    const int rc{app.Run()};

    need_restart = (app.GetErrorStage() ==
                        src::launcher::BootAppSystemGroup::INITIALIZATION &&
                    rc == INIT_RESTART) ||
                   rc == RUN_RESTART;

    bool is_reslist_gen =
        !need_restart && systems.ShouldContinueGenerateReslist();

    if (!need_restart) need_restart = is_reslist_gen;

    if (!is_reslist_gen) RemoveParametersOverrides(command_line);
  } while (need_restart);

  return 0;
}

}  // namespace

// Entry point for the application.
#ifdef WIN32
DLL_EXPORT int LauncherMain(HINSTANCE instance, HINSTANCE, LPSTR cmd_line,
                            int window_show_flags)
#else
DLL_EXPORT int LauncherMain(int argc, char **argv)
#endif
{
#ifdef WIN32
  SetAppInstance(instance);
#endif

  // Hook the debug output stuff.
  SpewOutputFunc(LauncherDefaultSpewFunc);

  // Printf/sscanf functions expect en_US UTF8 localization.  Mac OSX also sets
  // LANG to en_US.UTF-8 before starting up (in info.plist I believe).
  //
  // Starting in Windows 10 version 1803 (10.0.17134.0), the Universal C Runtime
  // supports using a UTF-8 code page.
  //
  // Need to double check that localization for libcef is handled correctly when
  // slam things to en_US.UTF-8.
  constexpr char kEnUsUtf8Locale[]{"en_US.UTF-8"};

  const src::launcher::ScopedAppLocale scoped_app_locale{kEnUsUtf8Locale};
  if (Q_stricmp(src::launcher::ScopedAppLocale::GetCurrentLocale(),
                kEnUsUtf8Locale)) {
    Warning("setlocale('%s') failed, current locale is '%s'.\n",
            kEnUsUtf8Locale,
            src::launcher::ScopedAppLocale::GetCurrentLocale());
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
    ThreadSleep(5000);
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

  // COM is required.
  const src::launcher::ScopedCom scoped_com{
      static_cast<COINIT>(COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE |
                          COINIT_SPEED_OVER_MEMORY)};
  if (FAILED(scoped_com.errc())) {
    const _com_error com_error{scoped_com.errc()};
    Error("Unable to initialize COM (0x%x): %s.\n\n", scoped_com.errc(),
          com_error.ErrorMessage());
  }

  using namespace std::chrono_literals;
  constexpr std::chrono::milliseconds kSystemTimerResolution{8ms};

  // System timer precision affects Sleep & friends performance.
  const src::launcher::ScopedTimerResolution scoped_timer_resolution{
      kSystemTimerResolution};
  if (!scoped_timer_resolution) {
    Warning(
        "Unable to set Windows timer resolution to %lld ms. Will use default "
        "one.",
        (long long)kSystemTimerResolution.count());
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

  // Relaunch app if needed.
  const src::launcher::ScopedAppRelaunch scoped_app_relaunch;

  // This call is to emulate steam's injection of the GameOverlay DLL into our
  // process if we are running from the command line directly, this allows the
  // same experience the user gets to be present when running from perforce, the
  // call has no effect on X360
  TryToLoadSteamOverlayDLL();

  const char *vcr_file_path;
  CVCRHelpers vcr_helpers;
  // Start VCR mode?
  if (command_line->CheckParm("-vcrrecord", &vcr_file_path)) {
    if (!VCRStart(vcr_file_path, true, &vcr_helpers)) {
      Error("-vcrrecord: can't open '%s' for writing.\n", vcr_file_path);
      return -1;
    }
  } else if (command_line->CheckParm("-vcrplayback", &vcr_file_path)) {
    if (!VCRStart(vcr_file_path, false, &vcr_helpers)) {
      Error("-vcrplayback: can't open '%s' for reading.\n", vcr_file_path);
      return -1;
    }
  }

  // See the function for why we do this.
  RemoveSpuriousGameParameters(command_line);

#ifdef WIN32
  const src::launcher::ScopedWinsock scoped_winsock{MAKEWORD(2, 0)};
  if (scoped_winsock.errc()) {
    Warning("Windows sockets 2.0 unavailable (%d): %s.\n",
            scoped_winsock.errc(),
            std::system_category().message(scoped_winsock.errc()).c_str());
  }
#endif

  // Run in text mode (no graphics & sound)?
  const bool is_text_mode{command_line->CheckParm("-textmode") &&
                          InitTextMode()};

  // Can only run one windowed source app at a time.
  const src::launcher::ScopedAppMultiRun scoped_app_multi_run;
  if (!scoped_app_multi_run.IsSingleRun()) {
    // Allow the user to explicitly say they want to be able to run multiple
    // instances of the source mutex.  Useful for side-by-side comparisons of
    // different renderers.
    const bool allow_multirun{command_line->CheckParm("-multirun") != nullptr};
    if (!allow_multirun) {
      Error(
          "Oops, the game is already launched\n\nSorry, but only "
          "single game can run at the same time.");

#if defined(WIN32)
      return ERROR_SINGLE_INSTANCE_APP;
#else
      return EEXIST;
#endif
    }
  }

#ifdef WIN32
  // Make low priority?
  if (command_line->CheckParm("-low")) {
    SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
  } else if (command_line->CheckParm("-high")) {
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
  } else if (!command_line->CheckParm("-normal")) {
    // dimhotepus: Above normal by default.
    SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
  }
#endif

  // If game is not run from Steam then add -insecure in order to avoid client
  // timeout message.
  if (!command_line->CheckParm("-steam")) {
    command_line->AppendParm("-insecure", nullptr);
  }

  // Figure out the directory the executable is running from and make that be
  // the current working directory.
  if (_chdir(base_directory)) {
    Warning("Unable to change current directory to %s: %s.", base_directory,
            std::generic_category().message(errno).c_str());
  }

  const src::launcher::ScopedHeapLeakDumper scoped_heap_leak_dumper{
      !!command_line->CheckParm("-leakcheck")};

  return RunApp(command_line, base_directory, is_text_mode);
}
