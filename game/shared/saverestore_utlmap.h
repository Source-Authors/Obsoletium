//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef SAVERESTORE_UTLMAP_H
#define SAVERESTORE_UTLMAP_H

#include "utlmap.h"
#include "saverestore_utlrbtree.h"

#if defined( _WIN32 )
#pragma once
#endif

template <class UTLMAP, fieldtype_t KEY_TYPE, fieldtype_t FIELD_TYPE>
class CUtlMapDataOps : public CDefSaveRestoreOps
{
public:
	CUtlMapDataOps()
	{
		UTLCLASS_SAVERESTORE_VALIDATE_TYPE( KEY_TYPE );
		UTLCLASS_SAVERESTORE_VALIDATE_TYPE( FIELD_TYPE );
	}

	void Save( const SaveRestoreFieldInfo_t &fieldInfo, ISave *pSave ) override
	{
		datamap_t *pKeyDatamap = CTypedescDeducer<KEY_TYPE>::Deduce( (UTLMAP *)nullptr );
		datamap_t *pFieldDatamap = CTypedescDeducer<FIELD_TYPE>::Deduce( (UTLMAP *)nullptr );
		typedescription_t dataDesc[] = 
		{
			{
				KEY_TYPE, 
				"K", 
				{ 0, 0 },
				1, 
				FTYPEDESC_SAVE, 
				NULL, 
				NULL, 
				NULL,
				pKeyDatamap,
				CDatamapFieldSizeDeducer<KEY_TYPE>::FieldSize(),
				nullptr,
				0,
				0.0f
			},
			
			{
				FIELD_TYPE, 
				"T", 
				{ offsetof(typename UTLMAP::Node_t, elem), 0 },
				1, 
				FTYPEDESC_SAVE, 
				NULL, 
				NULL, 
				NULL,
				pFieldDatamap,
				CDatamapFieldSizeDeducer<FIELD_TYPE>::FieldSize(),
				nullptr,
				0,
				0.0f
			}
		};
		
		datamap_t dataMap = 
		{
			dataDesc,
			2,
			"um",
			NULL,
			false,
			false,
			0,
#ifdef _DEBUG
			true
#endif
		};

		auto *pUtlRBTree = ((UTLMAP *)fieldInfo.pField)->AccessTree();

		pSave->StartBlock();
		
		int nElems = pUtlRBTree->Count();
		pSave->WriteInt( &nElems, 1 );

		auto i = pUtlRBTree->FirstInorder();
		while ( i != pUtlRBTree->InvalidIndex() )
		{
			auto &elem = pUtlRBTree->Element( i );

			pSave->WriteAll( &elem, &dataMap );

			i = pUtlRBTree->NextInorder( i );
		}
		pSave->EndBlock();
	}
	
	void Restore( const SaveRestoreFieldInfo_t &fieldInfo, IRestore *pRestore ) override
	{
		datamap_t *pKeyDatamap = CTypedescDeducer<KEY_TYPE>::Deduce( (UTLMAP *)nullptr );
		datamap_t *pFieldDatamap = CTypedescDeducer<FIELD_TYPE>::Deduce( (UTLMAP *)nullptr );
		typedescription_t dataDesc[] = 
		{
			{
				KEY_TYPE, 
				"K", 
				{ 0, 0 },
				1, 
				FTYPEDESC_SAVE, 
				NULL, 
				NULL, 
				NULL,
				pKeyDatamap,
				CDatamapFieldSizeDeducer<KEY_TYPE>::FieldSize(),
				nullptr,
				0,
				0.0f
			},
			
			{
				FIELD_TYPE, 
				"T", 
				{ offsetof(typename UTLMAP::Node_t, elem), 0 },
				1, 
				FTYPEDESC_SAVE, 
				NULL, 
				NULL, 
				NULL,
				pFieldDatamap,
				CDatamapFieldSizeDeducer<FIELD_TYPE>::FieldSize(),
				nullptr,
				0,
				0.0f
			}
		};
		
		datamap_t dataMap = 
		{
			dataDesc,
			2,
			"um",
			NULL,
			false,
			false,
			0,
#ifdef _DEBUG
			true
#endif
		};

		auto *pUtlMap = ((UTLMAP *)fieldInfo.pField);

		pRestore->StartBlock();

		int nElems = pRestore->ReadInt();
		typename UTLMAP::CTree::ElemType_t temp;

		while ( nElems-- )
		{
			pRestore->ReadAll( &temp, &dataMap );
			pUtlMap->Insert( temp.key, temp.elem );
		}
		
		pRestore->EndBlock();
	}
	
	void MakeEmpty( const SaveRestoreFieldInfo_t &fieldInfo ) override
	{
		auto *pUtlMap = (UTLMAP *)fieldInfo.pField;
		pUtlMap->RemoveAll();
	}

	bool IsEmpty( const SaveRestoreFieldInfo_t &fieldInfo ) override
	{
		auto *pUtlMap = (UTLMAP *)fieldInfo.pField;
		return ( pUtlMap->Count() == 0 );
	}
	
};

//-------------------------------------

template <fieldtype_t KEYTYPE, fieldtype_t FIELD_TYPE>
class CUtlMapDataopsInstantiator
{
public:
	template <class UTLMAP>
	static ISaveRestoreOps *GetDataOps(UTLMAP *)
	{
		static CUtlMapDataOps<UTLMAP, KEYTYPE, FIELD_TYPE> ops;
		return &ops;
	}
};

//-------------------------------------

#define SaveUtlMap( pSave, pUtlMap, fieldtype) \
	CUtlMapDataopsInstantiator<fieldtype>::GetDataOps( pUtlMap )->Save( pUtlMap, pSave );

#define RestoreUtlMap( pRestore, pUtlMap, fieldtype) \
	CUtlMapDataopsInstantiator<fieldtype>::GetDataOps( pUtlMap )->Restore( pUtlMap, pRestore );

//-------------------------------------

#define DEFINE_UTLMAP(name,keyType,fieldtype) \
	{ FIELD_CUSTOM, #name, { offsetof(classNameTypedef,name), 0 }, 1, FTYPEDESC_SAVE, NULL, CUtlMapDataopsInstantiator<keyType, fieldtype>::GetDataOps(&(((classNameTypedef *)0)->name)), nullptr, nullptr, 0, nullptr, 0, 0.0f }


#endif // SAVERESTORE_UTLMAP_H
