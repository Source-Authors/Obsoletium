//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "replay/replayutils.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include "qlimits.h"
#include "filesystem.h"
#include "replay/replaytime.h"
#include "tier1/fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

static char gs_szGameDir[MAX_OSPATH];

//----------------------------------------------------------------------------------------

void Replay_GetFirstAvailableFilename( OUT_Z_CAP(nDstLen) char *pDst, intp nDstLen, const char *pIdealFilename, const char *pExt,
									   const char *pFilePath, intp nStartIndex )
{
	// Strip extension from ideal filename
	char szIdealFilename[ MAX_OSPATH ];
	V_StripExtension( pIdealFilename, szIdealFilename );

	intp i = nStartIndex;
	while ( 1 )
	{
		V_strncpy( pDst, szIdealFilename, nDstLen );
		V_strcat( pDst, Replay_va( "_%zd%s", i, pExt ), nDstLen );

		// Get a potential working path/filename
		CFmtStr fmtTestFilename(
			"%s%c%s",
			pFilePath,
			CORRECT_PATH_SEPARATOR,
			pDst
		);

		// Make sure slashes are correct for platform
		V_FixSlashes( fmtTestFilename.Access() );

		// Fix up double slashes
		V_FixDoubleSlashes( fmtTestFilename.Access() );

		if ( !g_pFullFileSystem->FileExists( fmtTestFilename ) )
			break;

		++i;
	}
}

//----------------------------------------------------------------------------------------

void Replay_ConstructReplayFilenameString( CUtlString &strOut, const char *pReplaySubDir, const char *pFilename, const char *pGameDir )
{
	// Construct full filename
	strOut.Format( "%s%creplays%c%s%c%s", pGameDir,
		CORRECT_PATH_SEPARATOR, CORRECT_PATH_SEPARATOR, pReplaySubDir,
		CORRECT_PATH_SEPARATOR, pFilename
	);
}

//----------------------------------------------------------------------------------------

char *Replay_va( PRINTF_FORMAT_STRING const char *format, ... )
{
	va_list		argptr;
	static char	string[8][512];
	static int	curstring = 0;
	
	curstring = ( curstring + 1 ) % 8;

	va_start (argptr, format);
	V_vsprintf_safe( string[curstring], format, argptr );
	va_end (argptr);

	return string[curstring];  
}

//----------------------------------------------------------------------------------------

void Replay_SetGameDir( const char *pGameDir )
{
	V_strcpy_safe( gs_szGameDir, pGameDir );
}

//----------------------------------------------------------------------------------------

const char *Replay_GetGameDir()
{
	return gs_szGameDir;
}

//----------------------------------------------------------------------------------------

const char *Replay_GetBaseDir()
{
	return Replay_va(
		"%s%creplays%c",
		Replay_GetGameDir(),
		CORRECT_PATH_SEPARATOR,
		CORRECT_PATH_SEPARATOR
	);
}

//----------------------------------------------------------------------------------------

void Replay_GetAutoName( OUT_Z_BYTECAP(nDestSize) wchar_t *pDest, int nDestSize, const char *pMapName )
{
	// Get date/time
	CReplayTime now;
	now.InitDateAndTimeToNow();

	// Convert map name to unicode
	wchar_t wszMapName[256];
	extern vgui::ILocalize *g_pVGuiLocalize;
	g_pVGuiLocalize->ConvertANSIToUnicode( pMapName, wszMapName );

	// Get localized date as string
	const wchar_t *pLocalizedDate = CReplayTime::GetLocalizedDate( g_pVGuiLocalize, now, true );

	// Create title
	g_pVGuiLocalize->ConstructString( pDest, nDestSize, L"%s1: %s2", 2, wszMapName, pLocalizedDate );
}

//----------------------------------------------------------------------------------------
