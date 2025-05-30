//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "dt_utlvector_common.h"
#include "utldict.h"

#include "tier0/memdbgon.h"

static CUtlDict<int,int> *g_STDict = 0;
static CUtlDict<int,int> *g_RTDict = 0;


char* AllocateStringHelper2( const char *pFormat, va_list marker )
{
	char str[512];
	V_vsprintf_safe( str, pFormat, marker );
	
	return V_strdup( str );
}


char* AllocateStringHelper( PRINTF_FORMAT_STRING const char *pFormat, ... )
{
	va_list marker;
	va_start( marker, pFormat );
	char *pRet = AllocateStringHelper2( pFormat, marker );
	va_end( marker );

	return pRet;
}


char* AllocateUniqueDataTableName( bool bSendTable, PRINTF_FORMAT_STRING const char *pFormat, ... )
{
	// Setup the string.
	va_list marker;
	va_start( marker, pFormat );
	char *pRet = AllocateStringHelper2( pFormat, marker );
	va_end( marker );

	// Make sure it's unique.
#ifdef _DEBUG
	// Have to allocate them here because if they're declared as straight global variables,
	// their constructors won't have been called yet by the time we get in here.
	if ( !g_STDict )
	{
		g_STDict = new CUtlDict<int,int>;
		g_RTDict = new CUtlDict<int,int>;
	}

	CUtlDict<int,int> *pDict = bSendTable ? g_STDict : g_RTDict;
	if ( pDict->Find( pRet ) != pDict->InvalidIndex() )
	{
		// If it hits this, then they have 2 utlvectors in different data tables with the same name and the
		// same size limit. The names of 
		Assert( false );
	}
	pDict->Insert( pRet, 0 );
#endif

	return pRet;
}
