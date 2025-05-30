//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CHANNELGRAPHPANEL_H
#define CHANNELGRAPHPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include <vgui_controls/Frame.h>
#include "utllinkedlist.h"
#include "utlvector.h"
#include "movieobjects/dmechannel.h"
#include "datamodel/dmehandle.h"

namespace vgui
{

typedef DmeTime_t (*TimeAccessor_t)();

//-----------------------------------------------------------------------------
// Purpose: Holds and displays a chart of dmechannel data
//-----------------------------------------------------------------------------
class CChannelGraphPanel : public Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CChannelGraphPanel, Panel );

public:
	CChannelGraphPanel( Panel *parent, const char *name );

	void SetChannel( CDmeChannel *pChannel );

	// input messages
	void OnCursorMoved( int mx, int my ) override;
	void OnMousePressed( MouseCode code ) override;
	void OnMouseReleased( MouseCode code ) override;
	void OnMouseWheeled( int delta ) override;
	void OnSizeChanged( int newWide, int newTall ) override;	// called after the size of a panel has been changed

protected:
	void Paint() override;
	void PerformLayout() override;
	void ApplySchemeSettings( IScheme *pScheme ) override;

	int TimeToPixel( DmeTime_t time );
	int ValueToPixel( float flValue );

private:
	CDmeHandle< CDmeChannel > m_hChannel;
	HFont m_font;
	// dimhotepus: Comment unused field.
	// TimeAccessor_t m_timeFunc;
	DmeTime_t m_graphMinTime, m_graphMaxTime;
	float m_graphMinValue, m_graphMaxValue;
	int m_nMouseStartX, m_nMouseStartY;
	int m_nMouseLastX, m_nMouseLastY;
	int m_nTextBorder;
	int m_nGraphOriginX;
	int m_nGraphOriginY;
	float m_flTimeToPixel;
	float m_flValueToPixel;
};


//-----------------------------------------------------------------------------
// CChannelGraphFrame
//-----------------------------------------------------------------------------
class CChannelGraphFrame : public Frame
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CChannelGraphFrame, Frame );

public:
	CChannelGraphFrame( Panel *parent, const char *pTitle );

	void SetChannel( CDmeChannel *pChannel );

	void OnCommand( const char *cmd ) override;
	void PerformLayout() override;

protected:
	CChannelGraphPanel *m_pChannelGraph;
};

} // namespace vgui

#endif // CHANNELGRAPHPANEL_H
