//===== Copyright � 1996-2011, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef COMMON_VSCRIPT_UTILS_H
#define COMMON_VSCRIPT_UTILS_H

#include "vscript/ivscript.h"
#include "tier1/KeyValues.h"
#include "tier1/fmtstr.h"

#define KV_VARIANT_SCRATCH_BUF_SIZE 128

inline KeyValues * ScriptTableToKeyValues( IScriptVM *pVM, char const *szName, HSCRIPT hTable );
inline HSCRIPT ScriptTableFromKeyValues( IScriptVM *pVM, KeyValues *kv, HSCRIPT hTable = INVALID_HSCRIPT );

inline bool ScriptVmKeyValueToVariant( IScriptVM *pVM, KeyValues *val, ScriptVariant_t &varValue, char (&chScratchBuffer)[KV_VARIANT_SCRATCH_BUF_SIZE] )
{
	switch ( val->GetDataType() )
	{
	case KeyValues::TYPE_STRING:
		varValue = val->GetString();
		return true;
	case KeyValues::TYPE_INT:
		varValue = val->GetInt();
		return true;
	case KeyValues::TYPE_FLOAT:
		varValue = val->GetFloat();
		return true;
	case KeyValues::TYPE_UINT64:
		V_to_chars( chScratchBuffer, val->GetUint64() );
		varValue = chScratchBuffer;
		return true;
	case KeyValues::TYPE_NONE:
		varValue = ScriptTableFromKeyValues( pVM, val );
		return true;
	default:
		Warning( "ScriptVmKeyValueToVariant failed to package parameter %s (type %hhu)\n",
			val->GetName(), static_cast<unsigned char>(val->GetDataType()) );
		return false;
	}
}

inline bool ScriptVmStringFromVariant( ScriptVariant_t &varValue, char (&chScratchBuffer)[KV_VARIANT_SCRATCH_BUF_SIZE] )
{
	switch ( varValue.GetType() )
	{
	case FIELD_INTEGER:
		V_to_chars( chScratchBuffer, ( int ) varValue );
		return true;
	case FIELD_FLOAT:
		V_to_chars( chScratchBuffer, ( float ) varValue );
		return true;
	case FIELD_BOOLEAN:
		V_to_chars( chScratchBuffer, ( int ) ( bool ) varValue );
		return true;
	case FIELD_CHARACTER:
		V_sprintf_safe( chScratchBuffer, "%c", ( char ) varValue );
		return true;
	case FIELD_CSTRING:
		V_sprintf_safe( chScratchBuffer, "%s", ( const char * ) varValue );
		return true;
	default:
		Warning( "ScriptVmStringFromVariant failed to unpack parameter variant type %d\n", varValue.GetType() );
		return false;
	}
}

inline KeyValues * ScriptVmKeyValueFromVariant( IScriptVM *pVM, ScriptVariant_t &varValue )
{
	KeyValues *val = nullptr;

	switch ( varValue.GetType() )
	{
	case FIELD_INTEGER:
		val = new KeyValues( "" );
		val->SetInt( nullptr, ( int ) varValue );
		return val;
	case FIELD_FLOAT:
		val = new KeyValues( "" );
		val->SetFloat( nullptr, ( float ) varValue );
		return val;
	case FIELD_BOOLEAN:
		val = new KeyValues( "" );
		val->SetInt( nullptr, ( ( bool ) varValue ) ? 1 : 0 );
		return val;
	case FIELD_CHARACTER:
		val = new KeyValues( "" );
		val->SetString( nullptr, CFmtStr( "%c", ( char ) varValue ) );
		return val;
	case FIELD_CSTRING:
		val = new KeyValues( "" );
		val->SetString( nullptr, ( char const * ) varValue );
		return val;
	case FIELD_HSCRIPT:
		return ScriptTableToKeyValues( pVM, "", ( HSCRIPT ) varValue );
	default:
		Warning( "ScriptVmKeyValueFromVariant failed to unpack parameter variant type %d\n", varValue.GetType() );
		return nullptr;
	}
}

inline KeyValues * ScriptTableToKeyValues( IScriptVM *pVM, char const *szName, HSCRIPT hTable )
{
	if ( !szName )
		szName = "";
	
	auto *kv = new KeyValues( szName );

	if ( hTable && pVM )
	{
		ScriptVariant_t varKey, varValue;
		for ( int nIterator = 0;
			( nIterator = pVM->GetKeyValue( hTable, nIterator, &varKey, &varValue ) ) != -1;
			pVM->ReleaseValue( varKey ), pVM->ReleaseValue( varValue ) )
		{
			char chScratchBuffer[ KV_VARIANT_SCRATCH_BUF_SIZE ] = {0};
			ScriptVmStringFromVariant( varKey, chScratchBuffer );

			if ( !chScratchBuffer[0] )
			{
				Assert( 0 );
				continue;
			}

			KeyValues *sub = ScriptVmKeyValueFromVariant( pVM, varValue );
			if ( !sub )
			{
				Assert( 0 );
				// sub->deleteThis();
				// continue;
				// still proceed - it will be a key with no data
				sub = new KeyValues( "" );
			}
			sub->SetName( chScratchBuffer );
			
			kv->AddSubKey( sub );
		}
	}

	return kv;
}

inline HSCRIPT ScriptTableFromKeyValues( IScriptVM *pVM, KeyValues *kv, HSCRIPT hTable )
{
	if ( !kv || !pVM )
		return nullptr;

	if ( hTable == INVALID_HSCRIPT )
	{
		ScriptVariant_t varTable;
		pVM->CreateTable( varTable );
		hTable = varTable;
	}

	for ( auto *val = kv->GetFirstSubKey(); val; val = val->GetNextKey() )
	{
		ScriptVariant_t varValue;
		char chScratchBuffer[ KV_VARIANT_SCRATCH_BUF_SIZE ];
		if ( !ScriptVmKeyValueToVariant( pVM, val, varValue, chScratchBuffer ) )
			continue;

#ifdef SCRIPTTABLE_SCRIPT_LOWERCASE_ALL_TABLE_KEYS
		char chNameBuffer[ KV_VARIANT_SCRATCH_BUF_SIZE ];
		V_strcpy_safe( chNameBuffer, val->GetName() );
		V_strlower( chNameBuffer );
#else
		char const *chNameBuffer = val->GetName();
#endif

		pVM->SetValue( hTable, chNameBuffer, varValue );
	}

	return hTable;
}



#endif // COMMON_VSCRIPT_UTILS_H
