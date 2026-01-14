//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// StrDlg.cpp : implementation file
//

#include "stdafx.h"
#include "StrDlg.h"
#include "hammer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

/////////////////////////////////////////////////////////////////////////////
// CStrDlg dialog

CStrDlg::CStrDlg(DWORD dwFlags, LPCTSTR pszString, LPCTSTR pszPrompt, 
				 LPCTSTR pszTitle)
	: CBaseDlg(CStrDlg::IDD, NULL)
{
	//{{AFX_DATA_INIT(CStrDlg)
	m_string = _T("");
	//}}AFX_DATA_INIT

	iRangeLow = 0;
	iRangeHigh = 10;
	iIncrement = 1;
	this->dwFlags = dwFlags;

	m_string = pszString;
	m_strPrompt = pszPrompt;
	m_strTitle = pszTitle;
}

void CStrDlg::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_PROMPT, m_strPrompt);
	DDX_Text(pDX, IDC_EDIT, m_string);
	DDX_Control(pDX, IDC_EDIT, m_cEdit);
	DDX_Control(pDX, IDC_SPIN, m_cSpin);
	//{{AFX_DATA_MAP(CStrDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CStrDlg, CBaseDlg)
	//{{AFX_MSG_MAP(CStrDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CStrDlg message handlers

void CStrDlg::SetRange(int iLow, int iHigh, int iIncrement_)
{
	iRangeLow = iLow;
	iRangeHigh = iHigh;
	this->iIncrement = 1;
}

BOOL CStrDlg::OnInitDialog() 
{
	__super::OnInitDialog();

	if(dwFlags & Spin)	// enable spin
	{
		m_cSpin.EnableWindow(TRUE);
		m_cSpin.SetRange(size_cast<short>(iRangeLow), size_cast<short>(iRangeHigh));
		m_cSpin.SetBuddy(&m_cEdit);
	}
	else
	{
		m_cSpin.ShowWindow(SW_HIDE);
	}

	SetWindowText(m_strTitle);

	return TRUE;
}
