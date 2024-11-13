// Copyright Valve Corporation, All rights reserved.
//
// Base dialog.

#ifndef SE_COMMON_WINDOWS_BASE_DLG_H_
#define SE_COMMON_WINDOWS_BASE_DLG_H_

#include "windows/dpi_wnd_behavior.h"

class CBaseDlg : public CDialog {
  DECLARE_DYNAMIC(CBaseDlg)

 public:
  CBaseDlg();
  explicit CBaseDlg(LPCTSTR lpszTemplateName, CWnd* pParentWnd = nullptr);
  explicit CBaseDlg(UINT nIDTemplate, CWnd* pParentWnd = nullptr);
  virtual ~CBaseDlg();

 protected:
  BOOL OnInitDialog() override;

  afx_msg void OnDestroy();
  afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
  DECLARE_MESSAGE_MAP()

 private:
  se::windows::ui::CDpiWindowBehavior m_dpi_behavior;
};

#endif  // !SE_COMMON_WINDOWS_BASE_DLG_H_
