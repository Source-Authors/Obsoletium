//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// mem.c
#include <cstdlib>
#include <memory.h>
#include <cstring>
#include "mem.h"


void *Mem_Malloc( size_t size )
{
	return malloc( size );
}

void *Mem_ZeroMalloc( size_t size )
{
	void *p;
	
	p = malloc( size );
	memset( (unsigned char *)p, 0, size );
	return p;
}

void *Mem_Realloc( void *memblock, size_t size )
{
	return realloc( memblock, size );
}

void *Mem_Calloc( int num, size_t size )
{
	return calloc( num, size );
}

char *Mem_Strdup( const char *strSource )
{
	return strdup( strSource );
}

void Mem_Free( void *p )
{
	free( p );
}
