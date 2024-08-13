//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef ISYS_H
#define ISYS_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "interface.h"

class CDedicatedAppSystemGroup;


abstract_class ISys
{
public:
	virtual				~ISys( void ) { } 

	virtual bool		LoadModules( CDedicatedAppSystemGroup *pAppSystemGroup ) = 0;

	virtual	void		Sleep( int msec ) = 0;
	virtual bool		GetExecutableName( char *out ) = 0;
	virtual void		ErrorMessage( int level, const char *msg ) = 0;

	virtual void		WriteStatusText( char *szText ) = 0;
	virtual void		UpdateStatus( int force ) = 0;

	virtual intp		LoadLibrary( char *lib ) = 0;
	virtual void		FreeLibrary( intp library ) = 0;

	virtual bool		CreateConsoleWindow( void ) = 0;
	virtual void		DestroyConsoleWindow( void ) = 0;

	virtual void		ConsoleOutput ( char *string ) = 0;
	virtual char		*ConsoleInput ( int index, char *buf, int buflen ) = 0;
	virtual void		Printf( PRINTF_FORMAT_STRING const char *fmt, ...) FMTFUNCTION( 2, 3 ) = 0;
};

extern ISys *sys;

#endif // ISYS_H