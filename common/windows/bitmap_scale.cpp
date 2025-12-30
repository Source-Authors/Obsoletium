// Copyright Valve Corporation, All rights reserved.
//
// Bitmap scaling functions.

#include "stdafx.h"
#include "bitmap_scale.h"

#include "tier0/dbg.h"

#include <afxcontrolbarutil.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace se::windows::ui {

/**
 * @brief Scale bitmap from old DPI to a new one.
 * @param sourceBmp Bitmap to scale.
 * @param oldDpiX Old DPI X.
 * @param oldDpiY Old DPI Y.
 * @param newDpiX New DPI X.
 * @param newDpiY New DPI Y.
 * @return Scaled bitmap.
 */
HBITMAP ScaleBitmapForDpi(HBITMAP sourceBmp, unsigned oldDpiX, unsigned oldDpiY,
                          unsigned newDpiX, unsigned newDpiY) {
  Assert(sourceBmp);

  Assert(oldDpiX > 0);
  Assert(oldDpiY > 0);
  Assert(newDpiX > 0);
  Assert(newDpiY > 0);

  BITMAP bm = {};
  ::GetObject(sourceBmp, sizeof(BITMAP), &bm);

  const CSize sourceSize{bm.bmWidth, bm.bmHeight};
  const CSize scaledSize{::MulDiv(sourceSize.cx, newDpiX, oldDpiX),
                         ::MulDiv(sourceSize.cy, newDpiY, oldDpiY)};

  CWindowDC screenDC{nullptr};
  HBITMAP scaledBmp{::CreateCompatibleBitmap(screenDC.GetSafeHdc(),
                                             scaledSize.cx, scaledSize.cy)};

  CDC srcCompatDC;
  srcCompatDC.CreateCompatibleDC(&screenDC);
  CDC destCompatDC;
  destCompatDC.CreateCompatibleDC(&screenDC);

  CMemDC srcDC{srcCompatDC, CRect(CPoint(), sourceSize)};
  auto oldSrcBmp = srcDC.GetDC().SelectObject(sourceBmp);
  RunCodeAtScopeExit(srcDC.GetDC().SelectObject(oldSrcBmp));

  CMemDC destDC{destCompatDC, CRect(CPoint(), scaledSize)};
  auto oldDestBmp = destDC.GetDC().SelectObject(scaledBmp);
  RunCodeAtScopeExit(destDC.GetDC().SelectObject(oldDestBmp));

  destDC.GetDC().StretchBlt(0, 0, scaledSize.cx, scaledSize.cy, &srcDC.GetDC(),
                            0, 0, sourceSize.cx, sourceSize.cy, SRCCOPY);

  return scaledBmp;
}

}  // namespace se::windows::ui
