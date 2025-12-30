//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "choreowidgetdrawhelper.h"
#include "tier0/dbg.h"
#include "choreoview.h"
#include "choreoviewcolors.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *widget - 
//-----------------------------------------------------------------------------
CChoreoWidgetDrawHelper::CChoreoWidgetDrawHelper( mxWindow *widget )
{
	Init( widget, 0, 0, 0, 0, COLOR_CHOREO_BACKGROUND, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *widget - 
//-----------------------------------------------------------------------------
CChoreoWidgetDrawHelper::CChoreoWidgetDrawHelper( mxWindow *widget, COLORREF bgColor )
{
	Init( widget, 0, 0, 0, 0, bgColor, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *widget - 
//			bounds - 
//-----------------------------------------------------------------------------
CChoreoWidgetDrawHelper::CChoreoWidgetDrawHelper( mxWindow *widget, RECT& bounds )
{
	Init( widget, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top, COLOR_CHOREO_BACKGROUND, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *widget - 
//			bounds - 
//-----------------------------------------------------------------------------
CChoreoWidgetDrawHelper::CChoreoWidgetDrawHelper( mxWindow *widget, RECT& bounds, bool noPageFlip )
{
	Init( widget, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top, COLOR_CHOREO_BACKGROUND, noPageFlip );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *widget - 
//			x - 
//			y - 
//			w - 
//			h - 
//-----------------------------------------------------------------------------
CChoreoWidgetDrawHelper::CChoreoWidgetDrawHelper( mxWindow *widget, int x, int y, int w, int h, COLORREF bgColor )
{
	Init( widget, x, y, w, h, bgColor, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *widget - 
//			bounds - 
//			bgColor - 
//-----------------------------------------------------------------------------
CChoreoWidgetDrawHelper::CChoreoWidgetDrawHelper( mxWindow *widget, RECT& bounds, COLORREF bgColor )
{
	Init( widget, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top, bgColor, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *widget - 
//			x - 
//			y - 
//			w - 
//			h - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::Init( mxWindow *widget, int x, int y, int w, int h, COLORREF bgColor, bool noPageFlip )
{
	m_bNoPageFlip = noPageFlip;

	m_x = x;
	m_y = y;

	m_w = w ? w : widget->w2();
	m_h = h ? h : widget->h2();

	m_hWnd = (HWND)widget->getHandle();
	Assert( m_hWnd );
	m_dcReal = GetDC( m_hWnd );
	m_rcClient.left		= m_x;
	m_rcClient.top		= m_y;
	m_rcClient.right	=  m_x + m_w;
	m_rcClient.bottom	= m_y + m_h;

	if ( !noPageFlip )
	{
		m_dcMemory = CreateCompatibleDC( m_dcReal );
		m_bmMemory = CreateCompatibleBitmap( m_dcReal, m_w, m_h );
		m_bmOld = (HBITMAP)SelectObject( m_dcMemory, m_bmMemory );
	}
	else
	{
		m_dcMemory = m_dcReal;
		m_x = m_y = 0;
	}

	m_clrOld = SetBkColor( m_dcMemory, bgColor );

	RECT rcFill = m_rcClient;
	OffsetRect( &rcFill, -m_rcClient.left, -m_rcClient.top );

	if ( !noPageFlip )
	{
		HBRUSH br = CreateSolidBrush( bgColor );
		RunCodeAtScopeExit(DeleteObject( br ));

		FillRect( m_dcMemory, &rcFill, br );
	}

	m_ClipRegion = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Finish up
//-----------------------------------------------------------------------------
CChoreoWidgetDrawHelper::~CChoreoWidgetDrawHelper( void )
{
	SelectClipRgn( m_dcMemory, NULL );

	while ( m_ClipRects.Count() > 0 )
	{
		StopClipping();
	}

	if ( !m_bNoPageFlip )
	{
		BitBlt( m_dcReal, m_x, m_y, m_w, m_h, m_dcMemory, 0, 0, SRCCOPY );

		SetBkColor( m_dcMemory, m_clrOld );

		SelectObject( m_dcMemory, m_bmOld );
		DeleteObject( m_bmMemory );

		DeleteDC( m_dcMemory );
	}

	ReleaseDC( m_hWnd, m_dcReal );

	ValidateRect( m_hWnd, &m_rcClient );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoWidgetDrawHelper::GetWidth( void )
{
	return m_w;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoWidgetDrawHelper::GetHeight( void )
{
	return m_h;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rc - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::GetClientRect( RECT& rc )
{
	rc.left = rc.top = 0;
	rc.right = m_w;
	rc.bottom = m_h;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : HDC
//-----------------------------------------------------------------------------
HDC CChoreoWidgetDrawHelper::GrabDC( void )
{
	return m_dcMemory;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *font - 
//			pointsize - 
//			weight - 
//			maxwidth - 
//			rcText - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::CalcTextRect( const char *font, int pointsize, int weight, int maxwidth, RECT& rcText, const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	va_start( args, fmt );
	vprintf( fmt, args );
	V_vsprintf_safe( output, fmt, args );

	HFONT fnt = CreateFont(
		 -pointsize, 
		 0,
		 0,
		 0,
		 weight,
		 FALSE,
		 FALSE,
		 FALSE,
		 ANSI_CHARSET,
		 OUT_TT_PRECIS,
		 CLIP_DEFAULT_PRECIS,
		 CLEARTYPE_NATURAL_QUALITY,
		 DEFAULT_PITCH,
		 font );
	RunCodeAtScopeExit(DeleteObject( fnt ));

	HFONT oldFont = (HFONT)SelectObject( m_dcMemory, fnt );
	RunCodeAtScopeExit(SelectObject( m_dcMemory, oldFont ));

	DrawText( m_dcMemory, output, -1, &rcText, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_WORDBREAK | DT_CALCRECT );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *font - 
//			pointsize - 
//			weight - 
//			*fmt - 
//			... - 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoWidgetDrawHelper::CalcTextWidth( const char *font, int pointsize, int weight, const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	va_start( args, fmt );
	vprintf( fmt, args );
	V_vsprintf_safe( output, fmt, args );

	HFONT fnt = CreateFont(
		 -pointsize, 
		 0,
		 0,
		 0,
		 weight,
		 FALSE,
		 FALSE,
		 FALSE,
		 ANSI_CHARSET,
		 OUT_TT_PRECIS,
		 CLIP_DEFAULT_PRECIS,
		 CLEARTYPE_NATURAL_QUALITY,
		 DEFAULT_PITCH,
		 font );
	RunCodeAtScopeExit(DeleteObject( fnt ));

	HDC screen = GetDC( NULL );
	RunCodeAtScopeExit(ReleaseDC(nullptr, screen));

	HFONT oldFont = (HFONT)SelectObject( screen, fnt );
	RunCodeAtScopeExit(SelectObject( screen, oldFont ));

	RECT rcText;
	rcText.left = rcText.top = 0;
	rcText.bottom = pointsize + 5;
	rcText.right = rcText.left + 2048;

	DrawText( screen, output, -1, &rcText, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE | DT_CALCRECT );

	return rcText.right;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *font - 
//			pointsize - 
//			weight - 
//			*fmt - 
//			... - 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoWidgetDrawHelper::CalcTextWidthW( const char *font, int pointsize, int weight, const wchar_t *fmt, ... )
{
	va_list args;
	static wchar_t output[1024];

	va_start( args, fmt );
	vwprintf( fmt, args );
	vswprintf( output, std::size(output), fmt, args );

	HFONT fnt = CreateFont(
		 -pointsize, 
		 0,
		 0,
		 0,
		 weight,
		 FALSE,
		 FALSE,
		 FALSE,
		 ANSI_CHARSET,
		 OUT_TT_PRECIS,
		 CLIP_DEFAULT_PRECIS,
		 CLEARTYPE_NATURAL_QUALITY,
		 DEFAULT_PITCH,
		 font );
	RunCodeAtScopeExit(DeleteObject( fnt ));

	HDC screen = GetDC( NULL );
	RunCodeAtScopeExit(ReleaseDC( NULL, screen ));

	HFONT oldFont = (HFONT)SelectObject( screen, fnt );
	RunCodeAtScopeExit(SelectObject( screen, oldFont ));

	RECT rcText;
	rcText.left = rcText.top = 0;
	rcText.bottom = pointsize + 5;
	rcText.right = rcText.left + 2048;

	DrawTextW( screen, output, -1, &rcText, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE | DT_CALCRECT );

	return rcText.right;
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fnt - 
//			*fmt - 
//			... - 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoWidgetDrawHelper::CalcTextWidth( HFONT fnt, const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	va_start( args, fmt );
	vprintf( fmt, args );
	V_vsprintf_safe( output, fmt, args );

	HDC screen = GetDC( NULL );
	RunCodeAtScopeExit(ReleaseDC( NULL, screen ));

	HFONT oldFont = (HFONT)SelectObject( screen, fnt );
	RunCodeAtScopeExit(SelectObject( screen, oldFont ));

	RECT rcText;
	rcText.left = rcText.top = 0;
	rcText.bottom = 1000;
	rcText.right = rcText.left + 2048;

	DrawText( screen, output, -1, &rcText, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE | DT_CALCRECT );

	return rcText.right;
}

int CChoreoWidgetDrawHelper::CalcTextWidthW( HFONT fnt, const wchar_t *fmt, ... )
{
	va_list args;
	static wchar_t output[1024];

	va_start( args, fmt );
	vwprintf( fmt, args );
	vswprintf( output, std::size(output), fmt, args );

	HDC screen = GetDC( NULL );
	RunCodeAtScopeExit(ReleaseDC( NULL, screen ));

	HFONT oldFont = (HFONT)SelectObject( screen, fnt );
	RunCodeAtScopeExit(SelectObject( screen, oldFont ));

	RECT rcText;
	rcText.left = rcText.top = 0;
	rcText.bottom = 1000;
	rcText.right = rcText.left + 2048;

	DrawTextW( screen, output, -1, &rcText, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE | DT_CALCRECT );

	return rcText.right;
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *font - 
//			pointsize - 
//			weight - 
//			clr - 
//			rcText - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawColoredText( const char *font, int pointsize, int weight, COLORREF clr, RECT& rcText, const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	va_start( args, fmt );
	V_vsprintf_safe( output, fmt, args );
	va_end( args  );
	
	DrawColoredTextCharset( font, pointsize, weight, ANSI_CHARSET, clr, rcText, output );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *font - 
//			pointsize - 
//			weight - 
//			clr - 
//			rcText - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawColoredTextW( const char *font, int pointsize, int weight, COLORREF clr, RECT& rcText, const wchar_t *fmt, ... )
{
	va_list args;
	static wchar_t output[1024];

	va_start( args, fmt );
	vswprintf( output, std::size(output), fmt, args );
	va_end( args  );
	
	DrawColoredTextCharsetW( font, pointsize, weight, ANSI_CHARSET, clr, rcText, output );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : font - 
//			clr - 
//			rcText - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawColoredText( HFONT font, COLORREF clr, RECT& rcText, const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	va_start( args, fmt );
	V_vsprintf_safe( output, fmt, args );
	va_end( args  );
	
	HFONT oldFont = (HFONT)SelectObject( m_dcMemory, font );
	RunCodeAtScopeExit(SelectObject( m_dcMemory, oldFont ));

	COLORREF oldColor = SetTextColor( m_dcMemory, clr );
	RunCodeAtScopeExit(SetTextColor( m_dcMemory, oldColor ));

	int oldBkMode = SetBkMode( m_dcMemory, TRANSPARENT );
	RunCodeAtScopeExit(SetBkMode( m_dcMemory, oldBkMode ));

	RECT rcTextOffset = rcText;
	OffsetSubRect( rcTextOffset );

	DrawText( m_dcMemory, output, -1, &rcTextOffset, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : font - 
//			clr - 
//			rcText - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawColoredTextW( HFONT font, COLORREF clr, RECT& rcText, const wchar_t *fmt, ... )
{
	va_list args;
	static wchar_t output[1024];

	va_start( args, fmt );
	vswprintf( output, std::size(output), fmt, args );
	va_end( args  );
	
	HFONT oldFont = (HFONT)SelectObject( m_dcMemory, font );
	RunCodeAtScopeExit(SelectObject( m_dcMemory, oldFont ));

	COLORREF oldColor = SetTextColor( m_dcMemory, clr );
	RunCodeAtScopeExit(SetTextColor( m_dcMemory, oldColor ));

	int oldBkMode = SetBkMode( m_dcMemory, TRANSPARENT );
	RunCodeAtScopeExit(SetBkMode( m_dcMemory, oldBkMode ));

	RECT rcTextOffset = rcText;
	OffsetSubRect( rcTextOffset );

	DrawTextW( m_dcMemory, output, -1, &rcTextOffset, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS );
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *font - 
//			pointsize - 
//			weight - 
//			clr - 
//			rcText - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawColoredTextCharset( const char *font, int pointsize, int weight, DWORD charset, COLORREF clr, RECT& rcText, const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	va_start( args, fmt );
	V_vsprintf_safe( output, fmt, args );
	va_end( args  );

	HFONT fnt = CreateFont(
		 -pointsize, 
		 0,
		 0,
		 0,
		 weight,
		 FALSE,
		 FALSE,
		 FALSE,
		 charset,
		 OUT_TT_PRECIS,
		 CLIP_DEFAULT_PRECIS,
		 CLEARTYPE_NATURAL_QUALITY,
		 DEFAULT_PITCH,
		 font );
	RunCodeAtScopeExit(DeleteObject( fnt ));

	HFONT oldFont = (HFONT)SelectObject( m_dcMemory, fnt );
	RunCodeAtScopeExit(SelectObject( m_dcMemory, oldFont ));

	COLORREF oldColor = SetTextColor( m_dcMemory, clr );
	RunCodeAtScopeExit(SetTextColor( m_dcMemory, oldColor ));

	int oldBkMode = SetBkMode( m_dcMemory, TRANSPARENT );
	RunCodeAtScopeExit(SetBkMode( m_dcMemory, oldBkMode ));

	RECT rcTextOffset = rcText;
	OffsetSubRect( rcTextOffset );

	DrawText( m_dcMemory, output, -1, &rcTextOffset, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS );
}

void CChoreoWidgetDrawHelper::DrawColoredTextCharsetW( const char *font, int pointsize, int weight, DWORD charset, COLORREF clr, RECT& rcText, const wchar_t *fmt, ... )
{
	va_list args;
	static wchar_t output[1024];

	va_start( args, fmt );
	vswprintf( output,  std::size(output), fmt, args );
	va_end( args  );
	

	HFONT fnt = CreateFont(
		 -pointsize, 
		 0,
		 0,
		 0,
		 weight,
		 FALSE,
		 FALSE,
		 FALSE,
		 charset,
		 OUT_TT_PRECIS,
		 CLIP_DEFAULT_PRECIS,
		 CLEARTYPE_NATURAL_QUALITY,
		 DEFAULT_PITCH,
		 font );
	RunCodeAtScopeExit(DeleteObject( fnt ));

	HFONT oldFont = (HFONT)SelectObject( m_dcMemory, fnt );
	RunCodeAtScopeExit(SelectObject( m_dcMemory, oldFont ));

	COLORREF oldColor = SetTextColor( m_dcMemory, clr );
	RunCodeAtScopeExit(SetTextColor( m_dcMemory, oldColor ));

	int oldBkMode = SetBkMode( m_dcMemory, TRANSPARENT );
	RunCodeAtScopeExit(SetBkMode( m_dcMemory, oldBkMode ));

	RECT rcTextOffset = rcText;
	OffsetSubRect( rcTextOffset );

	DrawTextW( m_dcMemory, output, -1, &rcTextOffset, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *font - 
//			pointsize - 
//			weight - 
//			clr - 
//			rcText - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawColoredTextMultiline( const char *font, int pointsize, int weight, COLORREF clr, RECT& rcText, const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	va_start( args, fmt );
	vprintf( fmt, args );
	V_vsprintf_safe( output, fmt, args );

	HFONT fnt = CreateFont(
		 -pointsize, 
		 0,
		 0,
		 0,
		 weight,
		 FALSE,
		 FALSE,
		 FALSE,
		 ANSI_CHARSET,
		 OUT_TT_PRECIS,
		 CLIP_DEFAULT_PRECIS,
		 CLEARTYPE_NATURAL_QUALITY,
		 DEFAULT_PITCH,
		 font );
	RunCodeAtScopeExit(DeleteObject( fnt ));

	HFONT oldFont = (HFONT)SelectObject( m_dcMemory, fnt );
	RunCodeAtScopeExit(SelectObject( m_dcMemory, oldFont ));

	COLORREF oldColor = SetTextColor( m_dcMemory, clr );
	RunCodeAtScopeExit(SetTextColor( m_dcMemory, oldColor ));

	int oldBkMode = SetBkMode( m_dcMemory, TRANSPARENT );
	RunCodeAtScopeExit(SetBkMode( m_dcMemory, oldBkMode ));

	RECT rcTextOffset = rcText;
	OffsetSubRect( rcTextOffset );

	DrawText( m_dcMemory, output, -1, &rcTextOffset, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_WORDBREAK | DT_WORD_ELLIPSIS );
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : r - 
//			g - 
//			b - 
//			style - 
//			width - 
//			x1 - 
//			y1 - 
//			x2 - 
//			y2 - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawColoredLine( COLORREF clr, int style, int width, int x1, int y1, int x2, int y2 )
{
	HPEN pen = CreatePen( style, width, clr );
	RunCodeAtScopeExit(DeleteObject( pen ));

	HPEN oldPen = (HPEN)SelectObject( m_dcMemory, pen );
	RunCodeAtScopeExit(SelectObject( m_dcMemory, oldPen ));

	MoveToEx( m_dcMemory, x1-m_x, y1-m_y, NULL );
	LineTo( m_dcMemory, x2-m_x, y2-m_y );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : clr - 
//			style - 
//			width - 
//			count - 
//			*pts - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawColoredPolyLine( COLORREF clr, int style, int width, CUtlVector< POINT >& points )
{
	intp c = points.Count();
	if ( c < 2 )
		return;

	HPEN pen = CreatePen( style, width, clr );
	RunCodeAtScopeExit(DeleteObject( pen ));

	HPEN oldPen = (HPEN)SelectObject( m_dcMemory, pen );
	RunCodeAtScopeExit(SelectObject( m_dcMemory, oldPen ));

	POINT *temp = (POINT *)_alloca( c * sizeof( POINT ) );
	Assert( temp );
	intp i;
	for ( i = 0; i < c; i++ )
	{
		POINT *pt = &points[ i ];

		temp[ i ].x = pt->x - m_x;
		temp[ i ].y = pt->y - m_y;
	}
	
	Assert(c <= std::numeric_limits<int>::max());
	Polyline( m_dcMemory, temp, static_cast<int>(c) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : r - 
//			g - 
//			b - 
//			style - 
//			width - 
//			x1 - 
//			y1 - 
//			x2 - 
//			y2 - 
//-----------------------------------------------------------------------------
POINTL CChoreoWidgetDrawHelper::DrawColoredRamp( COLORREF clr, int style, int width, int x1, int y1, int x2, int y2, float rate, float sustain )
{
	HPEN pen = CreatePen( style, width, clr );
	RunCodeAtScopeExit(DeleteObject( pen ));

	HPEN oldPen = (HPEN)SelectObject( m_dcMemory, pen );
	RunCodeAtScopeExit(SelectObject( m_dcMemory, oldPen ));

	MoveToEx( m_dcMemory, x1-m_x, y1-m_y, NULL );
	int dx = x2 - x1;
	int dy = y2 - y1;

	POINTL p{0, 0};
	for (float i = 0.1f; i <= 1.09f; i += 0.1f)
	{
		float j = 3.0f * i * i - 2.0f * i * i * i;
		p.x = x1+(int)(dx*i*(1.0f-rate))-m_x;
		p.y = y1+(int)(dy*sustain*j)-m_y;
		LineTo( m_dcMemory, p.x, p.y );
	}

	return p;
};

//-----------------------------------------------------------------------------
// Purpose: Draw a filled rect
// Input  : clr - 
//			x1 - 
//			y1 - 
//			x2 - 
//			y2 - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawFilledRect( COLORREF clr, RECT& rc )
{
	RECT rcCopy = rc;

	HBRUSH br = CreateSolidBrush( clr );
	RunCodeAtScopeExit(DeleteObject( br ));

	OffsetSubRect( rcCopy );
	FillRect( m_dcMemory, &rcCopy, br );
}

//-----------------------------------------------------------------------------
// Purpose: Draw a filled rect
// Input  : clr - 
//			x1 - 
//			y1 - 
//			x2 - 
//			y2 - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawFilledRect( COLORREF clr, int x1, int y1, int x2, int y2 )
{
	HBRUSH br = CreateSolidBrush( clr );
	RunCodeAtScopeExit(DeleteObject( br ));

	RECT rc;
	rc.left = x1;
	rc.right = x2;
	rc.top = y1;
	rc.bottom = y2;
	OffsetSubRect( rc );
	FillRect( m_dcMemory, &rc, br );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : clr - 
//			style - 
//			width - 
//			rc - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawOutlinedRect( COLORREF clr, int style, int width, RECT& rc )
{
	DrawOutlinedRect( clr, style, width, rc.left, rc.top, rc.right, rc.bottom );
}

//-----------------------------------------------------------------------------
// Purpose: Draw an outlined rect
// Input  : clr - 
//			style - 
//			width - 
//			x1 - 
//			y1 - 
//			x2 - 
//			y2 - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawOutlinedRect( COLORREF clr, int style, int width, int x1, int y1, int x2, int y2 )
{
	HPEN pen = CreatePen( PS_SOLID, width, clr );
	RunCodeAtScopeExit(DeleteObject( pen ));

	HPEN oldpen = (HPEN)SelectObject( m_dcMemory, pen );
	RunCodeAtScopeExit(SelectObject( m_dcMemory, oldpen ));

	HBRUSH brush = (HBRUSH)GetStockObject( NULL_BRUSH );
	RunCodeAtScopeExit(DeleteObject( brush ));

	HBRUSH oldbrush = (HBRUSH)SelectObject( m_dcMemory, brush );
	RunCodeAtScopeExit(SelectObject( m_dcMemory, oldbrush ));

	RECT rc;
	rc.left = x1;
	rc.right = x2;
	rc.top = y1;
	rc.bottom = y2;
	OffsetSubRect( rc);

	Rectangle( m_dcMemory, rc.left, rc.top, rc.right, rc.bottom );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x1 - 
//			y1 - 
//			x2 - 
//			y2 - 
//			clr - 
//			thickness - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawLine( int x1, int y1, int x2, int y2, COLORREF clr, int thickness )
{
	HPEN pen = CreatePen( PS_SOLID, thickness, clr );
	RunCodeAtScopeExit(DeleteObject( pen ));

	HPEN oldpen = (HPEN)SelectObject( m_dcMemory, pen );
	RunCodeAtScopeExit(SelectObject( m_dcMemory, oldpen ));

	HBRUSH brush = (HBRUSH)GetStockObject( NULL_BRUSH );
	RunCodeAtScopeExit(DeleteObject( brush ));

	HBRUSH oldbrush = (HBRUSH)SelectObject( m_dcMemory, brush );
	RunCodeAtScopeExit(SelectObject( m_dcMemory, oldbrush ));

	// Offset
	x1 -= m_x;
	x2 -= m_x;
	y1 -= m_y;
	y2 -= m_y;

	MoveToEx( m_dcMemory, x1, y1, NULL );
	LineTo( m_dcMemory, x2, y2 );
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rc - 
//			fillr - 
//			fillg - 
//			fillb - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawTriangleMarker( RECT& rc, COLORREF fill, bool inverted /*= false*/ )
{
	POINT region[3];
	int cPoints = 3;

	if ( !inverted )
	{
		region[ 0 ].x = rc.left - m_x;
		region[ 0 ].y = rc.top - m_y;

		region[ 1 ].x = rc.right - m_x;
		region[ 1 ].y = rc.top - m_y;

		region[ 2 ].x = ( ( rc.left + rc.right ) / 2 ) - m_x;
		region[ 2 ].y = rc.bottom - m_y;
	}
	else
	{
		region[ 0 ].x = rc.left - m_x;
		region[ 0 ].y = rc.bottom - m_y;

		region[ 1 ].x = rc.right - m_x;
		region[ 1 ].y = rc.bottom - m_y;

		region[ 2 ].x = ( ( rc.left + rc.right ) / 2 ) - m_x;
		region[ 2 ].y = rc.top - m_y;
	}

	HRGN rgn = CreatePolygonRgn( region, cPoints, ALTERNATE );
	RunCodeAtScopeExit(DeleteObject( rgn ));

	int oldPF = SetPolyFillMode( m_dcMemory, ALTERNATE );
	RunCodeAtScopeExit(SetPolyFillMode( m_dcMemory, oldPF ));
	
	HBRUSH brFace = CreateSolidBrush( fill );
	RunCodeAtScopeExit(DeleteObject( brFace ));

	FillRgn( m_dcMemory, rgn, brFace );
}

void CChoreoWidgetDrawHelper::StartClipping( RECT& clipRect )
{
	RECT fixed = clipRect;
	OffsetSubRect( fixed );

	m_ClipRects.AddToTail( fixed );

	ClipToRects();
}

void CChoreoWidgetDrawHelper::StopClipping( void )
{
	Assert( m_ClipRects.Count() > 0 );
	if ( m_ClipRects.Count() <= 0 )
		return;

	m_ClipRects.Remove( m_ClipRects.Count() - 1 );

	ClipToRects();
}

void CChoreoWidgetDrawHelper::ClipToRects( void )
{
	SelectClipRgn( m_dcMemory, NULL );
	if ( m_ClipRegion )
	{
		DeleteObject( m_ClipRegion );
		m_ClipRegion = HRGN( 0 );
	}

	if ( m_ClipRects.Count() > 0 )
	{
		RECT rc = m_ClipRects[ 0 ];
		m_ClipRegion = CreateRectRgn( rc.left, rc.top, rc.right, rc.bottom );
		for ( intp i = 1; i < m_ClipRects.Count(); i++ )
		{
			RECT add = m_ClipRects[ i ];

			HRGN addIn = CreateRectRgn( add.left, add.top, add.right, add.bottom );
			RunCodeAtScopeExit(DeleteObject( addIn ));

			HRGN result = CreateRectRgn( 0, 0, 100, 100 );

			CombineRgn( result, m_ClipRegion, addIn, RGN_AND );
			
			DeleteObject( m_ClipRegion );

			m_ClipRegion = result;
		}
	}

	SelectClipRgn( m_dcMemory, m_ClipRegion );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rc - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::OffsetSubRect( RECT& rc )
{
	OffsetRect( &rc, -m_x, -m_y );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : br - 
//			rc - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawFilledRect( HBRUSH br, RECT& rc )
{
	RECT rcFill = rc;
	OffsetSubRect( rcFill );
	FillRect( m_dcMemory, &rcFill, br );
}

void CChoreoWidgetDrawHelper::DrawCircle( COLORREF clr, int x, int y, int radius, bool filled /*= true*/ )
{
	RECT rc;
	int ihalfradius = radius >> 1;

	rc.left = x - ihalfradius;
	rc.right = rc.left + 2 * ihalfradius;
	rc.top = y - ihalfradius;
	rc.bottom = y + 2 * ihalfradius - 1;

	OffsetSubRect( rc );

	HPEN pen = CreatePen( PS_SOLID, 1, clr );
	RunCodeAtScopeExit(DeleteObject( pen ));

	HBRUSH br = CreateSolidBrush( clr );
	RunCodeAtScopeExit(DeleteObject( br ));

	HPEN oldPen = (HPEN)SelectObject( m_dcMemory, pen );
	RunCodeAtScopeExit(SelectObject( m_dcMemory, oldPen ));

	HBRUSH oldBr = (HBRUSH)SelectObject( m_dcMemory, br );
	RunCodeAtScopeExit(SelectObject( m_dcMemory, oldBr ));

	if ( filled )
	{
		Ellipse( m_dcMemory, rc.left, rc.top, rc.right, rc.bottom );
	}
	else
	{
		Arc( m_dcMemory, rc.left, rc.top, rc.right, rc.bottom,
			rc.left, rc.top, rc.left, rc.top );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rc - 
//			clr1 - 
//			clr2 - 
//			vertical - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawGradientFilledRect( RECT& rc, COLORREF clr1, COLORREF clr2, bool vertical )
{
	RECT rcDraw = rc;
	OffsetRect( &rcDraw, -m_x, -m_y );

	TRIVERTEX        vert[2] ;
	GRADIENT_RECT    gradient_rect;
	vert[0].x      = rcDraw.left;
	vert[0].y      = rcDraw.top;
	vert[0].Red    = GetRValue( clr1 ) << 8;
	vert[0].Green  = GetGValue( clr1 ) << 8;
	vert[0].Blue   = GetBValue( clr1 ) << 8;
	vert[0].Alpha  = 0x0000;
	
	vert[1].x      = rcDraw.right;
	vert[1].y      = rcDraw.bottom; 
	vert[1].Red    = GetRValue( clr2 ) << 8;
	vert[1].Green  = GetGValue( clr2 ) << 8;
	vert[1].Blue   = GetBValue( clr2 ) << 8;
	vert[1].Alpha  = 0x0000;

	gradient_rect.UpperLeft  = 0;
	gradient_rect.LowerRight = 1;

	GradientFill(
		m_dcMemory,
		vert, 2,
		&gradient_rect, 1,
		vertical ? GRADIENT_FILL_RECT_V : GRADIENT_FILL_RECT_H );
}
