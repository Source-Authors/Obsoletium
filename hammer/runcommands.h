//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef _RUNCOMMANDS_H
#define _RUNCOMMANDS_H

#include <afxtempl.h>
#include "tier0/platform.h"

//
// RunCommands functions
//

enum
{
	CCChangeDir = 0x100,
	CCCopyFile,
	CCDelFile,
	CCRenameFile
};

// command:
typedef struct
{
	BOOL bEnable;				// Run this command?

	int iSpecialCmd;			// Nonzero if special command exists
	char szRun[MAX_PATH];
	char szParms[MAX_PATH];
	BOOL bLongFilenames;		// Obsolete, but kept here for file backwards compatibility
	BOOL bEnsureCheck;
	char szEnsureFn[MAX_PATH];
	BOOL bUseProcessWnd;
	BOOL bNoWait;

} CCOMMAND, *PCCOMMAND;

// list of commands:
typedef CArray<CCOMMAND, CCOMMAND&> CCommandArray;

// run a list of commands:
bool RunCommands(CCommandArray& Commands, LPCTSTR pszDocName);
void FixGameVars(char *pszSrc, OUT_Z_CAP(dstSize) char *pszDst, intp dstSize, BOOL bUseQuotes = TRUE);
template<intp dstSize>
void FixGameVars(char* pszSrc, OUT_Z_ARRAY char (&pszDst)[dstSize], BOOL bUseQuotes = TRUE)
{
	FixGameVars(pszSrc, pszDst, dstSize, bUseQuotes);
}
bool IsRunningCommands();

#endif // _RUNCOMMANDS_H
