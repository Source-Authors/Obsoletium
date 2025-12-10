//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//=============================================================================

#include <stdafx.h>
#include "DispShore.h"
#include "FaceEditSheet.h"
#include "MainFrm.h"
#include "GlobalFunctions.h"
#include "MapDisp.h"
#include "utlvector.h"
#include "mapdoc.h"
#include "mapworld.h"
#include "mapsolid.h"
#include "materialsystem/imesh.h"
#include "Material.h"
#include "collisionutils.h"
#include "TextureSystem.h"
#include "mapoverlay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

IMPLEMENT_MAPCLASS( CMapOverlayTransition )

#define DISPSHORE_WIDTH_WORLD		25.0f
#define DISPSHORE_WIDTH_WATER		25.0f
#define DISPSHORE_VECTOR_EPS		0.1f
#define	DISPSHORE_SURF_LENGTH		120.0f

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Shoreline_t::Shoreline_t()
{
	m_nShorelineId = -1;
	m_aSegments.Purge();
	m_aOverlays.Purge();
	m_flLength = 0.0f;
	memset(&m_ShoreData, 0, sizeof(m_ShoreData));
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Shoreline_t::~Shoreline_t()
{
	m_aSegments.Purge();
	m_aOverlays.Purge();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Shoreline_t::AddSegment( Vector &vecPoint0, Vector &vecPoint1, 
						      Vector &vecNormal, float flWaterZ, 
							  CMapFace *pWaterFace, EditDispHandle_t hDisp )
{
	// Check for duplicates!
	intp nSegmentCount = m_aSegments.Count();
	for ( intp iSegment = 0; iSegment < nSegmentCount; ++iSegment )
	{
		// dimhotepus: Cache to speedup.
		const auto &segment = m_aSegments[iSegment];

		if ( VectorsAreEqual( segment.m_vecPoints[0], vecPoint0, DISPSHORE_VECTOR_EPS ) ) 
		{ 
			if ( VectorsAreEqual( segment.m_vecPoints[1], vecPoint1, DISPSHORE_VECTOR_EPS ) ) 
				return;
		}

		if ( VectorsAreEqual( segment.m_vecPoints[1], vecPoint0, DISPSHORE_VECTOR_EPS ) ) 
		{
			if ( VectorsAreEqual( segment.m_vecPoints[0], vecPoint1, DISPSHORE_VECTOR_EPS ) )
				return;
		}
	}

	const intp iSegment = m_aSegments.AddToTail();
	auto &segment = m_aSegments[iSegment];

	Vector vecEdge, vecCross;
	VectorSubtract( vecPoint1, vecPoint0, vecEdge );
	CrossProduct( vecNormal, vecEdge, vecCross );
	if ( vecCross.z >= 0.0f )
	{
		VectorCopy( vecPoint1, segment.m_vecPoints[0] );
		VectorCopy( vecPoint0, segment.m_vecPoints[1] );
	}
	else
	{
		VectorCopy( vecPoint0, segment.m_vecPoints[0] );
		VectorCopy( vecPoint1, segment.m_vecPoints[1] );
	}

	VectorCopy( vecNormal, segment.m_vecNormals[0] );
	VectorCopy( vecNormal, segment.m_vecNormals[1] );

	segment.m_hDisp = hDisp;
	segment.m_flWaterZ = flWaterZ;
	segment.m_iStartPoint = 0;
	segment.m_bTouch = false;
	segment.m_bCreated = false;
	segment.m_vecCenter.Init();

	segment.m_WorldFace.m_bAdjWinding = false;	
	segment.m_WaterFace.m_bAdjWinding = false;
	for ( int i = 0; i < 4; ++i )
	{
		segment.m_WorldFace.m_vecPoints[i].Init();
		segment.m_WorldFace.m_vecTexCoords[i].Init();
		segment.m_WorldFace.m_pFaces[i] = NULL;

		segment.m_WaterFace.m_vecPoints[i].Init();
		segment.m_WaterFace.m_vecTexCoords[i].Init();
		segment.m_WaterFace.m_pFaces[i] = NULL;
	}
}

//=============================================================================
//
// CDispShoreManager
//
class CDispShoreManager : public IDispShoreManager
{
public:

	CDispShoreManager();
	~CDispShoreManager();

	// Interface.
	bool		Init( void );
	void		Shutdown( void );

	intp		GetShorelineCount( void );
	Shoreline_t *GetShoreline( intp nShorelineId );
	void		AddShoreline( intp nShorelineId );
	void		RemoveShoreline( intp nShorelineId );
	void		BuildShoreline( intp nShorelineId, CUtlVector<CMapFace*> &aFaces, CUtlVector<CMapFace*> &aWaterFaces );
       
	void		Draw( CRender3D *pRender );
	void		DebugDraw( CRender3D *pRender );

private:

	void BuildShorelineSegments( Shoreline_t *pShoreline, CUtlVector<CMapFace*> &aFaces, CUtlVector<CMapFace*> &aWaterFaces );
	void AverageShorelineNormals( Shoreline_t *pShoreline );
	void BuildShorelineOverlayPoints( Shoreline_t *pShoreline, CUtlVector<CMapFace*> &aWaterFaces );
	void BuildShorelineOverlayPoint( Shoreline_t *pShoreline, intp iSegment, CUtlVector<CMapFace*> &aWaterFaces );
	bool TexcoordShoreline( Shoreline_t *pShoreline );
	void ShorelineLength( Shoreline_t *pShoreline );
	void GenerateTexCoord( Shoreline_t *pShoreline, intp iSegment, float flLengthToSegment, bool bEnd );
	void BuildShorelineOverlays( Shoreline_t *pShoreline );
	void CreateOverlays( Shoreline_t *pShoreline, intp iSegment );

	void DrawShorelines( intp iShoreline );
	void DrawShorelineNormals( intp iShoreline );
	void DrawShorelineOverlayPoints( CRender3D *pRender, intp iShoreline );

	bool ConnectShorelineSegments( Shoreline_t *pShoreline );
	intp FindShorelineStart( Shoreline_t *pShoreline );

	bool IsTouched( Shoreline_t *pShoreline, intp iSegment ) const	{ return pShoreline->m_aSegments[iSegment].m_bTouch; }

private:

	CUtlVector<Shoreline_t>		m_aShorelines;

	// Displacement face and water face cache - for building.
	CUtlVector<CMapDisp*>		m_aDispCache;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static CDispShoreManager s_DispShoreManager;

IDispShoreManager *GetShoreManager( void )
{
	return &s_DispShoreManager;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CDispShoreManager::CDispShoreManager()
{
	m_aDispCache.Purge();
	m_aShorelines.Purge();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CDispShoreManager::~CDispShoreManager()
{
	m_aDispCache.Purge();
	m_aShorelines.Purge();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CDispShoreManager::Init( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::Shutdown( void )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
intp CDispShoreManager::GetShorelineCount( void )
{
	return m_aShorelines.Count();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Shoreline_t *CDispShoreManager::GetShoreline( intp nShorelineId )
{
	for ( auto &l : m_aShorelines )
	{
		if ( l.m_nShorelineId == nShorelineId )
			return &l;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::AddShoreline( intp nShorelineId )
{
	// Check to see if the id is already taken, if so remove it and re-add it.
	RemoveShoreline( nShorelineId );

	intp iShoreline = m_aShorelines.AddToTail();
	m_aShorelines[iShoreline].m_nShorelineId = nShorelineId;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::RemoveShoreline( intp nShorelineId )
{
	intp nShorelineCount = m_aShorelines.Count();
	for ( intp iShoreline = ( nShorelineCount - 1 ); iShoreline >= 0; --iShoreline )
	{
		if ( m_aShorelines[iShoreline].m_nShorelineId == nShorelineId )
		{
			m_aShorelines.Remove( iShoreline );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::BuildShoreline( intp nShorelineId, CUtlVector<CMapFace*> &aFaces, CUtlVector<CMapFace*> &aWaterFaces )
{
	// Verify faces to build a shoreline.
	if ( ( aFaces.Count() == 0 ) ||( aWaterFaces.Count() == 0 ) )
		return;

	Shoreline_t *pShoreline = GetShoreline( nShorelineId );
	if ( pShoreline )
	{
		BuildShorelineSegments( pShoreline, aFaces, aWaterFaces );
		AverageShorelineNormals( pShoreline );
		BuildShorelineOverlayPoints( pShoreline, aWaterFaces );
		TexcoordShoreline( pShoreline );
		BuildShorelineOverlays( pShoreline );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::BuildShorelineSegments( Shoreline_t *pShoreline, CUtlVector<CMapFace*> &aFaces, CUtlVector<CMapFace*> &aWaterFaces )
{
	intp nWaterFaceCount = aWaterFaces.Count();
	for ( intp iWaterFace = 0; iWaterFace < nWaterFaceCount; ++iWaterFace )
	{
		intp nFaceCount = aFaces.Count();
		for ( intp iFace = 0; iFace < nFaceCount; ++iFace )
		{	
			CMapFace *pFace = aFaces.Element( iFace );
			if ( pFace )
			{
				if ( !pFace->HasDisp() )
				{
					// Ignore for now!
				}
				else
				{
					// Displacement.
					CMapDisp *pDisp = EditDispMgr()->GetDisp( pFace->GetDisp() );
					if ( pDisp )
					{
						pDisp->CreateShoreOverlays( aWaterFaces[iWaterFace], pShoreline );
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::AverageShorelineNormals( Shoreline_t *pShoreline )
{
	intp nSegmentCount = pShoreline->m_aSegments.Count();
	if ( nSegmentCount == 0 )
		return;

	for ( intp iSegment1 = 0; iSegment1 < nSegmentCount; ++iSegment1 )
	{
		for ( intp iSegment2 = iSegment1 + 1; iSegment2 < nSegmentCount; ++iSegment2 )
		{
			int iPoint1 = -1;
			int iPoint2 = -1;

			// dimhotepus: Cache segments.
			auto &segment1 = pShoreline->m_aSegments[iSegment1];
			auto &segment2 = pShoreline->m_aSegments[iSegment2];

			if ( VectorsAreEqual( segment1.m_vecPoints[0], 
				                  segment2.m_vecPoints[0], DISPSHORE_VECTOR_EPS ) )
			{
				iPoint1 = 0;
				iPoint2 = 0;
			}

			if ( VectorsAreEqual( segment1.m_vecPoints[0], 
				                  segment2.m_vecPoints[1], DISPSHORE_VECTOR_EPS ) )
			{
				iPoint1 = 0;
				iPoint2 = 1;
			}

			if ( VectorsAreEqual( segment1.m_vecPoints[1], 
				                  segment2.m_vecPoints[0], DISPSHORE_VECTOR_EPS ) )
			{
				iPoint1 = 1;
				iPoint2 = 0;
			}

			if ( VectorsAreEqual( segment1.m_vecPoints[1], 
				                  segment2.m_vecPoints[1], DISPSHORE_VECTOR_EPS ) )
			{
				iPoint1 = 1;
				iPoint2 = 1;
			}

			if ( ( iPoint1 != -1 ) && ( iPoint2 != -1 ) )
			{
				segment2.m_vecPoints[iPoint2] = segment1.m_vecPoints[iPoint1];

				Vector vecNormal = segment1.m_vecNormals[iPoint1] + segment2.m_vecNormals[iPoint2];
				VectorNormalize( vecNormal );

				segment1.m_vecNormals[iPoint1] = vecNormal;
				segment2.m_vecNormals[iPoint2] = vecNormal;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::BuildShorelineOverlayPoints( Shoreline_t *pShoreline, CUtlVector<CMapFace*> &aWaterFaces )
{
	intp nSegmentCount = pShoreline->m_aSegments.Count();
	if ( nSegmentCount == 0 )
		return;

	for ( intp iSegment = 0; iSegment < nSegmentCount; ++iSegment )
	{
		BuildShorelineOverlayPoint( pShoreline, iSegment, aWaterFaces );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::BuildShorelineOverlayPoint( Shoreline_t *pShoreline, intp iSegment, CUtlVector<CMapFace*> &aWaterFaces )
{
	// dimhotepus: Cache to speedup.
	auto &segment = pShoreline->m_aSegments[iSegment];
	// Get the displacement manager and segment displacement.
	CMapDisp *pDisp = EditDispMgr()->GetDisp( segment.m_hDisp );
	if ( !pDisp )
		return;

	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( !pDispMgr )
		return;

	// Build a bounding box from the world points.
	Vector vecPoints[4];	
	vecPoints[0] = segment.m_vecPoints[0];
	vecPoints[3] = segment.m_vecPoints[1];
	vecPoints[1] = vecPoints[0] + ( segment.m_vecNormals[0] * pShoreline->m_ShoreData.m_flWidths[0] );
	vecPoints[2] = vecPoints[3] + ( segment.m_vecNormals[1] * pShoreline->m_ShoreData.m_flWidths[0] );

	Vector vecWorldMin = vecPoints[0]; 
	Vector vecWorldMax = vecPoints[0];
	for ( int iPoint = 1; iPoint < 4; ++iPoint )
	{
		for ( int iAxis = 0; iAxis < 3; ++iAxis )
		{
			if ( vecPoints[iPoint][iAxis] < vecWorldMin[iAxis] )
			{
				vecWorldMin[iAxis] = vecPoints[iPoint][iAxis];
			}

			if ( vecPoints[iPoint][iAxis] > vecWorldMax[iAxis] )
			{
				vecWorldMax[iAxis] = vecPoints[iPoint][iAxis];
			}
		}
	}

	for ( int iAxis = 0; iAxis < 2; ++iAxis )
	{
		vecWorldMin[iAxis] -= 1.0f;
		vecWorldMax[iAxis] += 1.0f;
	}
	vecWorldMin.z -= 150.0f;
	vecWorldMax.z += 150.0f;

	// Build a list of displacements that intersect the bounding box.
	CUtlVector<CMapDisp*> m_aDispList;
	m_aDispList.Purge();

	Vector vecDispMin, vecDispMax;
	intp nDispCount = pDispMgr->WorldCount();
	for ( intp iDisp = 0; iDisp < nDispCount; ++iDisp )
	{
		CMapDisp *pCurDisp = pDispMgr->GetFromWorld( iDisp );
		if ( !pCurDisp )
			continue;

		if ( pCurDisp == pDisp )
			continue;

		// Check for intersections.
		pCurDisp->GetBoundingBox( vecDispMin, vecDispMax );
		if ( IsBoxIntersectingBox( vecWorldMin, vecWorldMax, vecDispMin, vecDispMax ) )
		{
			m_aDispList.AddToTail( pCurDisp );
		}
	}

	// World points.
	CMapFace *pFace = static_cast<CMapFace*>( pDisp->GetParent() );
	for ( int iFace = 0; iFace < 4; ++iFace )
	{
		segment.m_WorldFace.m_pFaces[iFace] = pFace;
	}

	segment.m_WorldFace.m_vecPoints[0] = segment.m_vecPoints[0];
	segment.m_WorldFace.m_vecPoints[3] = segment.m_vecPoints[1];

	Vector vecPoint = segment.m_WorldFace.m_vecPoints[0] + ( segment.m_vecNormals[0] * pShoreline->m_ShoreData.m_flWidths[0] );
	Vector vecStart( vecPoint.x, vecPoint.y, vecPoint.z + 150.0f );
	Vector vecEnd( vecPoint.x, vecPoint.y, vecPoint.z - 150.0f );		
	Vector vecHit, vecHitNormal;
	CMapFace *pHitFace = pFace;
	if ( !pDisp->TraceLine( vecHit, vecHitNormal, vecStart, vecEnd ) )
	{
		nDispCount = m_aDispList.Count();
		intp iDisp;
		for ( iDisp = 0; iDisp < nDispCount; ++iDisp )
		{
			if ( m_aDispList[iDisp]->TraceLine( vecHit, vecHitNormal, vecStart, vecEnd ) )
			{
				pHitFace = ( CMapFace* )m_aDispList[iDisp]->GetParent();
				break;
			}
		}

		if ( iDisp == nDispCount )
		{
			pDisp->TraceLineSnapTo( vecHit, vecHitNormal, vecStart, vecEnd );
		}
	}
	segment.m_WorldFace.m_vecPoints[1] = vecHit;
	segment.m_WorldFace.m_pFaces[1] = pHitFace;
	
	vecPoint = segment.m_WorldFace.m_vecPoints[3] + ( segment.m_vecNormals[1] * pShoreline->m_ShoreData.m_flWidths[0] );
	vecStart.Init( vecPoint.x, vecPoint.y, vecPoint.z + 150.0f );
	vecEnd.Init( vecPoint.x, vecPoint.y, vecPoint.z - 150.0f );
	pHitFace = pFace;
	if ( !pDisp->TraceLine( vecHit, vecHitNormal, vecStart, vecEnd ) )
	{
		nDispCount = m_aDispList.Count();
		intp iDisp;
		for ( iDisp = 0; iDisp < nDispCount; ++iDisp )
		{
			if ( m_aDispList[iDisp]->TraceLine( vecHit, vecHitNormal, vecStart, vecEnd ) )
			{
				pHitFace = ( CMapFace* )m_aDispList[iDisp]->GetParent();
				break;
			}
		}

		if ( iDisp == nDispCount )
		{
			pDisp->TraceLineSnapTo( vecHit, vecHitNormal, vecStart, vecEnd );
		}
	}
	segment.m_WorldFace.m_vecPoints[2] = vecHit;
	segment.m_WorldFace.m_pFaces[2] = pHitFace;
	
	// Water points.
	segment.m_WaterFace.m_vecPoints[0] = segment.m_vecPoints[0] + ( segment.m_vecNormals[0] * -pShoreline->m_ShoreData.m_flWidths[1] );
	segment.m_WaterFace.m_vecPoints[1] = segment.m_vecPoints[0];
	segment.m_WaterFace.m_vecPoints[2] = segment.m_vecPoints[1];
	segment.m_WaterFace.m_vecPoints[3] = segment.m_vecPoints[1] + ( segment.m_vecNormals[1] * -pShoreline->m_ShoreData.m_flWidths[1] );
	intp nWaterFaceCount = aWaterFaces.Count();
	for ( intp iWaterFace = 0; iWaterFace < nWaterFaceCount; ++iWaterFace )
	{
		CMapFace *pWaterFace = aWaterFaces.Element( iWaterFace );
		if ( pWaterFace )
		{
			for ( int iWaterPoint = 0; iWaterPoint < 4; ++iWaterPoint )
			{
				vecPoint = segment.m_WaterFace.m_vecPoints[iWaterPoint];
				vecStart.Init( vecPoint.x, vecPoint.y, vecPoint.z + 150.0f );
				vecEnd.Init( vecPoint.x, vecPoint.y, vecPoint.z - 150.0f );
				if ( pWaterFace->TraceLineInside( vecHit, vecHitNormal, vecStart, vecEnd ) )
				{
					segment.m_WaterFace.m_pFaces[iWaterPoint] = pWaterFace;
				}
			}
		}
	}

	// Water face clean up!
	int nNoFaceCount = false;
	for ( int iWaterPoint = 0; iWaterPoint < 4; ++iWaterPoint )
	{
		if ( !segment.m_WaterFace.m_pFaces[iWaterPoint] )
		{
			++nNoFaceCount;
		}
	}
	if ( ( nNoFaceCount > 0 ) && ( nNoFaceCount < 4 ) )
	{
		// Find a valid face.
		CMapFace *pWaterFace = NULL;
		for ( int iWaterPoint = 0; iWaterPoint < 4; ++iWaterPoint )
		{
			if ( segment.m_WaterFace.m_pFaces[iWaterPoint] )
			{
				pWaterFace = segment.m_WaterFace.m_pFaces[iWaterPoint];
				break;
			}
		}
	
		for ( int iWaterPoint = 0; iWaterPoint < 4; ++iWaterPoint )
		{
			if ( !segment.m_WaterFace.m_pFaces[iWaterPoint] )
			{
				segment.m_WaterFace.m_pFaces[iWaterPoint] = pWaterFace;
			}
		}
	}	

	// Center.
	segment.m_vecCenter = ( segment.m_vecPoints[0] + segment.m_vecPoints[1] ) * 0.5f;

	// Check winding.
	Vector vecEdge0, vecEdge1, vecCross;

	segment.m_WorldFace.m_bAdjWinding = false;
	VectorSubtract( segment.m_WorldFace.m_vecPoints[1], segment.m_WorldFace.m_vecPoints[0], vecEdge0 );
	VectorSubtract( segment.m_WorldFace.m_vecPoints[2], segment.m_WorldFace.m_vecPoints[0], vecEdge1 );
	VectorNormalize( vecEdge0 );
	VectorNormalize( vecEdge1 );
	CrossProduct( vecEdge1, vecEdge0, vecCross );
	if ( vecCross.z < 0.0f )
	{
		// Adjust winding.
		Vector vecTmp = segment.m_WorldFace.m_vecPoints[1];
		CMapFace *pTmpFace = segment.m_WorldFace.m_pFaces[1];
		segment.m_WorldFace.m_vecPoints[1] = segment.m_WorldFace.m_vecPoints[3];
		segment.m_WorldFace.m_pFaces[1] = segment.m_WorldFace.m_pFaces[3];
		segment.m_WorldFace.m_vecPoints[3] = vecTmp;
		segment.m_WorldFace.m_pFaces[3] = pTmpFace;
		segment.m_WorldFace.m_bAdjWinding = true;
	}

	segment.m_WaterFace.m_bAdjWinding = false;
	VectorSubtract( segment.m_WaterFace.m_vecPoints[1], segment.m_WaterFace.m_vecPoints[0], vecEdge0 );
	VectorSubtract( segment.m_WaterFace.m_vecPoints[2], segment.m_WaterFace.m_vecPoints[0], vecEdge1 );
	VectorNormalize( vecEdge0 );
	VectorNormalize( vecEdge1 );
	CrossProduct( vecEdge1, vecEdge0, vecCross );
	if ( vecCross.z < 0.0f )
	{
		// Adjust winding.
		Vector vecTmp = segment.m_WaterFace.m_vecPoints[1];
		CMapFace *pTmpFace = segment.m_WaterFace.m_pFaces[1];
		segment.m_WaterFace.m_vecPoints[1] = segment.m_WaterFace.m_vecPoints[3];
		segment.m_WaterFace.m_pFaces[1] = segment.m_WaterFace.m_pFaces[3];
		segment.m_WaterFace.m_vecPoints[3] = vecTmp;
		segment.m_WaterFace.m_pFaces[3] = pTmpFace;
		segment.m_WaterFace.m_bAdjWinding = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CDispShoreManager::TexcoordShoreline( Shoreline_t *pShoreline )
{
	intp nSegmentCount = pShoreline->m_aSegments.Count();
	if ( nSegmentCount == 0 )
		return false;

	// Conncect the shoreline segments to produce a continuous shoreline.
	if ( !ConnectShorelineSegments( pShoreline ) )
		return false;

	ShorelineLength( pShoreline );

	float flLengthToSegment = 0.0f;
	for ( intp iSortSegment : pShoreline->m_aSortedSegments )
	{
		GenerateTexCoord( pShoreline, iSortSegment, flLengthToSegment, false );

		// dimhotepus: Cache to speedup.
		const auto &segment = pShoreline->m_aSegments[iSortSegment];

		Vector vecEdge;
		VectorSubtract( segment.m_vecPoints[1], segment.m_vecPoints[0], vecEdge );
		flLengthToSegment += vecEdge.Length();

		GenerateTexCoord( pShoreline, iSortSegment, flLengthToSegment, true );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CDispShoreManager::ConnectShorelineSegments( Shoreline_t *pShoreline )
{
	// Reset/recreate the shoreline sorted segment list.
	pShoreline->m_aSortedSegments.Purge();

	intp iSegment = FindShorelineStart( pShoreline );
	if ( iSegment == -1 )
	{
		iSegment = 0;
	}

	intp nSegmentCount = pShoreline->m_aSegments.Count();
	while ( iSegment != -1 )
	{
		intp iSegment2;
		for ( iSegment2 = 0; iSegment2 < nSegmentCount; ++iSegment2 )
		{
			if ( iSegment2 == iSegment )
				continue;
			
			// dimhotepus: Cache segments.
			const auto &segment1 = pShoreline->m_aSegments[iSegment];
			auto &segment2 = pShoreline->m_aSegments[iSegment2];

			const bool bIsTouching0 = 
				VectorsAreEqual( segment1.m_vecPoints[0], segment2.m_vecPoints[0], DISPSHORE_VECTOR_EPS ) ||
				VectorsAreEqual( segment1.m_vecPoints[1], segment2.m_vecPoints[0], DISPSHORE_VECTOR_EPS );

			const bool bIsTouching1 = 
				VectorsAreEqual( segment1.m_vecPoints[0], segment2.m_vecPoints[1], DISPSHORE_VECTOR_EPS ) ||
				VectorsAreEqual( segment1.m_vecPoints[1], segment2.m_vecPoints[1], DISPSHORE_VECTOR_EPS );

			if ( ( bIsTouching0 || bIsTouching1 ) && !IsTouched( pShoreline, iSegment2 ) )
			{
				segment2.m_iStartPoint = 0;
				if ( bIsTouching1 )
				{
					segment2.m_iStartPoint = 1;
				}

				pShoreline->m_aSortedSegments.AddToTail( iSegment2 );
				segment2.m_bTouch = true;
				break;
			}
		}

		if ( iSegment2 != nSegmentCount )
		{
			iSegment = iSegment2;
		}
		else
		{
			iSegment = -1;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
intp CDispShoreManager::FindShorelineStart( Shoreline_t *pShoreline )
{
	// Find a segment that doesn't have any (fewest) matching point data.
	intp nSegmentCount = pShoreline->m_aSegments.Count();
	for ( intp iSegment = 0; iSegment < nSegmentCount; ++iSegment )
	{
		// dimhotepus: Cache segments.
		auto &segment1 = pShoreline->m_aSegments[iSegment];

		intp nTouchCount = 0;
		unsigned int iStartPoint = std::numeric_limits<unsigned int>::max();
		for ( intp iSegment2 = 0; iSegment2 < nSegmentCount; ++iSegment2 )
		{
			if ( iSegment == iSegment2 )
				continue;
		
			// dimhotepus: Cache segments.
			auto &segment2 = pShoreline->m_aSegments[iSegment2];

			if ( VectorsAreEqual( segment1.m_vecPoints[0], segment2.m_vecPoints[0], DISPSHORE_VECTOR_EPS ) ) 
			{ 
				++nTouchCount; 
				iStartPoint = 1;
			}

			if ( VectorsAreEqual( segment1.m_vecPoints[0], segment2.m_vecPoints[1], DISPSHORE_VECTOR_EPS ) ) 
			{ 
				++nTouchCount;
				iStartPoint = 1;
			}

			if ( VectorsAreEqual( segment1.m_vecPoints[1], segment2.m_vecPoints[0], DISPSHORE_VECTOR_EPS ) ) 
			{ 
				++nTouchCount;
				iStartPoint = 0;
			}

			if ( VectorsAreEqual( segment1.m_vecPoints[1], segment2.m_vecPoints[1], DISPSHORE_VECTOR_EPS ) ) 
			{ 
				++nTouchCount; 
				iStartPoint = 0;
			}
		}

		if ( nTouchCount == 1 )
		{
			segment1.m_iStartPoint = iStartPoint;
			pShoreline->m_aSortedSegments.AddToTail( iSegment );
			segment1.m_bTouch = true;
			return iSegment;
		}
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::ShorelineLength( Shoreline_t *pShoreline )
{
	float flLength = 0.0f;
	for ( const auto &segment : pShoreline->m_aSegments )
	{
		Vector vecEdge;
		VectorSubtract( segment.m_vecPoints[1], segment.m_vecPoints[0], vecEdge );

		flLength += vecEdge.Length();
	}

	pShoreline->m_flLength = flLength;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::GenerateTexCoord( Shoreline_t *pShoreline, intp iSegment, float flLengthToSegment, bool bEnd )
{
	float flValue = pShoreline->m_ShoreData.m_vecLengthTexcoord[1] - pShoreline->m_ShoreData.m_vecLengthTexcoord[0];
	// dimhotepus: Cache to speedup.
	auto &segment = pShoreline->m_aSegments[iSegment];

	if ( segment.m_iStartPoint == 0 )
	{
		if ( !bEnd )
		{
			float flRatio = flLengthToSegment / pShoreline->m_flLength;
			segment.m_WorldFace.m_vecTexCoords[0].x = flValue * flRatio;
			segment.m_WaterFace.m_vecTexCoords[0].x = flValue * flRatio;
			segment.m_WorldFace.m_vecTexCoords[0].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			segment.m_WaterFace.m_vecTexCoords[0].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[0];
			
			if ( segment.m_WorldFace.m_bAdjWinding )
			{
				segment.m_WorldFace.m_vecTexCoords[3].x = flValue * flRatio;
				segment.m_WorldFace.m_vecTexCoords[3].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1];
			}
			else
			{
				segment.m_WorldFace.m_vecTexCoords[1].x = flValue * flRatio;
				segment.m_WorldFace.m_vecTexCoords[1].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1];
			}

			if ( segment.m_WaterFace.m_bAdjWinding )
			{
				segment.m_WaterFace.m_vecTexCoords[3].x = flValue * flRatio;
				segment.m_WaterFace.m_vecTexCoords[3].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			}
			else
			{
				segment.m_WaterFace.m_vecTexCoords[1].x = flValue * flRatio;
				segment.m_WaterFace.m_vecTexCoords[1].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			}
		}
		else
		{
			float flRatio = flLengthToSegment / pShoreline->m_flLength;

			segment.m_WorldFace.m_vecTexCoords[2].x = flValue * flRatio;
			segment.m_WaterFace.m_vecTexCoords[2].x = flValue * flRatio;
			segment.m_WorldFace.m_vecTexCoords[2].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1];
			segment.m_WaterFace.m_vecTexCoords[2].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			
			if ( segment.m_WorldFace.m_bAdjWinding )
			{
				flRatio = flLengthToSegment / pShoreline->m_flLength;
				segment.m_WorldFace.m_vecTexCoords[1].x = flValue * flRatio;
				segment.m_WorldFace.m_vecTexCoords[1].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			}
			else
			{
				flRatio = flLengthToSegment / pShoreline->m_flLength;
				segment.m_WorldFace.m_vecTexCoords[3].x = flValue * flRatio;
				segment.m_WorldFace.m_vecTexCoords[3].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			}

			if ( segment.m_WaterFace.m_bAdjWinding )
			{
				flRatio = flLengthToSegment / pShoreline->m_flLength;
				segment.m_WaterFace.m_vecTexCoords[1].x = flValue * flRatio;
				segment.m_WaterFace.m_vecTexCoords[1].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[0];
			}
			else
			{
				flRatio = flLengthToSegment / pShoreline->m_flLength;
				segment.m_WaterFace.m_vecTexCoords[3].x = flValue * flRatio;
				segment.m_WaterFace.m_vecTexCoords[3].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[0];
			}
		}
	}
	else
	{
		if ( !bEnd )
		{
			float flRatio = flLengthToSegment / pShoreline->m_flLength;
			segment.m_WorldFace.m_vecTexCoords[2].x = flValue * flRatio;
			segment.m_WaterFace.m_vecTexCoords[2].x = flValue * flRatio;
			segment.m_WorldFace.m_vecTexCoords[2].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1];
			segment.m_WaterFace.m_vecTexCoords[2].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			
			if ( segment.m_WorldFace.m_bAdjWinding )
			{
				flRatio = flLengthToSegment / pShoreline->m_flLength;
				segment.m_WorldFace.m_vecTexCoords[1].x = flValue * flRatio;
				segment.m_WorldFace.m_vecTexCoords[1].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			}
			else
			{
				flRatio = flLengthToSegment / pShoreline->m_flLength;
				segment.m_WorldFace.m_vecTexCoords[3].x = flValue * flRatio;
				segment.m_WorldFace.m_vecTexCoords[3].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			}

			if ( segment.m_WaterFace.m_bAdjWinding )
			{
				flRatio = flLengthToSegment / pShoreline->m_flLength;
				segment.m_WaterFace.m_vecTexCoords[1].x = flValue * flRatio;
				segment.m_WaterFace.m_vecTexCoords[1].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[0];
			}
			else
			{
				flRatio = flLengthToSegment / pShoreline->m_flLength;
				segment.m_WaterFace.m_vecTexCoords[3].x = flValue * flRatio;
				segment.m_WaterFace.m_vecTexCoords[3].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[0];
			}
		}
		else
		{
			float flRatio = flLengthToSegment / pShoreline->m_flLength;
			segment.m_WorldFace.m_vecTexCoords[0].x = flValue * flRatio;
			segment.m_WaterFace.m_vecTexCoords[0].x = flValue * flRatio;
			segment.m_WorldFace.m_vecTexCoords[0].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			segment.m_WaterFace.m_vecTexCoords[0].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[0];
			
			if ( segment.m_WorldFace.m_bAdjWinding )
			{
				segment.m_WorldFace.m_vecTexCoords[3].x = flValue * flRatio;
				segment.m_WorldFace.m_vecTexCoords[3].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1];
			}
			else
			{
				segment.m_WorldFace.m_vecTexCoords[1].x = flValue * flRatio;
				segment.m_WorldFace.m_vecTexCoords[1].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1];
			}

			if ( segment.m_WaterFace.m_bAdjWinding )
			{
				segment.m_WaterFace.m_vecTexCoords[3].x = flValue * flRatio;
				segment.m_WaterFace.m_vecTexCoords[3].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			}
			else
			{
				segment.m_WaterFace.m_vecTexCoords[1].x = flValue * flRatio;
				segment.m_WaterFace.m_vecTexCoords[1].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::BuildShorelineOverlays( Shoreline_t *pShoreline )
{	
	// Reset the list.
	if ( pShoreline->m_aOverlays.Count() != 0 )
	{
		pShoreline->m_aOverlays.Purge();
	}

	intp nSegmentCount = pShoreline->m_aSegments.Count();
	if ( nSegmentCount == 0 )
		return;

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc )
		return;
	
	for ( intp iSegment = 0; iSegment < nSegmentCount; ++iSegment )
	{
		CMapDisp *pDisp = EditDispMgr()->GetDisp( pShoreline->m_aSegments[iSegment].m_hDisp );
		if ( !pDisp )
			continue;

		CMapFace *pFace = ( CMapFace* )pDisp->GetParent();
		if ( !pFace )
			continue;

		CreateOverlays( pShoreline, iSegment );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::CreateOverlays( Shoreline_t *pShoreline, intp iSegment )
{
	// Create the face list than this overlay will act upon.
	CUtlVector<CMapFace*> aWorldFaces;
	CUtlVector<CMapFace*> aWaterFaces;

	// dimhotepus: Cache to speedup.
	auto &segment = pShoreline->m_aSegments[iSegment];

	for ( int iFace = 0; iFace < 4; ++iFace )
	{
		if ( !segment.m_WorldFace.m_pFaces[iFace] ||
			 !segment.m_WaterFace.m_pFaces[iFace] )
			return;

		// World
		if ( aWorldFaces.Find( segment.m_WorldFace.m_pFaces[iFace] ) == -1 )
		{
			aWorldFaces.AddToTail( segment.m_WorldFace.m_pFaces[iFace] );
		}

		// Water
		if ( aWaterFaces.Find( segment.m_WaterFace.m_pFaces[iFace] ) == -1 )
		{
			aWaterFaces.AddToTail( segment.m_WaterFace.m_pFaces[iFace] );
		}
	}

	// Create and add data to the world overlay.
	intp iOverlay = pShoreline->m_aOverlays.AddToTail();
	CMapOverlay *pOverlay = &pShoreline->m_aOverlays[iOverlay];

	pOverlay->SetOverlayType( OVERLAY_TYPE_SHORE );

	pOverlay->Basis_Init( aWorldFaces[0] );
	pOverlay->Handles_Init( aWorldFaces[0] );
	pOverlay->SideList_Init( aWorldFaces[0] );

	intp nFaceCount = aWorldFaces.Count();
	for ( int iFace = 1; iFace < nFaceCount; ++iFace )
	{
		pOverlay->SideList_AddFace( aWorldFaces[iFace] );
	}

	pOverlay->SetLoaded( true );

	pOverlay->HandleMoveTo( 0, segment.m_WorldFace.m_vecPoints[0], segment.m_WorldFace.m_pFaces[0] );
	pOverlay->HandleMoveTo( 1, segment.m_WorldFace.m_vecPoints[1], segment.m_WorldFace.m_pFaces[1] );
	pOverlay->HandleMoveTo( 2, segment.m_WorldFace.m_vecPoints[2], segment.m_WorldFace.m_pFaces[2] );
	pOverlay->HandleMoveTo( 3, segment.m_WorldFace.m_vecPoints[3], segment.m_WorldFace.m_pFaces[3] );

	if ( !pShoreline->m_ShoreData.m_pTexture )
	{
		pOverlay->SetMaterial( "decals/decal_signroute004b" );
	}
	else
	{
		pOverlay->SetMaterial( pShoreline->m_ShoreData.m_pTexture );
	}
	pOverlay->SetTexCoords( segment.m_WorldFace.m_vecTexCoords );

	pOverlay->CalcBounds( true );

	pOverlay->DoClip();
	pOverlay->PostUpdate( Notify_Changed );

	// Create and add data to the water overlay.
	iOverlay = pShoreline->m_aOverlays.AddToTail();
	pOverlay = &pShoreline->m_aOverlays[iOverlay];

	pOverlay->SetOverlayType( OVERLAY_TYPE_SHORE );

	pOverlay->Basis_Init( aWaterFaces[0] );
	pOverlay->Handles_Init( aWaterFaces[0] );
	pOverlay->SideList_Init( aWaterFaces[0] );

	nFaceCount = aWaterFaces.Count();
	for ( intp iFace = 1; iFace < nFaceCount; ++iFace )
	{
		pOverlay->SideList_AddFace( aWaterFaces[iFace] );
	}

	pOverlay->SetLoaded( true );

	pOverlay->HandleMoveTo( 0, segment.m_WaterFace.m_vecPoints[0], segment.m_WaterFace.m_pFaces[0] );
	pOverlay->HandleMoveTo( 1, segment.m_WaterFace.m_vecPoints[1], segment.m_WaterFace.m_pFaces[1] );
	pOverlay->HandleMoveTo( 2, segment.m_WaterFace.m_vecPoints[2], segment.m_WaterFace.m_pFaces[2] );
	pOverlay->HandleMoveTo( 3, segment.m_WaterFace.m_vecPoints[3], segment.m_WaterFace.m_pFaces[3] );

	if ( !pShoreline->m_ShoreData.m_pTexture )
	{
		pOverlay->SetMaterial( "decals/decal_signroute004b" );
	}
	else
	{
		pOverlay->SetMaterial( pShoreline->m_ShoreData.m_pTexture );
	}
	pOverlay->SetTexCoords( segment.m_WaterFace.m_vecTexCoords );

	pOverlay->SetOverlayType( OVERLAY_TYPE_SHORE );

	pOverlay->CalcBounds( true );

	pOverlay->DoClip();
	pOverlay->PostUpdate( Notify_Changed );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::Draw( CRender3D *pRender )
{
	for ( auto &line : m_aShorelines )
	{
		for ( auto &overlay : line.m_aOverlays )
		{
			overlay.Render3D( pRender );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::DebugDraw( CRender3D *pRender )
{
	pRender->SetRenderMode( RENDER_MODE_WIREFRAME );

	intp nShorelineCount = GetShorelineCount();
	for ( intp iShoreline = 0; iShoreline < nShorelineCount; ++iShoreline )
	{
		DrawShorelines( iShoreline );
		DrawShorelineNormals( iShoreline );
		DrawShorelineOverlayPoints( pRender, iShoreline );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::DrawShorelines( intp iShoreline )
{
	Shoreline_t *pShoreline = &m_aShorelines[iShoreline];
	if ( pShoreline )
	{
		intp nSegmentCount = pShoreline->m_aSegments.Count();
		if ( nSegmentCount == 0 )
			return;

		CMeshBuilder meshBuilder;
		CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
		IMesh* pMesh = pRenderContext->GetDynamicMesh();
		meshBuilder.Begin( pMesh, MATERIAL_LINES, ( nSegmentCount * 2 ) );

		for ( intp iSegment = 0; iSegment < nSegmentCount; ++iSegment )
		{
			// dimhotepus: Cache to speedup.
			const auto &segment = pShoreline->m_aSegments[iSegment];

			meshBuilder.Color3f( 1.0f, 0.0f, 0.0f );
			meshBuilder.Position3f( segment.m_vecPoints[0].x, 
				                    segment.m_vecPoints[0].y, 
				                    segment.m_vecPoints[0].z + 50.0f );
			meshBuilder.AdvanceVertex();
			
			meshBuilder.Color3f( 1.0f, 0.0f, 0.0f );
			meshBuilder.Position3f( segment.m_vecPoints[1].x, 
				                    segment.m_vecPoints[1].y, 
				                    segment.m_vecPoints[1].z + 50.0f );
			meshBuilder.AdvanceVertex();
		}

		meshBuilder.End();
		pMesh->Draw();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::DrawShorelineNormals( intp iShoreline )
{
#define DISPSHORE_NORMAL_SCALE 25.0f

	Shoreline_t *pShoreline = &m_aShorelines[iShoreline];
	if ( pShoreline )
	{
		intp nSegmentCount = pShoreline->m_aSegments.Count();
		if ( nSegmentCount == 0 )
			return;

		CMeshBuilder meshBuilder;
		CMatRenderContextPtr pRenderContext( materials );
		IMesh* pMesh = pRenderContext->GetDynamicMesh();
		meshBuilder.Begin( pMesh, MATERIAL_LINES, ( nSegmentCount * 4 ) );

		for ( intp iSegment = 0; iSegment < nSegmentCount; ++iSegment )
		{
			// dimhotepus: Cache segments.
			const auto &segment = pShoreline->m_aSegments[iSegment];

			// Normal for vertex 0.
			meshBuilder.Color3f( 1.0f, 1.0f, 0.0f );
			meshBuilder.Position3f( segment.m_vecPoints[0].x, 
				                    segment.m_vecPoints[0].y, 
				                    segment.m_vecPoints[0].z + 50.0f );
			meshBuilder.AdvanceVertex();
			
			meshBuilder.Color3f( 1.0f, 1.0f, 0.0f );
			meshBuilder.Position3f( segment.m_vecPoints[0].x + ( segment.m_vecNormals[0].x * DISPSHORE_NORMAL_SCALE ), 
				                    segment.m_vecPoints[0].y + ( segment.m_vecNormals[0].y * DISPSHORE_NORMAL_SCALE ), 
				                    segment.m_vecPoints[0].z + 50.0f + ( segment.m_vecNormals[0].z * DISPSHORE_NORMAL_SCALE ) );
			meshBuilder.AdvanceVertex();
			
			// Normal for vertex 1.
			meshBuilder.Color3f( 1.0f, 1.0f, 0.0f );
			meshBuilder.Position3f( segment.m_vecPoints[1].x, 
				                    segment.m_vecPoints[1].y, 
				                    segment.m_vecPoints[1].z + 50.0f );
			meshBuilder.AdvanceVertex();
			
			meshBuilder.Color3f( 1.0f, 1.0f, 0.0f );
			meshBuilder.Position3f( segment.m_vecPoints[1].x + ( segment.m_vecNormals[1].x * DISPSHORE_NORMAL_SCALE ), 
				                    segment.m_vecPoints[1].y + ( segment.m_vecNormals[1].y * DISPSHORE_NORMAL_SCALE ), 
				                    segment.m_vecPoints[1].z + 50.0f + ( segment.m_vecNormals[1].z * DISPSHORE_NORMAL_SCALE ) );
			meshBuilder.AdvanceVertex();
		}

		meshBuilder.End();
		pMesh->Draw();
	}

#undef DISPSHORE_NORMAL_SCALE
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::DrawShorelineOverlayPoints( CRender3D *pRender, intp iShoreline )
{
#define DISPSHORE_BOX_SIZE	5.0f

	Shoreline_t *pShoreline = &m_aShorelines[iShoreline];
	if ( pShoreline )
	{
		intp nSegmentCount = pShoreline->m_aSegments.Count();
		if ( nSegmentCount == 0 )
			return;
		
		Vector vecWorldMin, vecWorldMax;
		for ( intp iSegment = 0; iSegment < nSegmentCount; ++iSegment )
		{
			// dimhotepus: Cache world face.
			const auto &worldFace = pShoreline->m_aSegments[iSegment].m_WorldFace;

			for ( int iWorldPoint = 0; iWorldPoint < 4; ++iWorldPoint )
			{
				vecWorldMin = worldFace.m_vecPoints[iWorldPoint];
				vecWorldMax = worldFace.m_vecPoints[iWorldPoint];
				for ( int iAxis = 0; iAxis < 3; ++iAxis )
				{
					vecWorldMin[iAxis] -= DISPSHORE_BOX_SIZE;
					vecWorldMax[iAxis] += DISPSHORE_BOX_SIZE;
				}
				
				pRender->RenderBox( vecWorldMin, vecWorldMax, 255, 0, 0, SELECT_NONE );
			}

			// dimhotepus: Cache water face.
			const auto &waterFace = pShoreline->m_aSegments[iSegment].m_WaterFace;
			
			for ( int iWorldPoint = 0; iWorldPoint < 4; ++iWorldPoint )
			{
				vecWorldMin = waterFace.m_vecPoints[iWorldPoint];
				vecWorldMax = waterFace.m_vecPoints[iWorldPoint];
				for ( int iAxis = 0; iAxis < 3; ++iAxis )
				{
					vecWorldMin[iAxis] -= DISPSHORE_BOX_SIZE;
					vecWorldMax[iAxis] += DISPSHORE_BOX_SIZE;
				}
				
				pRender->RenderBox( vecWorldMin, vecWorldMax, 0, 0, 255, SELECT_NONE );
			}
		}
	}

#undef DISPSHORE_BOX_SIZE
}

