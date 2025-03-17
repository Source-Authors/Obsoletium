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
#include "windows/dpi_aware_font.h"

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
	DpiAwareFont *pFont;

// Operations
public:
	int Execute(LPCTSTR pszCmd, LPCTSTR pszCmdLine);
	int Execute(PRINTF_FORMAT_STRING LPCTSTR pszCmd, ...);

	void Clear();
	void Append(CString str);
	DpiAwareFont* CreateFont();
	void GetReady(LPCTSTR pszDocName);

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

	//{{AFX_MSG(CProcessWnd)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnCopyAll();
	afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
