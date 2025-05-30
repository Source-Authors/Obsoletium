// Copyright Valve Corporation, All rights reserved.
//
// Base status bar.

#include "stdafx.h"
#include "base_status_bar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_DYNCREATE(CBaseStatusBar, CStatusBar)

BEGIN_MESSAGE_MAP(CBaseStatusBar, CStatusBar)
  ON_WM_CREATE()
  ON_WM_DESTROY()
  ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
END_MESSAGE_MAP()

CBaseStatusBar::CBaseStatusBar()
    : m_dpi_behavior{false}, m_indicator_count{0} {}

BOOL CBaseStatusBar::SetIndicators(const UINT* lpIDArray, int nIDCount) {
  m_indicator_count = nIDCount;
  return __super::SetIndicators(lpIDArray, nIDCount);
}

int CBaseStatusBar::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  const int rc{__super::OnCreate(lpCreateStruct)};
  if (!rc) {
    m_dpi_behavior.OnCreateWindow(m_hWnd);
  }
  return rc;
}

void CBaseStatusBar::OnDestroy() {
  m_dpi_behavior.OnDestroyWindow();

  __super::OnDestroy();
}

LRESULT CBaseStatusBar::OnDpiChanged(WPARAM wParam, LPARAM lParam) {
  // Need initial height.
  const int initialHeight{GetHeight()};

  const LRESULT rc{m_dpi_behavior.OnWindowDpiChanged(wParam, lParam)};

  // Custom scaling for height as default DPI behavior is not applied.
  ScaleHeight(initialHeight);

  return rc;
}

int CBaseStatusBar::GetHeight() const {
  CRect rc;
  return GetStatusBarCtrl().GetRect(0, &rc) ? rc.Height() : -1;
}

void CBaseStatusBar::ScaleHeight(int currentHeight) {
  if (currentHeight >= 0) {
    const int scaledHeight{m_dpi_behavior.ScaleOnY(currentHeight)};

    // First send SB_SETMINHEIGHT.
    GetStatusBarCtrl().SetMinHeight(scaledHeight);

    // App must send WM_SIZE with 0 params to redraw according to
    // SB_SETMINHEIGHT docs.
    SendMessage(WM_SIZE);

    unsigned id, style;
    int width;

    // Scale panels.
    for (int i = 0; i < m_indicator_count; ++i) {
      GetPaneInfo(0, id, style, width);
      SetPaneInfo(0, id, style, m_dpi_behavior.ScaleOnX(width));
    }
  }
}