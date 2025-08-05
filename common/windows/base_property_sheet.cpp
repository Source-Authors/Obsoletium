// Copyright Valve Corporation, All rights reserved.
//
// Base property sheet.

#include "stdafx.h"
#include "base_property_sheet.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_DYNAMIC(CBasePropertySheet, CPropertySheet)

BEGIN_MESSAGE_MAP(CBasePropertySheet, CPropertySheet)
  ON_WM_DESTROY()
  ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
END_MESSAGE_MAP()

CBasePropertySheet::CBasePropertySheet() : m_dpi_behavior{false} {}

CBasePropertySheet::CBasePropertySheet(UINT nIDCaption, CWnd* pParentWnd,
                                       UINT iSelectPage)
    : CPropertySheet{nIDCaption, pParentWnd, iSelectPage},
      m_dpi_behavior{false} {}

CBasePropertySheet::CBasePropertySheet(LPCTSTR pszCaption, CWnd* pParentWnd,
                                       UINT iSelectPage)
    : CPropertySheet{pszCaption, pParentWnd, iSelectPage},
      m_dpi_behavior{false} {}

// extended construction
CBasePropertySheet::CBasePropertySheet(UINT nIDCaption, CWnd* pParentWnd,
                                       UINT iSelectPage, HBITMAP hbmWatermark,
                                       HPALETTE hpalWatermark,
                                       HBITMAP hbmHeader)
    : CPropertySheet{nIDCaption,   pParentWnd,    iSelectPage,
                     hbmWatermark, hpalWatermark, hbmHeader},
      m_dpi_behavior{false} {}

CBasePropertySheet::CBasePropertySheet(LPCTSTR pszCaption, CWnd* pParentWnd,
                                       UINT iSelectPage, HBITMAP hbmWatermark,
                                       HPALETTE hpalWatermark,
                                       HBITMAP hbmHeader)
    : CPropertySheet{pszCaption,   pParentWnd,    iSelectPage,
                     hbmWatermark, hpalWatermark, hbmHeader},
      m_dpi_behavior{false} {}

BOOL CBasePropertySheet::OnInitDialog() {
  const BOOL rc{__super::OnInitDialog()};
  // dimhotepus: Try to add main icon for dialog.
  HANDLE hExeIcon = LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101),
                               IMAGE_ICON, 0, 0, LR_SHARED);
  if (hExeIcon) {
    SendMessage(WM_SETICON, ICON_BIG, (LPARAM)hExeIcon);
  }
  m_dpi_behavior.OnCreateWindow(m_hWnd);
  return rc;
}

void CBasePropertySheet::OnDestroy() {
  m_dpi_behavior.OnDestroyWindow();

  __super::OnDestroy();
}

LRESULT CBasePropertySheet::OnDpiChanged(WPARAM wParam, LPARAM lParam) {
  return m_dpi_behavior.OnWindowDpiChanged(wParam, lParam);
}
