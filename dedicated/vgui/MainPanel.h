// Copyright Valve Corporation, All rights reserved.

#ifndef SE_DEDICATED_VGUI_MAIN_PANEL_H_
#define SE_DEDICATED_VGUI_MAIN_PANEL_H_

#include "tier1/utlvector.h"

#include "vgui_controls/Frame.h"
#include "vgui_controls/Panel.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/PHandle.h"

#include "IManageServer.h"
#include "CreateMultiplayerGameServerPage.h"

class IAdminServer;

namespace se::dedicated {

// Root panel for dedicated server GUI
class CMainPanel : public vgui::Panel {
  using BaseClass = vgui::Panel;

 public:
  // Construction/destruction
  CMainPanel();
  virtual ~CMainPanel();

  virtual void Initialize();

  // displays the dialog, moves it into focus, updates if it has to
  virtual void Open();

  // returns a pointer to a static instance of this dialog
  // valid for use only in sort functions
  static CMainPanel *GetInstance();
  virtual void StartServer(const char *cvars);

  void ActivateBuildMode();

  void *GetShutdownHandle() { return m_hShutdown; }

  void AddConsoleText(const char *msg);

  bool Stopping() const { return m_bClosing; }

  bool IsInConfig() const { return m_bIsInConfig; }

 private:
  // called when dialog is shut down
  virtual void OnClose();
  virtual void OnTick();
  void DoStop();

  // GUI elements
  IManageServer *m_pGameServer;

  // the popup menu
  vgui::DHANDLE<vgui::ProgressBox> m_pProgressBox;
  CCreateMultiplayerGameServerPage *m_pConfigPage;

  // Event that lets the thread tell the main window it shutdown
  void *m_hShutdown;

  bool m_bStarting;  // whether the server is currently starting
  bool m_bStarted;   // whether the server has been started or not
  bool m_bClosing;   // whether we are shutting down
  bool m_bIsInConfig;
  serveritem_t s1;
  int m_hResourceWaitHandle;
  float m_flPreviousSteamProgress;

  DECLARE_PANELMAP();
};

}  // namespace se::dedicated

#endif  // !SE_DEDICATED_VGUI_MAIN_PANEL_H_
