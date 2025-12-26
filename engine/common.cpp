//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "common.h"
#include "host.h"
#include "draw.h"
#include "zone.h"
#include "sys.h"
#include "edict.h"
#include "coordsize.h"
#include "characterset.h"
#include "bitbuf.h"
#include "posix_file_stream.h"
#include "traceinit.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "convar.h"
#include "gl_matsysiface.h"
#include "filesystem_init.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "tier0/icommandline.h"
#include "vstdlib/random.h"
#include "sys_dll.h"
#include "datacache/idatacache.h"
#include "matchmaking.h"
#include "tier1/KeyValues.h"
#include "vgui_baseui_interface.h"
#include "tier2/tier2.h"
#include "language.h"
#ifndef SWDS
#include "cl_steamauth.h"
#endif
#include "tier3/tier3.h"
#include "vgui/ILocalize.h"
#include "tier1/lzss.h"
#include <snappy.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Things in other C files.
#define MAX_LOG_DIRECTORIES	10000

bool com_ignorecolons = false; 

// wordbreak parsing set
static constexpr characterset_t g_BreakSet{"{}()'"}, g_BreakSetIncludingColons{"{}()':"};

char	com_token[COM_TOKEN_MAX_LENGTH];

/*
All of Quake's data access is through a hierarchical file system, but the contents of 
the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all
game directories.  The sys_* files pass this to host_init in engineparms->basedir.
This can be overridden with the "-basedir" command line parm to allow code
debugging in a different directory.  The base directory is
only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory
that all generated files (savegames, screenshots, demos, config files) will
be saved to.  This can be overridden with the "-game" command line parameter.
The game directory can never be changed while quake is executing.
This is a precacution against having a malicious server instruct clients
to write files over areas they shouldn't.

The "cache directory" is only used during development to save network bandwidth,
especially over ISDN / T1 lines.  If there is a cache directory
specified, when a file is found by the normal search path, it will be mirrored
into the cache directory, then opened there.

FIXME:
The file "parms.txt" will be read out of the game directory and appended to the
current command line arguments to allow different games to initialize startup
parms differently.  This could be used to add a "-sspeed 22050" for the high
quality sound edition.  Because they are added at the end, they will not override
an explicit setting on the original command line.
*/

/*
==============================
COM_ExplainDisconnection

==============================
*/
void COM_ExplainDisconnection( bool bPrint, PRINTF_FORMAT_STRING const char *fmt, ... )
{
	va_list		argptr;

	va_start (argptr, fmt); //-V2019 //-V2018
	V_vsprintf_safe(gszDisconnectReason, fmt, argptr);
	va_end (argptr);

	gfExtendedError = true;

	if ( bPrint )
	{
		if ( gszDisconnectReason[0] == '#' )
		{
			wchar_t formatStr[256];
			const wchar_t *wpchReason = g_pVGuiLocalize ? g_pVGuiLocalize->Find(gszDisconnectReason) : NULL;
			if ( wpchReason )
			{
				V_wcscpy_safe( formatStr, wpchReason );

				char conStr[256];
				g_pVGuiLocalize->ConvertUnicodeToANSI(formatStr, conStr);
				ConMsg( "%s\n", conStr );
			}
			else
				ConMsg( "%s\n", gszDisconnectReason );
		}
		else
		{
			ConMsg( "%s\n", gszDisconnectReason );
		}
	}
}

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
const char *COM_Parse ( IN_Z const char *data)
{
	return COM_Parse( data, com_token );
}


/*
==============
COM_Parse

Parse a token out of a string
==============
*/
const char *COM_Parse ( IN_Z const char *data, OUT_Z_CAP(tokenSize) char* token, size_t tokenSize )
{
	const bool hasToken{tokenSize != 0};

	if ( hasToken )
		token[0] = '\0';
	
	if ( !data || !hasToken )
		return nullptr;

	const characterset_t *breaks = !com_ignorecolons
		? &g_BreakSetIncludingColons
		: &g_BreakSet;

	size_t len = 0, maxLen = tokenSize - 1;
	unsigned char c;

// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return nullptr;                    // end of file;
		data++;
	}
	
// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (true)
		{
			c = *data++;

			if (c=='\"' || !c)
			{
				if (len <= maxLen)
				{
					token[len] = '\0';
				}
				else
				{
					AssertMsg( "%s buffer token is truncated, %zu bytes is not enough.", data, tokenSize );
					token[maxLen] = '\0';
				}
				return data;
			}

			if (len <= maxLen)
			{
				token[len] = c;
			}
			else
			{
				AssertMsg( "%s buffer token is truncated, %zu bytes is not enough.", data, tokenSize );
				token[maxLen] = '\0';
				return data;
			}
			len++;
		}
	}

// parse single characters
	if ( breaks->HasChar( c ) )
	{
		token[len] = c;
		len++;

		if (len <= maxLen)
		{
			token[len] = '\0';
		}
		else
		{
			AssertMsg( "%s buffer token is truncated, %zu bytes is not enough.", data, tokenSize );
			token[maxLen] = '\0';
			return data;
		}
		return data + 1;
	}

// parse a regular word
	do
	{
		if (len <= maxLen)
		{
			token[len] = c;
		}
		else
		{
			AssertMsg( "%s buffer token is truncated, %zu bytes is not enough.", data, tokenSize );
			token[maxLen] = '\0';
			return data;
		}
		data++;
		len++;
		c = *data;
		if ( breaks->HasChar( c ) )
			break;
	} while (c > 32);

	if (len <= maxLen)
	{
		token[len] = '\0';
	}
	else
	{
		AssertMsg( "%s buffer token is truncated, %zu bytes is not enough.", data, tokenSize );
		token[maxLen] = '\0';
		return data;
	}
	return data;
}

/*
==============
COM_AddNoise

Changes n random bits in a data block
==============
*/
void COM_AddNoise( INOUT_BYTECAP(number) unsigned char *data, int length, int number )
{
	for ( int i = 0; i < number; i++ )
	{
		int randomByte = RandomInt( 0, length-1 );
		int randomBit = RandomInt( 0, 7 );

		// get original data
		unsigned char dataByte = data[randomByte];

		// flip bit
		if ( dataByte & randomBit )
		{
			dataByte &= ~randomBit;
		}
		else
		{
			dataByte |= randomBit;
		}

		// write back
		data[randomByte] = dataByte;
	}
}

/*
==============
COM_Parse_Line

Parse a line out of a string
==============
*/
const char *COM_ParseLine ( IN_Z const char *data)
{
	return COM_ParseLine( data, com_token );
}


/*
==============
COM_Parse_Line

Parse a line out of a string
==============
*/
const char *COM_ParseLine ( IN_Z const char *data, OUT_Z_CAP(tokenSize) char* token, size_t tokenSize )
{
	const bool hasToken{tokenSize != 0};

	if ( hasToken )
		token[0] = '\0';
	
	if ( !data || !hasToken )
		return nullptr;

	size_t len = 0, maxLen = tokenSize - 1;
	char c = *data;

	// parse a line out of the data
	do
	{
		if (len <= maxLen)
		{
			token[len] = c;
		}
		else
		{
			AssertMsg( "%s buffer token is truncated, %zu bytes is not enough.", data, tokenSize );
			token[maxLen] = '\0';
			return data;
		}

		data++;
		len++;

		c = *data;
	} while ( ( c >= ' ' || c < 0 || c == '\t' ) && len < tokenSize - 1 );
	
	if (len <= maxLen)
	{
		token[len] = '\0';
	}
	else
	{
		AssertMsg( "%s buffer token is truncated, %zu bytes is not enough.", data, tokenSize );
		token[maxLen] = '\0';
		return data;
	}

	// end of file
	if (c == '\0')
		return nullptr;

	// eat whitespace (LF,CR,etc.) at the end of this line
	while ( (c = *data) < ' ' )
	{
		// end of file
		if (c == '\0')
			return nullptr;

		data++;
	}

	return data;
}


/*
==============
COM_TokenWaiting

Returns 1 if additional data is waiting to be processed on this line
==============
*/
int COM_TokenWaiting( IN_Z const char *buffer )
{
	const char *p = buffer;
	while ( *p && *p!='\n')
	{
		if ( !V_isspace( *p ) || V_isalnum( *p ) )
			return 1;

		p++;
	}

	return 0;
}


/*
============
tmpstr512

rotates through a bunch of string buffers of 512 bytes each
============
*/
char *tmpstr512()
{
	static char	string[32][512];
	static int	curstring = 0;
	curstring = ( curstring + 1 ) & 31;
	return string[curstring];  
}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
============
*/
char *va( PRINTF_FORMAT_STRING const char *format, ... )
{
	char* outbuf = tmpstr512();
	va_list argptr;
	va_start (argptr, format); //-V2019 //-V2018
	V_vsnprintf( outbuf, 512, format, argptr );
	va_end (argptr);
	return outbuf;
}

/*
============
vstr

prints a vector into a temporary string
bufffer.
============
*/
const char *vstr(Vector& v)
{
	char* outbuf = tmpstr512();
	Q_snprintf(outbuf, 512, "%.2f %.2f %.2f", v[0], v[1], v[2]);
	return outbuf;
}

char    com_basedir[MAX_OSPATH];
char    com_gamedir[MAX_OSPATH];

/*
==================
CL_CheckGameDirectory

Client side game directory change.
==================
*/
bool COM_CheckGameDirectory( IN_Z const char *gamedir )
{
	// Switch game directories if needed, or abort if it's not good.
	char szGD[ MAX_OSPATH ];

	if ( !gamedir || !gamedir[0] )
	{
		ConMsg( "Server didn't specify a gamedir, assuming no change\n" );
		return true;
	}

	// Rip out the current gamedir.
	Q_FileBase( com_gamedir, szGD );

	if ( Q_stricmp( szGD, gamedir ) )
	{
		// Changing game directories without restarting is not permitted any more
		ConMsg( "COM_CheckGameDirectory: game directories don't match (%s / %s)\n", szGD, gamedir );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Finds the file in the search path.
// Input  : *filename - 
//			*file - 
// Output : int
//-----------------------------------------------------------------------------
int COM_FindFile( const char *filename, FileHandle_t *file )
{
	Assert( file );
	int filesize = -1;
	*file = g_pFileSystem->Open( filename, "rb" );
	if ( *file )
	{
		filesize = g_pFileSystem->Size( *file );
	}
	return filesize;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//			*file - 
// Output : int
//-----------------------------------------------------------------------------
int COM_OpenFile(IN_Z const char *filename, FileHandle_t *file )
{
	return COM_FindFile( filename, file );
}

/*
============
COM_WriteFile

The filename will be prefixed by the current game directory
============
*/
void COM_WriteFile ( IN_Z const char *filename, IN_BYTECAP(len) void *data, int len)
{
	intp nameLen = V_strlen( filename ) + 2;
	char *pName = stackallocT( char, nameLen );

	Q_snprintf( pName, nameLen, "%s", filename);

	V_FixSlashes( pName );
	COM_CreatePath( pName );

	FileHandle_t handle = g_pFileSystem->Open( pName, "wb" );
	if ( !handle )
	{
		Warning ("COM_WriteFile: failed on %s\n", pName);
		return;
	}
	
	RunCodeAtScopeExit(g_pFileSystem->Close( handle ));
	
	g_pFileSystem->Write( data, len, handle );
}

/*
============
COM_CreatePath

Only used for CopyFile
============
*/
void COM_CreatePath (IN_Z const char *path)
{
	char temppath[1024];
	Q_strncpy(temppath, path, sizeof(temppath));
	V_StripFilename( temppath );

	Sys_mkdir( temppath );
}


/*
===========
COM_CopyFile

Copies a file from pSourcePath to pDestPath.
===========
*/
bool COM_CopyFile ( IN_Z const char *pSourcePath, IN_Z const char *pDestPath )
{
	// dimhotepus: Copy file by x32 times greater chunks for performance.
	constexpr unsigned bufferSize = 4096 * 32;
	std::unique_ptr<char[]>	buf = std::make_unique<char[]>( bufferSize );

	FileHandle_t in = g_pFileSystem->Open(pSourcePath, "rb");
	AssertMsg( in, "COM_CopyFile(): Input file '%s' failed to open", pSourcePath );

	if ( !in )
		return false;

	RunCodeAtScopeExit(g_pFileSystem->Close(in));

	// create directories up to the cache file
	COM_CreatePath( pDestPath );

	FileHandle_t out = g_pFileSystem->Open( pDestPath, "wb" );
	AssertMsg( out, "COM_CopyFile(): Output file '%s' failed to open", pDestPath );
	
	if ( !out )
	{
		return false;
	}

	RunCodeAtScopeExit(g_pFileSystem->Close(out));

	unsigned remaining = g_pFileSystem->Size( in );
	while ( remaining > 0 )
	{
		const unsigned count = remaining < bufferSize ? remaining : bufferSize;

		g_pFileSystem->Read( buf.get(), count, in );
		g_pFileSystem->Write( buf.get(), count, out );

		remaining -= count;
	}

	return true;
}


/*
===========
COM_ExpandFilename

Finds the file in the search path, copies over the name with the full path name.
This doesn't search in the pak file.
===========
*/
int COM_ExpandFilename( INOUT_Z_CAP(maxlength) char *filename, int maxlength )
{
	char expanded[MAX_OSPATH];

	if ( g_pFileSystem->GetLocalPath_safe( filename, expanded ) != NULL )
	{
		Q_strncpy( filename, expanded, maxlength );
		return 1;
	}

	if ( filename && filename[0] != '*' )
	{
		Warning ("COM_ExpandFilename: can't find %s\n", filename);
	}
	
	return 0;
}

/*
===========
COM_FileSize

Returns the size of the file only.
===========
*/
int COM_FileSize (IN_Z const char *filename)
{
	return g_pFileSystem->Size(filename);
}

//-----------------------------------------------------------------------------
// Purpose: Close file handle
// Input  : hFile - 
//-----------------------------------------------------------------------------
void COM_CloseFile( FileHandle_t hFile )
{
	g_pFileSystem->Close( hFile );
}
 

/*
============
COM_LoadFile

Filename are reletive to the quake directory.
Allways appends a 0 byte.
============
*/
cache_user_t *loadcache;
byte    *loadbuf;
int             loadsize;
byte *COM_LoadFile (IN_Z const char *path, int usehunk, int *pLength)
{
	FileHandle_t	hFile;
	byte			*buf = NULL;
	char			base[128];
	int             len;

	if (pLength)
	{
		*pLength = 0;
	}

// look for it in the filesystem or pack files
	len = COM_OpenFile( path, &hFile );
	if ( !hFile )
	{
		return NULL;
	}

	// Extract the filename base name for hunk tag
	Q_FileBase( path, base );

	unsigned bufSize = len + 1;

	switch ( usehunk )
	{
	case 1:
		buf = Hunk_AllocName<byte>(bufSize, base);
		break;
	case 2:
		AssertMsg( 0, "Temp alloc no longer supported\n" );
		break;
	case 3:
		AssertMsg( 0, "Cache alloc no longer supported\n" );
		break;
	case 4:
		{
			if (len+1 > loadsize)
				buf = (byte *)malloc(bufSize);
			else
				buf = loadbuf;
		}
		break;
	case 5:
		buf = (byte *)malloc(bufSize);  // YWB:  FIXME, this is evil.
		break;
	default:
		Sys_Error ("COM_LoadFile: bad usehunk");
	}

	if ( !buf ) 
	{
		Sys_Error ("COM_LoadFile: not enough space for %s", path);
		COM_CloseFile(hFile);	// exit here to prevent fault on oom (kdb)
		return NULL;			
	}
		
	g_pFileSystem->ReadEx( buf, bufSize, len, hFile );
	COM_CloseFile( hFile );

	((byte *)buf)[ len ] = 0;

	if ( pLength )
	{
		*pLength = len;
	}
	return buf;
}

/*
===============
COM_CopyFileChunk

===============
*/
void COM_CopyFileChunk( FileHandle_t dst, FileHandle_t src, int nSize )
{
	int   copysize = nSize;
	char  copybuf[COM_COPY_CHUNK_SIZE];

	while (copysize > COM_COPY_CHUNK_SIZE)
	{
		g_pFileSystem->Read ( copybuf, COM_COPY_CHUNK_SIZE, src );
		g_pFileSystem->Write( copybuf, COM_COPY_CHUNK_SIZE, dst );
		copysize -= COM_COPY_CHUNK_SIZE;
	}

	g_pFileSystem->Read ( copybuf, copysize, src );
	g_pFileSystem->Write( copybuf, copysize, dst );

	g_pFileSystem->Flush ( src );
	g_pFileSystem->Flush ( dst );
}

// uses malloc if larger than bufsize
byte *COM_LoadStackFile (IN_Z const char *path, OUT_BYTECAP(bufsize) void *buffer, int bufsize, int& filesize )
{
	loadbuf = (byte *)buffer;
	loadsize = bufsize;
	byte *buf = COM_LoadFile (path, 4, &filesize );
	return buf;
}

void COM_ShutdownFileSystem( void )
{
}

/*
================
COM_Shutdown

Remove the searchpaths
================
*/
void COM_Shutdown( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: allocates memory and copys source text
// Input  : *in - 
// Output : char *CopyString
//-----------------------------------------------------------------------------
char *COM_StringCopy(IN_Z const char *in)
{
	// dimhotepus: Use V_strdup for clarity.
	return V_strdup(in);
}

void COM_StringFree(IN_Z const char *in)
{
	delete [] in;
}


void COM_SetupLogDir( IN_Z const char *mapname )
{
	char gameDir[MAX_OSPATH];
	COM_GetGameDir( gameDir );

	// Blat out the all directories in the LOGDIR path
	g_pFileSystem->RemoveSearchPath( NULL, "LOGDIR" );

	// set the log directory
	if ( mapname && CommandLine()->FindParm("-uselogdir") )
	{
		int i;
		char sRelativeLogDir[MAX_PATH];
		for ( i = 0; i < MAX_LOG_DIRECTORIES; i++ )
		{
			Q_snprintf( sRelativeLogDir, sizeof( sRelativeLogDir ), "logs/%s/%04i", mapname, i );
			if ( !g_pFileSystem->IsDirectory( sRelativeLogDir, "GAME" ) )
				break;
		}

		// Loop at max
		if ( i == MAX_LOG_DIRECTORIES )
		{
			i = 0;	
			Q_snprintf( sRelativeLogDir, sizeof( sRelativeLogDir ), "logs/%s/%04i", mapname, i );
		}

		// Make sure the directories we need exist.
		g_pFileSystem->CreateDirHierarchy( sRelativeLogDir, "GAME" );	

		{
			static bool pathsetup = false;

			if ( !pathsetup )
			{
				pathsetup = true;

				// Set the search path
				char sLogDir[MAX_PATH];
				Q_snprintf( sLogDir, sizeof( sLogDir ), "%s/%s", gameDir, sRelativeLogDir );
				g_pFileSystem->AddSearchPath( sLogDir, "LOGDIR" );
			}
		}
	}
	else
	{
		// Default to the base game directory for logs.
		g_pFileSystem->AddSearchPath( gameDir, "LOGDIR" );
	}
}

/*
================
COM_GetModDirectory - return the final directory in the game dir (i.e "cstrike", "hl2", rather than c:\blah\cstrike )
================
*/
const char *COM_GetModDirectory()
{
	static char modDir[MAX_PATH] = {};
	if ( Q_isempty( modDir ) )
	{
		const char *gamedir = CommandLine()->ParmValue("-game", CommandLine()->ParmValue( "-defaultgamedir", "hl2" ) );
		V_strcpy_safe( modDir, gamedir );
		if ( strchr( modDir, '/' ) || strchr( modDir, '\\' ) )
		{
			V_StripLastDir( modDir );
			intp dirlen = Q_strlen( modDir );
			Q_strncpy( modDir, gamedir + dirlen, sizeof(modDir) - dirlen );
		}
	}

	return modDir;
}


/*
================
Return if we should load content from the _hd folder for this mod
This logic needs to match with the gameui/OptionsSubVideo.cpp code
================
*/
bool BLoadHDContent( const char *pchModDir, const char *pchBaseDir )
{
	char szModSteamInfPath[ 1024 ];
	V_ComposeFileName( pchModDir, "game_hd.txt", szModSteamInfPath );

	char szFullPath[ 1024 ];
	V_MakeAbsolutePath( szFullPath, szModSteamInfPath, pchBaseDir );

	auto [f, errc] = se::posix::posix_file_stream_factory::open(szFullPath, "rb");
	return !errc;
}


extern void Host_CheckGore( void );

/*
================
COM_InitFilesystem
================
*/
void COM_InitFilesystem( const char *pFullModPath )
{
	CFSSearchPathsInit initInfo;

#ifndef SWDS	
	{
		static char language[128];
		language[0] = 0;

		// There are two language at play here.  The Audio language which is controled by the
		// properties on the game itself in Steam (at least for now).  And the language Steam is set to.
		// Under Windows the text in the game is controled by the language Steam is set in, but the audio
		// is controled by the language set in the game's properties which we can get from Steam3Client

		// A command line override for audio language has also been added.
		// -audiolanguage <language>
		// User must have the .vpk files for the language installed though in order to use the command line switch
		
		if ( Steam3Client().SteamApps() )
		{
			// use -audiolanguage command line to override audio language, otherwise take language from steam
			V_strcpy_safe(language, CommandLine()->ParmValue("-audiolanguage", Steam3Client().SteamApps()->GetCurrentGameLanguage()));
		}
		else
		{
			// still allow command line override even when not running steam
			if (CommandLine()->CheckParm("-audiolanguage"))
			{
				V_strcpy_safe(language, CommandLine()->ParmValue("-audiolanguage", "english"));
			}
		}

		if ( ( !Q_isempty(language) ) && ( Q_stricmp(language, "english") ) )
		{
			initInfo.m_pLanguage = language;
		}
	}
#endif
	
	initInfo.m_pFileSystem = g_pFileSystem;
	initInfo.m_pDirectoryName = pFullModPath;
	if ( !initInfo.m_pDirectoryName )
	{
		initInfo.m_pDirectoryName = GetCurrentGame();
	}

	Host_CheckGore();

	initInfo.m_bLowViolence = g_bLowViolence;
	initInfo.m_bMountHDContent = BLoadHDContent( initInfo.m_pDirectoryName, GetBaseDirectory() );

	// Load gameinfo.txt and setup all the search paths, just like the tools do.
	if ( FileSystem_LoadSearchPaths( initInfo ) != FS_OK )
	{
		Error( "Unable to load search paths from gameinfo.txt\n" );
	}

	// The mod path becomes com_gamedir.
	V_MakeAbsolutePath( com_gamedir, initInfo.m_ModPath );

	// Set com_basedir.
	V_strcpy_safe ( com_basedir, GetBaseDirectory() ); // the "root" directory where hl2.exe is
	Q_strlower( com_basedir );
	Q_FixSlashes( com_basedir );
	
#if !defined( SWDS ) && !defined( DEDICATED )
	EngineVGui()->SetVGUIDirectories();
#endif

	// Set LOGDIR to be something reasonable
	COM_SetupLogDir( NULL );

//	g_pFileSystem->PrintSearchPaths();

}

const char *COM_DXLevelToString( int dxlevel )
{
	bool bHalfPrecision = false;

	const char *pShaderDLLName = g_pMaterialSystemHardwareConfig->GetShaderDLLName();
	if( pShaderDLLName && Q_stristr( pShaderDLLName, "nvfx" ) )
	{
		bHalfPrecision = true;
	}
	
	if( CommandLine()->CheckParm( "-dxlevel" ) )
	{
		switch( dxlevel )
		{
		case 0:
			return "default";
		case 60:
			return "6.0";
		case 70:
			return "7.0";
		case 80:
			return "8.0";
		case 81:
			return "8.1";
		case 82:
			return !bHalfPrecision
				? "8.1 with some 9.0 (full-precision)"
				: "8.1 with some 9.0 (half-precision)";
		case 90:
			return !bHalfPrecision
				? "9.0 [sm2] (full-precision)"
				: "9.0 [sm2] (half-precision)";
		case 92:
			return !bHalfPrecision
				? "9.0 [sm2] togl (full-precision)"
				: "9.0 [sm2] togl (half-precision)";
		case 95:
			return !bHalfPrecision
				? "9.0 [sm3] (full-precision)"
				: "9.0 [sm3] (half-precision)";
		case 100:
			return !bHalfPrecision
				? "10.0 [sm4] (full-precision)"
				: "10.0 [sm4] (half-precision)";
		case 110:
			return !bHalfPrecision
				? "11.0 [sm5] (full-precision)"
				: "11.0 [sm5] (half-precision)";
		case 111:
			return !bHalfPrecision
				? "11.1 [sm5] (full-precision)"
				: "11.1 [sm5] (half-precision)";
		case 120:
			return !bHalfPrecision
				? "12.0 [sm6] (full-precision)"
				: "12.0 [sm6] (half-precision)";
		default:
			return "UNKNOWN";
		}
	}
	else
	{
		switch( dxlevel )
		{
		case 60:
			return "gamemode - 6.0";
		case 70:
			return "gamemode - 7.0";
		case 80:
			return "gamemode - 8.0";
		case 81:
			return "gamemode - 8.1";
		case 82:
			return !bHalfPrecision
				? "gamemode - 8.1 with some 9.0 (full-precision)"
				: "gamemode - 8.1 with some 9.0 (half-precision)";
		case 90:
			return !bHalfPrecision
				? "gamemode - 9.0 [sm2] (full-precision)"
				: "gamemode - 9.0 [sm2] (half-precision)";
		case 92:
			return !bHalfPrecision
				? "gamemode - 9.0 [sm2] togl (full-precision)"
				: "gamemode - 9.0 [sm2] togl (half-precision)";
		case 95:
			return !bHalfPrecision
				? "gamemode - 9.0 [sm3] (full-precision)"
				: "gamemode - 9.0 [sm3] (half-precision)";
		case 100:
			return !bHalfPrecision
				? "gamemode - 10.0 [sm4] (full-precision)"
				: "gamemode - 10.0 [sm4] (half-precision)";
		case 110:
			return !bHalfPrecision
				? "gamemode - 11.0 [sm5] (full-precision)"
				: "gamemode - 11.0 [sm5] (half-precision)";
		case 111:
			return !bHalfPrecision
				? "gamemode - 11.1 [sm5] (full-precision)"
				: "gamemode - 11.1 [sm5] (half-precision)";
		case 120:
			return !bHalfPrecision
				? "gamemode - 12.0 [sm6] (full-precision)"
				: "gamemode - 12.0 [sm6] (half-precision)";
		default:
			return "gamemode";
		}
	}
}

const char *COM_FormatSeconds( int seconds )
{
	static char string[64];

	int hours = 0;
	int minutes = seconds / 60;

	if ( minutes > 0 )
	{
		seconds -= (minutes * 60);
		hours = minutes / 60;

		if ( hours > 0 )
		{
			minutes -= (hours * 60);
		}
	}
	
	if ( hours > 0 )
	{
		V_sprintf_safe( string, "%2i:%02i:%02i", hours, minutes, seconds );
	}
	else
	{
		V_sprintf_safe( string, "%02i:%02i", minutes, seconds );
	}

	return string;
}

// Non-VarArgs version
void COM_LogString( char const *pchFile, char const *pchString )
{
	if ( !g_pFileSystem )
	{
		Assert( 0 );
		return;
	}

	const char *pfilename = pchFile ? pchFile : "hllog.txt";
	FileHandle_t fp = g_pFileSystem->Open( pfilename, "a+t");
	if (fp)
	{
		RunCodeAtScopeExit(g_pFileSystem->Close(fp));

		g_pFileSystem->Write( pchString, V_strlen( pchString), fp );
	}
}

void COM_Log( const char *pszFile, PRINTF_FORMAT_STRING const char *fmt, ...) FMTFUNCTION( 2, 3 )
{
	if ( !g_pFileSystem )
	{
		Assert( 0 );
		return;
	}

	va_list		argptr;
	char		string[8192];

	va_start (argptr,fmt); //-V2019 //-V2018
	V_vsprintf_safe(string, fmt, argptr);
	va_end (argptr);

	COM_LogString( pszFile, string );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename1 - 
//			*filename2 - 
//			*iCompare - 
// Output : int
//-----------------------------------------------------------------------------
int COM_CompareFileTime( IN_Z const char *filename1, IN_Z const char *filename2, int *iCompare)
{
	int bRet = 0;
	if ( iCompare )
	{
		*iCompare = 0;
	}

	if (filename1 && filename2)
	{
		// dimhotepus: long -> time_t
		time_t ft1 = g_pFileSystem->GetFileTime( filename1 );
		time_t ft2 = g_pFileSystem->GetFileTime( filename2 );

		if ( iCompare )
		{
			*iCompare = Sys_CompareFileTime( ft1,  ft2 );
		}
		bRet = 1;
	}

	return bRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szGameDir - 
//-----------------------------------------------------------------------------
void COM_GetGameDir(OUT_Z_CAP(maxlen) char *szGameDir, int maxlen)
{
	if (!szGameDir) return;
	Q_strncpy(szGameDir, com_gamedir, maxlen );
}

//-----------------------------------------------------------------------------
// Purpose: Parse a token from a file stream
// Input  : *data - 
//			*token - 
// Output : char
//-----------------------------------------------------------------------------
const char *COM_ParseFile( IN_Z const char *data, OUT_Z_CAP(maxtoken) char *token, intp maxtoken )
{
	return COM_Parse(data, token, maxtoken);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void COM_Init
//-----------------------------------------------------------------------------
void COM_Init ( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool COM_IsValidPath(IN_Z const char *pszFilename )
{
	if ( !pszFilename )
	{
		return false;
	}

	if ( Q_isempty( pszFilename )    ||
		Q_strstr( pszFilename, "\\\\" ) ||	// to protect network paths
		Q_strstr( pszFilename, ":" )    || // to protect absolute paths
		Q_strstr( pszFilename, ".." ) ||   // to protect relative paths
		Q_strstr( pszFilename, "\n" ) ||   // CFileSystem_Stdio::FS_fopen doesn't allow this
		Q_strstr( pszFilename, "\r" ) )    // CFileSystem_Stdio::FS_fopen doesn't allow this
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool COM_IsValidLogFilename( IN_Z const char *pszFilename )
{
	if ( !pszFilename || !pszFilename[0] )
		return false;

	if ( V_stristr( pszFilename, "   " ) || V_stristr( pszFilename, "\t" ) ) // don't multiple spaces or tab
		return false;

	const char *extension = V_strrchr( pszFilename, '.' );
	if ( extension )
	{
		if ( Q_stricmp( extension, ".log" ) && Q_stricmp( extension, ".txt" ) ) // must use .log or .txt if an extension is specified
			return false;

		if ( extension == pszFilename ) // bad filename (just an extension)
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
unsigned int COM_GetIdealDestinationCompressionBufferSize_Snappy( unsigned int uncompressedSize )
{
	// 4 for the ID, plus whatever Snappy says it would need.
	return 4 + snappy::MaxCompressedLength( uncompressedSize );
}

//-----------------------------------------------------------------------------
void *COM_CompressBuffer_Snappy( IN_BYTECAP(sourceLen) const void *source, unsigned int sourceLen, unsigned int *compressedLen, unsigned int maxCompressedLen )
{
	Assert( source );
	Assert( compressedLen );

	// Allocate a buffer big enough to hold the worst case.
	unsigned nMaxCompressedSize = COM_GetIdealDestinationCompressionBufferSize_Snappy( sourceLen );
	char *pCompressed = (char*)malloc( nMaxCompressedSize );
	if ( pCompressed == NULL )
		return NULL;

	// Do the compression
	*(uint32 *)pCompressed = SNAPPY_ID;
	size_t compressed_length;
	snappy::RawCompress( (const char *)source, sourceLen, pCompressed + sizeof(uint32), &compressed_length );
	compressed_length += 4;
	Assert( compressed_length <= nMaxCompressedSize );

	// Check if this result is OK
	if ( maxCompressedLen != 0 && compressed_length > maxCompressedLen )
	{
		free( pCompressed );
		return NULL;
	}

	*compressedLen = compressed_length;
	return pCompressed;
}

//-----------------------------------------------------------------------------
bool COM_BufferToBufferCompress_Snappy( OUT_BYTECAP(*destLen) void *dest, unsigned int *destLen, IN_BYTECAP(sourceLen) const void *source, unsigned int sourceLen )
{
	Assert( dest );
	Assert( destLen );
	Assert( source );

	// Check if we need to use a temporary buffer
	unsigned nMaxCompressedSize = COM_GetIdealDestinationCompressionBufferSize_Snappy( sourceLen );
	unsigned compressedLen = *destLen;
	if ( compressedLen < nMaxCompressedSize )
	{

		// Yep.  Use the other function to allocate the buffer of the right size and comrpess into it
		void *temp = COM_CompressBuffer_Snappy( source, sourceLen, &compressedLen, compressedLen );
		if ( temp == NULL )
			return false;

		// Copy over the data
		V_memcpy( dest, temp, compressedLen );
		*destLen = compressedLen;
		free( temp );
		return true;
	}

	// We have room and should be able to compress directly
	*(uint32 *)dest = SNAPPY_ID;
	size_t compressed_length;
	snappy::RawCompress( (const char *)source, sourceLen, (char *)dest + sizeof(uint32), &compressed_length );
	compressed_length += 4;
	Assert( compressed_length <= nMaxCompressedSize );
	*destLen = compressed_length;
	return true;
}

//-----------------------------------------------------------------------------
unsigned COM_GetIdealDestinationCompressionBufferSize_LZSS( unsigned int uncompressedSize )
{
	// Our LZSS compressor doesn't need any extra space because it will stop and fail
	// as soon as it figures out it's unable to reduce the size of the data by more than
	// 32 bytes
	return uncompressedSize;
}

//-----------------------------------------------------------------------------
void *COM_CompressBuffer_LZSS( IN_BYTECAP(sourceLen) const void *source, unsigned int sourceLen, OUT_BYTECAP(maxCompressedLen) unsigned int *compressedLen, unsigned int maxCompressedLen )
{
	Assert( source );
	Assert( compressedLen );

	CLZSS s;
	unsigned int uCompressedLen = 0;
	byte *pbOut = s.Compress( static_cast<const byte *>(source), sourceLen, &uCompressedLen );
	if ( pbOut && uCompressedLen > 0 && ( uCompressedLen <= maxCompressedLen || maxCompressedLen == 0 ) )
	{
		*compressedLen = uCompressedLen;
		return pbOut;
	}

	free( pbOut );

	// dimhotepus: 0 if error.
	*compressedLen = 0;
	return NULL;
}

//-----------------------------------------------------------------------------
bool COM_BufferToBufferCompress_LZSS( OUT_BYTECAP(*destLen) void *dest, unsigned int *destLen, IN_BYTECAP(sourceLen) const void *source, unsigned int sourceLen )
{
	Assert( dest );
	Assert( destLen );
	Assert( source );

	CLZSS s;
	unsigned int uCompressedLen = 0;
	if ( !s.CompressNoAlloc( static_cast<const byte *>(source), sourceLen, static_cast<unsigned char *>(dest), &uCompressedLen ) )
		return false;

	*destLen = uCompressedLen;
	return true;
}

//-----------------------------------------------------------------------------
int COM_GetUncompressedSize( IN_BYTECAP(compressedLen) const void *compressed, unsigned int compressedLen )
{
	const lzss_header_t *pHeader = static_cast<const lzss_header_t *>(compressed);

	// Check for our own LZSS compressed data
	if ( ( compressedLen >= sizeof(lzss_header_t) ) && pHeader->id == LZSS_ID )
		return LittleLong( pHeader->actualSize );

	// Check for Snappy compressed
	if ( compressedLen > sizeof(pHeader->id) && pHeader->id == SNAPPY_ID )
	{
		size_t snappySize;
		if ( snappy::GetUncompressedLength( (const char *)compressed + sizeof(pHeader->id), compressedLen-sizeof(pHeader->id), &snappySize ) )
			return (int)snappySize;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Generic buffer decompression from source into dest
//-----------------------------------------------------------------------------
bool COM_BufferToBufferDecompress( OUT_BYTECAP(*destLen) void *dest, unsigned int *destLen, IN_BYTECAP(sourceLen) const void *source, unsigned int sourceLen )
{
	int nDecompressedSize = COM_GetUncompressedSize( source, sourceLen );
	if ( nDecompressedSize >= 0 )
	{

		// Check buffer size
		if ( (unsigned)nDecompressedSize > *destLen )
		{
			Warning( "NET_BufferToBufferDecompress with improperly sized dest buffer (%u in, %u needed)\n", *destLen, nDecompressedSize );
			return false;
		}

		const lzss_header_t *pHeader = static_cast<const lzss_header_t *>(source);
		if ( pHeader->id == LZSS_ID )
		{
			CLZSS s;
			int nActualDecompressedSize = s.SafeUncompress( static_cast<const byte *>(source), static_cast<byte *>(dest), *destLen );
			if ( nActualDecompressedSize != nDecompressedSize )
			{
				Warning( "NET_BufferToBufferDecompress: header said %d bytes would be decompressed, but we LZSS decompressed %d\n", nDecompressedSize, nActualDecompressedSize );
				return false;
			}
			*destLen = nDecompressedSize;
			return true;
		}

		if ( pHeader->id == SNAPPY_ID )
		{
			if ( !snappy::RawUncompress( static_cast<const char *>(source) + 4, sourceLen - 4, static_cast<char *>(dest) ) )
			{
				Warning( "NET_BufferToBufferDecompress: Snappy decompression failed\n" );
				return false;
			}
			*destLen = nDecompressedSize;
			return true;
		}

		// Mismatch between this routine and COM_GetUncompressedSize
		AssertMsg( false, "Unknown compression type?" );
		return false;
	}
	else
	{
		if ( sourceLen > *destLen )
		{
			Warning( "NET_BufferToBufferDecompress with improperly sized dest buffer (%u in, %u needed)\n", *destLen, sourceLen );
			return false;
		}

		V_memcpy( dest, source, sourceLen );
		*destLen = sourceLen;
	}

	return true;
}
