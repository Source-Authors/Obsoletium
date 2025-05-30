// Copyright Valve Corporation, All rights reserved.
//
// vkeyeditView.cpp : implementation of the CVkeyeditView class

#include "stdafx.h"
#include "vkeyedit.h"

#include "vkeyeditDoc.h"
#include "vkeyeditView.h"

#include "tier1/KeyValues.h"

#include <CommCtrl.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CVkeyeditView

IMPLEMENT_DYNCREATE(CVkeyeditView, CTreeView)

BEGIN_MESSAGE_MAP(CVkeyeditView, CTreeView)
  //{{AFX_MSG_MAP(CVkeyeditView)
  ON_NOTIFY_REFLECT(TVN_SELCHANGED, OnSelchanged)
  //}}AFX_MSG_MAP
  // Standard printing commands
  ON_COMMAND(ID_FILE_PRINT, CTreeView::OnFilePrint)
  ON_COMMAND(ID_FILE_PRINT_DIRECT, CTreeView::OnFilePrint)
  ON_COMMAND(ID_FILE_PRINT_PREVIEW, CTreeView::OnFilePrintPreview)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVkeyeditView construction/destruction

CVkeyeditView::CVkeyeditView() = default;

CVkeyeditView::~CVkeyeditView() = default;

BOOL CVkeyeditView::PreCreateWindow(CREATESTRUCT& cs) {
  // TODO: Modify the Window class or styles here by modifying
  //  the CREATESTRUCT cs

  cs.style |= TVS_HASLINES | TVS_EDITLABELS | TVS_HASBUTTONS | TVS_LINESATROOT;

  return __super::PreCreateWindow(cs);
}

/////////////////////////////////////////////////////////////////////////////
// CVkeyeditView drawing

void CVkeyeditView::OnDraw(CDC*) {
  CVkeyeditDoc* pDoc = GetDocument();
  ASSERT_VALID(pDoc);
  // TODO: add draw code for native data here
}

/////////////////////////////////////////////////////////////////////////////
// CVkeyeditView printing

BOOL CVkeyeditView::OnPreparePrinting(CPrintInfo* pInfo) {
  // default preparation
  return DoPreparePrinting(pInfo);
}

void CVkeyeditView::OnBeginPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/) {
  // TODO: add extra initialization before printing
}

void CVkeyeditView::OnEndPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/) {
  // TODO: add cleanup after printing
}

/////////////////////////////////////////////////////////////////////////////
// CVkeyeditView message handlers

void CVkeyeditView::CalcWindowRect(LPRECT lpClientRect, UINT nAdjustType) {
  // TODO: Add your specialized code here and/or call the base class

  __super::CalcWindowRect(lpClientRect, nAdjustType);
}

// Sort the item in reverse alphabetical order.
static int CALLBACK MyCompareProc(LPARAM lParam1, LPARAM lParam2, LPARAM) {
  return strcmp(((KeyValues*)(lParam1))->GetName(),
                ((KeyValues*)(lParam2))->GetName());
}

void CVkeyeditView::OnUpdate(CView*, LPARAM lHint, CObject* pHint) {
  // TODO: Add your specialized code here and/or call the base class
  CTreeCtrl& theTree = GetTreeCtrl();

  KeyValues* kv = (KeyValues*)pHint;

  if (!kv || lHint != 1) return;

  theTree.DeleteAllItems();

  while (kv) {
    InsertKeyValues(kv, TVI_ROOT);

    kv = kv->GetNextKey();
  }

  // The pointer to my tree control.
  TVSORTCB tvs;
  // Sort the tree control's items using my
  // callback procedure.
  tvs.hParent = TVI_ROOT;
  tvs.lpfnCompare = MyCompareProc;
  tvs.lParam = (LPARAM)&theTree;

  theTree.SortChildrenCB(&tvs);
}

bool CVkeyeditView::InsertKeyValues(KeyValues* kv, HTREEITEM hParent) {
  CTreeCtrl& theTree = GetTreeCtrl();

  TVINSERTSTRUCT tvInsert;
  tvInsert.hParent = hParent;
  tvInsert.hInsertAfter = TVI_LAST;
  tvInsert.item.mask = TVIF_TEXT;
  tvInsert.item.lParam = (LPARAM)kv;
  tvInsert.item.pszText = (char*)kv->GetName();

  HTREEITEM hItem = theTree.InsertItem(&tvInsert);

  theTree.SetItemData(hItem, (DWORD_PTR)kv);

  KeyValues* subkey = kv->GetFirstTrueSubKey();

  while (subkey) {
    InsertKeyValues(subkey, hItem);
    subkey = subkey->GetNextKey();
  }

  // The pointer to my tree control.
  TVSORTCB tvs;
  // Sort the tree control's items using my
  // callback procedure.
  tvs.hParent = hParent;
  tvs.lpfnCompare = MyCompareProc;
  tvs.lParam = (LPARAM)&theTree;

  theTree.SortChildrenCB(&tvs);

  return true;
}

void CVkeyeditView::OnSelchanged(NMHDR* pNMHDR, LRESULT* pResult) {
  NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
  CTreeCtrl& theTree = this->GetTreeCtrl();

  HTREEITEM hItem = pNMTreeView->itemNew.hItem;

  GetDocument()->UpdateAllViews(this, 2, (CObject*)theTree.GetItemData(hItem));

  *pResult = 0;
}
