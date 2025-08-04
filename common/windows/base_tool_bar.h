// Copyright Valve Corporation, All rights reserved.
//
// Base tool bar.

#ifndef SE_COMMON_WINDOWS_BASE_TOOL_BAR_
#define SE_COMMON_WINDOWS_BASE_TOOL_BAR_

#include "windows/dpi_wnd_behavior.h"

class CBaseToolBar : public CToolBar {
  DECLARE_DYNCREATE(CBaseToolBar)

 public:
  CBaseToolBar();

  BOOL CreateEx(CWnd* pParentWnd, DWORD dwCtrlStyle = TBSTYLE_FLAT,
                DWORD dwStyle = WS_CHILD | WS_VISIBLE | CBRS_ALIGN_TOP,
                CRect rcBorders = CRect(0, 0, 0, 0),
                UINT nID = AFX_IDW_TOOLBAR) override;

  BOOL LoadToolBarForDpi(LPCTSTR lpszResourceName, LPCTSTR lpszBitmapName,
                         bool isTransparentBitmap);
  BOOL LoadToolBarForDpi(UINT nIDResource, UINT nIDBitmap = UINT_MAX,
                         bool isTransparentBitmap = true) {
    return LoadToolBarForDpi(MAKEINTRESOURCE(nIDResource),
                             nIDBitmap == UINT_MAX
                                 ? MAKEINTRESOURCE(nIDResource)
                                 : MAKEINTRESOURCE(nIDBitmap), isTransparentBitmap);
  }

 protected:
  // Generated message map functions
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnDestroy();
  afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
  DECLARE_MESSAGE_MAP()

  void SetSizesForDpi(SIZE sizeButton, SIZE sizeImage);
  BOOL LoadBitmapForDpi(LPCTSTR lpszResourceName, bool isTransparent);

 private:
  se::windows::ui::CDpiWindowBehavior m_dpi_behavior;
};

#endif  // !SE_COMMON_WINDOWS_BASE_TOOL_BAR_
