//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include "hammer.h"
#include "EditPathDlg.h"
#include "GameConfig.h"
#include "fgdlib/GameData.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


CEditPathDlg::CEditPathDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CEditPathDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CEditPathDlg)
	m_strClass = _T("");
	m_iDirection = -1;
	m_strName = _T("");
	//}}AFX_DATA_INIT
}


void CEditPathDlg::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CEditPathDlg)
	DDX_Control(pDX, IDC_CLASS, m_cClass);
	DDX_CBString(pDX, IDC_CLASS, m_strClass);
	DDX_Radio(pDX, IDC_LOOP, m_iDirection);
	DDX_Text(pDX, IDC_NAME, m_strName);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CEditPathDlg, CBaseDlg)
	//{{AFX_MSG_MAP(CEditPathDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


BOOL CEditPathDlg::OnInitDialog() 
{
	__super::OnInitDialog();

	CString strCurrent;
	m_cClass.GetWindowText(strCurrent);

	// add class list to combo box
	// setup class list
	m_cClass.SetRedraw(FALSE);
	m_cClass.ResetContent();

	CString str;
	intp nCount = pGD->GetClassCount();
	for (intp i = 0; i < nCount; i++)
	{
		GDclass *pc = pGD->GetClass(i);
		if (!pc->IsBaseClass())
		{
			str = pc->GetName();
			if (!pc->IsClass("worldspawn"))
			{
				m_cClass.AddString(str);
			}
		}
	}
	m_cClass.SetRedraw(TRUE);
	m_cClass.SetWindowText(strCurrent);

	m_cClass.Invalidate();

	return TRUE;
}
