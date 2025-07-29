// Copyright Valve Corporation, All rights reserved.
//
// Base property page.

#include "stdafx.h"
#include "base_property_page.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_DYNAMIC(CBasePropertyPage, CPropertyPage)

BEGIN_MESSAGE_MAP(CBasePropertyPage, CPropertyPage)
  ON_WM_DESTROY()
  ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
END_MESSAGE_MAP()

CBasePropertyPage::CBasePropertyPage() : m_dpi_behavior{false} {}

CBasePropertyPage::CBasePropertyPage(UINT nIDTemplate, UINT nIDCaption,
                                     DWORD dwSize)
    : CPropertyPage{nIDTemplate, nIDCaption, dwSize}, m_dpi_behavior{false} {}

CBasePropertyPage::CBasePropertyPage(LPCTSTR lpszTemplateName, UINT nIDCaption,
                                     DWORD dwSize)
    : CPropertyPage{lpszTemplateName, nIDCaption, dwSize},
      m_dpi_behavior{false} {}

CBasePropertyPage::CBasePropertyPage(UINT nIDTemplate, UINT nIDCaption,
                                     UINT nIDHeaderTitle,
                                     UINT nIDHeaderSubTitle, DWORD dwSize)
    : CPropertyPage{nIDTemplate, nIDCaption, nIDHeaderTitle, nIDHeaderSubTitle,
                    dwSize},
      m_dpi_behavior{false} {}

CBasePropertyPage::CBasePropertyPage(LPCTSTR lpszTemplateName, UINT nIDCaption,
                                     UINT nIDHeaderTitle,
                                     UINT nIDHeaderSubTitle, DWORD dwSize)
    : CPropertyPage{lpszTemplateName, nIDCaption, nIDHeaderTitle,
                    nIDHeaderSubTitle, dwSize},
      m_dpi_behavior{false} {}

BOOL CBasePropertyPage::OnInitDialog() {
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

void CBasePropertyPage::OnDestroy() {
  m_dpi_behavior.OnDestroyWindow();

  __super::OnDestroy();
}

LRESULT CBasePropertyPage::OnDpiChanged(WPARAM wParam, LPARAM lParam) {
  return m_dpi_behavior.OnWindowDpiChanged(wParam, lParam);
}
