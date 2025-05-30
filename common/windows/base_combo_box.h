// Copyright Valve Corporation, All rights reserved.
//
// Base combobox window.

#ifndef SE_COMMON_WINDOWS_BASE_COMBO_BOX_
#define SE_COMMON_WINDOWS_BASE_COMBO_BOX_

#include "windows/dpi_wnd_behavior.h"

class CBaseComboBox : public CComboBox {
  DECLARE_DYNAMIC(CBaseComboBox)

 public:
  CBaseComboBox();

  BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd,
              UINT nID) override;
  void SubclassDlgItem(UINT nID, CWnd* pParent);

 protected:
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnDestroy();
  afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
  DECLARE_MESSAGE_MAP()

 protected:
  se::windows::ui::CDpiWindowBehavior m_dpi_behavior;

  bool m_dpi_behavior_created;
};

#endif  // !SE_COMMON_WINDOWS_BASE_MDI_CHILD_WND_
