// Copyright Valve Corporation, All rights reserved.

#ifndef SE_UTILS_SMP_STEAM_WMP_EVENT_DISPATCH_H_
#define SE_UTILS_SMP_STEAM_WMP_EVENT_DISPATCH_H_

#include "stdafx.h"

#include <wmpids.h>
#include <wmp.h>

class SteamWmpEventDispatch : public CComObjectRootEx<CComSingleThreadModel>,
                              public IWMPEvents,
                              public _WMPOCXEvents {
 public:
  BEGIN_COM_MAP(SteamWmpEventDispatch)
    COM_INTERFACE_ENTRY(_WMPOCXEvents)
    COM_INTERFACE_ENTRY(IWMPEvents)
    COM_INTERFACE_ENTRY(IDispatch)
  END_COM_MAP()

  // IDispatch methods
  STDMETHOD(GetIDsOfNames)
  (REFIID, OLECHAR FAR *FAR *, unsigned int, LCID, DISPID FAR *) {
    return E_NOTIMPL;
  }

  STDMETHOD(GetTypeInfo)
  (unsigned int, LCID, ITypeInfo FAR *FAR *) { return E_NOTIMPL; }

  STDMETHOD(GetTypeInfoCount)(unsigned int FAR *) { return E_NOTIMPL; }

  STDMETHOD(Invoke)
  (DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
   DISPPARAMS FAR *pDispParams, VARIANT FAR *pVarResult,
   EXCEPINFO FAR *pExcepInfo, unsigned int FAR *puArgErr);

  // IWMPEvents methods
  void STDMETHODCALLTYPE OpenStateChange(long NewState);
  void STDMETHODCALLTYPE PlayStateChange(long NewState);
  void STDMETHODCALLTYPE AudioLanguageChange(long LangID);
  void STDMETHODCALLTYPE StatusChange();
  void STDMETHODCALLTYPE ScriptCommand(BSTR scType, BSTR Param);
  void STDMETHODCALLTYPE NewStream();
  void STDMETHODCALLTYPE Disconnect(long Result);
  void STDMETHODCALLTYPE Buffering(VARIANT_BOOL Start);
  void STDMETHODCALLTYPE Error();
  void STDMETHODCALLTYPE Warning(long WarningType, long Param,
                                 BSTR Description);
  void STDMETHODCALLTYPE EndOfStream(long Result);
  void STDMETHODCALLTYPE PositionChange(double oldPosition, double newPosition);
  void STDMETHODCALLTYPE MarkerHit(long MarkerNum);
  void STDMETHODCALLTYPE DurationUnitChange(long NewDurationUnit);
  void STDMETHODCALLTYPE CdromMediaChange(long CdromNum);
  void STDMETHODCALLTYPE PlaylistChange(IDispatch *Playlist,
                                        WMPPlaylistChangeEventType change);
  void STDMETHODCALLTYPE
  CurrentPlaylistChange(WMPPlaylistChangeEventType change);
  void STDMETHODCALLTYPE CurrentPlaylistItemAvailable(BSTR bstrItemName);
  void STDMETHODCALLTYPE MediaChange(IDispatch *Item);
  void STDMETHODCALLTYPE CurrentMediaItemAvailable(BSTR bstrItemName);
  void STDMETHODCALLTYPE CurrentItemChange(IDispatch *pdispMedia);
  void STDMETHODCALLTYPE MediaCollectionChange();
  void STDMETHODCALLTYPE
  MediaCollectionAttributeStringAdded(BSTR bstrAttribName, BSTR bstrAttribVal);
  void STDMETHODCALLTYPE MediaCollectionAttributeStringRemoved(
      BSTR bstrAttribName, BSTR bstrAttribVal);
  void STDMETHODCALLTYPE MediaCollectionAttributeStringChanged(
      BSTR bstrAttribName, BSTR bstrOldAttribVal, BSTR bstrNewAttribVal);
  void STDMETHODCALLTYPE PlaylistCollectionChange();
  void STDMETHODCALLTYPE PlaylistCollectionPlaylistAdded(BSTR bstrPlaylistName);
  void STDMETHODCALLTYPE
  PlaylistCollectionPlaylistRemoved(BSTR bstrPlaylistName);
  void STDMETHODCALLTYPE PlaylistCollectionPlaylistSetAsDeleted(
      BSTR bstrPlaylistName, VARIANT_BOOL varfIsDeleted);
  void STDMETHODCALLTYPE ModeChange(BSTR ModeName, VARIANT_BOOL NewValue);
  void STDMETHODCALLTYPE MediaError(IDispatch *pMediaObject);
  void STDMETHODCALLTYPE OpenPlaylistSwitch(IDispatch *pItem);
  void STDMETHODCALLTYPE DomainChange(BSTR strDomain);
  void STDMETHODCALLTYPE SwitchedToPlayerApplication();
  void STDMETHODCALLTYPE SwitchedToControl();
  void STDMETHODCALLTYPE PlayerDockedStateChange();
  void STDMETHODCALLTYPE PlayerReconnect();
  void STDMETHODCALLTYPE Click(short nButton, short nShiftState, long fX,
                               long fY);
  void STDMETHODCALLTYPE DoubleClick(short nButton, short nShiftState, long fX,
                                     long fY);
  void STDMETHODCALLTYPE KeyDown(short nKeyCode, short nShiftState);
  void STDMETHODCALLTYPE KeyPress(short nKeyAscii);
  void STDMETHODCALLTYPE KeyUp(short nKeyCode, short nShiftState);
  void STDMETHODCALLTYPE MouseDown(short nButton, short nShiftState, long fX,
                                   long fY);
  void STDMETHODCALLTYPE MouseMove(short nButton, short nShiftState, long fX,
                                   long fY);
  void STDMETHODCALLTYPE MouseUp(short nButton, short nShiftState, long fX,
                                 long fY);
};

using CComWMPEventDispatch = CComObject<SteamWmpEventDispatch>;

#endif  // !SE_UTILS_SMP_STEAM_WMP_EVENT_DISPATCH_H_
