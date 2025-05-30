// Copyright Valve Corporation, All rights reserved.
//
// MainFrm.cpp : implementation of the CMainFrame class

#include "stdafx.h"

#include "vkeyedit.h"
#include "vkeyeditView.h"
#include "Vkeylistview.h"

#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
  //{{AFX_MSG_MAP(CMainFrame)
  // NOTE - the ClassWizard will add and remove mapping macros here.
  //    DO NOT EDIT what you see in these blocks of generated code !
  ON_WM_CREATE()
  ON_WM_DESTROY()
  ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
  //}}AFX_MSG_MAP
END_MESSAGE_MAP()

static constexpr UINT indicators[] = {
    ID_SEPARATOR,  // status line indicator
    ID_INDICATOR_CAPS,
    ID_INDICATOR_NUM,
    ID_INDICATOR_SCRL,
};

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame() = default;

CMainFrame::~CMainFrame() = default;

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (__super::OnCreate(lpCreateStruct) == -1) return -1;

  if (!m_wndToolBar.CreateEx(this, TBSTYLE_FLAT,
                             WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_GRIPPER |
                                 CBRS_TOOLTIPS | CBRS_FLYBY |
                                 CBRS_SIZE_DYNAMIC) ||
      !m_wndToolBar.LoadToolBarForDpi(SRC_IDI_APP_MAIN)) {
    TRACE0("Failed to create toolbar\n");
    return -1;  // fail to create
  }

  if (!m_wndStatusBar.Create(this) ||
      !m_wndStatusBar.SetIndicators(indicators,
                                    sizeof(indicators) / sizeof(UINT))) {
    TRACE0("Failed to create status bar\n");
    return -1;  // fail to create
  }

  // TODO: Delete these three lines if you don't want the toolbar to
  //  be dockable
  m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
  EnableDocking(CBRS_ALIGN_ANY);
  DockControlBar(&m_wndToolBar);

  return 0;
}

void CMainFrame::OnDestroy() {
  m_dpi_behavior.OnDestroyWindow();

  __super::OnDestroy();
}

LRESULT CMainFrame::OnDpiChanged(WPARAM wParam, LPARAM lParam) {
  const LRESULT rc{m_dpi_behavior.OnWindowDpiChanged(wParam, lParam)};

  return rc;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs) {
  if (!CFrameWnd::PreCreateWindow(cs)) return FALSE;

  return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers

BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT, CCreateContext* pContext) {
  if (!m_dpi_behavior.OnCreateWindow(m_hWnd)) return FALSE;

  // create splitter window
  if (!m_wndSplitter.CreateStatic(this, 1, 2)) return FALSE;

  if (!m_wndSplitter.CreateView(
          0, 0, RUNTIME_CLASS(CVkeyeditView),
          CSize(m_dpi_behavior.ScaleOnX(200), m_dpi_behavior.ScaleOnY(100)),
          pContext) ||
      !m_wndSplitter.CreateView(
          0, 1, RUNTIME_CLASS(CVkeylistview),
          CSize(m_dpi_behavior.ScaleOnX(100), m_dpi_behavior.ScaleOnY(100)),
          pContext)) {
    m_wndSplitter.DestroyWindow();
    return FALSE;
  }

  return TRUE;
}
