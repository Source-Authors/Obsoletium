//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// OptionProperties.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "OptionProperties.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

/////////////////////////////////////////////////////////////////////////////
// COptionProperties

IMPLEMENT_DYNAMIC(COptionProperties, CBasePropertySheet)

COptionProperties::COptionProperties(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
	: CBasePropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
	DoStandardInit();
}

COptionProperties::COptionProperties(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
	: CBasePropertySheet(pszCaption, pParentWnd, iSelectPage)
{
	DoStandardInit();
}

COptionProperties::~COptionProperties()
{
}


BEGIN_MESSAGE_MAP(COptionProperties, CBasePropertySheet)
	//{{AFX_MSG_MAP(COptionProperties)
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptionProperties message handlers

void COptionProperties::DoStandardInit()
{
	AddPage(&Configs);
	AddPage(&General);
	AddPage(&View2D);
	AddPage(&View3D);
	AddPage(&Textures);
	AddPage(&Build);
}

void COptionProperties::OnClose() 
{
	__super::OnClose();
}
