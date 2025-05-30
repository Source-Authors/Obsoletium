// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"

#include "steam_wmp_host.h"
#include "steam_wmp_event.h"

#include <comdef.h>
#include <commctrl.h>
#include <initguid.h>

#include <vector>
#include <string>
#include <strstream>
#include <fstream>

BEGIN_OBJECT_MAP(ObjectMap)
END_OBJECT_MAP()

namespace {

bool g_bFadeIn = true;

HDC g_hdcCapture = nullptr;
HDC g_hdcBlend = nullptr;
HBITMAP g_hbmCapture = nullptr;
HBITMAP g_hbmBlend = nullptr;
HMONITOR g_hMonitor = nullptr;

int g_screenWidth = 0;
int g_screenHeight = 0;

std::string g_URL;

double g_timeAtFadeStart = 0.0;
int g_nBlurSteps = 0;

constexpr inline DWORD ID_SKIP_FADE_TIMER = 1;
constexpr inline DWORD ID_DRAW_TIMER = 2;

constexpr inline double FADE_TIME{1.0};
constexpr inline int MAX_BLUR_STEPS{100};

CComModule atl_com;

size_t g_nTimingIndex = 0;
double g_timings[65536];

const char *GetEventName(se::smp::SmpPlayerEvent event) {
  using namespace se::smp;

  switch (event) {
    case SmpPlayerEvent::AppLaunch:
      return "App Launch";
    case SmpPlayerEvent::AppExit:
      return "App Exit";
    case SmpPlayerEvent::AppClose:
      return "Close";
    case SmpPlayerEvent::FadeOut:
      return "Fade Out";

    case SmpPlayerEvent::BeginMedia:
      return "Media Begin";
    case SmpPlayerEvent::EndMedia:
      return "Media End";

    case SmpPlayerEvent::Jump2Home:
      return "Jump Home";
    case SmpPlayerEvent::Jump2End:
      return "Jump End";

    case SmpPlayerEvent::Buffering:
      return "Buffering";
    case SmpPlayerEvent::Waiting:
      return "Waiting";
    case SmpPlayerEvent::Transitioning:
      return "Transitioning";
    case SmpPlayerEvent::Ready:
      return "Ready";
    case SmpPlayerEvent::Reconnecting:
      return "Reconnecting";

    case SmpPlayerEvent::Play:
      return "Play";
    case SmpPlayerEvent::Pause:
      return "Pause";
    case SmpPlayerEvent::Stop:
      return "Stop";
    case SmpPlayerEvent::ScrubFrom:
      return "Scrub From";
    case SmpPlayerEvent::ScrubTo:
      return "Scrub To";
    case SmpPlayerEvent::StepForward:
      return "Step Forward";
    case SmpPlayerEvent::StepBack:
      return "Step Back";
    case SmpPlayerEvent::JumpFroward:
      return "Jump Forward";
    case SmpPlayerEvent::JumpBack:
      return "Jump Back";
    case SmpPlayerEvent::Repeat:
      return "Repeat";

    case SmpPlayerEvent::Maximize:
      return "Maximize Window";
    case SmpPlayerEvent::Minimize:
      return "Minimize Window";
    case SmpPlayerEvent::Restore:
      return "Restore Window";

    case SmpPlayerEvent::Error:
      return "Dispatcher Error";

    default:
      return "<unknown>";
  }
}

}  // namespace

namespace se::smp {

HWND g_hBlackFadingWindow = nullptr;
bool g_bFrameCreated = false;
SteamWmpHost g_frame;
SteamWmpHost *g_pFrame = nullptr;

void LogPlayerEvent(SmpPlayerEvent e);

void LogPlayerEvent(SmpPlayerEvent e, double pos) {
  static ULONGLONG first_tick_ms{GetTickCount64()};
  ULONGLONG time_ms{GetTickCount64() - first_tick_ms};

  char msg[256];
  sprintf_s(msg, "[log] Event '%s' at time %.3fs and pos %f.\n",
            GetEventName(e), time_ms / 1000.f, 1000 * pos);
  OutputDebugString(msg);
}

bool ShowFadeWindow(bool bShow) {
  if (bShow) {
    g_timeAtFadeStart = 0.0;
    g_bFadeIn = false;

    ::SetTimer(g_hBlackFadingWindow, ID_DRAW_TIMER, 10, nullptr);

    if (se::smp::g_pFrame) se::smp::g_pFrame->ShowWindow(SW_HIDE);

    ::SetWindowPos(g_hBlackFadingWindow, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    ::InvalidateRect(g_hBlackFadingWindow, nullptr, TRUE);
  } else {
    ::SetWindowPos(g_hBlackFadingWindow, HWND_BOTTOM, 0, 0, 0, 0,
                   SWP_NOREDRAW | SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW |
                       SWP_NOACTIVATE | SWP_DEFERERASE);
  }

  return true;
}

}  // namespace se::smp

namespace {

LRESULT CALLBACK SnmpWindowProc(HWND hWnd, UINT msg, WPARAM wparam,
                                LPARAM lparam) {
  using namespace se::smp;

  switch (msg) {
    case WM_CREATE: {
      g_timeAtFadeStart = 0.0;
      g_nBlurSteps = 0;

      g_hBlackFadingWindow = hWnd;

      MONITORINFO mi = {};
      mi.cbSize = sizeof(mi);
      if (GetMonitorInfo(g_hMonitor, &mi)) {
        SetWindowPos(hWnd, HWND_TOPMOST, mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, 0);
      }

      g_frame.SetUrl(g_URL);

      if (!g_bFadeIn) {
        g_pFrame->ShowWindow(SW_HIDE);
        g_pFrame->PostMessage(WM_DESTROY, 0, 0);
      }

      InvalidateRect(hWnd, nullptr, TRUE);

      // if the fade doesn't start in 1 second, then just jump to the video
      SetTimer(hWnd, ID_SKIP_FADE_TIMER, 1000, nullptr);
      SetTimer(hWnd, ID_DRAW_TIMER, 10, nullptr);  // draw timer
    } break;

    case WM_TIMER:
      if (wparam == ID_DRAW_TIMER) {
        static LARGE_INTEGER s_nPerformanceFrequency;
        if (g_timeAtFadeStart == 0.0) {
          LARGE_INTEGER nTimeAtFadeStart;
          QueryPerformanceCounter(&nTimeAtFadeStart);
          QueryPerformanceFrequency(&s_nPerformanceFrequency);

          g_timeAtFadeStart = nTimeAtFadeStart.QuadPart /
                              double(s_nPerformanceFrequency.QuadPart);

          KillTimer(hWnd, ID_SKIP_FADE_TIMER);
          // restart skip fade timer and give it an extra 100ms to allow the
          // fade to draw fully black once
          SetTimer(hWnd, ID_SKIP_FADE_TIMER, 100 + UINT(FADE_TIME * 1000),
                   nullptr);
        }

        LARGE_INTEGER time;
        QueryPerformanceCounter(&time);

        g_timings[g_nTimingIndex++ % ARRAYSIZE(g_timings)] =
            time.QuadPart / double(s_nPerformanceFrequency.QuadPart);
        double dt = time.QuadPart / double(s_nPerformanceFrequency.QuadPart -
                                           g_timeAtFadeStart);

        bool is_fade_finished = dt >= FADE_TIME;
        double fraction = is_fade_finished ? 1.0 : dt / FADE_TIME;

        bool is_blur =
            g_bFadeIn && (int(fraction * MAX_BLUR_STEPS) > g_nBlurSteps);
        if (is_blur) ++g_nBlurSteps;

        BYTE fade = BYTE(fraction * 255.999);
        if (g_bFadeIn) fade = 255 - fade;

        if (!PatBlt(g_hdcBlend, 0, 0, g_screenWidth, g_screenHeight,
                    BLACKNESS)) {
          FailedUI(HRESULT_FROM_WIN32(GetLastError()), "PatBlt w/e: ");
        }

        BLENDFUNCTION blendfunc = {AC_SRC_OVER, 0, fade, 0};
        if (!::AlphaBlend(g_hdcBlend, 0, 0, g_screenWidth, g_screenHeight,
                          g_hdcCapture, 0, 0, g_screenWidth, g_screenHeight,
                          blendfunc)) {
          FailedUI(HRESULT_FROM_WIN32(GetLastError()), "AlphaBlend w/e: ");
        }

        if (!BitBlt((HDC)wparam, 0, 0, g_screenWidth, g_screenHeight,
                    g_hdcBlend, 0, 0, SRCCOPY)) {
          FailedUI(HRESULT_FROM_WIN32(GetLastError()), "BitBlt w/e: ");
        }

        if (!is_fade_finished) break;
      }

      //	case WM_TIMER:
      {
        KillTimer(hWnd, ID_SKIP_FADE_TIMER);
        KillTimer(hWnd, ID_DRAW_TIMER);

        if (!g_bFadeIn) {
          ShowWindow(hWnd, SW_HIDE);
          PostMessage(hWnd, WM_CLOSE, 0, 0);
          return 1;
        }

        if (!g_bFrameCreated) {
          g_bFrameCreated = true;
          g_pFrame = &g_frame;
          g_pFrame->GetWndClassInfo().m_wc.hIcon = LoadIcon(
              atl_com.GetResourceInstance(), MAKEINTRESOURCE(SRC_IDI_APP_MAIN));

          RECT rect = {CW_USEDEFAULT, 0, 0, 0};

          char title[MAX_PATH * 2];
          sprintf_s(title, "Steam Media Player - %s", g_URL.c_str());

          g_pFrame->Create(::GetDesktopWindow(), rect, title,
                           WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, (UINT)0);
          g_pFrame->ShowWindow(SW_SHOW);
        }

        // close WMP window once we paint the fullscreen fade window
        if (!g_bFadeIn) g_pFrame->ShowWindow(SW_HIDE);
      }
      return 1;

    case WM_KEYDOWN:
      if (wparam == VK_ESCAPE) ::DestroyWindow(hWnd);
      break;

    case WM_DESTROY:
      g_hBlackFadingWindow = nullptr;

      if (g_bFrameCreated) {
        g_bFrameCreated = false;

        g_pFrame->DestroyWindow();
        g_pFrame = nullptr;
      }

      ::PostQuitMessage(0);
      break;
  }

  return DefWindowProc(hWnd, msg, wparam, lparam);
}

HWND CreateFullscreenWindow(HINSTANCE instance, HMONITOR monitor, bool bFadeIn,
                            const std::string &url) {
  if (se::smp::g_hBlackFadingWindow) return se::smp::g_hBlackFadingWindow;

  static bool s_bRegistered = false;

  constexpr TCHAR wnd_class_name[] = "Valve_SMP_WndClass";

  if (!s_bRegistered) {
    WNDCLASS wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = SnmpWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = instance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = nullptr;
    wc.hbrBackground = nullptr;
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = wnd_class_name;
    if (!RegisterClass(&wc)) return nullptr;

    s_bRegistered = true;
  }

  g_bFadeIn = bFadeIn;

  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  if (!GetMonitorInfo(monitor, &mi)) {
    GetClientRect(::GetDesktopWindow(), &mi.rcMonitor);
  }

  char title[MAX_PATH * 2];
  sprintf_s(title, "Steam Media Player - %s", url.c_str());

  constexpr DWORD windowStyle =
      WS_POPUP | WS_MAXIMIZE | WS_EX_TOPMOST | WS_VISIBLE;

  se::smp::g_hBlackFadingWindow =
      CreateWindow(wnd_class_name, title, windowStyle, mi.rcMonitor.left,
                   mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                   mi.rcMonitor.bottom - mi.rcMonitor.top, nullptr, nullptr,
                   instance, nullptr);
  ShowWindow(se::smp::g_hBlackFadingWindow, SW_SHOWMAXIMIZED);

  while (ShowCursor(FALSE) >= 0);

  return se::smp::g_hBlackFadingWindow;
}

bool CreateDesktopBitmaps(HMONITOR monitor) {
  MONITORINFOEX mi = {};
  mi.cbSize = sizeof(mi);
  if (!GetMonitorInfo(monitor, &mi)) return false;

  g_screenWidth = mi.rcMonitor.right - mi.rcMonitor.left;
  g_screenHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;

  HDC dc_screen = CreateDC(mi.szDevice, mi.szDevice, nullptr, nullptr);
  if (!dc_screen) return false;

  g_hdcCapture = CreateCompatibleDC(dc_screen);
  g_hdcBlend = CreateCompatibleDC(dc_screen);

  if (!g_hdcCapture || !g_hdcBlend) return false;

  if ((GetDeviceCaps(dc_screen, SHADEBLENDCAPS) & SB_CONST_ALPHA) == 0) {
    OutputDebugString("[log] Display doesn't support AlphaBlend!\n");
  }

  if ((GetDeviceCaps(dc_screen, RASTERCAPS) & RC_BITBLT) == 0) {
    OutputDebugString("[log] Display doesn't support BitBlt!\n");
  }

  if (GetDeviceCaps(dc_screen, BITSPIXEL) < 32) {
    OutputDebugString("[log] Display doesn't support 32bpp!\n");
  }

  if (g_screenWidth != GetDeviceCaps(dc_screen, HORZRES) ||
      g_screenHeight != GetDeviceCaps(dc_screen, VERTRES)) {
    OutputDebugString("[log] Screen DC size differs from monitor size!\n");
  }

  g_hbmCapture =
      CreateCompatibleBitmap(dc_screen, g_screenWidth, g_screenHeight);
  g_hbmBlend = CreateCompatibleBitmap(dc_screen, g_screenWidth, g_screenHeight);
  if (!g_hbmCapture || !g_hbmBlend) return false;

  HGDIOBJ old_capture = SelectObject(g_hdcCapture, g_hbmCapture);
  HGDIOBJ old_blend = SelectObject(g_hdcBlend, g_hbmBlend);

  const bool rc = !!BitBlt(g_hdcCapture, 0, 0, g_screenWidth, g_screenHeight,
                           dc_screen, 0, 0, SRCCOPY);

  SelectObject(g_hdcBlend, old_blend);
  SelectObject(g_hdcCapture, old_capture);

  return rc;
}

std::vector<std::string> ParseCommandLine(const char *cmd_line) {
  std::vector<std::string> args{""};

  bool quoted{false};
  for (const auto *cp = cmd_line; *cp; ++cp) {
    if (*cp == '\"') {
      quoted = !quoted;
    } else if (isspace(*cp) && !quoted) {
      if (!args.back().empty()) args.emplace_back("");
    } else {
      args.back().push_back(*cp);
    }
  }

  if (args.back().empty()) args.pop_back();

  return args;
}

class ScopedShowCursor {
 public:
  /**
   * @brief Creates scoped cursor.
   * @param should_show Show cursor or not?
   */
  explicit ScopedShowCursor(bool should_show) noexcept
      : should_show_{should_show} {
    ::ShowCursor(should_show ? TRUE : FALSE);
  }

  ScopedShowCursor(ScopedShowCursor &) = delete;
  ScopedShowCursor(ScopedShowCursor &&) = delete;
  ScopedShowCursor &operator=(ScopedShowCursor &) = delete;
  ScopedShowCursor &operator=(ScopedShowCursor &&) = delete;

  ~ScopedShowCursor() noexcept { ::ShowCursor(should_show_ ? FALSE : TRUE); }

 private:
  const bool should_show_;
};

class ScopedRegistryValue {
 public:
  ScopedRegistryValue(HKEY root, const char *key_name, const char *value_name,
                      DWORD value) noexcept
      : root_{root}, key_name_{key_name}, value_name_{value_name} {
    rc_ = SetRegistryValue(root, key_name, value_name, value, old_value_,
                           value_exists_);
  }

  ScopedRegistryValue(ScopedRegistryValue &) = delete;
  ScopedRegistryValue(ScopedRegistryValue &&) = delete;
  ScopedRegistryValue &operator=(ScopedRegistryValue &) = delete;
  ScopedRegistryValue &operator=(ScopedRegistryValue &&) = delete;

  ~ScopedRegistryValue() noexcept {
    if (rc_ == ERROR_SUCCESS) {
      RestoreRegistryValue(root_, key_name_, value_name_, old_value_,
                           value_exists_);
    }
  }

 private:
  static LONG SetRegistryValue(HKEY root, const char *key_name,
                               const char *value_name, DWORD value,
                               DWORD &old_value, bool &value_exists) {
    HKEY key{nullptr};
    LONG rc{RegCreateKeyEx(root, key_name, 0, nullptr, REG_OPTION_NON_VOLATILE,
                           KEY_ALL_ACCESS, nullptr, &key, nullptr)};
    if (rc != ERROR_SUCCESS) {
      se::smp::FailedUI(HRESULT_FROM_WIN32(GetLastError()),
                        "Unable to open registry key: ");
      OutputDebugString(key_name);
      OutputDebugString("\n");
      return rc;
    }

    DWORD type{0};
    DWORD size{sizeof(old_value)};

    // amusingly enough, if value_name doesn't exist, RegQueryValueEx returns
    // ERROR_FILE_NOT_FOUND
    rc = RegQueryValueEx(key, value_name, nullptr, &type, (LPBYTE)&old_value,
                         &size);

    value_exists = rc == ERROR_SUCCESS;

    rc = RegSetValueEx(key, value_name, 0, REG_DWORD, (CONST BYTE *)&value,
                       sizeof(value));
    if (rc != ERROR_SUCCESS) {
      se::smp::FailedUI(HRESULT_FROM_WIN32(GetLastError()),
                        "Unable to write registry value: ");

      OutputDebugString(value_name);
      OutputDebugString(" in key ");
      OutputDebugString(key_name);
      OutputDebugString("\n");
    }

    if (RegCloseKey(key) != ERROR_SUCCESS) {
      se::smp::FailedUI(HRESULT_FROM_WIN32(GetLastError()),
                        "Unable to close registry key: ");
    }

    return rc;
  }

  static LONG RestoreRegistryValue(HKEY root, const char *key_name,
                                   const char *value_name, DWORD old_value,
                                   bool value_exists) {
    HKEY key{nullptr};
    LONG rc{RegCreateKeyEx(root, key_name, 0, nullptr, REG_OPTION_NON_VOLATILE,
                           KEY_ALL_ACCESS, nullptr, &key, nullptr)};
    if (rc != ERROR_SUCCESS) {
      se::smp::FailedUI(HRESULT_FROM_WIN32(GetLastError()),
                        "Unable to open registry key: ");

      OutputDebugString(key_name);
      OutputDebugString("\n");
      return rc;
    }

    if (value_exists) {
      rc = RegSetValueEx(key, value_name, 0, REG_DWORD,
                         (CONST BYTE *)&old_value, sizeof(old_value));
      if (rc != ERROR_SUCCESS) {
        se::smp::FailedUI(HRESULT_FROM_WIN32(GetLastError()),
                          "RegSetValueEx w/e: ");
      }
    } else {
      rc = RegDeleteValue(key, value_name);
      if (rc != ERROR_SUCCESS) {
        se::smp::FailedUI(HRESULT_FROM_WIN32(GetLastError()),
                          "RegDeleteValue w/e: ");
      }
    }

    if (RegCloseKey(key) != ERROR_SUCCESS) {
      se::smp::FailedUI(HRESULT_FROM_WIN32(GetLastError()),
                        "Unable to close registry key: ");
    }

    return rc;
  }

  HKEY root_;
  const char *key_name_;
  const char *value_name_;

  DWORD old_value_;
  bool value_exists_;

  LONG rc_;
};

}  // namespace

int WINAPI WinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE,
                   _In_ LPSTR cmd_line, _In_ int) {
  using namespace se::smp;

  if (*cmd_line == '\0') {
    FailedUI(HRESULT_FROM_WIN32(ERROR_INVALID_COMMAND_LINE),
             "Sorry, there is no media to play.\n\nPlease pass "
             "enclosed in double quotes media file path or URL.");
    return ERROR_INVALID_COMMAND_LINE;
  }

  std::vector<std::string> args{ParseCommandLine(cmd_line)};
  if (args.size() != 1) {
    FailedUI(HRESULT_FROM_WIN32(ERROR_INVALID_COMMAND_LINE),
             "Sorry, there is more than one media to play.\n\nPlease pass "
             "enclosed in double quotes media file path or URL.");
    return ERROR_INVALID_COMMAND_LINE;
  }

  LogPlayerEvent(SmpPlayerEvent::AppLaunch, 0.0);

  const ScopedRegistryValue scoped_registry_value{
      HKEY_CURRENT_USER,
      "Software\\Microsoft\\MediaPlayer\\Preferences\\VideoSettings",
      "UseVMROverlay", 0};

  g_URL = args[0];
  cmd_line = GetCommandLine();  // this line necessary for _ATL_MIN_CRT

  if (FailedUI(
          CoInitializeEx(0, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE |
                                COINIT_SPEED_OVER_MEMORY),
          "Sorry, Component Object Model library failed to "
          "initialize.\n\nPlease, contact the publisher for further help.")) {
    return 1;
  }

  atexit(CoUninitialize);

  if (FailedUI(
          atl_com.Init(ObjectMap, instance, &LIBID_ATLLib),
          "Sorry, Component Object Model module failed to "
          "initialize.\n\nPlease, contact the publisher for further help.")) {
    return 2;
  }

  POINT pt;
  GetCursorPos(&pt);

  HMONITOR monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
  assert(monitor);

  if (!CreateDesktopBitmaps(monitor)) {
    OutputDebugString("[log] CreateDesktopBitmaps FAILED!\n");
  }

  g_hMonitor = monitor;

  if (!CreateFullscreenWindow(instance, monitor, true, g_URL)) {
    MessageBox(nullptr, "Unable to create Steam Media Player window.\n",
               "Steam Media Player - Error", MB_OK | MB_ICONERROR);
    return 3;
  }

  MSG msg = {};
  {
    const ScopedShowCursor scoped_show_cursor{false};

    while (GetMessage(&msg, 0, 0, 0)) {
      TranslateMessage(&msg);

      DispatchMessage(&msg);
    }
  }

  LogPlayerEvent(SmpPlayerEvent::AppExit);

  atl_com.Term();

  return static_cast<int>(msg.wParam);
}
