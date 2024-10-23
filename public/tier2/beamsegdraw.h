//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#if !defined( BEAMSEGDRAW_H )
#define BEAMSEGDRAW_H
#ifdef _WIN32
#pragma once
#endif

#define NOISE_DIVISIONS		128


#include "mathlib/vector.h"
#include "materialsystem/imesh.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct BeamTrail_t;
class IMaterial;


//-----------------------------------------------------------------------------
// CBeamSegDraw is a simple interface to beam rendering.
//-----------------------------------------------------------------------------
struct BeamSeg_t
{
	Vector		m_vPos;
	Vector		m_vColor;
	float		m_flTexCoord;	// Y texture coordinate
	float		m_flWidth;
	float		m_flAlpha;
};

class CBeamSegDraw
{
public:
	CBeamSegDraw() : m_pMeshBuilder(nullptr), m_nMeshVertCount(-1), m_nTotalSegs(-1), m_nSegsDrawn(-1), m_vNormalLast(), m_pRenderContext(nullptr)
	{
		memset( &m_Seg, 0, sizeof(m_Seg) );
	}
	// Pass null for pMaterial if you have already set the material you want.
	void			Start( IMatRenderContext *pRenderContext, int nSegs, IMaterial *pMaterial=0, CMeshBuilder *pMeshBuilder = NULL, int nMeshVertCount = 0 );
	virtual void	NextSeg( BeamSeg_t *pSeg );
	void			End();

protected:
	void			SpecifySeg( const Vector &vecCameraPos, const Vector &vNextPos );
	void			ComputeNormal( const Vector &vecCameraPos, const Vector &vStartPos, const Vector &vNextPos, Vector *pNormal );

	CMeshBuilder	*m_pMeshBuilder;
	int				m_nMeshVertCount;

	CMeshBuilder	m_Mesh;
	BeamSeg_t		m_Seg;	

	int				m_nTotalSegs;
	int				m_nSegsDrawn;

	Vector			m_vNormalLast;
	IMatRenderContext *m_pRenderContext;
};

class CBeamSegDrawArbitrary : public CBeamSegDraw
{
public:
	void			SetNormal( const Vector &normal );
	void			NextSeg( BeamSeg_t *pSeg ) override;

protected:
	void			SpecifySeg( const Vector &vNextPos );

	BeamSeg_t		m_PrevSeg;
};

//-----------------------------------------------------------------------------
// Assumes the material has already been bound
//-----------------------------------------------------------------------------
void DrawSprite( const Vector &vecOrigin, float flWidth, float flHeight, color32 color );

#endif // BEAMDRAW_H
