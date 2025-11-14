//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TGAIMAGEPANEL_H
#define TGAIMAGEPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Panel.h"

//-----------------------------------------------------------------------------
// Purpose: Displays a tga image
//-----------------------------------------------------------------------------
class CTGAImagePanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CTGAImagePanel, vgui::Panel );

public:
	// dimhotepus: Scale support (max width and max height).
	CTGAImagePanel( vgui::Panel *parent, const char *name, int maxWidth = -1, int maxHeight = -1 );

	~CTGAImagePanel();

	void SetTGA( const char *filename );
	void SetTGANonMod( const char *filename );

	void Paint( ) override;

private:
	int m_iTextureID;
	// dimhotepus: Scale UI.
	int m_iImageMaxWidth, m_iImageMaxHeight;
	int m_iImageRealWidth, m_iImageRealHeight;
	bool m_bHasValidTexture, m_bLoadedTexture;
	char m_szTGAName[256];
};

#endif //TGAIMAGEPANEL_H
