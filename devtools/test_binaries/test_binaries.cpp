//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// test_binaries.cpp : test for debug section
//
// Adapted from PEDUMP, AUTHOR:  Matt Pietrek - 1993
//--------------------

#include <winlite.h>
#include <cstdio>

#include "common.h"

#include "tier0/platform.h"
#include "tier1/strtools.h"

namespace {

bool HasSection( PIMAGE_SECTION_HEADER section, int numSections, const char *pSectionName )
{
	for ( int i = 0; i < numSections; i++ )
	{
		if ( !strnicmp( (char *)section[i].Name, pSectionName, 8 ) )
			return true;
	}

	return false;
}

bool TestExeFile( const char *pFilename, PIMAGE_DOS_HEADER dosHeader )
{
	PIMAGE_NT_HEADERS pNTHeader = MakePtr( PIMAGE_NT_HEADERS, dosHeader,
		dosHeader->e_lfanew );

	// First, verify that the e_lfanew field gave us a reasonable
	// pointer, then verify the PE signature.
	if ( IsBadReadPtr(pNTHeader, sizeof(IMAGE_NT_HEADERS)) ||
	     pNTHeader->Signature != IMAGE_NT_SIGNATURE )
	{
		fprintf(stderr, "Unhandled EXE type, or invalid .EXE (%s).\n", pFilename);
		return false;
	}

	if ( HasSection( (PIMAGE_SECTION_HEADER)(pNTHeader+1), pNTHeader->FileHeader.NumberOfSections, "ValveDBG" ) )
	{
		printf("%s is a debug build.\n", pFilename);
	}

	return true;
}

//
// Open up a file, memory map it, and call the appropriate dumping routine
//
bool TestFile(const char *pFilename)
{
	HANDLE hFile = CreateFile(pFilename, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if ( hFile == INVALID_HANDLE_VALUE )
	{
		fprintf(stderr, "Couldn't open file %s with CreateFile().\n", pFilename );
		return false;
	}
	RunCodeAtScopeExit(CloseHandle(hFile));

	HANDLE hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if ( hFileMapping == nullptr )
	{
		fprintf(stderr, "Couldn't open file %s mapping with CreateFileMapping().\n", pFilename );
		return false;
	}
	RunCodeAtScopeExit(CloseHandle(hFileMapping));

	LPVOID lpFileBase = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
	if ( lpFileBase == nullptr )
	{
		fprintf(stderr, "Couldn't map view of file %s with MapViewOfFile().\n", pFilename );
		return false;
	}
	RunCodeAtScopeExit(UnmapViewOfFile(lpFileBase));

	PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)lpFileBase;
	if ( dosHeader->e_magic == IMAGE_DOS_SIGNATURE )
	{
		return TestExeFile( pFilename, dosHeader );
	}

	fprintf(stderr, "Unrecognized %s file format.\n", pFilename);
	return false;
}

}  // namespace

int main(int argc, char* argv[])
{
	if ( argc < 2 )
	{
		fprintf(stderr, "Usage: test_binaries <FILENAME>.\n" );
		return EINVAL;
	}

	char fileName[MAX_PATH], dir[MAX_PATH];
	if ( !V_ExtractFilePath( argv[1], dir ) )
	{
		dir[0] = '\0';
	}
	else
	{
		V_FixSlashes( dir, '/' );

		if ( const intp len = V_strlen(dir); len && dir[len-1] !='/' )
		{
			V_strcat_safe( dir, "/" );
		}
	}
	
	WIN32_FIND_DATA findData;
	if ( HANDLE hFind = FindFirstFile( argv[1], &findData ); hFind != INVALID_HANDLE_VALUE )
	{
		RunCodeAtScopeExit(FindClose( hFind ));

		do
		{
			V_sprintf_safe( fileName, "%s%s", dir, findData.cFileName );
			TestFile( fileName );
		} while ( FindNextFile( hFind, &findData ) );

		return 0;
	}

	fprintf(stderr, "Can't find %s.\n", argv[1] );
	return EINVAL;
}
