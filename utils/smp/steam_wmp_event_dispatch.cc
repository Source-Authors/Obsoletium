// Copyright Valve Corporation, All rights reserved.

#include "steam_wmp_event_dispatch.h"

#include <comdef.h>

#include "steam_wmp_host.h"

extern HWND g_hBlackFadingWindow;
extern CComPtr<IWMPPlayer> g_spWMPPlayer;
extern SteamWmpHost* g_pFrame;
extern double g_timeAtFadeStart;
extern bool g_bFadeIn;

bool g_bFadeWindowTriggered = false;

bool IsFullScreen();
bool SetFullScreen(bool bWantToBeFullscreen);
bool IsVideoPlaying();
void PlayVideo(bool bPlay);
bool ShowFadeWindow(bool bShow);
void LogPlayerEvent(EventType_t e, double pos);
void LogPlayerEvent(EventType_t e);

HRESULT SteamWmpEventDispatch::Invoke(DISPID dispIdMember, REFIID, LCID, WORD,
                                      DISPPARAMS FAR* pDispParams, VARIANT FAR*,
                                      EXCEPINFO FAR*, unsigned int FAR*) {
  if (!pDispParams) return E_POINTER;

  if (pDispParams->cNamedArgs != 0) return DISP_E_NONAMEDARGS;

  HRESULT hr = DISP_E_MEMBERNOTFOUND;

  switch (dispIdMember) {
    case DISPID_WMPCOREEVENT_OPENSTATECHANGE:
      OpenStateChange(pDispParams->rgvarg[0].lVal /* NewState */);
      break;

    case DISPID_WMPCOREEVENT_PLAYSTATECHANGE:
      PlayStateChange(pDispParams->rgvarg[0].lVal /* NewState */);
      break;

    case DISPID_WMPCOREEVENT_AUDIOLANGUAGECHANGE:
      AudioLanguageChange(pDispParams->rgvarg[0].lVal /* LangID */);
      break;

    case DISPID_WMPCOREEVENT_STATUSCHANGE:
      StatusChange();
      break;

    case DISPID_WMPCOREEVENT_SCRIPTCOMMAND:
      ScriptCommand(pDispParams->rgvarg[1].bstrVal /* scType */,
                    pDispParams->rgvarg[0].bstrVal /* Param */);
      break;

    case DISPID_WMPCOREEVENT_NEWSTREAM:
      NewStream();
      break;

    case DISPID_WMPCOREEVENT_DISCONNECT:
      Disconnect(pDispParams->rgvarg[0].lVal /* Result */);
      break;

    case DISPID_WMPCOREEVENT_BUFFERING:
      Buffering(pDispParams->rgvarg[0].boolVal /* Start */);
      break;

    case DISPID_WMPCOREEVENT_ERROR:
      Error();
      break;

    case DISPID_WMPCOREEVENT_WARNING:
      Warning(pDispParams->rgvarg[1].lVal /* WarningType */,
              pDispParams->rgvarg[0].lVal /* Param */,
              pDispParams->rgvarg[2].bstrVal /* Description */);
      break;

    case DISPID_WMPCOREEVENT_ENDOFSTREAM:
      EndOfStream(pDispParams->rgvarg[0].lVal /* Result */);
      break;

    case DISPID_WMPCOREEVENT_POSITIONCHANGE:
      PositionChange(pDispParams->rgvarg[1].dblVal /* oldPosition */,
                     pDispParams->rgvarg[0].dblVal /* newPosition */);
      break;

    case DISPID_WMPCOREEVENT_MARKERHIT:
      MarkerHit(pDispParams->rgvarg[0].lVal /* MarkerNum */);
      break;

    case DISPID_WMPCOREEVENT_DURATIONUNITCHANGE:
      DurationUnitChange(pDispParams->rgvarg[0].lVal /* NewDurationUnit */);
      break;

    case DISPID_WMPCOREEVENT_CDROMMEDIACHANGE:
      CdromMediaChange(pDispParams->rgvarg[0].lVal /* CdromNum */);
      break;

    case DISPID_WMPCOREEVENT_PLAYLISTCHANGE:
      PlaylistChange(
          pDispParams->rgvarg[1].pdispVal /* Playlist */,
          (WMPPlaylistChangeEventType)pDispParams->rgvarg[0].lVal /* change */);
      break;

    case DISPID_WMPCOREEVENT_CURRENTPLAYLISTCHANGE:
      CurrentPlaylistChange(
          (WMPPlaylistChangeEventType)pDispParams->rgvarg[0].lVal /* change */);
      break;

    case DISPID_WMPCOREEVENT_CURRENTPLAYLISTITEMAVAILABLE:
      CurrentPlaylistItemAvailable(
          pDispParams->rgvarg[0].bstrVal /*  bstrItemName */);
      break;

    case DISPID_WMPCOREEVENT_MEDIACHANGE:
      MediaChange(pDispParams->rgvarg[0].pdispVal /* Item */);
      break;

    case DISPID_WMPCOREEVENT_CURRENTMEDIAITEMAVAILABLE:
      CurrentMediaItemAvailable(
          pDispParams->rgvarg[0].bstrVal /* bstrItemName */);
      break;

    case DISPID_WMPCOREEVENT_CURRENTITEMCHANGE:
      CurrentItemChange(pDispParams->rgvarg[0].pdispVal /* pdispMedia */);
      break;

    case DISPID_WMPCOREEVENT_MEDIACOLLECTIONCHANGE:
      MediaCollectionChange();
      break;

    case DISPID_WMPCOREEVENT_MEDIACOLLECTIONATTRIBUTESTRINGADDED:
      MediaCollectionAttributeStringAdded(
          pDispParams->rgvarg[1].bstrVal /* bstrAttribName */,
          pDispParams->rgvarg[0].bstrVal /* bstrAttribVal */);
      break;

    case DISPID_WMPCOREEVENT_MEDIACOLLECTIONATTRIBUTESTRINGREMOVED:
      MediaCollectionAttributeStringRemoved(
          pDispParams->rgvarg[1].bstrVal /* bstrAttribName */,
          pDispParams->rgvarg[0].bstrVal /* bstrAttribVal */);
      break;

    case DISPID_WMPCOREEVENT_MEDIACOLLECTIONATTRIBUTESTRINGCHANGED:
      MediaCollectionAttributeStringChanged(
          pDispParams->rgvarg[2].bstrVal /* bstrAttribName */,
          pDispParams->rgvarg[1].bstrVal /* bstrOldAttribVal */,
          pDispParams->rgvarg[0].bstrVal /* bstrNewAttribVal */);
      break;

    case DISPID_WMPCOREEVENT_PLAYLISTCOLLECTIONCHANGE:
      PlaylistCollectionChange();
      break;

    case DISPID_WMPCOREEVENT_PLAYLISTCOLLECTIONPLAYLISTADDED:
      PlaylistCollectionPlaylistAdded(
          pDispParams->rgvarg[0].bstrVal /* bstrPlaylistName */);
      break;

    case DISPID_WMPCOREEVENT_PLAYLISTCOLLECTIONPLAYLISTREMOVED:
      PlaylistCollectionPlaylistRemoved(
          pDispParams->rgvarg[0].bstrVal /* bstrPlaylistName */);
      break;

    case DISPID_WMPCOREEVENT_PLAYLISTCOLLECTIONPLAYLISTSETASDELETED:
      PlaylistCollectionPlaylistSetAsDeleted(
          pDispParams->rgvarg[1].bstrVal /* bstrPlaylistName */,
          pDispParams->rgvarg[0].boolVal /* varfIsDeleted */);
      break;

    case DISPID_WMPCOREEVENT_MODECHANGE:
      ModeChange(pDispParams->rgvarg[1].bstrVal /* ModeName */,
                 pDispParams->rgvarg[0].boolVal /* NewValue */);
      break;

    case DISPID_WMPCOREEVENT_MEDIAERROR:
      MediaError(pDispParams->rgvarg[0].pdispVal /* pMediaObject */);
      break;

    case DISPID_WMPCOREEVENT_OPENPLAYLISTSWITCH:
      OpenPlaylistSwitch(pDispParams->rgvarg[0].pdispVal /* pItem */);
      break;

    case DISPID_WMPCOREEVENT_DOMAINCHANGE:
      DomainChange(pDispParams->rgvarg[0].bstrVal /* strDomain */);
      break;

    case DISPID_WMPOCXEVENT_SWITCHEDTOPLAYERAPPLICATION:
      SwitchedToPlayerApplication();
      break;

    case DISPID_WMPOCXEVENT_SWITCHEDTOCONTROL:
      SwitchedToControl();
      break;

    case DISPID_WMPOCXEVENT_PLAYERDOCKEDSTATECHANGE:
      PlayerDockedStateChange();
      break;

    case DISPID_WMPOCXEVENT_PLAYERRECONNECT:
      PlayerReconnect();
      break;

    case DISPID_WMPOCXEVENT_CLICK:
      Click(pDispParams->rgvarg[3].iVal /* nButton */,
            pDispParams->rgvarg[2].iVal /* nShiftState */,
            pDispParams->rgvarg[1].lVal /* fX */,
            pDispParams->rgvarg[0].lVal /* fY */);
      break;

    case DISPID_WMPOCXEVENT_DOUBLECLICK:
      DoubleClick(pDispParams->rgvarg[3].iVal /* nButton */,
                  pDispParams->rgvarg[2].iVal /* nShiftState */,
                  pDispParams->rgvarg[1].lVal /* fX */,
                  pDispParams->rgvarg[0].lVal /* fY */);
      break;

    case DISPID_WMPOCXEVENT_KEYDOWN:
      KeyDown(pDispParams->rgvarg[1].iVal /* nKeyCode */,
              pDispParams->rgvarg[0].iVal /* nShiftState */);
      break;

    case DISPID_WMPOCXEVENT_KEYPRESS:
      KeyPress(pDispParams->rgvarg[0].iVal /* nKeyAscii */);
      break;

    case DISPID_WMPOCXEVENT_KEYUP:
      KeyUp(pDispParams->rgvarg[1].iVal /* nKeyCode */,
            pDispParams->rgvarg[0].iVal /* nShiftState */);
      break;

    case DISPID_WMPOCXEVENT_MOUSEDOWN:
      MouseDown(pDispParams->rgvarg[3].iVal /* nButton */,
                pDispParams->rgvarg[2].iVal /* nShiftState */,
                pDispParams->rgvarg[1].lVal /* fX */,
                pDispParams->rgvarg[0].lVal /* fY */);
      break;

    case DISPID_WMPOCXEVENT_MOUSEMOVE:
      MouseMove(pDispParams->rgvarg[3].iVal /* nButton */,
                pDispParams->rgvarg[2].iVal /* nShiftState */,
                pDispParams->rgvarg[1].lVal /* fX */,
                pDispParams->rgvarg[0].lVal /* fY */);
      break;

    case DISPID_WMPOCXEVENT_MOUSEUP:
      MouseUp(pDispParams->rgvarg[3].iVal /* nButton */,
              pDispParams->rgvarg[2].iVal /* nShiftState */,
              pDispParams->rgvarg[1].lVal /* fX */,
              pDispParams->rgvarg[0].lVal /* fY */);
      break;
  }

  return hr;
}

// Sent when the control changes OpenState
void SteamWmpEventDispatch::OpenStateChange(long NewState) {}

// Sent when the control changes PlayState
void SteamWmpEventDispatch::PlayStateChange(long NewState) {
  WMPPlayState playstate;

  if (!FailedUI(g_spWMPPlayer->get_playState(&playstate),
                "Failed to get WMP play state.")) {
    switch (playstate) {
      case wmppsBuffering:
        LogPlayerEvent(EventType_t::ET_BUFFERING);
        break;

      case wmppsWaiting:
        LogPlayerEvent(EventType_t::ET_WAITING);
        break;

      case wmppsTransitioning:
        LogPlayerEvent(EventType_t::ET_TRANSITIONING);
        break;

      case wmppsReady:
        LogPlayerEvent(EventType_t::ET_READY);
        break;

      case wmppsReconnecting:
        LogPlayerEvent(EventType_t::ET_RECONNECTING);
        break;

      case wmppsPlaying: {
        static bool s_first = true;
        if (s_first) {
          s_first = false;

          LogPlayerEvent(EventType_t::ET_MEDIABEGIN);

          SetFullScreen(true);
          ShowFadeWindow(false);
        } else {
          LogPlayerEvent(EventType_t::ET_PLAY);
        }
        break;
      }

      case wmppsPaused:
        LogPlayerEvent(EventType_t::ET_PAUSE);
        break;

      case wmppsStopped:
        LogPlayerEvent(EventType_t::ET_STOP);
        break;

      case wmppsMediaEnded:
        LogPlayerEvent(EventType_t::ET_MEDIAEND);

        if (IsFullScreen() && !g_bFadeWindowTriggered) {
          g_bFadeWindowTriggered = true;
          ShowFadeWindow(true);
        }
        break;
    }
  }
}

// Sent when the audio language changes
void SteamWmpEventDispatch::AudioLanguageChange(long LangID) {}

// Sent when the status string changes
void SteamWmpEventDispatch::StatusChange() {}

// Sent when a synchronized command or URL is received
void SteamWmpEventDispatch::ScriptCommand(BSTR scType, BSTR Param) {}

// Sent when a new stream is encountered (obsolete)
void SteamWmpEventDispatch::NewStream() {}

// Sent when the control is disconnected from the server (obsolete)
void SteamWmpEventDispatch::Disconnect(long Result) {}

// Sent when the control begins or ends buffering
void SteamWmpEventDispatch::Buffering(VARIANT_BOOL Start) {}

// Sent when the control has an error condition
void SteamWmpEventDispatch::Error() {
  LogPlayerEvent(EventType_t::ET_ERROR, 0.0);
}

// Sent when the control has an warning condition (obsolete)
void SteamWmpEventDispatch::Warning(long WarningType, long Param,
                                    BSTR Description) {}

// Sent when the media has reached end of stream
void SteamWmpEventDispatch::EndOfStream(long Result) {}

// Indicates that the current position of the movie has changed
void SteamWmpEventDispatch::PositionChange(double oldPosition,
                                           double newPosition) {
  LogPlayerEvent(EventType_t::ET_SCRUBFROM, (float)oldPosition);
  LogPlayerEvent(EventType_t::ET_SCRUBTO, (float)newPosition);
}

// Sent when a marker is reached
void SteamWmpEventDispatch::MarkerHit(long MarkerNum) {}

// Indicates that the unit used to express duration and position has changed
void SteamWmpEventDispatch::DurationUnitChange(long NewDurationUnit) {}

// Indicates that the CD ROM media has changed
void SteamWmpEventDispatch::CdromMediaChange(long CdromNum) {}

// Sent when a playlist changes
void SteamWmpEventDispatch::PlaylistChange(IDispatch* Playlist,
                                           WMPPlaylistChangeEventType change) {}

// Sent when the current playlist changes
void SteamWmpEventDispatch::CurrentPlaylistChange(
    WMPPlaylistChangeEventType change) {}

// Sent when a current playlist item becomes available
void SteamWmpEventDispatch::CurrentPlaylistItemAvailable(BSTR bstrItemName) {}

// Sent when a media object changes
void SteamWmpEventDispatch::MediaChange(IDispatch* Item) {}

// Sent when a current media item becomes available
void SteamWmpEventDispatch::CurrentMediaItemAvailable(BSTR bstrItemName) {}

// Sent when the item selection on the current playlist changes
void SteamWmpEventDispatch::CurrentItemChange(IDispatch* pdispMedia) {}

// Sent when the media collection needs to be requeried
void SteamWmpEventDispatch::MediaCollectionChange() {}

// Sent when an attribute string is added in the media collection
void SteamWmpEventDispatch::MediaCollectionAttributeStringAdded(
    BSTR bstrAttribName, BSTR bstrAttribVal) {}

// Sent when an attribute string is removed from the media collection
void SteamWmpEventDispatch::MediaCollectionAttributeStringRemoved(
    BSTR bstrAttribName, BSTR bstrAttribVal) {}

// Sent when an attribute string is changed in the media collection
void SteamWmpEventDispatch::MediaCollectionAttributeStringChanged(
    BSTR bstrAttribName, BSTR bstrOldAttribVal, BSTR bstrNewAttribVal) {}

// Sent when playlist collection needs to be requeried
void SteamWmpEventDispatch::PlaylistCollectionChange() {}

// Sent when a playlist is added to the playlist collection
void SteamWmpEventDispatch::PlaylistCollectionPlaylistAdded(
    BSTR bstrPlaylistName) {}

// Sent when a playlist is removed from the playlist collection
void SteamWmpEventDispatch::PlaylistCollectionPlaylistRemoved(
    BSTR bstrPlaylistName) {}

// Sent when a playlist has been set or reset as deleted
void SteamWmpEventDispatch::PlaylistCollectionPlaylistSetAsDeleted(
    BSTR bstrPlaylistName, VARIANT_BOOL varfIsDeleted) {}

// Playlist playback mode has changed
void SteamWmpEventDispatch::ModeChange(BSTR ModeName, VARIANT_BOOL NewValue) {}

// Sent when the media object has an error condition
void SteamWmpEventDispatch::MediaError(IDispatch* media_object) {
  while (ShowCursor(TRUE) < 0);

  ShowWindow(g_hBlackFadingWindow, SW_HIDE);
  if (g_pFrame) g_pFrame->ShowWindow(SW_HIDE);

  CComPtr<IWMPMedia> wmp_media;
  if (media_object && SUCCEEDED(media_object->QueryInterface(&wmp_media))) {
    char message[1024] = "Unknown error.";

    _bstr_t bstr;
    if (HRESULT hr{wmp_media->get_sourceURL(bstr.GetAddress())};
        SUCCEEDED(hr)) {
      sprintf_s(message,
                "Sorry, media file '%s' can not be opened.\n\nPlease check "
                "file format is known and supported.\n",
                static_cast<char*>(CW2T(bstr)));
    }

    MessageBox(nullptr, message, "Steam Media Player - Media Error",
               MB_OK | MB_ICONERROR);
  } else {
    MessageBox(nullptr, "Sorry, generic media error occurred.\n",
               "Steam Media Player - Media Error", MB_OK | MB_ICONERROR);
  }

  g_pFrame->PostMessage(WM_CLOSE);
}

// Current playlist switch with no open state change
void SteamWmpEventDispatch::OpenPlaylistSwitch(IDispatch* pItem) {}

// Sent when the current DVD domain changes
void SteamWmpEventDispatch::DomainChange(BSTR strDomain) {}

// Sent when display switches to player application
void SteamWmpEventDispatch::SwitchedToPlayerApplication() {}

// Sent when display switches to control
void SteamWmpEventDispatch::SwitchedToControl() {}

// Sent when the player docks or undocks
void SteamWmpEventDispatch::PlayerDockedStateChange() {}

// Sent when the OCX reconnects to the player
void SteamWmpEventDispatch::PlayerReconnect() {}

// Occurs when a user clicks the mouse
void SteamWmpEventDispatch::Click(short nButton, short nShiftState, long fX,
                                  long fY) {
  if (IsFullScreen()) SetFullScreen(false);
}

// Occurs when a user double-clicks the mouse
void SteamWmpEventDispatch::DoubleClick(short nButton, short nShiftState,
                                        long fX, long fY) {
  // the controls are just drawn into the main window, wheras the video has its
  // own window this check allows us to only fullscreen on doubleclick within
  // the video area
  POINT pt = {fX, fY};
  HWND hWnd = g_pFrame->ChildWindowFromPoint(pt);

  if (ChildWindowFromPoint(hWnd, pt) != hWnd) {
    SetFullScreen(!IsFullScreen());
  }
}

// Occurs when a key is pressed
void SteamWmpEventDispatch::KeyDown(short nKeyCode, short nShiftState) {
  if (!g_pFrame) return;

  // 4 is the alt keymask
  BOOL rval;
  if (nShiftState & 4) {
    g_pFrame->OnSysKeyDown(WM_KEYDOWN, nKeyCode, 0, rval);
  } else {
    g_pFrame->OnKeyDown(WM_KEYDOWN, nKeyCode, 0, rval);
  }
}

// Occurs when a key is pressed and released
void SteamWmpEventDispatch::KeyPress(short nKeyAscii) {}

// Occurs when a key is released
void SteamWmpEventDispatch::KeyUp(short nKeyCode, short nShiftState) {}

// Occurs when a mouse button is pressed
void SteamWmpEventDispatch::MouseDown(short nButton, short nShiftState, long fX,
                                      long fY) {}

// Occurs when a mouse pointer is moved
void SteamWmpEventDispatch::MouseMove(short nButton, short nShiftState, long fX,
                                      long fY) {}

// Occurs when a mouse button is released
void SteamWmpEventDispatch::MouseUp(short nButton, short nShiftState, long fX,
                                    long fY) {}
