//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ROTATINGPROGRESSBAR_H
#define ROTATINGPROGRESSBAR_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/ProgressBar.h>

namespace vgui
{

	//-----------------------------------------------------------------------------
	// Purpose: Progress Bar that rotates an image around its center 
	//-----------------------------------------------------------------------------
	class RotatingProgressBar : public ProgressBar
	{
		DECLARE_CLASS_SIMPLE_OVERRIDE( RotatingProgressBar, ProgressBar );

	public:
		RotatingProgressBar(Panel *parent, const char *panelName);
		~RotatingProgressBar();

		void ApplySettings(KeyValues *inResourceData) override;
		void ApplySchemeSettings(IScheme *pScheme) override;

		void SetImage( const char *imageName );

	protected:
		void Paint() override;
		void PaintBackground() override;
		void OnTick() override;

	private:
		int m_nTextureId;
		char *m_pszImageName;

		float m_flStartRadians;
		float m_flEndRadians;

		float m_flLastAngle;

		// dimhotepus: float -> int
		int m_flTickDelay;
		float m_flApproachSpeed;

		float m_flRotOriginX;
		float m_flRotOriginY;

		float m_flRotatingX;
		float m_flRotatingY;
		float m_flRotatingWide;
		float m_flRotatingTall;

	};		  

} // namespace vgui

#endif // ROTATINGPROGRESSBAR_H
