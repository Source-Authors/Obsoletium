//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef INCREMENTAL_H
#define INCREMENTAL_H
#ifdef _WIN32
#pragma once
#endif



#include "iincremental.h"
#include "threads.h"
#include "bspfile.h"
#include "tier0/threadtools.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utlvector.h"
#include "tier1/utlbuffer.h"


#define INCREMENTALFILE_VERSION	31241


class CIncLight;

// dimhotepus: class -> struct
struct CLightValue
{
	float m_Dot;
};


class CLightFace
{
public:
	CLightFace() : m_FaceIndex{0}, m_LightFacesIndex{0}, m_pLight{nullptr} {}

	unsigned short				m_FaceIndex;		// global face index
	unsigned short				m_LightFacesIndex;	// index into CIncLight::m_LightFaces.

	// The lightmap grid for this face. Only used while building lighting data for a face.
	// Compressed into m_CompressedData immediately afterwards.
	CUtlVector<CLightValue>		m_LightValues;

	CUtlBuffer					m_CompressedData;
	CIncLight					*m_pLight;
};




class CIncLight
{
public:
	CIncLight();
	~CIncLight();

	CIncLight(const CIncLight &) = delete;
	CIncLight& operator=(const CIncLight &) = delete;

	CLightFace*		FindOrCreateLightFace( int iFace, int lmSize, bool *bNew=NULL );


public:

	CThreadMutex	m_pCS;

	// This is the light for which m_LightFaces was built.
	dworldlight_t	m_Light;

	CLightFace		*m_pCachedFaces[MAX_TOOL_THREADS+1];

	// The list of faces that this light contributes to.
	CUtlLinkedList<CLightFace*, unsigned short>	m_LightFaces;

	// Largest value in intensity of light. Used to scale dot products up into a
	// range where their values make sense.
	float			m_flMaxIntensity;
};


class CIncrementalHeader
{
public:
	struct CLMSize
	{
		unsigned char m_Width;
		unsigned char m_Height;
	};

	CUtlVector<CLMSize>	m_FaceLightmapSizes;
};


class CIncremental : public IIncremental
{
public:
	CIncremental();
	~CIncremental();

// IIncremental overrides.
public:

	bool Init( char const *pBSPFilename, char const *pIncrementalFilename ) override;

	// Load the light definitions out of the incremental file.
	// Figure out which lights have changed.
	// Change 'activelights' to only consist of new or changed lights.
	bool PrepareForLighting() override;

	void AddLightToFace( 
		IncrementalLightID lightID, 
		int iFace, 
		int iSample,
		int lmSize,
		float dot,
		int iThread ) override;

	void FinishFace(
		IncrementalLightID lightID,
		int iFace,
		int iThread ) override;

	// For each face that was changed during the lighting process, save out
	// new data for it in the incremental file.
	bool Finalize() override;

	void GetFacesTouched( CUtlVector<unsigned char> &touched ) override;

	bool Serialize() override;

private:

	// Read/write the header from the file.
	bool				ReadIncrementalHeader( intp fp, CIncrementalHeader *pHeader );
	bool				WriteIncrementalHeader( intp fp );

	// Returns true if the incremental file is valid and we can use InitUpdate.
	bool				IsIncrementalFileValid();
	
	void				Term();

	// For each light in 'activelights', add a light to m_Lights and link them together.
	void				AddLightsForActiveLights();

	// Load and save the state.
	bool				LoadIncrementalFile();
	bool				SaveIncrementalFile();

	typedef CUtlVector<CLightFace*> CFaceLightList;
	void				LinkLightsToFaces( CUtlVector<CFaceLightList> &faceLights );


private:

	char const		*m_pIncrementalFilename;
	char const		*m_pBSPFilename;
	
	CUtlLinkedList<CIncLight*, IncrementalLightID>	m_Lights;

	// The face index is set to 1 if a face has new lighting data applied to it.
	// This is used to optimize the set of lightmaps we recomposite.
	CUtlVector<unsigned char>	m_FacesTouched;
	
	int				m_TotalMemory;

	// Set to true when one or more runs were completed successfully.
	bool			m_bSuccessfulRun;
};



#endif // INCREMENTAL_H
