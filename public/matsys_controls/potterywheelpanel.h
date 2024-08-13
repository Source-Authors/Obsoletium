//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef POTTERYWHEELPANEL_H
#define POTTERYWHEELPANEL_H

#ifdef _WIN32
#pragma once
#endif


#include "vgui_controls/EditablePanel.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "tier2/camerautils.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IManipulator;
class CPotteryWheelManip;
class CBaseManipulator;
class CTransformManipulator;
class CDmxElement;

namespace vgui
{
	class IScheme;
}


//-----------------------------------------------------------------------------
// Pottery wheel Panel
//-----------------------------------------------------------------------------
class CPotteryWheelPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CPotteryWheelPanel, vgui::EditablePanel );

public:
	// constructor, destructor
	CPotteryWheelPanel( vgui::Panel *pParent, const char *pName );
	virtual ~CPotteryWheelPanel();

	// Overriden methods of vgui::Panel
	void ApplySettings( KeyValues *inResourceData ) override;
	void Init( int x, int y, int wide, int tall ) override;
	void Paint() override;

	void OnKeyCodePressed ( vgui::KeyCode code ) override;
	void OnKeyCodeReleased( vgui::KeyCode code ) override;
	void OnMousePressed ( vgui::MouseCode code ) override;
	void OnMouseReleased( vgui::MouseCode code ) override;
	void OnCursorMoved( int x, int y ) override;
	void OnMouseWheeled( int delta ) override;
	void OnTick() override;

	void OnMouseCaptureLost() override;

	// Sets the camera to look at the the thing we're spinning around
	void LookAt( const Vector &vecCenter, float flRadius );
	void LookAt( float flRadius );

	void ComputePanelPosition( const Vector &vecPosition, Vector2D *pPanelPos );

	void SetBackgroundColor( int r, int g, int b );
	void SetBackgroundColor( const Color& c );
	const Color& GetBackgroundColor() const;

	// Light probe
	void SetLightProbe( CDmxElement *pLightProbe );

	// Camera.
	int	 GetCameraFOV( void );
	void SetCameraFOV( float flFOV );
	void SetCameraPositionAndAngles( const Vector &vecPos, const QAngle &angDir, bool syncManipulators = true );
	void GetCameraPositionAndAngles( Vector &vecPos, QAngle &angDir );
	void SetCameraOffset( const Vector &vecOffset );
	void GetCameraOffset( Vector &vecOffset );
	void ResetCameraPivot( void );
	void ComputeCameraTransform( matrix3x4_t *pWorldToCamera );
	void UpdateCameraTransform();

private:
	// Inherited classes must implement this
	virtual void OnPaint3D() = 0;

protected:

	enum
	{
		MAX_LIGHT_COUNT = 4
	};
	
	struct LightInfo_t
	{
		LightDesc_t m_Desc;
		matrix3x4_t m_LightToWorld;
	};



	enum ManipulationMode_t 
	{
		CAMERA_ROTATE,
		CAMERA_TRANSLATE,
		CAMERA_ZOOM,
		LIGHT_MODE,
	};

	virtual void EnterManipulationMode( ManipulationMode_t manipMode, bool bMouseCapture = true, vgui::MouseCode mouseCode = vgui::MouseCode( -1 ) );
	void Select();
	void AcceptManipulation( bool bReleaseMouseCapture = true );
	void CancelManipulation();
	void EnableMouseCapture( bool enable, vgui::MouseCode code = vgui::MouseCode( -1 ) );
	bool WarpMouse( int &x, int &y );
	IManipulator		*m_pCurrentManip;
	int m_nManipStartX, m_nManipStartY;
	int m_nClickStartX, m_nClickStartY;

	// Re-apply the manipulators on a new model
	void ApplyManipulation();

	// Synchronize the manipulators with the current transform
	void SyncManipulation();

	bool HasLightProbe() const;
	ITexture *GetLightProbeCubemap( bool bHDR );
	void DrawGrid();
	CMaterialReference	m_Wireframe;

	bool	m_bRenderToTexture;

	virtual void SetupRenderState( int nDisplayWidth, int nDisplayHeight );

private:
	void CreateDefaultLights();
	void DestroyLights();
	void ParseLightsFromKV( KeyValues *pLightsKV );

	CMaterialReference m_LightProbeBackground;
	CMaterialReference m_LightProbeHDRBackground;
	CTextureReference m_LightProbeCubemap;
	CTextureReference m_LightProbeHDRCubemap;

	Camera_t m_Camera;
	matrix3x4_t m_CameraPivot;
	int m_nLightCount;
	LightInfo_t m_Lights[MAX_LIGHT_COUNT];
	Vector4D m_vecAmbientCube[6];

	Color m_ClearColor;
	Vector					m_vecCameraOffset;
	CTransformManipulator	*m_pCameraRotate;
	CTransformManipulator	*m_pCameraTranslate;
	CBaseManipulator		*m_pCameraZoom;
	CPotteryWheelManip		*m_pLightManip;
	vgui::MouseCode			m_nCaptureMouseCode;

	int m_xoffset, m_yoffset;

	bool	m_bHasLightProbe : 1;

	CPanelAnimationVar( bool, m_bUseParentBG, "useparentbg", "0" );
};


#endif // SIMPLEPOTTERYWHEELPANEL_H
