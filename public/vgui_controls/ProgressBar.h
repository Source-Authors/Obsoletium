//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PROGRESSBAR_H
#define PROGRESSBAR_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

namespace vgui
{

//-----------------------------------------------------------------------------
// Purpose: Status bar that visually displays discrete progress in the form
//			of a segmented strip
//-----------------------------------------------------------------------------
class ProgressBar : public Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( ProgressBar, Panel );

public:
	ProgressBar(Panel *parent, const char *panelName);
	~ProgressBar();

	// 'progress' is in the range [0.0f, 1.0f]
	MESSAGE_FUNC_FLOAT( SetProgress, "SetProgress", progress );
	float GetProgress();
	virtual void SetSegmentInfo( int gap, int width );

	// utility function for calculating a time remaining string
	static bool ConstructTimeRemainingString(OUT_Z_BYTECAP(outputBufferSizeInBytes) wchar_t *output,
		intp outputBufferSizeInBytes,
		float startTime,
		float currentTime,
		float currentProgress,
		float lastProgressUpdateTime,
		bool addRemainingSuffix);

	template<intp size>
	static bool ConstructTimeRemainingString(OUT_Z_ARRAY wchar_t (&output)[size],
		float startTime,
		float currentTime,
		float currentProgress,
		float lastProgressUpdateTime,
		bool addRemainingSuffix)
	{
		return ConstructTimeRemainingString( output, size,
			startTime, currentTime,
			currentProgress, lastProgressUpdateTime,
			addRemainingSuffix );
	}

	void SetBarInset( int pixels );
	int GetBarInset( void );
	void SetMargin( int pixels );
	int GetMargin();
	
	void ApplySettings(KeyValues *inResourceData) override;
	void GetSettings(KeyValues *outResourceData) override;
	const char *GetDescription() override;

	// returns the number of segment blocks drawn
	int GetDrawnSegmentCount();

	enum ProgressDir_e
	{
		PROGRESS_EAST,
		PROGRESS_WEST,
		PROGRESS_NORTH,
		PROGRESS_SOUTH
	};

	int GetProgressDirection() const { return m_iProgressDirection; }
	void SetProgressDirection( int val ) { m_iProgressDirection = val; }

protected:
	void Paint() override;
	void PaintSegment( int &x, int &y, int tall, int wide );
	void PaintBackground() override;
	void ApplySchemeSettings(IScheme *pScheme) override;
	MESSAGE_FUNC_PARAMS( OnDialogVariablesChanged, "DialogVariables", dialogVariables );
	/* CUSTOM MESSAGE HANDLING
		"SetProgress"
			input:	"progress"	- float value of the progress to set
	*/

protected:
	int m_iProgressDirection;
	float _progress;

private:
	int _segmentGap;
	int _segmentWide;
	int m_iBarInset;
	int m_iBarMargin;
	char *m_pszDialogVar;
};

//-----------------------------------------------------------------------------
// Purpose: Non-segmented progress bar
//-----------------------------------------------------------------------------
class ContinuousProgressBar : public ProgressBar
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( ContinuousProgressBar, ProgressBar );

public:
	ContinuousProgressBar(Panel *parent, const char *panelName);
	MESSAGE_FUNC_FLOAT( SetPrevProgress, "SetPrevProgress", prevProgress );

	void SetGainColor( Color color ) { m_colorGain = color; }
	void SetLossColor( Color color ) { m_colorLoss = color; }

	void Paint() override;

protected:
	float _prevProgress;
	Color m_colorGain;
	Color m_colorLoss;
};

} // namespace vgui

#endif // PROGRESSBAR_H
