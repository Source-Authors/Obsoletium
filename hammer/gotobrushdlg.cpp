//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements a dialog that allows the user to type in a brush ID
//			to select the corresponding brush. Used for locating errors
//			reported by the map compile tools.
//
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "GotoBrushDlg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


BEGIN_MESSAGE_MAP(CGotoBrushDlg, CBaseDlg)
	//{{AFX_MSG_MAP(CGotoBrushDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CGotoBrushDlg::CGotoBrushDlg(CWnd *pParent)
	: CBaseDlg(CGotoBrushDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CGotoBrushDlg)
	m_nBrushID = 0;
	//}}AFX_DATA_INIT
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pDX - 
//-----------------------------------------------------------------------------
void CGotoBrushDlg::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CGotoBrushDlg)
	DDX_Text(pDX, IDC_BRUSH_NUMBER, m_nBrushID);
	//}}AFX_DATA_MAP
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGotoBrushDlg::OnOK() 
{
	__super::OnOK();
}

