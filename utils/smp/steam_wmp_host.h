// Copyright Valve Corporation, All rights reserved.

#ifndef SE_UTILS_SMP_STEAM_WMP_HOST_H_
#define SE_UTILS_SMP_STEAM_WMP_HOST_H_

#include <oledlg.h>
#include <wmp.h>

#include <string>

#include "steam_wmp_event_dispatch.h"
#include "resource.h"

class SteamWmpHost
    : public CWindowImpl<
          SteamWmpHost, CWindow,
          CWinTraits<WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                     WS_EX_APPWINDOW | WS_EX_WINDOWEDGE>> {
 public:
  DECLARE_WND_CLASS_EX(nullptr, 0, 0)

  BEGIN_MSG_MAP(SteamWmpHost)
    MESSAGE_HANDLER(WM_CREATE, OnCreate)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    MESSAGE_HANDLER(WM_CLOSE, OnClose)
    MESSAGE_HANDLER(WM_SIZE, OnSize)
    MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)
    MESSAGE_HANDLER(WM_LBUTTONDOWN, OnClick)
    MESSAGE_HANDLER(WM_MBUTTONDOWN, OnClick)
    MESSAGE_HANDLER(WM_RBUTTONDOWN, OnClick)
    MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnLeftDoubleClick)
    MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
    MESSAGE_HANDLER(WM_SYSKEYDOWN, OnSysKeyDown)
    MESSAGE_HANDLER(WM_ERASEBKGND, OnErase)
    MESSAGE_HANDLER(WM_NCACTIVATE, OnNCActivate)

    COMMAND_ID_HANDLER(ID_HALF_SIZE, OnVideoScale)
    COMMAND_ID_HANDLER(ID_FULL_SIZE, OnVideoScale)
    COMMAND_ID_HANDLER(ID_DOUBLE_SIZE, OnVideoScale)
    COMMAND_ID_HANDLER(ID_STRETCH_TO_FIT, OnVideoScale)
  END_MSG_MAP()

  LRESULT OnClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnErase(UINT, WPARAM, LPARAM, BOOL& bHandled);
  LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnContextMenu(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClick(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnLeftDoubleClick(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnSysKeyDown(UINT, WPARAM wParam, LPARAM, BOOL&);
  LRESULT OnKeyDown(UINT, WPARAM wParam, LPARAM, BOOL&);
  LRESULT OnNCActivate(UINT, WPARAM wParam, LPARAM, BOOL&);

  LRESULT OnVideoScale(WORD wNotifyCode, WORD wID, HWND hWndCtl,
                       BOOL& bHandled);

  void SetUrl(const std::string &url);

  CAxWindow m_wndView;
  CComPtr<IConnectionPoint> m_spConnectionPoint;
  DWORD m_dwAdviseCookie;
  HMENU m_hPopupMenu;
  std::string m_url;
};

// event logging - this should really be in its own smp.h or something...
enum class EventType_t {
  ET_APPLAUNCH,
  ET_APPEXIT,
  ET_CLOSE,
  ET_FADEOUT,

  ET_MEDIABEGIN,
  ET_MEDIAEND,

  ET_JUMPHOME,
  ET_JUMPEND,

  ET_BUFFERING,
  ET_WAITING,
  ET_TRANSITIONING,
  ET_READY,
  ET_RECONNECTING,

  ET_PLAY,
  ET_PAUSE,
  ET_STOP,
  ET_SCRUBFROM,
  ET_SCRUBTO,
  ET_STEPFWD,
  ET_STEPBCK,
  ET_JUMPFWD,
  ET_JUMPBCK,
  ET_REPEAT,

  ET_MAXIMIZE,
  ET_MINIMIZE,
  ET_RESTORE,

  ET_ERROR
};

bool FailedUI(HRESULT hr, const char* description);

#endif  // !SE_UTILS_SMP_STEAM_WMP_HOST_H_
