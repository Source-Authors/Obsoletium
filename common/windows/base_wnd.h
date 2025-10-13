// Copyright Valve Corporation, All rights reserved.
//
// Base window.

#ifndef SE_COMMON_WINDOWS_BASE_WND_
#define SE_COMMON_WINDOWS_BASE_WND_

#include "windows/dpi_wnd_behavior.h"

class CBaseWnd : public CWnd {
  DECLARE_DYNCREATE(CBaseWnd)

 public:
  CBaseWnd();
  
  void CalcWindowRect(LPRECT lpClientRect, UINT nAdjustType = adjustBorder) override;

 protected:
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnDestroy();
  afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
  DECLARE_MESSAGE_MAP()

 protected:
  se::windows::ui::CDpiWindowBehavior m_dpi_behavior;
};

#endif  // !SE_COMMON_WINDOWS_BASE_WND_
