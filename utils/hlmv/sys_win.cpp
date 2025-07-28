// Copyright Valve Corporation, All rights reserved.
//
// Abstract system dependent functions.

#include "sys.h"
#include "tier1/strtools.h"

#include "winlite.h"

void Sys_CopyStringToClipboard(const char *out) {
  if (!out || !::OpenClipboard(nullptr)) return;

  // Remove the current Clipboard contents
  if (!::EmptyClipboard()) {
    ::CloseClipboard();
    return;
  }

  ::EmptyClipboard();

  intp len = Q_strlen(out) + 1;

  HGLOBAL clipbuffer = GlobalAlloc(GMEM_DDESHARE, len);

  char *buffer = (char *)GlobalLock(clipbuffer);
  Q_strncpy(buffer, out, len);
  GlobalUnlock(clipbuffer);

  ::SetClipboardData(CF_TEXT, clipbuffer);

  ::CloseClipboard();
}
