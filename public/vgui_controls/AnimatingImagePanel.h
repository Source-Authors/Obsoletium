//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ANIMATINGIMAGEPANEL_H
#define ANIMATINGIMAGEPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <tier1/utlvector.h>
#include <vgui_controls/Panel.h>

namespace vgui
{

//-----------------------------------------------------------------------------
// Purpose: Animating image
//-----------------------------------------------------------------------------
class AnimatingImagePanel : public Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( AnimatingImagePanel, Panel );

public:
	AnimatingImagePanel(Panel *parent, const char *name);

	// Add an image to the end of the list of animations
	// image - pointer to the image to add to the end of the list
	virtual void AddImage(IImage *image);
	
	// Load a set of animations by name.
	// baseName - The name of the animations without their frame number or file extension, (e.g. c1.tga becomes just c.)
	// framecount: number of frames in the animation
	virtual void LoadAnimation(const char *baseName, int frameCount);

	virtual void StartAnimation();
	virtual void StopAnimation();
	virtual void ResetAnimation(int frame = 0);

protected:
	void OnTick() override;
	void PerformLayout() override;
	void PaintBackground() override;

	void GetSettings(KeyValues *outResourceData) override;
	void ApplySettings(KeyValues *inResourceData) override;
	const char *GetDescription() override;

private:
	int m_iCurrentImage;
	int m_iNextFrameTime;
	int m_iFrameTimeMillis;
	CUtlVector<IImage *> m_Frames;
	char *m_pImageName;
	bool m_bAnimating;
	bool m_bFiltered;
	bool m_bScaleImage;
};

}; // namespace vgui

#endif // ANIMATINGIMAGEPANEL_H
