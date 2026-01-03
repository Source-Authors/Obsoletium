// Copyright Valve Corporation, All rights reserved.

#include "isystem.h"

#include <system_error>

#include "console/conproc.h"
#include "dedicated.h"
#include "engine_hlds_api.h"
#include "mathlib/mathlib.h"
#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "tier0/vcrmode.h"
#include "tier1/strtools.h"
#include "tier1/KeyValues.h"
#include "tier1/checksum_md5.h"
#include "tier2/tier2.h"
#include "idedicatedexports.h"
#include "vgui/vguihelpers.h"
#include "appframework/AppFramework.h"
#include "filesystem_init.h"
#include "vstdlib/cvar.h"
#include "inputsystem/iinputsystem.h"
#include "materialsystem/imaterialsystem.h"

#include "app_version_config.h"
#include "../dedicated_main/resource.h"

#ifdef __GLIBC__
#include <gnu/libc-version.h>
#endif

#ifdef _WIN32
#include "winlite.h"

#include <direct.h>
#else
#include <unistd.h>
#endif

#if defined(_WIN32)
#include "console/TextConsoleWin32.h"
#else
#include "console/TextConsoleUnix.h"
#endif

namespace se::dedicated {

void *FileSystemFactory(const char *pName, int *pReturnCode);
int ProcessConsoleInput(ISystem *system, IDedicatedServerAPI *api);

#ifdef _WIN32
CTextConsoleWin32 console;
#else
CTextConsoleUnix console;
#endif

}  // namespace se::dedicated

namespace {

struct VCRHelpers : public IVCRHelpers {
  void ErrorMessage(const char *message) override {
    fprintf(stderr, "ERROR: %s\n", message);
  }

  void *GetMainWindow() override { return nullptr; }
};

// Run a single VGUI frame.  If is_finished is true, run VGUIFinishedConfig()
// first.
bool DoRunVGUIFrame(bool is_console_mode, bool is_finished = false) {
#ifdef _WIN32
  if (!is_console_mode) {
    if (is_finished) se::dedicated::VGUIFinishedConfig();

    se::dedicated::RunVGUIFrame();
    return true;
  }
#endif

  return false;
}

// Handle the VCRHook PeekMessage loop.  Return true if WM_QUIT received.
bool HandleVCRHook() {
#if defined(_WIN32)
  MSG msg = {};

  bool is_done = false;
  while (VCRHook_PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT) {
      is_done = true;
      break;
    }

    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  if (is_done) return true;
#endif  // _WIN32

  return false;
}

// initialize the console or wait for vgui to start the server
bool ConsoleStartup(bool is_console_mode) {
#ifdef _WIN32
  if (!is_console_mode) {
    se::dedicated::RunVGUIFrame();

    // Run the config screen
    while (se::dedicated::VGUIIsInConfig() && se::dedicated::VGUIIsRunning())
      se::dedicated::RunVGUIFrame();

    if (se::dedicated::VGUIIsStopping()) return false;

    return true;
  }

  if (!se::dedicated::console.Init()) return false;
#endif  // _WIN32

  return true;
}

#ifdef POSIX
char g_posix_exe_name[MAX_PATH];
#endif

template <int size>
bool GetExecutableName(char (&out)[size]) {
#ifdef _WIN32
  if (!::GetModuleFileNameA(::GetModuleHandleA(nullptr), out, size)) {
    return false;
  }
#else
  V_strcpy_safe(out, g_posix_exe_name);
#endif
  return true;
}

// Return the directory where this .exe is running from.
template <int size>
const char *GetBaseDirectory(char (&base_dir)[size],
                             ICommandLine *command_line) {
  base_dir[0] = '\0';

  if (GetExecutableName(base_dir)) {
    char *separator{strrchr(base_dir, CORRECT_PATH_SEPARATOR)};
    if (separator && *separator) {
      *(separator + 1) = '\0';
    }

    const size_t len{strlen(base_dir)};
    if (len > 0) {
      char &sep{base_dir[len - 1]};

      if (sep == CORRECT_PATH_SEPARATOR || sep == INCORRECT_PATH_SEPARATOR) {
        sep = '\0';
      }
    }
  }

  const char *override_dir{command_line->CheckParm("-basedir")};
  if (override_dir) V_strcpy_safe(base_dir, override_dir);

  Q_strlower(base_dir);
  Q_FixSlashes(base_dir);

  return base_dir;
}

void DumpAppInformation(int argc, char **argv) {
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
      SE_PRODUCT_FILE_DESCRIPTION_STRING, SE_PRODUCT_FILE_VERSION_INFO_STRING,
      kCompilerVersion.c_str(), __GLIBC__, __GLIBC_MINOR__,
      gnu_get_libc_version(), _GLIBCXX_RELEASE, __GLIBCXX__);
#endif

#ifdef _LIBCPP_VERSION
  Msg("%s v.%s build with %s on libc++ v.%u, ABI v.%u.\n",
      SE_PRODUCT_FILE_DESCRIPTION_STRING, SE_PRODUCT_FILE_VERSION_INFO_STRING,
      kCompilerVersion.c_str(), _LIBCPP_VERSION, _LIBCPP_ABI_VERSION);
#endif
#endif  // POSIX

#ifdef _WIN32
  Msg("%s v.%s build with MSVC %u.%u\n", SE_PRODUCT_FILE_DESCRIPTION_STRING,
      SE_PRODUCT_FILE_VERSION_INFO_STRING, _MSC_FULL_VER, _MSC_BUILD);
  Msg("%s started with command line args:\n", SE_PRODUCT_FILE_DESCRIPTION_STRING);
  for (int i{0}; i < argc; ++i) {
    Msg("  %s\n", argv[i]);
  }
#endif

#ifdef __SANITIZE_ADDRESS__
  Msg("%s running under AddressSanitizer.\n", SE_PRODUCT_FILE_DESCRIPTION_STRING);
#endif
}

}  // namespace

namespace se::dedicated {

#ifdef _WIN32
bool g_is_console_mode = false;
extern char *gpszCvars;
#endif

SpewRetval_t DedicatedSpewOutputFunc(SpewType_t spew_type, const char *msg);

// Server loop
void RunServer(ISystem *system, IDedicatedServerAPI *api,
               bool is_console_mode) {
#ifdef _WIN32
  if (gpszCvars) api->AddConsoleText(gpszCvars);
#endif

  // Run 2 engine frames first to get the engine to load its resources.
  for (int i = 0; i < 2; i++) {
    DoRunVGUIFrame(is_console_mode);

    if (!api->RunFrame()) return;
  }

  // Run final VGUI frame.
  DoRunVGUIFrame(is_console_mode, true);

  bool is_done = false;

  while (!is_done) {
    // Check on VCRHook_PeekMessage...
    if (HandleVCRHook()) break;

    if (!DoRunVGUIFrame(is_console_mode)) ProcessConsoleInput(system, api);

    if (!api->RunFrame()) is_done = true;

    system->UpdateStatus(0 /* don't force */);
  }
}

ISystem *g_system;

// Instantiate all main libraries.
bool DedicatedAppSystemGroup::Create() {
#ifndef _WIN32
  if (!console.Init()) return false;
#endif

  g_system = system_;

  // Hook the debug output stuff (override the spew func in the appframework)
  SpewOutputFunc(DedicatedSpewOutputFunc);

  // Added the dedicated exports module for the engine to grab
  AppModule_t dedicatedModule = LoadModule(Sys_GetFactoryThis());
  IDedicatedExports *dedicated = AddSystem<IDedicatedExports>(
      dedicatedModule, VENGINE_DEDICATEDEXPORTS_API_VERSION);
  if (!dedicated) return false;

  if ((api_ = system_->LoadModules(this))) {
    std::tuple<ISystem *, IDedicatedServerAPI *, bool> startup_config{
        std::make_tuple(system_, api_, is_console_mode_)};
    dedicated->Startup(&startup_config);

    // Find the input system and tell it to skip Steam Controller initialization
    // (we have to set this flag before Init gets called on the input system).
    //
    // Dedicated server should skip controller initialization to avoid
    // initializing Steam, because we don't want the user to be flagged as
    // "playing" the game.
    auto inputsystem = FindSystem<IInputSystem>(INPUTSYSTEM_INTERFACE_VERSION);
    if (inputsystem) inputsystem->SetSkipControllerInitialization(true);

    return true;
  }

  return false;
}

bool DedicatedAppSystemGroup::PreInit() {
  // A little hack needed because dedicated links directly to filesystem .cpp
  // files
  g_pFullFileSystem = nullptr;

  if (!BaseClass::PreInit()) return false;

  if (!g_pFullFileSystem) return false;

  CFSSteamSetupInfo steamInfo;
  steamInfo.m_pDirectoryName = nullptr;
  steamInfo.m_bOnlyUseDirectoryName = false;
  steamInfo.m_bToolsMode = false;
  steamInfo.m_bSetSteamDLLPath = false;
  steamInfo.m_bSteam = g_pFullFileSystem->IsSteam();
  steamInfo.m_bNoGameInfo = steamInfo.m_bSteam;
  if (FileSystem_SetupSteamEnvironment(steamInfo) != FS_OK) return false;

  CFSMountContentInfo fsInfo;
  fsInfo.m_pFileSystem = g_pFullFileSystem;
  fsInfo.m_bToolsMode = false;
  fsInfo.m_pDirectoryName = steamInfo.m_GameInfoPath;

  if (FileSystem_MountContent(fsInfo) != FS_OK) return false;

#ifdef _WIN32
  g_is_console_mode = is_console_mode_;
#endif

  CreateInterfaceFn factory{GetFactory()};
  auto *inputsystem = static_cast<IInputSystem *>(
      factory(INPUTSYSTEM_INTERFACE_VERSION, nullptr));
  if (inputsystem) inputsystem->SetConsoleTextMode(true);

#ifdef _WIN32
  if (!is_console_mode_) {
    StartVGUI(GetFactory());
  } else
#endif
  {
    if (!system_->CreateConsoleWindow()) return false;
  }

  return true;
}

int DedicatedAppSystemGroup::Main() {
  if (!ConsoleStartup(is_console_mode_)) return -1;

#ifdef _WIN32
  if (!is_console_mode_) RunVGUIFrame();
#endif

  static char base_dir[MAX_PATH];

  // Set up mod information
  ModInfo_t info;
  info.m_pInstance = GetAppInstance();
  info.m_pBaseDirectory = GetBaseDirectory(base_dir, command_line_);
  info.m_pInitialMod = command_line_->ParmValue("-game", "hl2");
  info.m_pInitialGame = command_line_->ParmValue("-defaultgamedir", "hl2");
  info.m_pParentAppSystemGroup = this;
  info.m_bTextMode = command_line_->CheckParm("-textmode");

  if (api_->ModInit(info)) api_->ModShutdown();

  return 0;
}

void DedicatedAppSystemGroup::PostShutdown() {
#ifdef _WIN32
  if (!is_console_mode_) StopVGUI();
#endif

  system_->DestroyConsoleWindow();
  console.ShutDown();

  BaseClass::PostShutdown();
}

void DedicatedAppSystemGroup::Destroy() {}

// This class is a helper class used for steam-based applications.  It loads up
// the file system in preparation for using it to load other required modules
// from steam.
//
// I couldn't use the one in appframework because the dedicated server inlines
// all the filesystem code.
struct DedicatedSteamApp : public CSteamApplication {
  explicit DedicatedSteamApp(CSteamAppSystemGroup *group)
      : CSteamApplication{group} {}

  bool Create() override;
};

bool DedicatedSteamApp::Create() {
  // Add in the cvar factory
  AppModule_t cvar_module{LoadModule(VStdLib_GetICVarFactory())};
  AddSystem(cvar_module, CVAR_INTERFACE_VERSION);

  AppModule_t file_system_module{LoadModule(FileSystemFactory)};
  m_pFileSystem =
      AddSystem<IFileSystem>(file_system_module, FILESYSTEM_INTERFACE_VERSION);

  if (!m_pFileSystem) {
    Warning("Unable to load the file system interface '%s'.\n",
            FILESYSTEM_INTERFACE_VERSION);
    return false;
  }

  return true;
}

/**
 * @brief Shared Posix / Windows entry point.
 * @param argc Args count.
 * @param argv Args values.
 * @param system System abstractions.
 * @param api Dedicated server API.
 * @param is_console_mode Is console mode or not?
 * @return return code.
 */
int BootMain(int argc, char **argv, bool is_console_mode, ISystem *system) {
  // Dump compiler / libs / app versions.
  DumpAppInformation(argc, argv);

  // Protect against exploits do too much damage.
  if (Plat_IsUserAnAdmin()) {
#ifdef _WIN32
    Warning(
        "Sorry, starting server as Administrator group member user is not "
        "allowed.\n");
#else
    Warning(
        "Sorry, starting server as priviledged (ex. root) group member user is "
        "not allowed.\n");
#endif

    return EPERM;
  }

  // clang-format off
#if !defined(POSIX) && !defined(_WIN64)
  __asm {
    fninit
  }
#endif
  // clang-format on

  SetupFPUControlWord();

#ifdef POSIX
  V_strcpy_safe(g_posix_exe_name, *argv);

  // Store off command line for argument searching.
  BuildCmdLine(argc, argv);
#endif

  MathLib_Init(GAMMA, TEXGAMMA, 0.0f, 1);

  ICommandLine *command_line{CommandLine()};
  // Store off command line for argument searching.
  command_line->CreateCmdLine(argc, argv);

#ifndef _WIN32
  Plat_SetCommandLine(command_line->GetCmdLine());
#endif

  VCRHelpers vcr_helpers;
  // Start VCR mode?
  const char *file_name;
  if (command_line->CheckParm("-vcrrecord", &file_name)) {
    if (!VCRStart(file_name, true, &vcr_helpers)) {
      Error("-vcrrecord: can't open '%s' for writing.\n", file_name);
      return EINVAL;
    }
  } else if (command_line->CheckParm("-vcrplayback", &file_name)) {
    if (!VCRStart(file_name, false, &vcr_helpers)) {
      Error("-vcrplayback: can't open '%s' for reading.\n", file_name);
      return EINVAL;
    }
  }

  // Figure out the directory the executable is running from and make that be
  // the current working directory.
  char base_dir[MAX_PATH];
  if (chdir(GetBaseDirectory(base_dir, command_line))) {
    int rc = errno;
    Error("Unable to change current directory to '%s': %s.\n", base_dir,
          std::generic_category().message(rc).c_str());
    return rc;
  }

  // Rehook the command line through VCR mode.
  command_line->CreateCmdLine(VCRHook_GetCommandLine());

  DedicatedAppSystemGroup dedicated_group{system, command_line,
                                          is_console_mode};
  DedicatedSteamApp app{&dedicated_group};

  return app.Run();
}

}  // namespace se::dedicated
