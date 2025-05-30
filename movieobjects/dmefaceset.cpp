//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmefaceset.h"
#include "movieobjects/dmematerial.h"
#include "tier0/dbg.h"
#include "tier1/utlbuffer.h"
#include "datamodel/dmelementfactoryhelper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeFaceSet, CDmeFaceSet );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeFaceSet::OnConstruction()
{
	m_indices.Init( this, "faces" );
	m_material.Init( this, "material" );
}

void CDmeFaceSet::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// accessors
//-----------------------------------------------------------------------------
CDmeMaterial *CDmeFaceSet::GetMaterial()
{
	return m_material.GetElement();
}

void CDmeFaceSet::SetMaterial( CDmeMaterial *pMaterial )
{
	m_material = pMaterial;
}

intp CDmeFaceSet::AddIndices( intp nCount )
{
	intp nCurrentCount = m_indices.Count();
	m_indices.EnsureCount( nCount + nCurrentCount );
	return nCurrentCount;
}

void CDmeFaceSet::SetIndices( intp nFirstIndex, intp nCount, int *pIndices )
{
	m_indices.SetMultiple( nFirstIndex, nCount, pIndices );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeFaceSet::SetIndex( intp i, int nValue )
{
	m_indices.Set( i, nValue );
}


//-----------------------------------------------------------------------------
// Returns the number of triangulated indices
//-----------------------------------------------------------------------------
intp CDmeFaceSet::GetNextPolygonVertexCount( intp nFirstIndex ) const
{
	intp nCurrIndex = nFirstIndex;
	intp nTotalCount = m_indices.Count();
	while( nCurrIndex < nTotalCount )
	{
		if ( m_indices[nCurrIndex] == -1 )
			break;
		++nCurrIndex;
	}

	return nCurrIndex - nFirstIndex;
}


//-----------------------------------------------------------------------------
// Returns the number of triangulated indices total
//-----------------------------------------------------------------------------
intp CDmeFaceSet::GetTriangulatedIndexCount() const
{
	intp nIndexCount = 0;
	intp nVertexCount = 0;
	intp nTotalCount = m_indices.Count();
	for ( intp nCurrIndex = 0; nCurrIndex < nTotalCount; ++nCurrIndex )
	{
		if ( m_indices[nCurrIndex] == -1 )
		{
			if ( nVertexCount >= 3 )
			{
				nIndexCount += ( nVertexCount - 2 ) * 3;
			}
			nVertexCount = 0;
			continue;
		}

		++nVertexCount;
	}

	if ( nVertexCount >= 3 )
	{
		nIndexCount += ( nVertexCount - 2 ) * 3;
	}

	return nIndexCount;
}


//-----------------------------------------------------------------------------
// Returns the number of indices total
//-----------------------------------------------------------------------------
intp CDmeFaceSet::GetIndexCount() const
{
	intp nIndexCount = 0;
	intp nVertexCount = 0;
	intp nTotalCount = m_indices.Count();

	for ( intp nCurrIndex = 0; nCurrIndex < nTotalCount; ++nCurrIndex )
	{
		if ( m_indices[nCurrIndex] == -1 )
		{
			nIndexCount += nVertexCount;
			nVertexCount = 0;
			continue;
		}

		++nVertexCount;
	}

	nIndexCount += nVertexCount;

	return nIndexCount;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeFaceSet::RemoveMultiple( intp elem, intp num )
{
	m_indices.RemoveMultiple( elem, num );
}


//-----------------------------------------------------------------------------
// Returns the number of faces in the face set
//-----------------------------------------------------------------------------
intp CDmeFaceSet::GetFaceCount() const
{
	intp nFaceCount = 0;
	intp nVertexCount = 0;

	const intp nIndexCount = NumIndices();
	for ( intp i = 0; i < nIndexCount; ++i )
	{
		if ( GetIndex( i ) < 0 )
		{
			if ( nVertexCount > 0 )
			{
				++nFaceCount;
			}
			nVertexCount = 0;
			continue;
		}

		++nVertexCount;
	}

	if ( nVertexCount > 0 )
	{
		++nFaceCount;
	}

	return nFaceCount;
}