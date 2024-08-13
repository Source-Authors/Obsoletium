//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef MATSYSTEMSURFACE_H
#define MATSYSTEMSURFACE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui/ISurface.h>
#include <vgui/IPanel.h>
#include <vgui/IClientPanel.h>
#include <vgui_controls/Panel.h>
#include <vgui/IInput.h>
#include <vgui_controls/Controls.h>
#include <vgui/Point.h>
#include "materialsystem/imaterialsystem.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterial.h"
#include "utlvector.h"
#include "utlsymbol.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "tier1/utldict.h"
#include "tier3/tier3.h"

using namespace vgui;

class IImage;

extern class IMaterialSystem *g_pMaterialSystem;
//-----------------------------------------------------------------------------
// The default material system embedded panel
//-----------------------------------------------------------------------------
class CMatEmbeddedPanel : public vgui::Panel
{
	typedef vgui::Panel BaseClass;
public:
	CMatEmbeddedPanel();
	void OnThink() override;

	VPANEL IsWithinTraverse(int x, int y, bool traversePopups) override;
};

//-----------------------------------------------------------------------------
//
// Implementation of the VGUI surface on top of the material system
//
//-----------------------------------------------------------------------------
class CMatSystemSurface : public CTier3AppSystem< IMatSystemSurface >
{
	typedef CTier3AppSystem< IMatSystemSurface > BaseClass;

public:
	CMatSystemSurface();
	virtual ~CMatSystemSurface();

	// Methods of IAppSystem
	bool Connect( CreateInterfaceFn factory ) override;
	void Disconnect() override;
	void *QueryInterface( const char *pInterfaceName ) override;
	InitReturnVal_t Init() override;
	void Shutdown() override;

	// initialization
	void SetEmbeddedPanel(vgui::VPANEL pEmbeddedPanel) override;

	// returns true if a panel is minimzed
	bool IsMinimized(vgui::VPANEL panel) override;

	// Sets the only panel to draw.  Set to NULL to clear.
	void RestrictPaintToSinglePanel(vgui::VPANEL panel) override;

	// frame
	void RunFrame() override;

	// implementation of vgui::ISurface
	vgui::VPANEL GetEmbeddedPanel() override;
	
	// drawing context
	void PushMakeCurrent(vgui::VPANEL panel, bool useInSets) override;
	void PopMakeCurrent(vgui::VPANEL panel) override;

	// rendering functions
	void DrawSetColor(int r,int g,int b,int a) override;
	void DrawSetColor(Color col) override;
	
	void DrawLine( int x0, int y0, int x1, int y1 ) override;
	void DrawTexturedLine( const vgui::Vertex_t &a, const vgui::Vertex_t &b ) override;
	void DrawPolyLine(int *px, int *py, int numPoints) override;
	void DrawTexturedPolyLine( const vgui::Vertex_t *p, int n ) override;

	void DrawFilledRect(int x0, int y0, int x1, int y1) override;
	void DrawFilledRectArray( IntRect *pRects, int numRects ) override;
	void DrawFilledRectFastFade( int x0, int y0, int x1, int y1, int fadeStartPt, int fadeEndPt, unsigned int alpha0, unsigned int alpha1, bool bHorizontal ) override;
	void DrawFilledRectFade( int x0, int y0, int x1, int y1, unsigned int alpha0, unsigned int alpha1, bool bHorizontal ) override;
	void DrawOutlinedRect(int x0, int y0, int x1, int y1) override;
	void DrawOutlinedCircle(int x, int y, int radius, int segments) override;

	// textured rendering functions
	int  CreateNewTextureID( bool procedural = false ) override;
	bool IsTextureIDValid(int id) override;

	bool DrawGetTextureFile(int id, char *filename, int maxlen ) override;
	int	 DrawGetTextureId( char const *filename ) override;
	int  DrawGetTextureId( ITexture *pTexture ) override;
	void DrawSetTextureFile(int id, const char *filename, int hardwareFilter, bool forceReload) override;
	void DrawSetTexture(int id) override;
	void DrawGetTextureSize(int id, int &wide, int &tall) override;
	bool DeleteTextureByID(int id) override;

	IVguiMatInfo *DrawGetTextureMatInfoFactory(int id) override;

	void DrawSetTextureRGBA(int id, const unsigned char *rgba, int wide, int tall, int hardwareFilter, bool forceReload) override;

	void DrawTexturedRect(int x0, int y0, int x1, int y1) override;
	void DrawTexturedSubRect( int x0, int y0, int x1, int y1, float texs0, float text0, float texs1, float text1 ) override;

	void DrawTexturedPolygon(int n, vgui::Vertex_t *pVertices, bool bClipVertices = true ) override;

	void DrawPrintText(const wchar_t *text, int textLen, FontDrawType_t drawType = FONT_DRAW_DEFAULT)override;
	void DrawUnicodeChar(wchar_t wch, FontDrawType_t drawType = FONT_DRAW_DEFAULT ) override;
	void DrawUnicodeString( const wchar_t *pwString, FontDrawType_t drawType = FONT_DRAW_DEFAULT ) override;
	void DrawSetTextFont(vgui::HFont font) override;
	void DrawFlushText() override;

	void DrawSetTextColor(int r, int g, int b, int a) override;
	void DrawSetTextColor(Color col) override;
	void DrawSetTextScale(float sx, float sy) override;
	void DrawSetTextPos(int x, int y) override;
	void DrawGetTextPos(int& x,int& y) override;

	vgui::IHTML *CreateHTMLWindow(vgui::IHTMLEvents *events,vgui::VPANEL context) override;
	void PaintHTMLWindow(vgui::IHTML *htmlwin) override;
	void DeleteHTMLWindow(vgui::IHTML *htmlwin) override;
	bool BHTMLWindowNeedsPaint(IHTML *htmlwin) override;

	void GetScreenSize(int &wide, int &tall) override;
	void SetAsTopMost(vgui::VPANEL panel, bool state) override;
	void BringToFront(vgui::VPANEL panel) override;
	void SetForegroundWindow (vgui::VPANEL panel) override;
	void SetPanelVisible(vgui::VPANEL panel, bool state) override;
	void SetMinimized(vgui::VPANEL panel, bool state) override;
	void FlashWindow(vgui::VPANEL panel, bool state) override;
	void SetTitle(vgui::VPANEL panel, const wchar_t *title) override;
	const wchar_t *GetTitle( vgui::VPANEL panel ) override;

	void SetAsToolBar(vgui::VPANEL panel, bool state) override;		// removes the window's task bar entry (for context menu's, etc.)

	// windows stuff
	void CreatePopup(VPANEL panel, bool minimized, bool showTaskbarIcon = true, bool disabled = false, bool mouseInput = true , bool kbInput = true) override;

	void SwapBuffers(vgui::VPANEL panel) override;
	void Invalidate(vgui::VPANEL panel) override;

	void SetCursor(vgui::HCursor cursor) override;
	bool IsCursorVisible() override;
	void SetCursorAlwaysVisible(bool visible) override;

	void ApplyChanges() override;
	bool IsWithin(int x, int y) override;
	bool HasFocus() override;

	bool SupportsFeature(SurfaceFeature_e feature) override;

	// engine-only focus handling (replacing WM_FOCUS windows handling)
	void SetTopLevelFocus(vgui::VPANEL panel) override;

	// fonts
	vgui::HFont CreateFont() override;
	bool SetFontGlyphSet(vgui::HFont font, const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags, int nRangeMin = 0, int nRangeMax = 0) override;
	bool SetBitmapFontGlyphSet(vgui::HFont font, const char *windowsFontName, float scalex, float scaley, int flags) override;
	int GetFontTall(HFont font) override;
	int GetFontTallRequested(HFont font) override;
	int GetFontAscent(HFont font, wchar_t wch) override;
	bool IsFontAdditive(HFont font)  override;
	void GetCharABCwide(HFont font, int ch, int &a, int &b, int &c) override;
	void GetTextSize(HFont font, const wchar_t *text, int &wide, int &tall) override;
	int GetCharacterWidth(vgui::HFont font, int ch) override;
	bool AddCustomFontFile(const char *fontName, const char *fontFileName) override;
	bool AddBitmapFontFile(const char *fontFileName) override;
	void SetBitmapFontName( const char *pName, const char *pFontFilename ) override;
	const char *GetBitmapFontName( const char *pName ) override;
	void PrecacheFontCharacters(HFont font, const wchar_t *pCharacters) override;
	void ClearTemporaryFontCache( void ) override;
	const char *GetFontName( HFont font ) override;
	const char *GetFontFamilyName( HFont font ) override;

	// GameUI-only accessed functions
	// uploads a part of a texture, used for font rendering
	void DrawSetSubTextureRGBA(int textureID, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall);
	void DrawUpdateRegionTextureRGBA( int nTextureID, int x, int y, const unsigned char *pchData, int wide, int tall, ImageFormat imageFormat ) override;

	// notify icons?!?
	vgui::VPANEL GetNotifyPanel() override;
	void SetNotifyIcon(vgui::VPANEL context, vgui::HTexture icon, vgui::VPANEL panelToReceiveMessages, const char *text) override;

	// plays a sound
	void PlaySound(const char *fileName) override;

	//!! these functions Should not be accessed directly, but only through other vgui items
	//!! need to move these to seperate interface
	intp GetPopupCount() override;
	vgui::VPANEL GetPopup( int index ) override;
	bool ShouldPaintChildPanel(vgui::VPANEL childPanel) override;
	bool RecreateContext(vgui::VPANEL panel) override;
	void AddPanel(vgui::VPANEL panel) override;
	void ReleasePanel(vgui::VPANEL panel) override;
	void MovePopupToFront(vgui::VPANEL panel) override;

	void SolveTraverse(vgui::VPANEL panel, bool forceApplySchemeSettings) override;
	void PaintTraverse(vgui::VPANEL panel) override;

	void EnableMouseCapture(vgui::VPANEL panel, bool state) override;

	void SetWorkspaceInsets( int left, int top, int right, int bottom ) override;
	void GetWorkspaceBounds(int &x, int &y, int &wide, int &tall) override;

	// Hook needed to Get input to work
	void AttachToWindow( void *hwnd, bool bLetAppDriveInput ) override;
	bool HandleInputEvent( const InputEvent_t &event ) override;

	void InitFullScreenBuffer( const char *pszRenderTargetName );
	void Set3DPaintTempRenderTarget( const char *pRenderTargetName ) override;
	void Reset3DPaintTempRenderTarget( void ) override;

	// Begins, ends 3D painting
	void Begin3DPaint( int iLeft, int iTop, int iRight, int iBottom, bool bRenderToTexture = true ) override;
	void End3DPaint() override;

	virtual void BeginSkinCompositionPainting() override;
	virtual void EndSkinCompositionPainting() override;

	// Disable clipping during rendering
	virtual void DisableClipping( bool bDisable ) override;
	virtual void GetClippingRect( int &left, int &top, int &right, int &bottom, bool &bClippingDisabled ) override;
	virtual void SetClippingRect( int left, int top, int right, int bottom ) override;

	// Prevents vgui from changing the cursor
	bool IsCursorLocked() const override;

	// Sets the mouse Get + Set callbacks
	void SetMouseCallbacks( GetMouseCallback_t GetFunc, SetMouseCallback_t SetFunc ) override;

	// Tells the surface to ignore windows messages
	void EnableWindowsMessages( bool bEnable ) override;

	// Installs a function to play sounds
	void InstallPlaySoundFunc( PlaySoundFunc_t soundFunc ) override;

	// Some drawing methods that cannot be accomplished under Win32
	void DrawColoredCircle( int centerx, int centery, float radius, int r, int g, int b, int a ) override;
	int	DrawColoredText( vgui::HFont font, int x, int y, int r, int g, int b, int a, PRINTF_FORMAT_STRING const char *fmt, ... ) override;
	void DrawColoredTextRect( vgui::HFont font, int x, int y, int w, int h, int r, int g, int b, int a, PRINTF_FORMAT_STRING const char *fmt, ... ) override;
	void DrawTextHeight( vgui::HFont font, int w, int& h, PRINTF_FORMAT_STRING const char *fmt, ... ) override;

	// Returns the length in pixels of the text
	int	DrawTextLen( vgui::HFont font, PRINTF_FORMAT_STRING const char *fmt, ... ) override;

	// Draws a panel in 3D space. 
	void DrawPanelIn3DSpace( vgui::VPANEL pRootPanel, const VMatrix &panelCenterToWorld, int pw, int ph, float sw, float sh ) override;

	// Only visible within vguimatsurface
	void DrawSetTextureMaterial(int id, IMaterial *pMaterial) override;
	void ReferenceProceduralMaterial( int id, int referenceId, IMaterial *pMaterial );

	// new stuff for Alfreds VGUI2 port!!
	virtual bool InEngine() { return true; }
	void GetProportionalBase( int &width, int &height ) override { width = BASE_WIDTH; height = BASE_HEIGHT; }
	bool HasCursorPosFunctions() override { return true; }

	void SetModalPanel(VPANEL ) override;
	VPANEL GetModalPanel() override;
	void UnlockCursor() override;
	void LockCursor() override;
	void SetTranslateExtendedKeys(bool state) override;
	VPANEL GetTopmostPopup() override;
	void GetAbsoluteWindowBounds(int &x, int &y, int &wide, int &tall) override;
	void CalculateMouseVisible() override;
	bool NeedKBInput() override;
	void SurfaceGetCursorPos(int &x, int &y) override;
	void SurfaceSetCursorPos(int x, int y) override;
	void MovePopupToBack(VPANEL panel) override;

	virtual bool IsInThink( VPANEL panel); 

	bool DrawGetUnicodeCharRenderInfo( wchar_t ch, CharRenderInfo& info ) override;
	void DrawRenderCharFromInfo( const CharRenderInfo& info ) override;

	// global alpha setting functions
	// affect all subsequent draw calls - shouldn't normally be used directly, only in Panel::PaintTraverse()
	void DrawSetAlphaMultiplier( float alpha /* [0..1] */ ) override;
	float DrawGetAlphaMultiplier() override;

	// web browser
	void SetAllowHTMLJavaScript( bool state ) override;

	// video mode changing
	void OnScreenSizeChanged( int nOldWidth, int nOldHeight ) override;

	vgui::HCursor CreateCursorFromFile( char const *curOrAniFile, char const *pPathID ) override;

	void PaintTraverseEx(VPANEL panel, bool paintPopups = false ) override;

	float GetZPos() const override;

	void SetPanelForInput( VPANEL vpanel ) override;

	vgui::IImage *GetIconImageForFullPath( char const *pFullPath ) override;

	void DestroyTextureID( int id ) override;


	const char *GetResolutionKey( void ) const override;

	bool ForceScreenSizeOverride( bool bState, int wide, int tall ) override;
	// LocalToScreen, ParentLocalToScreen fixups for explicit PaintTraverse calls on Panels not at 0, 0 position
	bool ForceScreenPosOffset( bool bState, int x, int y ) override;

	void OffsetAbsPos( int &x, int &y ) override;

	void GetKernedCharWidth( HFont font, wchar_t ch, wchar_t chBefore, wchar_t chAfter, float &wide, float &flabcA ) override;
	

	const char *GetWebkitHTMLUserAgentString() override { return "Valve Client"; }

	void *Deprecated_AccessChromeHTMLController() override { return NULL; }

	virtual void SetFullscreenViewport( int x, int y, int w, int h ) override;
	virtual void SetFullscreenViewportAndRenderTarget( int x, int y, int w, int h, ITexture *pRenderTarget ) override;
	virtual void GetFullscreenViewportAndRenderTarget( int & x, int & y, int & w, int & h, ITexture **ppRenderTarget ) override;
	virtual void GetFullscreenViewport( int & x, int & y, int & w, int & h ) override;
	virtual void PushFullscreenViewport() override;
	virtual void PopFullscreenViewport() override;

	// support for software cursors
	virtual void SetSoftwareCursor( bool bUseSoftwareCursor ) override;
	virtual void PaintSoftwareCursor()  override;

	// Methods of ILocalizeTextQuery
public:
	//virtual int ComputeTextWidth( const wchar_t *pString );

	// Causes fonts to get reloaded, etc.
	void ResetFontCaches() override;

	bool IsScreenSizeOverrideActive( void ) override;
	bool IsScreenPosOverrideActive( void ) override;

	IMaterial *DrawGetTextureMaterial( int id ) override;

	int GetTextureNumFrames( int id ) override;
	void DrawSetTextureFrame( int id, int nFrame, unsigned int *pFrameCache ) override;

private:
	//void DrawRenderCharInternal( const FontCharRenderInfo& info );
	void DrawRenderCharInternal( const CharRenderInfo& info );

private:
	enum { BASE_HEIGHT = 480, BASE_WIDTH = 640 };

	struct PaintState_t
	{
		vgui::VPANEL m_pPanel;
		int	m_iTranslateX;
		int m_iTranslateY;
		int	m_iScissorLeft;
		int	m_iScissorRight;
		int	m_iScissorTop;
		int	m_iScissorBottom;
	};

	// material Setting method 
	void InternalSetMaterial( IMaterial *material = NULL );

	// Draws the fullscreen buffer into the panel
	void DrawFullScreenBuffer( int nLeft, int nTop, int nRight, int nBottom );

	// Helper method to initialize vertices (transforms them into screen space too)
	void InitVertex( vgui::Vertex_t &vertex, int x, int y, float u, float v );

	// Draws a quad + quad array 
	void DrawQuad( const vgui::Vertex_t &ul, const vgui::Vertex_t &lr, unsigned char *pColor );
	void DrawQuadArray( int numQuads, vgui::Vertex_t *pVerts, unsigned char *pColor, bool bShouldClip = true );

	// Necessary to wrap the rendering
	void StartDrawing( void );
	void StartDrawingIn3DSpace( const VMatrix &screenToWorld, int pw, int ph, float sw, float sh );
	void FinishDrawing( void );

	// Sets up a particular painting state...
	void SetupPaintState( const PaintState_t &paintState );

	void ResetPopupList();
	void AddPopup( vgui::VPANEL panel );
	void RemovePopup( vgui::VPANEL panel );
	void AddPopupsToList( vgui::VPANEL panel );

	// Helper for drawing colored text
	int DrawColoredText( vgui::HFont font, int x, int y, int r, int g, int b, int a, const char *fmt, va_list argptr );
	void SearchForWordBreak( vgui::HFont font, char *text, int& chars, int& pixels );

	void InternalThinkTraverse(VPANEL panel);
	void InternalSolveTraverse(VPANEL panel);
	void InternalSchemeSettingsTraverse(VPANEL panel, bool forceApplySchemeSettings);

	// handles mouse movement
	void SetCursorPos(int x, int y);
	void GetCursorPos(int &x, int &y);

	void DrawTexturedLineInternal( const Vertex_t &a, const Vertex_t &b );

	// Gets texture coordinates for drawing the full screen buffer
	void GetFullScreenTexCoords( int x, int y, int w, int h, float *pMinU, float *pMinV, float *pMaxU, float *pMaxV );

	// Is a panel under the restricted panel?
	bool IsPanelUnderRestrictedPanel( VPANEL panel );

	// Point Translation for current panel
	int				m_nTranslateX;
	int				m_nTranslateY;

	// alpha multiplier for current panel [0..1]
	float			m_flAlphaMultiplier;

	// The size of the window to draw into
	int				m_pSurfaceExtents[4];

	// Color for drawing all non-text things
	unsigned char	m_DrawColor[4];

	// Color for drawing text
	unsigned char	m_DrawTextColor[4];

	// Location of text rendering
	int				m_pDrawTextPos[2];

	// Meshbuilder used for drawing
	IMesh* m_pMesh;
	CMeshBuilder meshBuilder;

	// White material used for drawing non-textured things
	CMaterialReference m_pWhite;

	// Used for 3D-rendered images
	CTextureReference m_FullScreenBuffer;
	CMaterialReference m_FullScreenBufferMaterial;
	int m_nFullScreenBufferMaterialId;
	CUtlString		m_FullScreenBufferName;

	bool			m_bUsingTempFullScreenBufferMaterial;

	// Root panel
	vgui::VPANEL m_pEmbeddedPanel;
	vgui::Panel *m_pDefaultEmbeddedPanel;
	vgui::VPANEL m_pRestrictedPanel;

	// List of pop-up panels based on the type enum above (draw order vs last clicked)
	CUtlVector<vgui::HPanel>	m_PopupList;

	// Stack of paint state...
	CUtlVector<	PaintState_t > m_PaintStateStack;

	vgui::HFont				m_hCurrentFont;
	vgui::HCursor			_currentCursor;
	bool					m_cursorAlwaysVisible;

	// The currently bound texture
	int m_iBoundTexture;

	// font drawing batching code
	enum { MAX_BATCHED_CHAR_VERTS = 4096 };
	vgui::Vertex_t m_BatchedCharVerts[ MAX_BATCHED_CHAR_VERTS ];
	int m_nBatchedCharVertCount;

	// What's the rectangle we're drawing in 3D paint mode?
	int m_n3DLeft, m_n3DRight, m_n3DTop, m_n3DBottom;

	// Are we painting in 3D? (namely drawing 3D objects *inside* the vgui panel)
	bool m_bIn3DPaintMode : 1;

	// If we are in 3d Paint mode, are we rendering to a texture?  (Or directly to the screen)
	bool m_b3DPaintRenderToTexture : 1;

	// Are we drawing the vgui panel in the 3D world somewhere?
	bool m_bDrawingIn3DWorld : 1;

	// Is the app gonna call HandleInputEvent?
	bool m_bAppDrivesInput : 1;

	// Are we currently in the think() loop
	bool m_bInThink : 1;

	bool m_bNeedsKeyboard : 1;
	bool m_bNeedsMouse : 1;
	bool m_bAllowJavaScript : 1;

	int m_nLastInputPollCount;

	VPANEL m_CurrentThinkPanel;

	// The attached HWND
	void *m_HWnd;

	// Installed function to play sounds
	PlaySoundFunc_t m_PlaySoundFunc;

	int		m_WorkSpaceInsets[4];

	class TitleEntry
	{
	public:
		TitleEntry()
		{
			panel = NULL;
			title[0] = 0;
		}

		vgui::VPANEL panel;
		wchar_t	title[128];
	};

	CUtlVector< TitleEntry >	m_Titles;
	CUtlVector< CUtlSymbol >	m_CustomFontFileNames;
	CUtlVector< CUtlSymbol >	m_BitmapFontFileNames;
	CUtlDict< int, int >		m_BitmapFontFileMapping;

	float	m_flZPos;
	CUtlDict< vgui::IImage *, unsigned short >	m_FileTypeImages;

	intp		GetTitleEntry( vgui::VPANEL panel );

	void DrawSetTextureRGBAEx(int id, const unsigned char *rgba, int wide, int tall, ImageFormat format ) override;

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

	int m_nFullscreenViewportX;
	int m_nFullscreenViewportY;
	int m_nFullscreenViewportWidth;
	int m_nFullscreenViewportHeight;
	ITexture *m_pFullscreenRenderTarget;

#ifdef LINUX
	struct font_entry
	{
		void *data;
		int size;
	};

	static CUtlDict< font_entry, unsigned short > m_FontData;

	static void *FontDataHelper( const char *pchFontName, int &size, const char *fontFileName );
#endif
};

#if GLMDEBUG
class MatSurfFuncLogger	// rip off of GLMFuncLogger - figure out a way to reunify these soon
{
	public:
	
		// simple function log
		MatSurfFuncLogger( char *funcName )
		{
			CMatRenderContextPtr prc( g_pMaterialSystem );

			m_funcName = funcName;
			m_earlyOut = false;
			
			prc->Printf( ">%s", m_funcName );
		};
		
		// more advanced version lets you pass args (i.e. called parameters or anything else of interest)
		// no macro for this one, since no easy way to pass through the args as well as the funcname
		MatSurfFuncLogger( char *funcName, PRINTF_FORMAT_STRING const char *fmt, ... )
		{
			CMatRenderContextPtr prc( g_pMaterialSystem );

			m_funcName = funcName;
			m_earlyOut = false;

			// this acts like GLMPrintf here
			// all the indent policy is down in GLMPrintfVA
			// which means we need to inject a ">" at the front of the format string to make this work... sigh.
			
			char modifiedFmt[2000];
			modifiedFmt[0] = '>';
			strcpy( modifiedFmt+1, fmt );
			
			va_list	vargs;
			va_start(vargs, fmt);
			prc->PrintfVA( modifiedFmt, vargs );
			va_end( vargs );
		}
		
		~MatSurfFuncLogger( )
		{
			CMatRenderContextPtr prc( g_pMaterialSystem );
			if (m_earlyOut)
			{
				prc->Printf( "<%s (early out)", m_funcName );
			}
			else
			{
				prc->Printf( "<%s", m_funcName );
			}
		};
	
		void EarlyOut( void )
		{
			m_earlyOut = true;
		};

		
		char *m_funcName;				// set at construction time
		bool m_earlyOut;
};

// handy macro to go with the helper class
#define	MAT_FUNC			MatSurfFuncLogger _logger_ ( __FUNCTION__ )

#else

#define	MAT_FUNC

#endif

#endif // MATSYSTEMSURFACE_H
