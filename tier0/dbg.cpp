// Copyright Valve Corporation, All rights reserved.

#include "pch_tier0.h"
#include "tier0/minidump.h"

#if defined( _WIN32 ) && !defined( _X360 )
#include "tier0/valve_off.h"
#pragma comment(lib,"user32.lib")	// For MessageBox
#endif

#include <cassert>
#include "Color.h"
#include "tier0/dbg.h"
#include "tier0/threadtools.h"
#include "tier0/icommandline.h"
#if defined( _X360 )
#include "xbox/xbox_console.h"
#endif

#include "tier0/etwprof.h"

#ifndef STEAM
#define PvRealloc realloc
#define PvAlloc malloc
#define PvFree free
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// internal structures
//-----------------------------------------------------------------------------
enum 
{ 
	MAX_GROUP_NAME_LENGTH = 48 
};

struct SpewGroup_t
{
	tchar	m_GroupName[MAX_GROUP_NAME_LENGTH];
	int		m_Level;
};

// Skip forward past the directory
static const char *SkipToFname( const tchar* pFile )
{
	if ( pFile == NULL )
		return "unknown";
	const tchar* pSlash = _tcsrchr( pFile, '\\' );
	const tchar* pSlash2 = _tcsrchr( pFile, '/' );
	if (pSlash < pSlash2) pSlash = pSlash2;
	return pSlash ? pSlash + 1: pFile;
}


//-----------------------------------------------------------------------------
DBG_INTERFACE SpewRetval_t DefaultSpewFunc( SpewType_t type, const tchar *pMsg )
{
#ifdef _X360
	if ( XBX_IsConsoleConnected() )
	{
		// send to console
		XBX_DebugString( XMAKECOLOR( 0,0,0 ), pMsg );
	}
	else
#endif
	{
		_tprintf( _T("%s"), pMsg );
#ifdef _WIN32
		Plat_DebugString( pMsg );
#endif
	}
	if ( type == SPEW_ASSERT )
	{
#ifndef WIN32
		// Non-win32
		bool bRaiseOnAssert = getenv( "RAISE_ON_ASSERT" ) || !!CommandLine()->FindParm( "-raiseonassert" );
		return bRaiseOnAssert ? SPEW_DEBUGGER : SPEW_CONTINUE;
#elif defined( _DEBUG )
		// Win32 debug
		return SPEW_DEBUGGER;
#else
		// Win32 release
		bool bRaiseOnAssert = !!CommandLine()->FindParm( "-raiseonassert" );
		return bRaiseOnAssert ? SPEW_DEBUGGER : SPEW_CONTINUE;
#endif
	}
	else if ( type == SPEW_ERROR )
		return SPEW_ABORT;
	else
		return SPEW_CONTINUE;
}

//-----------------------------------------------------------------------------
DBG_INTERFACE SpewRetval_t DefaultSpewFuncAbortOnAsserts( SpewType_t type, const tchar *pMsg )
{
	SpewRetval_t r = DefaultSpewFunc( type, pMsg );
	if ( type == SPEW_ASSERT )
		r = SPEW_ABORT;
	return r;
}


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
static SpewOutputFunc_t   s_SpewOutputFunc = DefaultSpewFunc;

static AssertFailedNotifyFunc_t	s_AssertFailedNotifyFunc = NULL;

static const tchar*	s_pFileName;
static int			s_Line;
static SpewType_t	s_SpewType;

static SpewGroup_t* s_pSpewGroups = 0;
static int			s_GroupCount = 0;
static int			s_DefaultLevel = 0;
#if !defined( _X360 )
static Color		s_DefaultOutputColor( 255, 255, 255, 255 );
#else
static Color		s_DefaultOutputColor( 0, 0, 0, 255 );
#endif

// Only useable from within a spew function
struct SpewInfo_t
{
	const Color*	m_pSpewOutputColor;
	const tchar*	m_pSpewOutputGroup;
	int				m_nSpewOutputLevel;
};

static thread_local SpewInfo_t *g_pSpewInfo;


// Standard groups
static const tchar* s_pDeveloper = _T("developer");
static const tchar* s_pConsole = _T("console");
static const tchar* s_pNetwork = _T("network");

enum StandardSpewGroup_t
{
	GROUP_DEVELOPER = 0,
	GROUP_CONSOLE,
	GROUP_NETWORK,

	GROUP_COUNT,
};

static int s_pGroupIndices[GROUP_COUNT] = { -1, -1, -1 };
static const char *s_pGroupNames[GROUP_COUNT] = { s_pDeveloper, s_pConsole, s_pNetwork };


//-----------------------------------------------------------------------------
// Spew output management.
//-----------------------------------------------------------------------------
void SpewOutputFunc( SpewOutputFunc_t func )
{
	s_SpewOutputFunc = func ? func : DefaultSpewFunc;
}

SpewOutputFunc_t GetSpewOutputFunc( void )
{
	if( s_SpewOutputFunc )
		return s_SpewOutputFunc;
	return DefaultSpewFunc;
}

void _ExitOnFatalAssert( const tchar* pFile, int line )
{
	(void)_SpewMessage( _T("Fatal assert failed: %s, line %d.  Application exiting.\n"), pFile, line );

	// only write out minidumps if we're not in the debugger
	if ( !Plat_IsInDebugSession() )
	{
		char rgchSuffix[512];
		_snprintf( rgchSuffix, sizeof(rgchSuffix), "fatalassert_%s_%d", SkipToFname( pFile ), line );
		WriteMiniDump( rgchSuffix );
	}

	DevMsg( 1, _T("_ExitOnFatalAssert\n") );
	exit( EXIT_FAILURE );
}


//-----------------------------------------------------------------------------
// Templates to assist in validating pointers:
//-----------------------------------------------------------------------------


DBG_INTERFACE void _AssertValidReadPtr( void* ptr, int count/* = 1*/ )
{
	Assert( !count || ptr );
}

DBG_INTERFACE void _AssertValidWritePtr( void* ptr, int count/* = 1*/ )
{
	Assert( !count || ptr );
}

DBG_INTERFACE void _AssertValidReadWritePtr( void* ptr, int count/* = 1*/ )
{
	Assert( !count || ptr );
}

#undef AssertValidStringPtr
DBG_INTERFACE void AssertValidStringPtr( const tchar* ptr, int /* = 0xFFFFFF */ )
{
	Assert( ptr );
}

//-----------------------------------------------------------------------------
// Should be called only inside a SpewOutputFunc_t, returns groupname, level, color
//-----------------------------------------------------------------------------
const tchar* GetSpewOutputGroup( void )
{
	SpewInfo_t *pSpewInfo = g_pSpewInfo;
	assert( pSpewInfo );
	if ( pSpewInfo )
		return pSpewInfo->m_pSpewOutputGroup;
	return NULL;
}

int GetSpewOutputLevel( void )
{
	SpewInfo_t *pSpewInfo = g_pSpewInfo;
	assert( pSpewInfo );
	if ( pSpewInfo )
		return pSpewInfo->m_nSpewOutputLevel;
	return -1;
}

const Color* GetSpewOutputColor( void )
{
	SpewInfo_t *pSpewInfo = g_pSpewInfo;
	assert( pSpewInfo );
	if ( pSpewInfo )
		return pSpewInfo->m_pSpewOutputColor;
	return &s_DefaultOutputColor;
}


//-----------------------------------------------------------------------------
// Spew functions
//-----------------------------------------------------------------------------
DBG_INTERFACE void  _SpewInfo( SpewType_t type, const tchar* pFile, int line )
{
	// Only grab the file name. Ignore the path.
	s_pFileName = SkipToFname( pFile );
	s_Line = line;
	s_SpewType = type;
}


static SpewRetval_t _SpewMessage( SpewType_t spewType, const char *pGroupName, int nLevel, const Color *pColor, const tchar* pMsgFormat, va_list args )
{
	tchar pTempBuffer[5020];

	assert( _tcslen( pMsgFormat ) < sizeof( pTempBuffer) ); // check that we won't artifically truncate the string

	/* Printf the file and line for warning + assert only... */
	int len = 0;
	if ( spewType == SPEW_ASSERT )
	{
		len = _sntprintf( pTempBuffer, sizeof( pTempBuffer ) - 1, _T("%s (%d) : "), s_pFileName, s_Line );
	}

	if ( len == -1 )
		return SPEW_ABORT;
	
	/* Create the message.... */
	int val= _vsntprintf( &pTempBuffer[len], sizeof( pTempBuffer ) - len - 1, pMsgFormat, args );
	if ( val == -1 )
		return SPEW_ABORT;

	len += val;
	assert( len * sizeof(*pMsgFormat) < sizeof(pTempBuffer) ); /* use normal assert here; to avoid recursion. */

	// Add \n for warning and assert
	if ( spewType == SPEW_ASSERT )
	{
		len += _stprintf( &pTempBuffer[len], _T("\n") ); 
	}
	
	assert( len < sizeof(pTempBuffer)/sizeof(pTempBuffer[0]) - 1 ); /* use normal assert here; to avoid recursion. */
	assert( s_SpewOutputFunc );
	
	/* direct it to the appropriate target(s) */
	SpewRetval_t ret;
	assert( g_pSpewInfo == NULL );
	SpewInfo_t spewInfo =
	{
		pColor,
		pGroupName,
		nLevel
	};

	g_pSpewInfo = &spewInfo;
	ret = s_SpewOutputFunc( spewType, pTempBuffer );
	g_pSpewInfo = (int)NULL;

	switch (ret)
	{
// Asserts put the break into the macro so it occurs in the right place
	case SPEW_DEBUGGER:
		if ( spewType != SPEW_ASSERT )
		{
			DebuggerBreak();
		}
		break;
		
	case SPEW_ABORT:
	{
		exit(1);
	}

	// dimhotepus: Do nothing.
	case SPEW_CONTINUE:
		break;
	}

	return ret;
}

#include "tier0/valve_off.h"

FORCEINLINE SpewRetval_t _SpewMessage( SpewType_t spewType, const tchar* pMsgFormat, va_list args )
{
	return _SpewMessage( spewType, "", 0, &s_DefaultOutputColor, pMsgFormat, args );
}


//-----------------------------------------------------------------------------
// Find a group, return true if found, false if not. Return in ind the
// index of the found group, or the index of the group right before where the
// group should be inserted into the list to maintain sorted order.
//-----------------------------------------------------------------------------
bool FindSpewGroup( const tchar* pGroupName, int* pInd )
{
	int s = 0;
	if (s_GroupCount)
	{
		int e = (int)(s_GroupCount - 1);
		while ( s <= e )
		{
			int m = (s+e) >> 1;
			int cmp = _tcsicmp( pGroupName, s_pSpewGroups[m].m_GroupName );
			if ( !cmp )
			{
				*pInd = m;
				return true;
			}
			if ( cmp < 0 )
				e = m - 1;
			else
				s = m + 1;
		}
	}
	*pInd = s;
	return false;
}

//-----------------------------------------------------------------------------
// True if -hushasserts was passed on command line.
//-----------------------------------------------------------------------------
bool HushAsserts()
{
#ifdef DBGFLAG_ASSERT
	static bool s_bHushAsserts = !!CommandLine()->FindParm( "-hushasserts" );
	return s_bHushAsserts;
#else
	return true;
#endif
}

//-----------------------------------------------------------------------------
// Tests to see if a particular spew is active
//-----------------------------------------------------------------------------
bool IsSpewActive( const tchar* pGroupName, int level )
{
	// If we don't find the spew group, use the default level.
	int ind;
	if ( FindSpewGroup( pGroupName, &ind ) )
		return s_pSpewGroups[ind].m_Level >= level;
	else
		return s_DefaultLevel >= level;
}

inline bool IsSpewActive( StandardSpewGroup_t group, int level )
{
	if ( static_cast<unsigned>(group) >= std::size(s_pGroupIndices) )
	{
		AssertMsg( static_cast<int>(group) >= sizeof(s_pGroupIndices), "Group index is out of range." );
		return false;
	}

	// If we don't find the spew group, use the default level.
	if ( s_pGroupIndices[group] >= 0 )
		return s_pSpewGroups[ s_pGroupIndices[group] ].m_Level >= level;
	return s_DefaultLevel >= level;
}

SpewRetval_t  _SpewMessage( PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... ) FMTFUNCTION( 1, 2 )
{
	va_list args;
	va_start( args, pMsgFormat );
	SpewRetval_t ret = _SpewMessage( s_SpewType, pMsgFormat, args );
	va_end(args);
	return ret;
}

SpewRetval_t _DSpewMessage( const tchar *pGroupName, int level, PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... )
{
	if( !IsSpewActive( pGroupName, level ) )
		return SPEW_CONTINUE;

	va_list args;
	va_start( args, pMsgFormat );
	SpewRetval_t ret = _SpewMessage( s_SpewType, pGroupName, level, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
	return ret;
}

DBG_INTERFACE SpewRetval_t ColorSpewMessage( SpewType_t type, const Color *pColor, PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... )
{
	va_list args;
	va_start( args, pMsgFormat );
	SpewRetval_t ret = _SpewMessage( type, "", 0, pColor, pMsgFormat, args );
	va_end(args);
	return ret;
}

void Msg( PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... )
{
	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_MESSAGE, pMsgFormat, args );
	va_end(args);
}

void DMsg( const tchar *pGroupName, int level, PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( pGroupName, level ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_MESSAGE, pGroupName, level, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}

void MsgV( PRINTF_FORMAT_STRING const tchar *pMsg, va_list arglist )
{
	_SpewMessage( SPEW_MESSAGE, pMsg, arglist );
}


void Warning( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_WARNING, pMsgFormat, args );
	va_end(args);
}

void DWarning( const tchar *pGroupName, int level, PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( pGroupName, level ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_WARNING, pGroupName, level, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}

void WarningV( PRINTF_FORMAT_STRING const tchar *pMsg, va_list arglist )
{
	_SpewMessage( SPEW_WARNING, pMsg, arglist );
}


void Log( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_LOG, pMsgFormat, args );
	va_end(args);
}

void DLog( const tchar *pGroupName, int level, PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( pGroupName, level ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_LOG, pGroupName, level, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}

void LogV( PRINTF_FORMAT_STRING const tchar *pMsg, va_list arglist )
{
	_SpewMessage( SPEW_LOG, pMsg, arglist );
}


void Error( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_ERROR, pMsgFormat, args );
	va_end(args);
}

void ErrorV( PRINTF_FORMAT_STRING const tchar *pMsg, va_list arglist )
{
	_SpewMessage( SPEW_ERROR, pMsg, arglist );
}

//-----------------------------------------------------------------------------
// A couple of super-common dynamic spew messages, here for convenience 
// These looked at the "developer" group, print if it's level 1 or higher 
//-----------------------------------------------------------------------------
void DevMsg( int level, PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_DEVELOPER, level ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_MESSAGE, s_pDeveloper, level, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}

void DevWarning( int level, PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_DEVELOPER, level ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_WARNING, s_pDeveloper, level, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}

void DevLog( int level, PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_DEVELOPER, level ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_LOG, s_pDeveloper, level, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}

void DevMsg( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_DEVELOPER, 1 ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_MESSAGE, s_pDeveloper, 1, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}

void DevWarning( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_DEVELOPER, 1 ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_WARNING, s_pDeveloper, 1, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}

void DevLog( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_DEVELOPER, 1 ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_LOG, s_pDeveloper, 1, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}


//-----------------------------------------------------------------------------
// A couple of super-common dynamic spew messages, here for convenience 
// These looked at the "console" group, print if it's level 1 or higher 
//-----------------------------------------------------------------------------
void ConColorMsg( int level, const Color& clr, PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_CONSOLE, level ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_MESSAGE, s_pConsole, level, &clr, pMsgFormat, args );
	va_end(args);
}

void ConMsg( int level, PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_CONSOLE, level ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_MESSAGE, s_pConsole, level, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}

void ConWarning( int level, PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_CONSOLE, level ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_WARNING, s_pConsole, level, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}

void ConLog( int level, PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_CONSOLE, level ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_LOG, s_pConsole, level, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}

void ConColorMsg( const Color& clr, PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_CONSOLE, 1 ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_MESSAGE, s_pConsole, 1, &clr, pMsgFormat, args );
	va_end(args);
}

void ConMsg( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_CONSOLE, 1 ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_MESSAGE, s_pConsole, 1, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}

void ConWarning( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_CONSOLE, 1 ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_WARNING, s_pConsole, 1, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}

void ConLog( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_CONSOLE, 1 ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_LOG, s_pConsole, 1, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}


void ConDColorMsg( const Color& clr, PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_CONSOLE, 2 ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_MESSAGE, s_pConsole, 2, &clr, pMsgFormat, args );
	va_end(args);
}

void ConDMsg( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_CONSOLE, 2 ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_MESSAGE, s_pConsole, 2, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}

void ConDWarning( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_CONSOLE, 2 ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_WARNING, s_pConsole, 2, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}

void ConDLog( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_CONSOLE, 2 ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_LOG, s_pConsole, 2, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}


//-----------------------------------------------------------------------------
// A couple of super-common dynamic spew messages, here for convenience 
// These looked at the "network" group, print if it's level 1 or higher 
//-----------------------------------------------------------------------------
void NetMsg( int level, PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_NETWORK, level ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_MESSAGE, s_pNetwork, level, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}

void NetWarning( int level, PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_NETWORK, level ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_WARNING, s_pNetwork, level, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}

void NetLog( int level, PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( GROUP_NETWORK, level ) )
		return;

	va_list args;
	va_start( args, pMsgFormat );
	_SpewMessage( SPEW_LOG, s_pNetwork, level, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
}

#include "tier0/valve_on.h"


//-----------------------------------------------------------------------------
// Sets the priority level for a spew group
//-----------------------------------------------------------------------------
void SpewActivate( const tchar* pGroupName, int level )
{
	Assert( pGroupName );
	
	// check for the default group first...
	if ((pGroupName[0] == '*') && (pGroupName[1] == '\0'))
	{
		s_DefaultLevel = level;
		return;
	}
	
	// Normal case, search in group list using binary search.
	// If not found, grow the list of groups and insert it into the
	// right place to maintain sorted order. Then set the level.
	int ind;
	if ( !FindSpewGroup( pGroupName, &ind ) )
	{
		// not defined yet, insert an entry.
		++s_GroupCount;
		if ( s_pSpewGroups )
		{
			s_pSpewGroups = (SpewGroup_t*)PvRealloc( s_pSpewGroups, 
				s_GroupCount * sizeof(SpewGroup_t) );
			
			// shift elements down to preserve order
			int numToMove = s_GroupCount - ind - 1;
			memmove( &s_pSpewGroups[ind+1], &s_pSpewGroups[ind], 
				numToMove * sizeof(SpewGroup_t) );

			// Update standard groups
			for ( int i = 0; i < GROUP_COUNT; ++i )
			{   
				if ( ( ind <= s_pGroupIndices[i] ) && ( s_pGroupIndices[i] >= 0 ) )
				{
					++s_pGroupIndices[i];
				}
			}
		}
		else
		{
			s_pSpewGroups = (SpewGroup_t*)PvAlloc( s_GroupCount * sizeof(SpewGroup_t) ); 
		}
		
		Assert( _tcslen( pGroupName ) < MAX_GROUP_NAME_LENGTH );
		_tcscpy( s_pSpewGroups[ind].m_GroupName, pGroupName );

		// Update standard groups
		for ( int i = 0; i < GROUP_COUNT; ++i )
		{
			if ( ( s_pGroupIndices[i] < 0 ) && !_tcsicmp( s_pGroupNames[i], pGroupName ) )
			{
				s_pGroupIndices[i] = ind;
				break;
			}
		}
	}
	s_pSpewGroups[ind].m_Level = level;
}

void SpewDeactivate()
{
	PvFree( s_pSpewGroups );
	s_pSpewGroups = nullptr;
	memset( &s_pGroupIndices, 0xFF, sizeof(s_pGroupIndices) );
}

// If we don't have a function from math.h, then it doesn't link certain floating-point
// functions in and printfs with %f cause runtime errors in the C libraries.
DBG_INTERFACE float CrackSmokingCompiler( float a )
{
	return (float)fabs( a );
}

void* Plat_SimpleLog( const tchar* file, int line )
{
	FILE* f = _tfopen( _T("simple.log"), _T("at+") );
  if ( f )
	{
		_ftprintf( f, _T("%s:%i\n"), file, line );
		fclose( f );
	}

	return NULL;
}

#ifdef DBGFLAG_VALIDATE
void ValidateSpew( CValidator &validator )
{
	validator.Push( _T("Spew globals"), NULL, _T("Global") );

	validator.ClaimMemory( s_pSpewGroups );

	validator.Pop( );
}
#endif // DBGFLAG_VALIDATE

//-----------------------------------------------------------------------------
// Purpose: For debugging startup times, etc.
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void COM_TimestampedLog( PRINTF_FORMAT_STRING char const *fmt, ... )
{
	// dimhotepus: Store time as double
	static double s_LastStamp = 0.0;
	static bool s_bShouldLog = false;
	static bool s_bShouldLogToETW = false;
	static bool s_bChecked = false;
	static bool	s_bFirstWrite = false;

	if ( !s_bChecked )
	{
		s_bShouldLog = ( IsX360() || CommandLine()->CheckParm( "-profile" ) ) ? true : false;
		s_bShouldLogToETW = ( CommandLine()->CheckParm( "-etwprofile" ) ) ? true : false;
		if ( s_bShouldLogToETW )
		{
			s_bShouldLog = true;
		}
		s_bChecked = true;
	}
	if ( !s_bShouldLog )
	{
		return;
	}

	char string[1024];
	va_list argptr;
	va_start( argptr, fmt );
	_vsnprintf( string, sizeof( string ), fmt, argptr );
	va_end( argptr );

	double curStamp = Plat_FloatTime();

#if defined( _X360 )
	XBX_rTimeStampLog( curStamp, string );
#endif

	if ( IsPC() )
	{
		// If ETW profiling is enabled then do it only.
		if ( s_bShouldLogToETW )
		{
			ETWMark( string );
		}
		else
		{
			if ( !s_bFirstWrite )
			{
				unlink( "timestamped.log" );
				s_bFirstWrite = true;
			}

			FILE* fp = fopen( "timestamped.log", "at+" );
			if ( fp )
			{
				fprintf( fp, "%8.4f / %8.4f:  %s\n", curStamp, curStamp - s_LastStamp, string );
				fclose( fp );
			}
		}
	}

	s_LastStamp = curStamp;
}

//-----------------------------------------------------------------------------
// Sets an assert failed notify handler
//-----------------------------------------------------------------------------
void SetAssertFailedNotifyFunc( AssertFailedNotifyFunc_t func )
{
	s_AssertFailedNotifyFunc = func;
}


//-----------------------------------------------------------------------------
// Calls the assert failed notify handler if one has been set
//-----------------------------------------------------------------------------
void CallAssertFailedNotifyFunc( const char *pchFile, int nLine, const char *pchMessage )
{
	if ( s_AssertFailedNotifyFunc )
		s_AssertFailedNotifyFunc( pchFile, nLine, pchMessage );
}


