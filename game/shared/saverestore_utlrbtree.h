//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef SAVERESTORE_UTLRBTREE_H
#define SAVERESTORE_UTLRBTREE_H

#include "tier1/utlrbtree.h"
#include "isaverestore.h"
#include "saverestore_utlclass.h"

#if defined( _WIN32 )
#pragma once
#endif

//-------------------------------------

template <class UTLRBTREE, fieldtype_t FIELD_TYPE>
class CUtlRBTreeDataOps : public CDefSaveRestoreOps
{
public:
	CUtlRBTreeDataOps()
	{
		UTLCLASS_SAVERESTORE_VALIDATE_TYPE( FIELD_TYPE );
	}

	void Save( const SaveRestoreFieldInfo_t &fieldInfo, ISave *pSave ) override
	{
		datamap_t *pTreeTypeDatamap = CTypedescDeducer<FIELD_TYPE>::Deduce( (UTLRBTREE *)nullptr );
		typedescription_t dataDesc = 
		{
			FIELD_TYPE, 
			"elem", 
			{ 0, 0 },
			1, 
			FTYPEDESC_SAVE, 
			NULL, 
			NULL, 
			NULL,
			pTreeTypeDatamap,
			-1,
		};
		
		datamap_t dataMap = 
		{
			&dataDesc,
			1,
			"urb",
			NULL,
			false,
			false,
			0,
#ifdef _DEBUG
			true
#endif
		};
		
		auto *pUtlRBTree = (UTLRBTREE *)fieldInfo.pField;

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
		datamap_t *pTreeTypeDatamap = CTypedescDeducer<FIELD_TYPE>::Deduce( (UTLRBTREE *)nullptr );
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
			pTreeTypeDatamap,
			-1,
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
		
		auto *pUtlRBTree = (UTLRBTREE *)fieldInfo.pField;

		pRestore->StartBlock();

		int nElems = pRestore->ReadInt();
		typename UTLRBTREE::ElemType_t temp;

		while ( nElems-- )
		{
			pRestore->ReadAll( &temp, &dataMap );
			pUtlRBTree->Insert( temp );
		}
		
		pRestore->EndBlock();
	}
	
	void MakeEmpty( const SaveRestoreFieldInfo_t &fieldInfo ) override
	{
		auto *pUtlRBTree = (UTLRBTREE *)fieldInfo.pField;
		pUtlRBTree->RemoveAll();
	}

	bool IsEmpty( const SaveRestoreFieldInfo_t &fieldInfo ) override
	{
		auto *pUtlRBTree = (UTLRBTREE *)fieldInfo.pField;
		return pUtlRBTree->Count() == 0;
	}
	
};

//-------------------------------------

template <fieldtype_t FIELD_TYPE>
class CUtlRBTreeDataopsInstantiator
{
public:
	template <class UTLRBTREE>
	static ISaveRestoreOps *GetDataOps(UTLRBTREE *)
	{
		static CUtlRBTreeDataOps<UTLRBTREE, FIELD_TYPE> ops;
		return &ops;
	}
};

//-------------------------------------

#define SaveUtlRBTree( pSave, pUtlRBTree, fieldtype) \
	CUtlRBTreeDataopsInstantiator<fieldtype>::GetDataOps( pUtlRBTree )->Save( pUtlRBTree, pSave );

#define RestoreUtlRBTree( pRestore, pUtlRBTree, fieldtype) \
	CUtlRBTreeDataopsInstantiator<fieldtype>::GetDataOps( pUtlRBTree )->Restore( pUtlRBTree, pRestore );

//-------------------------------------

#define DEFINE_UTLRBTREE(name,fieldtype) \
	{ FIELD_CUSTOM, #name, { offsetof(classNameTypedef,name), 0 }, 1, FTYPEDESC_SAVE, NULL, CUtlRBTreeDataopsInstantiator<fieldtype>::GetDataOps(&(((classNameTypedef *)0)->name)), NULL }

#endif // SAVERESTORE_UTLRBTREE_H
