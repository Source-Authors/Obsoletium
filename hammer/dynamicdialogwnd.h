//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// DynamicDialogWnd.h : header file
//

#include "windows/base_dlg.h"
#include "windows/base_wnd.h"

/////////////////////////////////////////////////////////////////////////////
// CDynamicDialogWnd window

class CDynamicDialogWnd : public CBaseWnd
{
// Construction
public:
	CDynamicDialogWnd(CWnd *pParent);

// Attributes
public:

// Operations
public:
	void SetDialogClass(UINT nID, CBaseDlg *pDialog);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDynamicDialogWnd)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CDynamicDialogWnd();

	// Generated message map functions
protected:
	//{{AFX_MSG(CDynamicDialogWnd)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	CBaseDlg *m_pDialog;
};

/////////////////////////////////////////////////////////////////////////////
