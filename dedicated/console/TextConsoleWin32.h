// Copyright Valve Corporation, All rights reserved.
//
// TextConsoleWin32.h: Win32 interface for the TextConsole class.

#ifndef SE_DEDICATED_CONSOLE_TEXT_CONSOLE_WIN32_H_
#define SE_DEDICATED_CONSOLE_TEXT_CONSOLE_WIN32_H_

#include <cstddef>  // ptrdiff_t
#include "textconsole.h"

#define MAX_CONSOLE_TEXTLEN 256
#define MAX_BUFFER_LINES 30

using HANDLE = void *;
using WORD = unsigned short;

namespace se::dedicated {

class CTextConsoleWin32 : public CTextConsole {
 public:
  CTextConsoleWin32();
  virtual ~CTextConsoleWin32(){};

  // CTextConsole
  bool Init();
  void ShutDown();
  void Print(const char *pszMsg);

  void SetTitle(const char *pszTitle);
  void SetStatusLine(const char *pszStatus);
  void UpdateStatus();

  char *GetLine(int index, char *buf, size_t buflen);
  int GetWidth();

  void SetVisible(bool visible);

 protected:
  // CTextConsoleWin32
  void SetColor(WORD);
  void PrintRaw(const char *pszMsg, ptrdiff_t nChars = -1);

 private:
  char m_szConsoleText[MAX_CONSOLE_TEXTLEN];  // console text buffer
  ptrdiff_t m_nConsoleTextLen;                // console textbuffer length
  ptrdiff_t m_nCursorPosition;  // position in the current input line

  // Saved input data when scrolling back through command history
  char m_szSavedConsoleText[MAX_CONSOLE_TEXTLEN];  // console text buffer
  ptrdiff_t m_nSavedConsoleTextLen;                // console textbuffer length

  char m_aszLineBuffer[MAX_BUFFER_LINES]
                      [MAX_CONSOLE_TEXTLEN];  // command buffer last
                                              // MAX_BUFFER_LINES commands
  int m_nInputLine;                           // Current line being entered
  int m_nBrowseLine;  // current buffer line for up/down arrow
  int m_nTotalLines;  // # of nonempty lines in the buffer

  ptrdiff_t ReceiveNewline();
  void ReceiveBackspace();
  void ReceiveTab();
  void ReceiveStandardChar(const char ch);
  void ReceiveUpArrow();
  void ReceiveDownArrow();
  void ReceiveLeftArrow();
  void ReceiveRightArrow();

 private:
  HANDLE hinput;   // standard input handle
  HANDLE houtput;  // standard output handle
  WORD Attrib;     // attrib colours for status bar

  char statusline[81];  // first line in console is status line
};

}  // namespace se::dedicated

#endif  // !SE_DEDICATED_CONSOLE_TEXT_CONSOLE_WIN32_H_
