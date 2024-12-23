// Copyright Valve Corporation, All rights reserved.
//
// Base dialog.

#include "stdafx.h"
#include "base_dlg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_DYNAMIC(CBaseDlg, CDialog)

BEGIN_MESSAGE_MAP(CBaseDlg, CDialog)
  ON_WM_DESTROY()
  ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
END_MESSAGE_MAP()

CBaseDlg::CBaseDlg() = default;

CBaseDlg::CBaseDlg(LPCTSTR lpszTemplateName, CWnd* pParentWnd)
    : CDialog{lpszTemplateName, pParentWnd} {}

CBaseDlg::CBaseDlg(UINT nIDTemplate, CWnd* pParentWnd)
    : CDialog{nIDTemplate, pParentWnd} {}

CBaseDlg::~CBaseDlg() = default;

BOOL CBaseDlg::OnInitDialog() {
  const BOOL rc{__super::OnInitDialog()};
  // dimhotepus: Try to add main icon for dialog.
  HANDLE hExeIcon = LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101),
                               IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
  if (hExeIcon) {
    SendMessage(WM_SETICON, ICON_BIG, (LPARAM)hExeIcon);
  }

  m_dpi_behavior.OnCreateWindow(m_hWnd);
  return rc;
}

void CBaseDlg::OnDestroy() {
  m_dpi_behavior.OnDestroyWindow();

  __super::OnDestroy();
}

LRESULT CBaseDlg::OnDpiChanged(WPARAM wParam, LPARAM lParam) {
  return m_dpi_behavior.OnWindowDpiChanged(wParam, lParam);
}
