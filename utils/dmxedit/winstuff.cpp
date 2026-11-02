// Copyright Valve Corporation, All rights reserved.
//
// Some junk to isolate <windows.h> from polluting everything.

#include "winlite.h"

namespace s1::dmxedit {

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool MyGetUserName( char *pszBuf, unsigned long *pBufSiz )
{
	return !!GetUserName( pszBuf, pBufSiz );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool MyGetComputerName( char *pszBuf, unsigned long *pBufSiz )
{
	return !!GetComputerName( pszBuf, pBufSiz );
}

}  // namespace s1::dmxedit