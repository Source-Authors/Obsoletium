// Copyright Valve Corporation, All rights reserved.
//
// Bitmap scaling functions.

#ifndef SE_COMMON_WINDOWS_BITMAP_SCALE_H_
#define SE_COMMON_WINDOWS_BITMAP_SCALE_H_

#include "tier0/basetypes.h"

FORWARD_DECLARE_HANDLE(HBITMAP);

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
                          unsigned newDpiX, unsigned newDpiY);

}  // namespace se::windows::ui

#endif  // !SE_COMMON_WINDOWS_BITMAP_SCALE_H_
