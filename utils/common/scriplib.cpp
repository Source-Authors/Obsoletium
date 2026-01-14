//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "scriplib.h"

#include "tier1/strtools.h"
#include "tier2/tier2.h"
#include "cmdlib.h"
#if defined(POSIX)
#include "../../filesystem/linux_support.h"
#include <sys/stat.h>
#endif
/*
=============================================================================

						PARSING STUFF

=============================================================================
*/

struct script_t
{
	char	filename[1024];
	char    *buffer,*script_p,*end_p;
	int     line;

	char	macrobuffer[4096];
	char	*macroparam[64];
	char	*macrovalue[64];
	int		nummacroparams;

};

constexpr int MAX_INCLUDES{64};

script_t	scriptstack[MAX_INCLUDES];
script_t	*script = NULL;
int			scriptline;

char    token[MAXTOKEN];
qboolean endofscript;
qboolean tokenready;                     // only true if UnGetToken was just called

typedef struct 
{
	char *param;
	char *value;
} variable_t;

CUtlVector<variable_t> g_definevariable;

/*
Callback stuff
*/

void DefaultScriptLoadedCallback( char const *, char const *, int )
{
}

SCRIPT_LOADED_CALLBACK g_pfnCallback = DefaultScriptLoadedCallback;

SCRIPT_LOADED_CALLBACK SetScriptLoadedCallback( SCRIPT_LOADED_CALLBACK pfnNewScriptLoadedCallback )
{
	SCRIPT_LOADED_CALLBACK pfnCallback = g_pfnCallback;
	g_pfnCallback = pfnNewScriptLoadedCallback;
	return pfnCallback;
}

/*
==============
AddScriptToStack
==============
*/
void AddScriptToStack (const char *filename, ScriptPathMode_t pathMode = SCRIPT_USE_ABSOLUTE_PATH)
{
	script++;
	if (script == &scriptstack[MAX_INCLUDES])
		Error ("script file exceeded MAX_INCLUDES");
	
	if ( pathMode == SCRIPT_USE_RELATIVE_PATH )
		V_strcpy_safe( script->filename, filename );
	else
		V_strcpy_safe( script->filename, ExpandPath (filename) );

	int size = LoadFile (script->filename, (void **)&script->buffer);

	// printf ("entering %s\n", script->filename);
	if ( g_pfnCallback )
	{
		if ( script == scriptstack + 1 )
			g_pfnCallback( script->filename, NULL, 0 );
		else
			g_pfnCallback( script->filename, script[-1].filename, script[-1].line );
	}

	script->line = 1;
	script->script_p = script->buffer;
	script->end_p = script->buffer + size;
}


/*
==============
LoadScriptFile
==============
*/
void LoadScriptFile (char *filename, ScriptPathMode_t pathMode)
{
	script = scriptstack;
	AddScriptToStack (filename, pathMode);

	endofscript = false;
	tokenready = false;
}


/*
==============
==============
*/

script_t	*macrolist[256];
int nummacros;

static void DefineMacro( char *macroname, char (&out)[MAXTOKEN] )
{
	script_t	*pmacro = (script_t *)malloc( sizeof( script_t ) );
	if (!pmacro) Error("Out of memory in DefineMacro for %s", macroname);

	V_strcpy_safe(pmacro->filename, macroname);
	pmacro->line = script->line;
	pmacro->nummacroparams = 0;

	char *mp = pmacro->macrobuffer;
	char *cp = script->script_p;

	while (TokenAvailable( ))
	{
		GetToken( false, out );

		if (out[0] == '\\' && out[1] == '\\')
		{
			break;
		}
		cp = script->script_p;

		pmacro->macroparam[pmacro->nummacroparams++] = mp;

		V_strncpy( mp, out, ssize(pmacro->macrobuffer) );
		mp += strlen( out ) + 1;

		if (mp >= pmacro->macrobuffer + sizeof( pmacro->macrobuffer ))
			Error("Macro buffer overflow\n");
	}
	// roll back script_p to previous valid location
	script->script_p = cp;

	// find end of macro def
	while (*cp && *cp != '\n')
	{
		//Msg("%d ", *cp );
		if (*cp == '\\' && *(cp+1) == '\\')
		{
			// skip till end of line
			while (*cp && *cp != '\n')
			{
				*cp = ' '; // replace with spaces
				cp++;
			}

			if (*cp)
			{
				cp++;
			}
		}
		else
		{
			cp++;
		}
	}

	intp size = (cp - script->script_p);

	pmacro->buffer = (char *)malloc( size + 1);
	if (!pmacro->buffer) Error("Out of memory in macro buffer");
	memcpy( pmacro->buffer, script->script_p, size );
	pmacro->buffer[size] = '\0';
	pmacro->end_p = &pmacro->buffer[size]; 

	macrolist[nummacros++] = pmacro;

	script->script_p = cp;
}


static void DefineVariable( char *variablename, char (&out)[MAXTOKEN] )
{
	variable_t v;

	v.param = strdup( variablename );

	GetToken( false, out );
	
	v.value = strdup( out );

	g_definevariable.AddToTail( v );
}



/*
==============
==============
*/
static bool AddMacroToStack( char *macroname, char (&out)[MAXTOKEN] )
{
	// lookup macro
	if (macroname[0] != '$')
		return false;

	int i;
	for (i = 0; i < nummacros; i++)
	{
		if (strcmpi( macrolist[i]->filename, &macroname[1] ) == 0)
		{
			break;
		}
	}
	if (i == nummacros)
		return false;

	script_t *pmacro = macrolist[i];

	// get tokens
	script_t	*pnext = script + 1;

	pnext++;
	if (pnext == &scriptstack[MAX_INCLUDES])
		Error ("script file exceeded MAX_INCLUDES");

	// get tokens
	char *cp = pnext->macrobuffer;

	pnext->nummacroparams = pmacro->nummacroparams;

	for (i = 0; i < pnext->nummacroparams; i++)
	{
		GetToken(false, out);

		strcpy( cp, out );
		pnext->macroparam[i] = pmacro->macroparam[i];
		pnext->macrovalue[i] = cp;

		cp += strlen( out ) + 1;

		if (cp >= pnext->macrobuffer + sizeof( pnext->macrobuffer ))
			Error("Macro buffer overflow\n");
	}

	script = pnext;
	V_strcpy_safe( script->filename, pmacro->filename );

	intp size = pmacro->end_p - pmacro->buffer;
	script->buffer = (char *)malloc( size + 1 );
	if (!script->buffer) return false;
	memcpy( script->buffer, pmacro->buffer, size );
	pmacro->buffer[size] = '\0';
	script->script_p = script->buffer;
	script->end_p = script->buffer + size;
	script->line = pmacro->line;

	return true;
}



static bool ExpandMacroToken( char *&token_p, char (&out)[MAXTOKEN] )
{
	if ( script->nummacroparams && *script->script_p == '$' )
	{
		char *cp = script->script_p + 1;

		while ( *cp > 32 && *cp != '$' )
		{
			cp++;
		}

		// found a word with $'s on either end?
		if (*cp != '$')
			return false;

		// get token pointer
		char *tp = script->script_p + 1;
		intp len = (cp - tp);
		*(tp + len) = '\0';

		// lookup macro parameter
		int index = 0;
		for (index = 0; index < script->nummacroparams; index++)
		{
			if (stricmp( script->macroparam[index], tp ) == 0)
				break;
		}
		if (index >= script->nummacroparams)
		{
			Error("unknown macro token \"%s\" in %s\n", tp, script->filename );
		}

		// paste token into 
		len = V_strlen( script->macrovalue[index] );
		strcpy( token_p, script->macrovalue[index] );
		token_p += len;
		
		script->script_p = cp + 1;

		if (script->script_p >= script->end_p)
			Error ("Macro expand overflow\n");

		if (token_p >= &out[MAXTOKEN])
			Error ("Token too large on line %i\n",scriptline);

		return true;
	}
	return false;
}



/*
==============
==============
*/
// FIXME: this should create a new script context so the individual tokens in the variable can be parsed
static bool ExpandVariableToken( char *&token_p, char (&out)[MAXTOKEN] )
{
	if ( *script->script_p == '$' )
	{
		char *cp = script->script_p + 1;

		while ( *cp > 32 && *cp != '$' )
		{
			cp++;
		}

		// found a word with $'s on either end?
		if (*cp != '$')
			return false;

		// get token pointer
		char *tp = script->script_p + 1;
		intp len = (cp - tp);
		*(tp + len) = '\0';

		// lookup macro parameter

		intp index;
		for (index = 0; index < g_definevariable.Count(); index++)
		{
			if (Q_strnicmp( g_definevariable[index].param, tp, len ) == 0)
				break;
		}
	
		if (index >= g_definevariable.Count() )
		{
			Error("unknown variable token \"%s\" in %s\n", tp, script->filename );
		}

		// paste token into 
		len = V_strlen( g_definevariable[index].value );
		strcpy( token_p, g_definevariable[index].value );
		token_p += len;
		
		script->script_p = cp + 1;

		if (script->script_p >= script->end_p)
			Error ("Macro expand overflow\n");

		if (token_p >= &out[MAXTOKEN])
			Error ("Token too large on line %i\n",scriptline);

		return true;
	}
	return false;
}



/*
==============
ParseFromMemory
==============
*/
void ParseFromMemory (char *buffer, intp size)
{
	script = scriptstack;
	script++;
	if (script == &scriptstack[MAX_INCLUDES])
		Error ("script file exceeded MAX_INCLUDES");
	V_strcpy_safe (script->filename, "memory buffer" );

	script->buffer = buffer;
	script->line = 1;
	script->script_p = script->buffer;
	script->end_p = script->buffer + size;

	endofscript = false;
	tokenready = false;
}


//-----------------------------------------------------------------------------
// Used instead of ParseFromMemory to temporarily add a memory buffer
// to the script stack.  ParseFromMemory just blows away the stack.
//-----------------------------------------------------------------------------
void PushMemoryScript( char *pszBuffer, const int nSize )
{
	if ( script == NULL )
	{
		script = scriptstack;
	}
	script++;
	if ( script == &scriptstack[MAX_INCLUDES] )
	{
		Error ( "script file exceeded MAX_INCLUDES" );
	}
	V_strcpy_safe (script->filename, "memory buffer" );

	script->buffer = pszBuffer;
	script->line = 1;
	script->script_p = script->buffer;
	script->end_p = script->buffer + nSize;

	endofscript = false;
	tokenready = false;
}


//-----------------------------------------------------------------------------
// Used after calling PushMemoryScript to clean up the memory buffer
// added to the script stack.  The normal end of script terminates
// all parsing at the end of a memory buffer even if there are more scripts
// remaining on the script stack
//-----------------------------------------------------------------------------
bool PopMemoryScript()
{
	if ( V_stricmp( script->filename, "memory buffer" ) )
		return false;

	if ( script == scriptstack )
	{
		endofscript = true;
		return false;
	}
	script--;
	scriptline = script->line;

	endofscript = false;

	return true;
}


/*
==============
UnGetToken

Signals that the current token was not used, and should be reported
for the next GetToken.  Note that

GetToken (true);
UnGetToken ();
GetToken (false);

could cross a line boundary.
==============
*/
void UnGetToken (void)
{
	tokenready = true;
}


qboolean EndOfScript (qboolean crossline, char (&out)[MAXTOKEN])
{
	if (!crossline)
		Error ("Line %i is incomplete\n",scriptline);

	if (!strcmp (script->filename, "memory buffer"))
	{
		endofscript = true;
		return false;
	}

	free (script->buffer);
	script->buffer = NULL;
	if (script == scriptstack+1)
	{
		endofscript = true;
		return false;
	}
	script--;
	scriptline = script->line;
	// printf ("returning to %s\n", script->filename);
	return GetToken (crossline, out);
}


//-----------------------------------------------------------------------------
// Purpose: Given an absolute path, do a find first find next on it and build
// a list of files.  Physical file system only
//-----------------------------------------------------------------------------
static void FindFileAbsoluteList( CUtlVector< CUtlString > &outAbsolutePathNames, const char *pszFindName )
{
	char szPath[MAX_PATH];
	V_strcpy_safe( szPath, pszFindName );
	V_StripFilename( szPath );

	char szResult[MAX_PATH];
	FileFindHandle_t hFile = FILESYSTEM_INVALID_FIND_HANDLE;
	const char *pszFoundFile = g_pFullFileSystem->FindFirst( pszFindName, &hFile );
	RunCodeAtScopeExit(g_pFullFileSystem->FindClose( hFile ));

	for ( ;
		  pszFoundFile && hFile != FILESYSTEM_INVALID_FIND_HANDLE;
		  pszFoundFile = g_pFullFileSystem->FindNext( hFile ) )
	{
		V_ComposeFileName( szPath, pszFoundFile, szResult );
		outAbsolutePathNames.AddToTail( szResult );
	}
}


//-----------------------------------------------------------------------------
// Data for checking for single character tokens while parsing
//-----------------------------------------------------------------------------
bool g_bCheckSingleCharTokens = false;
CUtlString g_sSingleCharTokens;


//-----------------------------------------------------------------------------
// Sets whether the scriplib parser will do a special check for single
// character tokens.  Returns previous state of whether single character
// tokens will be checked.
//-----------------------------------------------------------------------------
bool SetCheckSingleCharTokens( bool bCheck )
{
	const bool bRetVal = g_bCheckSingleCharTokens;

	g_bCheckSingleCharTokens = bCheck;

	return bRetVal;
}


//-----------------------------------------------------------------------------
// Sets the list of single character tokens to check if SetCheckSingleCharTokens
// is turned on.
//-----------------------------------------------------------------------------
CUtlString SetSingleCharTokenList( const char *pszSingleCharTokenList )
{
	const CUtlString sRetVal = g_sSingleCharTokens;

	if ( pszSingleCharTokenList )
	{
		g_sSingleCharTokens = pszSingleCharTokenList;
	}

	return sRetVal;
}


/*
==============
GetToken
==============
*/
qboolean GetToken (qboolean crossline, char (&out)[MAXTOKEN])
{
	char    *token_p;

	if (tokenready)                         // is a token allready waiting?
	{
		tokenready = false;
		return true;
	}

	// printf("script_p %x (%x)\n", script->script_p, script->end_p ); fflush( stdout );

	if (script->script_p >= script->end_p)
	{
		return EndOfScript (crossline, out);
	}

	tokenready = false;

	// skip space, ctrl chars
skipspace:
	while (*script->script_p <= ' ')
	{
		if (script->script_p >= script->end_p)
		{
			return EndOfScript (crossline, out);
		}
		if (*(script->script_p++) == '\n')
		{
			if (!crossline)
			{
				Error ("Line %i is incomplete\n",scriptline);
			}
			scriptline = ++script->line;
		}
	}

	if (script->script_p >= script->end_p)
	{
		return EndOfScript (crossline, out);
	}

	// strip single line comments
	if (*script->script_p == ';' || *script->script_p == '#' ||		 // semicolon and # is comment field
		(*script->script_p == '/' && *((script->script_p)+1) == '/')) // also make // a comment field
	{											
		if (!crossline)
			Error ("Line %i is incomplete\n",scriptline);
		while (*script->script_p++ != '\n')
		{
			if (script->script_p >= script->end_p)
			{
				return EndOfScript (crossline, out);
			}
		}
		scriptline = ++script->line;
		goto skipspace;
	}

	//  strip out matching /* */ comments
	if (*script->script_p == '/' && *((script->script_p)+1) == '*')
	{
		script->script_p += 2;
		while (*script->script_p != '*' || *((script->script_p)+1) != '/')
		{
			if (*script->script_p++ != '\n')
			{
				if (script->script_p >= script->end_p)
				{
					return EndOfScript (crossline, out);
				}

				scriptline = ++script->line;
			}
		}
		script->script_p += 2;
		goto skipspace;
	}

	// copy token to buffer
	token_p = out;

	if (*script->script_p == '"')
	{
		// quoted token
		script->script_p++;
		while (*script->script_p != '"')
		{
			*token_p++ = *script->script_p++;
			if (script->script_p == script->end_p)
				break;
			if (token_p == &out[MAXTOKEN])
				Error ("Token too large on line %i\n",scriptline);
		}
		script->script_p++;
	}
	else if ( g_bCheckSingleCharTokens && !g_sSingleCharTokens.IsEmpty() && strchr( g_sSingleCharTokens.String(), *script->script_p ) != NULL )
	{
		*token_p++ = *script->script_p++;
	}
	else	// regular token
	while ( *script->script_p > 32 && *script->script_p != ';')
	{
		if ( !ExpandMacroToken( token_p, out ) )
		{
			if ( !ExpandVariableToken( token_p, out ) )
			{
				*token_p++ = *script->script_p++;
				if (script->script_p == script->end_p)
					break;
				if (token_p == &out[MAXTOKEN])
					Error ("Token too large on line %i\n",scriptline);

			}
		}
	}

	// add null to end of token
	*token_p = 0;

	// check for other commands
	if ( !stricmp( out, "$include" ) )
	{
		GetToken( false, out );

		bool bFallbackToToken = true;

		CUtlVector< CUtlString > expandedPathList;

		if ( CmdLib_ExpandWithBasePaths( expandedPathList, out ) > 0 )
		{
			for ( intp i = 0; i < expandedPathList.Count(); ++i )
			{
				CUtlVector< CUtlString > findFileList;
				FindFileAbsoluteList( findFileList, expandedPathList[i].String() );

				if ( findFileList.Count() > 0 )
				{
					bFallbackToToken = false;

					// Only add the first set of glob matches from the first base path
					for ( intp j = 0; j < findFileList.Count(); ++j )
					{
						AddScriptToStack( findFileList[j].String() );
					}

					break;
				}
			}
		}

		if ( bFallbackToToken )
		{
			AddScriptToStack( out );
		}

		return GetToken( crossline, out );
	}
	else if (!stricmp (out, "$definemacro"))
	{
		GetToken (false, out);
		DefineMacro(out, out);
		return GetToken( crossline, out );
	}
	else if (!stricmp (out, "$definevariable"))
	{
		GetToken (false, out);
		DefineVariable(out, out);
		return GetToken (crossline, out);
	}
	else if (AddMacroToStack( out, out ))
	{
		return GetToken (crossline, out);
	}

	return true;
}


/*
==============
GetExprToken - use C mathematical operator parsing rules to split tokens instead of whitespace
==============
*/
qboolean GetExprToken (qboolean crossline, char (&out)[MAXTOKEN])
{
	char    *token_p;

	if (tokenready)                         // is a token allready waiting?
	{
		tokenready = false;
		return true;
	}

	if (script->script_p >= script->end_p)
		return EndOfScript (crossline, out);

	tokenready = false;

//
// skip space
//
skipspace:
	while (*script->script_p <= 32)
	{
		if (script->script_p >= script->end_p)
			return EndOfScript (crossline, out);
		if (*script->script_p++ == '\n')
		{
			if (!crossline)
				Error ("Line %i is incomplete\n",scriptline);
			scriptline = ++script->line;
		}
	}

	if (script->script_p >= script->end_p)
		return EndOfScript (crossline, out);

	if (*script->script_p == ';' || *script->script_p == '#' ||		 // semicolon and # is comment field
		(*script->script_p == '/' && *((script->script_p)+1) == '/')) // also make // a comment field
	{											
		if (!crossline)
			Error ("Line %i is incomplete\n",scriptline);
		while (*script->script_p++ != '\n')
			if (script->script_p >= script->end_p)
				return EndOfScript (crossline, out);
		goto skipspace;
	}

//
// copy token
//
	token_p = out;

	if (*script->script_p == '"')
	{
		// quoted token
		script->script_p++;
		while (*script->script_p != '"')
		{
			*token_p++ = *script->script_p++;
			if (script->script_p == script->end_p)
				break;
			if (token_p == &out[MAXTOKEN])
				Error ("Token too large on line %i\n",scriptline);
		}
		script->script_p++;
	}
	else
	{
		if ( V_isalpha( *script->script_p ) || *script->script_p == '_' )
		{
			// regular token
			while ( V_isalnum( *script->script_p ) || *script->script_p == '_' )
			{
				*token_p++ = *script->script_p++;
				if (script->script_p == script->end_p)
					break;
				if (token_p == &out[MAXTOKEN])
					Error ("Token too large on line %i\n",scriptline);
			}
		}
		else if ( V_isdigit( *script->script_p ) || *script->script_p == '.' )
		{
			// regular token
			while ( V_isdigit( *script->script_p ) || *script->script_p == '.' )
			{
				*token_p++ = *script->script_p++;
				if (script->script_p == script->end_p)
					break;
				if (token_p == &out[MAXTOKEN])
					Error ("Token too large on line %i\n",scriptline);
			}
		}
		else
		{
			// single char
			*token_p++ = *script->script_p++;
		}
	}

	*token_p = 0;

	if (!stricmp (out, "$include"))
	{
		GetToken (false, out);
		AddScriptToStack (out);
		return GetToken (crossline, out);
	}

	return true;
}


/*
==============
TokenAvailable

Returns true if there is another token on the line
==============
*/
qboolean TokenAvailable (void)
{
	char    *search_p;

	if (tokenready)                         // is a token allready waiting?
	{
		return true;
	}

	search_p = script->script_p;

	if (search_p >= script->end_p)
		return false;

	while ( *search_p <= 32)
	{
		if (*search_p == '\n')
			return false;
		search_p++;
		if (search_p == script->end_p)
			return false;

	}

	if (*search_p == ';' || *search_p == '#' ||		 // semicolon and # is comment field
		(*search_p == '/' && *((search_p)+1) == '/')) // also make // a comment field
		return false;

	return true;
}

qboolean GetTokenizerStatus( char **pFilename, int *pLine )
{
	// is this the default state?
	if (!script)
		return false;

	if (script->script_p >= script->end_p)
		return false;

	if (pFilename)
	{
		*pFilename = script->filename;
	}
	if (pLine)
	{
		*pLine = script->line;
	}
	return true;
}


#include <cstdio>
#include <cstdlib>
#ifdef WIN32
#include <direct.h>
#include <io.h>
#include <sys/utime.h>
#endif
#include <ctime>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "tier1/utlbuffer.h"

class CScriptLib final : public IScriptLib
{
public:
	bool ReadFileToBuffer( const char *pSourceName, CUtlBuffer &buffer, bool bText = false, bool bNoOpenFailureWarning = false ) override;
	bool WriteBufferToFile( const char *pTargetName, CUtlBuffer &buffer, DiskWriteMode_t writeMode ) override;
	intp FindFiles( char* pFileMask, bool bRecurse, CUtlVector<fileList_t> &fileList ) override;
	[[deprecated]] char *MakeTemporaryFilename( char const *pchModPath, char *pPath, intp pathSize ) override;
	void DeleteTemporaryFiles( const char *pFileMask ) override;
	int CompareFileTime( const char *pFilenameA, const char *pFilenameB ) override;
	bool DoesFileExist( const char *pFilename ) override;

private:

	intp GetFileList( const char* pDirPath, const char* pPattern, CUtlVector< fileList_t > &fileList );
	void RecurseFileTree_r( const char* pDirPath, int depth, CUtlVector< CUtlString > &dirList );
};

static CScriptLib g_ScriptLib;
IScriptLib *scriptlib = &g_ScriptLib;
IScriptLib *g_pScriptLib = &g_ScriptLib;

//-----------------------------------------------------------------------------
// Existence check
//-----------------------------------------------------------------------------
bool CScriptLib::DoesFileExist( const char *pFilename )
{
	return g_pFullFileSystem->FileExists( pFilename );
}

//-----------------------------------------------------------------------------
// Purpose: Helper utility, read file into buffer
//-----------------------------------------------------------------------------
bool CScriptLib::ReadFileToBuffer( const char *pSourceName, CUtlBuffer &buffer, bool bText, bool bNoOpenFailureWarning )
{
	bool bSuccess = true;

	if ( !g_pFullFileSystem->ReadFile( pSourceName, NULL, buffer ) )
	{
		if ( !bNoOpenFailureWarning )
		{
			Msg( "ReadFileToBuffer(): Error opening %s: %s\n", pSourceName, strerror( errno ) );
		}
		return false;
	}

	if ( bText )
	{
		// force it into text mode
		buffer.SetBufferType( true, true );
	}
	else
	{
		buffer.SetBufferType( false, false );
	}

	return bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose: Helper utility, Write buffer to file
//-----------------------------------------------------------------------------
bool CScriptLib::WriteBufferToFile( const char *pTargetName, CUtlBuffer &buffer, DiskWriteMode_t writeMode )
{
	char*	ptr;
	char	dirPath[MAX_PATH];

	bool bSuccess = true;

	// create path
	// prime and skip to first seperator
	V_strcpy_safe( dirPath, pTargetName );
	ptr = strchr( dirPath, '\\' );
	while ( ptr )
	{		
		ptr = strchr( ptr+1, '\\' );
		if ( ptr )
		{
			*ptr = '\0';
			if ( _mkdir( dirPath ) && errno != EEXIST ) return false;
			*ptr = '\\';
		}
	}

	bool bDoWrite = false;
	if ( writeMode == WRITE_TO_DISK_ALWAYS )
	{
		bDoWrite = true;
	}
	else if ( writeMode == WRITE_TO_DISK_UPDATE )
	{
		if ( DoesFileExist( pTargetName ) )
		{
			bDoWrite = true;
		}
	}

	if ( bDoWrite )
	{
		bSuccess = g_pFullFileSystem->WriteFile( pTargetName, NULL, buffer );
	}

	return bSuccess;
}

//-----------------------------------------------------------------------------
// Returns -1, 0, or 1.
//-----------------------------------------------------------------------------
int CScriptLib::CompareFileTime( const char *pFilenameA, const char *pFilenameB )
{
	time_t timeA = g_pFullFileSystem->GetFileTime( pFilenameA );
	time_t timeB = g_pFullFileSystem->GetFileTime( pFilenameB );

	if ( timeA == -1)
	{
		// file a not exist
		timeA = 0;
	}
	if ( timeB == -1 )
	{
		// file b not exist
		timeB = 0;
	}

	if ( (uintp)timeA < (uintp)timeB )
	{
		return -1;
	}
	else if ( (uintp)timeA > (uintp)timeB )
	{
		return 1;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Make a temporary filename
//-----------------------------------------------------------------------------
[[deprecated]] char *CScriptLib::MakeTemporaryFilename( char const *pchModPath, char *pPath, intp pathSize )
{
	AssertMsg( false, "Deprecated MakeTemporaryFilename." );

	return nullptr;
}

//-----------------------------------------------------------------------------
// Delete temporary files
//-----------------------------------------------------------------------------
void CScriptLib::DeleteTemporaryFiles( const char *pFileMask )
{
	const char *pEnv = getenv( "temp" );
	if ( !pEnv )
	{
		pEnv = getenv( "tmp" );
	}

	if ( pEnv )
	{
		char tempPath[MAX_PATH];
		V_strcpy_safe( tempPath, pEnv );
		V_AppendSlash( tempPath );
		V_strcat_safe( tempPath, pFileMask );

		CUtlVector<fileList_t> fileList;
		FindFiles( tempPath, false, fileList );
		for ( auto &l : fileList )
		{
			if (unlink(l.fileName.String())) {
				Warning("Unable to remove temp file '%s': %s.\n", l.fileName.String(),
						std::generic_category().message(errno).c_str());
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get list of files from current path that match pattern
//-----------------------------------------------------------------------------
intp CScriptLib::GetFileList( const char* pDirPath, const char* pPattern, CUtlVector< fileList_t > &fileList )
{
	char	sourcePath[MAX_PATH];
	char	fullPath[MAX_PATH];
	bool	bFindDirs;

	fileList.Purge();

	V_strcpy_safe( sourcePath, pDirPath );
	intp len = V_strlen( sourcePath );
	if ( !len )
	{
		V_strcpy_safe( sourcePath, ".\\" );
	}
	else if ( sourcePath[len-1] != '\\' )
	{
		sourcePath[len]   = '\\';
		sourcePath[len+1] = '\0';
	}

	V_strcpy_safe( fullPath, sourcePath );
	if ( pPattern[0] == '\\' && pPattern[1] == '\0' )
	{
		// find directories only
		bFindDirs = true;
		V_strcat_safe( fullPath, "*" );
	}
	else
	{
		// find files, use provided pattern
		bFindDirs = false;
		V_strcat_safe( fullPath, pPattern );
	}

#ifdef WIN32
	struct _finddata_t findData;
	intptr_t h = _findfirst( fullPath, &findData );
	if ( h == -1 )
	{
		return 0;
	}

	RunCodeAtScopeExit(_findclose( h ));

	do
	{
		// dos attribute complexities i.e. _A_NORMAL is 0
		if ( bFindDirs )
		{
			// skip non dirs
			if ( !( findData.attrib & _A_SUBDIR ) )
				continue;
		}
		else
		{
			// skip dirs
			if ( findData.attrib & _A_SUBDIR )
				continue;
		}

		if ( !stricmp( findData.name, "." ) )
			continue;

		if ( !stricmp( findData.name, ".." ) )
			continue;

		char fileName[MAX_PATH];
		V_strcpy_safe( fileName, sourcePath );
		V_strcat_safe( fileName, findData.name );

		intp j = fileList.AddToTail();
		fileList[j].fileName.Set( fileName );
		fileList[j].timeWrite = findData.time_write;
	}
	while ( !_findnext( h, &findData ) );
#elif defined(POSIX)
	FIND_DATA findData;
	Q_FixSlashes( fullPath );
	void *h = FindFirstFile( fullPath, &findData );
	if ( (intp)h == -1 )
	{
		return 0;
	}

	RunCodeAtScopeExit(FindClose(h));

	do
	{
		// dos attribute complexities i.e. _A_NORMAL is 0
		if ( bFindDirs )
		{
			// skip non dirs
			if ( !( findData.dwFileAttributes & S_IFDIR ) )
				continue;
		}
		else
		{
			// skip dirs
			if ( findData.dwFileAttributes & S_IFDIR )
				continue;
		}

		if ( !stricmp( findData.cFileName, "." ) )
			continue;

		if ( !stricmp( findData.cFileName, ".." ) )
			continue;

		char fileName[MAX_PATH];
		V_strcpy_safe( fileName, sourcePath );
		V_strcat_safe( fileName, findData.cFileName );

		intp j = fileList.AddToTail();
		fileList[j].fileName.Set( fileName );
		struct stat statbuf;
		if ( stat( fileName, &statbuf ) )
#ifdef OSX
			fileList[j].timeWrite = statbuf.st_mtimespec.tv_sec;
#else
			fileList[j].timeWrite = statbuf.st_mtime;
#endif
		else
			fileList[j].timeWrite = 0;
	}
	while ( !FindNextFile( h, &findData ) );
#else
#error "Please define your platform"
#endif

	return fileList.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Recursively determine directory tree
//-----------------------------------------------------------------------------
void CScriptLib::RecurseFileTree_r( const char* pDirPath, int depth, CUtlVector< CUtlString > &dirList )
{
	// recurse from source directory, get directories only
	CUtlVector< fileList_t > fileList;
	intp dirCount = GetFileList( pDirPath, "\\", fileList );
	if ( !dirCount )
	{
		// add directory name to search tree
		dirList[dirList.AddToTail()].Set( pDirPath );
		return;
	}

	for ( intp i=0; i<dirCount; i++ )
	{
		// form new path name, recurse into
		RecurseFileTree_r( fileList[i].fileName.String(), depth+1, dirList );
	}

	dirList[dirList.AddToTail()].Set( pDirPath );
}

//-----------------------------------------------------------------------------
// Purpose: Generate a list of file matching mask
//-----------------------------------------------------------------------------
intp CScriptLib::FindFiles( char* pFileMask, bool bRecurse, CUtlVector<fileList_t> &fileList )
{
	char	dirPath[MAX_PATH];
	char	pattern[MAX_PATH];
	char	extension[MAX_PATH];

	// get path only
	V_strcpy_safe( dirPath, pFileMask );
	V_StripFilename( dirPath );

	// get pattern only
	V_FileBase( pFileMask, pattern );
	V_ExtractFileExtension( pFileMask, extension );
	if ( extension[0] )
	{
		V_strcat_safe( pattern, "." );
		V_strcat_safe( pattern, extension );
	}

	if ( !bRecurse )
	{
		GetFileList( dirPath, pattern, fileList );
	}
	else
	{
		// recurse and get the tree
		CUtlVector< fileList_t > tempList;
		CUtlVector< CUtlString > dirList;
		RecurseFileTree_r( dirPath, 0, dirList );
		for ( intp i=0; i<dirList.Count(); i++ )
		{
			// iterate each directory found
			tempList.Purge();
			tempList.EnsureCapacity( dirList.Count() );

			GetFileList( dirList[i].String(), pattern, tempList );

			intp start = fileList.AddMultipleToTail( tempList.Count() );
			for ( intp j=0; j<tempList.Count(); j++ )
			{
				fileList[start+j] = tempList[j];
			}
		}	
	}

	return fileList.Count();
}
