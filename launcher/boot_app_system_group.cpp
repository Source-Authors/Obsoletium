// Copyright Valve Corporation, All rights reserved.

#include "boot_app_system_group.h"

#include "appframework/AppFramework.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "filesystem.h"
#include "filesystem_init.h"
#include "filesystem/IQueuedLoader.h"
#include "icvar.h"
#include "inputsystem/iinputsystem.h"
#include "istudiorender.h"
#include "materialsystem/imaterialsystem.h"
#include "video/ivideoservices.h"
#include "vphysics_interface.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui/VGUI.h"
#include "sourcevr/isourcevirtualreality.h"
#include "vstdlib/iprocessutils.h"

#include "tier1/tier1.h"
#include "tier2/tier2.h"
#include "tier3/tier3.h"

#include "file_logger.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace {

constexpr inline char kDefaultHalfLife2GameDirectory[]{"hl2"};

// The dirty disk error report function
void ReportDirtyDiskNoMaterialSystem() {}

}  // namespace

namespace se::launcher {

std::unique_ptr<FileLogger> BootAppSystemGroup::file_logger_ = nullptr;

// Instantiate all main libraries
bool BootAppSystemGroup::Create() {
  double start_time{Plat_FloatTime()};

  IFileSystem *file_system =
      FindSystem<IFileSystem>(FILESYSTEM_INTERFACE_VERSION);
  file_system->InstallDirtyDiskReportFunc(ReportDirtyDiskNoMaterialSystem);

  resource_listing_ = CreateResourceListing(command_line_, file_system);

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
  if (FileSystem_GetFileSystemDLLName(file_system_path, is_steam) != FS_OK) {
    return false;
  }

  const AppModule_t file_system_module{LoadModule(file_system_path)};
  AddSystem(file_system_module, QUEUEDLOADER_INTERFACE_VERSION);

  // Hook in datamodel and p4 control if we're running with -tools
  if ((command_line_->FindParm("-tools") &&
       !command_line_->FindParm("-nop4")) ||
      command_line_->FindParm("-p4")) {
#ifdef STAGING_ONLY
    AppModule_t p4libModule{LoadModule("p4lib" DLL_EXT_STRING)};
    IP4 *p4{AddSystem<IP4>(p4libModule, P4_INTERFACE_VERSION)};

    // If we are running with -steam then that means the tools are being used by
    // an SDK user. Don't exit in this case!
    if (!p4 && !command_line_->FindParm("-steam")) {
      return false;
    }
#endif  // STAGING_ONLY

    const AppModule_t vstdlib_module{LoadModule("vstdlib" DLL_EXT_STRING)};
    const IProcessUtils *process_utils{AddSystem<IProcessUtils>(
        vstdlib_module, PROCESS_UTILS_INTERFACE_VERSION)};
    if (!process_utils) return false;
  }

  // Connect to iterfaces loaded in AddSystems that we need locally
  IMaterialSystem *material_system{
      FindSystem<IMaterialSystem>(MATERIAL_SYSTEM_INTERFACE_VERSION)};
  if (!material_system) return false;

  engine_api_ = FindSystem<IEngineAPI>(VENGINE_LAUNCHER_API_VERSION);

  // Load the hammer DLL if we're in editor mode
#if defined(_WIN32) && defined(STAGING_ONLY)
  if (m_bEditMode) {
    AppModule_t hammerModule{LoadModule("hammer_dll" DLL_EXT_STRING)};
    g_pHammer = AddSystem<IHammer>(hammerModule, INTERFACEVERSION_HAMMER);
    if (!g_pHammer) return false;
  }
#endif

  {
    // Load up the appropriate shader DLL
    // This has to be done before connection.
    const char *shader_api{"shaderapidx9" DLL_EXT_STRING};
    if (command_line_->FindParm("-noshaderapi")) {
      shader_api = "shaderapiempty" DLL_EXT_STRING;
    }

    // dimhotepus: Allow to override shader API DLL.
    const char *override_shader_api{nullptr};
    if (command_line_->CheckParm("-shaderapi", &override_shader_api)) {
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

bool BootAppSystemGroup::PreInit() {
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
  fs_mount.m_bToolsMode = is_edit_mode_;
  fs_mount.m_pDirectoryName = steam_setup.m_GameInfoPath;
  if (FileSystem_MountContent(fs_mount) != FS_OK) return false;

  if (IsPC()) {
    fs_mount.m_pFileSystem->AddSearchPath("platform", "PLATFORM");

    // This will get called multiple times due to being here, but only the first
    // one will do anything
    resource_listing_->Init(base_dir_,
                            command_line_->ParmValue("-game", "hl2"));

    // This will also get called each time, but will actually fix up the command
    // line as needed
    resource_listing_->SetupCommandLine();
  }

  Assert(!file_logger_);

  // FIXME: Logfiles is mod-specific, needs to move into the engine.
  file_logger_ =
      std::make_unique<FileLogger>(command_line_, g_pFullFileSystem, base_dir_);

  g_pFullFileSystem->AddLoggingFunc(&LogAccessCallback);

  // Required to run through the editor
  if (is_edit_mode_) g_pMaterialSystem->EnableEditorMaterials();

  StartupInfo_t startup_info;
  startup_info.m_pInstance = GetAppInstance();
  startup_info.m_pBaseDirectory = base_dir_;
  startup_info.m_pInitialMod = DetermineDefaultMod();
  startup_info.m_pInitialGame = DetermineDefaultGame();
  startup_info.m_pParentAppSystemGroup = this;
  startup_info.m_bTextMode = is_text_mode_;

  engine_api_->SetStartupInfo(startup_info);

  return true;
}

int BootAppSystemGroup::Main() { return engine_api_->Run(); }

void BootAppSystemGroup::PostShutdown() {
  // FIXME: Logfiles is mod-specific, needs to move into the engine.
  g_pFullFileSystem->RemoveLoggingFunc(&LogAccessCallback);
  file_logger_->Shutdown();

  DisconnectTier3Libraries();
  DisconnectTier2Libraries();
  ConVar_Unregister();
  DisconnectTier1Libraries();
}

void BootAppSystemGroup::Destroy() {
  engine_api_ = nullptr;
  hammer_ = nullptr;
}

bool BootAppSystemGroup::ShouldContinueGenerateReslist() {
  return resource_listing_->ShouldContinue();
}

// Callback function from filesystem
void BootAppSystemGroup::LogAccessCallback(const char *file_path,
                                           const char *options) {
  file_logger_->LogAccess(file_path, options);
}

// Determines the initial mod to use at load time.  We eventually (hopefully)
// will be able to switch mods at runtime because the engine/hammer integration
// really wants this feature.
const char *BootAppSystemGroup::DetermineDefaultMod() {
  return !is_edit_mode_
             ? command_line_->ParmValue("-game", kDefaultHalfLife2GameDirectory)
             : hammer_->GetDefaultMod();
}

const char *BootAppSystemGroup::DetermineDefaultGame() {
  return !is_edit_mode_ ? command_line_->ParmValue(
                              "-defaultgamedir", kDefaultHalfLife2GameDirectory)
                        : hammer_->GetDefaultGame();
}

}  // namespace se::launcher
