// Copyright Valve Corporation, All rights reserved.
//
// DPI aware font.

#include "stdafx.h"
#include "dpi_aware_font.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BOOL DpiAwareFont::CreatePointFont(int nPointSize, LPCTSTR lpszFaceName,
                                   CDC* pDC) {
  ASSERT(AfxIsValidString(lpszFaceName));

  LOGFONT logFont;
  memset(&logFont, 0, sizeof(LOGFONT));
  logFont.lfCharSet = DEFAULT_CHARSET;
  logFont.lfHeight = nPointSize;
  Checked::tcsncpy_s(logFont.lfFaceName, _countof(logFont.lfFaceName),
                     lpszFaceName, _TRUNCATE);

  return CreatePointFontIndirect(&logFont, pDC);
}

BOOL DpiAwareFont::CreatePointFontIndirect(const LOGFONT* lpLogFont, CDC* pDC) {
  ASSERT(AfxIsValidAddress(lpLogFont, sizeof(LOGFONT), FALSE));
  HDC hDC;
  if (pDC != NULL) {
    ASSERT_VALID(pDC);
    ASSERT(pDC->m_hAttribDC != NULL);
    hDC = pDC->m_hAttribDC;
  } else
    hDC = ::GetDC(NULL);

  // convert nPointSize to logical units based on pDC
  LOGFONT logFont = *lpLogFont;
  POINT pt;
  // 72 points/inch, 10 decipoints/point
  pt.y = ::MulDiv(m_dpi_y, logFont.lfHeight, 720);
  pt.x = 0;
  ::DPtoLP(hDC, &pt, 1);
  POINT ptOrg = {0, 0};
  ::DPtoLP(hDC, &ptOrg, 1);
  logFont.lfHeight = -abs(pt.y - ptOrg.y);

  if (pDC == NULL) ReleaseDC(NULL, hDC);

  return CreateFontIndirect(&logFont);
}