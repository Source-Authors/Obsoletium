//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "dt_recv_decoder.h"

#include "common.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// ------------------------------------------------------------------------------------ //
// CRecvDecoder.
// ------------------------------------------------------------------------------------ //

CRecvDecoder::CRecvDecoder()
{
	m_pTable = nullptr;
	m_pClientSendTable = nullptr;
	m_pDTITable = nullptr;
}

CClientSendProp::~CClientSendProp()
{
	// dimhotepus: Own table name.
	COM_StringFree(m_pTableName);
}

void CClientSendProp::SetTableName( const char* name) 
{
	// dimhotepus: Own table name.
	COM_StringFree(m_pTableName);
	m_pTableName = COM_StringCopy(name);
}


CClientSendTable::CClientSendTable() = default;


CClientSendTable::~CClientSendTable()
{
	delete [] m_SendTable.m_pNetTableName;
	
	for ( int iProp=0; iProp < m_SendTable.m_nProps; iProp++ )
	{
		delete [] m_SendTable.m_pProps[iProp].m_pVarName;
		delete [] m_SendTable.m_pProps[iProp].m_pExcludeDTName;
		delete [] m_SendTable.m_pProps[iProp].m_pParentArrayPropName;
	}

	delete [] m_SendTable.m_pProps;
}



