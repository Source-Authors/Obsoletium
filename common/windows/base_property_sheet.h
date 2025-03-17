// Copyright Valve Corporation, All rights reserved.
//
// Base property sheet.

#ifndef SE_COMMON_WINDOWS_BASE_PROPERTY_SHEET_H_
#define SE_COMMON_WINDOWS_BASE_PROPERTY_SHEET_H_

#include "windows/dpi_wnd_behavior.h"

class CBasePropertySheet : public CPropertySheet {
  DECLARE_DYNAMIC(CBasePropertySheet)

 public:
  CBasePropertySheet();
  explicit CBasePropertySheet(UINT nIDCaption, CWnd* pParentWnd = nullptr,
                              UINT iSelectPage = 0);
  explicit CBasePropertySheet(LPCTSTR pszCaption, CWnd* pParentWnd = nullptr,
                              UINT iSelectPage = 0);

  // extended construction
  CBasePropertySheet(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage,
                     HBITMAP hbmWatermark, HPALETTE hpalWatermark = nullptr,
                     HBITMAP hbmHeader = nullptr);
  CBasePropertySheet(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage,
                     HBITMAP hbmWatermark, HPALETTE hpalWatermark = nullptr,
                     HBITMAP hbmHeader = nullptr);

 protected:
  BOOL OnInitDialog() override;

  afx_msg void OnDestroy();
  afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
  DECLARE_MESSAGE_MAP()

  se::windows::ui::CDpiWindowBehavior m_dpi_behavior;
};

#endif  // !SE_COMMON_WINDOWS_BASE_PROPERTY_SHEET_H_
