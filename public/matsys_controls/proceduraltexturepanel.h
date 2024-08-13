//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef PROCEDURALTEXTUREPANEL_H
#define PROCEDURALTEXTUREPANEL_H

#ifdef _WIN32
#pragma once
#endif


#include "materialsystem/itexture.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "vgui_controls/EditablePanel.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct BGRA8888_t;


//-----------------------------------------------------------------------------
//
// Procedural texture image panel
//
//-----------------------------------------------------------------------------
class CProceduralTexturePanel : public vgui::EditablePanel, public ITextureRegenerator
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CProceduralTexturePanel, vgui::EditablePanel );

public:
	// constructor
	CProceduralTexturePanel( vgui::Panel *pParent, const char *pName );
	~CProceduralTexturePanel();

	// Methods of ITextureRegenerator
	void Release()  override {}
	void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect ) override;

	// initialization, shutdown
	virtual bool Init( int nWidth, int nHeight, bool bAllocateImageBuffer );
	virtual void Shutdown();

	// Returns the image buffer + dimensions
	BGRA8888_t *GetImageBuffer();
	int GetImageWidth() const;
	int GetImageHeight() const;

	// Redownloads the procedural texture
	void DownloadTexture();

	// Sets the rectangle to paint. Use null to fill the entire panel
	void SetPaintRect( const Rect_t *pPaintRect = NULL );

	// Sets the texcoords to use with the procedural texture
	void SetTextureSubRect( const Rect_t &subRect );

	// Maintain proportions when drawing
	void MaintainProportions( bool bEnable );

	void Paint( void ) override;
	void PaintBackground( void ) override {}

private:
	void CleanUp();

protected:
	// Image buffer
	BGRA8888_t *m_pImageBuffer;
	int m_nWidth;
	int m_nHeight;

	// Paint rectangle
	Rect_t m_PaintRect;

	// Texture coordinate rectangle
	Rect_t m_TextureSubRect;

	CTextureReference	m_ProceduralTexture;
	CMaterialReference	m_ProceduralMaterial;

	int m_nTextureID;
	bool m_bMaintainProportions;
	bool m_bUsePaintRect;
};


#endif // PROCEDURALTEXTUREPANEL_H
