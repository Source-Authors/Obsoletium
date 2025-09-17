//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef VGUI_BUDGETBARGRAPHPANEL_H
#define VGUI_BUDGETBARGRAPHPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>


class CBaseBudgetPanel;


class CBudgetBarGraphPanel : public vgui::Panel
{
	using BaseClass = vgui::Panel;

public:
	CBudgetBarGraphPanel( CBaseBudgetPanel *pParent, const char *pPanelName );
	~CBudgetBarGraphPanel() override;

	void Paint() override;

private:

	void DrawInstantaneous();
	void DrawPeaks();
	void DrawAverages();
	void DrawTimeLines();
	void GetBudgetGroupTopAndBottom( intp id, int &top, int &bottom );
	void DrawBarAtIndex( intp id, float percent );
	void DrawTickAtIndex( intp id, float percent, int red, int green, int blue, int alpha );

private:
	
	CBaseBudgetPanel *m_pBudgetPanel;
};

#endif // VGUI_BUDGETBARGRAPHPANEL_H
