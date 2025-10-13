// Copyright Valve Corporation, All rights reserved.
//
// Base property page.

#ifndef SE_COMMON_WINDOWS_BASE_PROPERTY_PAGE_H_
#define SE_COMMON_WINDOWS_BASE_PROPERTY_PAGE_H_

#include "windows/dpi_wnd_behavior.h"

class CBasePropertyPage : public CPropertyPage {
  DECLARE_DYNAMIC(CBasePropertyPage)

 public:
  CBasePropertyPage();
  explicit CBasePropertyPage(UINT nIDTemplate, UINT nIDCaption = 0,
                             DWORD dwSize = sizeof(PROPSHEETPAGE));
  explicit CBasePropertyPage(LPCTSTR lpszTemplateName, UINT nIDCaption = 0,
                             DWORD dwSize = sizeof(PROPSHEETPAGE));

  // extended construction
  CBasePropertyPage(UINT nIDTemplate, UINT nIDCaption, UINT nIDHeaderTitle,
                    UINT nIDHeaderSubTitle = 0,
                    DWORD dwSize = sizeof(PROPSHEETPAGE));
  CBasePropertyPage(LPCTSTR lpszTemplateName, UINT nIDCaption,
                    UINT nIDHeaderTitle, UINT nIDHeaderSubTitle = 0,
                    DWORD dwSize = sizeof(PROPSHEETPAGE));

 protected:
  BOOL OnInitDialog() override;

  afx_msg void OnDestroy();
  afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
  DECLARE_MESSAGE_MAP()

 private:
  se::windows::ui::CDpiWindowBehavior m_dpi_behavior;
};

#endif  // !SE_COMMON_WINDOWS_BASE_PROPERTY_PAGE_H_
