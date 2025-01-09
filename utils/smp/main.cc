// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"

#include "steam_wmp_host.h"

#include <comdef.h>
#include <commctrl.h>
#include <initguid.h>

#include <vector>
#include <string>
#include <strstream>
#include <fstream>

#include "IceKey.h"

CComModule atl_com;

BEGIN_OBJECT_MAP(ObjectMap)
END_OBJECT_MAP()

#define ID_SKIP_FADE_TIMER 1
#define ID_DRAW_TIMER 2

constexpr inline double FADE_TIME{1.0};
constexpr inline int MAX_BLUR_STEPS{100};

HINSTANCE g_hInstance;
HWND g_hBlackFadingWindow = nullptr;
bool g_bFadeIn = true;
bool g_bFrameCreated = false;
SteamWmpHost g_frame;
SteamWmpHost *g_pFrame = nullptr;
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

void LogPlayerEvent(EventType_t e);

DWORD g_dwUseVMROverlayOldValue = 0;
bool g_bUseVMROverlayValueExists = false;

void SetRegistryValue(const char *pKeyName, const char *value_name, DWORD value,
                      DWORD &old_value, bool &value_exists) {
  HKEY key{nullptr};
  LONG rc{RegCreateKeyEx(HKEY_CURRENT_USER, pKeyName, 0, nullptr,
                         REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &key,
                         nullptr)};
  if (rc != ERROR_SUCCESS) {
    FailedUI(HRESULT_FROM_WIN32(GetLastError()),
             "Unable to open registry key: ");
    OutputDebugString(pKeyName);
    OutputDebugString("\n");
    return;
  }

  DWORD type{0};
  DWORD size{sizeof(old_value)};

  // amusingly enough, if pValueName doesn't exist, RegQueryValueEx returns
  // ERROR_FILE_NOT_FOUND
  rc = RegQueryValueEx(key, value_name, nullptr, &type, (LPBYTE)&old_value,
                       &size);

  value_exists = rc == ERROR_SUCCESS;

  rc = RegSetValueEx(key, value_name, 0, REG_DWORD, (CONST BYTE *)&value,
                     sizeof(value));
  if (rc != ERROR_SUCCESS) {
    FailedUI(HRESULT_FROM_WIN32(GetLastError()),
             "Unable to write registry value ");
    OutputDebugString(value_name);
    OutputDebugString(" in key ");
    OutputDebugString(pKeyName);
    OutputDebugString("\n");
  }

  RegCloseKey(key);
}

void RestoreRegistryValue(const char *pKeyName, const char *pValueName,
                          DWORD dwOldValue, bool bValueExisted) {
  HKEY key{nullptr};
  LONG rc{RegCreateKeyEx(HKEY_CURRENT_USER, pKeyName, 0, nullptr,
                         REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &key,
                         nullptr)};
  if (rc != ERROR_SUCCESS) {
    FailedUI(HRESULT_FROM_WIN32(GetLastError()),
             "Unable to open registry key: ");
    OutputDebugString(pKeyName);
    OutputDebugString("\n");
    return;
  }

  if (bValueExisted) {
    rc = RegSetValueEx(key, pValueName, 0, REG_DWORD, (CONST BYTE *)&dwOldValue,
                       sizeof(dwOldValue));
  } else {
    rc = RegDeleteValue(key, pValueName);
  }

  if (rc != ERROR_SUCCESS) {
    FailedUI(HRESULT_FROM_WIN32(GetLastError()), "SetRegistryValue w/e: ");
  }

  RegCloseKey(key);
}

bool GetRegistryString(const char *pKeyName, const char *pValueName,
                       const char *pValueString, int nValueLen) {
  HKEY key{nullptr};
  LONG rc{RegCreateKeyEx(HKEY_CURRENT_USER, pKeyName, 0, nullptr,
                         REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &key,
                         nullptr)};
  if (rc != ERROR_SUCCESS) {
    FailedUI(HRESULT_FROM_WIN32(GetLastError()),
             "Unable to open registry key: ");
    OutputDebugString(pKeyName);
    OutputDebugString("\n");
    return false;
  }

  DWORD dwType = 0;
  rc = RegQueryValueEx(key, pValueName, nullptr, &dwType, (LPBYTE)pValueString,
                       (DWORD *)&nValueLen);

  RegCloseKey(key);

  if (rc != ERROR_SUCCESS || dwType != REG_SZ) {
    FailedUI(HRESULT_FROM_WIN32(GetLastError()),
             "Unable to read registry string: ");
    OutputDebugString(pValueName);
    OutputDebugString("\n");
    return false;
  }

  return true;
}

struct EventData_t {
  EventData_t(ULONGLONG t, double pos, EventType_t e)
      : time(t), position(pos), event(e) {}

  ULONGLONG time;     // real time
  double position;    // movie position
  EventType_t event;  // event type
};

const char *GetEventName(EventType_t event) {
  switch (event) {
    case EventType_t::ET_APPLAUNCH:
      return "App Launch";
    case EventType_t::ET_APPEXIT:
      return "App Exit";
    case EventType_t::ET_CLOSE:
      return "Close";
    case EventType_t::ET_FADEOUT:
      return "Fade Out";

    case EventType_t::ET_MEDIABEGIN:
      return "Media Begin";
    case EventType_t::ET_MEDIAEND:
      return "Media End";

    case EventType_t::ET_JUMPHOME:
      return "Jump Home";
    case EventType_t::ET_JUMPEND:
      return "Jump End";

    case EventType_t::ET_BUFFERING:
      return "Buffering";
    case EventType_t::ET_WAITING:
      return "Waiting";
    case EventType_t::ET_TRANSITIONING:
      return "Transitioning";
    case EventType_t::ET_READY:
      return "Ready";
    case EventType_t::ET_RECONNECTING:
      return "Reconnecting";

    case EventType_t::ET_PLAY:
      return "Play";
    case EventType_t::ET_PAUSE:
      return "Pause";
    case EventType_t::ET_STOP:
      return "Stop";
    case EventType_t::ET_SCRUBFROM:
      return "Scrub From";
    case EventType_t::ET_SCRUBTO:
      return "Scrub To";
    case EventType_t::ET_STEPFWD:
      return "Step Forward";
    case EventType_t::ET_STEPBCK:
      return "Step Back";
    case EventType_t::ET_JUMPFWD:
      return "Jump Forward";
    case EventType_t::ET_JUMPBCK:
      return "Jump Back";
    case EventType_t::ET_REPEAT:
      return "Repeat";

    case EventType_t::ET_MAXIMIZE:
      return "Maximize Window";
    case EventType_t::ET_MINIMIZE:
      return "Minimize Window";
    case EventType_t::ET_RESTORE:
      return "Restore Window";

    case EventType_t::ET_ERROR:
      return "Dispatcher Error";

    default:
      return "<unknown>";
  }
}

std::vector<EventData_t> g_events;

void LogPlayerEvent(EventType_t e, double pos) {
  static ULONGLONG first_tick_ms{GetTickCount64()};
  ULONGLONG time_ms{GetTickCount64() - first_tick_ms};

  char msg[256];
  sprintf_s(msg, "[log] Event '%s' at time %.3fs and pos %d.\n",
            GetEventName(e), time_ms / 1000.f, int(1000 * pos));
  OutputDebugString(msg);

  bool should_drop_event = false;

  const size_t events_count = g_events.size();
  if ((e == EventType_t::ET_STEPFWD || e == EventType_t::ET_STEPBCK) &&
      events_count >= 2) {
    const EventData_t &e1 = g_events[events_count - 1];
    const EventData_t &e2 = g_events[events_count - 2];

    if ((e1.event == e || e1.event == EventType_t::ET_REPEAT) &&
        e2.event == e) {
      // only store starting and ending stepfwd or stepbck events, since there
      // can be so many also keep events that are more than a second apart
      if (e1.event == EventType_t::ET_REPEAT) {
        // keep dropping events while e1 isn't before a gap
        should_drop_event = time_ms - e1.time < 1000;
      } else {
        // e2 was kept last time, so keep e1 if e2 was kept because it was
        // before a gap
        should_drop_event = e1.time - e2.time < 1000;
      }
    }
  }

  if (should_drop_event) {
    g_events[events_count - 1] =
        EventData_t{time_ms, pos, EventType_t::ET_REPEAT};
  } else {
    g_events.emplace_back(time_ms, pos, e);
  }
}

void PrintStats(const char *file_path, const std::string &url) {
  std::ofstream os(file_path, std::ios_base::out | std::ios_base::binary);

  size_t offset = url.find_last_of("/\\");
  if (offset == url.npos) offset = 0;

  // filename
  os << url.substr(offset + 1) << '\n';

  // number of events
  os << g_events.size() << "\n";

  // event data (tab-delimited)
  for (auto &e : g_events) {
    os << GetEventName(e.event) << "\t" << e.time << "\t"
       << int(1000 * e.position) << "\n";
  }
}

void RestoreRegistry() {
  RestoreRegistryValue(
      "Software\\Microsoft\\MediaPlayer\\Preferences\\VideoSettings",
      "UseVMROverlay", g_dwUseVMROverlayOldValue, g_bUseVMROverlayValueExists);
}

int g_nTimingIndex = 0;
double g_timings[65536];

LRESULT CALLBACK SnmpWindowProc(HWND hWnd, UINT msg, WPARAM wparam,
                                LPARAM lparam) {
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

        g_timings[g_nTimingIndex++] =
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

          RECT rcPos = {CW_USEDEFAULT, 0, 0, 0};

          g_pFrame->Create(::GetDesktopWindow(), rcPos,
                           _T( "Steam Media Player" ),
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

bool ShowFadeWindow(bool bShow) {
  if (bShow) {
    g_timeAtFadeStart = 0.0;
    g_bFadeIn = false;

    ::SetTimer(g_hBlackFadingWindow, ID_DRAW_TIMER, 10, nullptr);

    if (g_pFrame) g_pFrame->ShowWindow(SW_HIDE);

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

HWND CreateFullscreenWindow(bool bFadeIn, const std::string &url) {
  if (g_hBlackFadingWindow) return g_hBlackFadingWindow;

  static bool s_bRegistered = false;

  constexpr TCHAR wnd_class_name[] = "Valve_SMP_WndClass";

  if (!s_bRegistered) {
    WNDCLASS wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = SnmpWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = g_hInstance;
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
  if (!GetMonitorInfo(g_hMonitor, &mi)) {
    GetClientRect(::GetDesktopWindow(), &mi.rcMonitor);
  }

  char title[MAX_PATH * 2];
  sprintf_s(title, "Steam Media Player - %s", url.c_str());

  constexpr DWORD windowStyle =
      WS_POPUP | WS_MAXIMIZE | WS_EX_TOPMOST | WS_VISIBLE;

  g_hBlackFadingWindow =
      CreateWindow(wnd_class_name, title, windowStyle, mi.rcMonitor.left,
                   mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                   mi.rcMonitor.bottom - mi.rcMonitor.top, nullptr, nullptr,
                   g_hInstance, nullptr);
  ShowWindow(g_hBlackFadingWindow, SW_SHOWMAXIMIZED);

  while (ShowCursor(FALSE) >= 0);

  return g_hBlackFadingWindow;
}

bool CreateDesktopBitmaps() {
  MONITORINFOEX mi = {};
  mi.cbSize = sizeof(mi);
  if (!GetMonitorInfo(g_hMonitor, &mi)) return false;

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

  SelectObject(g_hdcCapture, old_capture);
  SelectObject(g_hdcBlend, old_blend);

  return rc;
}

std::vector<std::string> ParseCommandLine(const char *cmdline) {
  std::vector<std::string> params{""};

  bool quoted = false;
  for (const char *cp = cmdline; *cp; ++cp) {
    if (*cp == '\"') {
      quoted = !quoted;
    } else if (isspace(*cp) && !quoted) {
      if (!params.back().empty()) params.emplace_back("");
    } else {
      params.back().push_back(*cp);
    }
  }

  if (params.back().empty()) params.pop_back();

  return params;
}

int WINAPI WinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE,
                   _In_ LPSTR cmd_line, _In_ int) {
  if (*cmd_line == '\0') {
    FailedUI(HRESULT_FROM_WIN32(ERROR_INVALID_COMMAND_LINE),
             "Sorry, you need to pass media file to play.\n");
    return ERROR_INVALID_COMMAND_LINE;
  }

  g_hInstance = instance;

  std::vector<std::string> params = ParseCommandLine(cmd_line);
  for (auto &p : params) {
    if (p[0] != '-' && p[0] != '/') g_URL = p;
  }

  SetRegistryValue(
      "Software\\Microsoft\\MediaPlayer\\Preferences\\VideoSettings",
      "UseVMROverlay", 0, g_dwUseVMROverlayOldValue,
      g_bUseVMROverlayValueExists);
  atexit(RestoreRegistry);

  LogPlayerEvent(EventType_t::ET_APPLAUNCH, 0.0);

  cmd_line = GetCommandLine();  // this line necessary for _ATL_MIN_CRT

  if (FailedUI(
          CoInitializeEx(0, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE |
                                COINIT_SPEED_OVER_MEMORY),
          "Failed to initialize COM.")) {
    return 1;
  }

  if (FailedUI(atl_com.Init(ObjectMap, instance, &LIBID_ATLLib),
               "Failed to initialize COM module.")) {
    return 2;
  }

  POINT pt;
  GetCursorPos(&pt);
  g_hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);

  if (!CreateDesktopBitmaps()) {
    OutputDebugString("[log] CreateDesktopBitmaps FAILED!\n");
  }

  if (!CreateFullscreenWindow(true, g_URL)) {
    MessageBox(nullptr, "Unable to create Steam Media Player window.\n",
               "Steam Media Player - Error", MB_OK | MB_ICONERROR);
    return 3;
  }

  ShowCursor(FALSE);

  MSG msg = {};
  while (GetMessage(&msg, 0, 0, 0)) {
    TranslateMessage(&msg);

    DispatchMessage(&msg);
  }

  ShowCursor(TRUE);

  LogPlayerEvent(EventType_t::ET_APPEXIT);

  atl_com.Term();
  CoUninitialize();

  return static_cast<int>(msg.wParam);
}
