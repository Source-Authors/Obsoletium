//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#define WIN32_LEAN_AND_MEAN
#define OEMRESOURCE

// SRC only
#define PROTECTED_THINGS_DISABLE

#include "winlite.h"
#include <imm.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <oleidl.h>

#include <vgui/VGUI.h>
#include <vgui/Dar.h>
#include <vgui/IClientPanel.h>
#include <vgui/ISurface.h>
#include <vgui/IInputInternal.h>
#include <vgui/IPanel.h>
#include <vgui/ISystem.h>
#include <vgui/ILocalize.h>
#include <vgui/IHTML.h>
#include <vgui/IVGui.h>
#include <vgui/IScheme.h>

#include <vgui/Cursor.h>
#include <vgui/KeyCode.h>
#include <vgui/MouseCode.h>

#include "vgui_internal.h"
#include "bitmap.h"
#include "VPanel.h"

#include <tier1/KeyValues.h>
#include "tier1/utlvector.h"
#include "tier1/utlsymbol.h"
#include "tier1/utldict.h"
#include "tier0/basetypes.h"

#include "filesystem.h"
#include "qlimits.h"
#include "SteamBootStrapper.h"

#include "vgui_surfacelib/Win32Font.h"
#include "vgui_surfacelib/FontManager.h"
#include "vgui_key_translation.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef PlaySound
#undef PlaySound
#endif

#ifdef CreateFont
#undef CreateFont
#endif

using namespace vgui;

#define PLAT(vpanel) (((VPanel *)vpanel)->Plat())

namespace vgui
{
class SurfacePlat
{
public:
	HWND    hwnd;
	HDC     hdc;
	HDC     hwndDC;
	HDC	  textureDC;
	HGLRC   hglrc;
	HRGN    clipRgn;
	HBITMAP bitmap;
	int     bitmapSize[2];
	int     restoreInfo[4];
	bool    isFullscreen;
	bool	disabled; // whether the window can take user input
	int     fullscreenInfo[3];
	VPanel	*embeddedPanel;
	
	bool    isToolbar;				// whether it has an icon in the tool tray or not
	HICON   notifyIcon;
	VPANEL notifyPanel;

	HPanel lastKeyFocusIndex;		// index to the last panel to have the focus
};

class Texture
{
public:
	int     _id;
	HBITMAP _bitmap;
	HBITMAP _maskBitmap;
	bool    _bMask;
	HICON	_icon;
	int     _wide;
	int     _tall;
	void   *_dib;
	void   *_maskDib;
	const char *_filename;
};
}

//-----------------------------------------------------------------------------
// Purpose: Implementation of ISurface for use running under windows, renders using GDI
//			This is not used in the game, EngineSurface is used instead
//-----------------------------------------------------------------------------
class CWin32Surface : public ISurface
{
public:
	friend class CIconImage;

	CWin32Surface();
	~CWin32Surface();

	void Shutdown() override;
	void RunFrame() override;
	VPANEL GetEmbeddedPanel() override;
	void SetEmbeddedPanel( VPANEL panel) override;

	// initializes the surface with the current window state
	virtual void SetCurrentContextPanel(VPANEL panel);
	void PushMakeCurrent(VPANEL panel,bool useInsets) override;
	void PopMakeCurrent(VPANEL panel) override;

	void DrawSetColor(int r, int g, int b, int a) override;
	void DrawSetColor(Color col) override;
	void DrawFilledRect(int x0, int y0, int x1, int y1) override;
	void DrawFilledRectFastFade( int x0, int y0, int x1, int y1, int fadeStartPt, int fadeEndPt, unsigned int alpha0, unsigned int alpha1, bool bHorizontal ) override;
	void DrawFilledRectFade( int x0, int y0, int x1, int y1, unsigned int alpha0, unsigned int alpha1, bool bHorizontal ) override;
	void DrawFilledRectArray( IntRect *pRects, int numRects ) override;
	void DrawOutlinedRect(int x0, int y0, int x1, int y1) override;
	void DrawLine(int x0, int y0, int x1, int y1) override;
	void DrawPolyLine(int *px, int *py, intp numPoints) override;
	void DrawSetTextFont(HFont font) override;
	void DrawSetTextColor(int r, int g, int b, int a) override;
	void DrawSetTextColor(Color col) override;
	void DrawSetTextPos(int x, int y) override;
	void DrawSetTextScale(float sx, float sy) override;
	void DrawPrintText(const wchar_t *, int textLen, FontDrawType_t drawType = FONT_DRAW_DEFAULT) override;
	void DrawUnicodeChar(wchar_t wch, FontDrawType_t drawType = FONT_DRAW_DEFAULT ) override;
	void DrawUnicodeString( const wchar_t *pwString, FontDrawType_t drawType = FONT_DRAW_DEFAULT ) override;
	void DrawGetTextPos(int& x,int& y) override;

	bool DrawGetTextureFile(int id, char *filename, int maxlen ) override;
	int	 DrawGetTextureId( char const *filename ) override;
	void DrawSetTextureFile(int id, const char *filename, int hardwareFilter, bool forceReload = false) override;
	void DrawSetTexture(int id) override;	
	void DrawSetTextureRGBA(int id, const unsigned char *rgba, int wide, int tall, int hardwareFilter, bool forceReload = false) override;
	void DrawSetTextureRGBAEx(int id, const unsigned char *rgba, int wide, int tall, ImageFormat imageFormat ) override;
	void DrawGetTextureSize(int id, int &wide, int &tall) override;
	IVguiMatInfo *DrawGetTextureMatInfoFactory( int ) override { return NULL; }
	void DrawTexturedRect(int x0, int y0, int x1, int y1) override;
	int CreateNewTextureID( bool procedural ) override;
	bool IsTextureIDValid(int id) override;
	virtual void FreeTextureData( Texture *pTexture );
	bool DeleteTextureByID(int id) override;
	void DrawFlushText() override;
	IHTML *CreateHTMLWindow(vgui::IHTMLEvents *events, VPANEL context) override;
	void PaintHTMLWindow(IHTML *htmlwin) override;
	void DeleteHTMLWindow(IHTML *htmlwin) override;

	VPANEL GetNotifyPanel() override;
	void SetNotifyIcon(VPANEL context, HTexture icon, VPANEL panelToReceiveMessages, const char *text) override;
	void SetPanelForInput( VPANEL vpanel ) override;

	void GetScreenSize(int &wide, int &tall) override;

	void SetAsTopMost(VPANEL panel, bool state) override;
	void BringToFront(VPANEL panel) override;
	void SetForegroundWindow(VPANEL panel) override;
	void SetPanelVisible(VPANEL panel, bool visible) override;
	void SetMinimized(VPANEL panel, bool state) override;
	bool IsMinimized(VPANEL panel) override;
	void FlashWindow(VPANEL panel, bool state) override;
	void SetTitle(VPANEL panel, const wchar_t *title) override;
	void SetAsToolBar(VPANEL panel, bool state) override;
	bool SupportsFeature(SurfaceFeature_e feature) override;
	void SetTopLevelFocus(VPANEL panel) override;

	intp GetPopupCount() override;
	VPANEL GetPopup(intp index) override;
	void AddPanel(VPANEL panel) override;
	void ReleasePanel(VPANEL panel) override;
	void CreatePopup(VPANEL panel, bool minimised, bool showTaskbarIcon, bool disabled, bool mouseInput, bool kbInput ) override;
	bool RecreateContext(VPANEL panel) override;
	void EnableMouseCapture(VPANEL panel, bool state) override;
	bool ShouldPaintChildPanel(VPANEL childPanel) override;
	void MovePopupToFront(VPANEL panel) override;
	void MovePopupToBack(VPANEL panel) override;
	void SwapBuffers(VPANEL panel) override;
	void Invalidate(VPANEL panel) override;
	void SetCursor(HCursor cursor) override;
	void SetCursorAlwaysVisible( bool ) override {}
	void ApplyChanges() override;
	bool IsWithin(int x, int y) override;
	bool HasFocus() override;
	void GetWorkspaceBounds(int &x, int &y, int &wide, int &tall) override;
	void SolveTraverse(VPANEL panel, bool forceApplySchemeSettings) override;
	void PaintTraverse(VPANEL panel) override;


	void RestrictPaintToSinglePanel(VPANEL panel) override;
	void SetModalPanel(VPANEL ) override;
	VPANEL GetModalPanel() override;
	void UnlockCursor() override;
	void LockCursor() override;
	void SetTranslateExtendedKeys(bool state) override;
	VPANEL GetTopmostPopup() override;


	// sound
	void PlaySound(const char *fileName) override;

	// fonts
	HFont CreateFont() override;
	bool SetFontGlyphSet(HFont font, const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags, int nRangeMin = 0, int nRangeMax = 0) override;
	int GetFontTall(HFont font) override;
	int GetFontTallRequested(HFont font) override;
	int GetFontAscent(HFont font, wchar_t wch) override;
	void GetCharABCwide(HFont font, wchar_t ch, int &a, int &b, int &c) override;
	int GetCharacterWidth(HFont font, wchar_t ch) override;
	void GetTextSize(HFont font, const wchar_t *text, int &wide, int &tall) override;
	bool AddCustomFontFile(const char *fontName, const char *fontFileName) override;
	bool AddBitmapFontFile(const char *fontFileName) override;
	void SetBitmapFontName( const char *pName, const char *pFontFilename ) override;
	const char *GetBitmapFontName( const char *pName ) override;
	bool SetBitmapFontGlyphSet(HFont font, const char *windowsFontName, float scalex, float scaley, int flags) override;
	bool IsFontAdditive(HFont font) override;
	void PrecacheFontCharacters(HFont font, const wchar_t *pCharacters) override;
	void ClearTemporaryFontCache( void ) override;
	const char *GetFontName( HFont font ) override;
	const char *GetFontFamilyName( HFont font ) override;

	bool IsCursorVisible() override { return true; }
	
	// gets the absolute coordinates of the screen (in screen space)
	void GetAbsoluteWindowBounds(int &x, int &y, int &wide, int &tall) override;

	// methods
	void setFocus(VPANEL panel);

	void GetProportionalBase( int &width, int &height ) override { width = BASE_WIDTH; height = BASE_HEIGHT; }

	void CalculateMouseVisible() override;
	bool NeedKBInput() override;

	// we use the default IInput cursor functions
	bool HasCursorPosFunctions() override { return false; }
	void SurfaceGetCursorPos(int &, int &) override {}
	void SurfaceSetCursorPos(int, int) override {}

	void SetAllowHTMLJavaScript( bool state ) override;
		// SRC specific interfaces
	void DrawTexturedLine( const Vertex_t &a, const Vertex_t &b ) override;
	void DrawOutlinedCircle(int x, int y, int radius, int segments) override;
	void DrawTexturedPolyLine( const Vertex_t *p, int n ) override; // (Note: this connects the first and last points).
	void DrawTexturedSubRect( int x0, int y0, int x1, int y1, float texs0, float text0, float texs1, float text1 ) override;
	void DrawTexturedPolygon(int n, Vertex_t *pVertices, bool bClipVertices = true) override;
	const wchar_t *GetTitle(VPANEL panel) override;
	virtual void LockCursor( bool state );
	bool IsCursorLocked( void ) const override;
	void SetWorkspaceInsets( int left, int top, int right, int bottom ) override;
	// Lower level char drawing code, call DrawGet then pass in info to DrawRender (NOT SUPPORTED BY DEFAULT SURFACE )!!!
	bool DrawGetUnicodeCharRenderInfo( wchar_t ch, CharRenderInfo& info ) override;
	void DrawRenderCharFromInfo( const CharRenderInfo& info ) override;

	// alpha multipliers not yet implemented
	void DrawSetAlphaMultiplier( float /* [0..1] */ ) override {}
	float DrawGetAlphaMultiplier() override { return 1.0f; }

	// Here's where the app systems get to learn about each other 
	bool Connect( CreateInterfaceFn factory ) override;
	void Disconnect() override;

	// Here's where systems can access other interfaces implemented by this object
	// Returns NULL if it doesn't implement the requested interface
	void *QueryInterface( const char *pInterfaceName ) override;

	// Init, shutdown
	InitReturnVal_t Init() override;
	//virtual void Shutdown();

	// screen size changing
	void OnScreenSizeChanged( int, int ) override
	{
	}

	// We don't support this for non material system surfaces (we could)
	vgui::HCursor CreateCursorFromFile( char const *, char const * ) override
	{
		Assert( 0 );
		return dc_arrow;
	}

	void PaintTraverseEx(VPANEL panel, [[maybe_unused]] bool paintPopups = false ) override
	{
		PaintTraverse( panel );
	}

	float GetZPos() const  override
	{
		return 0.0f;
	}

	IImage *GetIconImageForFullPath( char const *pFullPath ) override;

	const char *GetResolutionKey( void ) const override
	{
		Assert( 0 );
		return NULL;
	}

	bool ForceScreenSizeOverride( bool bState, int wide, int tall ) override;
	// LocalToScreen, ParentLocalToScreen fixups for explicit PaintTraverse calls on Panels not at 0, 0 position
	bool ForceScreenPosOffset( bool bState, int x, int y ) override;

	void OffsetAbsPos( int &x, int &y ) override;

	bool IsScreenSizeOverrideActive( void ) override;
	bool IsScreenPosOverrideActive( void ) override;

	void GetKernedCharWidth( HFont, wchar_t, wchar_t, wchar_t, float &wide, float &flabcA ) override
	{
		Assert( 0 );
		wide = 0.0f;
		flabcA = 0.0f;
	}

	// split screen state changed, etc.
	void ResetFontCaches() override
	{
	}

	void DestroyTextureID( int id ) override;

	int GetTextureNumFrames( int id ) override;
	void DrawSetTextureFrame( int id, int nFrame, unsigned int *pFrameCache ) override;


	void DrawUpdateRegionTextureRGBA( int, int, int, const unsigned char *, int, int, ImageFormat ) override
	{
	}
	bool BHTMLWindowNeedsPaint(IHTML *) override
	{
		return false;
	}


	const char *GetWebkitHTMLUserAgentString() override;

	void *Deprecated_AccessChromeHTMLController() override { return NULL; }

	void SetFullscreenViewport( int x, int y, int w, int h ) override 
	{
		m_nFullscreenViewportX = x; 
		m_nFullscreenViewportY = y; 
		m_nFullscreenViewportWidth = w; 
		m_nFullscreenViewportHeight = h; 
		m_pFullscreenRenderTarget = NULL; 
	}

	void GetFullscreenViewport( int & x, int & y, int & w, int & h ) override 
	{ 
		x = m_nFullscreenViewportX; 
		y = m_nFullscreenViewportY; 
		w = m_nFullscreenViewportWidth; 
		h = m_nFullscreenViewportHeight;  
	}
	void PushFullscreenViewport() override;
	void PopFullscreenViewport() override;

	// software cursors aren't available in tools
	void SetSoftwareCursor( bool ) override {}
	void PaintSoftwareCursor() override {}

private:

	CUtlDict< IImage *, unsigned short >	m_FileTypeImages;

	enum { BASE_HEIGHT = ::BASE_HEIGHT, BASE_WIDTH = ::BASE_WIDTH };

	bool LoadTGA(Texture *texture, const char *filename);
	bool LoadBMP(Texture *texture, const char *filename);

	void InternalThinkTraverse(VPANEL panel);
	void InternalSolveTraverse(VPANEL panel);
	void InternalSchemeSettingsTraverse(VPANEL panel, bool forceApplySchemeSettings);

	VPANEL GetContextPanelForChildPanel(VPANEL childPanel);
	void initStaticData();

	// sets the current line drawing color, called by DrawSetTextColor
	void SetLineColor(Color col);

	VPANEL _embeddedPanel;				// main panel
	VPANEL _notifyPanel;
	VPANEL _currentContextPanel;
	HTexture _notifyIcon;
	Dar<VPANEL> _popupList;			// list of panels that have their own win32 window
	HICON _currentCursor;

	CUtlVector<CUtlSymbol> m_CustomFontFileNames;

	// current font info
	HFont m_hCurrentFont;
	CWin32Font *m_pActiveFont;
	HPEN pen; // the pen used to draw lines

	bool _needKB;
	bool _needMouse;
	
	int		m_TextPos[2];

	Texture *m_pCurrentTexture;
	static bool TextureLessFunc(const Texture &lhs, const Texture &rhs);
	Texture *GetTextureById(int id);
	Texture *AllocTextureForId(int id);
	int GetNumTextures();

	CUtlRBTree<Texture, int> m_VGuiSurfaceTextures;

	struct ScreenOverride_t
	{
		ScreenOverride_t() : m_bActive( false ) 
		{
			m_nValue[ 0 ] = m_nValue[ 1 ] = 0;
		}
		bool	m_bActive;
		int		m_nValue[ 2 ];
	};

	ScreenOverride_t m_ScreenSizeOverride;
	ScreenOverride_t m_ScreenPosOverride;

	bool m_bAllowJavaScript;

	int m_nFullscreenViewportX;
	int m_nFullscreenViewportY;
	int m_nFullscreenViewportWidth;
	int m_nFullscreenViewportHeight;
	ITexture *m_pFullscreenRenderTarget; 

};

CWin32Surface g_Surface;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CWin32Surface, ISurface, VGUI_SURFACE_INTERFACE_VERSION, g_Surface);

//!! these defines duplicated in Surface_Win32.cpp
#define WM_MY_TRAY_NOTIFICATION (WM_USER+1)

static UINT staticShutdownMsg = 0;
static HICON staticDefaultCursor[20];
static WNDCLASS staticWndclass = {};
static ATOM staticWndclassAtom = 0;
static bool staticSurfaceAvailable;

// these functions defined below
static LRESULT CALLBACK staticProc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam);
static void staticNotifyIconProc(HWND hwnd, WPARAM wparam, LPARAM lparam);

#ifdef DEBUG_TIMING

#define START_TIMER() \
	float paintTime;\
	static LARGE_INTEGER ticksPerSecond = { 0 };\
	if (!ticksPerSecond.QuadPart)\
	{\
		QueryPerformanceFrequency(&ticksPerSecond);\
	}\
	LARGE_INTEGER Start, end;\
	QueryPerformanceCounter(&Start);

#define END_TIMER(x) \
	QueryPerformanceCounter(&end);\
	paintTime = (float)(((end.QuadPart - Start.QuadPart) * 1000000) / ticksPerSecond.QuadPart) / 1000;\
	if (paintTime > 1.0f) Msg(x, paintTime);


#else
#define START_TIMER()
#define END_TIMER(x)

#endif // DEBUG_TIMING

//-----------------------------------------------------------------------------
// Purpose: Handles drag and drop
//-----------------------------------------------------------------------------
class CSurfaceDragDropTarget : public IDropTarget
{
public:
	CSurfaceDragDropTarget()
	{
		_refCount = 0;
		_dragData = NULL;

		HRESULT hr = OleInitialize(NULL);
		if (FAILED(hr)) Warning("OleInitialize failed w/e %ld", hr);
	}
	~CSurfaceDragDropTarget()
	{
		OleUninitialize();
	}

private:
	CInterlockedUInt _refCount;
	KeyValues *_dragData;

    virtual HRESULT STDMETHODCALLTYPE QueryInterface( 
        /* [in] */ REFIID riid,
        /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject)
	{
		if (riid == IID_IDropTarget)
		{
			*ppvObject = (IDropTarget *)this;
			return S_OK;
		}

		return E_NOINTERFACE;
	}
    
    ULONG STDMETHODCALLTYPE AddRef( void) override
	{
		return ++_refCount;
	}
    
    ULONG STDMETHODCALLTYPE Release( void) override
	{
		return --_refCount;
	}

	virtual HRESULT STDMETHODCALLTYPE DragEnter(IDataObject *pDataObject, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
	{
		if (_dragData)
		{
			_dragData->deleteThis();
		}
		_dragData = calculateData(pDataObject);
		return DragOver(grfKeyState, pt, pdwEffect);
	}

	virtual HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL, DWORD *pdwEffect)
	{
		*pdwEffect = DROPEFFECT_NONE;

		if (!_dragData || !g_pIVgui->IsRunning())
			return S_OK;

		// get the panel the mouse is over
		VPANEL mouseOver = g_pInput->GetMouseOver();
		if (mouseOver)
		{
			// check to see if the panel will accept this message
			if (((VPanel *)mouseOver)->Client()->RequestInfo(_dragData))
			{
				*pdwEffect = DROPEFFECT_COPY;
			}
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE DragLeave()
	{
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE Drop(IDataObject *, DWORD, POINTL, DWORD * pdwEffect)
	{
		*pdwEffect = DROPEFFECT_NONE;

		if (!_dragData || !g_pIVgui->IsRunning())
			return S_OK;


		// get the panel the mouse is over
		VPANEL target = (VPANEL)_dragData->GetPtr("AcceptPanel");
		if (target && _dragData)
		{
			// check to see if the panel will accept this message
			g_pIVgui->PostMessage(target, _dragData, NULL);
			_dragData = NULL;
		}

		if (_dragData)
		{
			_dragData->deleteThis();
		}
		_dragData = NULL;

		return S_OK;
	}

	// internal methods
	virtual KeyValues *calculateData(IDataObject *pDataObject)
	{
		KeyValues *dragData = NULL;

		// check on the type of data
		FORMATETC format =
		{
			CF_TEXT,
			NULL,
			DVASPECT_CONTENT,
			-1,
			TYMED_HGLOBAL
    		};
		STGMEDIUM storage;

		if (pDataObject->GetData(&format, &storage) == S_OK)
		{
			// we got some data
			if (storage.tymed == TYMED_HGLOBAL)
			{
				const char *buf = (const char *)GlobalLock(storage.hGlobal);
				if (buf)
				{
					dragData = new KeyValues("DragDrop", "type", "text", "text", buf);
					GlobalUnlock(storage.hGlobal);
				}
			}

			ReleaseStgMedium(&storage);
		}
		else
		{
			// try getting a file
			format.cfFormat = CF_HDROP;
			if (pDataObject->GetData(&format, &storage) == S_OK && storage.tymed == TYMED_HGLOBAL)
			{
				dragData = new KeyValues("DragDrop", "type", "files");
				KeyValues *fileList = dragData->FindKey("list", true);

				// parse out the file list
				HDROP hdrop = (HDROP)GlobalLock(storage.hGlobal);
				if (hdrop)
				{
					char namebuf[32], buf[512];
					int count = DragQueryFile(hdrop, 0xFFFFFFFF, buf, 511);
					for (int i = 0; i < count; i++)
					{
						V_to_chars(namebuf, i);
						DragQueryFile(hdrop, i, buf, 511);
						fileList->SetString(namebuf, buf);
					}
					GlobalUnlock(storage.hGlobal);
				}
			}
		}

		return dragData;
	}
};

static CSurfaceDragDropTarget staticDragDropTarget;

bool CWin32Surface::TextureLessFunc(const Texture &lhs, const Texture &rhs)
{
	return lhs._id < rhs._id;
}

Texture *CWin32Surface::GetTextureById(int id)
{
	Texture findTex = {id, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	auto index = m_VGuiSurfaceTextures.Find(findTex);
	if (m_VGuiSurfaceTextures.IsValidIndex(index))
	{
		return &m_VGuiSurfaceTextures[index];
	}

	return NULL;
}

Texture *CWin32Surface::AllocTextureForId(int id)
{
	Texture newTex = {id, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	auto index = m_VGuiSurfaceTextures.Insert(newTex);
	return &m_VGuiSurfaceTextures[index];
}

int CWin32Surface::GetNumTextures()
{
	return m_VGuiSurfaceTextures.Count();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void staticGenerateIconForTexture(Texture *texture, HDC hdc)
{
	// see if there is an iconic version of the texture file first
	char buf[256];
	Q_snprintf(buf, sizeof( buf ), "%s.ico", texture->_filename);
	texture->_icon = (HICON)::LoadImage(NULL, buf, IMAGE_ICON, 16, 16, LR_LOADFROMFILE | LR_DEFAULTSIZE);
	if (texture->_icon)
		return;

	if (texture->_wide > 32 || texture->_tall > 32)
		return;

	// generate an icon from the tga

	// generate the AND plane based on the alpha
	uchar planeAND[32 * 32 / 8];

	/* !! disabled until verified
	//
	// from the alpha of the bitmap, generate a bitmask
	//
	uchar *ptr = (uchar *)texture->_dib + 3;	// jump to alpha portion
	uchar *ptrEnd = (uchar *)texture->_dib + (texture->_wide * texture->_tall * 4);
	uchar *andPtr = planeAND;
	while (ptr < ptrEnd)
	{
		// Start all empty
		*andPtr = 0;

		// assume the number of pixel is a multiple of 8
		// if the alpha is high enough, then mask out the pixel

		// it's two bits per pixel, strangely enough
		if (*ptr > 127)	{ *andPtr |= 0x03; }	ptr += 4;
		if (*ptr > 127)	{ *andPtr |= 0x0C; }	ptr += 4;
		if (*ptr > 127)	{ *andPtr |= 0x30; }	ptr += 4;
		if (*ptr > 127)	{ *andPtr |= 0xC0; }	ptr += 4;

		andPtr++;
	}
	*/

	memset(planeAND, 0x00, sizeof(planeAND));
	HBITMAP mask = ::CreateBitmap(texture->_wide, texture->_tall, 1, 1, planeAND);

	// create the icon
	ICONINFO iconInfo;
	iconInfo.fIcon = TRUE;
	iconInfo.xHotspot = 8;
	iconInfo.yHotspot = 8;
	iconInfo.hbmMask = mask;
	iconInfo.hbmColor = texture->_bitmap;

	texture->_icon = ::CreateIconIndirect(&iconInfo);

	::DeleteObject(mask);
}


//-----------------------------------------------------------------------------
// Purpose: Constructor, basic variable initialization
//-----------------------------------------------------------------------------
CWin32Surface::CWin32Surface() : m_VGuiSurfaceTextures(0, 128, TextureLessFunc)
{
	_currentCursor = NULL;
	m_pCurrentTexture = NULL;

	initStaticData();

	staticSurfaceAvailable = false;
	m_bAllowJavaScript = false;

	m_hCurrentFont = 0;

	pen = NULL;

	_needKB = true;
	_needMouse = true;

	m_TextPos[0] = m_TextPos[1] = 0;

	m_nFullscreenViewportX = m_nFullscreenViewportY = 0;
	m_nFullscreenViewportWidth = m_nFullscreenViewportHeight = 0;
	m_pFullscreenRenderTarget = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CWin32Surface::~CWin32Surface()
{
	// ensure we don't try and process any more windows messages
	staticSurfaceAvailable = false;

	// free all the textures
	
	for (int i = 0; i < m_VGuiSurfaceTextures.MaxElement(); i++)
	{
		if (!m_VGuiSurfaceTextures.IsValidIndex(i))
			continue;

		Texture *texture = &m_VGuiSurfaceTextures[i];

		if (texture->_bitmap)
		{
			::DeleteObject(texture->_bitmap);
		}

		if (texture->_maskBitmap)
		{
			::DeleteObject(texture->_maskBitmap);
		}

		if (texture->_icon)
		{
			::DestroyIcon(texture->_icon);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Shuts down app
//-----------------------------------------------------------------------------
void CWin32Surface::Shutdown()
{
	for ( auto i = m_FileTypeImages.First(); i != m_FileTypeImages.InvalidIndex(); i = m_FileTypeImages.Next( i ) )
	{
		delete m_FileTypeImages[ i ];
	}
	m_FileTypeImages.RemoveAll();

	// free the fonts
	FontManager().ClearAllFonts();

	// release any custom font files
	{for (int i = 0; i < m_CustomFontFileNames.Count(); i++)
	{
		::RemoveFontResource(m_CustomFontFileNames[i].String());
	}}
	m_CustomFontFileNames.RemoveAll();

	// ensure we don't try and process windows messages during Shutdown
	staticSurfaceAvailable = false;

	// release any panels still with surfaces
	while (GetPopupCount())
	{
		ReleasePanel(GetPopup(0));
	}

	// kill our windows instance
	::UnregisterClass("Surface", ::GetModuleHandle(NULL));
	staticWndclassAtom = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
VPANEL CWin32Surface::GetEmbeddedPanel()
{
	return _embeddedPanel;
}

	// SRC specific interfaces
 void CWin32Surface::DrawTexturedLine( const Vertex_t &, const Vertex_t & )
 {

 }
 void CWin32Surface::DrawOutlinedCircle(int, int, int, int) 
 {

 }
 void CWin32Surface::DrawTexturedPolyLine( const Vertex_t *, int )
 {
 }
 void CWin32Surface::DrawTexturedSubRect( int, int, int, int, float, float, float, float )
 {
 }
 void CWin32Surface::DrawTexturedPolygon(int n, Vertex_t *pVertices, bool bClipVertices /*= true*/)
 {
	NOTE_UNUSED( bClipVertices );

	POINT *pt;
	HDC hdc = PLAT(_currentContextPanel)->hdc;
	
	pt = (POINT *)malloc(sizeof(POINT) * n);
	if(pt) 
	{
		for(int i=0;i<n;i++)
		{
			pt[i].x= pVertices[i].m_Position.x;
			pt[i].y= pVertices[i].m_Position.y;
		}

		COLORREF pencolor = ::GetTextColor(hdc);
		COLORREF brushcolor = ::GetBkColor(hdc);

		//Set the pen color to the current brush color to avoid strange outlines on our polygons
		DrawSetTextColor(GetRValue(brushcolor),GetGValue(brushcolor),GetBValue(brushcolor),255);

		//create a brush
		HBRUSH hbrush = ::CreateSolidBrush(brushcolor);
		HBRUSH oldBrush = (HBRUSH)::SelectObject(hdc, hbrush);

		::Polygon(hdc, pt, n);
		
		::SelectObject(hdc, oldBrush);
		::DeleteObject(hbrush);
		free(pt);

		//restore pen colour 
		DrawSetTextColor(GetRValue(pencolor),GetGValue(pencolor),GetBValue(pencolor),255);
	}
 }

 const wchar_t *CWin32Surface::GetTitle(VPANEL)
 {
	return L"";
 }
 void CWin32Surface::LockCursor( bool )
 {
 }
 bool CWin32Surface::IsCursorLocked( void ) const
 {
	 return false;
 }
 void CWin32Surface::SetWorkspaceInsets( int, int, int, int )
 {
 }
 //-----------------------------------------------------------------------------
// Connect, disconnect...
//-----------------------------------------------------------------------------
bool CWin32Surface::Connect( CreateInterfaceFn )
{	
return true;
}

void CWin32Surface::Disconnect()
{
}


//-----------------------------------------------------------------------------
// Access to other interfaces...
//-----------------------------------------------------------------------------
void *CWin32Surface::QueryInterface( const char * )
{
	return NULL;
}


//-----------------------------------------------------------------------------
// Initialization and shutdown...
//-----------------------------------------------------------------------------
InitReturnVal_t CWin32Surface::Init( void )
{
	return INIT_FAILED;
}



//-----------------------------------------------------------------------------
// Purpose: Sets up the panel for use
// Input  : *embeddedPanel - Main panel that becomes the top of the hierarchy
//-----------------------------------------------------------------------------
void CWin32Surface::SetEmbeddedPanel( VPANEL panel )
{
	_embeddedPanel = panel;

	staticSurfaceAvailable = true;

	SetCurrentContextPanel(panel);
	CreatePopup(panel, false, true, false, true, true);

	// send a message to ourselves every 50ms (20Hz) so that we don't block in the message queue too long
	::SetTimer(PLAT(_currentContextPanel)->hwnd, 0, 50, (TIMERPROC) NULL);

	// fonts initialization
	char language[64];
	if (g_pSystem->GetRegistryString("HKEY_CURRENT_USER\\Software\\Valve\\Source\\Language", language, sizeof(language)-1))
	{
		FontManager().SetLanguage(language);
	}
	else
	{
		FontManager().SetLanguage("english");
	}
}

// Lower level char drawing code, call DrawGet then pass in info to DrawRender
bool CWin32Surface::DrawGetUnicodeCharRenderInfo( wchar_t, CharRenderInfo& info )
{
	// Only supported in engine renderer!
	Assert( 0 );
	info.valid = false;
	return false;
}

void CWin32Surface::DrawRenderCharFromInfo( const CharRenderInfo& )
{
	Assert( 0 );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the current drawing context
//-----------------------------------------------------------------------------
void CWin32Surface::SetCurrentContextPanel(VPANEL panel)
{
	_currentContextPanel = panel;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
VPANEL CWin32Surface::GetContextPanelForChildPanel(VPANEL childPanel)
{
	VPANEL contextPanel = childPanel;
	while (contextPanel && !PLAT(contextPanel))
	{
		contextPanel = (VPANEL)((VPanel *)contextPanel)->GetParent();
	}
	return contextPanel;
}

void CWin32Surface::PushMakeCurrent(VPANEL panel, bool useInsets)
{
	// find the win32 window context from the hierarchy
	VPANEL currentContextPanel = GetContextPanelForChildPanel(panel);

	SetCurrentContextPanel(currentContextPanel);

	// clear the current active font so that it will be reset
	m_pActiveFont = NULL;
	
	if (!currentContextPanel)
	{
		// no drawing context found, something is seriously wrong
		Msg( "Warning: VPanel with no drawing context\n" );
	}

	int inset[4];

	//!! need to make the inset part of VPanel
	((VPanel *)panel)->Client()->GetInset(inset[0],inset[1],inset[2],inset[3]);

	if(!useInsets)
	{
		inset[0]=0;
		inset[1]=0;
		inset[2]=0;
		inset[3]=0;
	}

	int absThis[4];
	((VPanel *)_currentContextPanel)->GetAbsPos(absThis[0], absThis[1]);
	((VPanel *)_currentContextPanel)->GetSize(absThis[2], absThis[3]);
	absThis[2] += absThis[0];
	absThis[3] += absThis[1];

	int absPanel[4];
	((VPanel *)panel)->GetAbsPos(absPanel[0], absPanel[1]);
	((VPanel *)panel)->GetSize(absPanel[2], absPanel[3]);
	absPanel[2] += absPanel[0];
	absPanel[3] += absPanel[1];

	int clipRect[4];
	((VPanel *)panel)->Client()->GetClipRect(clipRect[0],clipRect[1],clipRect[2],clipRect[3]);

	if ( _currentContextPanel == panel )
	{
		// this panel has it's own window, so use screen space
		::SetViewportOrgEx(PLAT(_currentContextPanel)->hdc,0+inset[0],0+inset[1],nullptr);
	}
	else
	{
		// child window, so set win32 up so all subsequent drawing calls are done in local space
		::SetViewportOrgEx(PLAT(_currentContextPanel)->hdc,(absPanel[0]+inset[0])-absThis[0],(absPanel[1]+inset[1])-absThis[1],nullptr);
	}

	// setup clipping
	// get and translate clipRect into surface space, then factor in inset
	int x0 = clipRect[0] - absThis[0];
	int y0 = clipRect[1] - absThis[1];
	int x1 = (clipRect[2] - absThis[0]) - inset[2];
	int y1 = (clipRect[3] - absThis[1]) - inset[3];

	//set the rect and select to make it current
	::SetRectRgn(PLAT(_currentContextPanel)->clipRgn,x0,y0,x1,y1);
	::SelectObject(PLAT(_currentContextPanel)->hdc, PLAT(_currentContextPanel)->clipRgn);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::PopMakeCurrent(VPANEL panel)
{
	if (panel == _currentContextPanel)
	{
		// reset the current panel to be the main panel
		SetCurrentContextPanel(_embeddedPanel);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::GetScreenSize(int &wide, int &tall)
{
	if ( m_ScreenSizeOverride.m_bActive )
	{
		wide = m_ScreenSizeOverride.m_nValue[ 0 ];
		tall = m_ScreenSizeOverride.m_nValue[ 1 ];
		return;
	}

	VPANEL context = _embeddedPanel;
	if (context)
	{
		wide = ::GetDeviceCaps(PLAT(context)->hdc, HORZRES);
		tall = ::GetDeviceCaps(PLAT(context)->hdc, VERTRES);
	}
}

bool CWin32Surface::ForceScreenSizeOverride( bool bState, int wide, int tall )
{
	bool bWasSet = m_ScreenSizeOverride.m_bActive;
	m_ScreenSizeOverride.m_bActive = bState;
	m_ScreenSizeOverride.m_nValue[ 0 ] = wide;
	m_ScreenSizeOverride.m_nValue[ 1 ] = tall;
	return bWasSet;
}

// LocalToScreen, ParentLocalToScreen fixups for explicit PaintTraverse calls on Panels not at 0, 0 position
bool CWin32Surface::ForceScreenPosOffset( bool bState, int x, int y )
{
	bool bWasSet = m_ScreenPosOverride.m_bActive;
	m_ScreenPosOverride.m_bActive = bState;
	m_ScreenPosOverride.m_nValue[ 0 ] = x;
	m_ScreenPosOverride.m_nValue[ 1 ] = y;
	return bWasSet;
}

void CWin32Surface::OffsetAbsPos( int &x, int &y )
{
	if ( !m_ScreenPosOverride.m_bActive )
		return;

	x += m_ScreenPosOverride.m_nValue[ 0 ];
	y += m_ScreenPosOverride.m_nValue[ 1 ];
}

bool CWin32Surface::IsScreenSizeOverrideActive( void )
{
	return ( m_ScreenSizeOverride.m_bActive );
}

bool CWin32Surface::IsScreenPosOverrideActive( void )
{
	return ( m_ScreenPosOverride.m_bActive );
}

void CWin32Surface::DestroyTextureID( int )
{
	// not implemented
}

int CWin32Surface::GetTextureNumFrames( int )
{
	// not implemented
	return 0;
}

void CWin32Surface::DrawSetTextureFrame( int, int, unsigned int * )
{
	// not implemented
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
VPANEL CWin32Surface::GetNotifyPanel()
{
	return _notifyPanel;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::SetNotifyIcon(VPANEL context, HTexture iconID, VPANEL panelToReceiveMessages, const char *text)
{
	context = GetContextPanelForChildPanel(context);
	if (!context)
		return;

	if (!text)
	{
		text = "";
	}

	// things haven't changed so just update the tooltip
	if (_notifyIcon == iconID && _notifyPanel == panelToReceiveMessages && context && PLAT(context)->notifyIcon)
	{
		::NOTIFYICONDATA iconData = 
		{
			sizeof(iconData),
			PLAT(context)->hwnd,
			1,									// icon ID
			NIF_TIP,							// modify tooltip only
			WM_MY_TRAY_NOTIFICATION,			// callback message
			PLAT(context)->notifyIcon,  // icon handle
			"",									// tooltip text
			0,
			0,
			{},
			{},
			{},
			0
		};

		strncpy(iconData.szTip, text, 63);
		iconData.szTip[63] = '\0';
		::Shell_NotifyIcon(NIM_MODIFY, &iconData);
		return;
	}

	_notifyIcon = iconID;
	_notifyPanel = panelToReceiveMessages;

	DWORD dwMessage = NIM_MODIFY;
	if (!iconID)
	{
		dwMessage = NIM_DELETE;
		PLAT(context)->notifyIcon = NULL;
	}
	else if (!PLAT(context)->notifyIcon)
	{
		dwMessage = NIM_ADD;
	}

	// make sure the icon has been loaded
	Texture *texture = NULL;
	if (iconID)
	{
		texture = GetTextureById(iconID);

		if (!texture->_icon)
		{
			// generate an icon for the texture
			staticGenerateIconForTexture(texture, PLAT(_currentContextPanel)->hdc);
		}
	}

	// get the icon
	if (texture)
	{
		PLAT(context)->notifyIcon = texture->_icon;
	}

	::NOTIFYICONDATA iconData = 
	{
		sizeof(iconData),
		PLAT(context)->hwnd,
		1,									// icon ID
		NIF_ICON | NIF_TIP | NIF_MESSAGE,	// items used
		WM_MY_TRAY_NOTIFICATION,			// callback message
		PLAT(context)->notifyIcon,  // icon handle
		"",									// tooltip text
		0,
		0,
		{},
		{},
		{},
		0
	};

	strncpy(iconData.szTip, text, 63);
	iconData.szTip[63] = '\0';

	BOOL success = ::Shell_NotifyIcon(dwMessage, &iconData);

	if (iconID && !success)
	{
		Msg("Shell_NotifyIcon(%ul) failed: %s.\n",
			dwMessage, std::system_category().message(::GetLastError()).c_str());
	}
}

void CWin32Surface::DrawSetColor(int r,int g,int b,int a)
{
	SetBkColor(PLAT(_currentContextPanel)->hdc,RGB(r,g,b));
}

void CWin32Surface::DrawSetColor(Color col)
{
	DrawSetColor(col[0], col[1], col[2], col[3]);
}

void CWin32Surface::DrawSetTextPos(int x, int y)
{
	MoveToEx(PLAT(_currentContextPanel)->hdc,x,y,nullptr);	
	m_TextPos[0] = x;
	m_TextPos[1] = y;
}

void CWin32Surface::DrawGetTextPos(int& x,int& y)
{
	x = m_TextPos[0];
	y = m_TextPos[1];
}


void CWin32Surface::DrawSetTextFont(HFont font)
{
	Assert(font);
	
	// make the font current
	m_hCurrentFont = font;

//	m_FontAmalgams[m_hCurrentFont].GetFontForChar('a')->SetAsActiveFont(_currentContextPanel->Plat()->hdc);
}

void CWin32Surface::DrawFilledRect(int x0,int y0,int x1,int y1)
{
	// trick to draw filled rectangles using current background color
	RECT rect = { x0, y0, x1, y1};
	ExtTextOut(PLAT(_currentContextPanel)->hdc, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL);
}

void CWin32Surface::DrawFilledRectArray( IntRect *pRects, int numRects )
{
	int i;
	for( i = 0; i < numRects; i++ )
	{
		DrawFilledRect( pRects[i].x0, pRects[i].y0, pRects[i].x1, pRects[i].y1 );
	}
}

void CWin32Surface::DrawOutlinedRect(int x0,int y0,int x1,int y1)
{
	// draw an outline of a rectangle using 4 filledRect
	DrawFilledRect(x0,y0,x1,y0+1);     // top
	DrawFilledRect(x0,y1-1,x1,y1);	   // bottom
	DrawFilledRect(x0,y0+1,x0+1,y1-1); // left
	DrawFilledRect(x1-1,y0+1,x1,y1-1); // right
}

void CWin32Surface::DrawLine(int x0,int y0,int x1,int y1)
{
	POINT pt[2];
	CONST POINT *p=pt;
	pt[0].x=x0;
	pt[0].y=y0;
	pt[1].x=x1;
	pt[1].y=y1;

//	MoveToEx(_currentContextPanel->Plat()->hdc,x0,y0,NULL);
//	LineTo(_currentContextPanel->Plat()->hdc,x1,y1);
	Polyline(PLAT(_currentContextPanel)->hdc,p , 2);
}


void CWin32Surface::DrawPolyLine(int *px, int *py, intp numPoints)
{
	POINT *pt = (POINT *)malloc(sizeof(POINT) * numPoints);
	if(pt) 
	{
		for(intp i=0;i<numPoints;i++)
		{
			pt[i].x= px[i];
			pt[i].y= py[i];
		}

		Assert(numPoints <= INT_MAX);
		Polyline(PLAT(_currentContextPanel)->hdc, pt , static_cast<int>(numPoints));
		free(pt);
	}
}

void CWin32Surface::DrawSetTextColor(int r,int g,int b,int a)
{
	// set the draw color for lines
	SetLineColor(Color(r,g,b,a));
	SetTextColor(PLAT(_currentContextPanel)->hdc,RGB(r,g,b));
}

void CWin32Surface::DrawSetTextColor(Color col)
{
	// set the draw color for lines
	SetLineColor(col);
	// end then for text
	DrawSetTextColor(col[0], col[1], col[2], col[3]);
}

void CWin32Surface::SetLineColor(Color col)
{
	HPEN tmp=pen;
	pen = CreatePen(PS_SOLID,0,RGB(col[0], col[1], col[2]));
	if(pen) 
	{
		SelectObject(PLAT(_currentContextPanel)->hdc, pen ); 
	}
	// you must delete the pen AFTER a new one is selected
	if(tmp) 
	{
		DeleteObject(tmp);
	}
}


void CWin32Surface::DrawPrintText(const wchar_t *text, int textLen, FontDrawType_t /*= FONT_DRAW_DEFAULT*/)
{
	Assert(text);
	if (!text)
		return;

	if (textLen < 1)
		return;

	for (int i = 0; i < textLen; i++)
	{
		DrawUnicodeChar(text[i]);
	}

//	ExtTextOut(_currentContextPanel->Plat()->hdc, 0, 0, 0, NULL, text, textLen, NULL);
}

void CWin32Surface::DrawUnicodeString( const wchar_t *pwString, FontDrawType_t /*= FONT_DRAW_DEFAULT*/)
{
	Assert( pwString );
	if ( !pwString )
		return;

	while ( wchar_t ch = *pwString++ )
	{
		DrawUnicodeChar( ch );	
	}
}

//-----------------------------------------------------------------------------
// Purpose: draws single unicode character at the current position with the
//			current font & color
//-----------------------------------------------------------------------------
void CWin32Surface::DrawUnicodeChar(wchar_t wch, FontDrawType_t /*= FONT_DRAW_DEFAULT*/)
{
	// set the current font
	CWin32Font *winFont = FontManager().GetFontForChar(m_hCurrentFont, wch);

	if (!winFont)
		return;

	if (m_pActiveFont != winFont)
	{
		winFont->SetAsActiveFont(PLAT(_currentContextPanel)->hdc);
		m_pActiveFont = winFont;
	}

	ExtTextOutW(PLAT(_currentContextPanel)->hdc, 0, 0, 0, NULL, &wch, 1, NULL);
}

//-----------------------------------------------------------------------------
// Purpose: allocates a new texture id
//-----------------------------------------------------------------------------
int CWin32Surface::CreateNewTextureID( bool )
{
	//!! hack, arbitrary base
	static int staticBindIndex = 2700;
	return staticBindIndex++;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the texture id has a valid texture bound to it
//-----------------------------------------------------------------------------
bool CWin32Surface::IsTextureIDValid(int id)
{
	return (GetTextureById(id) != NULL);
}

//-----------------------------------------------------------------------------
// Purpose: clear up alloced resources on a texture
//-----------------------------------------------------------------------------
void CWin32Surface::FreeTextureData( vgui::Texture *pTexture )
{
	if ( pTexture->_bitmap )
	{
		::DeleteObject( pTexture->_bitmap );
	}

//	if ( pTexture->m_bitmapScaled )
//	{
//		::DeleteObject( pTexture->m_bitmapScaled );
//	}

	if ( pTexture->_maskBitmap )
	{
		::DeleteObject( pTexture->_maskBitmap );
	}

	if ( pTexture->_icon )
	{
		::DestroyIcon( pTexture->_icon );
	}

//	if ( pTexture->rgba )
//		delete[] pTexture->rgba;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the texture id has a valid texture bound to it
//-----------------------------------------------------------------------------
bool CWin32Surface::DeleteTextureByID(int id)
{
#if defined( PS3OVERLAYUI_EXPORTS )
	AUTO_LOCK( m_MutexTextureData );
#endif

	Texture findTex = {id, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	auto index = m_VGuiSurfaceTextures.Find(findTex);
	if (m_VGuiSurfaceTextures.IsValidIndex(index))
	{
		Texture *texture = &m_VGuiSurfaceTextures[index];

		FreeTextureData( texture );

		m_VGuiSurfaceTextures.RemoveAt(index);
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: does nothing, since we don't need this optimization in win32
//-----------------------------------------------------------------------------
void CWin32Surface::DrawFlushText()
{
}



//-----------------------------------------------------------------------------
// Purpose: create a html helper object
//-----------------------------------------------------------------------------
IHTML *CWin32Surface::CreateHTMLWindow(vgui::IHTMLEvents *, VPANEL )
{
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: delete an html helper object
//-----------------------------------------------------------------------------
void CWin32Surface::DeleteHTMLWindow(IHTML *)
{
}


//-----------------------------------------------------------------------------
// Purpose: tell a html window to update its backing texture
//-----------------------------------------------------------------------------
void CWin32Surface::PaintHTMLWindow(IHTML *)
{
}


//-----------------------------------------------------------------------------
void CWin32Surface::SetAllowHTMLJavaScript( bool ) 
{ 
}

//-----------------------------------------------------------------------------
// Purpose: sets the current active texture
//-----------------------------------------------------------------------------
void CWin32Surface::DrawSetTexture(int id)
{
	Texture *texture = GetTextureById(id);
	m_pCurrentTexture = texture;
}

// dimhotepus: bpp int -> unsigned short.
HBITMAP staticCreateBitmapHandle(int wide, int tall, HDC hdc, unsigned short bpp, void **dib);
//-----------------------------------------------------------------------------
// Purpose: maps a texture from memory to an id, and uploads it into the engine
//-----------------------------------------------------------------------------
void CWin32Surface::DrawSetTextureRGBA(int id,const unsigned char* rgba,int wide,int tall, int, bool)
{
	DrawSetTextureRGBAEx( id, rgba, wide, tall, IMAGE_FORMAT_RGBA8888 );
}

//-----------------------------------------------------------------------------
// Purpose: maps a texture from memory to an id, and uploads it into the engine
//-----------------------------------------------------------------------------
void CWin32Surface::DrawSetTextureRGBAEx(int id,const unsigned char* rgba,int wide,int tall, ImageFormat )
{
	Texture *texture = GetTextureById(id);

	if (texture)
	{
		if (texture->_bitmap)
		{
			::DeleteObject(texture->_bitmap);
		}

		if (texture->_maskBitmap)
		{
			::DeleteObject(texture->_maskBitmap);
		}
	}
	if (!texture)
	{
		// allocate a new texture
		texture = AllocTextureForId(id);
		memset(texture, 0, sizeof(Texture));
	}

	{
		// no texture or forced  load the new texture
		texture->_id = id;
		texture->_filename =NULL;

		texture->_wide = wide;
		texture->_tall = tall;
		texture->_icon = NULL;
		texture->_dib = NULL;
		texture->_bitmap = staticCreateBitmapHandle(texture->_wide, 
			texture->_tall,	PLAT(_currentContextPanel)->hdc, 32, &texture->_dib );

		// copy over the texture data
		memcpy(texture->_dib,rgba,wide*tall*4);

	}

	m_pCurrentTexture = texture;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : id - 
//			*filename - 
//			maxlen - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWin32Surface::DrawGetTextureFile(int id, char *filename, int maxlen )
{
	Texture *texture = GetTextureById(id);
	if ( !texture )
		return false;

	Q_strncpy( filename, texture->_filename, maxlen );
	return true;
}

int	  CWin32Surface::DrawGetTextureId( char const *filename )
{
	auto i = m_VGuiSurfaceTextures.FirstInorder();
	while ( i != m_VGuiSurfaceTextures.InvalidIndex() )
	{
		Texture *texture = &m_VGuiSurfaceTextures[i];
		if ( !Q_stricmp( filename, texture->_filename ) )
			return texture->_id;

		i = m_VGuiSurfaceTextures.NextInorder( i );
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Maps a texture file to an id, and makes it the current drawing texture
//			tries to load as a .tga first, and if not found as a .bmp
//-----------------------------------------------------------------------------
void CWin32Surface::DrawSetTextureFile(int id, const char *filename, int, bool forceReload /*= false*/)
{
	Texture *texture = GetTextureById(id);
	
	if (!texture || stricmp(filename, texture->_filename) || forceReload )
	{
		// no texture, or the filename is different;  load the new texture
		if (!texture)
		{
			// allocate a new texture
			texture = AllocTextureForId(id);
			memset(texture, 0, sizeof(Texture));
		}
		if (texture)
		{
			if (texture->_bitmap)
			{
				::DeleteObject(texture->_bitmap);
			}

			if (texture->_maskBitmap)
			{
				::DeleteObject(texture->_maskBitmap);
			}
		}
		texture->_id = id;
		texture->_filename = filename;

		// try and find the file
		bool success = LoadTGA(texture, filename);
		if (!success)
		{
			// strip off the vgui/ and try again
			const char *psz = Q_stristr(filename, "vgui/");
			if (psz)
			{
				success = LoadTGA(texture, filename + ssize("vgui/") - 1);
			}
		}

		if (!success)
		{
			Msg("Error: texture file '%s' does not exist or is invalid\n", filename);
			return;
		}
	}

	m_pCurrentTexture = texture;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::DrawTexturedRect(int x0,int y0,int x1,int y1)
{
	if (m_pCurrentTexture == nullptr)
	{
		return;
	}

	if (PLAT(_currentContextPanel)->textureDC == nullptr)
	{
		return;
	}

	HBITMAP bitmap = m_pCurrentTexture->_bitmap;
	int wide = m_pCurrentTexture->_wide;
	int tall = m_pCurrentTexture->_tall;

	HGDIOBJ oldObject;

	if (m_pCurrentTexture->_bMask)
	{
		HBITMAP bitmap_mask = m_pCurrentTexture->_maskBitmap;

		// draw the mask first to clear out the background to black 
		oldObject = ::SelectObject(PLAT(_currentContextPanel)->textureDC, bitmap_mask);
		::StretchBlt(PLAT(_currentContextPanel)->hdc,x0,y0,x1-x0,y1-y0,PLAT(_currentContextPanel)->textureDC,0,0,wide,tall,SRCAND);

		// draw over the 'black areas' with the bitmap
		::SelectObject(PLAT(_currentContextPanel)->textureDC, bitmap);
		::StretchBlt(PLAT(_currentContextPanel)->hdc,x0,y0,x1-x0,y1-y0,PLAT(_currentContextPanel)->textureDC,0,0,wide,tall,SRCPAINT);
	}
	else
	{
		oldObject = ::SelectObject(PLAT(_currentContextPanel)->textureDC, bitmap);
		::StretchBlt(PLAT(_currentContextPanel)->hdc,x0,y0,x1-x0,y1-y0,PLAT(_currentContextPanel)->textureDC,0,0,wide,tall,SRCCOPY);
	}
	::SelectObject(PLAT(_currentContextPanel)->textureDC, oldObject);

// test code, should be used in win98/win2k for true alpha
//	BLENDFUNCTION blendFunction = { AC_SRC_OVER, 0, 255, 0 };
//	::AlphaBlend(PLAT(_currentContextPanel)->hdc,x0,y0,x1-x0,y1-y0,PLAT(_currentContextPanel)->textureDC,0,0,wide,tall,blendFunction);
}

//-----------------------------------------------------------------------------
// Purpose: Called by vgui to get texture dimensions
//-----------------------------------------------------------------------------
void CWin32Surface::DrawGetTextureSize(int id, int &wide, int &tall)
{
	Texture *texture = GetTextureById(id);

	if (!texture)
		return;

	wide = texture->_wide;
	tall = texture->_tall;
}

#pragma pack(1)
typedef struct
{
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} tga_header_t;
#pragma pack()

HBITMAP staticCreateBitmapHandle(int wide, int tall, HDC hdc, unsigned short bpp, void **dib)
{
	BITMAPINFOHEADER bitmapInfoHeader;
	memset(&bitmapInfoHeader, 0, sizeof(bitmapInfoHeader));
	bitmapInfoHeader.biSize = sizeof(bitmapInfoHeader);
	bitmapInfoHeader.biWidth = wide;
	bitmapInfoHeader.biHeight = -tall;
	bitmapInfoHeader.biPlanes = 1;
	bitmapInfoHeader.biBitCount = bpp;
	bitmapInfoHeader.biCompression = BI_RGB;

	HBITMAP hRet;
	hRet = CreateDIBSection(hdc, (BITMAPINFO*)&bitmapInfoHeader, DIB_RGB_COLORS, dib, 0, 0);
	if ( !hRet )
		Error( "staticCreateBitmapHandle: can't create DIB" );

	return hRet;
}

#define DIB_HEADER_MARKER   ((word) ('M' << 8) | 'B')
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWin32Surface::LoadBMP(Texture *texture, const char *filename)
{
	// try load the tga
	char buf[1024];
	V_sprintf_safe(buf, "%s.bmp", filename);

	FileHandle_t file = g_pFullFileSystem->Open(buf, "rb", NULL);
	if (!file)
		return false;

	bool success = false;

	// Parse bitmap
	BITMAPFILEHEADER bmfHeader;
	LPBITMAPINFO lpbmi;

	const DWORD dwFileSize = g_pFullFileSystem->Size( file );

	g_pFullFileSystem->Read( &bmfHeader, sizeof(bmfHeader), file );
	
	if (bmfHeader.bfType == DIB_HEADER_MARKER)
	{
		const DWORD dwBitsSize = dwFileSize - sizeof(bmfHeader);

		HGLOBAL hDIB = ::GlobalAlloc( GMEM_MOVEABLE | GMEM_ZEROINIT, dwBitsSize );
		if ( !hDIB )
		{
			g_pFullFileSystem->Close(file);
			return false;
		}

		char *pDIB = (LPSTR)::GlobalLock(hDIB);
		{
			g_pFullFileSystem->Read(pDIB, dwBitsSize, file );

			lpbmi = (LPBITMAPINFO)pDIB;

			// we now have a block of memory, rgba
			// throw a bitmap header on it and register it in windows
			texture->_wide = lpbmi->bmiHeader.biWidth;
			texture->_tall = lpbmi->bmiHeader.biHeight;
			texture->_icon = NULL;
			texture->_bMask = false;
			texture->_bitmap = staticCreateBitmapHandle(
				texture->_wide, 
				texture->_tall, 
				PLAT(_currentContextPanel)->hdc, 
				32, &texture->_dib );

			unsigned char *rgba = (unsigned char *)( pDIB + sizeof( BITMAPINFOHEADER ) + 256 * sizeof( RGBQUAD ) );

			// Copy raw data
			for (int j = 0; j < texture->_tall; j++)
			{ 
				for (int i = 0; i <texture->_wide; i++)
				{
					int y = (texture->_tall - j - 1);

					int offs = ( y * texture->_wide + i);
					int offsdest = (j * texture->_wide + i) * 4;
					unsigned char *src = ((unsigned char *)rgba) + offs;
					char *dst = ((char*)texture->_dib) + offsdest;
					
					dst[0] = lpbmi->bmiColors[ *src ].rgbRed;
					dst[1] = lpbmi->bmiColors[ *src ].rgbGreen;
					dst[2] = lpbmi->bmiColors[ *src ].rgbBlue;
					dst[3] = (unsigned char)255;
				}
			}

			success = true;
		}

		::GlobalUnlock( hDIB);
		::GlobalFree((HGLOBAL) hDIB);
	}

	g_pFullFileSystem->Close(file);

	return success;


}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWin32Surface::LoadTGA(Texture *texture, const char *filename)
{
	bool invertAlpha = false;

	// try load the tga
	char buf[1024];
	V_sprintf_safe(buf, "%s.tga", filename);

	FileHandle_t file = g_pFullFileSystem->Open(buf, "rb", NULL);
	if (!file)
	{
		return LoadBMP( texture, filename );
	}

	// read the header
	tga_header_t tgaHeader;
	g_pFullFileSystem->Read(&tgaHeader, sizeof(tgaHeader), file);		

	if (tgaHeader.image_type != 2 && tgaHeader.image_type != 10)
	{
		g_pIVgui->DPrintf2("Error: texture file '%s' has invalid image_type %d\n", filename, tgaHeader.image_type);
		return false;
	}

	if (tgaHeader.colormap_type != 0)
	{
		return false;
	}

	if (tgaHeader.pixel_size != 24 && tgaHeader.pixel_size !=32)
	{
		return false;
	}

	if (tgaHeader.id_length != 0)
	{
		g_pFullFileSystem->Seek( file, tgaHeader.id_length, FILESYSTEM_SEEK_CURRENT );
	}

	// allocate memory for the destination
	uchar *rgba = (unsigned char *)malloc(tgaHeader.width * tgaHeader.height * 4);

	int column, row;
	uchar *ptr;
	bool bMask = false;

	if (tgaHeader.image_type == 2)
	{
		for (row = tgaHeader.height - 1; row >= 0; row--)
		{
			ptr = ((uchar *)rgba) + (row * tgaHeader.width * 4);
			for (column = 0; column < tgaHeader.width; column++) 
			{
				switch (tgaHeader.pixel_size) 
				{
					case 24:
					{
						g_pFullFileSystem->Read(ptr + 2, 1, file);
						g_pFullFileSystem->Read(ptr + 1, 1, file);
						g_pFullFileSystem->Read(ptr + 0, 1, file);
						ptr[3] = 255;

						if (invertAlpha)
						{
							ptr[3] = 0;
						}
						ptr += 4;
						break;
					}
					case 32:
					{
						g_pFullFileSystem->Read(ptr + 2, 1, file);
						g_pFullFileSystem->Read(ptr + 1, 1, file);
						g_pFullFileSystem->Read(ptr + 0, 1, file);
						g_pFullFileSystem->Read(ptr + 3, 1, file);
						
						if (!invertAlpha)
						{
							// if it's 0, then it's going to be zeroed out
							if (ptr[3] == 0)
							{
								ptr[3] = 255;
							}
							else
							{
								// anything besides 0 will be shown
								ptr[3] = 0;
							}
							//ptr[3] = 255 - ptr[3];
						}
						bMask = true;

						ptr += 4;
						break;
					}
				}
			}
		}
	}
	else
	{
		uchar packetHeader, packetSize, j, color[4];

		for(row = tgaHeader.height - 1; row >= 0; row--)
		{
			ptr = ((uchar*)rgba) + row * tgaHeader.width * 4;
			for(column=0;column<tgaHeader.width;) 
			{
				g_pFullFileSystem->Read(&packetHeader, 1, file);
				packetSize = 1 + (packetHeader & 0x7f);

				if (packetHeader & 0x80)
				{
					switch (tgaHeader.pixel_size)
					{
						case 24:
						{
							g_pFullFileSystem->Read(color + 2, 1, file);
							g_pFullFileSystem->Read(color + 1, 1, file);
							g_pFullFileSystem->Read(color + 0, 1, file);

							if (invertAlpha)
							{
								color[3] = 0;
							}
							else
							{
								color[3] = 255;
							}

							break;
						}
						case 32:
						{
							g_pFullFileSystem->Read(color + 2, 1, file);
							g_pFullFileSystem->Read(color + 1, 1, file);
							g_pFullFileSystem->Read(color + 0, 1, file);
							g_pFullFileSystem->Read(color + 3, 1, file);

							bMask = true;
							if (invertAlpha)
							{
								color[3] = 255 - color[3];
							}

							break;
						}
					}
					
					for (j = 0; j < packetSize; j++)
					{
						ptr[0] = color[0];
						ptr[1] = color[1];
						ptr[2] = color[2];
						ptr[3] = color[3];
						ptr += 4;
						column++;
						if (column == tgaHeader.width)
						{
							column = 0;
							if (row > 0)
							{
								row--;
							}
							else
							{
								goto breakOut;
							}
							ptr = ((uchar*)rgba) + (row * tgaHeader.width * 4);
						}
					}
				}
				else
				{
					for (j = 0; j < packetSize; j++) 
					{
						switch (tgaHeader.pixel_size)
						{
							case 24:
							{
								g_pFullFileSystem->Read(ptr + 2, 1, file);
								g_pFullFileSystem->Read(ptr + 1, 1, file);
								g_pFullFileSystem->Read(ptr + 0, 1, file);

								ptr[3] = 255;

								if (invertAlpha)
								{
									ptr[3] = 0;
								}
								else
								{
									ptr[3] = 255;
								}

								ptr += 4;
								break;
							}
							case 32:
							{
								g_pFullFileSystem->Read(ptr + 2, 1, file);
								g_pFullFileSystem->Read(ptr + 1, 1, file);
								g_pFullFileSystem->Read(ptr + 0, 1, file);
								g_pFullFileSystem->Read(ptr + 3, 1, file);

								if (invertAlpha)
								{
									ptr[3]=255-ptr[3];
								}

								ptr += 4;
								break;
							}
						}
						column++;
						if (column == tgaHeader.width)
						{
							column = 0;
							if(row > 0)
							{
								row--;
							}
							else
							{
								goto breakOut;
							}
							ptr = ((uchar*)rgba) + row * tgaHeader.width * 4;
						}
					}
				}
			}
		}
		breakOut:;
	}

	g_pFullFileSystem->Close(file);


	// we now have a block of memory, rgba
	// throw a bitmap header on it and register it in windows
	texture->_wide = tgaHeader.width;
	texture->_tall = tgaHeader.height;
	texture->_icon = NULL;
	texture->_bitmap = staticCreateBitmapHandle(tgaHeader.width, tgaHeader.height, PLAT(_currentContextPanel)->hdc, 32, &texture->_dib);
	texture->_bMask = bMask;
	if (bMask)
		texture->_maskBitmap = staticCreateBitmapHandle(tgaHeader.width, tgaHeader.height, PLAT(_currentContextPanel)->hdc, 32, &texture->_maskDib);
	else
		texture->_maskBitmap = NULL;

	for (int j = 0; j < texture->_tall; j++)
	{
		for (int i = 0; i < texture->_wide; i++)
		{
			int offs = (j * texture->_wide + i) * 4;
			char *src = ((char*)rgba) + offs;
			char *dst = ((char*)texture->_dib) + offs;
			
			if (bMask)
			{
				char *maskDst = ((char*) texture->_maskDib) + offs;

				// if there's value here, then it needs to be cleared
				if (src[3])
				{
					// mask will be & with background, so it needs to be 0xFF
					maskDst[0] = maskDst[1] = maskDst[2] = maskDst[3] = -1;

					// clear the art in the bitmap because it's not going to display and will be | with background
					dst[0] = dst[1] = dst[2] = dst[3] = 0;
				}
				else
				{
					// this will clear the background before drawing this bitmap
					maskDst[0] = maskDst[1] = maskDst[2] = maskDst[3] = 0x00;

					// copy over the art
					dst[0] = src[2];
					dst[1] = src[1];
					dst[2] = src[0];
					dst[3] = src[3];
				}
			}
			else
			{
				dst[0] = src[2];
				dst[1] = src[1];
				dst[2] = src[0];
				dst[3] = src[3];
			}
		}
	}

	free(rgba);
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: brings the current surface to the foreground
//-----------------------------------------------------------------------------
void CWin32Surface::BringToFront(VPANEL panel)
{
	// force the panel to the top of the windows z-order
	panel = GetContextPanelForChildPanel(panel);
	if (panel && PLAT(panel))
	{
		// this should trigger a WM_SETFOCUS message which will move the window to the top
		::SetActiveWindow(PLAT(panel)->hwnd);
	}
}


//-----------------------------------------------------------------------------
// Purpose: puts the thread that created the specified window into the foreground 
//          and activates the window.
//-----------------------------------------------------------------------------
void CWin32Surface::SetForegroundWindow(VPANEL panel)
{
	if (panel && PLAT(panel))
	{
		::SetForegroundWindow(PLAT(panel)->hwnd);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::MovePopupToFront(VPANEL panel)
{
	_popupList.MoveElementToEnd(panel);

	g_pIVgui->PostMessage(panel, new KeyValues("OnMovedPopupToFront"), NULL);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::MovePopupToBack(VPANEL)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// HWND_TOPMOST - Places the window above all non-topmost windows. 
// The window maintains its topmost position even when it is deactivated.
// HWND_NOTOPMOST - Places the window above all non-topmost windows (that is, behind 
// all topmost windows). This flag has no effect if the window is already a non-topmost window.
//-----------------------------------------------------------------------------
void CWin32Surface::SetAsTopMost(VPANEL panel, bool state)
{
	panel = GetContextPanelForChildPanel(panel);
	DWORD style=SWP_NOMOVE|SWP_NOSIZE;	
	
	if (PLAT(panel)->disabled)
	{
		style |= SWP_NOACTIVATE;
	}
		
	if (state)
	{
		SetFocus(PLAT(panel)->hwnd);
		SetWindowPos(PLAT(panel)->hwnd,HWND_TOPMOST,0,0,0,0,style  );
	}
	else
	{
		SetWindowPos(PLAT(panel)->hwnd,HWND_NOTOPMOST,0,0,0,0,style );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::SetPanelVisible(VPANEL panel, bool visible)
{
	// if the panel has an attached window we need to set it's style
	if (PLAT(panel))
	{
		if (visible)
		{
			// show the window
			::ShowWindow(PLAT(panel)->hwnd, SW_SHOWNA);
		}
		else
		{
			// hide the window
			::ShowWindow(PLAT(panel)->hwnd, SW_HIDE);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Flashes the window icon in the taskbar, to get the users attention
// Input  : flashCount - number of times to flash the window
//-----------------------------------------------------------------------------
void CWin32Surface::FlashWindow(VPANEL panel, bool state)
{
	if (!PLAT(panel))
		return;

	::FlashWindow(PLAT(panel)->hwnd, state);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::SetTopLevelFocus(VPANEL)
{
	// this is handled by WM_FOCUS messages instead of directly
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::SetMinimized(VPANEL panel, bool state)
{
	if (PLAT(panel))
	{
		if (state)
		{
			::ShowWindow(PLAT(panel)->hwnd, SW_MINIMIZE);
		}
		else
		{
			::ShowWindow(PLAT(panel)->hwnd, SW_SHOWNORMAL);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the window is minimized
//-----------------------------------------------------------------------------
bool CWin32Surface::IsMinimized(VPANEL panel)
{
	if (PLAT(panel)->hwnd)
	{
		return ::IsIconic(PLAT(panel)->hwnd);
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::SetTitle(VPANEL panel, const wchar_t *title)
{
	panel = GetContextPanelForChildPanel(panel);
	if (panel)
	{
		SetWindowTextW(PLAT(panel)->hwnd, title);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::SetAsToolBar(VPANEL panel, bool state)
{
	panel = GetContextPanelForChildPanel(panel);
	if (panel && PLAT(panel))
	{
		if (state)
		{
			::SetWindowLong(PLAT(panel)->hwnd, GWL_EXSTYLE, ::GetWindowLong(PLAT(panel)->hwnd, GWL_EXSTYLE) | WS_EX_TOOLWINDOW);
		}
		else
		{
			::SetWindowLong(PLAT(panel)->hwnd, GWL_EXSTYLE, ::GetWindowLong(PLAT(panel)->hwnd, GWL_EXSTYLE) & ~WS_EX_TOOLWINDOW);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
intp CWin32Surface::GetPopupCount()
{
	return _popupList.GetCount();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
VPANEL CWin32Surface::GetPopup(intp index)
{
	return _popupList[index];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::CreatePopup(VPANEL panel, bool minimised, bool showTaskbarIcon, bool disabled, bool mouseInput, bool kbInput )
{
	if (((VPanel *)panel)->IsPopup() && PLAT(panel))
	{
		// it's already a popup so bail
		return;
	}

	// make sure it's in the hierarchy
	if (!((VPanel *)panel)->GetParent() && panel != _embeddedPanel)
	{
		((VPanel *)panel)->SetParent((VPanel *)_embeddedPanel);
	}

	int x,y,wide,tall;
	((VPanel *)panel)->GetPos(x, y);
	((VPanel *)panel)->GetSize(wide, tall);
	((VPanel *)panel)->SetPopup(true);
	((VPanel *)panel)->SetKeyBoardInputEnabled(kbInput);
	((VPanel *)panel)->SetMouseInputEnabled(mouseInput);

	// find our parent window if we have one
	HWND hwndParent = NULL;
	VPANEL pParent = GetContextPanelForChildPanel(panel);
	if (pParent && pParent != _embeddedPanel)
	{
		hwndParent = PLAT(pParent)->hwnd;
	}

	//create the window and initialize platform specific data
	//window is initial a popup and not visible
	//when ApplyChanges is called the window will be shown unless
	//it SetVisible(false) is called
	SurfacePlat *plat = new SurfacePlat;
	((VPanel *)panel)->SetPlat(plat);

	// WS_SYSMENU and WS_MINIMIZEBOX flags are there simply to make icon appear in taskbar
	// WS_EX_TOOLWINDOW hides the taskbar/ALT-TAB button
	DWORD style=0,style_ex=0;
	if (!showTaskbarIcon)
	{
		style = WS_POPUP | WS_CLIPCHILDREN;
		style_ex= WS_EX_TOOLWINDOW;
		if (!hwndParent)
		{
			hwndParent = PLAT(_embeddedPanel)->hwnd;
		}
	}
	else
	{
		style = WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
	}
	if (panel != _embeddedPanel && ((VPanel *)panel)->IsVisible())
	{
		style |= WS_VISIBLE;
	}

	if(disabled)
	{
		style |= WS_DISABLED;
	}

	if ( minimised )
	{
		style |= WS_MINIMIZE;
	}

	plat->hwnd = CreateWindowEx(style_ex, "Surface", "", style, x, y, wide, tall, hwndParent, NULL, GetModuleHandle(NULL), NULL);

	plat->clipRgn = CreateRectRgn(0,0,64,64);
	plat->hdc = CreateCompatibleDC(NULL);
	plat->hwndDC = NULL;
	plat->bitmap = nullptr;
	plat->bitmapSize[0] = 0;
	plat->bitmapSize[1] = 0;
	plat->isFullscreen = false;
	plat->embeddedPanel = (VPanel *)panel;
	plat->disabled=disabled;
	plat->notifyIcon = NULL;
	plat->textureDC = NULL;

	::SetBkMode(plat->hdc, TRANSPARENT);
	::SetWindowLongPtr(plat->hwnd, GWLP_USERDATA, (LONG_PTR)g_pIVgui->PanelToHandle(panel));
	::SetTextAlign(plat->hdc, TA_LEFT | TA_TOP | TA_UPDATECP);
	
	if (!((VPanel *)panel)->IsVisible() || panel == _embeddedPanel)
	{
		SetPanelVisible(panel, false);
	}

	// create the context
	RecreateContext(panel);

	::RegisterDragDrop(plat->hwnd, &staticDragDropTarget);

	// add the panel to the popup list
	if (!_popupList.HasElement(panel))
	{
		_popupList.AddElement(panel);
	}
	else
	{
		// somehow getting added twice, fundamental problem
		DebuggerBreak();
	}

	// hack, force a windows sound to be played
	// the first time PlaySound is called there is 300ms hang, so this forces it to happen at startup
	// instead of while the user is trying to do something
	// we can't do this before a window created else it'll hang
	static bool first = true;
	if (first)
	{
		// double startTime = system()->GetCurrentTime();
		::PlaySoundA("", NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT | SND_NOSTOP | SND_NOWAIT);
		// double endTime = system()->GetCurrentTime();
		// ivgui()->DPrintf2("PlaySound() : %dms\n", (int)((endTime - startTime) * 1000));
		first = false;
	}

	//!! hack to set a valid current context panel
	SetCurrentContextPanel(_embeddedPanel);
}

//-----------------------------------------------------------------------------
// Purpose: Called when a panel is created
//-----------------------------------------------------------------------------
void CWin32Surface::AddPanel(VPANEL)
{
}

//-----------------------------------------------------------------------------
// Purpose: Called when a panel gets deleted
//-----------------------------------------------------------------------------
void CWin32Surface::ReleasePanel(VPANEL panel)
{
	_popupList.RemoveElement(panel);

	if (panel == _currentContextPanel)
	{
		_currentContextPanel = _embeddedPanel;
	}

	if (PLAT(panel))
	{
		SurfacePlat *plat = PLAT(panel);

		// release drag/drop
		::RevokeDragDrop(plat->hwnd);

		// remove notify icons
		if (plat->notifyIcon)
		{
			SetNotifyIcon(panel, NULL, NULL, NULL);
		}

		// hide the panel
		SetPanelVisible(panel, false);

		// free all the windows/bitmap/DC handles we are using
		::SetWindowLongPtr(plat->hwnd, GWLP_USERDATA, (LONG_PTR)INVALID_PANEL);
		::SetWindowPos(plat->hwnd, HWND_BOTTOM, 0, 0, 1, 1, SWP_NOREDRAW|SWP_HIDEWINDOW);

		// free the window context
		if ( plat->bitmap )
		{
			::DeleteObject( plat->bitmap );
		}

		if ( plat->textureDC )
		{
			::DeleteDC( plat->textureDC );
		}

		if (plat->hwndDC)
		{
			::ReleaseDC( plat->hwnd, plat->hwndDC );
		}
		::DeleteDC( plat->hdc );
		::DestroyWindow( plat->hwnd );
		::DeleteObject( plat->clipRgn );

		// don't free staticWndclassAtom, because it is shared amongst surfaces,
		// and is automatically freed when the application terminates

		delete plat;
		((VPanel *)panel)->SetPlat(NULL);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Applies any changes to the panel into the underline wnidow
//-----------------------------------------------------------------------------
bool CWin32Surface::RecreateContext(VPANEL panel)
{
	if (panel && PLAT(panel))
	{
		SurfacePlat *plat = PLAT(panel);
		int wide,tall;
		((VPanel *)panel)->GetSize(wide,tall);

		// rebuild bitmap only if necessary
		// simple scheme to prevent excessive allocations by allocating only when
		// bigger. It also adds in 100 extra for subsequent sizings
		// it will also realloc if the size is 200 smaller to shrink memory usage
		if ((wide > plat->bitmapSize[0]) 
			|| (tall > plat->bitmapSize[1])
			|| (wide < (plat->bitmapSize[0] - 200)) 
			|| (tall < (plat->bitmapSize[1] - 200)))
		{
			if (plat->bitmap != nullptr)
			{
				::DeleteObject(plat->bitmap);
			}

			plat->hwndDC = GetDC(plat->hwnd);

			plat->bitmap = ::CreateCompatibleBitmap(plat->hwndDC, wide + 100, tall + 100);
			plat->bitmapSize[0] = wide + 100;
			plat->bitmapSize[1] = tall + 100;

			if (plat->textureDC)
			{
				::DeleteDC(plat->textureDC);
			}

			::SelectObject(plat->hdc, plat->bitmap);
			plat->textureDC = ::CreateCompatibleDC(plat->hdc);

			::ReleaseDC(plat->hwnd, plat->hwndDC);
			plat->hwndDC = NULL;
		}
	}
 
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::EnableMouseCapture(VPANEL panel, bool state)
{
	VPANEL contextPanel = GetContextPanelForChildPanel(panel);
	if (state)
	{
		::SetCapture(PLAT(contextPanel)->hwnd);
	}
	else
	{
		::ReleaseCapture();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWin32Surface::ShouldPaintChildPanel(VPANEL childPanel)
{
	// don't Paint children as part of the normal process, handle them in with WM_PAINT messages instead
	return !((VPanel *)childPanel)->IsPopup();
}

//-----------------------------------------------------------------------------
// Purpose: Called after a Paint to display the new buffer
//-----------------------------------------------------------------------------
void CWin32Surface::SwapBuffers(VPANEL panel)
{
	if (PLAT(panel))
	{
		START_TIMER();
		
		SurfacePlat *plat = PLAT(panel);

		int wide,tall;
		((VPanel *)panel)->GetSize(wide,tall);
		
		plat->hwndDC = ::GetDC(plat->hwnd);

		// reset origin and clipping then blit
		::SetRectRgn(plat->clipRgn, 0, 0, wide, tall);
		::SelectObject(plat->hdc, plat->clipRgn);
		::SetViewportOrgEx(plat->hdc, 0, 0, NULL);
		::BitBlt(plat->hwndDC, 0, 0, wide, tall, plat->hdc, 0, 0, SRCCOPY);

		::ReleaseDC(plat->hwnd, plat->hwndDC);
		plat->hwndDC = NULL;

		END_TIMER("SwapBuffers time: %.2fms\n");
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame to change to window state if necessary
//-----------------------------------------------------------------------------
void CWin32Surface::ApplyChanges()
{
	for (int i = 0; i < GetPopupCount(); i++)
	{
		VPANEL panel = GetPopup(i);

		if (!PLAT(panel))
			continue;

		SurfacePlat *Plat = PLAT(panel);

		// force the main panel to be invisible
		if (panel == GetEmbeddedPanel())
		{
			::ShowWindow(Plat->hwnd, SW_HIDE);
			continue;
		}

		// hide and skip by invisible panels
		if (!((VPanel *)panel)->IsVisible())
		{
			::ShowWindow(Plat->hwnd, SW_HIDE);
			continue;
		}

		RECT rect;
		int x, y, wide, tall, sx, sy, swide, stall;

		// get how big the win32 window
		::GetWindowRect(Plat->hwnd, &rect);
		sx = rect.left;
		sy = rect.top;
		swide = rect.right-rect.left;
		stall = rect.bottom-rect.top;

		// how big is the embedded VPanel
		((VPanel *)panel)->GetPos(x, y);
		((VPanel *)panel)->GetSize(wide, tall);

		// if they are not the same, then adjust the win32 window so it is
		if ((x != sx) || (y != sy) || (wide != swide) || (tall != stall))
		{
			::SetWindowPos(Plat->hwnd, nullptr, x, y, wide, tall, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
			if ( sx > 0 || sy > 0 ) // only message for moves that are on the screen
			{
				g_pIVgui->PostMessage(panel, new KeyValues("Move"), NULL ); 
			}
		}

		// check to see if the win32 window is visible
		if (::GetWindowLong(Plat->hwnd, GWL_STYLE) & WS_VISIBLE)
		{
			//check to see if embedded VPanel is not visible, if so then hide the win32 window
			if (!((VPanel *)panel)->IsVisible())
			{
				::ShowWindow(Plat->hwnd, SW_HIDE);
			}
		}
		else // win32 window is hidden
		{
			//check to see if embedded VPanel is visible, if so then show the win32 window
			if (((VPanel *)panel)->IsVisible())
			{
				::ShowWindow(Plat->hwnd, SW_SHOWNA);
			}
		}

		//if the win32 window changed size and is visible, then the context needs to be recreated
		if (((wide != swide) || (tall != stall)) && ((VPanel *)panel)->IsVisible())
		{
			RecreateContext(panel);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: recurses the panels calculating absolute positions
//			parents must be solved before children
//-----------------------------------------------------------------------------
void CWin32Surface::InternalSolveTraverse(VPANEL panel)
{
	// solve the parent
	((VPanel *)panel)->Solve();
	
	// now we can solve the children
	for (int i = 0; i < ((VPanel *)panel)->GetChildCount(); i++)
	{
		VPanel *child = ((VPanel *)panel)->GetChild(i);
		if (child->IsVisible())
		{
			InternalSolveTraverse((VPANEL)child);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: recurses the panels giving them a chance to do a user-defined think,
//			PerformLayout and ApplySchemeSettings
//			must be done child before parent
//-----------------------------------------------------------------------------
void CWin32Surface::InternalThinkTraverse(VPANEL panel)
{
	// parent
	((VPanel *)panel)->Client()->Think();

	// then the children...
	for (int i = 0; i < ((VPanel *)panel)->GetChildCount(); i++)
	{
		VPanel *child = ((VPanel *)panel)->GetChild(i);
		if (child->IsVisible())
		{
			InternalThinkTraverse((VPANEL)child);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: recurses the panels giving them a chance to do a ApplySchemeSettings
//			must be done child before parent
//-----------------------------------------------------------------------------
void CWin32Surface::InternalSchemeSettingsTraverse(VPANEL panel, bool forceApplySchemeSettings)
{
	// think the children...
	for (int i = 0; i < ((VPanel *)panel)->GetChildCount(); i++)
	{
		VPanel *child = ((VPanel *)panel)->GetChild(i);
		if ( forceApplySchemeSettings || child->IsVisible() )
		{
			InternalSchemeSettingsTraverse((VPANEL)child, forceApplySchemeSettings);
		}
	}

	// ... then the parent
	((VPanel *)panel)->Client()->PerformApplySchemeSettings();
}

//-----------------------------------------------------------------------------
// Purpose: Walks through the panel tree calling Solve() on them all, in order
//-----------------------------------------------------------------------------
void CWin32Surface::SolveTraverse(VPANEL panel, bool forceApplySchemeSettings)
{
	// ignore visibility for this
	InternalSchemeSettingsTraverse(panel,forceApplySchemeSettings); // do apply scheme settings, child to parent

	if (!((VPanel *)panel)->IsVisible())
		return;

	InternalThinkTraverse(panel);
	InternalSolveTraverse(panel);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::PaintTraverse(VPANEL panel)
{
	START_TIMER();

	((VPanel *)panel)->Client()->PaintTraverse(false,true);

	END_TIMER("Paint time: %.2fms\n");
}

// FIXME: write these functions!
void CWin32Surface::RestrictPaintToSinglePanel(VPANEL)
{
}

void CWin32Surface::SetModalPanel(VPANEL )
{
}

VPANEL CWin32Surface::GetModalPanel()
{
return 0;
}

void CWin32Surface::UnlockCursor()
{
}

void CWin32Surface::LockCursor()
{
}

void CWin32Surface::SetTranslateExtendedKeys(bool)
{
}

VPANEL CWin32Surface::GetTopmostPopup()
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if the mouse should be visible or not
//-----------------------------------------------------------------------------
void CWin32Surface::CalculateMouseVisible()
{
	int i;
	_needMouse = false;
	_needKB = false;

	for(i = 0 ; i < g_pSurface->GetPopupCount() ; i++ )
	{
		VPanel *pop = (VPanel *)g_pSurface->GetPopup( i ) ;
		
		bool isVisible=pop->IsVisible();
		VPanel *p= pop->GetParent();

		while(p && isVisible)
		{
			if( p->IsVisible()==false)
			{
				isVisible=false;
				break;
			}
			p=p->GetParent();
		}
	
		if(isVisible)
		{
			_needMouse |= pop->IsMouseInputEnabled();
			_needKB |= pop->IsKeyBoardInputEnabled();
		}
	}

	if (_needMouse)
	{
		SetCursor(vgui::dc_arrow);
		UnlockCursor();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWin32Surface::NeedKBInput()
{
	return _needKB;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the current cursor
//-----------------------------------------------------------------------------
void CWin32Surface::SetCursor(HCursor cursor)
{
	switch (cursor)
	{
		case dc_none:
		case dc_arrow:
		case dc_ibeam:
		case dc_hourglass:
		case dc_waitarrow:
		case dc_crosshair:
		case dc_up:
		case dc_sizenwse:
		case dc_sizenesw:
		case dc_sizewe:
		case dc_sizens:
		case dc_sizeall:
		case dc_no:
		case dc_hand:
		{
			_currentCursor = staticDefaultCursor[cursor];
			break;
		}
		case dc_user:
		{
			break;
		}
		case dc_blank: // don't touch the cursor, just stick with what windows is currently using
		{
			return;
			break;
		}
	}
	
	::SetCursor(_currentCursor);
}

//-----------------------------------------------------------------------------
// Purpose: Forces the window to be redrawn
//-----------------------------------------------------------------------------
void CWin32Surface::Invalidate(VPANEL panel)
{
	panel = GetContextPanelForChildPanel(panel);
	if (panel && PLAT(panel) && ((VPanel *)panel)->IsVisible())
	{
		// last parm must be false so WM_ERASEBKGND is not generated, that should
		// only be generated by windows
		InvalidateRect(PLAT(panel)->hwnd, NULL, false);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if any of the windows have focus
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWin32Surface::HasFocus()
{
	HWND focus = ::GetFocus();
	if (!focus)
		return false;

	// see if any of the windows on the surface have the focus
	for (int i = 0; i < GetPopupCount(); i++)
	{
		VPANEL panel = GetPopup(i); 
		if (focus == PLAT(panel)->hwnd)
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the cursor is over this surface
//			Uses the windows call to do this, instead of doing it procedurally
//-----------------------------------------------------------------------------
bool CWin32Surface::IsWithin(int x,int y)
{
	POINT pnt={x,y};
	HWND hwnd = WindowFromPoint(pnt);

	for (int i = 0; i < GetPopupCount(); i++)
	{
		if (PLAT(GetPopup(i))->hwnd == hwnd)
		{
			return true;
		}
	}

 	return false;
}

//-----------------------------------------------------------------------------
// Purpose: handles a set focus message
//-----------------------------------------------------------------------------
void CWin32Surface::setFocus(VPANEL panel)
{
	// reset the focus to the last focused panel
	panel = GetContextPanelForChildPanel(panel);
	if (panel && PLAT(panel))
	{
		// make sure we have a valid panel
		VPANEL focus = panel;
		if (focus && ((VPanel *)focus)->HasParent((VPanel *)panel))
		{
			// Must be a child of the modal surface, if set
			if ( g_pInput->GetAppModalSurface() && !((VPanel *)focus)->HasParent((VPanel *)g_pInput->GetAppModalSurface()) )
			{
				// trying to set focus to something that's not the modal panel, set it back
				SetForegroundWindow(g_pInput->GetAppModalSurface());
			}
			else
			{
				// make sure focus is at top of list
				// the subfocus will be automatically calculated in IInput::RunFrame()
				((VPanel *)focus)->MoveToFront();
				return;
			}
		}
	}
		
	// let the main panel get focus as the default
	if (panel)
	{
		((VPanel *)panel)->Client()->RequestFocus(0);
	}
}

struct FontRangeItem_t
{
	const char *name;
	int lowerRange;
	int upperRange;
};

FontRangeItem_t g_FontRanges[] = 
{
	{ "Basic Latin",							0x0000, 0x007F },
	{ "Latin-1 Supplement",						0x0080, 0x00FF },
	{ "Latin Extended-A",						0x0100, 0x017F },
	{ "Latin Extended-B",						0x0180, 0x024F },
	{ "IPA Extensions",							0x0250, 0x02AF },
	{ "Spacing Modifier Letters",				0x02B0, 0x02FF },
	{ "Combining Diacritical Marks",			0x0300, 0x036F },
	{ "Greek",									0x0370, 0x03FF },
	{ "Cyrillic",								0x0400, 0x04FF },
	{ "Armenian",								0x0530, 0x058F },
	{ "Hebrew",									0x0590, 0x05FF },
	{ "Arabic",									0x0600, 0x06FF },
	{ "Syriac",									0x0700, 0x074F },
	{ "Thaana",									0x0780, 0x07BF },
	{ "Devanagari",								0x0900, 0x097F },
	{ "Bengali",								0x0980, 0x09FF },
	{ "Gurmukhi",								0x0A00, 0x0A7F },
	{ "Gujarati",								0x0A80, 0x0AFF },
	{ "Oriya",									0x0B00, 0x0B7F },
	{ "Tamil",									0x0B80, 0x0BFF },
	{ "Telugu",									0x0C00, 0x0C7F },
	{ "Kannada",								0x0C80, 0x0CFF },
	{ "Malayalam",								0x0D00, 0x0D7F },
	{ "Sinhala",								0x0D80, 0x0DFF },
	{ "Thai",									0x0E00, 0x0E7F },
	{ "Lao",									0x0E80, 0x0EFF },
	{ "Tibetan",								0x0F00, 0x0FBF },
	{ "Myanmar",								0x1000, 0x109F },
	{ "Georgian",								0x10A0, 0x10FF },
	{ "Hangul Jamo",							0x1100, 0x11FF },
	{ "Ethiopic",								0x1200, 0x137F },
	{ "Cherokee",								0x13A0, 0x13FF },
	{ "Unified Canadian Aboriginal Syllabics",	0x1400, 0x167F },
	{ "Ogham",									0x1680, 0x169F },
	{ "Runic",									0x16A0, 0x16FF },
	{ "Khmer",									0x1780, 0x17FF },
	{ "Mongolian",								0x1800, 0x18AF },
	{ "Latin Extended Additional",				0x1E00, 0x1EFF },
	{ "Greek Extended",							0x1F00, 0x1FFF },
	{ "General Punctuation",					0x2000, 0x206F },
	{ "Superscripts and Subscripts",			0x2070, 0x209F },
	{ "Currency Symbols",						0x20A0, 0x20CF },
	{ "Combining Diacritical Marks for Symbols",0x20D0, 0x20FF },
	{ "Letterlike Symbols",						0x2100, 0x214F },
	{ "Number Forms",							0x2150, 0x218F },
	{ "Arrows",									0x2190, 0x21FF },
	{ "Mathematical Operators",					0x2200, 0x22FF },
	{ "Miscellaneous Technical",				0x2300, 0x23FF },
	{ "Control Pictures",						0x2400, 0x243F },
	{ "Optical Character Recognition",			0x2440, 0x245F },
	{ "Enclosed Alphanumerics",					0x2460, 0x24FF },
	{ "Box Drawing",							0x2500, 0x257F },
	{ "Block Elements",							0x2580, 0x259F },
	{ "Geometric Shapes",						0x25A0, 0x25FF },
	{ "Miscellaneous Symbols",					0x2600, 0x26FF },
	{ "Dingbats",								0x2700, 0x27BF },
	{ "Braille Patterns",						0x2800, 0x28FF },
	{ "CJK Radicals Supplement",				0x2E80, 0x2EFF },
	{ "KangXi Radicals",						0x2F00, 0x2FDF },
	{ "Ideographic Description Characters",		0x2FF0, 0x2FFF },
	{ "CJK Symbols and Punctuation",			0x3000, 0x303F },
	{ "Hiragana",								0x3040, 0x309F },
	{ "Katakana",								0x30A0, 0x30FF },
	{ "Bopomofo",								0x3100, 0x312F },
	{ "Hangul Compatibility Jamo",				0x3130, 0x318F },
	{ "Kanbun",									0x3190, 0x319F },
	{ "Bopomofo Extended",						0x31A0, 0x31BF },
	{ "Enclosed CJK Letters and Months",		0x3200, 0x32FF },
	{ "CJK Compatibility",						0x3300, 0x33FF },
	{ "CJK Unified Ideographs Extension A",		0x3400, 0x4DB5 },
	{ "CJK Unified Ideographs",					0x4E00, 0x9FFF },
	{ "Yi Syllables",							0xA000, 0xA48F },
	{ "Yi Radicals",							0xA490, 0xA4CF },
	{ "Hangul Syllables",						0xAC00, 0xD7A3 },
	{ "-- surrogates --",						0xD800, 0xDBFF },
	{ "CJK Compatibility Ideographs",			0xF900, 0xFAFF },
	{ "Alphabetic Presentation Forms",			0xFB00, 0xFB4F },
	{ "Arabic Presentation Forms-A",			0xFB50, 0xFDFF },
	{ "Combining Half Marks",					0xFE20, 0xFE2F },
	{ "CJK Compatibility Forms",				0xFE30, 0xFE4F },
	{ "Small Form Variants",					0xFE50, 0xFE6F },
	{ "Arabic Presentation Forms-B",			0xFE70, 0xFEFF },
	{ "Halfwidth and Fullwidth Forms",			0xFF00, 0xFFEF },
	{ "Specials",								0xFFF0, 0xFFFF },
};


//-----------------------------------------------------------------------------
// Purpose: creates a new empty font
//-----------------------------------------------------------------------------
HFont CWin32Surface::CreateFont()
{
	return FontManager().CreateFont();
}

//-----------------------------------------------------------------------------
// Purpose: adds glyphs to a font created by CreateFont()
//-----------------------------------------------------------------------------
bool CWin32Surface::SetFontGlyphSet(HFont font, const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags, int nRangeMin, int nRangeMax)
{
	return FontManager().SetFontGlyphSet(font, windowsFontName, tall, weight, blur, scanlines, flags, nRangeMin, nRangeMax);
}

//-----------------------------------------------------------------------------
// Purpose: returns the max height of a font
//-----------------------------------------------------------------------------
int CWin32Surface::GetFontTall(HFont font)
{
	return FontManager().GetFontTall(font);
}

//-----------------------------------------------------------------------------
// Purpose: returns the requested height of a font
//-----------------------------------------------------------------------------
int CWin32Surface::GetFontTallRequested(HFont font)
{
	return FontManager().GetFontTallRequested(font);
}

//-----------------------------------------------------------------------------
// Purpose: returns the max height of a font
//-----------------------------------------------------------------------------
int CWin32Surface::GetFontAscent(HFont font, wchar_t wch)
{
	return FontManager().GetFontAscent(font,wch);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : font - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWin32Surface::IsFontAdditive(HFont font)
{
	return FontManager().IsFontAdditive(font);
}

//-----------------------------------------------------------------------------
// Purpose: returns the abc widths of a single character
//-----------------------------------------------------------------------------
void CWin32Surface::GetCharABCwide(HFont font, wchar_t ch, int &a, int &b, int &c)
{
	FontManager().GetCharABCwide(font, ch, a, b, c);
}

//-----------------------------------------------------------------------------
// Purpose: returns the pixel width of a single character
//-----------------------------------------------------------------------------
int CWin32Surface::GetCharacterWidth(HFont font, wchar_t ch)
{
	return FontManager().GetCharacterWidth(font, ch);
}

//-----------------------------------------------------------------------------
// Purpose: returns the area of a text string, including newlines
//-----------------------------------------------------------------------------
void CWin32Surface::GetTextSize(HFont font, const wchar_t *text, int &wide, int &tall)
{
	FontManager().GetTextSize(font, text, wide, tall);
}

//-----------------------------------------------------------------------------
// Purpose: adds a custom font file (only supports true type font files (.ttf) for now)
//-----------------------------------------------------------------------------
bool CWin32Surface::AddCustomFontFile(const char *, const char *fontFileName)
{
	char fullPath[ MAX_PATH ];
	g_pFullFileSystem->GetLocalPath_safe(fontFileName, fullPath);
	m_CustomFontFileNames.AddToTail(fontFileName);
	return (::AddFontResource(fullPath) > 0);
}

//-----------------------------------------------------------------------------
// Purpose: Pre-compiled bitmap font support for game engine - not implemented for GDI
//-----------------------------------------------------------------------------
bool CWin32Surface::AddBitmapFontFile(const char *)
{
	Assert( 0 );
	return false;
}
void CWin32Surface::SetBitmapFontName( const char *, const char * )
{
	Assert( 0 );
}
const char *CWin32Surface::GetBitmapFontName( const char * )
{
	Assert( 0 );
	return NULL;
}
bool CWin32Surface::SetBitmapFontGlyphSet(HFont, const char *, float, float, int)
{
	Assert( 0 );
	return false;
}

void CWin32Surface::PrecacheFontCharacters(HFont, const wchar_t *)
{
	Assert( 0 );
}

void CWin32Surface::ClearTemporaryFontCache( void )
{
	Assert( 0 );
}

const char *CWin32Surface::GetFontName( HFont font )
{
	return FontManager().GetFontName( font );
}

const char *CWin32Surface::GetFontFamilyName( HFont font )
{
	return FontManager().GetFontFamilyName( font );
}

void CWin32Surface::DrawSetTextScale(float, float)
{
	Assert( 0 );
}
void CWin32Surface::SetPanelForInput( VPANEL )
{
	Assert( 0 );
}

void CWin32Surface::DrawFilledRectFastFade( int, int, int, int, int, int, unsigned int, unsigned int, bool )
{
	Assert( 0 );
}

void CWin32Surface::DrawFilledRectFade( int, int, int, int, unsigned int, unsigned int, bool )
{
	Assert( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the bounds of the usable workspace area
//-----------------------------------------------------------------------------
void CWin32Surface::GetWorkspaceBounds(int &x, int &y, int &wide, int &tall)
{
	RECT rcScreen;
	::SystemParametersInfo(SPI_GETWORKAREA, 0, &rcScreen, 0);

	x = rcScreen.left;
	y = rcScreen.top;
	wide = rcScreen.right - x;
	tall = rcScreen.bottom - y;
}

//-----------------------------------------------------------------------------
// Purpose: gets the absolute coordinates of the screen (in screen space)
//-----------------------------------------------------------------------------
void CWin32Surface::GetAbsoluteWindowBounds(int &x, int &y, int &wide, int &tall)
{
	// always work in full window screen space
	x = 0;
	y = 0;
	GetScreenSize(wide, tall);
}

//-----------------------------------------------------------------------------
// Purpose: Plays a sound
// Input  : *fileName - name of the wav file
//-----------------------------------------------------------------------------
void CWin32Surface::PlaySound(const char *fileName)
{
	char localPath[MAX_PATH];
	if (!g_pFullFileSystem->GetLocalPath_safe(fileName, localPath))
		return;

	g_pFullFileSystem->GetLocalCopy(localPath);
	::PlaySoundA(localPath, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT | SND_NOSTOP | SND_NOWAIT);
}

bool GetIconSize( ICONINFO& iconInfo, int& w, int& h )
{
	w = h = 0;

	HBITMAP bitmap = iconInfo.hbmColor;
	BITMAP bm;
	if ( 0 == GetObject((HGDIOBJ)bitmap, sizeof(BITMAP), (LPVOID)&bm) ) 
	{
        return false; 
	}

	w = bm.bmWidth;
	h = bm.bmHeight;

	return true;
}

class CIconImage : public IImage
{
public:
	CIconImage( HICON hIcon ) : m_hIcon( CopyIcon( hIcon ) )
	{
		m_Pos.x = m_Pos.y = 0;

		ICONINFO iconInfo;
		if ( 0 != GetIconInfo( m_hIcon, &iconInfo ) )
		{
			int w, h;
			GetIconSize( iconInfo, w, h );
			m_Size.cx = w;
			m_Size.cy = h;
		}
		else
		{
			m_Size.cx = 0;
			m_Size.cy = 0;
		}
	}

	// virtual destructor
	virtual ~CIconImage()
	{
		DestroyIcon( m_hIcon );
	}

	// Call to Paint the image
	// Image will draw within the current panel context at the specified position
	virtual void Paint()
	{
		if ( !m_hIcon )
			return;

		if ( !m_Size.cx || !m_Size.cy )
			return;

		HDC hdc = PLAT(g_Surface._currentContextPanel)->hdc;

		// Translate position to screen space based on surface state...
		int x, y;
		x = m_Pos.x;
		y = m_Pos.y;

		DrawIconEx
		( 
			hdc,
			x, y, 
			m_hIcon, 
			m_Size.cx, m_Size.cy,
			0,
			NULL,
			DI_NORMAL 
		);
	}

	// Set the position of the image
	virtual void SetPos(int x, int y)
	{
		m_Pos.x = x;
		m_Pos.y = y;
	}

	// Gets the size of the content
	virtual void GetContentSize(int &wide, int &tall)
	{
		wide = m_Size.cx;
		tall = m_Size.cy;
	}

	// Get the size the image will actually draw in (usually defaults to the content size)
	virtual void GetSize(int &wide, int &tall) 
	{
		GetContentSize( wide, tall );
	}

	// Sets the size of the image
	virtual void SetSize(int, int)
	{
		// Nothing
	}

	// Set the draw color 
	virtual void SetColor(Color)
	{
		// Nothing
	}

	virtual bool Evict() { return false; }

	virtual int GetNumFrames() { return 0; }
	virtual void SetFrame( int ) {}
	
	virtual HTexture GetID() { return 0; }
	virtual void SetRotation( int ) { return; };

private:

	HICON		m_hIcon;
	POINT		m_Pos;
	SIZE		m_Size;
};

static bool ShouldMakeUnique( char const *extension )
{
	if ( !Q_stricmp( extension, "cur" ) )
		return true;
	if ( !Q_stricmp( extension, "ani" ) )
		return true;
	return false;
}

IImage *CWin32Surface::GetIconImageForFullPath( char const *pFullPath )
{
	IImage *newIcon = NULL;

	SHFILEINFO info = {};
	DWORD_PTR dwResult = SHGetFileInfo( 
		pFullPath,
		0,
		&info,
		sizeof( info ),
		SHGFI_TYPENAME | SHGFI_ICON | SHGFI_SMALLICON | SHGFI_SHELLICONSIZE 
	);
	if ( dwResult )
	{
		if ( info.szTypeName[ 0 ] != 0 )
		{
			char ext[ 32 ];
			V_ExtractFileExtension( pFullPath, ext );

			char lookup[ 512 ];
			V_sprintf_safe( lookup, "%s", ShouldMakeUnique( ext ) ? pFullPath : info.szTypeName );

			// Now check the dictionary
			auto idx = m_FileTypeImages.Find( lookup );
			if ( idx == m_FileTypeImages.InvalidIndex() )
			{
				newIcon = new CIconImage( info.hIcon );
				idx = m_FileTypeImages.Insert( lookup, newIcon );
			}

			newIcon = m_FileTypeImages[ idx ];
		}

		if ( info.hIcon ) DestroyIcon( info.hIcon );
	}

	return newIcon;
}

//-----------------------------------------------------------------------------
// Purpose: Handles windows message pump
//-----------------------------------------------------------------------------
void CWin32Surface::RunFrame()
{
	::MSG msg;
	if (!g_pIVgui->GetShouldVGuiControlSleep())
	{
		// if vgui doesn't control sleeping, then make sure we don't block in this loop
		if (!::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
			return;
	}

	// always get at least one message
	// this means it will block until input
	// there is a timer set to force this loop to run at least 20Hz, which should be fine for the desktop
	do
	{
		BOOL ret = ::GetMessage(&msg, NULL, 0, 0);
		if (ret == 0)
			break;

		if (ret == -1)
		{
			g_pIVgui->Stop();
			break;
		}

		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}
	while (::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE));
}

//-----------------------------------------------------------------------------
// Purpose: cap bits
//-----------------------------------------------------------------------------
bool CWin32Surface::SupportsFeature(SurfaceFeature_e feature)
{
	switch (feature)
	{
	case ISurface::ESCAPE_KEY:
	case ISurface::ANTIALIASED_FONTS:
	case ISurface::CLEARTYPE_FONTS:
	case ISurface::OPENING_NEW_HTML_WINDOWS:
	case ISurface::FRAME_MINIMIZE_MAXIMIZE:
	case ISurface::DIRECT_HWND_RENDER:
		return true;

	case ISurface::DROPSHADOW_FONTS:
	case ISurface::OUTLINE_FONTS:
	default:
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Initializes all the static data for the app
//			Should be only called during load time
//-----------------------------------------------------------------------------
void CWin32Surface::initStaticData()
{
	//load up all default cursors, this gets called everytime a Surface is created, but
	//who cares
	staticDefaultCursor[dc_none]     =nullptr;
	staticDefaultCursor[dc_arrow]    =(HICON)LoadCursor(nullptr,(LPCTSTR)OCR_NORMAL);
	staticDefaultCursor[dc_ibeam]    =(HICON)LoadCursor(nullptr,(LPCTSTR)OCR_IBEAM);
	staticDefaultCursor[dc_hourglass]=(HICON)LoadCursor(nullptr,(LPCTSTR)OCR_WAIT);
	staticDefaultCursor[dc_waitarrow]=(HICON)LoadCursor(nullptr,(LPCTSTR)OCR_APPSTARTING);
	staticDefaultCursor[dc_crosshair]=(HICON)LoadCursor(nullptr,(LPCTSTR)OCR_CROSS);
	staticDefaultCursor[dc_up]       =(HICON)LoadCursor(nullptr,(LPCTSTR)OCR_UP);
	staticDefaultCursor[dc_sizenwse] =(HICON)LoadCursor(nullptr,(LPCTSTR)OCR_SIZENWSE);
	staticDefaultCursor[dc_sizenesw] =(HICON)LoadCursor(nullptr,(LPCTSTR)OCR_SIZENESW);
	staticDefaultCursor[dc_sizewe]   =(HICON)LoadCursor(nullptr,(LPCTSTR)OCR_SIZEWE);
	staticDefaultCursor[dc_sizens]   =(HICON)LoadCursor(nullptr,(LPCTSTR)OCR_SIZENS);
	staticDefaultCursor[dc_sizeall]  =(HICON)LoadCursor(nullptr,(LPCTSTR)OCR_SIZEALL);
	staticDefaultCursor[dc_no]       =(HICON)LoadCursor(nullptr,(LPCTSTR)OCR_NO);
	staticDefaultCursor[dc_hand]     =(HICON)LoadCursor(nullptr,(LPCTSTR)32649);

	// make and register a very simple Window Class
	memset( &staticWndclass,0,sizeof(staticWndclass) );
	staticWndclass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	staticWndclass.lpfnWndProc = staticProc;
	staticWndclass.hInstance = GetModuleHandle(NULL);

	// Get the resource ID of the icon group from the environment...default to 101.
	DWORD wIconID = 101;
	const char *sIconResourceID = getenv(szSteamBootStrapperIconIdEnvVar);
	if ( sIconResourceID )
	{
		unsigned wTmpIconID = 0;
		if ( sscanf(sIconResourceID, "%u", &wTmpIconID) == 1 && wTmpIconID > 101 )
		{
			wIconID = wTmpIconID;
		}
	}
	staticWndclass.hIcon = ::LoadIcon(staticWndclass.hInstance, MAKEINTRESOURCE(wIconID));

	staticWndclass.lpszClassName = "Surface";
	staticWndclassAtom = ::RegisterClass( &staticWndclass );

	// register our global Shutdown command
	staticShutdownMsg = ::RegisterWindowMessage("ShutdownValvePlatform");
}

//-----------------------------------------------------------------------------
// Purpose: our basic user agent string when doing http requests from the client for webkit
//-----------------------------------------------------------------------------
const char *CWin32Surface::GetWebkitHTMLUserAgentString()
{
	return  "Valve Client";
}


//-----------------------------------------------------------------------------
// Purpose: Handle switching in and out of "render to fullscreen" mode. We don't
//			actually support this mode in tools.
//-----------------------------------------------------------------------------
void CWin32Surface::PushFullscreenViewport()
{
	AssertMsg( false, "Fullscreen viewport mode is unimplemented in CWin32Surface" );
}

void CWin32Surface::PopFullscreenViewport()
{
}


//-----------------------------------------------------------------------------
// Purpose: Handles windows messages sent to the notify tray icon
//-----------------------------------------------------------------------------
static void staticNotifyIconProc(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	switch (lparam)
	{
		case WM_LBUTTONDOWN:
		{
			// notify the window of the double click
			if (g_Surface.GetNotifyPanel())
			{
				g_pIVgui->PostMessage(g_pSurface->GetNotifyPanel(), new KeyValues("NotifyIconMsg", "msg", "WM_LBUTTONDOWN"), NULL);
			}
			break;
		}

		case WM_LBUTTONDBLCLK:
		{
			//HACK: always bring us to front if the user double clicks the icon
			::SetForegroundWindow(hwnd);

			// notify the window of the double click
			if (g_pSurface->GetNotifyPanel())
			{
				g_pIVgui->PostMessage(g_pSurface->GetNotifyPanel(), new KeyValues("NotifyIconMsg", "msg", "LBUTTONDBLCLK"), NULL);
			}
			break;
		}

		case WM_RBUTTONUP:		// win95/98/NT
		case WM_CONTEXTMENU:	// win2k/ME
		{
			// display context menu
			if (g_pSurface->GetNotifyPanel())
			{
				g_pIVgui->PostMessage(g_pSurface->GetNotifyPanel(), new KeyValues("NotifyIconMsg", "msg", "CONTEXTMENU"), NULL);
			}
			break;
		}
		default:
			break;
	}
}

static KeyCode g_iPreviousKeyCode = KEY_NONE;

static void PostCursorMoved( HWND hwnd, LPARAM lparam )
{
	POINT pt;
	pt.x = (short)LOWORD(lparam);
	pt.y = (short)HIWORD(lparam);
	::ClientToScreen( hwnd, &pt);
	g_pInput->InternalCursorMoved(pt.x, pt.y);

}

static LRESULT CALLBACK staticProc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam)
{
	static UINT s_uTaskbarRestart;

	VPANEL panel = NULL;
	IClientPanel *client = NULL;

	if (staticSurfaceAvailable)
	{
		panel = g_pIVgui->HandleToPanel(static_cast<HPanel>(::GetWindowLongPtr(hwnd, GWLP_USERDATA)));

		if (panel)
		{
			client = ((VPanel *)panel)->Client();
		}
	}

	// special case msg handle
	if (msg == staticShutdownMsg)
	{
		// we're being notified that we have to Shutdown
		g_pIVgui->ShutdownMessage(lparam);
		return ::DefWindowProc(hwnd,msg,wparam,lparam);
	}
	
	if (msg == WM_ENDSESSION && wparam == TRUE)
	{
		// system is being shutdown
		// after this message is processed, the app will be terminated
		// so all necessary shutdown functions need to occur now
		if (g_pSurface->GetEmbeddedPanel())
		{
			// dimhotepus: Do not leak KeyValues.
			g_pIPanel->SendMessage(g_pSurface->GetEmbeddedPanel(), KeyValuesAD("WindowsEndSession"), NULL);
		}
		return 0;
	}

	if (!panel)
	{
		return ::DefWindowProc(hwnd,msg,wparam,lparam);
	}

	bool sendToDefWindowProc = true;
	// md: temporarily disabled until the infinite recursion gets fixed.
	
	if ( ImmIsUIMessage( NULL, msg, wparam, lparam ) )
	{
		sendToDefWindowProc = false;
	}
	
	switch (msg)
	{
		case WM_XBUTTONDOWN:
		{
			PostCursorMoved( hwnd, lparam );
			MouseCode code = ( GET_XBUTTON_WPARAM( wparam ) == XBUTTON1 ) ? MOUSE_4 : MOUSE_5;
			g_pInput->SetMouseCodeState( code, BUTTON_PRESSED );
			g_pInput->InternalMousePressed( code );
			break;
		}
		case WM_XBUTTONUP:
		{
			PostCursorMoved( hwnd, lparam );
			MouseCode code = ( GET_XBUTTON_WPARAM( wparam ) == XBUTTON1 ) ? MOUSE_4 : MOUSE_5;
			g_pInput->SetMouseCodeState( code, BUTTON_RELEASED );
			g_pInput->InternalMouseReleased( code );
			break;
		}

		case WM_XBUTTONDBLCLK:
		{
			PostCursorMoved( hwnd, lparam );
			MouseCode code = ( GET_XBUTTON_WPARAM( wparam ) == XBUTTON1 ) ? MOUSE_4 : MOUSE_5;
			g_pInput->SetMouseCodeState( code, BUTTON_DOUBLECLICKED );
			g_pInput->InternalMouseDoublePressed( code );
			break;
		}

		case WM_CREATE:
		{
			s_uTaskbarRestart = RegisterWindowMessage(TEXT("TaskbarCreated"));
			break;
		}
		case WM_CLOSE:
		{
			// tell the panel to close
			g_pIVgui->PostMessage(panel, new KeyValues("Close"), NULL);

			// don't Run default message pump, as that destroys the window
			return 0;
		}
		case WM_MY_TRAY_NOTIFICATION:
		{
			staticNotifyIconProc(hwnd, wparam, lparam);
			break;
		}
		case WM_SETFOCUS:
		{
			g_Surface.setFocus(panel);
			//g_pIVgui->DPrintf("Set Focus %s %p\n", client->GetName(), panel);
			break;
		}
		case WM_KILLFOCUS:
		{
			g_Surface.setFocus(NULL);
			break;
		}
		case WM_APP:
		{
			g_Surface.setFocus(NULL);
			break;
		}
		case WM_SETCURSOR:
		{		
//!!			SetCursor(staticCurrentCursor);
			break;
		}
		case WM_MOUSEMOVE:
		{
			POINT pt;
			pt.x = (short)LOWORD(lparam);
			pt.y = (short)HIWORD(lparam);

			
			// This code catches the case when a WM_LBUTTONUP is lost

			bool bLMButtonDown = wparam & MK_LBUTTON;
			bool bRMButtonDown = wparam & MK_RBUTTON;
			bool bMMButtonDown = wparam & MK_MBUTTON;
			if ( !bLMButtonDown && g_pInput->IsMouseDown( MOUSE_LEFT ) )
			{
				g_pInput->SetMouseCodeState( MOUSE_LEFT, BUTTON_RELEASED );
				g_pInput->InternalMouseReleased(MOUSE_LEFT);
			}
			if ( !bRMButtonDown && g_pInput->IsMouseDown( MOUSE_RIGHT ) )
			{
				g_pInput->SetMouseCodeState( MOUSE_LEFT, BUTTON_RELEASED );
				g_pInput->InternalMouseReleased(MOUSE_LEFT);
			}
			if ( !bMMButtonDown && g_pInput->IsMouseDown( MOUSE_MIDDLE ) )
			{
				g_pInput->SetMouseCodeState( MOUSE_MIDDLE, BUTTON_RELEASED );
				g_pInput->InternalMouseReleased(MOUSE_MIDDLE);
			}
			
			::ClientToScreen((HWND)hwnd, &pt);
			g_pInput->InternalCursorMoved(pt.x, pt.y);
			break;
		}
		case WM_LBUTTONDOWN:
		{
			PostCursorMoved( hwnd, lparam );
			g_pInput->SetMouseCodeState( MOUSE_LEFT, BUTTON_PRESSED );
			g_pInput->InternalMousePressed(MOUSE_LEFT);
			break;
		}
		case WM_RBUTTONDOWN:
		{
			PostCursorMoved( hwnd, lparam );
			g_pInput->SetMouseCodeState( MOUSE_RIGHT, BUTTON_PRESSED );
			g_pInput->InternalMousePressed(MOUSE_RIGHT);
			break;
		}
		case WM_MBUTTONDOWN:
		{
			PostCursorMoved( hwnd, lparam );
			g_pInput->SetMouseCodeState( MOUSE_MIDDLE, BUTTON_PRESSED );
			g_pInput->InternalMousePressed(MOUSE_MIDDLE);
			break;
		}
		case WM_LBUTTONDBLCLK:
		{
			PostCursorMoved( hwnd, lparam );
			g_pInput->SetMouseCodeState( MOUSE_LEFT, BUTTON_DOUBLECLICKED );
			g_pInput->InternalMouseDoublePressed( MOUSE_LEFT );
			break;
		}
		case WM_RBUTTONDBLCLK:
		{
			PostCursorMoved( hwnd, lparam );
			g_pInput->SetMouseCodeState( MOUSE_RIGHT, BUTTON_DOUBLECLICKED );
			g_pInput->InternalMouseDoublePressed( MOUSE_RIGHT );
			break;
		}
		case WM_MBUTTONDBLCLK:
		{
			PostCursorMoved( hwnd, lparam );
			g_pInput->SetMouseCodeState( MOUSE_MIDDLE, BUTTON_DOUBLECLICKED );
			g_pInput->InternalMouseDoublePressed( MOUSE_MIDDLE );
			break;
		}
		case WM_LBUTTONUP:
		{
			PostCursorMoved( hwnd, lparam );
			g_pInput->SetMouseCodeState( MOUSE_LEFT, BUTTON_RELEASED );
			g_pInput->InternalMouseReleased( MOUSE_LEFT );
			break;
		}
		case WM_RBUTTONUP:
		{
			PostCursorMoved( hwnd, lparam );
			g_pInput->SetMouseCodeState( MOUSE_RIGHT, BUTTON_RELEASED );
			g_pInput->InternalMouseReleased( MOUSE_RIGHT );
			break;
		}
		case WM_MBUTTONUP:
		{
			PostCursorMoved( hwnd, lparam );
			g_pInput->SetMouseCodeState( MOUSE_MIDDLE, BUTTON_RELEASED );
			g_pInput->InternalMouseReleased( MOUSE_MIDDLE );
			break;
		}
		case WM_MOUSEWHEEL:
		{
			g_pInput->InternalMouseWheeled(GET_WHEEL_DELTA_WPARAM( wparam )/WHEEL_DELTA);
			break;
		}
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		{
			int code = static_cast<int>(wparam);
			g_iPreviousKeyCode = KeyCode_VirtualKeyToVGUI( code );
			bool bRepeating = ( lparam & ( 1<<30 ) ) != 0;
			if ( !bRepeating )
			{
				g_pInput->SetKeyCodeState( g_iPreviousKeyCode, BUTTON_PRESSED );
				g_pInput->InternalKeyCodePressed( g_iPreviousKeyCode );
			}
			g_pInput->InternalKeyCodeTyped( g_iPreviousKeyCode );

			// Deal with toggles
			if ( !bRepeating )
			{
				if ( code == VK_CAPITAL || code == VK_SCROLL || code == VK_NUMLOCK )
				{
					ButtonCode_t toggleCode;
					switch( code )
					{
					default: case VK_CAPITAL: toggleCode = KEY_CAPSLOCKTOGGLE; break;
					case VK_SCROLL: toggleCode = KEY_SCROLLLOCKTOGGLE; break;
					case VK_NUMLOCK: toggleCode = KEY_NUMLOCKTOGGLE; break;
					}

					SHORT wState = GetKeyState( code );
					bool bToggleState = ( wState & 0x1 ) != 0;
					if ( bToggleState )
					{
						g_pInput->SetKeyCodeState( toggleCode, BUTTON_PRESSED );
						g_pInput->InternalKeyCodePressed( toggleCode );
					}
					else
					{
						g_pInput->SetKeyCodeState( toggleCode, BUTTON_RELEASED );
						g_pInput->InternalKeyCodeReleased( toggleCode );
					}
				}
			}

			break;
		}
		case WM_SYSCHAR:
		case WM_CHAR:
		{
			int unichar = static_cast<int>(wparam);
			g_pInput->InternalKeyTyped(unichar);
			break;
		}
		case WM_KEYUP:
		case WM_SYSKEYUP:
		{
			KeyCode code = KeyCode_VirtualKeyToVGUI( static_cast<int>(wparam) );
			g_pInput->SetKeyCodeState( code, BUTTON_RELEASED );
			g_pInput->InternalKeyCodeReleased( code );
			break;
		}
		case WM_ERASEBKGND:
		{
			//since the vgui Invalidate call does not erase the background
			//this will only be called when windows itselfs wants a Repaint.
			//this is the desired behavior because this call will for the
			//surface and all its children to end up being repainted, which
			//is what you want when windows wants you to Repaint the surface,
			//but not what you want when say a control wants to be painted
			//VPanel::Repaint will always Invalidate the surface so it will
			//get a WM_PAINT, but that does not necessarily mean you want
			//the whole surface painted
			//simply this means.. this call only happens when windows wants the Repaint
			//and WM_PAINT gets called just after this to do the real painting

			client->Repaint();
//			vgui::g_pIVgui->DPrintf( "WM_ERASEBKGND(%X)\n", panel );
			break;
		}
		case WM_PAINT:
		{
			//surface was by repainted vgui or by windows itself, do the repainting all repainting 
			//will goes through here and nowhere else

			// post Paint messages to the que
//			panel->SolveTraverse();

			// post a message to Paint the window
//			vgui::g_pIVgui->DPrintf( "WM_PAINT(%X)\n", panel );
//			g_pInput->PostMessage( panel, new KeyValues("Paint"), NULL );

			// this relies on platTick() being called BEFORE dispatchMessages(), so that painting occurs

			// on the same frame that the WM_PAINT message is received
//			double startTime, solveTime, paintTime;
			// OLD CODE

			// check that the panel is visible all the way to the root

			bool IsVisible=g_pIPanel->IsVisible(panel);
			VPANEL p= g_pIPanel->GetParent(panel);
			// drill down the heirachy checking that everything is visible
			while(p && IsVisible)
			{
				if( g_pIPanel->IsVisible(p)==false)
				{
					IsVisible=false;
					break;
				}
				p=g_pIPanel->GetParent(p);
			}

			if( IsVisible )
			{
				PAINTSTRUCT ps;
				::BeginPaint(hwnd,&ps);
	//			startTime = system()->GetCurrentTime();
 				g_Surface.SolveTraverse(panel, false);
	//			solveTime = system()->GetCurrentTime();
				g_Surface.PaintTraverse(panel);
	//			paintTime = system()->GetCurrentTime();
				::EndPaint(hwnd,&ps);
			}

//			debug timing code
//			ivgui()->DPrintf2("Paint timings (%.3f %.3f)\n", (float)(solveTime - startTime), (float)(paintTime - solveTime));
		
		/*	char dbtxt[200];
			Q_snprintf(dbtxt,200,"paint:%p -- %p\n",hwnd,g_Surface.GetHTMLWindow(0)->GetIEHWND());
			OutputDebugString( dbtxt );
		*/
			//clear the update rectangle so it does not get another Repaint
			::ValidateRect(hwnd, NULL);	

			break;
		}
		default:
		{
			if (msg == s_uTaskbarRestart)
			{
				// notify window to re-add taskbar icons
				g_pIVgui->PostMessage(panel, new KeyValues("TaskbarRestart"), NULL);
			}

			break;
		}
	}

	if ( sendToDefWindowProc )
	{
		return DefWindowProc(hwnd,msg,wparam,lparam);
	}
	return TRUE;
}
