//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "basedialogparams.h"
#include <mxtk/mx.h>
#include "tier0/dbg.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : self - 
//-----------------------------------------------------------------------------
void CBaseDialogParams::PositionSelf( void *self )
{
	RECT rcDlg;
	HWND dlgWindow = (HWND)self;
	GetWindowRect( dlgWindow, &rcDlg );
	
	unsigned dpi = GetDpiForWindow( dlgWindow );

	// Get relative to primary monitor instead of actual window parent
	RECT rcParent;
	rcParent.left = 0;
	rcParent.right = rcParent.left + GetSystemMetricsForDpi( SM_CXFULLSCREEN, dpi );
	rcParent.top = 0;
	rcParent.bottom = rcParent.top + GetSystemMetricsForDpi( SM_CYFULLSCREEN, dpi );

	int dialogw, dialogh;
	int parentw, parenth;
	
	parentw = rcParent.right - rcParent.left;
	parenth = rcParent.bottom - rcParent.top;
	dialogw = rcDlg.right - rcDlg.left;
	dialogh = rcDlg.bottom - rcDlg.top;
	
	int dlgleft, dlgtop;
	dlgleft = ( parentw - dialogw ) / 2;
	dlgtop = ( parenth - dialogh ) / 2;
	
	if ( m_bPositionDialog )
	{
		int top = m_nTop - dialogh - 5;
		int left = m_nLeft;
		
		MoveWindow( dlgWindow,
			left,
			top,
			dialogw,
			dialogh,
			TRUE );
	}
	else
	{
		
		MoveWindow( dlgWindow, 
			dlgleft,
			dlgtop,
			dialogw,
			dialogh,
			TRUE
			);
	}
	
}