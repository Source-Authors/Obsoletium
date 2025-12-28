// Copyright Valve Corporation, All rights reserved.
//
// Base tool bar.

#include "stdafx.h"
#include "base_tool_bar.h"
#include "bitmap_scale.h"
#include "tier0/platform.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace {

struct CToolBarData {
  WORD wVersion;
  WORD wWidth;
  WORD wHeight;
  WORD wItemCount;

  WORD* items() { return (WORD*)(this + 1); }
};

}  // namespace

IMPLEMENT_DYNCREATE(CBaseToolBar, CToolBar)

BEGIN_MESSAGE_MAP(CBaseToolBar, CToolBar)
  ON_WM_CREATE()
  ON_WM_DESTROY()
  ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
END_MESSAGE_MAP()

CBaseToolBar::CBaseToolBar() : m_dpi_behavior{false} {}

BOOL CBaseToolBar::CreateEx(CWnd* pParentWnd, DWORD dwCtrlStyle, DWORD dwStyle,
                            CRect rcBorders, UINT nID) {
  const BOOL rc{
      __super::CreateEx(pParentWnd, dwCtrlStyle, dwStyle, rcBorders, nID)};
  if (rc) {
    SetSizesForDpi(m_sizeButton, m_sizeImage);
  }
  return rc;
}

BOOL CBaseToolBar::LoadToolBarForDpi(LPCTSTR lpszResourceName,
                                     LPCTSTR lpszBitmapName,
                                     bool isTransparentBitmap) {
  ASSERT_VALID(this);
  Assert(lpszResourceName != nullptr);
  Assert(lpszBitmapName != nullptr);

  // determine location of the bitmap in resource fork
  HINSTANCE hInst{::AfxFindResourceHandle(lpszResourceName, RT_TOOLBAR)};
  HRSRC hRsrc{::FindResource(hInst, lpszResourceName, RT_TOOLBAR)};
  if (hRsrc == nullptr) return FALSE;

  HGLOBAL hGlobal{::LoadResource(hInst, hRsrc)};
  if (hGlobal == nullptr) return FALSE;

  auto* pData = static_cast<CToolBarData*>(::LockResource(hGlobal));
  if (pData == nullptr) return FALSE;
  ASSERT(pData->wVersion == 1);

  auto* pItems = static_cast<UINT*>(alloca(sizeof(UINT) * pData->wItemCount));
  for (int i = 0; i < pData->wItemCount; i++) pItems[i] = pData->items()[i];
  BOOL bResult = SetButtons(pItems, pData->wItemCount);

  if (bResult) {
    // set new sizes of the buttons
    CSize sizeImage(pData->wWidth, pData->wHeight);
    CSize sizeButton(pData->wWidth + 7, pData->wHeight + 7);
    SetSizesForDpi(sizeButton, sizeImage);

    // load bitmap now that sizes are known by the toolbar control
    bResult = LoadBitmapForDpi(lpszBitmapName, isTransparentBitmap);
  }

  ::UnlockResource(hGlobal);
  // dimhotepus: Since 32 bit windows LoadResource doesn't need FreeResource
  // FreeResource(hGlobal);

  return bResult;
}

void CBaseToolBar::SetSizesForDpi(SIZE sizeButton, SIZE sizeImage) {
  const unsigned current_dpi_x{m_dpi_behavior.GetCurrentDpiX()};
  const unsigned current_dpi_y{m_dpi_behavior.GetCurrentDpiY()};

  // button must be big enough to hold image per MFC doc.
  //   + 7 pixels on x
  //   + 6 pixels on y
  const CSize scaledSizeImage{
      ::MulDiv(sizeImage.cx, current_dpi_x, m_dpi_behavior.GetPreviousDpiX()),
      ::MulDiv(sizeImage.cy, current_dpi_y, m_dpi_behavior.GetPreviousDpiY())};
  const CSize scaledSizeButton{scaledSizeImage.cx + m_dpi_behavior.ScaleOnX(7),
                               scaledSizeImage.cy + m_dpi_behavior.ScaleOnY(7)};

  __super::SetSizes(scaledSizeButton, scaledSizeImage);
}

BOOL CBaseToolBar::LoadBitmapForDpi(LPCTSTR lpszResourceName,
                                    bool isTransparent) {
  ASSERT_VALID(this);
  ASSERT(lpszResourceName != nullptr);

  // determine location of the bitmap in resource fork
  HINSTANCE hInstImageWell{
      ::AfxFindResourceHandle(lpszResourceName, RT_BITMAP)};
  HRSRC hRsrcImageWell{
      ::FindResource(hInstImageWell, lpszResourceName, RT_BITMAP)};
  if (hRsrcImageWell == nullptr) return FALSE;

  {
    // load the colored bitmap
    HBITMAP hbmImageWell{(HBITMAP)::LoadImage(
        ::AfxGetInstanceHandle(), lpszResourceName, IMAGE_BITMAP, 0, 0,
        LR_CREATEDIBSECTION | (isTransparent ? LR_LOADTRANSPARENT : 0))};
    if (hbmImageWell == nullptr) return FALSE;

    RunCodeAtScopeExit(::DeleteObject(hbmImageWell));

    // scale the bitmap
    HBITMAP scaledBitmap{se::windows::ui::ScaleBitmapForDpi(
        hbmImageWell, m_dpi_behavior.GetPreviousDpiX(),
        m_dpi_behavior.GetPreviousDpiY(), m_dpi_behavior.GetCurrentDpiX(),
        m_dpi_behavior.GetCurrentDpiY())};

    // tell common control toolbar about the new bitmap
    if (!AddReplaceBitmap(scaledBitmap)) return FALSE;
  }

  // remember the resource handles so the bitmap can be recolored if necessary
  m_hInstImageWell = hInstImageWell;
  m_hRsrcImageWell = hRsrcImageWell;

  return TRUE;
}

int CBaseToolBar::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  const int rc{__super::OnCreate(lpCreateStruct)};

  if (!rc && m_hWnd) {
    m_dpi_behavior.OnCreateWindow(m_hWnd);
  }

  return rc;
}

void CBaseToolBar::OnDestroy() {
  m_dpi_behavior.OnDestroyWindow();

  __super::OnDestroy();
}

LRESULT CBaseToolBar::OnDpiChanged(WPARAM wParam, LPARAM lParam) {
  const LRESULT rc{m_dpi_behavior.OnWindowDpiChanged(wParam, lParam)};

  if (!rc) {
    // Recompute sizes.
    SetSizesForDpi(m_sizeButton, m_sizeImage);
  }

  return rc;
}
