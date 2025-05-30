//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SAVERESTORE_UTLSYMBOL_H
#define SAVERESTORE_UTLSYMBOL_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlsymbol.h"

class CUtlSymbolDataOps : public CDefSaveRestoreOps
{
public:
	CUtlSymbolDataOps( CUtlSymbolTable &masterTable ) : m_symbolTable(masterTable) {}

	void Save( const SaveRestoreFieldInfo_t &fieldInfo, ISave *pSave ) override
	{
		auto *sym = ((CUtlSymbol *)fieldInfo.pField);
		
		pSave->WriteString( m_symbolTable.String( *sym ) );
	}
	
	void Restore( const SaveRestoreFieldInfo_t &fieldInfo, IRestore *pRestore ) override
	{
		auto *sym = ((CUtlSymbol *)fieldInfo.pField);

		char tmp[1024];
		pRestore->ReadString( tmp, 0 );
		*sym = m_symbolTable.AddString( tmp );
	}
	
	void MakeEmpty( const SaveRestoreFieldInfo_t &fieldInfo ) override
	{
		auto *sym = ((CUtlSymbol *)fieldInfo.pField);
		*sym = UTL_INVAL_SYMBOL;
	}

	bool IsEmpty( const SaveRestoreFieldInfo_t &fieldInfo ) override
	{
		auto *sym = ((CUtlSymbol *)fieldInfo.pField);
		return (*sym).IsValid() ? false : true;
	}

private:
	CUtlSymbolTable &m_symbolTable;
	
};

#endif // SAVERESTORE_UTLSYMBOL_H
