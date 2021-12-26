// Copyright Valve Corporation, All rights reserved.
//
// Purpose: A higher level link library for general use in the game and tools.

#include "interfaces/interfaces.h"

#include "tier0/platform.h"
#include "tier0/dbg.h"

#include <algorithm>

// Tier 1 libraries
ICvar *cvar{nullptr};
ICvar *g_pCVar{nullptr};
IProcessUtils *g_pProcessUtils{nullptr};
IPhysics2 *g_pPhysics2{nullptr};
IPhysics2ActorManager *g_pPhysics2ActorManager{nullptr};
IPhysics2ResourceManager *g_pPhysics2ResourceManager{nullptr};
IEventSystem *g_pEventSystem{nullptr};
ILocalize *g_pLocalize{nullptr};

// for utlsortvector.h
#ifndef _WIN32
void *g_pUtlSortVectorQSortContext{nullptr};
#endif

// Tier 2 libraries
IResourceSystem *g_pResourceSystem{nullptr};
IRenderDeviceMgr *g_pRenderDeviceMgr{nullptr};
IFileSystem *g_pFullFileSystem{nullptr};
IAsyncFileSystem *g_pAsyncFileSystem{nullptr};
IMaterialSystem *materials{nullptr};
IMaterialSystem *g_pMaterialSystem{nullptr};
IMaterialSystem2 *g_pMaterialSystem2{nullptr};
IInputSystem *g_pInputSystem{nullptr};
IInputStackSystem *g_pInputStackSystem{nullptr};
INetworkSystem *g_pNetworkSystem{nullptr};
ISoundSystem *g_pSoundSystem{nullptr};
IMaterialSystemHardwareConfig *g_pMaterialSystemHardwareConfig{nullptr};
IDebugTextureInfo *g_pMaterialSystemDebugTextureInfo{nullptr};
IVBAllocTracker *g_VBAllocTracker{nullptr};
IColorCorrectionSystem *colorcorrection{nullptr};
IP4 *p4{nullptr}; //-V707
IMdlLib *mdllib{nullptr};
IQueuedLoader *g_pQueuedLoader{nullptr};
IResourceAccessControl *g_pResourceAccessControl{nullptr};
IPrecacheSystem *g_pPrecacheSystem{nullptr};
ISceneSystem *g_pSceneSystem{nullptr};

#if defined(PLATFORM_X360)
IXboxInstaller *g_pXboxInstaller{nullptr};
#endif

IMatchFramework *g_pMatchFramework{nullptr};
IGameUISystemMgr *g_pGameUISystemMgr{nullptr};

// Not exactly a global, but we're going to keep track of these here anyways
IRenderDevice *g_pRenderDevice{nullptr};
IRenderHardwareConfig *g_pRenderHardwareConfig{nullptr};

// Tier3 libraries
IMeshSystem *g_pMeshSystem{nullptr};
IStudioRender *g_pStudioRender{nullptr};
IStudioRender *studiorender{nullptr};
IMatSystemSurface *g_pMatSystemSurface{nullptr};
vgui::IInput *g_pVGuiInput{nullptr};
vgui::ISurface *g_pVGuiSurface{nullptr};
vgui::IPanel *g_pVGuiPanel{nullptr};
vgui::IVGui *g_pVGui{nullptr};
vgui::IVGUILocalize *g_pVGuiLocalize{nullptr};
vgui::ISchemeManager *g_pVGuiSchemeManager{nullptr};
vgui::ISystem *g_pVGuiSystem{nullptr};
IDataCache *g_pDataCache{nullptr};
IMDLCache *g_pMDLCache{nullptr};
IMDLCache *mdlcache{nullptr};
IAvi *g_pAVI{nullptr};
IBik *g_pBIK{nullptr};
IDmeMakefileUtils *g_pDmeMakefileUtils{nullptr};
IPhysicsCollision *g_pPhysicsCollision{nullptr};
ISoundEmitterSystemBase *g_pSoundEmitterSystem{nullptr};
IWorldRendererMgr *g_pWorldRendererMgr{nullptr};
IVGuiRenderSurface *g_pVGuiRenderSurface{nullptr};

// Mapping of interface string to globals
struct InterfaceGlobal {
  const char *interface_name;
  void *global;
};

// At each level of connection, we're going to keep track of which interfaces
// we filled in. When we disconnect, we'll clear those interface pointers out.
struct ConnectionRegistration {
  void *global_storage;
  size_t connection_phase;
};

namespace {

InterfaceGlobal interface_globals[] {
  {CVAR_INTERFACE_VERSION, &cvar}, {CVAR_INTERFACE_VERSION, &g_pCVar},
      {EVENTSYSTEM_INTERFACE_VERSION, &g_pEventSystem},
      {PROCESS_UTILS_INTERFACE_VERSION, &g_pProcessUtils},
      {VPHYSICS2_INTERFACE_VERSION, &g_pPhysics2},
      {VPHYSICS2_ACTOR_MGR_INTERFACE_VERSION, &g_pPhysics2ActorManager},
      {VPHYSICS2_RESOURCE_MGR_INTERFACE_VERSION, &g_pPhysics2ResourceManager},
      {FILESYSTEM_INTERFACE_VERSION, &g_pFullFileSystem},
      {ASYNCFILESYSTEM_INTERFACE_VERSION, &g_pAsyncFileSystem},
      {RESOURCESYSTEM_INTERFACE_VERSION, &g_pResourceSystem},
      {MATERIAL_SYSTEM_INTERFACE_VERSION, &g_pMaterialSystem},
      {MATERIAL_SYSTEM_INTERFACE_VERSION, &materials},
      {MATERIAL_SYSTEM2_INTERFACE_VERSION, &g_pMaterialSystem2},
      {INPUTSYSTEM_INTERFACE_VERSION, &g_pInputSystem},
      {INPUTSTACKSYSTEM_INTERFACE_VERSION, &g_pInputStackSystem},
      {NETWORKSYSTEM_INTERFACE_VERSION, &g_pNetworkSystem},
      {RENDER_DEVICE_MGR_INTERFACE_VERSION, &g_pRenderDeviceMgr},
      {MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION,
       &g_pMaterialSystemHardwareConfig},
      {SOUNDSYSTEM_INTERFACE_VERSION, &g_pSoundSystem},
      {DEBUG_TEXTURE_INFO_VERSION, &g_pMaterialSystemDebugTextureInfo},
      {VB_ALLOC_TRACKER_INTERFACE_VERSION, &g_VBAllocTracker},
      {COLORCORRECTION_INTERFACE_VERSION, &colorcorrection},
      {P4_INTERFACE_VERSION, &p4}, {MDLLIB_INTERFACE_VERSION, &mdllib},
      {QUEUEDLOADER_INTERFACE_VERSION, &g_pQueuedLoader},
      {RESOURCE_ACCESS_CONTROL_INTERFACE_VERSION, &g_pResourceAccessControl},
      {PRECACHE_SYSTEM_INTERFACE_VERSION, &g_pPrecacheSystem},
      {STUDIO_RENDER_INTERFACE_VERSION, &g_pStudioRender},
      {STUDIO_RENDER_INTERFACE_VERSION, &studiorender},
      {VGUI_IVGUI_INTERFACE_VERSION, &g_pVGui},
      {VGUI_INPUT_INTERFACE_VERSION, &g_pVGuiInput},
      {VGUI_PANEL_INTERFACE_VERSION, &g_pVGuiPanel},
      {VGUI_SURFACE_INTERFACE_VERSION, &g_pVGuiSurface},
      {VGUI_SCHEME_INTERFACE_VERSION, &g_pVGuiSchemeManager},
      {VGUI_SYSTEM_INTERFACE_VERSION, &g_pVGuiSystem},
      {LOCALIZE_INTERFACE_VERSION, &g_pLocalize},
      {LOCALIZE_INTERFACE_VERSION, &g_pVGuiLocalize},
      {MAT_SYSTEM_SURFACE_INTERFACE_VERSION, &g_pMatSystemSurface},
      {DATACACHE_INTERFACE_VERSION, &g_pDataCache},
      {MDLCACHE_INTERFACE_VERSION, &g_pMDLCache},
      {MDLCACHE_INTERFACE_VERSION, &mdlcache}, {AVI_INTERFACE_VERSION, &g_pAVI},
      {BIK_INTERFACE_VERSION, &g_pBIK},
      {DMEMAKEFILE_UTILS_INTERFACE_VERSION, &g_pDmeMakefileUtils},
      {VPHYSICS_COLLISION_INTERFACE_VERSION, &g_pPhysicsCollision},
      {SOUNDEMITTERSYSTEM_INTERFACE_VERSION, &g_pSoundEmitterSystem},
      {MESHSYSTEM_INTERFACE_VERSION, &g_pMeshSystem},
      {RENDER_DEVICE_INTERFACE_VERSION, &g_pRenderDevice},
      {RENDER_HARDWARECONFIG_INTERFACE_VERSION, &g_pRenderHardwareConfig},
      {SCENESYSTEM_INTERFACE_VERSION, &g_pSceneSystem},
      {WORLD_RENDERER_MGR_INTERFACE_VERSION, &g_pWorldRendererMgr},
      {RENDER_SYSTEM_SURFACE_INTERFACE_VERSION, &g_pVGuiRenderSurface},

#if defined(_X360)
      {XBOXINSTALLER_INTERFACE_VERSION, &g_pXboxInstaller},
#endif

      {MATCHFRAMEWORK_INTERFACE_VERSION, &g_pMatchFramework},
      {GAMEUISYSTEMMGR_INTERFACE_VERSION, &g_pGameUISystemMgr},
};

// The # of times this DLL has been connected
size_t connections_count{0};

size_t registrations_count{0};

ConnectionRegistration
    connection_registrations[std::size(interface_globals) + 1];

void RegisterInterface(CreateInterfaceFn factory, const char *name,
                       void **global) {
  if (!(*global)) {
    *global = factory(name, nullptr);

    if (*global) {
      Assert(registrations_count < std::size(connection_registrations));

      ConnectionRegistration &reg{
          connection_registrations[registrations_count++]};

      reg.global_storage = global;
      reg.connection_phase = connections_count;
    }
  }
}

void ReconnectInterface(CreateInterfaceFn factory, const char *name,
                        void **global) {
  *global = factory(name, nullptr);

  bool bFound = false;

  Assert(registrations_count < std::size(connection_registrations));

  for (size_t i = 0; i < registrations_count; ++i) {
    ConnectionRegistration &reg{connection_registrations[i]};

    if (reg.global_storage != global) continue;

    reg.global_storage = global;

    bFound = true;
  }

  if (!bFound && *global) {
    Assert(registrations_count < std::size(connection_registrations));

    ConnectionRegistration &reg{
        connection_registrations[registrations_count++]};

    reg.global_storage = global;
    reg.connection_phase = connections_count;
  }
}

}  // namespace

// Call this to connect to all tier 1 libraries.  It's up to the caller to check
// the globals it cares about to see if ones are missing
void ConnectInterfaces(CreateInterfaceFn *factories, int count) {
  // This is no longer questionable: ConnectInterfaces() is expected to be
  // called multiple times for a file that exports multiple interfaces.
  for (int i = 0; i < count; ++i) {
    for (const auto &global : interface_globals) {
      ReconnectInterface(factories[i], global.interface_name,
                         (void **)global.global);
    }
  }

  ++connections_count;
}

void DisconnectInterfaces() {
  Assert(connections_count > 0);

  if (--connections_count == std::numeric_limits<size_t>::max()) return;

  for (size_t i = 0; i < registrations_count; ++i) {
    if (connection_registrations[i].connection_phase != connections_count)
      continue;

    // Disconnect!
    *(void **)(connection_registrations[i].global_storage) = nullptr;
  }
}

// Reloads an interface
void ReconnectInterface(CreateInterfaceFn factory, const char *name) {
  for (const auto &global : interface_globals) {
    if (strcmp(global.interface_name, name) != 0) continue;

    ReconnectInterface(factory, global.interface_name, (void **)global.global);
  }
}
