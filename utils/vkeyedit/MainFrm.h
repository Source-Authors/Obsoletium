// Copyright Valve Corporation, All rights reserved.
//
// MainFrm.h : interface of the CMainFrame class

#if !defined(AFX_MAINFRM_H__66B5DA3F_6D29_46CA_81AA_4A27A938BD44__INCLUDED_)
#define AFX_MAINFRM_H__66B5DA3F_6D29_46CA_81AA_4A27A938BD44__INCLUDED_

#include "windows/base_tool_bar.h"
#include "windows/base_status_bar.h"
#include "windows/dpi_wnd_behavior.h"

class CMainFrame : public CFrameWnd {
 protected:  // create from serialization only
  CMainFrame();
  DECLARE_DYNCREATE(CMainFrame)

  CSplitterWnd m_wndSplitter;

  // Attributes
 public:
  // Operations
 public:
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CMainFrame)
 public:
  virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

 protected:
  virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
  //}}AFX_VIRTUAL

  // Implementation
 public:
  virtual ~CMainFrame();

 protected:  // control bar embedded members
  CBaseStatusBar m_wndStatusBar;
  CBaseToolBar m_wndToolBar;

  se::windows::ui::CDpiWindowBehavior m_dpi_behavior;

  // Generated message map functions
 protected:
  //{{AFX_MSG(CMainFrame)
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnDestroy();
  afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
  // NOTE - the ClassWizard will add and remove member functions here.
  //    DO NOT EDIT what you see in these blocks of generated code!
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before
// the previous line.

#endif  // !defined(AFX_MAINFRM_H__66B5DA3F_6D29_46CA_81AA_4A27A938BD44__INCLUDED_)
