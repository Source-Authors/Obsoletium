// Copyright Valve Corporation, All rights reserved.
//
// Base MDI child window.

#ifndef SE_COMMON_WINDOWS_BASE_MDI_CHILD_WND_
#define SE_COMMON_WINDOWS_BASE_MDI_CHILD_WND_

#include "windows/dpi_wnd_behavior.h"

class CBaseMDIChildWnd : public CMDIChildWnd {
  DECLARE_DYNCREATE(CBaseMDIChildWnd)

 public:
  CBaseMDIChildWnd();

 protected:
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnDestroy();
  afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
  DECLARE_MESSAGE_MAP()

 protected:
  se::windows::ui::CDpiWindowBehavior m_dpi_behavior;
};

#endif  // !SE_COMMON_WINDOWS_BASE_MDI_CHILD_WND_
