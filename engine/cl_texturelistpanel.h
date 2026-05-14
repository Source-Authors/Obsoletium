//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CL_TEXTURELISTPANEL_H
#define CL_TEXTURELISTPANEL_H
#ifdef _WIN32
#pragma once
#endif

namespace vgui {

class Panel;

}


void CL_CreateTextureListPanel( vgui::Panel *pPanel );
// dimhotepus: Pair with create.
void CL_DestroyTextureListPanel();

void VGui_UpdateTextureListPanel();


#endif // CL_TEXTURELISTPANEL_H
