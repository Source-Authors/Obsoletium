//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "TGAImagePanel.h"
#include "bitmap/tgaloader.h"
#include "vgui/ISurface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

CTGAImagePanel::CTGAImagePanel( vgui::Panel *parent, const char *name, int maxWidth, int maxHeight ) : BaseClass( parent, name )
{
	m_iTextureID = -1;
	m_iImageMaxWidth = maxWidth != -1 ? maxWidth : std::numeric_limits<int>::max();
	m_iImageMaxHeight = maxHeight != -1 ? maxHeight : std::numeric_limits<int>::max();
	m_iImageRealWidth = m_iImageRealHeight = 0;
	m_bHasValidTexture = false;
	m_bLoadedTexture = false;
	m_szTGAName[0] = 0;

	SetPaintBackgroundEnabled( false );
}

CTGAImagePanel::~CTGAImagePanel()
{
	// release the texture memory
	if ( vgui::surface() && m_iTextureID != -1 )
	{
		vgui::surface()->DestroyTextureID( m_iTextureID );
		m_iTextureID = -1;
	}
}

void CTGAImagePanel::SetTGA( const char *filename )
{
	V_sprintf_safe( m_szTGAName, "//MOD/%s", filename );
}

void CTGAImagePanel::SetTGANonMod( const char *filename )
{
	V_strcpy_safe( m_szTGAName, filename );
}

void CTGAImagePanel::Paint()
{
	if ( !m_bLoadedTexture )
	{
		m_bLoadedTexture = true;
		// get a texture id, if we haven't already
		if ( m_iTextureID == -1 )
		{
			m_iTextureID = vgui::surface()->CreateNewTextureID( true );
			// dimhotepus: Scale UI.
			SetSize( m_iImageMaxWidth, m_iImageMaxHeight );
		}

		// load the file
		CUtlMemory<unsigned char> tga;
		int iImageWidth, iImageHeight;

		// dimhotepus: This can take a while, put up a waiting cursor.
		const ScopedPanelWaitCursor scopedWaitCursor{this};

		if ( TGALoader::LoadRGBA8888( m_szTGAName, tga, iImageWidth, iImageHeight ) )
		{
			// set the textureID
			surface()->DrawSetTextureRGBA( m_iTextureID, tga.Base(), iImageWidth, iImageHeight, false, true );
			m_bHasValidTexture = true;

			// dimhotepus: Scale UI.
			surface()->DrawGetTextureSize( m_iTextureID, m_iImageRealWidth, m_iImageRealHeight );
			m_iImageRealWidth = QuickPropScale( m_iImageRealWidth );
			m_iImageRealHeight = QuickPropScale( m_iImageRealHeight );

			iImageWidth = QuickPropScale( min( m_iImageMaxWidth, iImageWidth ) );
			iImageHeight = QuickPropScale( min( m_iImageMaxHeight, iImageHeight ) );

			// set our size to be the size of the tga
			SetSize( iImageWidth, iImageHeight );
		}
		else
		{
			m_bHasValidTexture = false;
		}
	}

	// draw the image
	int wide, tall;
	if ( m_bHasValidTexture )
	{
		surface()->DrawSetTexture( m_iTextureID );
		surface()->DrawSetColor( 255, 255, 255, 255 );
		// dimhotepus: Scale UI.
		surface()->DrawTexturedRect( 0, 0, m_iImageRealWidth, m_iImageRealHeight );
	}
	else
	{
		// draw a black fill instead
		// dimhotepus: Scale UI.
		wide = m_iImageMaxWidth, tall = m_iImageMaxHeight;
		surface()->DrawSetColor( 0, 0, 0, 255 );
		surface()->DrawFilledRect( 0, 0, wide, tall );
	}
}
