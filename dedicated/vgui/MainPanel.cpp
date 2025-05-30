// Copyright Valve Corporation, All rights reserved.

#ifdef _WIN32
// base vgui interfaces
#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>
#include <vgui/IVGui.h>
#include <vgui/ISurface.h>
#include <vgui/Cursor.h>
#include <vgui_controls/ProgressBox.h>

#include "filesystem.h"
#include "IAdminServer.h"

#include "MainPanel.h"
#include "IManageServer.h"
#include "IVguiModule.h"
#include "vgui/IVGui.h"

#include "winlite.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

namespace {

CSysModule *g_hAdminServerModule = nullptr;

}  // namespace

namespace se::dedicated {

extern IAdminServer *g_pAdminServer;

CMainPanel *s_InternetDlg = nullptr;
char *gpszCvars = nullptr;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMainPanel::CMainPanel() : Panel(NULL, "CMainPanel") {
  SetPaintBackgroundEnabled(false);
  SetFgColor(Color(0, 0, 0, 0));
  m_bStarting = false;
  m_flPreviousSteamProgress = 0.0f;
  m_pGameServer = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CMainPanel::~CMainPanel() { free(se::dedicated::gpszCvars); }

//-----------------------------------------------------------------------------
// Purpose: Called once to set up
//-----------------------------------------------------------------------------
void CMainPanel::Initialize() {
  s_InternetDlg = this;
  m_pGameServer = NULL;

  m_bStarted = false;
  m_bIsInConfig = true;
  m_bClosing = false;
  m_pProgressBox = NULL;
  m_hShutdown = NULL;

  MoveToFront();

  m_pConfigPage = new CCreateMultiplayerGameServerPage(this, "Config");
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMainPanel::Open(void) {
  m_pConfigPage->SetVisible(true);
  m_pConfigPage->MoveToFront();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMainPanel::OnClose() { DoStop(); }

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to a static instance of this dialog
//-----------------------------------------------------------------------------
CMainPanel *CMainPanel::GetInstance() { return s_InternetDlg; }

//-----------------------------------------------------------------------------
// Purpose: Changes to the console page and starts up the actual server
//-----------------------------------------------------------------------------
void CMainPanel::StartServer(const char *cvars) {
  surface()->SetCursor(dc_hourglass);
  m_pConfigPage->GetServer(s1);

  // hide the config page and close it
  m_pConfigPage->SetVisible(false);
  m_pConfigPage->Close();

  se::dedicated::gpszCvars = strdup(cvars);

  // show the basic progress box immediately
  m_pProgressBox = new ProgressBox("#Start_Server_Loading_Title",
                                   "#Server_UpdatingSteamResources",
                                   "Starting dedicated server...");
  m_pProgressBox->SetCancelButtonVisible(true);
  m_pProgressBox->ShowWindow();

  // make sure we have all the steam content for this mod
  char reslist[_MAX_PATH];
  V_sprintf_safe(reslist, "reslists/%s/preload.lst",
                 m_pConfigPage->GetGameName());
  m_hResourceWaitHandle = g_pFullFileSystem->WaitForResources(reslist);
  if (!m_hResourceWaitHandle) {
    Assert(0);
  }

  m_pProgressBox->SetCancelButtonEnabled(false);

  m_hShutdown = CreateEvent(NULL, TRUE, FALSE, NULL);
  ivgui()->AddTickSignal(GetVPanel());
}

//-----------------------------------------------------------------------------
// Purpose: lets us delay the loading of the management screen until the server
// has started
//-----------------------------------------------------------------------------
void CMainPanel::OnTick() {
  if (m_hResourceWaitHandle) {
    // see if we've been cancelled
    if (!m_pProgressBox.Get() || !m_pProgressBox->IsVisible()) {
      // cancel out
      g_pFullFileSystem->CancelWaitForResources(m_hResourceWaitHandle);
      m_hResourceWaitHandle = NULL;
      DoStop();
      return;
    }

    // update resource waiting
    bool complete;
    float progress;
    if (g_pFullFileSystem->GetWaitForResourcesProgress(m_hResourceWaitHandle,
                                                       &progress, &complete)) {
      vgui::ivgui()->DPrintf2("progress %.2f %s\n", progress,
                              complete ? "not complete" : "complete");

      // don't set the progress if we've jumped straight from 0 to 100% complete
      if (!(progress == 1.0f && m_flPreviousSteamProgress == 0.0f)) {
        m_pProgressBox->SetProgress(progress);
        m_flPreviousSteamProgress = progress;
      }
    }

    // This is here because without it, the dedicated server will consume a lot
    // of CPU and it will slow Steam down so much that it'll download at 64k
    // instead of 6M.
    ThreadSleep(200);

    // see if we're done
    if (complete) {
      m_hResourceWaitHandle = NULL;
      m_bStarting = true;
      m_bIsInConfig = false;
      // at this stage in the process the user can no longer cancel
      m_pProgressBox->SetCancelButtonEnabled(false);
    }
  }

  if (m_bStarting)  // if we are actively launching the app
  {
    static int count = 0;
    if (WAIT_OBJECT_0 == WaitForSingleObject(m_hShutdown, 10) || count > 5000) {
      if (!m_bStarted) {
        serveritem_t server;
        m_pConfigPage->GetServer(server);

        ManageServerUIHandle_t managePage =
            g_pAdminServer->OpenManageServerDialog(server.name, server.gameDir);

        m_pGameServer = g_pAdminServer->GetManageServerInterface(managePage);
        m_bStarted = true;

        if (m_pProgressBox) {
          m_pProgressBox->Close();
          m_pProgressBox = NULL;
        }
      } else  // must be stopping the server
      {
        DoStop();
      }

      surface()->SetCursor(dc_user);
      m_bStarting = false;
      ResetEvent(m_hShutdown);
    } else {
      count++;
    }
  }
}

//-----------------------------------------------------------------------------
// Purpose: stops VGUI and kills any progress dialog we may have been displaying
//-----------------------------------------------------------------------------
void CMainPanel::DoStop() {
  surface()->SetCursor(dc_user);

  m_bStarted = false;
  m_bClosing = true;

  if (m_pProgressBox) {
    m_pProgressBox->Close();
    m_pProgressBox = NULL;
  }

  ivgui()->Stop();
}

//-----------------------------------------------------------------------------
// Purpose: Pushes text into the console
//-----------------------------------------------------------------------------
void CMainPanel::AddConsoleText(const char *msg) {
  if (m_pGameServer) m_pGameServer->AddToConsole(msg);
}

//-----------------------------------------------------------------------------
// Purpose: Message map
//-----------------------------------------------------------------------------
MessageMapItem_t CMainPanel::m_MessageMap[] = {
    MAP_MESSAGE(CMainPanel, "Quit", OnClose),
};

IMPLEMENT_PANELMAP(CMainPanel, BaseClass);

}  // namespace se::dedicated

#endif  // _WIN32
