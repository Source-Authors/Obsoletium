//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//
//=============================================================================//
// TextConsoleUnix.h: Unix interface for the TextConsole class.
//
//////////////////////////////////////////////////////////////////////

#if !defined TEXTCONSOLE_UNIX_H
#define TEXTCONSOLE_UNIX_H

#ifndef _WIN32

#include <cstdio>
#include "textconsole.h"

namespace se::dedicated {

class CTextConsoleUnix : public CTextConsole
{
public:
	virtual ~CTextConsoleUnix() {}

	// CTextConsole
	bool Init();
	void ShutDown();
	void Print( const char *pszMsg );

	void SetTitle( const char *pszTitle );
	void SetStatusLine( const char *pszStatus );
	void UpdateStatus();

	char *GetLine( int index, char *buf, size_t buflen );
	int   GetWidth();

private:
	bool  m_bConDebug;
	FILE *m_tty;
};

}  // namespace se::dedicated

#endif // _ndef WIN32

#endif // !defined TEXTCONSOLE_UNIX_H
