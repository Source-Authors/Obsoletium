//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Serialize and Unserialize Wavefront OBJ <-> DME Data
//
//=============================================================================

#ifndef DMOBJSERIALIZER_H
#define DMOBJSERIALIZER_H

#if defined( _WIN32 )
#pragma once
#endif

#include "datamodel/idatamodel.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"
#include "tier1/UtlStringMap.h"

class CDmeMesh;
class CDmeDag;
class CDmeVertexDeltaData;
class CDmeCombinationOperator;


//-----------------------------------------------------------------------------
// Serialization class for OBJ files
//-----------------------------------------------------------------------------
class CDmObjSerializer : public IDmSerializer
{
public:
	// Inherited from IDMSerializer
	const char *GetName() const override { return "obj"; }
	const char *GetDescription() const override { return "Wavefront OBJ"; }
	bool IsBinaryFormat() const override { return false; }
	bool StoresVersionInFile() const override { return false; }
	int GetCurrentVersion() const override { return 0; } // doesn't store a version
	bool Serialize( CUtlBuffer &buf, CDmElement *pRoot ) override;
	bool Unserialize( CUtlBuffer &buf, const char *pEncodingName, int nEncodingVersion,
		const char *pSourceFormatName, int nSourceFormatVersion,
		DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot ) override;
	virtual const char *GetImportedFormat() const { return NULL; }
	virtual int GetImportedVersion() const { return 1; }

	CDmElement *ReadOBJ( const char *pFilename, CDmeMesh **ppCreatedMesh = NULL, bool bLoadAllDeltas = true, bool bAbsolute = true );

	bool WriteOBJ( const char *pFilename, CDmElement *pRoot, bool bWriteDeltas, const char *pDeltaName = NULL, bool absolute = true );

	void MeshToObj( CUtlBuffer &b, const matrix3x4_t &parentWorldMatrix, CDmeMesh *pMesh, const char *pDeltaName = NULL, bool absolute = true );

	CDmeVertexDeltaData *GetDelta( const char *pDeltaName, bool bAbsolute );

private:
	CDmElement *ReadOBJ(
		CUtlBuffer &buf,
		DmFileId_t dmFileId,
		const char *pName,
		const char *pFilename = NULL,
		CDmeMesh *pBaseMesh = NULL,
		CDmeMesh **ppCreatedMesh = NULL,
		bool bAbsolute = true );

	static intp OutputVectors( CUtlBuffer &b, const char *pPrefix, const CUtlVector< Vector > &vData, const matrix3x4_t &matrix );

	static intp OutputVectors( CUtlBuffer &b, const char *pPrefix, const CUtlVector< Vector2D > &vData );

	static void DeltaToObj( CUtlBuffer &b, const matrix3x4_t &parentWorldMatrix, CDmeMesh *pMesh, const char *pDeltaName = NULL );

	void ParseMtlLib( CUtlBuffer &buf );

	const char *FindMtlEntry( const char *pTgaName );

	// dimhotepus: Make const characterset_t.
	static bool ParseVertex( CUtlBuffer& bufParse, const characterset_t &breakSet, int &v, int &t, int &n );

	static const char *SkipSpace( const char *pBuf );

	void DagToObj( CUtlBuffer &b, const matrix3x4_t &parentWorldMatrix, CDmeDag *pDag, const char *pDeltaName = NULL, bool absolute = true );

	static void FindDeltaMeshes( CDmeDag *pDag, CUtlVector< CDmeMesh * > &meshes );

	bool LoadDependentDeltas( const char *pDeltaName );

	struct MtlInfo_t
	{
		CUtlString m_MtlName;
		CUtlString m_TgaName;
	};

	CUtlVector< MtlInfo_t > m_mtlLib;

	CUtlString m_objDirectory;

	struct DeltaInfo_t
	{
		DeltaInfo_t()
		: m_pMesh( NULL )
		, m_pComboOp(NULL)
		, m_pDeltaData( NULL )
		{}

		CUtlString m_filename;
		CDmeMesh *m_pMesh;
		CDmeCombinationOperator *m_pComboOp;
		CDmeVertexDeltaData *m_pDeltaData;
	};

	CUtlStringMap< DeltaInfo_t > m_deltas;

	int m_nPositionOffset;
	int m_nTextureOffset;
	int m_nNormalOffset;
};

#endif // DMOBJSERIALIZER_H
