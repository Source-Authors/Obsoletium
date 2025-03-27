// Copyright Valve Corporation, All rights reserved.
//
// DPI aware font.

#ifndef SE_COMMON_WINDOWS_DPI_AWARE_FONT_
#define SE_COMMON_WINDOWS_DPI_AWARE_FONT_

#include "windows/dpi_wnd_behavior.h"

class DpiAwareFont : public CFont {
 public:
  DpiAwareFont() : DpiAwareFont{USER_DEFAULT_SCREEN_DPI} {}
  explicit DpiAwareFont(unsigned dpiY) : m_dpi_y{dpiY}, CFont{} {}

  BOOL CreatePointFont(int nPointSize, LPCTSTR lpszFaceName, CDC* pDC = NULL);
  BOOL CreatePointFontIndirect(const LOGFONT* lpLogFont, CDC* pDC = NULL);

 private:
  unsigned m_dpi_y;
};

#endif  // !SE_COMMON_WINDOWS_DPI_AWARE_FONT_
