// Copyright Valve Corporation, All rights reserved.
//
// Base child MDI window.

#include "stdafx.h"
#include "base_mdi_child_wnd.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_DYNCREATE(CBaseMDIChildWnd, CMDIChildWnd)

BEGIN_MESSAGE_MAP(CBaseMDIChildWnd, CMDIChildWnd)
  ON_WM_CREATE()
  ON_WM_DESTROY()
  ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
END_MESSAGE_MAP()

CBaseMDIChildWnd::CBaseMDIChildWnd() = default;

int CBaseMDIChildWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  const int rc{__super::OnCreate(lpCreateStruct)};
  if (!rc) {
    m_dpi_behavior.OnCreateWindow(m_hWnd);
  }
  return rc;
}

void CBaseMDIChildWnd::OnDestroy() {
  m_dpi_behavior.OnDestroyWindow();

  __super::OnDestroy();
}

LRESULT CBaseMDIChildWnd::OnDpiChanged(WPARAM wParam, LPARAM lParam) {
  return m_dpi_behavior.OnWindowDpiChanged(wParam, lParam);
}
