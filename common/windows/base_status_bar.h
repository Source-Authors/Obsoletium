// Copyright Valve Corporation, All rights reserved.
//
// Base status bar.

#ifndef SE_COMMON_WINDOWS_BASE_STATUS_BAR_
#define SE_COMMON_WINDOWS_BASE_STATUS_BAR_

#include "windows/dpi_wnd_behavior.h"

class CBaseStatusBar : public CStatusBar {
  DECLARE_DYNCREATE(CBaseStatusBar)

 public:
  CBaseStatusBar();
  BOOL SetIndicators(const UINT* lpIDArray, int nIDCount);

 protected:
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnDestroy();
  afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
  DECLARE_MESSAGE_MAP()

  [[nodiscard]] int GetHeight() const;
  void ScaleHeight(int initialHeight);

 private:
  se::windows::ui::CDpiWindowBehavior m_dpi_behavior;

  int m_indicator_count;
};

#endif  // !SE_COMMON_WINDOWS_BASE_STATUS_BAR_
