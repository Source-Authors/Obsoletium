//========= Copyright Valve Corporation, All rights reserved. ============//
//
// A class representing an abstract shape (ie drawable object)
//
//=============================================================================

#ifndef DMEFACESET_H
#define DMEFACESET_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmattributevar.h"
#include "tier1/utlvector.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeMaterial;


//-----------------------------------------------------------------------------
// A class representing a face of a polygonal mesh
//-----------------------------------------------------------------------------
class CDmeFaceSet : public CDmElement
{
	DEFINE_ELEMENT( CDmeFaceSet, CDmElement );

public:
	// material accessors
	CDmeMaterial *GetMaterial();
	void SetMaterial( CDmeMaterial *pMaterial );

	// Total number of indices in the face set including the -1 end of face designators
	intp NumIndices() const;
	const int *GetIndices() const;
	intp AddIndices( intp nCount );
	void SetIndex( intp i, int nValue );
	void SetIndices( intp nFirstIndex, intp nCount, int *pIndices );
	int GetIndex( intp i ) const;

	// Returns the number of vertices in the next polygon
	intp GetNextPolygonVertexCount( intp nFirstIndex ) const;

	// Returns the number of triangulated indices total
	intp GetTriangulatedIndexCount() const;

	// Total number of indices in the face set excluding the -1 end of face designators
	intp GetIndexCount() const;

	// Removes multiple faces from the face set
	void RemoveMultiple( intp elem, intp num );

	// Returns the number of faces in total... This should be the number of -1 indices in the face set
	// Which should equal  NumIndices() - GetIndexCount() but this function accounts for
	// empty faces (which aren't counted as faces) and a missing -1 terminator at the end
	intp GetFaceCount() const;

private:
	CDmaArray< int > m_indices;
	CDmaElement< CDmeMaterial > m_material;
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline intp CDmeFaceSet::NumIndices() const
{
	return m_indices.Count();
}

inline const int *CDmeFaceSet::GetIndices() const
{
	return m_indices.Base();
}

inline int CDmeFaceSet::GetIndex( intp i ) const
{
	return m_indices[i];
}


#endif // DMEFACESET_H
