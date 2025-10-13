// Copyright Valve Corporation, All rights reserved.

#include "conproc.h"

#ifdef _WIN32

#include <system_error>

#include "winlite.h"
#include "isystem.h"
#include "tier0/icommandline.h"
#include "tier0/threadtools.h"
#include "tier1/strtools.h"

namespace {

HANDLE remote_console_thread_handle;
HANDLE done_event_handle;
HANDLE file_buffer_handle;
HANDLE child_send_event_handle;
HANDLE parent_send_event_handle;
HANDLE std_out_handle;
HANDLE std_in_handle;

[[nodiscard]] std::error_code GetSystemError(
    int rc = ::GetLastError()) noexcept {
  return std::error_code{rc, std::system_category()};
}

[[nodiscard]] std::error_code GetGenericError(int rc = errno) noexcept {
  return std::error_code{rc, std::generic_category()};
}

BOOL SetConsoleWidthAndHeight(HANDLE std_out, short width, short height,
                              se::dedicated::ISystem *system) {
  COORD max_coord = GetLargestConsoleWindowSize(std_out);
  if (max_coord.X == 0 && max_coord.Y == 0) {
    system->Printf("\n\nGetLargestConsoleWindowSize failed w/e: %s.\n",
                   GetSystemError().message().c_str());
    return FALSE;
  }

  if (width > max_coord.X) width = max_coord.X;
  if (height > max_coord.Y) height = max_coord.Y;

  CONSOLE_SCREEN_BUFFER_INFO info;
  if (!GetConsoleScreenBufferInfo(std_out, &info)) {
    system->Printf("\n\nGetConsoleScreenBufferInfo(std_out) failed w/e: %s.\n",
                   GetSystemError().message().c_str());
    return FALSE;
  }

  // height
  info.srWindow.Left = 0;
  info.srWindow.Right = info.dwSize.X - 1;
  info.srWindow.Top = 0;
  info.srWindow.Bottom = height - 1;

  if (height < info.dwSize.Y) {
    if (!SetConsoleWindowInfo(std_out, TRUE, &info.srWindow)) return FALSE;

    info.dwSize.Y = height;

    if (!SetConsoleScreenBufferSize(std_out, info.dwSize)) return FALSE;
  } else if (height > info.dwSize.Y) {
    info.dwSize.Y = height;

    if (!SetConsoleScreenBufferSize(std_out, info.dwSize)) return FALSE;
    if (!SetConsoleWindowInfo(std_out, TRUE, &info.srWindow)) return FALSE;
  }

  if (!GetConsoleScreenBufferInfo(std_out, &info)) return FALSE;

  // width
  info.srWindow.Left = 0;
  info.srWindow.Right = width - 1;
  info.srWindow.Top = 0;
  info.srWindow.Bottom = info.dwSize.Y - 1;

  if (width < info.dwSize.X) {
    if (!SetConsoleWindowInfo(std_out, TRUE, &info.srWindow)) return FALSE;

    info.dwSize.X = width;

    if (!SetConsoleScreenBufferSize(std_out, info.dwSize)) return FALSE;
  } else if (width > info.dwSize.X) {
    info.dwSize.X = width;

    if (!SetConsoleScreenBufferSize(std_out, info.dwSize)) return FALSE;
    if (!SetConsoleWindowInfo(std_out, TRUE, &info.srWindow)) return FALSE;
  }

  return TRUE;
}

void *GetMappedBuffer(HANDLE file_buffer) {
  return ::MapViewOfFile(file_buffer, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
}

void ReleaseMappedBuffer(void *pBuffer) { ::UnmapViewOfFile(pBuffer); }

BOOL GetScreenBufferLines(int *piLines) {
  CONSOLE_SCREEN_BUFFER_INFO info;
  BOOL rc = GetConsoleScreenBufferInfo(std_out_handle, &info);

  if (rc) *piLines = info.dwSize.Y;

  return rc;
}

BOOL SetScreenBufferLines(short iLines, se::dedicated::ISystem *system) {
  return SetConsoleWidthAndHeight(std_out_handle, 80, iLines, system);
}

BOOL ReadText(LPTSTR pszText, short iBeginLine, short iEndLine) {
  COORD coord{0, iBeginLine};

  DWORD chars_read;
  BOOL rc = ReadConsoleOutputCharacterA(std_out_handle, pszText,
                                        80 * (iEndLine - iBeginLine + 1), coord,
                                        &chars_read);

  // Make sure it's null terminated.
  if (rc) pszText[chars_read] = '\0';

  return rc;
}

short CharToCode(char c) {
  int upper = toupper(c);

  switch (c) {
    case 13:
      return 28;

    default:
      break;
  }

  if (V_isalpha(c)) return static_cast<short>(30 + upper - 65);
  if (V_isdigit(c)) return static_cast<short>(1 + upper - 47);

  return c;
}

BOOL WriteText(LPCTSTR szText) {
  INPUT_RECORD rec = {};
  char *sz = (LPTSTR)szText;

  while (*sz) {
    // 13 is the code for a carriage return (\n) instead of 10.
    if (*sz == 10) *sz = 13;

    auto upper = static_cast<unsigned short>(toupper(*sz));

    rec.EventType = KEY_EVENT;
    rec.Event.KeyEvent.bKeyDown = TRUE;
    rec.Event.KeyEvent.wRepeatCount = 1;
    rec.Event.KeyEvent.wVirtualKeyCode = upper;
    rec.Event.KeyEvent.wVirtualScanCode = CharToCode(*sz);
    rec.Event.KeyEvent.uChar.AsciiChar = *sz;
    rec.Event.KeyEvent.uChar.UnicodeChar = *sz;
    rec.Event.KeyEvent.dwControlKeyState = V_isupper(*sz) ? 0x80 : 0x0;

    DWORD dwWritten;
    WriteConsoleInputA(std_in_handle, &rec, 1, &dwWritten);

    rec.Event.KeyEvent.bKeyDown = FALSE;

    WriteConsoleInputA(std_in_handle, &rec, 1, &dwWritten);

    sz++;
  }

  return TRUE;
}

unsigned __stdcall RemoteConsoleThread(void *ctx) {
  // dimhotepus: Add thread name to aid debugging.
  ThreadSetDebugName("RemoteConRead");

  se::dedicated::ISystem *system = static_cast<se::dedicated::ISystem *>(ctx);
  Assert(system);

  HANDLE events_2_wait[2]{parent_send_event_handle, done_event_handle};

  while (true) {
    const DWORD wait_result{WaitForMultipleObjects(
        std::size(events_2_wait), events_2_wait, FALSE, INFINITE)};

    // heventDone fired, so we're exiting.
    if (wait_result == WAIT_OBJECT_0 + 1) break;

    int *pBuffer = (int *)GetMappedBuffer(file_buffer_handle);
    // hfileBuffer is invalid.  Just leave.
    if (!pBuffer) {
      system->Printf("RemoteConsoleThread: Invalid -HFILE handle.\n");
      break;
    }

    switch (pBuffer[0]) {
      case to_underlying(ConsoleCommand::CCOM_WRITE_TEXT):
        // Param1 : Text
        pBuffer[0] = WriteText((LPCTSTR)(pBuffer + 1));
        break;

      case to_underlying(ConsoleCommand::CCOM_GET_TEXT): {
        // Param1 : Begin line
        // Param2 : End line
        short begin = pBuffer[1];
        short end = pBuffer[2];
        pBuffer[0] = ReadText((LPTSTR)(pBuffer + 1), begin, end);
      } break;

      case to_underlying(ConsoleCommand::CCOM_GET_SCR_LINES):
        // No params
        pBuffer[0] = GetScreenBufferLines(&pBuffer[1]);
        break;

      case to_underlying(ConsoleCommand::CCOM_SET_SCR_LINES):
        // Param1 : Number of lines
        pBuffer[0] = SetScreenBufferLines(pBuffer[1], system);
        break;
    }

    ReleaseMappedBuffer(pBuffer);
    SetEvent(child_send_event_handle);
  }

  _endthreadex(0);
  return 0;
}

}  // namespace

namespace se::dedicated {

void ShutdownRemoteConsole() {
  if (done_event_handle) {
    ::SetEvent(done_event_handle);

    ::WaitForSingleObject(remote_console_thread_handle, INFINITE);
    ::CloseHandle(remote_console_thread_handle);
  }
}

bool StartupRemoteConsole(ICommandLine *command_line, ISystem *system) {
  HANDLE file_handle = nullptr;
  const char *p;
  // give external front ends a chance to hook into the console
  if (command_line->CheckParm("-HFILE", &p) && p) {
    // dimhotepus: x86-64 port
    file_handle = (HANDLE) static_cast<uintp>(strtoull(p, nullptr, 10));
  }

  HANDLE parent_event_handle = nullptr;
  if (command_line->CheckParm("-HPARENT", &p) && p) {
    // dimhotepus: x86-64 port
    parent_event_handle = (HANDLE) static_cast<uintp>(strtoull(p, nullptr, 10));
  }

  HANDLE child_event_handle = nullptr;
  if (command_line->CheckParm("-HCHILD", &p) && p) {
    // dimhotepus: x86-64 port
    child_event_handle = (HANDLE) static_cast<uintp>(strtoull(p, nullptr, 10));
  }

  // ignore if we don't have all the events.
  if (!file_handle || !parent_event_handle || !child_event_handle) {
    system->Printf(
        "\n\nStartupRemoteConsole: No external front end present.\n");
    return true;
  }

  system->Printf("StartupRemoteConsole: Setting up external control.\n");

  file_buffer_handle = file_handle;
  parent_send_event_handle = parent_event_handle;
  child_send_event_handle = child_event_handle;

  // So we'll know when to go away.
  done_event_handle = CreateEventA(nullptr, FALSE, FALSE, nullptr);
  if (!done_event_handle) {
    system->Printf("StartupRemoteConsole: Couldn't create done event: %s.\n",
                   GetSystemError().message().c_str());
    return false;
  }

  unsigned threadAddr;
  if (!(remote_console_thread_handle = reinterpret_cast<HANDLE>(_beginthreadex(
            nullptr, 0, RemoteConsoleThread, system, 0, &threadAddr)))) {
    CloseHandle(done_event_handle);
    system->Printf(
        "StartupRemoteConsole: Couldn't create third party thread: %s.\n",
        GetGenericError().message().c_str());
    return false;
  }

  // save off the input/output handles.
  std_out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  std_in_handle = GetStdHandle(STD_INPUT_HANDLE);

  short want_height{50};
  if (command_line->CheckParm("-conheight", &p) && p) {
    want_height = atoi(p);
  }

  // Force 80 character width, at least 50 character height
  SetConsoleWidthAndHeight(std_out_handle, 80, want_height, system);
  return true;
}

}  // namespace se::dedicated

#endif  // _WIN32
