// Copyright Valve Corporation, All rights reserved.
//
// CTextConsoleWin32.cpp: Win32 implementation of the TextConsole class.

#include "TextConsoleWin32.h"

#include "tier0/dbg.h"
#include "tier1/utlvector.h"

#include "winlite.h"

// Could possibly switch all this code over to using readline. This:
//   https://sourceforge.net/projects/mingweditline/
// readline() / add_history(char *)

#ifdef _WIN32

namespace {

BOOL WINAPI ConsoleHandlerRoutine(DWORD) { return TRUE; }

}  // namespace

namespace se::dedicated {

CTextConsoleWin32::CTextConsoleWin32() {
  m_szConsoleText[0] = '\0';
  hinput = NULL;
  houtput = NULL;
  Attrib = 0;
  statusline[0] = '\0';
}

bool CTextConsoleWin32::Init() {
  (void)::AllocConsole();

#ifdef PLATFORM_64BITS
  SetTitle("SOURCE DEDICATED SERVER - 64 Bit");
#else
  SetTitle("SOURCE DEDICATED SERVER");
#endif

  hinput = GetStdHandle(STD_INPUT_HANDLE);
  houtput = GetStdHandle(STD_OUTPUT_HANDLE);

  if (!SetConsoleCtrlHandler(&ConsoleHandlerRoutine, TRUE)) {
    Print("WARNING! TextConsole::Init: Could not attach console hook.\n");
  }

  Attrib = FOREGROUND_GREEN | FOREGROUND_INTENSITY | BACKGROUND_INTENSITY;

  SetWindowPos(GetConsoleWindow(), HWND_TOP, 0, 0, 0, 0,
               SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);

  memset(m_szConsoleText, 0, sizeof(m_szConsoleText));
  m_nConsoleTextLen = 0;
  m_nCursorPosition = 0;

  memset(m_szSavedConsoleText, 0, sizeof(m_szSavedConsoleText));
  m_nSavedConsoleTextLen = 0;

  memset(m_aszLineBuffer, 0, sizeof(m_aszLineBuffer));
  m_nTotalLines = 0;
  m_nInputLine = 0;
  m_nBrowseLine = 0;

  // these are log messages, not related to console
  Msg("Console initialized.\n");

  return CTextConsole::Init();
}

void CTextConsoleWin32::ShutDown() { ::FreeConsole(); }

void CTextConsoleWin32::SetVisible(bool visible) {
  ShowWindow(GetConsoleWindow(), visible ? SW_SHOW : SW_HIDE);
  m_ConsoleVisible = visible;
}

char *CTextConsoleWin32::GetLine(int index, char *buf, size_t buflen) {
  while (true) {
    INPUT_RECORD recs[1024];
    unsigned long numread;
    unsigned long numevents;

    if (!GetNumberOfConsoleInputEvents(hinput, &numevents)) {
      Error("CTextConsoleWin32::GetLine: !GetNumberOfConsoleInputEvents");
      return NULL;
    }

    if (numevents <= 0) break;

    if (!ReadConsoleInput(hinput, recs, std::size(recs), &numread)) {
      Error("CTextConsoleWin32::GetLine: !ReadConsoleInput");
      return NULL;
    }

    if (numread == 0) return NULL;

    for (unsigned long i = 0; i < numread; i++) {
      INPUT_RECORD *pRec = &recs[i];
      if (pRec->EventType != KEY_EVENT) continue;

      if (pRec->Event.KeyEvent.bKeyDown) {
        // check for cursor keys
        if (pRec->Event.KeyEvent.wVirtualKeyCode == VK_UP) {
          ReceiveUpArrow();
        } else if (pRec->Event.KeyEvent.wVirtualKeyCode == VK_DOWN) {
          ReceiveDownArrow();
        } else if (pRec->Event.KeyEvent.wVirtualKeyCode == VK_LEFT) {
          ReceiveLeftArrow();
        } else if (pRec->Event.KeyEvent.wVirtualKeyCode == VK_RIGHT) {
          ReceiveRightArrow();
        } else {
          char ch = pRec->Event.KeyEvent.uChar.AsciiChar;
          switch (ch) {
            case '\r':  // Enter
            {
              intp nLen = ReceiveNewline();
              if (nLen) {
                strncpy(buf, m_szConsoleText, buflen);
                buf[buflen - 1] = 0;
                return buf;
              }
            } break;

            case '\b':  // Backspace
              ReceiveBackspace();
              break;

            case '\t':  // TAB
              ReceiveTab();
              break;

            default:
              if ((ch >= ' ') &&
                  (ch <= '~'))  // dont' accept nonprintable chars
              {
                ReceiveStandardChar(ch);
              }
              break;
          }
        }
      }
    }
  }

  return NULL;
}

void CTextConsoleWin32::Print(const char *pszMsg) {
  if (m_nConsoleTextLen) {
    intp nLen = m_nConsoleTextLen;

    while (nLen--) {
      PrintRaw("\b \b");
    }
  }

  PrintRaw(pszMsg);

  if (m_nConsoleTextLen) {
    PrintRaw(m_szConsoleText, m_nConsoleTextLen);
  }

  UpdateStatus();
}

void CTextConsoleWin32::PrintRaw(const char *pszMsg, intp nChars) {
  unsigned long dummy;

  if (houtput == nullptr) {
    houtput = GetStdHandle(STD_OUTPUT_HANDLE);

    if (houtput == nullptr) return;
  }

  if (nChars <= 0) {
    nChars = V_strlen(pszMsg);

    if (nChars <= 0) return;
  }

  // filter out ASCII BEL characters because windows actually plays a
  // bell sound, which can be used to lag the server in a DOS attack.
  char *temp_buffer = nullptr;
  for (intp i = 0; i < nChars; ++i) {
    if (pszMsg[i] == '\a') {
      if (!temp_buffer) {
        temp_buffer = new char[nChars];

        memcpy(temp_buffer, pszMsg, nChars);
      }

      temp_buffer[i] = '.';
    }
  }

  WriteFile(houtput, temp_buffer ? temp_buffer : pszMsg, nChars, &dummy,
            nullptr);

  delete[] temp_buffer;  // usually NULL
}

int CTextConsoleWin32::GetWidth() {
  int nWidth = 0;

  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (GetConsoleScreenBufferInfo(houtput, &csbi)) {
    nWidth = csbi.dwSize.X;
  }

  if (nWidth <= 1) nWidth = 80;

  return nWidth;
}

void CTextConsoleWin32::SetStatusLine(const char *pszStatus) {
  strncpy(statusline, pszStatus, 80);
  statusline[79] = '\0';
  UpdateStatus();
}

void CTextConsoleWin32::UpdateStatus() {
  WORD wAttrib[80];

  for (auto &a : wAttrib) {
    // FOREGROUND_GREEN | FOREGROUND_INTENSITY |
    // BACKGROUND_INTENSITY ;
    a = Attrib;
  }

  COORD coord = {};
  DWORD dwWritten = 0;

  WriteConsoleOutputAttribute(houtput, wAttrib, std::size(wAttrib), coord,
                              &dwWritten);
  WriteConsoleOutputCharacterA(houtput, statusline, ssize(statusline) - 1,
                               coord, &dwWritten);
}

void CTextConsoleWin32::SetTitle(const char *pszTitle) {
  SetConsoleTitleA(pszTitle);
}

void CTextConsoleWin32::SetColor(WORD attrib) { Attrib = attrib; }

intp CTextConsoleWin32::ReceiveNewline(void) {
  intp nLen = 0;

  PrintRaw("\n");

  if (m_nConsoleTextLen) {
    nLen = m_nConsoleTextLen;

    m_szConsoleText[m_nConsoleTextLen] = 0;
    m_nConsoleTextLen = 0;
    m_nCursorPosition = 0;

    // cache line in buffer, but only if it's not a duplicate of the previous
    // line
    if ((m_nInputLine == 0) ||
        (strcmp(m_aszLineBuffer[m_nInputLine - 1], m_szConsoleText))) {
      strncpy(m_aszLineBuffer[m_nInputLine], m_szConsoleText,
              MAX_CONSOLE_TEXTLEN);

      m_nInputLine++;

      if (m_nInputLine > m_nTotalLines) m_nTotalLines = m_nInputLine;

      if (m_nInputLine >= MAX_BUFFER_LINES) m_nInputLine = 0;
    }

    m_nBrowseLine = m_nInputLine;
  }

  return nLen;
}

void CTextConsoleWin32::ReceiveBackspace() {
  intp nCount;

  if (m_nCursorPosition == 0) {
    return;
  }

  m_nConsoleTextLen--;
  m_nCursorPosition--;

  PrintRaw("\b");

  for (nCount = m_nCursorPosition; nCount < m_nConsoleTextLen; nCount++) {
    m_szConsoleText[nCount] = m_szConsoleText[nCount + 1];
    PrintRaw(m_szConsoleText + nCount, 1);
  }

  PrintRaw(" ");

  nCount = m_nConsoleTextLen;
  while (nCount >= m_nCursorPosition) {
    PrintRaw("\b");
    nCount--;
  }

  m_nBrowseLine = m_nInputLine;
}

void CTextConsoleWin32::ReceiveTab() {
  CUtlVector<char *> matches;

  m_szConsoleText[m_nConsoleTextLen] = 0;

  if (matches.Count() == 0) {
    return;
  }

  if (matches.Count() == 1) {
    char *pszCmdName;
    char *pszRest;

    pszCmdName = matches[0];
    pszRest = pszCmdName + strlen(m_szConsoleText);

    if (pszRest) {
      PrintRaw(pszRest);
      V_strcat_safe(m_szConsoleText, pszRest);
      m_nConsoleTextLen += strlen(pszRest);

      PrintRaw(" ");
      V_strcat_safe(m_szConsoleText, " ");
      m_nConsoleTextLen++;
    }
  } else {
    intp nLongestCmd;
    int nTotalColumns;
    int nCurrentColumn;
    char *pszCurrentCmd;
    int i = 0;

    nLongestCmd = 0;

    pszCurrentCmd = matches[0];
    while (pszCurrentCmd) {
      intp len = V_strlen(pszCurrentCmd);
      if (len > nLongestCmd) {
        nLongestCmd = len;
      }

      i++;
      pszCurrentCmd = (char *)matches[i];
    }

    nTotalColumns = (GetWidth() - 1) / (nLongestCmd + 1);
    nCurrentColumn = 0;

    PrintRaw("\n");

    // Would be nice if these were sorted, but not that big a deal
    pszCurrentCmd = matches[0];
    i = 0;
    while (pszCurrentCmd) {
      char szFormatCmd[256];

      nCurrentColumn++;

      if (nCurrentColumn > nTotalColumns) {
        PrintRaw("\n");
        nCurrentColumn = 1;
      }

      Q_snprintf(szFormatCmd, sizeof(szFormatCmd), "%-*s ", nLongestCmd,
                 pszCurrentCmd);
      PrintRaw(szFormatCmd);

      i++;
      pszCurrentCmd = matches[i];
    }

    PrintRaw("\n");
    PrintRaw(m_szConsoleText);
    // TODO: Tack on 'common' chars in all the matches, i.e. if I typed 'con'
    // and all the
    //       matches begin with 'connect_' then print the matches but also
    //       complete the command up to that point at least.
  }

  m_nCursorPosition = m_nConsoleTextLen;
  m_nBrowseLine = m_nInputLine;
}

void CTextConsoleWin32::ReceiveStandardChar(const char ch) {
  intp nCount;

  // If the line buffer is maxed out, ignore this char
  if (m_nConsoleTextLen >=
      (static_cast<intp>(sizeof(m_szConsoleText)) - 2)) {
    return;
  }

  nCount = m_nConsoleTextLen;
  while (nCount > m_nCursorPosition) {
    m_szConsoleText[nCount] = m_szConsoleText[nCount - 1];
    nCount--;
  }

  m_szConsoleText[m_nCursorPosition] = ch;

  PrintRaw(m_szConsoleText + m_nCursorPosition,
           m_nConsoleTextLen - m_nCursorPosition + 1);

  m_nConsoleTextLen++;
  m_nCursorPosition++;

  nCount = m_nConsoleTextLen;
  while (nCount > m_nCursorPosition) {
    PrintRaw("\b");
    nCount--;
  }

  m_nBrowseLine = m_nInputLine;
}

void CTextConsoleWin32::ReceiveUpArrow() {
  int nLastCommandInHistory;

  nLastCommandInHistory = m_nInputLine + 1;
  if (nLastCommandInHistory > m_nTotalLines) {
    nLastCommandInHistory = 0;
  }

  if (m_nBrowseLine == nLastCommandInHistory) {
    return;
  }

  if (m_nBrowseLine == m_nInputLine) {
    if (m_nConsoleTextLen > 0) {
      // Save off current text
      strncpy(m_szSavedConsoleText, m_szConsoleText, m_nConsoleTextLen);
      // No terminator, it's a raw buffer we always know the length of
    }

    m_nSavedConsoleTextLen = m_nConsoleTextLen;
  }

  m_nBrowseLine--;
  if (m_nBrowseLine < 0) {
    m_nBrowseLine = m_nTotalLines - 1;
  }

  while (m_nConsoleTextLen--)  // delete old line
  {
    PrintRaw("\b \b");
  }

  // copy buffered line
  PrintRaw(m_aszLineBuffer[m_nBrowseLine]);

  strncpy(m_szConsoleText, m_aszLineBuffer[m_nBrowseLine], MAX_CONSOLE_TEXTLEN);
  m_nConsoleTextLen = V_strlen(m_aszLineBuffer[m_nBrowseLine]);

  m_nCursorPosition = m_nConsoleTextLen;
}

void CTextConsoleWin32::ReceiveDownArrow() {
  if (m_nBrowseLine == m_nInputLine) {
    return;
  }

  m_nBrowseLine++;
  if (m_nBrowseLine > m_nTotalLines) {
    m_nBrowseLine = 0;
  }

  while (m_nConsoleTextLen--)  // delete old line
  {
    PrintRaw("\b \b");
  }

  if (m_nBrowseLine == m_nInputLine) {
    if (m_nSavedConsoleTextLen > 0) {
      // Restore current text
      strncpy(m_szConsoleText, m_szSavedConsoleText, m_nSavedConsoleTextLen);
      // No terminator, it's a raw buffer we always know the length of

      PrintRaw(m_szConsoleText, m_nSavedConsoleTextLen);
    }

    m_nConsoleTextLen = m_nSavedConsoleTextLen;
  } else {
    // copy buffered line
    PrintRaw(m_aszLineBuffer[m_nBrowseLine]);

    strncpy(m_szConsoleText, m_aszLineBuffer[m_nBrowseLine],
            MAX_CONSOLE_TEXTLEN);

    m_nConsoleTextLen = V_strlen(m_aszLineBuffer[m_nBrowseLine]);
  }

  m_nCursorPosition = m_nConsoleTextLen;
}

void CTextConsoleWin32::ReceiveLeftArrow() {
  if (m_nCursorPosition == 0) {
    return;
  }

  PrintRaw("\b");
  m_nCursorPosition--;
}

void CTextConsoleWin32::ReceiveRightArrow() {
  if (m_nCursorPosition == m_nConsoleTextLen) {
    return;
  }

  PrintRaw(m_szConsoleText + m_nCursorPosition, 1);
  m_nCursorPosition++;
}

}  // namespace se::dedicated

#endif  // _WIN32
