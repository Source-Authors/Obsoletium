// Copyright Valve Corporation, All rights reserved.
//
// Base combobox window.

#include "stdafx.h"
#include "base_combo_box.h"
#include "base_wnd.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_DYNAMIC(CBaseComboBox, CComboBox)

BEGIN_MESSAGE_MAP(CBaseComboBox, CComboBox)
  ON_WM_CREATE()
  ON_WM_DESTROY()
  ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
END_MESSAGE_MAP()

CBaseComboBox::CBaseComboBox() : m_dpi_behavior_created{false} {}

BOOL CBaseComboBox::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd,
                           UINT nID) {
  const int rc{__super::Create(dwStyle, rect, pParentWnd, nID)};

  if (rc && !m_dpi_behavior_created) {
    m_dpi_behavior.OnCreateWindow(m_hWnd);
    m_dpi_behavior_created = true;
  }

  return rc;
}

void CBaseComboBox::SubclassDlgItem(UINT nID, CWnd* pParent) {
  if (!m_dpi_behavior_created) {
    Assert(pParent);
    CWnd* pWnd{pParent->GetDlgItem(nID)};
    Assert(pWnd);

    m_dpi_behavior.OnCreateWindow(pWnd->GetSafeHwnd());
    m_dpi_behavior_created = true;
  }

  __super::SubclassDlgItem(nID, pParent);
}

int CBaseComboBox::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  const int rc{__super::OnCreate(lpCreateStruct)};

  if (!rc && !m_dpi_behavior_created) {
    m_dpi_behavior.OnCreateWindow(m_hWnd);
    m_dpi_behavior_created = true;
  }

  return rc;
}

void CBaseComboBox::OnDestroy() {
  m_dpi_behavior.OnDestroyWindow();

  __super::OnDestroy();
}

LRESULT CBaseComboBox::OnDpiChanged(WPARAM wParam, LPARAM lParam) {
  return m_dpi_behavior.OnWindowDpiChanged(wParam, lParam);
}
