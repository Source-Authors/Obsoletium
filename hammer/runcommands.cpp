//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Handles running the OS commands for map compilation.
//
//=============================================================================

#include "stdafx.h"
#include "RunCommands.h"

#include <io.h>
#include <direct.h>
#include <process.h>
#include <afxtempl.h>

#include "GameConfig.h"
#include "Options.h"
#include "ProcessWnd.h"
#include "GlobalFunctions.h"
#include "hammer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

static bool s_bRunsCommands = false;

bool IsRunningCommands() { return s_bRunsCommands; }

static char *pszDocPath, *pszDocName, *pszDocExt;

void FixGameVars(char *pszSrc, OUT_Z_CAP(dstSize) char *pszDst, intp dstSize, BOOL bUseQuotes)
{
	if (!dstSize) Error("Unable to fix game vars for zero size buffer.\n");

	pszDst[0] = '\0';

	char *pszEnd = pszDst + dstSize;
	// run through the parms list and substitute $variable strings for
	//  the real thing
	char *pSrc = pszSrc, *pDst = pszDst;
	BOOL bInQuote = FALSE;
	while(pSrc[0])
	{
		if (pDst == pszEnd)
		{
			Warning( "Unable to fix game vars. Buffer size is %zd which is not enough.\n", dstSize );
			pDst[-1] = '\0';
			return;
		}

		if(pSrc[0] == '$')	// found a parm
		{
			if(pSrc[1] == '$')	// nope, it's a single symbol
			{
				if (pDst == pszEnd)
				{
					Warning( "Unable to fix game vars. Buffer size is %zd which is not enough.\n", dstSize );
					pDst[-1] = '\0';
					return;
				}

				*pDst++ = '$';
				++pSrc;
			}
			else
			{
				// figure out which parm it is .. 
				++pSrc;
				
				if (!bInQuote && bUseQuotes)
				{
					if (pDst == pszEnd)
					{
						Warning( "Unable to fix game vars. Buffer size is %zd which is not enough.\n", dstSize );
						pDst[-1] = '\0';
						return;
					}

					// not in quote, and subbing a variable.. start quote
					*pDst++ = '\"';
					bInQuote = TRUE;
				}

				if(!strnicmp(pSrc, "file", 4))
				{
					pSrc += 4;
					V_strncpy(pDst, pszDocName, pszEnd - pDst);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "ext", 3))
				{
					pSrc += 3;
					V_strncpy(pDst, pszDocExt, pszEnd - pDst);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "path", 4))
				{
					pSrc += 4;
					V_strncpy(pDst, pszDocPath, pszEnd - pDst);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "exedir", 6))
				{
					pSrc += 6;
					V_strncpy(pDst, g_pGameConfig->m_szGameExeDir, pszEnd - pDst);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "bspdir", 6))
				{
					pSrc += 6;
					V_strncpy(pDst, g_pGameConfig->szBSPDir, pszEnd - pDst);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "bsp_exe", 7))
				{
					pSrc += 7;
					V_strncpy(pDst, g_pGameConfig->szBSP, pszEnd - pDst);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "vis_exe", 7))
				{
					pSrc += 7;
					V_strncpy(pDst, g_pGameConfig->szVIS, pszEnd - pDst);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "light_exe", 9))
				{
					pSrc += 9;
					V_strncpy(pDst, g_pGameConfig->szLIGHT, pszEnd - pDst);
					pDst += strlen(pDst);
				}
				else if(!strnicmp(pSrc, "game_exe", 8))
				{
					pSrc += 8;
					V_strncpy(pDst, g_pGameConfig->szExecutable, pszEnd - pDst);
					pDst += strlen(pDst);
				}
				else if (!strnicmp(pSrc, "gamedir", 7))
				{
					pSrc += 7;
					V_strncpy(pDst, g_pGameConfig->m_szModDir, pszEnd - pDst);
					pDst += strlen(pDst);
				}
			}
		}
		else
		{
			if(*pSrc == ' ' && bInQuote)
			{
				bInQuote = FALSE;

				if (pDst == pszEnd)
				{
					Warning( "Unable to fix game vars. Buffer size is %zd which is not enough.\n", dstSize );
					pDst[-1] = '\0';
					return;
				}

				*pDst++ = '\"';	// close quotes
			}

			if (pDst == pszEnd)
			{
				Warning( "Unable to fix game vars. Buffer size is %zd which is not enough.\n", dstSize );
				pDst[-1] = '\0';
				return;
			}

			// just copy the char into the destination buffer
			*pDst++ = *pSrc++;
		}
	}

	if(pDst == pszEnd)
	{
		Warning( "Unable to fix game vars. Buffer size is %zd which is not enough.\n", dstSize );
		pDst[-1] = '\0';
		return;
	}

	if(bInQuote)
	{
		bInQuote = FALSE;
		*pDst++ = '\"';	// close quotes
	}

	if (pDst == pszEnd)
	{
		Warning( "Unable to fix game vars. Buffer size is %zd which is not enough.\n", dstSize );
		pDst[-1] = '\0';
		return;
	}

	pDst[0] = '\0';
}

static void RemoveQuotes(IN_Z_CAP(bufferSize) char *pBuf, intp bufferSize)
{
	if (Q_isempty(pBuf)) return;

	if (pBuf[0] == '"')
		V_memmove(pBuf, pBuf + 1, bufferSize - 1);

	const intp end = bufferSize - 2;

	if (end <= 0)
		pBuf[0] = '\0';
	else if (pBuf[end] == '"')
		pBuf[end] = '\0';
}

LPCTSTR GetErrorString()
{
	static thread_local char szBuf[200];
	// dimhotepus: Add error string using system_category API.
	V_strcpy_safe(szBuf, std::system_category().message(::GetLastError()).c_str());
	char *p = strchr(szBuf, '\r');	// get rid of \r\n
	if(p) p[0] = '\0';
	return szBuf;
}

CProcessWnd procWnd;

template<intp nameSize, intp extensionSize>
void SplitFileNameFromPath(char* szDocLongPath,
	char (&name)[nameSize],
	char (&extension)[extensionSize])
{
	char *p = strrchr(szDocLongPath, '.');
	if(p && strrchr(szDocLongPath, '\\') < p && strrchr(szDocLongPath, '/') < p)
	{
		// got the extension
		V_strcpy_safe(extension, p+1);
		p[0] = '\0';
	}

	p = strrchr(szDocLongPath, '\\');
	if(!p)
		p = strrchr(szDocLongPath, '/');

	if(p)
	{
		// got the filepart
		V_strcpy_safe(name, p+1);
		p[0] = '\0';
	}
}

bool RunCommands(CCommandArray& Commands, LPCTSTR pszOrigDocName, CWnd *parent)
{
	s_bRunsCommands = true;

	char szCurDir[MAX_PATH];
	if ( !_getcwd(szCurDir, size_cast<int>(ssize(szCurDir))) )
	{
		// dimhotepus: Add warning when current directory unavailable.
		Warning( "Unable to get current directory: %s\n",
			std::generic_category().message(errno).c_str() );
	}

	procWnd.GetReady(pszOrigDocName, parent);

	// cut up document name into file and extension components.
	//  create two sets of buffers - one set with the long filename
	//  and one set with the 8.3 format.

	static char szDocLongPath[MAX_PATH] = {0}, szDocLongName[MAX_PATH] = {0}, 
		szDocLongExt[MAX_PATH] = {0};
	szDocLongPath[0] = szDocLongName[0] = szDocLongExt[0];

	char szDocShortPath[MAX_PATH] = {0}, szDocShortName[MAX_PATH] = {0}, 
		szDocShortExt[MAX_PATH] = {0};

	GetFullPathName(pszOrigDocName, std::size(szDocLongPath), szDocLongPath, nullptr);
	GetShortPathName(pszOrigDocName, szDocShortPath, std::size(szDocShortPath));

	// split them up
	SplitFileNameFromPath(szDocLongPath, szDocLongName, szDocLongExt);
	SplitFileNameFromPath(szDocShortPath, szDocShortName, szDocShortExt);

	char *ppParms[32];
	BitwiseClear(ppParms);
	INT_PTR iSize = Commands.GetSize(), i = 0;
	while(iSize--)
	{
		CCOMMAND &cmd = Commands[i++];

		// anything there?
		if((!cmd.szRun[0] && !cmd.iSpecialCmd) || !cmd.bEnable)
			continue;

		// set name pointers for long filenames
		pszDocExt = szDocLongExt;
		pszDocName = szDocLongName;
		pszDocPath = szDocLongPath;
		
		// dimhotepus: x5 -> x4
		static char szNewParms[MAX_PATH*4], szNewRun[MAX_PATH*4];
		szNewParms[0] = szNewRun[0] = '\0';

		// HACK: force the spawnv call for launching the game
		if (!Q_stricmp(cmd.szRun, "$game_exe"))
		{
			cmd.bUseProcessWnd = FALSE;
		}

		FixGameVars(cmd.szRun, szNewRun, TRUE);
		FixGameVars(cmd.szParms, szNewParms, TRUE);

		CString strTmp;
		strTmp.Format("\r\n"
			"** Executing...\r\n"
			"** Command: %s\r\n"
			"** Parameters: %s\r\n\r\n", szNewRun, szNewParms);
		procWnd.Append(strTmp);
		
		// create a parameter list (not always required)
		if(!cmd.bUseProcessWnd || cmd.iSpecialCmd)
		{
			char *p = szNewParms;
			ppParms[0] = szNewRun;
			int iArg = 1;
			BOOL bDone = FALSE;
			while(p[0])
			{
				ppParms[iArg++] = p;
				while(p[0])
				{
					if(p[0] == ' ')
					{
						// found a space-separator
						p[0] = '\0';

						p++;

						// skip remaining white space
						while (*p == ' ')
							p++;

						break;
					}

					// found the beginning of a quoted parameters
					if(p[0] == '\"')
					{
						while(1)
						{
							p++;
							if(p[0] == '\"')
							{
								// found the end
								if(p[1] == '\0')
									bDone = TRUE;
								p[1] = 0;	// kick its ass
								p += 2;

								// skip remaining white space
								while (*p == ' ')
									p++;

								break;
							}
						}
						break;
					}

					// else advance p
					++p;
				}

				if(!p[0] || bDone)
					break;	// done.
			}

			ppParms[iArg] = NULL;

			if(cmd.iSpecialCmd)
			{
				BOOL bError = FALSE;
				CString error = "";

				if(cmd.iSpecialCmd == CCCopyFile && iArg == 3)
				{
					RemoveQuotes(ppParms[1], V_strlen(ppParms[1]));
					RemoveQuotes(ppParms[2], V_strlen(ppParms[2]));
					
					// dimhotepus: stricmp -> V_stricmp for performance.
					// don't copy if we're already there
					if (V_stricmp(ppParms[1], ppParms[2]) != 0 && 
							(!CopyFile(ppParms[1], ppParms[2], FALSE)))
					{
						bError = TRUE;
						error = std::system_category().message(::GetLastError()).c_str();
					}
				}
				else if(cmd.iSpecialCmd == CCDelFile && iArg == 2)
				{
					RemoveQuotes(ppParms[1], V_strlen(ppParms[1]));
					if(!DeleteFile(ppParms[1]))
					{
						bError = TRUE;
						error = std::system_category().message(::GetLastError()).c_str();
					}
				}
				else if(cmd.iSpecialCmd == CCRenameFile && iArg == 3)
				{
					RemoveQuotes(ppParms[1], V_strlen(ppParms[1]));
					RemoveQuotes(ppParms[2], V_strlen(ppParms[2]));
					if(rename(ppParms[1], ppParms[2]))
					{
						bError = TRUE;
						error = strerror(errno);
					}
				}
				else if(cmd.iSpecialCmd == CCChangeDir && iArg == 2)
				{
					RemoveQuotes(ppParms[1], V_strlen(ppParms[1]));
					if(mychdir(ppParms[1]) == -1)
					{
						bError = TRUE;
						error = strerror(errno);
					}
				}

				if(bError)
				{
					CString str;
					str.Format("The command failed:\r\n  \"%s\"\r\n", error.GetString());
					procWnd.Append(str);
					procWnd.SetForegroundWindow();
					str += "\r\nDo you want to continue?";
					if(AfxMessageBox(str, MB_YESNO | MB_ICONQUESTION) == IDNO)
						break;
				}
			}
			else
			{
				// Change to the game exe folder before spawning the engine.
				// This is necessary for Steam to find the correct Steam DLL (it
				// uses the current working directory to search).
				char szDir[MAX_PATH];
				V_strcpy_safe(szDir, szNewRun);
				// dimhotepus: Strip quotes around dir.
				RemoveQuotes(szDir, V_strlen(szDir));
				V_StripFilename(szDir);

				mychdir(szDir);

				// YWB Force asynchronous operation so that engine doesn't hang on
				//  exit???  Seems to work.
				// spawnv doesn't like quotes
				RemoveQuotes(szNewRun, V_strlen(szNewRun));
				intptr_t rc = _spawnv(/*cmd.bNoWait ?*/ _P_NOWAIT /*: P_WAIT*/, szNewRun, ppParms);
				if (rc == -1)
				{
					const int err = errno;
					CString str;
					str.Format("The command failed:\r\n  \"%s\"\r\n", std::generic_category().message(err).c_str());
					procWnd.Append(str);
					procWnd.SetForegroundWindow();
				}
			}
		}
		else
		{
			procWnd.Execute(szNewRun, szNewParms);
		}

		// check for existence?
		if(cmd.bEnsureCheck)
		{
			char szFile[MAX_PATH];
			FixGameVars(cmd.szEnsureFn, szFile, FALSE);
			if(GetFileAttributes(szFile) == INVALID_FILE_ATTRIBUTES)
			{
				// not there!
				CString str;
				// dimhotepus: Enchance error details.
				str.Format("The file \"%s\" was not built by %s.\n\n"
					"Do you want to continue?",
					Q_isempty(szFile) ? pszOrigDocName : szFile,
					szNewRun);
				procWnd.SetForegroundWindow();
				if(AfxMessageBox(str, MB_YESNO | MB_ICONQUESTION) == IDNO)
					break;	// outta here
			}
		}
	}

	mychdir(szCurDir);

	s_bRunsCommands = false;

	return TRUE;
}

