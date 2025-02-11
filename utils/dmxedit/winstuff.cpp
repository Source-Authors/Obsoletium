// Copyright Valve Corporation, All rights reserved.
//
// Some junk to isolate <windows.h> from polluting everything.

#ifdef WIN32
#include "winlite.h"
#endif // #ifdef WIN32

namespace se::dmxedit {

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

}  // namespace se::dmxedit