//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// RunMapCfgDlg.h : header file
//

#include "windows/base_dlg.h"
#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
// CRunMapCfgDlg dialog

class CRunMapCfgDlg : public CBaseDlg
{
// Construction
public:
	CRunMapCfgDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CRunMapCfgDlg)
	enum { IDD = IDD_RUNMAPCONFIGS };
	CListBox	m_cConfigurations;
	//}}AFX_DATA

	class CHammer *m_pApp;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CRunMapCfgDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	void AddSequenceToList(int iIndex, class CCommandSequence * pSeq);

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CRunMapCfgDlg)
	afx_msg void OnNew();
	afx_msg void OnRemove();
	afx_msg void OnRename();
	virtual BOOL OnInitDialog();
	afx_msg void OnCopy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
