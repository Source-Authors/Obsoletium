//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include <fstream>

#include "hammer.h"
#include "lprvwindow.h"
#include "TextureBrowser.h"
#include "CustomMessages.h"
#include "IEditorTexture.h"
#include "GameConfig.h"
#include "GlobalFunctions.h"
#include "TextureSystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "lpreview_thread.h"
#include "MainFrm.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>



BEGIN_MESSAGE_MAP(CLightingPreviewResultsWindow, CBaseWnd)
	//{{AFX_MSG_MAP(CTextureWindow)
	ON_WM_PAINT()
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes data members.
//-----------------------------------------------------------------------------
CLightingPreviewResultsWindow::CLightingPreviewResultsWindow()
{
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CLightingPreviewResultsWindow::~CLightingPreviewResultsWindow()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParentWnd - 
//			rect - 
//-----------------------------------------------------------------------------
void CLightingPreviewResultsWindow::Create( CWnd *pParentWnd, const char *pszTitle )
{
	static CString wndClassName;

	if (wndClassName.IsEmpty())
	{
		// create class
		wndClassName = AfxRegisterWndClass(
			CS_DBLCLKS | CS_HREDRAW | 
			CS_VREDRAW, LoadCursor(NULL, IDC_ARROW),
			(HBRUSH) GetStockObject(BLACK_BRUSH), NULL);
	}

	CString format;
	format.Format("Lighting Preview - %s", pszTitle);

	// dimhotepus: Scale manually by parent DPI as we are not created yet.
	se::windows::ui::CDpiWindowBehavior dpiWindowBehavior{false};

	dpiWindowBehavior.OnCreateWindow(pParentWnd->GetSafeHwnd());
	RunCodeAtScopeExit(dpiWindowBehavior.OnDestroyWindow());

	CRect rect{dpiWindowBehavior.ScaleOnX(500), dpiWindowBehavior.ScaleOnY(500),
		dpiWindowBehavior.ScaleOnX(600), dpiWindowBehavior.ScaleOnY(600)};

	__super::CreateEx(0, wndClassName, format.GetString(),
				   WS_OVERLAPPEDWINDOW|WS_SIZEBOX,
				   rect, NULL, NULL,NULL);
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLightingPreviewResultsWindow::OnClose()
{
	GetMainWnd()->GlobalNotify(LPRV_WINDOWCLOSED);
	CWnd::OnClose();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLightingPreviewResultsWindow::OnPaint()
{
	// dimhotepus: Exit when nothing to paint.
	if ( !g_pLPreviewOutputBitmap ) return;

	CPaintDC dc(this); // device context for painting

	const int width{g_pLPreviewOutputBitmap->Width()};
	const int height{g_pLPreviewOutputBitmap->Height()};

	// blit it
	BITMAPINFOHEADER mybmh = {};
	mybmh.biSize = sizeof(BITMAPINFOHEADER);
	mybmh.biHeight = -height;
	// now, set up bitmapheader struct for StretchDIB
	mybmh.biWidth=width;
	mybmh.biPlanes=1;
	mybmh.biBitCount=32;
	mybmh.biCompression=BI_RGB;
	mybmh.biSizeImage=width * height;

	CRect clientrect;
	GetClientRect(clientrect);

	StretchDIBits(
		dc.GetSafeHdc(),
		clientrect.left,
		clientrect.top,
		1+clientrect.Width(),
		1+clientrect.Height(),
		0,
		0,
		width,
		height,
		g_pLPreviewOutputBitmap->GetBits(),
		(BITMAPINFO *) &mybmh,
		DIB_RGB_COLORS,
		SRCCOPY);
}
