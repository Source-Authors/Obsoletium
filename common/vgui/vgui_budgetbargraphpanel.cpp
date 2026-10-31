//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "vgui_budgetbargraphpanel.h"

#include "vgui_basebudgetpanel.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Label.h"
#include "tier1/convar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// dimhotepus: Add color bounds [0..255].
ConVar budget_bargraph_background_alpha( "budget_bargraph_background_alpha", "128", FCVAR_ARCHIVE, "how translucent the budget panel is", true, 0, true, std::numeric_limits<uint8_t>::max() );

ConVar budget_peaks_window( "budget_peaks_window", "30", FCVAR_ARCHIVE, "number of frames to look at when figuring out peak frametimes" );
ConVar budget_averages_window( "budget_averages_window", "30", FCVAR_ARCHIVE, "number of frames to look at when figuring out average frametimes" );
ConVar budget_show_peaks( "budget_show_peaks", "1", FCVAR_ARCHIVE, "enable/disable peaks in the budget panel" );
ConVar budget_show_averages( "budget_show_averages", "0", FCVAR_ARCHIVE, "enable/disable averages in the budget panel" );


CBudgetBarGraphPanel::CBudgetBarGraphPanel( CBaseBudgetPanel *pParent, const char *pPanelName ) : 
	BaseClass( pParent, pPanelName )
{
	m_pBudgetPanel = pParent;

	// dimhotepus: Scale UI.
	// SetProportional( false );
	SetKeyBoardInputEnabled( false );
	SetMouseInputEnabled( false );
	SetVisible( true );

	SetPaintBackgroundEnabled( true );
	SetBgColor( Color( 255, 0, 0, size_cast<uint8_t>( budget_bargraph_background_alpha.GetInt() ) ) );
}

CBudgetBarGraphPanel::~CBudgetBarGraphPanel() = default;

void CBudgetBarGraphPanel::GetBudgetGroupTopAndBottom( intp id, int &top, int &bottom )
{
	// Ask where the corresponding graph label is.
	m_pBudgetPanel->GetGraphLabelScreenSpaceTopAndBottom( id, top, bottom );
	int tall = bottom - top;

	int x = 0;
	ScreenToLocal( x, top );

	bottom = top + tall;
}

void CBudgetBarGraphPanel::DrawBarAtIndex( intp id, float percent )
{
	int panelWidth, panelHeight;
	GetSize( panelWidth, panelHeight );

	int top, bottom;
	GetBudgetGroupTopAndBottom( id, top, bottom );

	int left = 0;
	int right = static_cast<int>( panelWidth * percent );

	int red, green, blue, alpha;
	m_pBudgetPanel->GetConfigData().m_BudgetGroupInfo[id].m_Color.GetColor( red, green, blue, alpha );
										 
	// DrawFilledRect is panel relative
	vgui::surface()->DrawSetColor( 0, 0, 0, alpha );
	vgui::surface()->DrawFilledRect( left, top, right+2, bottom );

	vgui::surface()->DrawSetColor( 255, 255, 255, alpha );
	vgui::surface()->DrawFilledRect( left, top+1, right+1, bottom-1 );

	vgui::surface()->DrawSetColor( red, green, blue, alpha );
	vgui::surface()->DrawFilledRect( left, top+2, right, bottom-2 );
}

void CBudgetBarGraphPanel::DrawTickAtIndex( intp id, float percent, int red, int green, int blue, int alpha )
{
	if( percent > 1.0f )
	{
		percent = 1.0f;
	}
	int panelWidth, panelHeight;
	GetSize( panelWidth, panelHeight );

	int top, bottom;
	GetBudgetGroupTopAndBottom( id, top, bottom );

	int right = ( int )( panelWidth * percent + 1.0f );
	int left = right - 2;

	// DrawFilledRect is panel relative
	vgui::surface()->DrawSetColor( 0, 0, 0, alpha );
	vgui::surface()->DrawFilledRect( left-2, top, right+2, bottom );

	vgui::surface()->DrawSetColor( 255, 255, 255, alpha );
	vgui::surface()->DrawFilledRect( left-1, top+1, right+1, bottom-1 );

	vgui::surface()->DrawSetColor( red, green, blue, alpha );
	vgui::surface()->DrawFilledRect( left, top+2, right, bottom-2 );
}

void CBudgetBarGraphPanel::DrawTimeLines()
{
	int panelWidth, panelHeight;
	GetSize( panelWidth, panelHeight );
	int i;
	int left, right, top, bottom;
	top = 0;
	bottom = panelHeight;

	const CBudgetPanelConfigData &config = m_pBudgetPanel->GetConfigData();

	float flValueInterval = config.m_flTimeLabelInterval;
	if ( config.m_nLinesPerTimeLabel != 0.0f )
	{
		flValueInterval = config.m_flTimeLabelInterval / config.m_nLinesPerTimeLabel;
	}
	
	int nTotalLines = flValueInterval != 0.0f
		? static_cast<int>( config.m_flBarGraphRange / flValueInterval )
		: static_cast<int>( config.m_flBarGraphRange );
	nTotalLines += 2;
	
	for( i = 0; i < nTotalLines; i++ )
	{
		int alpha;
		if( i % (config.m_nLinesPerTimeLabel*2) == 0 )
		{
			alpha = 150;
		}
		else if( i % config.m_nLinesPerTimeLabel == 0 ) 
		{
			alpha = 100;
		}
		else
		{
			alpha = 50;
		}
		
		float flTemp = ( config.m_flBarGraphRange != 0.0f ) ? ( flValueInterval / config.m_flBarGraphRange ) : flValueInterval;
		left = static_cast<int>( -0.5f + panelWidth * ( i * flTemp ) );
		right = left + 1;

		vgui::surface()->DrawSetColor( 0, 0, 0, alpha );
		vgui::surface()->DrawFilledRect( left-1, top, right+1, bottom );

		vgui::surface()->DrawSetColor( 255, 255, 255, alpha );
		vgui::surface()->DrawFilledRect( left, top+1, right, bottom-1 );
	}
}

void CBudgetBarGraphPanel::DrawInstantaneous()
{
	intp nGroups;
	int nSamplesPerGroup, nSampleOffset;
	const double *pBudgetGroupTimes = m_pBudgetPanel->GetBudgetGroupData( nGroups, nSamplesPerGroup, nSampleOffset );
	if( !pBudgetGroupTimes )
	{
		return;
	}

	intp i;
	for( i = 0; i < nGroups; i++ )
	{
		float percent = m_pBudgetPanel->GetBudgetGroupPercent( static_cast<float>( pBudgetGroupTimes[nSamplesPerGroup * i + nSampleOffset] ) );
		DrawBarAtIndex( i, percent );
	}
}

void CBudgetBarGraphPanel::DrawPeaks()
{
	intp nGroups;
	int nSamplesPerGroup, nSampleOffset;
	const double *pBudgetGroupTimes = m_pBudgetPanel->GetBudgetGroupData( nGroups, nSamplesPerGroup, nSampleOffset );
	if( !pBudgetGroupTimes )
	{
		return;
	}
	const int numSamples = budget_peaks_window.GetInt();
	for( intp i = 0; i < nGroups; i++ )
	{
		double max = 0.0;
		for( int j = 0; j < numSamples; j++ )
		{
			const int offset = ( nSampleOffset - j + BUDGET_HISTORY_COUNT ) % BUDGET_HISTORY_COUNT;
			double tmp = pBudgetGroupTimes[i * nSamplesPerGroup + offset];
			if( tmp > max )
			{
				max = tmp;
			}
		}
		const float percent = m_pBudgetPanel->GetBudgetGroupPercent( static_cast<float>( max ) );
		DrawTickAtIndex( i, percent, 255, 0, 0, 255 );
	}
}

void CBudgetBarGraphPanel::DrawAverages()
{
	intp nGroups;
	int nSamplesPerGroup, nSampleOffset;
	const double *pBudgetGroupTimes = m_pBudgetPanel->GetBudgetGroupData( nGroups, nSamplesPerGroup, nSampleOffset );
	if( !pBudgetGroupTimes )
	{
		return;
	}
	const int numSamples = budget_averages_window.GetInt();
	for( intp i = 0; i < nGroups; i++ )
	{
		int red, green, blue, alpha;
		m_pBudgetPanel->GetConfigData().m_BudgetGroupInfo[i].m_Color.GetColor( red, green, blue, alpha );

		double sum = 0.0;
		for( int j = 0; j < numSamples; j++ )
		{
			const int offset = ( nSampleOffset - j + BUDGET_HISTORY_COUNT ) % BUDGET_HISTORY_COUNT;
			sum += pBudgetGroupTimes[i * nSamplesPerGroup + offset];
		}
		sum *= 1.0 / numSamples;
		const float percent = m_pBudgetPanel->GetBudgetGroupPercent( static_cast<float>( sum ) );
		DrawTickAtIndex( i, percent, red, green, blue, alpha );
	}
}

void CBudgetBarGraphPanel::Paint()
{
	int width, height;
	GetSize( width, height );

	if ( !m_pBudgetPanel->IsDedicated() )
	{
		SetBgColor( Color( 255, 0, 0, static_cast<uint8_t>( budget_bargraph_background_alpha.GetInt() ) ) );
	}

	DrawTimeLines();
	DrawInstantaneous();
	if( budget_show_peaks.GetBool() )
	{
		DrawPeaks();
	}
	if( budget_show_averages.GetBool() )
	{
		DrawAverages();
	}
}

