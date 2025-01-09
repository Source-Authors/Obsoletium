// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"

#include "steam_wmp_host.h"

#include <comdef.h>

#include <cassert>
#include <string>

extern HWND g_hBlackFadingWindow;
extern SteamWmpHost* g_pFrame;
extern bool g_bFrameCreated;
extern double g_timeAtFadeStart;
extern bool g_bFadeIn;
extern bool g_bFadeWindowTriggered;

bool ShowFadeWindow(bool bShow);
void LogPlayerEvent(EventType_t e, double pos);

CComPtr<IWMPPlayer> g_spWMPPlayer;
bool g_bWantToBeFullscreen = false;
bool g_bPlayOnRestore = false;
int g_desiredVideoScaleMode = ID_STRETCH_TO_FIT;

bool FailedUI(HRESULT hr, const char* description) {
  const bool is_failed{FAILED(hr)};

  if (is_failed) {
    char error[1024];
    sprintf_s(error, "%s\n\n(hr) 0x%08x: %s.\n", description, hr,
              _com_error{hr}.ErrorMessage());

    OutputDebugString(error);
    // MessageBox(nullptr, error, "Steam Media Player - Error",
    //            MB_OK | MB_ICONERROR);
  }

  return is_failed;
}

void LogPlayerEvent(EventType_t e) {
  double dpos = 0.0;

  if (g_spWMPPlayer) {
    if (CComPtr<IWMPControls> wmp_controls;
        !FailedUI(g_spWMPPlayer->get_controls(&wmp_controls),
                  "Failed to get WMP controls.")) {
      FailedUI(wmp_controls->get_currentPosition(&dpos),
               "Failed to get current WMP position.");
    }
  }

  LogPlayerEvent(e, dpos);
}

bool IsStretchedToFit() {
  if (!g_spWMPPlayer) return false;

  CComPtr<IWMPPlayer2> wmp_player2;

  if (FailedUI(g_spWMPPlayer.QueryInterface(&wmp_player2),
               "Failed to get WMP Player 2 interface.")) {
    return false;
  }

  VARIANT_BOOL stretch_2_fit{VARIANT_FALSE};

  if (FailedUI(wmp_player2->get_stretchToFit(&stretch_2_fit),
               "Failed to get WMP stretch-to-fit proeprty.")) {
    return false;
  }

  return stretch_2_fit == VARIANT_TRUE;
}

bool SetVideoScaleMode(int videoScaleMode) {
  g_desiredVideoScaleMode = videoScaleMode;

  if (!g_spWMPPlayer) return false;

  long width = 0, height = 0;
  CComPtr<IWMPMedia> wmp_media;

  if (FailedUI(g_spWMPPlayer->get_currentMedia(&wmp_media),
               "Failed to get WMP Media interface.")) {
    return false;
  }

  if (FailedUI(wmp_media->get_imageSourceWidth(&width),
               "Failed to get image source width.")) {
    return false;
  }

  if (FailedUI(wmp_media->get_imageSourceHeight(&height),
               "Failed to get image source height.")) {
    return false;
  }

  VARIANT_BOOL stretch_2_fit = VARIANT_FALSE;
  switch (videoScaleMode) {
    case ID_HALF_SIZE:
      // TODO - set video player size
      break;
    case ID_FULL_SIZE:
      // TODO - set video player size
      break;
    case ID_DOUBLE_SIZE:
      // TODO - set video player size
      break;

    case ID_STRETCH_TO_FIT:
      stretch_2_fit = VARIANT_TRUE;
      break;

    default:
      return false;
  }

  CComPtr<IWMPPlayer2> wmp_player2;
  if (FailedUI(g_spWMPPlayer.QueryInterface(&wmp_player2),
               "Failed to get WMP Player 2 interface.")) {
    return false;
  }

  bool is_stretch_2_fit = stretch_2_fit == VARIANT_TRUE;
  if (is_stretch_2_fit == IsStretchedToFit()) return true;

  FailedUI(wmp_player2->put_stretchToFit(stretch_2_fit),
           "Failed to set stretch-to-fit option.");

  return is_stretch_2_fit == IsStretchedToFit();
}

bool IsFullScreen() {
  if (!g_spWMPPlayer) return false;

  VARIANT_BOOL is_fullscreen = VARIANT_TRUE;
  return !FailedUI(g_spWMPPlayer->get_fullScreen(&is_fullscreen),
                   "Failed to get full screen mode.") &&
         is_fullscreen == VARIANT_TRUE;
}

bool SetFullScreen(bool bWantToBeFullscreen) {
  g_bWantToBeFullscreen = bWantToBeFullscreen;

  if (!g_spWMPPlayer) return false;

  LogPlayerEvent(bWantToBeFullscreen ? EventType_t::ET_MAXIMIZE
                                     : EventType_t::ET_RESTORE);

  bool bIsFullscreen = IsFullScreen();

  CComPtr<IWMPPlayer2> wmp_player2;
  if (FailedUI(g_spWMPPlayer.QueryInterface(&wmp_player2),
               "Failed to get WMP Player 2 interface.")) {
    return false;
  }

  if (VARIANT_BOOL vbStretch = VARIANT_TRUE;
      !FailedUI(wmp_player2->get_stretchToFit(&vbStretch),
                "Failed to get stretch-to-fit option.")) {
    bool bStretch = vbStretch == VARIANT_TRUE;

    if (bStretch != g_bWantToBeFullscreen) {
      bIsFullscreen = !g_bWantToBeFullscreen;
    }
  }

  if (g_bWantToBeFullscreen == bIsFullscreen) return true;

  if (g_bWantToBeFullscreen) {
    if (FailedUI(g_spWMPPlayer->put_uiMode(_bstr_t{"none"}),
                 "Failed to set none UI mode.")) {
      return false;
    }

    if (FailedUI(g_spWMPPlayer->put_fullScreen(VARIANT_TRUE),
                 "Failed to set full screen mode.")) {
      return false;
    }

    if (FailedUI(wmp_player2->put_stretchToFit(VARIANT_TRUE),
                 "Failed to set stretch-to-fit option.")) {
      return false;
    }

    while (ShowCursor(FALSE) >= 0);
  } else {
    if (FailedUI(g_spWMPPlayer->put_fullScreen(VARIANT_FALSE),
                 "Failed to undo full screen mode.")) {
      return false;
    }

    if (FailedUI(g_spWMPPlayer->put_uiMode(_bstr_t{"full"}),
                 "Failed to set full UI mode.")) {
      return false;
    }

    if (FailedUI(wmp_player2->put_stretchToFit(g_desiredVideoScaleMode ==
                                                       ID_STRETCH_TO_FIT
                                                   ? VARIANT_TRUE
                                                   : VARIANT_FALSE),
                 "Failed to set stretch-to-fit option.")) {
      return false;
    }

    g_pFrame->ShowWindow(SW_RESTORE);

    while (ShowCursor(TRUE) < 0);
  }

  bIsFullscreen = IsFullScreen();

  if (bIsFullscreen != g_bWantToBeFullscreen) {
    g_bWantToBeFullscreen = bIsFullscreen;
    OutputDebugString("[log] SetFullScreen FAILED!\n");
    return false;
  }

  if (g_bWantToBeFullscreen) {
    VARIANT_BOOL vbStretch = VARIANT_TRUE;
    if (FailedUI(wmp_player2->get_stretchToFit(&vbStretch),
                 "Failed to get stretch-to-fit option.")) {
      return false;
    }

    if (vbStretch != VARIANT_TRUE) {
      OutputDebugString("[log] SetFullScreen FAILED to set stretchToFit!\n");
      return false;
    }
  }

  return true;
}

bool IsVideoPlaying() {
  WMPPlayState playState;
  return !FailedUI(g_spWMPPlayer->get_playState(&playState),
                   "Failed WMP Play state.\n") &&
         playState == wmppsPlaying;
}

void PlayVideo(bool bPlay) {
  CComPtr<IWMPControls> wmp_controls;

  if (!FailedUI(g_spWMPPlayer->get_controls(&wmp_controls),
                "Failed to get WMP controls.")) {
    if (bPlay) {
      FailedUI(wmp_controls->play(), "Failed to play.");
    } else {
      FailedUI(wmp_controls->pause(), "Failed to pause.");
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
// SteamWmpHost

LRESULT SteamWmpHost::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam,
                               BOOL& bHandled) {
  AtlAxWinInit();

  CComPtr<IAxWinHostWindow> spHost;
  CComPtr<IConnectionPointContainer> spConnectionContainer;
  CComWMPEventDispatch* pEventListener = nullptr;
  CComPtr<IWMPEvents> spEventListener;
  CComPtr<IWMPSettings> spWMPSettings;

  m_dwAdviseCookie = 0;
  m_hPopupMenu = nullptr;

  HRESULT hr;

  // create window
  RECT rcClient;
  GetClientRect(&rcClient);
  m_wndView.Create(m_hWnd, rcClient, nullptr,
                   WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);
  if (!m_wndView.m_hWnd) goto FAILURE;

  // load OCX in window

  hr = m_wndView.QueryHost(&spHost);
  if (FailedUI(hr, "Failed to query host.")) goto FAILURE;

  hr = spHost->CreateControl(
      CComBSTR(_T("{6BF52A52-394A-11d3-B153-00C04F79FAA6}")), m_wndView, 0);
  if (FailedUI(hr, "Failed to create control.")) goto FAILURE;

  hr = m_wndView.QueryControl(&g_spWMPPlayer);
  if (FailedUI(hr, "Failed to query WMP Player control.")) goto FAILURE;

  // start listening to events

  hr = CComWMPEventDispatch::CreateInstance(&pEventListener);
  if (FailedUI(hr, "Failed to create WMP Event Dispatcher.")) goto FAILURE;

  spEventListener = pEventListener;

  hr = g_spWMPPlayer->QueryInterface(&spConnectionContainer);
  if (FailedUI(hr, "Failed to query connection container interface."))
    goto FAILURE;

  // See if OCX supports the IWMPEvents interface
  hr = spConnectionContainer->FindConnectionPoint(__uuidof(IWMPEvents),
                                                  &m_spConnectionPoint);
  if (FAILED(hr)) {
    // If not, try the _WMPOCXEvents interface, which will use IDispatch
    hr = spConnectionContainer->FindConnectionPoint(__uuidof(_WMPOCXEvents),
                                                    &m_spConnectionPoint);
    if (FailedUI(hr,
                 "Failed to find connection to IWMPEvents and _WMPOCXEvents."))
      goto FAILURE;
  }

  hr = m_spConnectionPoint->Advise(spEventListener, &m_dwAdviseCookie);
  if (FailedUI(hr, "Failed to advice on WMP Event Dispatcher.")) goto FAILURE;

  hr = g_spWMPPlayer->get_settings(&spWMPSettings);
  if (FailedUI(hr, "Failed to get WMP Settings.")) goto FAILURE;

  FailedUI(spWMPSettings->put_volume(100), "Failed to set volume to 100.");
  FailedUI(g_spWMPPlayer->put_enableContextMenu(VARIANT_FALSE),
           "Failed to disable context menu.");

  // set the url of the movie
  hr = g_spWMPPlayer->put_URL(CT2W(m_url.c_str()));
  if (FailedUI(hr, "Failed to set URL to play.")) goto FAILURE;

  return 0;

FAILURE:
  OutputDebugString("[log] SteamWmpHost::OnCreate FAILED!\n");

  DestroyWindow();
  if (g_hBlackFadingWindow) ::DestroyWindow(g_hBlackFadingWindow);

  return 1;
}

LRESULT SteamWmpHost::OnClose(UINT uMsg, WPARAM wParam, LPARAM lParam,
                              BOOL& bHandled) {
  LogPlayerEvent(EventType_t::ET_CLOSE);

  if (m_hPopupMenu) {
    DestroyMenu(m_hPopupMenu);
    m_hPopupMenu = nullptr;
  }

  DestroyWindow();
  if (g_hBlackFadingWindow) ::DestroyWindow(g_hBlackFadingWindow);

  return 0;
}

LRESULT SteamWmpHost::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam,
                                BOOL& bHandled) {
  // stop listening to events
  if (m_spConnectionPoint) {
    if (m_dwAdviseCookie)
      FailedUI(m_spConnectionPoint->Unadvise(m_dwAdviseCookie),
               "Failed to unadvise.");

    m_spConnectionPoint.Release();
  }

  // close the OCX
  if (g_spWMPPlayer) {
    FailedUI(g_spWMPPlayer->close(), "Failed to close WMP.");
    g_spWMPPlayer.Release();
  }

  m_hWnd = nullptr;
  g_bFrameCreated = false;

  bHandled = FALSE;
  return 1;
}

LRESULT SteamWmpHost::OnErase(UINT /* uMsg */, WPARAM /* wParam */,
                              LPARAM /* lParam */, BOOL& bHandled) {
  if (g_bWantToBeFullscreen && !IsFullScreen()) {
    g_pFrame->BringWindowToTop();
    SetFullScreen(false);
  }

  bHandled = TRUE;
  return 0;
}

LRESULT SteamWmpHost::OnSize(UINT /* uMsg */, WPARAM wParam, LPARAM lParam,
                             BOOL& /* lResult */) {
  if (wParam == SIZE_MAXIMIZED) {
    SetFullScreen(true);
  } else {
    if (wParam == SIZE_MINIMIZED) {
      LogPlayerEvent(EventType_t::ET_MINIMIZE);

      if (IsVideoPlaying()) {
        g_bPlayOnRestore = true;
        PlayVideo(false);
      }
    } else if (wParam == SIZE_RESTORED) {
      LogPlayerEvent(EventType_t::ET_RESTORE);
    }

    RECT rcClient;
    GetClientRect(&rcClient);
    m_wndView.MoveWindow(rcClient.left, rcClient.top,
                         rcClient.right - rcClient.left,
                         rcClient.bottom - rcClient.top);
  }

  return 0;
}

LRESULT SteamWmpHost::OnContextMenu(UINT /* uMsg */, WPARAM /* wParam */,
                                    LPARAM lParam, BOOL& /* lResult */) {
  if (!m_hPopupMenu) {
    m_hPopupMenu = CreatePopupMenu();

    AppendMenu(m_hPopupMenu, MF_STRING, ID_HALF_SIZE, "Zoom 50%");
    AppendMenu(m_hPopupMenu, MF_STRING, ID_FULL_SIZE, "Zoom 100%");
    AppendMenu(m_hPopupMenu, MF_STRING, ID_DOUBLE_SIZE, "Zoom 200%");
    AppendMenu(m_hPopupMenu, MF_STRING, ID_STRETCH_TO_FIT,
               "Stretch to fit window");
  }

  const int x{GET_X_LPARAM(lParam)};
  const int y{GET_Y_LPARAM(lParam)};

  return TrackPopupMenu(m_hPopupMenu, TPM_LEFTALIGN | TPM_TOPALIGN, x, y, 0,
                        m_hWnd, nullptr) != 0
             ? 0
             : 1;
}

LRESULT SteamWmpHost::OnClick(UINT /* uMsg */, WPARAM /* wParam */,
                              LPARAM /* lParam */, BOOL& /* lResult */) {
  if (IsFullScreen()) SetFullScreen(false);

  return 1;
}

LRESULT SteamWmpHost::OnLeftDoubleClick(UINT /* uMsg */, WPARAM /* wParam */,
                                        LPARAM /* lParam */,
                                        BOOL& /* lResult */) {
  SetFullScreen(!IsFullScreen());

  return 1;
}

LRESULT SteamWmpHost::OnSysKeyDown(UINT /* uMsg */, WPARAM wParam,
                                   LPARAM /* lParam */, BOOL& /* lResult */) {
  switch (wParam) {
    case VK_RETURN:
      SetFullScreen(!IsFullScreen());
      break;
  }

  return 1;
}

LRESULT SteamWmpHost::OnKeyDown(UINT /* uMsg */, WPARAM wParam,
                                LPARAM /* lParam */, BOOL& /* lResult */) {
  switch (wParam) {
    case VK_SPACE:
      PlayVideo(!IsVideoPlaying());
      break;

    case VK_LEFT:
    case VK_RIGHT: {
      CComPtr<IWMPControls> wmp_controls;
      if (FailedUI(g_spWMPPlayer->get_controls(&wmp_controls),
                   "Failed to get WMP Controls.")) {
        break;
      }

      CComPtr<IWMPControls2> wmp_controls2;
      if (FailedUI(wmp_controls.QueryInterface(&wmp_controls2),
                   "Failed to get WMP Controls 2.")) {
        break;
      }

      if (FailedUI(wmp_controls2->step(wParam == VK_LEFT ? -1 : 1),
                   "Failed to step.")) {
        break;
      }

      LogPlayerEvent(wParam == VK_LEFT ? EventType_t::ET_STEPBCK
                                       : EventType_t::ET_STEPFWD);
    } break;

    case VK_UP:
    case VK_DOWN: {
      CComPtr<IWMPControls> wmp_controls;
      if (FailedUI(g_spWMPPlayer->get_controls(&wmp_controls),
                   "Failed to get WMP controls.")) {
        break;
      }

      double curpos = 0.0f;
      if (FailedUI(wmp_controls->get_currentPosition(&curpos),
                   "Failed to get current position.")) {
        break;
      }

      curpos += wParam == VK_UP ? -5.0f : 5.0f;

      if (FailedUI(wmp_controls->put_currentPosition(curpos),
                   "Failed to set current position")) {
        break;
      }

      if (!IsVideoPlaying()) {
        if (FailedUI(wmp_controls->play(), "Failed to play.")) {
          break;
        }

        if (FailedUI(wmp_controls->pause(), "Failed to pause.")) {
          break;
        }
      }

      LogPlayerEvent(wParam == VK_UP ? EventType_t::ET_JUMPBCK
                                     : EventType_t::ET_JUMPFWD);
    } break;

    case VK_HOME: {
      CComPtr<IWMPControls> wmp_controls;
      if (FailedUI(g_spWMPPlayer->get_controls(&wmp_controls),
                   "Failed to get WMP Controls")) {
        break;
      }

      if (FailedUI(wmp_controls->put_currentPosition(0.0),
                   "Failed to set current position")) {
        break;
      }

      if (!IsVideoPlaying()) {
        if (FailedUI(wmp_controls->play(), "Failed to play.")) {
          break;
        }

        if (FailedUI(wmp_controls->pause(), "Failed to pause.")) {
          break;
        }
      }

      LogPlayerEvent(EventType_t::ET_JUMPHOME);
    } break;

    case VK_END: {
      CComPtr<IWMPMedia> wmp_media;
      if (FailedUI(g_spWMPPlayer->get_currentMedia(&wmp_media),
                   "Failed to get current WMP Media.")) {
        break;
      }

      CComPtr<IWMPControls> wmp_controls;
      if (FailedUI(g_spWMPPlayer->get_controls(&wmp_controls),
                   "Failed to get WMP Controls")) {
        break;
      }

      double duration = 0.0;
      if (FailedUI(wmp_media->get_duration(&duration),
                   "Failed to get duration.")) {
        break;
      }

      // back a little more than a frame - this avoids triggering the end fade
      if (FailedUI(wmp_controls->put_currentPosition(duration - 0.050),
                   "Failed to set current position")) {
        break;
      }

      if (FailedUI(wmp_controls->play(), "Failed to play.")) {
        break;
      }

      if (FailedUI(wmp_controls->pause(), "Failed to pause.")) {
        break;
      }

      LogPlayerEvent(EventType_t::ET_JUMPEND);
    } break;

    case VK_ESCAPE:
      if (IsFullScreen() && !g_bFadeWindowTriggered) {
        CComPtr<IWMPControls> wmp_controls;
        if (FailedUI(g_spWMPPlayer->get_controls(&wmp_controls),
                     "Failed to get WMP Controls")) {
          break;
        }

        if (FailedUI(wmp_controls->stop(), "Failed to stop WMP Controls.\n")) {
          break;
        }

        LogPlayerEvent(EventType_t::ET_FADEOUT);

        g_bFadeWindowTriggered = true;
        ShowFadeWindow(true);
      }
      break;
  }

  return 0;
}

LRESULT SteamWmpHost::OnNCActivate(UINT /* uMsg */, WPARAM wParam,
                                   LPARAM /* lParam */, BOOL& /* lResult */) {
  if (wParam) {
    if (g_bWantToBeFullscreen) {
      SetFullScreen(true);
    }
    if (g_bPlayOnRestore) {
      g_bPlayOnRestore = false;
      PlayVideo(true);
    }
  }

  return 1;
}

LRESULT SteamWmpHost::OnVideoScale(WORD wNotifyCode, WORD wID, HWND hWndCtl,
                                   BOOL& bHandled) {
  SetVideoScaleMode(wID);
  return 0;
}

void SteamWmpHost::SetUrl(const std::string& url) { m_url = url; }
