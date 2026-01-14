//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the angle custom control, a circle with a line indicating
//			a rotation angle.
//
//=============================================================================//

#include "stdafx.h"
#include "AngleBox.h"
#include "hammer.h"
#include "hammer_mathlib.h"
#include "CustomMessages.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



BEGIN_MESSAGE_MAP(CAngleBox, CBaseWnd)
	//{{AFX_MSG_MAP(CAngleBox)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDOWN()
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CAngleBox::CAngleBox(void)
{
	m_bDifferent = false;
	m_bDragging = false;
	m_vecAngles.Init();
	m_pEdit = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CAngleBox::~CAngleBox()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CAngleBox::OnMouseMove(UINT nFlags, CPoint point) 
{
	if (m_bDragging)
	{
		//
		// Remove old angle line by redrawing it (XOR).
		//
		DrawAngleLine(&m_DragDC);

		//
		// Calculate new yaw.
		//
		// dimhotepus: int -> float.
		float nNewYaw = fixang(180 - (int)lineangle(size_cast<float>(point.x), size_cast<float>(point.y), size_cast<float>(m_ptClientCenter.x), size_cast<float>(m_ptClientCenter.y)));
		m_vecAngles.Init();
		m_vecAngles[YAW] = nNewYaw;
		
		//
		// Draw the new angle line.
		//
		DrawAngleLine(&m_DragDC);
	}

	__super::OnMouseMove(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CAngleBox::OnLButtonUp(UINT nFlags, CPoint point) 
{
	// release dc
	if (m_bDragging)
	{
		::ReleaseDC(m_hWnd, m_DragDC.Detach());
		m_bDragging = false;
		ReleaseCapture();

		//
		// They've explicity set the angles, so clear the different flag for
		// the multiselect case.
		//
		SetDifferent(false);

		GetParent()->PostMessage(ABN_CHANGED, GetDlgCtrlID(), 0);
	}
		
	__super::OnLButtonUp(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CAngleBox::OnLButtonDown(UINT nFlags, CPoint point) 
{
	//
	// Start dragging.
	//
	m_DragDC.Attach(::GetDC(m_hWnd));
	m_bDragging = true;
	SetCapture();

	__super::OnLButtonDown(nFlags, point);

	OnMouseMove(0, point);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pDC - 
//-----------------------------------------------------------------------------
void CAngleBox::DrawAngleLine(CDC *pDC)
{
	if ((m_vecAngles[PITCH] != 0) || (m_vecAngles[ROLL] != 0) ||
		(m_vecAngles[YAW] < 0 || m_vecAngles[YAW] > 359) ||	m_bDifferent)
	{
		return;
	}

	int oldRop2 = pDC->SetROP2(R2_XORPEN);
	RunCodeAtScopeExit(pDC->SetROP2(oldRop2));

	CGdiObject *pOldObj = pDC->SelectStockObject(WHITE_PEN);
	RunCodeAtScopeExit(pDC->SelectObject(pOldObj));

	CRect r;
	GetClientRect(r);
	m_ptClientCenter = r.CenterPoint();

	float rad = size_cast<float>(r.Width() / 2 - 3);

	float fSin, fCos;
	DirectX::XMScalarSinCos(&fSin, &fCos, DEG2RAD(m_vecAngles[YAW] + 90));

	CPoint pt;
	pt.x = size_cast<LONG>(m_ptClientCenter.x + fSin * rad + 0.5f);
	pt.y = size_cast<LONG>(m_ptClientCenter.y + fCos * rad + 0.5f);

	pDC->MoveTo(m_ptClientCenter);
	pDC->LineTo(pt);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the current state of the control as a keyvalue string.
// Input  : szAngles - Buffer to receive angles string.
// Output : Returns 'szAngles'.
//-----------------------------------------------------------------------------
bool CAngleBox::GetAngles(QAngle &vecAngles)
{
	if (m_bDifferent)
	{
		return false;
	}

	vecAngles = m_vecAngles;

	return(true);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the current state of the control as a keyvalue string.
// Input  : szAngles - Buffer to receive angles string.
// Output : Returns 'szAngles'.
//-----------------------------------------------------------------------------
char *CAngleBox::GetAngles(char *szAngles, intp size)
{
	QAngle vecAngles;
	GetAngles(vecAngles);
	V_snprintf(szAngles, size, "%g %g %g", (double)vecAngles[0], (double)vecAngles[1], (double)vecAngles[2]);
	return(szAngles);
}


//-----------------------------------------------------------------------------
// Purpose: Returns a string indicating the current state of the angle control.
//			This is used for setting the text in the companion edit control.
// Input  : szBuf - Buffer to receive string.
//-----------------------------------------------------------------------------
char *CAngleBox::GetAngleEditText(char *szBuf, ptrdiff_t len)
{
	if (len > 0)
		szBuf[0] = '\0';

	if (m_bDifferent)
	{
		V_strncpy(szBuf, "(diff)", len);
	}
	else if ((m_vecAngles[PITCH] == 90) && (m_vecAngles[YAW] == 0) && (m_vecAngles[ROLL] == 0))
	{
		V_strncpy(szBuf, "Down", len);
	}
	else if ((m_vecAngles[PITCH] == -90) && (m_vecAngles[YAW] == 0) && (m_vecAngles[ROLL] == 0))
	{
		V_strncpy(szBuf, "Up", len);
	}
	else if (m_vecAngles[YAW] >= 0)
	{
		itoa((int)m_vecAngles[YAW], szBuf, 10);
	}

	return(szBuf);
}


//-----------------------------------------------------------------------------
// Purpose: Called internally and by the linked combo box, this updates the angles
//			without updating the linked combo box.
// Input  : szAngles - 
//			bRedraw - 
//-----------------------------------------------------------------------------
void CAngleBox::SetAnglesInternal(const QAngle &vecAngles, bool bRedraw)
{
	QAngle vecAngleSet = vecAngles;
	while (vecAngleSet[YAW] < 0)
	{
		vecAngleSet[YAW] += 360.0;
	}

	CDC *pDC = NULL;

	if (bRedraw)
	{
		//
		// Erase the old line.
		//
		Assert(::IsWindow(m_hWnd));
		pDC = GetDC();
		if (pDC != NULL)
		{
			DrawAngleLine(pDC);
		}
	}

	//
	// Update the data member.
	//
	m_vecAngles = vecAngleSet;

	if ((bRedraw) && (pDC != NULL))
	{
		//
		// Draw the new line.
		//
		DrawAngleLine(pDC);
		ReleaseDC(pDC);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called from the client code, this sets our angles and updates the
//			linked combo box.
// Input  : szAngles - 
//			bRedraw - 
//-----------------------------------------------------------------------------
void CAngleBox::SetAngles(const QAngle &vecAngles, bool bRedraw)
{
	SetAnglesInternal(vecAngles, bRedraw);
	UpdateAngleEditText();
}


//-----------------------------------------------------------------------------
// Purpose: Called from the client code, this sets our angles via a string and
//			updates the linked combo box.
// Input  : szAngles - 
//			bRedraw - 
//-----------------------------------------------------------------------------
void CAngleBox::SetAngles(const char *szAngles, bool bRedraw)
{
	QAngle vecAngles(0, 0, 0);
	[[maybe_unused]] const int scanned =
		sscanf(szAngles, "%f %f %f", &vecAngles[PITCH], &vecAngles[YAW], &vecAngles[ROLL]);
	Assert(scanned == 3);
	SetAngles(vecAngles, bRedraw);
}


//-----------------------------------------------------------------------------
// Purpose: Called internally and by the linked combo box, this sets our
//			'different' state without updating the linked combo box.
// Input  : bDifferent - 
//			bRedraw - 
//-----------------------------------------------------------------------------
void CAngleBox::SetDifferentInternal(bool bDifferent, bool bRedraw)
{
	CDC *pDC = NULL;

	if (bRedraw)
	{
		//
		// Erase the old line.
		//
		Assert(::IsWindow(m_hWnd));
		pDC = GetDC();
		if (pDC != NULL)
		{
			DrawAngleLine(pDC);
		}
	}

	//
	// Update the data member.
	//
	m_bDifferent = bDifferent;

	if ((bRedraw) && (pDC != NULL))
	{
		//
		// Draw the new line.
		//
		DrawAngleLine(pDC);
		ReleaseDC(pDC);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets our state to indicate multiselect of objects with different
//			angles to avoid mucking with the angles unless they explicitly set
//			them to something new.
//-----------------------------------------------------------------------------
void CAngleBox::SetDifferent(bool bDifferent, bool bRedraw)
{
	SetDifferentInternal(bDifferent, bRedraw);
	UpdateAngleEditText();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAngleBox::OnPaint(void)
{
	PAINTSTRUCT ps;
	CDC *pDC = BeginPaint(&ps);
	
	if (pDC == NULL)
	{
		return;
	}

	RunCodeAtScopeExit(EndPaint(&ps));

	CBrush brushWindow(GetSysColor(COLOR_3DFACE));
	CBrush brushBlack(RGB(0, 0, 0));

	CBrush *pBackBrush = IsWindowEnabled() ? &brushBlack : &brushWindow;

	CRect r;
	GetClientRect(r);

	//
	// Fill with the window color.
	//
	pDC->FillRect(&r, &brushWindow);

	//
	// Draw a 3D circle.
	//
	m_ptClientCenter = r.CenterPoint();
	
	CGdiObject *pOldObj = pDC->SelectStockObject(NULL_PEN);
	RunCodeAtScopeExit(pDC->SelectObject(pOldObj));

	CBrush *oldBr1 = pDC->SelectObject(pBackBrush);
	RunCodeAtScopeExit(pDC->SelectObject(oldBr1));

	pDC->Ellipse(r);

	CPen hi(PS_SOLID, 2, GetSysColor(COLOR_3DSHADOW));
	CPen lo(PS_SOLID, 2, GetSysColor(COLOR_3DHILIGHT));

	{
		HGDIOBJ oldHi = pDC->SelectObject(hi);
		RunCodeAtScopeExit(pDC->SelectObject(oldHi));

		pDC->Arc(r, CPoint(r.right, r.top), CPoint(r.left, r.bottom));
	}

	{
		HGDIOBJ oldLo = pDC->SelectObject(lo);
		RunCodeAtScopeExit(pDC->SelectObject(oldLo));

		pDC->Arc(r, CPoint(r.left, r.bottom), CPoint(r.right, r.top));
	}

	//
	// Draw center point.
	//
	pDC->SetPixel(m_ptClientCenter, RGB(0xff, 0xff, 0xff));

	//
	// Draw line indicating angles direction.
	//
	if (IsWindowEnabled())
	{
		DrawAngleLine(pDC);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Enables or disables the angles controls.
//-----------------------------------------------------------------------------
void CAngleBox::Enable(bool bEnable)
{
	if (bEnable)
	{
		EnableWindow(TRUE);

		if (m_pEdit)
		{
			m_pEdit->EnableWindow(TRUE);
		}
	}
	else
	{
		EnableWindow(FALSE);
		
		if (m_pEdit)
		{
			m_pEdit->EnableWindow(FALSE);
		}
	}

	Invalidate(FALSE);
	UpdateWindow();
}


//-----------------------------------------------------------------------------
// Purpose: Hides or shows the angles controls.
//-----------------------------------------------------------------------------
void CAngleBox::Show(bool bShow)
{
	if (bShow)
	{
		ShowWindow(SW_SHOW);
		
		if (m_pEdit)
		{
			m_pEdit->ShowWindow(SW_SHOW);
		}
	}
	else
	{
		ShowWindow(SW_HIDE);

		if (m_pEdit)
		{
			m_pEdit->ShowWindow(SW_HIDE);
		}
	}

	Invalidate(FALSE);
	UpdateWindow();
}


//-----------------------------------------------------------------------------
// Purpose: Updates the text in the angle combo to reflect the current angles
//			in the angles control.
//-----------------------------------------------------------------------------
void CAngleBox::UpdateAngleEditText(void)
{
	if (m_pEdit)
	{
		char szBuf[20];
		GetAngleEditText(szBuf, ssize(szBuf));
		m_pEdit->SetAnglesInternal(szBuf);
	}
}


BEGIN_MESSAGE_MAP(CAngleCombo, CWnd)
	//{{AFX_MSG_MAP(CAngleBox)
	ON_CONTROL_REFLECT(CBN_EDITCHANGE, OnChangeAngleEdit)
	ON_CONTROL_REFLECT(CBN_SELENDOK, OnSelChangeAngleEdit)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Construktor.
//-----------------------------------------------------------------------------
CAngleCombo::CAngleCombo()
	: CComboBox()
{
	m_pBox = NULL;
	m_bEnableUpdate = true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szAngles - 
//-----------------------------------------------------------------------------
void CAngleCombo::SetAnglesInternal(const char *szAngles)
{
	m_bEnableUpdate = false;
	SetWindowText(szAngles);
	m_bEnableUpdate = true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles a change in the contents of the angle edit control.
//-----------------------------------------------------------------------------
void CAngleCombo::OnChangeAngleEdit(void) 
{
	if (m_bEnableUpdate)
	{
		char buf[64];
		GetWindowText(buf, 64);
		UpdateAngleBox(buf);

		GetParent()->PostMessage(ABN_CHANGED, GetDlgCtrlID(), 0);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handles a change in the current selection of the angle edit combo.
//-----------------------------------------------------------------------------
void CAngleCombo::OnSelChangeAngleEdit(void) 
{
	char buf[64];
	int nSel = GetCurSel();
	GetLBText(nSel, buf);
	UpdateAngleBox(buf);

	GetParent()->PostMessage(ABN_CHANGED, GetDlgCtrlID(), 0);
}


//-----------------------------------------------------------------------------
// Purpose: Updates angle box with the settings from the combo box. Call the
//			internal functions so we don't get a reflected notification, mucking
//			up our state.
//-----------------------------------------------------------------------------
void CAngleCombo::UpdateAngleBox(char *szText)
{
	if (m_pBox)
	{
		m_pBox->SetDifferentInternal(false);

		if (V_isdigit(szText[0]))
		{
			// dimhotepus: atoi -> V_atof.
			QAngle vecAngles(0, V_atof(szText), 0);
			m_pBox->SetAnglesInternal(vecAngles, true);
		}
		else if (!stricmp(szText, "down"))
		{
			m_pBox->SetAnglesInternal(QAngle(90, 0, 0), true);
		}
		else
		{
			m_pBox->SetAnglesInternal(QAngle(-90, 0, 0), true);
		}
	}
}

