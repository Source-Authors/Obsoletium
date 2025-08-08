//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// ProcessWnd.h : header file
//

#include "windows/base_wnd.h"
/////////////////////////////////////////////////////////////////////////////
// CProcessWnd window

class CProcessWnd : public CBaseWnd
{
// Construction
public:
	CProcessWnd();

// Attributes
public:
	// pipes:
	char *pEditBuf;
	UINT uBufLen;

	CEdit Edit;
	CFont Font;

// Operations
public:
	int Execute(LPCTSTR pszCmd, LPCTSTR pszCmdLine);
	int Execute(PRINTF_FORMAT_STRING LPCTSTR pszCmd, ...);

	void Clear();
	void Append(CString str);
	void GetReady(LPCTSTR pszDocName, CWnd *parent);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CProcessWnd)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CProcessWnd();

	// Generated message map functions
protected:

	BOOL PreTranslateMessage(MSG* pMsg);

	CString m_EditText;
	CButton m_btnCopyAll;
	
	// Need additional one as controls created dynamically after DPI is applied.
	se::windows::ui::CDpiWindowBehavior m_edit_dpi_behavior{false};
	se::windows::ui::CDpiWindowBehavior m_copy_all_dpi_behavior{false};

	//{{AFX_MSG(CProcessWnd)
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnCopyAll();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
