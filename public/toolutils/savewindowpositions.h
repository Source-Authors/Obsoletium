//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef SAVEWINDOWPOSITIONS_H
#define SAVEWINDOWPOSITIONS_H

#include "tier0/commonmacros.h"

namespace vgui
{
	class Panel;
	class ToolWindow;
	class IToolWindowFactory;
};

class CMovieView;


//-----------------------------------------------------------------------------
// Purpose: This will save the bounds and the visibility state of UI elements registered during startup
//-----------------------------------------------------------------------------
// dimhotepus: Make abstract.
abstract_class IWindowPositionMgr
{
public:
	virtual void	SavePositions( char const *filename, char const *key ) = 0;
	virtual bool	LoadPositions( char const *filename, vgui::Panel *parent, vgui::IToolWindowFactory *factory, char const *key, bool force = false ) = 0;
	virtual void	RegisterPanel( char const *saveName, vgui::Panel *panel, bool contextMenu ) = 0;
	virtual void	UnregisterPanel( vgui::Panel *panel ) = 0;
};

extern IWindowPositionMgr *windowposmgr;

#endif // SAVEWINDOWPOSITIONS_H
