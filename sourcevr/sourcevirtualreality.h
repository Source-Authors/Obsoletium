//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The implementation of ISourceVirtualReality, which provides utility
//			functions for VR including head tracking, window/viewport information,
//			rendering information, and distortion
//
//=============================================================================

#ifndef SOURCEVIRTUALREALITY_H
#define SOURCEVIRTUALREALITY_H
#if defined( _WIN32 )
#pragma once
#endif

#include "tier3/tier3.h"
#include "sourcevr/isourcevirtualreality.h"
#include "materialsystem/itexture.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "openvr/headers/openvr.h"


// this is a callback so we can regenerate the distortion texture whenever we need to
class CDistortionTextureRegen : public ITextureRegenerator
{
public:
	CDistortionTextureRegen( vr::Hmd_Eye eEye ) : m_eEye( eEye ) {}
	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect ) override;
	virtual void Release()  override {}

private:
	vr::Hmd_Eye m_eEye;
};

//-----------------------------------------------------------------------------
// The implementation
//-----------------------------------------------------------------------------


class CSourceVirtualReality: public CTier3AppSystem< ISourceVirtualReality >
{
	typedef CTier3AppSystem< ISourceVirtualReality > BaseClass;

public:

	CSourceVirtualReality();
	~CSourceVirtualReality() = default;

	//---------------------------------------------------------
	// Initialization and shutdown
	//---------------------------------------------------------

	//
	// IAppSystem
	//
	bool							Connect( CreateInterfaceFn factory ) override;
	void							Disconnect() override;
	void *							QueryInterface( const char *pInterfaceName ) override;

	// these will come from the engine
	InitReturnVal_t					Init() override;
	void							Shutdown() override;


	//---------------------------------------------------------
	// ISourceVirtualReality implementation
	//---------------------------------------------------------
	virtual bool ShouldRunInVR() override;
	virtual bool IsHmdConnected() override;
	virtual void GetViewportBounds( VREye eEye, int *pnX, int *pnY, int *pnWidth, int *pnHeight ) override;
	virtual bool DoDistortionProcessing ( VREye eEye ) override;
	virtual bool CompositeHud ( VREye eEye, float ndcHudBounds[4], bool bDoUndistort, bool bBlackout, bool bTranslucent ) override;
	virtual VMatrix GetMideyePose() override;
	virtual bool SampleTrackingState ( float PlayerGameFov, float fPredictionSeconds ) override;
	virtual bool GetEyeProjectionMatrix ( VMatrix *pResult, VREye, float zNear, float zFar, float fovScale ) override;
	virtual bool WillDriftInYaw() override;
	virtual bool GetDisplayBounds( VRRect_t *pRect ) override;
	virtual VMatrix GetMidEyeFromEye( VREye eEye ) override;
	virtual unsigned GetVRModeAdapter() override;
	virtual void CreateRenderTargets( IMaterialSystem *pMaterialSystem ) override;
	virtual void ShutdownRenderTargets() override;
	virtual ITexture *GetRenderTarget( VREye eEye, EWhichRenderTarget eWhich ) override;
	virtual void GetRenderTargetFrameBufferDimensions( int & nWidth, int & nHeight ) override;
	virtual bool Activate() override;
	virtual void Deactivate() override;
	virtual bool ShouldForceVRMode( ) override;
	virtual void SetShouldForceVRMode( ) override;

	void RefreshDistortionTexture();
	void AcquireNewZeroPose();

	bool StartTracker();
	void StopTracker();	
	bool ResetTracking();	// Called to reset tracking

	// makes sure we've initialized OpenVR so we can use
	// m_pHmd
	bool EnsureOpenVRInited();

	// Prefer this to the convar so that convar will stick for the entire 
	// VR activation. We can't lazy-crate render targets and don't
	// want to create the "just in case" somebody turns on this experimental 
	// mode
	bool UsingOffscreenRenderTarget() const { return m_bUsingOffscreenRenderTarget; }

	vr::IVRSystem * GetHmd() { return m_pHmd; }
private:
	bool m_bActive;
	bool m_bShouldForceVRMode;
	bool m_bUsingOffscreenRenderTarget; 
	CDistortionTextureRegen m_textureGeneratorLeft;
	CDistortionTextureRegen m_textureGeneratorRight;
	CTextureReference g_StereoGuiTexture;
	CTextureReference m_pDistortionTextureLeft;
	CTextureReference m_pDistortionTextureRight;
	CTextureReference m_pPredistortRT;
	CTextureReference m_pPredistortRTDepth;
	CMaterialReference m_warpMaterial;
	CMaterialReference m_DistortLeftMaterial;
	CMaterialReference m_DistortRightMaterial;
	CMaterialReference m_DistortHUDLeftMaterial;
	CMaterialReference m_DistortHUDRightMaterial;
	CMaterialReference m_InWorldUIMaterial;
	CMaterialReference m_InWorldUIOpaqueMaterial;
	CMaterialReference m_blackMaterial;

	vr::IVRSystem				*m_pHmd;

	bool	  m_bHaveValidPose;
	VMatrix   m_ZeroFromHeadPose;

};


#endif // SOURCEVIRTUALREALITY_H
