// Copyright Valve Corporation, All rights reserved.

#ifndef SE_UTILS_SMP_STEAM_WMP_EVENT_DISPATCH_H_
#define SE_UTILS_SMP_STEAM_WMP_EVENT_DISPATCH_H_

#include "stdafx.h"

#include <wmpids.h>
#include <wmp.h>

namespace se::smp {

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
  (REFIID, OLECHAR FAR *FAR *, unsigned int, LCID, DISPID FAR *) override {
    return E_NOTIMPL;
  }

  STDMETHOD(GetTypeInfo)
  (unsigned int, LCID, ITypeInfo FAR *FAR *) override { return E_NOTIMPL; }

  STDMETHOD(GetTypeInfoCount)(unsigned int FAR *) override { return E_NOTIMPL; }

  STDMETHOD(Invoke)
  (DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
   DISPPARAMS FAR *pDispParams, VARIANT FAR *pVarResult,
   EXCEPINFO FAR *pExcepInfo, unsigned int FAR *puArgErr) override;

  // IWMPEvents methods
  void STDMETHODCALLTYPE OpenStateChange(long NewState) override;
  void STDMETHODCALLTYPE PlayStateChange(long NewState) override;
  void STDMETHODCALLTYPE AudioLanguageChange(long LangID) override;
  void STDMETHODCALLTYPE StatusChange() override;
  void STDMETHODCALLTYPE ScriptCommand(BSTR scType, BSTR Param) override;
  void STDMETHODCALLTYPE NewStream() override;
  void STDMETHODCALLTYPE Disconnect(long Result) override;
  void STDMETHODCALLTYPE Buffering(VARIANT_BOOL Start) override;
  void STDMETHODCALLTYPE Error() override;
  void STDMETHODCALLTYPE Warning(long WarningType, long Param,
                                 BSTR Description) override;
  void STDMETHODCALLTYPE EndOfStream(long Result) override;
  void STDMETHODCALLTYPE PositionChange(double oldPosition,
                                        double newPosition) override;
  void STDMETHODCALLTYPE MarkerHit(long MarkerNum) override;
  void STDMETHODCALLTYPE DurationUnitChange(long NewDurationUnit) override;
  void STDMETHODCALLTYPE CdromMediaChange(long CdromNum) override;
  void STDMETHODCALLTYPE PlaylistChange(
      IDispatch *Playlist, WMPPlaylistChangeEventType change) override;
  void STDMETHODCALLTYPE
  CurrentPlaylistChange(WMPPlaylistChangeEventType change) override;
  void STDMETHODCALLTYPE
  CurrentPlaylistItemAvailable(BSTR bstrItemName) override;
  void STDMETHODCALLTYPE MediaChange(IDispatch *Item) override;
  void STDMETHODCALLTYPE CurrentMediaItemAvailable(BSTR bstrItemName) override;
  void STDMETHODCALLTYPE CurrentItemChange(IDispatch *pdispMedia) override;
  void STDMETHODCALLTYPE MediaCollectionChange() override;
  void STDMETHODCALLTYPE MediaCollectionAttributeStringAdded(
      BSTR bstrAttribName, BSTR bstrAttribVal) override;
  void STDMETHODCALLTYPE MediaCollectionAttributeStringRemoved(
      BSTR bstrAttribName, BSTR bstrAttribVal) override;
  void STDMETHODCALLTYPE MediaCollectionAttributeStringChanged(
      BSTR bstrAttribName, BSTR bstrOldAttribVal,
      BSTR bstrNewAttribVal) override;
  void STDMETHODCALLTYPE PlaylistCollectionChange() override;
  void STDMETHODCALLTYPE
  PlaylistCollectionPlaylistAdded(BSTR bstrPlaylistName) override;
  void STDMETHODCALLTYPE
  PlaylistCollectionPlaylistRemoved(BSTR bstrPlaylistName) override;
  void STDMETHODCALLTYPE PlaylistCollectionPlaylistSetAsDeleted(
      BSTR bstrPlaylistName, VARIANT_BOOL varfIsDeleted) override;
  void STDMETHODCALLTYPE ModeChange(BSTR ModeName,
                                    VARIANT_BOOL NewValue) override;
  void STDMETHODCALLTYPE MediaError(IDispatch *pMediaObject) override;
  void STDMETHODCALLTYPE OpenPlaylistSwitch(IDispatch *pItem) override;
  void STDMETHODCALLTYPE DomainChange(BSTR strDomain) override;
  void STDMETHODCALLTYPE SwitchedToPlayerApplication() override;
  void STDMETHODCALLTYPE SwitchedToControl() override;
  void STDMETHODCALLTYPE PlayerDockedStateChange() override;
  void STDMETHODCALLTYPE PlayerReconnect() override;
  void STDMETHODCALLTYPE Click(short nButton, short nShiftState, long fX,
                               long fY) override;
  void STDMETHODCALLTYPE DoubleClick(short nButton, short nShiftState, long fX,
                                     long fY) override;
  void STDMETHODCALLTYPE KeyDown(short nKeyCode, short nShiftState) override;
  void STDMETHODCALLTYPE KeyPress(short nKeyAscii) override;
  void STDMETHODCALLTYPE KeyUp(short nKeyCode, short nShiftState) override;
  void STDMETHODCALLTYPE MouseDown(short nButton, short nShiftState, long fX,
                                   long fY) override;
  void STDMETHODCALLTYPE MouseMove(short nButton, short nShiftState, long fX,
                                   long fY) override;
  void STDMETHODCALLTYPE MouseUp(short nButton, short nShiftState, long fX,
                                 long fY) override;
};

using CComWMPEventDispatch = CComObject<SteamWmpEventDispatch>;

}  // namespace se::smp

#endif  // !SE_UTILS_SMP_STEAM_WMP_EVENT_DISPATCH_H_
