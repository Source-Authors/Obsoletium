// Copyright Valve Corporation, All rights reserved.

#include <chrono>
#include <cstdarg>
#include <system_error>
#include <variant>

#include "winlite.h"
#include <WinSock2.h>
#include <DbgHelp.h>
#include <VersionHelpers.h>

#include "args_win.h"
#include "engine_hlds_api.h"
#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "tier0/minidump.h"
#include "tier1/strtools.h"
#include "inputsystem/iinputsystem.h"
#include "vgui/vguihelpers.h"
#include "appframework/AppFramework.h"
#include "materialsystem/imaterialsystem.h"
#include "istudiorender.h"
#include "vgui/IVGui.h"
#include "console/TextConsoleWin32.h"
#include "icvar.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "vphysics_interface.h"
#include "filesystem.h"
#include "tier0/scoped_breakpad_handler.h"
#include "filesystem/IQueuedLoader.h"
#include "windows/scoped_timer_resolution.h"
#include "windows/scoped_winsock_initializer.h"

#include "isystem.h"
#include "dedicated.h"
#include "console/conproc.h"
#include "console/textconsole.h"

#include "vscript/ivscript.h"

#include "scoped_app_locale.h"

namespace s1::dedicated {

extern CTextConsoleWin32 console;

// Loose implementation for operating system specific layer.
class WindowsSystem : public ISystem {
 public:
  explicit WindowsSystem(ICommandLine* command_line, bool is_console_mode)
      : command_line_{command_line}, is_console_mode_{is_console_mode} {}
  ~WindowsSystem() override = default;

  WindowsSystem(const WindowsSystem&) = delete;
  WindowsSystem& operator=(const WindowsSystem&) = delete;

  IDedicatedServerAPI* LoadModules(
      DedicatedAppSystemGroup* app_system_group) override;

  void ErrorMessage(int level, const char* msg) override;

  void WriteStatusText(char* status_text) override;
  void UpdateStatus(int force) override;

  bool CreateConsoleWindow() override;
  void DestroyConsoleWindow() override;

  void ConsoleOutput(char* string) override;
  char* ConsoleInput(int index, char* buf, size_t size) override;
  void Printf(PRINTF_FORMAT_STRING const char* fmt, ...) override;

 private:
  ICommandLine* command_line_;
  IDedicatedServerAPI* api_{nullptr};
  double last_status_update_time_{0.0};
  const bool is_console_mode_;
};

void WindowsSystem::ErrorMessage([[maybe_unused]] int level, const char* msg) {
  Plat_MessageBox("SRCDS - Fatal Error", "SRCDS Error", msg,
                  Plat_MessageBoxButton::Ok, Plat_MessageBoxIcon::Error);
  // Fatal error: do not report success to the parent process.
  PostQuitMessage(1);
}

void WindowsSystem::UpdateStatus(int force) {
  if (!api_) return;

  const double now_time{Plat_FloatTime()};

  if (!force && now_time - last_status_update_time_ < 0.5) return;

  last_status_update_time_ = now_time;

  int active_players{0}, max_players{0};
  char map_name[64]{};
  char host_name[128]{};
  float fps{0.0f};

  api_->UpdateStatus(&fps, &active_players, &max_players, map_name,
                     sizeof(map_name));
  api_->UpdateHostname(host_name, sizeof(host_name));

#ifdef PLATFORM_64BITS
  V_strcat_safe(host_name, " - 64 Bit");
#endif
  console.SetTitle(host_name);

  char prompt[256];
  V_sprintf_safe(prompt, "%.1f fps %2i/%2i on map %16s", fps, active_players,
                 max_players, map_name);

  console.SetStatusLine(prompt);
  console.UpdateStatus();
}

void WindowsSystem::ConsoleOutput(char* string) {
  if (!is_console_mode_) {
    VGUIPrintf(string);
  } else {
    console.Print(string);
  }
}

void WindowsSystem::Printf(PRINTF_FORMAT_STRING const char* fmt, ...) {
  // Dump text to debugging console.
  va_list argptr;
  char message[1024];

  va_start(argptr, fmt);
  V_vsprintf_safe(message, fmt, argptr);
  va_end(argptr);

  ConsoleOutput(message);
}

char* WindowsSystem::ConsoleInput(int index, char* buf, size_t buflen) {
  return console.GetLine(index, buf, buflen);
}

void WindowsSystem::WriteStatusText(char* status) { SetConsoleTitleA(status); }

bool WindowsSystem::CreateConsoleWindow() {
  if (!::AllocConsole()) return false;

  if (!StartupRemoteConsole(command_line_, this)) {
    // Do not leak the console we just allocated.
    ::FreeConsole();
    return false;
  }

  return true;
}

void WindowsSystem::DestroyConsoleWindow() {
  ::FreeConsole();

  // shut down QHOST hooks if necessary
  ShutdownRemoteConsole();
}

// Loading modules used by the dedicated server.
IDedicatedServerAPI* WindowsSystem::LoadModules(
    DedicatedAppSystemGroup* app_system_group) {
  AppSystemInfo_t appSystems[] = {
      // NOTE: This one must be first!!
      {"engine" DLL_EXT_STRING, CVAR_QUERY_INTERFACE_VERSION},
      {"inputsystem" DLL_EXT_STRING, INPUTSYSTEM_INTERFACE_VERSION},
      {"materialsystem" DLL_EXT_STRING, MATERIAL_SYSTEM_INTERFACE_VERSION},
      {"studiorender" DLL_EXT_STRING, STUDIO_RENDER_INTERFACE_VERSION},
      {"vphysics" DLL_EXT_STRING, VPHYSICS_INTERFACE_VERSION},
      {"datacache" DLL_EXT_STRING, DATACACHE_INTERFACE_VERSION},
      {"datacache" DLL_EXT_STRING, MDLCACHE_INTERFACE_VERSION},
      {"datacache" DLL_EXT_STRING, STUDIO_DATA_CACHE_INTERFACE_VERSION},
      {"vgui2" DLL_EXT_STRING, VGUI_IVGUI_INTERFACE_VERSION},
      {"engine" DLL_EXT_STRING, VENGINE_HLDS_API_VERSION},
      {"dedicated" DLL_EXT_STRING, QUEUEDLOADER_INTERFACE_VERSION},
      {"vscript" DLL_EXT_STRING, VSCRIPT_INTERFACE_VERSION},
      {"", ""}  // Required to terminate the list
  };

  if (!app_system_group->AddSystems(appSystems)) return nullptr;

  auto* material_system = app_system_group->FindSystem<IMaterialSystem>(
      MATERIAL_SYSTEM_INTERFACE_VERSION);
  if (!material_system) {
    Warning("Unable to find material system interface '%s'.\n",
            MATERIAL_SYSTEM_INTERFACE_VERSION);
    return nullptr;
  }
  material_system->SetShaderAPI("shaderapiempty" DLL_EXT_STRING);

  api_ = app_system_group->FindSystem<IDedicatedServerAPI>(
      VENGINE_HLDS_API_VERSION);
  if (!api_) {
    Warning("Unable to find dedicated server interface '%s'.\n",
            VENGINE_HLDS_API_VERSION);
  }

  return api_;
}

int BootMain(int argc, char** argv, bool is_console_mode, ISystem* system);

}  // namespace s1::dedicated

namespace {


}  // namespace

/**
 * @brief Entry point.
 * @param instance Dedicated server module instance.
 * @param cmd_line Command line (unused, full command line is re-read).
 * @param cmd_show Show window state (unused).
 * @return Exit code.
 */
DLL_EXPORT int DedicatedMain(HINSTANCE instance, HINSTANCE,
                             [[maybe_unused]] LPSTR cmd_line,
                             [[maybe_unused]] int cmd_show) {
  // Handle crashes and write dumps.
  [[maybe_unused]] auto& scoped_breakpad_handler =
      s1::breakpad::ScopedBreakpadHandler::Instance();

  SetAppInstance(instance);

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

  const CPUInformation* cpu_info{GetCPUInformation()};
  if (!cpu_info->m_bSSE || !cpu_info->m_bSSE2 || !cpu_info->m_bSSE3 ||
      !cpu_info->m_bSSE41 || !cpu_info->m_bSSE42) {
    Warning(
        "Sorry, your CPU missed SSE / SSE2 / SSE3 / SSE4.1 / SSE4.2 "
        "instructions required for the game.\n\nPlease upgrade your CPU.");
    return STATUS_ILLEGAL_INSTRUCTION;
  }

  if (!cpu_info->m_bRDTSC) {
    Warning(
        "Sorry, your CPU missed RDTSC instruction required for the "
        "game.\n\nPlease upgrade your CPU.");
    return STATUS_ILLEGAL_INSTRUCTION;
  }

  if (!::IsWindows10OrGreater()) {
    Warning(
        "Sorry, Windows 10+ required to run the game.\n\nPlease update your "
        "operating system.");
    return ERROR_OLD_WIN_VERSION;
  }

  const auto args_result =
      s1::dedicated::Args::FromCommandLine(::GetCommandLineW());
  if (const auto* rc = std::get_if<std::error_code>(&args_result)) {
    Warning("Sorry, unable to parse command line arguments: %s.\n",
            rc->message().c_str());
    return rc->value();
  }

  const s1::dedicated::Args* args{
      std::get_if<s1::dedicated::Args>(&args_result)};
  ICommandLine* command_line{CommandLine()};
  command_line->CreateCmdLine(VCRHook_GetCommandLine());

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
  if (scoped_winsock.error_code()) {
    Warning("Windows sockets 2.2+ unavailable (%d): %s.\n",
            scoped_winsock.error_code().value(),
            scoped_winsock.error_code().message().c_str());
    // Can't run without sockets.
    return scoped_winsock.error_code().value();
  }

  const bool is_console_mode{command_line->HasParm("-console")};
  s1::dedicated::WindowsSystem system{command_line, is_console_mode};

  // dimhotepus: Now we write dumps using breakpad.

  return s1::dedicated::BootMain(args->count(), args->values(), is_console_mode,
                                 &system);
}