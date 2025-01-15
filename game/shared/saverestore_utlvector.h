//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef SAVERESTORE_UTLVECTOR_H
#define SAVERESTORE_UTLVECTOR_H

#include "utlvector.h"
#include "isaverestore.h"
#include "saverestore_utlclass.h"

#if defined( _WIN32 )
#pragma once
#endif

//-------------------------------------

template <class UTLVECTOR, fieldtype_t FIELD_TYPE>
class CUtlVectorDataOps : public CDefSaveRestoreOps
{
public:
	CUtlVectorDataOps()
	{
		UTLCLASS_SAVERESTORE_VALIDATE_TYPE( FIELD_TYPE );
	}

	void Save( const SaveRestoreFieldInfo_t &fieldInfo, ISave *pSave ) override
	{
		datamap_t *pArrayTypeDatamap = CTypedescDeducer<FIELD_TYPE>::Deduce( (UTLVECTOR *)nullptr );
		typedescription_t dataDesc = 
		{
			FIELD_TYPE, 
			"elems", 
			{ 0, 0 },
			1, 
			FTYPEDESC_SAVE, 
			NULL, 
			NULL, 
			NULL,
			pArrayTypeDatamap,
			-1,
			nullptr,
			0,
			0.0f
		};
		
		datamap_t dataMap = 
		{
			&dataDesc,
			1,
			"uv",
			NULL,
			false,
			false,
			0,
#ifdef _DEBUG
			true
#endif
		};
		
		auto *pUtlVector = (UTLVECTOR *)fieldInfo.pField;
		Assert(pUtlVector->Count() <= std::numeric_limits<int>::max());
		int nElems = static_cast<int>(pUtlVector->Count());
		
		pSave->WriteInt( &nElems, 1 );
		if ( pArrayTypeDatamap == NULL )
		{
			if ( nElems )
			{
				// dimhotepus: Check do not truncate vector during save.
				AssertMsg( nElems <= std::numeric_limits<decltype(dataDesc.fieldSize)>::max(),
					"Trying to save vector of %d elements while max allowed is %zd.\n",
					nElems, static_cast<intp>(std::numeric_limits<decltype(dataDesc.fieldSize)>::max()) );

				dataDesc.fieldSize = static_cast<decltype(dataDesc.fieldSize)>(nElems);
				dataDesc.fieldSizeInBytes = nElems * CDatamapFieldSizeDeducer<FIELD_TYPE>::FieldSize();
				pSave->WriteFields("elems", &((*pUtlVector)[0]), &dataMap, &dataDesc, 1 );
			}
		}
		else
		{
			// @Note (toml 11-21-02): Save load does not support arrays of user defined types (embedded)
			dataDesc.fieldSizeInBytes = CDatamapFieldSizeDeducer<FIELD_TYPE>::FieldSize();
			for ( int i = 0; i < nElems; i++ )
				pSave->WriteAll( &((*pUtlVector)[i]), &dataMap );
		}
	}
	
	void Restore( const SaveRestoreFieldInfo_t &fieldInfo, IRestore *pRestore ) override
	{
		datamap_t *pArrayTypeDatamap = CTypedescDeducer<FIELD_TYPE>::Deduce( (UTLVECTOR *)nullptr );
		typedescription_t dataDesc = 
		{
			FIELD_TYPE, 
			"elems", 
			{ 0, 0 },
			1, 
			FTYPEDESC_SAVE, 
			NULL, 
			NULL, 
			NULL,
			pArrayTypeDatamap,
			-1,
			nullptr,
			0,
			0.0f
		};
		
		datamap_t dataMap = 
		{
			&dataDesc,
			1,
			"uv",
			NULL,
			false,
			false,
			0,
#ifdef _DEBUG
			true
#endif
		};
		
		auto *pUtlVector = (UTLVECTOR *)fieldInfo.pField;

		int nElems = pRestore->ReadInt();
		
		pUtlVector->SetCount( nElems );
		if ( pArrayTypeDatamap == NULL )
		{
			if ( nElems )
			{
				// dimhotepus: Check do not truncate vector during restore.
				AssertMsg( nElems <= std::numeric_limits<decltype(dataDesc.fieldSize)>::max(),
					"Trying to restore vector of %d elements while max allowed is %zd.\n",
					nElems, static_cast<intp>(std::numeric_limits<decltype(dataDesc.fieldSize)>::max()) );

				dataDesc.fieldSize = static_cast<decltype(dataDesc.fieldSize)>(nElems);
				dataDesc.fieldSizeInBytes = nElems * CDatamapFieldSizeDeducer<FIELD_TYPE>::FieldSize();
				pRestore->ReadFields("elems", &((*pUtlVector)[0]), &dataMap, &dataDesc, 1 );
			}
		}
		else
		{
			// @Note (toml 11-21-02): Save load does not support arrays of user defined types (embedded)
			dataDesc.fieldSizeInBytes = CDatamapFieldSizeDeducer<FIELD_TYPE>::FieldSize();
			for ( int i = 0; i < nElems; i++ )
				pRestore->ReadAll( &((*pUtlVector)[i]), &dataMap );
		}
	}
	
	void MakeEmpty( const SaveRestoreFieldInfo_t &fieldInfo ) override
	{
		auto *pUtlVector = (UTLVECTOR *)fieldInfo.pField;
		pUtlVector->SetCount( 0 );
	}

	bool IsEmpty( const SaveRestoreFieldInfo_t &fieldInfo ) override
	{
		auto *pUtlVector = (UTLVECTOR *)fieldInfo.pField;
		return pUtlVector->Count() == 0;
	}
	
};

//-------------------------------------

template <fieldtype_t FIELD_TYPE>
class CUtlVectorDataopsInstantiator
{
public:
	template <class UTLVECTOR>
	static ISaveRestoreOps *GetDataOps(UTLVECTOR *)
	{
		static CUtlVectorDataOps<UTLVECTOR, FIELD_TYPE> ops;
		return &ops;
	}
};

//-------------------------------------

#define SaveUtlVector( pSave, pUtlVector, fieldtype) \
	CUtlVectorDataopsInstantiator<fieldtype>::GetDataOps( pUtlVector )->Save( pUtlVector, pSave );

#define RestoreUtlVector( pRestore, pUtlVector, fieldtype) \
	CUtlVectorDataopsInstantiator<fieldtype>::GetDataOps( pUtlVector )->Restore( pUtlVector, pRestore );

//-------------------------------------

#define DEFINE_UTLVECTOR(name,fieldtype) \
	{ FIELD_CUSTOM, #name, { offsetof(classNameTypedef,name), 0 }, 1, FTYPEDESC_SAVE, NULL, CUtlVectorDataopsInstantiator<fieldtype>::GetDataOps(&(((classNameTypedef *)0)->name)), NULL, nullptr, 0, nullptr, 0, 0.0f }

#define DEFINE_GLOBAL_UTLVECTOR(name,fieldtype) \
	{ FIELD_CUSTOM, #name, { offsetof(classNameTypedef,name), 0 }, 1, FTYPEDESC_SAVE|FTYPEDESC_GLOBAL, NULL, CUtlVectorDataopsInstantiator<fieldtype>::GetDataOps(&(((classNameTypedef *)0)->name)), NULL, nullptr, 0, nullptr, 0, 0.0f }


#endif // SAVERESTORE_UTLVECTOR_H
