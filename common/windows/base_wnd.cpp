// Copyright Valve Corporation, All rights reserved.
//
// Base window.

#include "stdafx.h"
#include "base_wnd.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_DYNCREATE(CBaseWnd, CWnd)

BEGIN_MESSAGE_MAP(CBaseWnd, CWnd)
  ON_WM_CREATE()
  ON_WM_DESTROY()
  ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
END_MESSAGE_MAP()

CBaseWnd::CBaseWnd() = default;

int CBaseWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  const int rc{__super::OnCreate(lpCreateStruct)};
  if (!rc) {
    m_dpi_behavior.OnCreateWindow(m_hWnd);
  }
  return rc;
}

void CBaseWnd::OnDestroy() {
  m_dpi_behavior.OnDestroyWindow();

  __super::OnDestroy();
}

LRESULT CBaseWnd::OnDpiChanged(WPARAM wParam, LPARAM lParam) {
  return m_dpi_behavior.OnWindowDpiChanged(wParam, lParam);
}

void CBaseWnd::CalcWindowRect(LPRECT lpClientRect, UINT nAdjustType) {
  DWORD dwExStyle = GetExStyle();
  if (nAdjustType == 0) dwExStyle &= ~WS_EX_CLIENTEDGE;
  ::AdjustWindowRectExForDpi(lpClientRect, GetStyle(), FALSE, dwExStyle, m_dpi_behavior.GetCurrentDpiX());
}
