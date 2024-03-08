// Copyright Valve Corporation, All rights reserved.
//
// Defines the entry point for the application.

#if defined(_WIN32)
#include "winlite.h"

#include <direct.h>
#include <shellapi.h>
#include <shlwapi.h>  // registry stuff
#include <winsock.h>
#include <comdef.h>
#endif

#include <chrono>
#include <clocale>

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
#include "appframework/IAppSystem.h"

#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "tier0/memalloc.h"
#include "tier0/platform.h"
#include "tier0/vcrmode.h"
#include "tier1/fmtstr.h"
#include "tier1/interface.h"
#include "tier1/tier1.h"
#include "tier1/utlrbtree.h"
#include "tier2/tier2.h"
#include "tier3/tier3.h"

#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "engine_launcher_api.h"
#include "filesystem.h"
#include "filesystem_init.h"
#include "filesystem/IQueuedLoader.h"
#include "ifilesystem.h"
#include "inputsystem/iinputsystem.h"
#include "istudiorender.h"
#include "iregistry.h"
#include "IHammer.h"
#include "materialsystem/imaterialsystem.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui/VGUI.h"
#include "video/ivideoservices.h"
#include "vphysics_interface.h"
#include "vstdlib/iprocessutils.h"
#include "reslistgenerator.h"
#include "sourcevr/isourcevirtualreality.h"

#define VERSION_SAFE_STEAM_API_INTERFACES
#include "steam/steam_api.h"

#if defined(USE_SDL)
#include "include/SDL3/SDL.h"

#if !defined(_WIN32)
#define MB_OK 0x00000001
#define MB_SYSTEMMODAL 0x00000002
#define MB_ICONERROR 0x00000004
int MessageBox(HWND hWnd, const char *message, const char *header,
               unsigned uType);
#endif  // _WIN32

#endif  // USE_SDL

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define DEFAULT_HL2_GAMEDIR "hl2"

#if defined(USE_SDL)
extern void *CreateSDLMgr();
#endif

void SortResList(char const *pchFileName, char const *pchSearchPath);

namespace {

// Spew function!
SpewRetval_t LauncherDefaultSpewFunc(SpewType_t spew_type,
                                     char const *message) {
#ifndef _CERT
  Plat_DebugString(message);

  switch (spew_type) {
    case SPEW_MESSAGE:
    case SPEW_LOG:
      return SPEW_CONTINUE;

    case SPEW_WARNING:
      if (!stricmp(GetSpewOutputGroup(), "init")) {
#if defined(WIN32) || defined(USE_SDL)
        ::MessageBox(NULL, message, "Launcher - Warning",
                     MB_OK | MB_SYSTEMMODAL | MB_ICONERROR);
#endif
      }
      return SPEW_CONTINUE;

    case SPEW_ASSERT:
      if (!ShouldUseNewAssertDialog()) {
#if defined(WIN32) || defined(USE_SDL)
        ::MessageBox(NULL, message, "Launcher - Assert",
                     MB_OK | MB_SYSTEMMODAL | MB_ICONERROR);
#endif
      }
      return SPEW_DEBUGGER;

    case SPEW_ERROR:
    default:
#if defined(WIN32) || defined(USE_SDL)
      ::MessageBox(NULL, message, "Launcher - Error",
                   MB_OK | MB_SYSTEMMODAL | MB_ICONERROR);
#endif
      _exit(1);
  }
#else
  if (spew_type != SPEW_ERROR) return SPEW_CONTINUE;
  _exit(1);
#endif
}

// Implementation of VCRHelpers.
class CVCRHelpers : public IVCRHelpers {
 public:
  virtual void ErrorMessage(const char *message) {
#if defined(WIN32) || defined(LINUX)
    NOVCR(::MessageBox(NULL, message, "VCR Error", MB_OK));
#endif
  }

  virtual void *GetMainWindow() { return nullptr; }
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
#ifdef WIN32
  bool ok{AllocConsole() != 0};
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

#define ALL_RESLIST_FILE "all.lst"
#define ENGINE_RESLIST_FILE "engine.lst"

// create file to dump out to
class CLogAllFiles {
 public:
  CLogAllFiles();
  void Init(ICommandLine *command_line, char (&base_directory)[MAX_PATH]);
  void Shutdown();
  void LogFile(const char *full_path_file_name, const char *options);

 private:
  static void LogAllFilesFunc(const char *full_path_file_name,
                              const char *options);
  void LogToAllReslist(char const *line);

  char m_szCurrentDir[_MAX_PATH];
  char m_szBaseDir[_MAX_PATH];
  ICommandLine *m_commandLine;

  bool m_bActive;

  // persistent across restarts
  CUtlRBTree<CUtlString, int> m_Logged;
  CUtlString m_sResListDir;
  CUtlString m_sFullGamePath;
};

static CLogAllFiles *g_LogFiles = nullptr;

bool AllLogLessFunc(CUtlString const &l, CUtlString const &r) {
  return CaselessStringLessThan(l.Get(), r.Get());
}

CLogAllFiles::CLogAllFiles()
    : m_bActive(false), m_Logged(0, 0, AllLogLessFunc) {
  MEM_ALLOC_CREDIT();
  m_szBaseDir[0] = '\0';
  m_szCurrentDir[0] = '\0';
  m_commandLine = nullptr;
  m_sResListDir = "reslists";
}

void CLogAllFiles::Init(ICommandLine *command_line,
                        char (&base_directory)[MAX_PATH]) {
  m_commandLine = command_line;

  // Can't do this in edit mode
  if (m_commandLine->CheckParm("-edit")) {
    return;
  }

  if (!m_commandLine->CheckParm("-makereslists")) {
    return;
  }

  Q_strcpy(m_szBaseDir, base_directory);

  m_bActive = true;

  char const *pszDir = NULL;
  if (m_commandLine->CheckParm("-reslistdir", &pszDir) && pszDir) {
    char szDir[MAX_PATH];
    Q_strncpy(szDir, pszDir, sizeof(szDir));
    Q_StripTrailingSlash(szDir);
#ifdef WIN32
    Q_strlower(szDir);
#endif
    Q_FixSlashes(szDir);
    if (!Q_isempty(szDir)) {
      m_sResListDir = szDir;
    }
  }

  // game directory has not been established yet, must derive ourselves
  char path[MAX_PATH];
  Q_snprintf(path, sizeof(path), "%s/%s", m_szBaseDir,
             m_commandLine->ParmValue("-game", "hl2"));
  Q_FixSlashes(path);
#ifdef WIN32
  Q_strlower(path);
#endif
  m_sFullGamePath = path;

  // create file to dump out to
  char directory[MAX_PATH];
  V_snprintf(directory, sizeof(directory), "%s\\%s", m_sFullGamePath.String(),
             m_sResListDir.String());
  g_pFullFileSystem->CreateDirHierarchy(directory, "GAME");

  g_pFullFileSystem->AddLoggingFunc(&LogAllFilesFunc);

  if (!m_commandLine->FindParm("-startmap") &&
      !m_commandLine->FindParm("-startstage")) {
    m_Logged.RemoveAll();
    g_pFullFileSystem->RemoveFile(
        CFmtStr("%s\\%s\\%s", m_sFullGamePath.String(), m_sResListDir.String(),
                ALL_RESLIST_FILE),
        "GAME");
  }

#ifdef WIN32
  ::GetCurrentDirectory(sizeof(m_szCurrentDir), m_szCurrentDir);
  Q_strncat(m_szCurrentDir, "\\", sizeof(m_szCurrentDir), 1);
  _strlwr(m_szCurrentDir);
#else
  getcwd(m_szCurrentDir, sizeof(m_szCurrentDir));
  Q_strncat(m_szCurrentDir, "/", sizeof(m_szCurrentDir), 1);
#endif
}

void CLogAllFiles::Shutdown() {
  if (!m_bActive) return;

  m_bActive = false;

  if (m_commandLine->CheckParm("-makereslists")) {
    g_pFullFileSystem->RemoveLoggingFunc(&LogAllFilesFunc);
  }

  // Now load and sort all.lst
  SortResList(CFmtStr("%s\\%s\\%s", m_sFullGamePath.String(),
                      m_sResListDir.String(), ALL_RESLIST_FILE),
              "GAME");
  // Now load and sort engine.lst
  SortResList(CFmtStr("%s\\%s\\%s", m_sFullGamePath.String(),
                      m_sResListDir.String(), ENGINE_RESLIST_FILE),
              "GAME");

  m_Logged.Purge();
}

void CLogAllFiles::LogToAllReslist(char const *line) {
  // Open for append, write data, close.
  FileHandle_t fh =
      g_pFullFileSystem->Open(CFmtStr("%s\\%s\\%s", m_sFullGamePath.String(),
                                      m_sResListDir.String(), ALL_RESLIST_FILE),
                              "at", "GAME");
  if (fh != FILESYSTEM_INVALID_HANDLE) {
    g_pFullFileSystem->Write("\"", 1, fh);
    g_pFullFileSystem->Write(line, Q_strlen(line), fh);
    g_pFullFileSystem->Write("\"\n", 2, fh);
    g_pFullFileSystem->Close(fh);
  }
}

void CLogAllFiles::LogFile(const char *full_path_file_name,
                           const char *options) {
  if (!m_bActive) {
    Assert(0);
    return;
  }

  // write out to log file
  Assert(full_path_file_name[1] == ':');

  int idx = m_Logged.Find(full_path_file_name);
  if (idx != m_Logged.InvalidIndex()) return;

  m_Logged.Insert(full_path_file_name);

  // make it relative to our root directory
  const char *relative = Q_stristr(full_path_file_name, m_szBaseDir);
  if (relative) {
    relative += Q_strlen(m_szBaseDir) + 1;

    char rel[MAX_PATH];
    Q_strncpy(rel, relative, sizeof(rel));
#ifdef WIN32
    Q_strlower(rel);
#endif
    Q_FixSlashes(rel);

    LogToAllReslist(rel);
  }
}

// Purpose: callback function from filesystem
void CLogAllFiles::LogAllFilesFunc(const char *full_path_file_name,
                                   const char *options) {
  g_LogFiles->LogFile(full_path_file_name, options);
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

//-----------------------------------------------------------------------------
// Inner loop: initialize, shutdown main systems, load steam to
//-----------------------------------------------------------------------------
class CSourceAppSystemGroup : public CSteamAppSystemGroup {
 public:
  CSourceAppSystemGroup(ICommandLine *command_line,
                        char (&base_directory)[MAX_PATH], bool is_text_mode)
      : m_commandLine(command_line),
        m_pEngineApi(nullptr),
        m_pHammer(nullptr),
        m_pReslistgenerator(CreateReslistGenerator()),
        m_bEditMode(false),
        m_bTextMode(false) {
    Q_strcpy(m_szBaseDir, base_directory);
  }
  ~CSourceAppSystemGroup() { DestroyReslistGenerator(m_pReslistgenerator); }

  // Methods of IApplication
  virtual bool Create();
  virtual bool PreInit();
  virtual int Main();
  virtual void PostShutdown();
  virtual void Destroy();

  bool ShouldContinueGenerateReslist();

 private:
  const char *DetermineDefaultMod();
  const char *DetermineDefaultGame();

  char m_szBaseDir[_MAX_PATH];
  ICommandLine *m_commandLine;
  IEngineAPI *m_pEngineApi;
  IHammer *m_pHammer;
  IResListGenerator *m_pReslistgenerator;
  bool m_bEditMode;
  bool m_bTextMode;
};

// The dirty disk error report function
void ReportDirtyDiskNoMaterialSystem() {}

// Instantiate all main libraries
bool CSourceAppSystemGroup::Create() {
  double start_time{Plat_FloatTime()};

  IFileSystem *file_system =
      (IFileSystem *)FindSystem(FILESYSTEM_INTERFACE_VERSION);
  file_system->InstallDirtyDiskReportFunc(ReportDirtyDiskNoMaterialSystem);

  // Are we running in edit mode?
  m_bEditMode = m_commandLine->CheckParm("-edit");

  AppSystemInfo_t app_systems[] = {
      {"engine" DLL_EXT_STRING,
       CVAR_QUERY_INTERFACE_VERSION},  // NOTE: This one must be first!!
      {"inputsystem" DLL_EXT_STRING, INPUTSYSTEM_INTERFACE_VERSION},
      {"materialsystem" DLL_EXT_STRING, MATERIAL_SYSTEM_INTERFACE_VERSION},
      {"datacache" DLL_EXT_STRING, DATACACHE_INTERFACE_VERSION},
      {"datacache" DLL_EXT_STRING, MDLCACHE_INTERFACE_VERSION},
      {"datacache" DLL_EXT_STRING, STUDIO_DATA_CACHE_INTERFACE_VERSION},
      {"studiorender" DLL_EXT_STRING, STUDIO_RENDER_INTERFACE_VERSION},
      {"vphysics" DLL_EXT_STRING, VPHYSICS_INTERFACE_VERSION},
      {"video_services" DLL_EXT_STRING, VIDEO_SERVICES_INTERFACE_VERSION},

      // NOTE: This has to occur before vgui2.dll so it replaces vgui2's surface
      // implementation
      {"vguimatsurface" DLL_EXT_STRING, VGUI_SURFACE_INTERFACE_VERSION},
      {"vgui2" DLL_EXT_STRING, VGUI_IVGUI_INTERFACE_VERSION},
      {"engine" DLL_EXT_STRING, VENGINE_LAUNCHER_API_VERSION},

      {"", ""}  // Required to terminate the list
  };

#if defined(USE_SDL)
  AddSystem((IAppSystem *)CreateSDLMgr(), SDLMGR_INTERFACE_VERSION);
#endif

  if (!AddSystems(app_systems)) {
    Error(
        "Please check game installed correctly.\n\n"
        "Unable to add launcher systems from *" DLL_EXT_STRING
        "s. Looks like required components are missed or broken.");
  }

  // This will be NULL for games that don't support VR. That's ok. Just don't
  // load the DLL
  const AppModule_t source_vr_module{LoadModule("sourcevr" DLL_EXT_STRING)};
  if (source_vr_module != APP_MODULE_INVALID) {
    AddSystem(source_vr_module, SOURCE_VIRTUAL_REALITY_INTERFACE_VERSION);
  }

  // pull in our filesystem dll to pull the queued loader from it, we need to do
  // it this way due to the steam/stdio split for our steam filesystem
  char file_system_path[MAX_PATH];
  bool is_steam;
  if (FileSystem_GetFileSystemDLLName(file_system_path, is_steam) != FS_OK)
    return false;

  const AppModule_t file_system_module{LoadModule(file_system_path)};
  AddSystem(file_system_module, QUEUEDLOADER_INTERFACE_VERSION);

  // Hook in datamodel and p4 control if we're running with -tools
  if (IsPC() && ((m_commandLine->FindParm("-tools") &&
                  !m_commandLine->FindParm("-nop4")) ||
                 m_commandLine->FindParm("-p4"))) {
#ifdef STAGING_ONLY
    AppModule_t p4libModule = LoadModule("p4lib" DLL_EXT_STRING);
    IP4 *p4 = (IP4 *)AddSystem(p4libModule, P4_INTERFACE_VERSION);

    // If we are running with -steam then that means the tools are being used by
    // an SDK user. Don't exit in this case!
    if (!p4 && !m_commandLine->FindParm("-steam")) {
      return false;
    }
#endif  // STAGING_ONLY

    const AppModule_t vstdlib_module{LoadModule("vstdlib" DLL_EXT_STRING)};
    const IProcessUtils *process_utils = (IProcessUtils *)AddSystem(
        vstdlib_module, PROCESS_UTILS_INTERFACE_VERSION);
    if (!process_utils) return false;
  }

  // Connect to iterfaces loaded in AddSystems that we need locally
  IMaterialSystem *material_system =
      (IMaterialSystem *)FindSystem(MATERIAL_SYSTEM_INTERFACE_VERSION);
  if (!material_system) return false;

  m_pEngineApi = (IEngineAPI *)FindSystem(VENGINE_LAUNCHER_API_VERSION);

  // Load the hammer DLL if we're in editor mode
#if defined(_WIN32) && defined(STAGING_ONLY)
  if (m_bEditMode) {
    AppModule_t hammerModule = LoadModule("hammer_dll" DLL_EXT_STRING);
    g_pHammer = (IHammer *)AddSystem(hammerModule, INTERFACEVERSION_HAMMER);
    if (!g_pHammer) {
      return false;
    }
  }
#endif  // defined( _WIN32 ) && defined( STAGING_ONLY )

  {
    // Load up the appropriate shader DLL
    // This has to be done before connection.
    const char *shader_api{"shaderapidx9" DLL_EXT_STRING};
    if (m_commandLine->FindParm("-noshaderapi")) {
      shader_api = "shaderapiempty" DLL_EXT_STRING;
    }

    // dimhotepus: Allow to override shader API DLL.
    const char *override_shader_api{nullptr};
    if (m_commandLine->CheckParm("-shaderapi", &override_shader_api)) {
      shader_api = override_shader_api;
    }

    material_system->SetShaderAPI(shader_api);
    // dimhotepus: Detect missed shader API.
    if (!material_system->HasShaderAPI()) {
      Error(
          "Please check game installed correctly.\n\n"
          "Unable to set shader API %s. Looks like required components are "
          "missed or broken.",
          shader_api);
    }
  }

  double elapsed = Plat_FloatTime() - start_time;
  COM_TimestampedLog(
      "CSourceAppSystemGroup::Create:  Took %.4f secs to load libraries and "
      "get factories.",
      (float)elapsed);

  return true;
}

bool CSourceAppSystemGroup::PreInit() {
  CreateInterfaceFn factory = GetFactory();
  ConnectTier1Libraries(&factory, 1);
  ConVar_Register();
  ConnectTier2Libraries(&factory, 1);
  ConnectTier3Libraries(&factory, 1);

  if (!g_pFullFileSystem || !g_pMaterialSystem) return false;

  CFSSteamSetupInfo steam_setup;
  steam_setup.m_bToolsMode = false;
  steam_setup.m_bSetSteamDLLPath = false;
  steam_setup.m_bSteam = g_pFullFileSystem->IsSteam();
  steam_setup.m_bOnlyUseDirectoryName = true;
  steam_setup.m_pDirectoryName = DetermineDefaultMod();
  if (!steam_setup.m_pDirectoryName) {
    steam_setup.m_pDirectoryName = DetermineDefaultGame();
    if (!steam_setup.m_pDirectoryName) {
      Error(
          "FileSystem_LoadFileSystemModule: no -defaultgamedir or -game "
          "specified.");
    }
  }
  if (FileSystem_SetupSteamEnvironment(steam_setup) != FS_OK) return false;

  CFSMountContentInfo fs_mount;
  fs_mount.m_pFileSystem = g_pFullFileSystem;
  fs_mount.m_bToolsMode = m_bEditMode;
  fs_mount.m_pDirectoryName = steam_setup.m_GameInfoPath;
  if (FileSystem_MountContent(fs_mount) != FS_OK) return false;

  if (IsPC()) {
    fs_mount.m_pFileSystem->AddSearchPath("platform", "PLATFORM");

    // This will get called multiple times due to being here, but only the first
    // one will do anything
    m_pReslistgenerator->Init(m_szBaseDir,
                              m_commandLine->ParmValue("-game", "hl2"));

    // This will also get called each time, but will actually fix up the command
    // line as needed
    m_pReslistgenerator->SetupCommandLine();
  }

  Assert(!g_LogFiles);

  g_LogFiles = new CLogAllFiles();
  // FIXME: Logfiles is mod-specific, needs to move into the engine.
  g_LogFiles->Init(m_commandLine, m_szBaseDir);

  // Required to run through the editor
  if (m_bEditMode) g_pMaterialSystem->EnableEditorMaterials();

  StartupInfo_t startup_info;
  startup_info.m_pInstance = GetAppInstance();
  startup_info.m_pBaseDirectory = m_szBaseDir;
  startup_info.m_pInitialMod = DetermineDefaultMod();
  startup_info.m_pInitialGame = DetermineDefaultGame();
  startup_info.m_pParentAppSystemGroup = this;
  startup_info.m_bTextMode = m_bTextMode;

  m_pEngineApi->SetStartupInfo(startup_info);

  return true;
}

int CSourceAppSystemGroup::Main() { return m_pEngineApi->Run(); }

void CSourceAppSystemGroup::PostShutdown() {
  // FIXME: Logfiles is mod-specific, needs to move into the engine.
  g_LogFiles->Shutdown();
  // dimhotepus: Fix log files leak.
  delete g_LogFiles;
  g_LogFiles = nullptr;

  if (IsPC()) {
    m_pReslistgenerator->Shutdown();
  }

  DisconnectTier3Libraries();
  DisconnectTier2Libraries();
  ConVar_Unregister();
  DisconnectTier1Libraries();
}

void CSourceAppSystemGroup::Destroy() {
  m_pEngineApi = nullptr;
  g_pMaterialSystem = nullptr;
  m_pHammer = nullptr;
}

bool CSourceAppSystemGroup::ShouldContinueGenerateReslist() {
  return m_pReslistgenerator->ShouldContinue();
}

// Determines the initial mod to use at load time.  We eventually (hopefully)
// will be able to switch mods at runtime because the engine/hammer integration
// really wants this feature.
const char *CSourceAppSystemGroup::DetermineDefaultMod() {
  return !m_bEditMode ? m_commandLine->ParmValue("-game", DEFAULT_HL2_GAMEDIR)
                      : m_pHammer->GetDefaultMod();
}

const char *CSourceAppSystemGroup::DetermineDefaultGame() {
  return !m_bEditMode
             ? m_commandLine->ParmValue("-defaultgamedir", DEFAULT_HL2_GAMEDIR)
             : m_pHammer->GetDefaultGame();
}

// MessageBox for SDL/OSX
#if defined(USE_SDL) && !defined(_WIN32)

int MessageBox(HWND hWnd, const char *message, const char *header,
               unsigned uType) {
  SDL_ShowSimpleMessageBox(0, header, message, GetAssertDialogParent());
  return 0;
}

#endif

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

void RemoveOverrides(ICommandLine *command_line) {
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

  // Hook the debug output stuff.
  SpewOutputFunc(LauncherDefaultSpewFunc);

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
      ::MessageBox(nullptr,
                   "Oops, the game is already launched\n\nSorry, but only "
                   "single game can run at the same time.",
                   "Source - Warning", MB_ICONERROR | MB_OK);

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

  bool need_restart{true};
  while (need_restart) {
    CSourceAppSystemGroup systems{command_line, base_directory, is_text_mode};
    CSteamApplication app{&systems};

    const int rc{app.Run()};

    need_restart =
        (app.GetErrorStage() == CSourceAppSystemGroup::INITIALIZATION &&
         rc == INIT_RESTART) ||
        rc == RUN_RESTART;

    bool is_reslist_gen =
        !need_restart && systems.ShouldContinueGenerateReslist();

    if (!need_restart) need_restart = is_reslist_gen;

    if (!is_reslist_gen) RemoveOverrides(command_line);
  }

  return 0;
}
