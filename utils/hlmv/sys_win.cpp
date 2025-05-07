// Copyright Valve Corporation, All rights reserved.
//
// Abstract system dependent functions.

#include "sys.h"
#include "tier1/strtools.h"

#include "winlite.h"

void Sys_CopyStringToClipboard( const char *pOut )
{
	if ( !pOut || !OpenClipboard( NULL ) )
	{
		return;
	}
	// Remove the current Clipboard contents
	if( !EmptyClipboard() )
	{
		return;
	}
	HGLOBAL clipbuffer;
	char *buffer;
	EmptyClipboard();
	
	int len = Q_strlen(pOut)+1;
	clipbuffer = GlobalAlloc(GMEM_DDESHARE, len );
	buffer = (char*)GlobalLock( clipbuffer );
	Q_strncpy( buffer, pOut, len );
	GlobalUnlock( clipbuffer );

	SetClipboardData( CF_TEXT,clipbuffer );

	CloseClipboard();
}

