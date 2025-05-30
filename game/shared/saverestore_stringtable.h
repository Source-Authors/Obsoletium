//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef SAVERESTORE_STRINGTABLE_H
#define SAVERESTORE_STRINGTABLE_H

#if defined( _WIN32 )
#pragma once
#endif


#include "isaverestore.h"
#include "networkstringtabledefs.h"


//-------------------------------------

class CStringTableSaveRestoreOps : public CDefSaveRestoreOps
{
public:
	void Init( INetworkStringTable* pNetworkStringTable )
	{
		m_pStringTable = pNetworkStringTable;
	}

	// save data type interface
	void Save( const SaveRestoreFieldInfo_t &fieldInfo, ISave *pSave ) override
	{
		int *pStringIndex = (int *)fieldInfo.pField;
		const char *pString = m_pStringTable->GetString( *pStringIndex );
		intp nLen = V_strlen( pString ) + 1;
		Assert(nLen <= std::numeric_limits<int>::max());
		int len = static_cast<int>(nLen);
		pSave->WriteInt( &len );
		pSave->WriteString( pString );
	}
	
	void Restore( const SaveRestoreFieldInfo_t &fieldInfo, IRestore *pRestore ) override
	{
		int *pStringIndex = (int *)fieldInfo.pField;
		int nLen = pRestore->ReadInt();
		char *pTemp = (char *)stackalloc( nLen );
		pRestore->ReadString( pTemp, nLen, nLen );
		*pStringIndex = m_pStringTable->AddString( CBaseEntity::IsServer(), pTemp );
	}
	
	void MakeEmpty( const SaveRestoreFieldInfo_t &fieldInfo ) override
	{
		int *pStringIndex = (int *)fieldInfo.pField;
		*pStringIndex = INVALID_STRING_INDEX;
	}

	bool IsEmpty( const SaveRestoreFieldInfo_t &fieldInfo ) override
	{
		int *pStringIndex = (int *)fieldInfo.pField;
		return *pStringIndex == INVALID_STRING_INDEX;
	}

private:
	INetworkStringTable *m_pStringTable;
};

#endif // SAVERESTORE_STRINGTABLE_H
