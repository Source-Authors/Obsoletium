//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "mxbitmaptools.h"
#include <mxtk/mx.h>
#include "hlfaceposer.h"
#include "filesystem.h"

bool LoadBitmapFromFile( const char *relative, mxbitmapdata_t& bitmap )
{
	bitmap.valid = false;
	bitmap.image = NULL;
	bitmap.width = -1;
	bitmap.height = -1;

	// Draw
	HDC dc = GetDC( NULL );
	if ( !dc )
	{
		return false;
	}
	RunCodeAtScopeExit(ReleaseDC( NULL, dc ));

	int width, height;

	width	= 100;
	height	= 100;

	HBITMAP  bmNewImage = nullptr;

	HBITMAP bm = CreateCompatibleBitmap( dc, width, height );
	if ( bm )
	{
		RunCodeAtScopeExit(DeleteObject( bm ));

		HBITMAP oldbm = (HBITMAP)SelectObject( dc, bm );
		RunCodeAtScopeExit(SelectObject( dc, oldbm ));
		
		HDC memdc = CreateCompatibleDC( dc );
		if ( memdc )
		{
			RunCodeAtScopeExit(DeleteDC( memdc ));

			char filename[ 512 ];
			filesystem->RelativePathToFullPath_safe( relative, "MOD", filename );

			bmNewImage = (HBITMAP)LoadImage( 
				(HINSTANCE) GetModuleHandle(0), filename,
				IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE );
			if ( !bmNewImage )
			{
				filesystem->RelativePathToFullPath_safe( relative, "GAME", filename );

				bmNewImage = (HBITMAP)LoadImage( 
					(HINSTANCE) GetModuleHandle(0), filename,
					IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE );
			}
			
			if ( bmNewImage )
			{
				HBITMAP oldmembm = (HBITMAP)SelectObject( memdc, bmNewImage );
				RunCodeAtScopeExit(SelectObject( memdc, oldmembm ));
				
				BITMAPINFO bmi = {};
				bmi.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
				
				if ( GetDIBits( memdc, bmNewImage, 0, 0, NULL, &bmi, DIB_RGB_COLORS ) )
				{
					bitmap.width = bmi.bmiHeader.biWidth;
					bitmap.height = bmi.bmiHeader.biHeight;
				}
			}
		}
	}

	if ( bmNewImage && 
		bitmap.width != -1 && 
		bitmap.height != -1 )
	{
		bitmap.image = bmNewImage;
		bitmap.valid = true;
	}

	return bitmap.valid;
}

void DrawBitmapToWindow( mxWindow *wnd, int x, int y, int w, int h, mxbitmapdata_t& bitmap )
{
	if ( !bitmap.valid )
		return;

	{
		// Draw
		HDC dc = GetDC( (HWND) wnd->getHandle() );
		if ( !dc )
			return;
		RunCodeAtScopeExit(ReleaseDC( (HWND) wnd->getHandle(), dc ));

		HBITMAP bm = CreateCompatibleBitmap( dc, w, h );
		RunCodeAtScopeExit(DeleteObject( bm ));

		HBITMAP oldbm = (HBITMAP)SelectObject( dc, bm );
		RunCodeAtScopeExit(SelectObject( dc, oldbm ));

		HDC memdc = CreateCompatibleDC( dc );
		RunCodeAtScopeExit(DeleteDC( memdc ));

		HBITMAP oldmembm = (HBITMAP)SelectObject( memdc, bitmap.image );
		RunCodeAtScopeExit(SelectObject( memdc, oldmembm ));

		int oldmode = SetStretchBltMode( dc, COLORONCOLOR );
		RunCodeAtScopeExit(SetStretchBltMode( dc, oldmode ));

		StretchBlt( dc, x, y, w, h, memdc, 0, 0, bitmap.width, bitmap.height, SRCCOPY );
	}

	RECT rc;
	rc.left = x;
	rc.right = x + w;
	rc.top = y;
	rc.bottom = y + h;

	ValidateRect( (HWND)wnd->getHandle(), &rc );
}

void DrawBitmapToDC( void *hdc, int x, int y, int w, int h, mxbitmapdata_t& bitmap )
{
	if ( !bitmap.valid )
		return;

	HDC dc = (HDC)hdc;

	HDC memdc = CreateCompatibleDC( dc );
	HBITMAP oldmembm = (HBITMAP)SelectObject( memdc, bitmap.image );

	int oldmode = SetStretchBltMode( dc, COLORONCOLOR );

	StretchBlt( dc, x, y, w, h, memdc, 0, 0, bitmap.width, bitmap.height, SRCCOPY );

	SetStretchBltMode( dc, oldmode );

	SelectObject( memdc, oldmembm );
	DeleteDC( memdc );
}