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

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Displays a tga image
//-----------------------------------------------------------------------------
class CTGAImagePanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CTGAImagePanel, vgui::Panel );

public:
	CTGAImagePanel( vgui::Panel *parent, const char *name );

	~CTGAImagePanel();

	void SetTGA( const char *filename );
	void SetTGANonMod( const char *filename );

	void Paint( void ) override;

private:
	int m_iTextureID;
	bool m_bHasValidTexture, m_bLoadedTexture;
	char m_szTGAName[256];
};

#endif //TGAIMAGEPANEL_H
