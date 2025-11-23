//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "toolutils/basestatusbar.h"
#include "toolutils/ConsolePage.h"
#include "vgui_controls/Label.h"
#include "movieobjects/dmeclip.h"
#include "tier1/KeyValues.h"
#include "vgui/IVGui.h"
#include "toolutils/enginetools_int.h"
#include "toolframework/ienginetool.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseStatusBar::CBaseStatusBar( vgui::Panel *parent, char const *panelName ) 
	: BaseClass( parent, panelName ),
	m_flLastFPSSnapShot( -1.0f )
{
	SetVisible( true );
	m_pConsole = new CConsolePage( this, true );
	m_pLabel = new Label( this, "Console", "#BxConsole" );
	m_pMemory = new Label( this, "Memory", "" );
	m_pFPS = new Label( this, "FPS", "" );
	m_pGameTime = new Label( this, "GameTime", "" );

	MakePopup( false );

	UpdateMemoryUsage( 9.999f );
}

//-----------------------------------------------------------------------------
// Purpose: Forces console to take up full area except right edge
// Input  :  - 
//-----------------------------------------------------------------------------
void CBaseStatusBar::PerformLayout()
{
	BaseClass::PerformLayout();

	int w, h;
	GetSize( w, h );

	int oldw = w;

	w = w * 45 / 100;

	// dimhotepus: Scale UI.
	int x = QuickPropScale( 8 );

	int cw, ch;
	m_pLabel->GetContentSize( cw, ch );
	m_pLabel->SetBounds( x, QuickPropScale( 4 ), cw, h - QuickPropScale( 8 ) ); //-V112

	x += cw + QuickPropScale( 4 ); //-V112

	int consoleWide = w - x - QuickPropScale( 8 );

	m_pConsole->SetBounds( x, QuickPropScale( 2 ), consoleWide, h - QuickPropScale( 4 ) ); //-V112

	int infoW = QuickPropScale( 85 );

	int rightx = oldw - infoW - QuickPropScale( 10 );
	m_pFPS->SetBounds( rightx, QuickPropScale( 2 ), infoW - QuickPropScale( 2 + 10 ), h - QuickPropScale( 8 ) );
	rightx -= infoW;
	m_pGameTime->SetBounds( rightx, QuickPropScale( 2 ), infoW - QuickPropScale( 2 ), h - QuickPropScale( 8 ) );
	rightx -= infoW;
	m_pMemory->SetBounds( rightx, QuickPropScale( 2 ), infoW - QuickPropScale( 2 ), h - QuickPropScale( 8 ) );
}

void CBaseStatusBar::UpdateMemoryUsage( float mbUsed )
{
	char mem[ 256 ];
	V_sprintf_safe( mem, "[mem: %.2f Mb]", mbUsed );
	m_pMemory->SetText( mem );
}

//-----------------------------------------------------------------------------
// Purpose: Message map
//-----------------------------------------------------------------------------
void CBaseStatusBar::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	// get the borders we need
	SetBorder(pScheme->GetBorder("ButtonBorder"));

	// get the background color
	SetBgColor(pScheme->GetColor( "StatusBar.BgColor", GetBgColor() ));

	m_pLabel->SetFont( pScheme->GetFont( "DefaultVerySmall" ) );
	m_pMemory->SetFont( pScheme->GetFont( "DefaultVerySmall" ) );
	m_pFPS->SetFont( pScheme->GetFont( "DefaultVerySmall" ) );
	m_pGameTime->SetFont( pScheme->GetFont( "DefaultVerySmall" ) );
}

static float GetMemoryUsage();

void CBaseStatusBar::OnThink()
{
	BaseClass::OnThink();

	float curtime = enginetools->GetRealTime();

	char gt[ 32 ];
	V_sprintf_safe( gt, "[game: %.3f]", enginetools->ServerTime() );
	m_pGameTime->SetText( gt );

	float elapsed = curtime - m_flLastFPSSnapShot;
	if ( elapsed < 0.4f )
		return;

	m_flLastFPSSnapShot = curtime;

	float ft = enginetools->GetRealFrameTime();
	if ( ft <= 0.0f )
	{
		m_pFPS->SetText( "[fps: ??]" );
	}
	else
	{
		char fps[ 32 ];
		V_sprintf_safe( fps, "[fps:  %.1f]", 1.0f / ft );
		m_pFPS->SetText( fps );
	}

	UpdateMemoryUsage( GetMemoryUsage() );
}

#include "winlite.h"
#include <psapi.h>
static float GetMemoryUsage()
{
	PROCESS_MEMORY_COUNTERS counters = {};
	counters.cb = sizeof( counters );

	if ( GetProcessMemoryInfo( GetCurrentProcess(), &counters, sizeof( counters ) ) )
	{
		return (float)counters.WorkingSetSize / ( 1024.0f * 1024.0f );
	}

	return 0;
}