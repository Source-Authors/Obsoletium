// Copyright Valve Corporation, All rights reserved.
//
// Vkeylistview.cpp : implementation file

#include "stdafx.h"
#include "vkeyedit.h"
#include "Vkeylistview.h"

#include "tier1/KeyValues.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CVkeylistview

IMPLEMENT_DYNCREATE(CVkeylistview, CListView)

BEGIN_MESSAGE_MAP(CVkeylistview, CListView)
  //{{AFX_MSG_MAP(CVkeylistview)
  // NOTE - the ClassWizard will add and remove mapping macros here.
  //    DO NOT EDIT what you see in these blocks of generated code !
  //}}AFX_MSG_MAP
END_MESSAGE_MAP()

CVkeylistview::CVkeylistview() = default;

CVkeylistview::~CVkeylistview() = default;

/////////////////////////////////////////////////////////////////////////////
// CVkeylistview drawing

void CVkeylistview::OnDraw(CDC*) {
  // CDocument* pDoc = GetDocument();
  // TODO: add draw code here
}

/////////////////////////////////////////////////////////////////////////////
// CVkeylistview message handlers

void CVkeylistview::OnInitialUpdate() {
  __super::OnInitialUpdate();

  CListCtrl& theList = GetListCtrl();

  theList.DeleteColumn(0);
  theList.DeleteColumn(0);

  theList.InsertColumn(0, _T("Name"), LVCFMT_LEFT, 200);
  theList.InsertColumn(1, _T("Value"), LVCFMT_LEFT, 800);

  theList.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
}

void CVkeylistview::CalcWindowRect(LPRECT lpClientRect, UINT nAdjustType) {
  // TODO: Add your specialized code here and/or call the base class

  __super::CalcWindowRect(lpClientRect, nAdjustType);
}

BOOL CVkeylistview::PreCreateWindow(CREATESTRUCT& cs) {
  // TODO: Add your specialized code here and/or call the base class

  return __super::PreCreateWindow(cs);
}

BOOL CVkeylistview::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName,
                           DWORD dwStyle, const RECT& rect, CWnd* pParentWnd,
                           UINT nID, CCreateContext* pContext) {
  // TODO: Add your specialized code here and/or call the base class

  dwStyle |= LVS_REPORT | LVS_SINGLESEL | LVS_EDITLABELS | LVS_AUTOARRANGE;

  return __super::Create(lpszClassName, lpszWindowName, dwStyle, rect,
                         pParentWnd, nID, pContext);
}

void CVkeylistview::OnUpdate(CView*, LPARAM lHint, CObject* pHint) {
  KeyValues* kv = (KeyValues*)pHint;

  if (!kv || lHint != 2) return;

  CListCtrl& theList = GetListCtrl();

  theList.DeleteAllItems();

  KeyValues* subkey = kv->GetFirstValue();

  LVITEM lvi;

  int i = 0;

  while (subkey) {
    lvi.mask = LVIF_TEXT;
    lvi.iItem = i;

    lvi.iSubItem = 0;
    lvi.pszText = (char*)subkey->GetName();
    theList.InsertItem(&lvi);

    lvi.iSubItem = 1;
    lvi.pszText = (char*)subkey->GetString();
    theList.SetItem(&lvi);

    i++;

    subkey = subkey->GetNextValue();
  }
}
