//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "pch_serverbrowser.h"

#include "VACBannedConnRefusedDialog.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CVACBannedConnRefusedDialog::CVACBannedConnRefusedDialog( VPANEL hVParent, const char *name ) : BaseClass( NULL, name )
{
	SetParent( hVParent );
	// dimhotepus: Scale UI.
	SetSize( QuickPropScale( 480 ), QuickPropScale( 220 ) );
	SetSizeable( false );

	LoadControlSettings( "servers/VACBannedConnRefusedDialog.res" );
	MoveToCenterOfScreen();
}
