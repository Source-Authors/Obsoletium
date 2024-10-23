// Copyright Valve Corporation, All rights reserved.

#ifdef _WIN32
#include "winlite.h"
#include <direct.h>

// includes for the VGUI version
#include <vgui_controls/Panel.h>
#include <vgui_controls/Controls.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui/IPanel.h>
#include "filesystem.h"
#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include "IVguiModule.h"

#include "vgui/MainPanel.h"
#include "IAdminServer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace {

CSysModule *g_hAdminServerModule = nullptr;
IVGuiModule *g_pAdminVGuiModule = nullptr;

}  // namespace

namespace se::dedicated {

IAdminServer *g_pAdminServer = nullptr;
CMainPanel *g_pMainPanel = nullptr;  // the main panel to show

//-----------------------------------------------------------------------------
// Purpose: Starts up the VGUI system and loads the base panel
//-----------------------------------------------------------------------------
int StartVGUI(CreateInterfaceFn dedicatedFactory) {
  // the "base dir" so we can scan mod name
  g_pFullFileSystem->AddSearchPath(".", "MAIN");
  // the main platform dir
  g_pFullFileSystem->AddSearchPath("platform", "PLATFORM", PATH_ADD_TO_HEAD);

  vgui::ivgui()->SetSleep(false);

  // find our configuration directory
  char szConfigDir[512];
  const char *steamPath = getenv("SteamInstallPath");
  if (steamPath) {
    // put the config dir directly under steam
    V_sprintf_safe(szConfigDir, "%s/config", steamPath);
  } else {
    // we're not running steam, so just put the config dir under the platform
    V_strcpy_safe(szConfigDir, "platform/config");
  }
  g_pFullFileSystem->CreateDirHierarchy("config", "PLATFORM");
  g_pFullFileSystem->AddSearchPath(szConfigDir, "CONFIG", PATH_ADD_TO_HEAD);

  // initialize the user configuration file
  vgui::system()->SetUserConfigFile("DedicatedServerDialogConfig.vdf",
                                    "CONFIG");

  // Init the surface
  g_pMainPanel = new CMainPanel();
  g_pMainPanel->SetVisible(true);

  vgui::surface()->SetEmbeddedPanel(g_pMainPanel->GetVPanel());

  // load the scheme
  vgui::scheme()->LoadSchemeFromFile("Resource/SourceScheme.res", NULL);

  // localization
  g_pVGuiLocalize->AddFile("Resource/platform_%language%.txt");
  g_pVGuiLocalize->AddFile("Resource/vgui_%language%.txt");
  g_pVGuiLocalize->AddFile("Admin/server_%language%.txt");

  // Start vgui
  vgui::ivgui()->Start();

  // load the module
#ifdef PLATFORM_64BITS
  g_pFullFileSystem->GetLocalCopy("bin/x64/adminserver" DLL_EXT_STRING);
#else
  g_pFullFileSystem->GetLocalCopy("bin/adminserver" DLL_EXT_STRING);
#endif

  g_hAdminServerModule = g_pFullFileSystem->LoadModule("adminserver");
  Assert(g_hAdminServerModule);

  if (!g_hAdminServerModule) {
    vgui::ivgui()->DPrintf2(
        "Admin Error: module version (adminserver" DLL_EXT_STRING ", %s) invalid, not "
        "loading\n",
        IMANAGESERVER_INTERFACE_VERSION);
  } else {
    // make sure we get the right version
    CreateInterfaceFn adminFactory = Sys_GetFactory(g_hAdminServerModule);

    g_pAdminServer =
        (IAdminServer *)adminFactory(ADMINSERVER_INTERFACE_VERSION, NULL);
    Assert(g_pAdminServer);
    g_pAdminVGuiModule =
        (IVGuiModule *)adminFactory("VGuiModuleAdminServer001", NULL);
    Assert(g_pAdminVGuiModule);

    if (!g_pAdminServer || !g_pAdminVGuiModule) {
      vgui::ivgui()->DPrintf2(
          "Admin Error: module version (adminserver" DLL_EXT_STRING ", %s) invalid, not "
          "loading\n",
          IMANAGESERVER_INTERFACE_VERSION);
    }

    // finish initializing admin module
    g_pAdminVGuiModule->Initialize(&dedicatedFactory, 1);
    g_pAdminVGuiModule->PostInitialize(&adminFactory, 1);
    g_pAdminVGuiModule->SetParent(g_pMainPanel->GetVPanel());
  }

  // finish setting up main panel
  g_pMainPanel->Initialize();
  g_pMainPanel->Open();

  return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Shuts down the VGUI system
//-----------------------------------------------------------------------------
void StopVGUI() {
  SetEvent(g_pMainPanel->GetShutdownHandle());

  delete g_pMainPanel;
  g_pMainPanel = NULL;

  if (g_hAdminServerModule) {
    g_pAdminVGuiModule->Shutdown();
    Sys_UnloadModule(g_hAdminServerModule);
  }
}

//-----------------------------------------------------------------------------
// Purpose: Run a single VGUI frame
//-----------------------------------------------------------------------------
void RunVGUIFrame() { vgui::ivgui()->RunFrame(); }

bool VGUIIsStopping() { return g_pMainPanel->Stopping(); }

bool VGUIIsRunning() { return vgui::ivgui()->IsRunning(); }

bool VGUIIsInConfig() { return g_pMainPanel->IsInConfig(); }

void VGUIFinishedConfig() {
  Assert(g_pMainPanel);
  if (g_pMainPanel)  // engine is loaded, pass the message on
  {
    SetEvent(g_pMainPanel->GetShutdownHandle());
  }
}

void VGUIPrintf(const char *msg) {
  if (!g_pMainPanel || VGUIIsInConfig() || VGUIIsStopping()) {
    Plat_DebugString(msg);
  } else if (g_pMainPanel) {
    g_pMainPanel->AddConsoleText(msg);
  }
}

}  // namespace se::dedicated

#endif  // _WIN32
