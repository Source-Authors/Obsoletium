// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"
#include "tier0/icommandline.h"
#include "tier0/dbg.h"

#ifdef POSIX
#include <climits>
#define MAX_PATH PATH_MAX
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Implements ICommandLine
//-----------------------------------------------------------------------------
class CCommandLine final : public ICommandLine
{
public:
	CCommandLine();
	virtual ~CCommandLine();

	void                      CreateCmdLine( const char *commandline ) override;
	void                      CreateCmdLine( int argc, char **argv ) override;
	[[nodiscard]] const char *GetCmdLine() const override;
	const char               *CheckParm( const char *psz, const char **ppszValue = nullptr ) const override;
	bool                      HasParm( const char *psz ) const override;
	void                      RemoveParm( const char *parm ) override;
	void                      AppendParm( const char *pszParm, const char *pszValues ) override;
	[[nodiscard]] int         ParmCount() const override;
	int                       FindParm( const char *psz ) const override;
	[[nodiscard]] const char *GetParm( int nIndex ) const override;
	const char               *ParmValue( const char *psz, const char *pDefaultVal = nullptr ) const override;
	int                       ParmValue( const char *psz, int nDefaultVal ) const override;
	float                     ParmValue( const char *psz, float flDefaultVal ) const override;
	const char               *ParmValueByIndex( int nIndex, const char *pDefaultVal = nullptr ) const override;
	void                      SetParm( int nIndex, char const *pParm ) override;

private:
	enum
	{
		MAX_PARAMETER_LEN = 128,
		MAX_PARAMETERS = 256
	};

	// When the commandline contains @name, it reads the parameters from that file
	void LoadParametersFromFile( const char *&pSrc, char *&pDst, size_t maxDestLen, bool bInQuotes );
	// Parse command line...
	void ParseCommandLine();
	// Frees the command line arguments
	void CleanUpParms();
	// Adds an argument..
	void AddArgument( const char *pFirst, const char *pLast );

private:
	// Copy of actual command line
	char *m_pszCmdLine;
	int   m_nParmCount;
	// Pointers to each argument...
	char *m_ppParms[MAX_PARAMETERS];
};


//-----------------------------------------------------------------------------
// Instance singleton and expose interface to rest of code
//-----------------------------------------------------------------------------
static CCommandLine g_CmdLine;
ICommandLine       *CommandLine_Tier0()
{
	return &g_CmdLine;
}

CCommandLine::CCommandLine() :
	m_pszCmdLine{ nullptr },
	m_nParmCount{ 0 }
{
	BitwiseClear( m_ppParms );
}

CCommandLine::~CCommandLine()
{
	CleanUpParms();
	delete[] m_pszCmdLine;
}


//-----------------------------------------------------------------------------
// Read commandline from file instead...
//-----------------------------------------------------------------------------
void CCommandLine::LoadParametersFromFile( const char *&pSrc, char *&pDst, size_t maxDestLen, bool bInQuotes )
{
	// Suck out the file name
	char  szFileName[MAX_PATH];
	char *pOut;
	char *pDestStart = pDst;

	if ( maxDestLen < 3 )
		return;

	// Skip the @ sign
	pSrc++;
	pOut = szFileName;
	char terminatingChar = ' ';

	if ( bInQuotes )
		terminatingChar = '\"';

	while ( *pSrc && *pSrc != terminatingChar )
	{
		*pOut++ = *pSrc++;
		if ( ( pOut - szFileName ) >= ( MAX_PATH - 1 ) )
			break;
	}

	*pOut = '\0';

	// Skip the space after the file name
	if ( *pSrc )
		pSrc++;

	// Now read in parameters from file
	FILE *fp = fopen( szFileName, "r" );
	RunCodeAtScopeExitOpt( fp,
	                       fclose( fp ) );
	if ( fp )
	{
		int c = fgetc( fp );
		while ( c != EOF )
		{
			// Turn return characters into spaces
			if ( c == '\n' )
				c = ' ';

			*pDst++ = static_cast< char >( c );

			// Don't go past the end, and allow for our terminating space character AND a terminating null character.
			if ( ( pDst - pDestStart ) >= ( static_cast< intp >( maxDestLen ) - 2 ) )
				break;

			// Get the next character, if there are more
			c = fgetc( fp );
		}

		// Add a terminating space character
		*pDst++ = ' ';
	}
	else
	{
		fprintf( stderr, "Can't open cmd line file '%s', skipping due to %s...", szFileName, strerror( errno ) );
	}
}

void CCommandLine::CreateCmdLine( int argc, char **argv )
{
	char cmdline[4096];
	cmdline[0] = '\0';

	char       *dest = cmdline;
	size_t      size = std::size( cmdline );
	const char *space = "";

	for ( int i = 0; i < argc; ++i )
	{
		// dimhotepus: Fixed buffer overflow risk by proper bounds checking
		size_t needed = strlen( space ) + strlen( argv[i] ) + 3; // quotes + null
		if ( needed > size )
		{
			AssertMsg( false, "CreateCmdLine: cmdline buffer overflow" );
			break;
		}

		// dimhotepus: Use safer snprintf with actual remaining size
		int written = _snprintf( dest, size, "%s\"%s\"", space, argv[i] );
		if ( written < 0 || (size_t)written >= size )
			break;

		dest[size - 1] = '\0';

		size_t len = strlen( dest );
		size -= len;
		dest += len;

		space = " ";
	}

	CreateCmdLine( cmdline );
}

//-----------------------------------------------------------------------------
// Purpose: Create a command line from the passed in string
//  Note that if you pass in a @filename, then the routine will read settings
//  from a file instead of the command line
//-----------------------------------------------------------------------------
void CCommandLine::CreateCmdLine( const char *commandline )
{
	delete[] m_pszCmdLine;

	char szFull[4096];
	szFull[0] = '\0';

	char       *pDst = szFull;
	const char *pSrc = commandline;
	bool        bInQuotes = false;
	const char *pInQuotesStart = nullptr;

	while ( *pSrc )
	{
		// Is this an unslashed quote?
		if ( *pSrc == '"' )
		{
			if ( pSrc == commandline || ( pSrc[-1] != '/' && pSrc[-1] != '\\' ) )
			{
				bInQuotes = !bInQuotes;
				pInQuotesStart = pSrc + 1;
			}
		}

		if ( *pSrc == '@' )
		{
			if ( pSrc == commandline || ( !bInQuotes && isspace( static_cast< unsigned char >( pSrc[-1] ) ) ) || ( bInQuotes && pSrc == pInQuotesStart ) )
			{
				LoadParametersFromFile( pSrc, pDst, sizeof( szFull ) - ( pDst - szFull ), bInQuotes );
				continue;
			}
		}

		// Don't go past the end.
		if ( ( pDst - szFull ) >= ( ssize( szFull ) - 1 ) )
			break;

		*pDst++ = *pSrc++;
	}

	*pDst = '\0';

	size_t len = strlen( szFull ) + 1;

	m_pszCmdLine = new char[len];
	// dimhotepus: Replaced unsafe memcpy with safer pattern.
	memcpy( m_pszCmdLine, szFull, len );

	ParseCommandLine();
}

//-----------------------------------------------------------------------------
// Finds a string in another string with a case insensitive test
//-----------------------------------------------------------------------------
// dimhotepus: _stristr -> V_stristr
static char *V_stristr( char *pStr, const char *pSearch )
{
	AssertValidStringPtr( pStr );
	AssertValidStringPtr( pSearch );

	if ( !pStr || !pSearch )
		return nullptr;

	char *pLetter = pStr;

	// Check the entire string
	while ( *pLetter != '\0' )
	{
		// Skip over non-matches
		if ( tolower( static_cast< unsigned char >( *pLetter ) ) == tolower( static_cast< unsigned char >( *pSearch ) ) )
		{
			// Check for match
			char const *pMatch = pLetter + 1;
			char const *pTest = pSearch + 1;

			while ( *pTest != '\0' )
			{
				// We've run off the end; don't bother.
				if ( *pMatch == '\0' )
					return nullptr;

				if ( tolower( static_cast< unsigned char >( *pMatch ) ) != tolower( static_cast< unsigned char >( *pTest ) ) )
					break;

				++pMatch;
				++pTest;
			}

			if ( *pTest == '\0' )
				return pLetter;
		}
		++pLetter;
	}

	return nullptr;
}

void CCommandLine::RemoveParm( const char *pszParm )
{
	if ( !m_pszCmdLine )
		return;

	// Search for first occurrence of pszParm
	size_t n;
	size_t nParmLen = strlen( pszParm );
	char  *p = m_pszCmdLine;

	while ( *p )
	{
		size_t curlen = strlen( p );
		char  *found = V_stristr( p, pszParm );

		if ( !found )
			break;

		char *pnextparam = found + 1;

		bool bHadQuote = false;

		if ( found > m_pszCmdLine && found[-1] == '\"' )
			bHadQuote = true;

		while ( pnextparam && *pnextparam && ( *pnextparam != ' ' ) && ( *pnextparam != '\"' ) )
			pnextparam++;

		if ( pnextparam && ( static_cast< size_t >( pnextparam - found ) > nParmLen ) )
		{
			p = pnextparam;
			continue;
		}

		while ( pnextparam && *pnextparam && ( *pnextparam != '-' ) && ( *pnextparam != '+' ) )
			pnextparam++;

		if ( bHadQuote )
		{
			found--;
		}

		if ( pnextparam && *pnextparam )
		{
			// We are either at the end of the string, or at the next param.  Just chop out the current param.
			n = curlen - ( pnextparam - p );
			memmove( found, pnextparam, n );
			found[n] = '\0';
		}
		else
		{
			// dimhotepus: Handle nullptr.
			if ( pnextparam )
			{
				// Clear out rest of string.
				n = pnextparam - found;
				memset( found, 0, n );
			}
		}
	}

	size_t len = strlen( m_pszCmdLine );

	// Strip and trailing ' ' characters left over.
	while ( true )
	{
		if ( len == 0 || m_pszCmdLine[len - 1] != ' ' )
			break;
		m_pszCmdLine[len - 1] = '\0';
		--len;
	}

	ParseCommandLine();
}


//-----------------------------------------------------------------------------
// Purpose: Append parameter and argument values to command line
// Input  : *pszParm -
//			*pszValues -
//-----------------------------------------------------------------------------
void CCommandLine::AppendParm( const char *pszParm, const char *pszValues )
{
	size_t nNewLength = strlen( pszParm );
	if ( pszValues )
		nNewLength += strlen( pszValues ) + 1;
	nNewLength++;

	if ( !m_pszCmdLine )
	{
		m_pszCmdLine = new char[nNewLength];
		// dimhotepus: Use safer string operations
		strncpy( m_pszCmdLine, pszParm, nNewLength - 1 );
		m_pszCmdLine[nNewLength - 1] = '\0';
		if ( pszValues )
		{
			strncat( m_pszCmdLine, " ", nNewLength - strlen( m_pszCmdLine ) - 1 );
			strncat( m_pszCmdLine, pszValues, nNewLength - strlen( m_pszCmdLine ) - 1 );
		}
		ParseCommandLine();
		return;
	}

	// Remove any remnants from the current Cmd Line.
	RemoveParm( pszParm );
	nNewLength += strlen( m_pszCmdLine ) + 1 + 1;
	char *pCmdString = new char[nNewLength];
	memset( pCmdString, 0, nNewLength );
	// dimhotepus: Replaced unsafe strcpy/strcat with bounded versions
	strncpy( pCmdString, m_pszCmdLine, nNewLength - 1 );
	strncat( pCmdString, " ", nNewLength - strlen( pCmdString ) - 1 );
	strncat( pCmdString, pszParm, nNewLength - strlen( pCmdString ) - 1 );
	if ( pszValues )
	{
		strncat( pCmdString, " ", nNewLength - strlen( pCmdString ) - 1 );
		strncat( pCmdString, pszValues, nNewLength - strlen( pCmdString ) - 1 );
	}

	// Kill off the old one
	delete[] m_pszCmdLine;

	pCmdString[nNewLength - 1] = '\0';
	// Point at the new command line.
	m_pszCmdLine = pCmdString;

	ParseCommandLine();
}

//-----------------------------------------------------------------------------
// Purpose: Return current command line
// Output : const char
//-----------------------------------------------------------------------------
const char *CCommandLine::GetCmdLine() const
{
	return m_pszCmdLine;
}

//-----------------------------------------------------------------------------
// Purpose: Search for the parameter in the current commandline
// Input  : *psz -
//			**ppszValue -
// Output : char
//-----------------------------------------------------------------------------
const char *CCommandLine::CheckParm( const char *psz, const char **ppszValue ) const
{
	if ( ppszValue )
		*ppszValue = nullptr;

	const int i = FindParm( psz );
	if ( i == 0 )
		return nullptr;

	if ( ppszValue )
	{
		if ( ( i + 1 ) >= m_nParmCount )
		{
			*ppszValue = nullptr;
		}
		else
		{
			*ppszValue = m_ppParms[i + 1];
		}
	}

	return m_ppParms[i];
}


//-----------------------------------------------------------------------------
// Adds an argument..
//-----------------------------------------------------------------------------
void CCommandLine::AddArgument( const char *pFirst, const char *pLast )
{
	if ( pLast <= pFirst )
		return;

	if ( m_nParmCount >= MAX_PARAMETERS )
		Error( "CCommandLine::AddArgument: exceeded %d parameters", MAX_PARAMETERS );

	const size_t nLen = pLast - pFirst + 1;
	m_ppParms[m_nParmCount] = new char[nLen];
	memcpy( m_ppParms[m_nParmCount], pFirst, nLen - 1 );
	m_ppParms[m_nParmCount][nLen - 1] = 0;

	++m_nParmCount;
}


//-----------------------------------------------------------------------------
// Parse command line...
//-----------------------------------------------------------------------------
void CCommandLine::ParseCommandLine()
{
	CleanUpParms();
	if ( !m_pszCmdLine )
		return;

	const char *pChar = m_pszCmdLine;
	while ( *pChar && isspace( static_cast< unsigned char >( *pChar ) ) )
	{
		++pChar;
	}

	bool        bInQuotes = false;
	const char *pFirstLetter = nullptr;
	for ( ; *pChar; ++pChar )
	{
		if ( bInQuotes )
		{
			if ( *pChar != '\"' )
				continue;

			AddArgument( pFirstLetter, pChar );
			pFirstLetter = nullptr;
			bInQuotes = false;
			continue;
		}

		// Haven't started a word yet...
		if ( !pFirstLetter )
		{
			if ( *pChar == '\"' )
			{
				bInQuotes = true;
				pFirstLetter = pChar + 1;
				continue;
			}

			if ( isspace( static_cast< unsigned char >( *pChar ) ) )
				continue;

			pFirstLetter = pChar;
			continue;
		}

		// Here, we're in the middle of a word. Look for the end of it.
		if ( isspace( static_cast< unsigned char >( *pChar ) ) )
		{
			AddArgument( pFirstLetter, pChar );
			pFirstLetter = nullptr;
		}
	}

	if ( pFirstLetter )
	{
		AddArgument( pFirstLetter, pChar );
	}
}


//-----------------------------------------------------------------------------
// Individual command line arguments
//-----------------------------------------------------------------------------
void CCommandLine::CleanUpParms()
{
	for ( int i = 0; i < m_nParmCount; ++i )
	{
		delete[] m_ppParms[i];
		m_ppParms[i] = nullptr;
	}
	m_nParmCount = 0;
}


//-----------------------------------------------------------------------------
// Returns individual command line arguments
//-----------------------------------------------------------------------------
int CCommandLine::ParmCount() const
{
	return m_nParmCount;
}

int CCommandLine::FindParm( const char *psz ) const
{
	// Start at 1 so as to not search the exe name
	for ( int i = 1; i < m_nParmCount; ++i )
	{
		if ( !_stricmp( psz, m_ppParms[i] ) )
			return i;
	}
	return 0;
}

bool CCommandLine::HasParm( const char *psz ) const
{
	return FindParm( psz ) != 0;
}

const char *CCommandLine::GetParm( int nIndex ) const
{
	Assert( nIndex >= 0 && nIndex < m_nParmCount );
	if ( nIndex < 0 || nIndex >= m_nParmCount )
		return "";
	return m_ppParms[nIndex];
}

void CCommandLine::SetParm( int nIndex, char const *pParm )
{
	if ( pParm )
	{
		Assert( nIndex >= 0 && nIndex < m_nParmCount );
		if ( nIndex >= 0 && nIndex < m_nParmCount )
		{
			delete[] m_ppParms[nIndex];
			// dimhotepus: Use strdup with safe deallocation pattern
			m_ppParms[nIndex] = strdup( pParm );
		}
	}
}

namespace
{

[[nodiscard]] bool CanBeCommandArg( const char value )
{
	return value == '-' || value == '+';
}

}

//-----------------------------------------------------------------------------
// Returns the argument after the one specified, or the default if not found
//-----------------------------------------------------------------------------
const char *CCommandLine::ParmValue( const char *psz, const char *pDefaultVal ) const
{
	const int nIndex = FindParm( psz );
	if ( nIndex == 0 || nIndex == m_nParmCount - 1 )
		return pDefaultVal;

	// Probably another cmdline parameter instead of a valid arg if it starts with '+' or '-'
	if ( CanBeCommandArg( m_ppParms[nIndex + 1][0] ) )
	{
		// dimhotepus: Allow negative numbers: "-5", "-1.5" should be valid values if they follow a parameter
		if ( !isdigit( static_cast< unsigned char >( m_ppParms[nIndex + 1][1] ) ) )
		{
			return pDefaultVal;
		}
	}
	return m_ppParms[nIndex + 1];
}

int CCommandLine::ParmValue( const char *psz, int nDefaultVal ) const
{
	const int nIndex = FindParm( psz );
	if ( nIndex == 0 || nIndex == m_nParmCount - 1 )
		return nDefaultVal;

	// Probably another cmdline parameter instead of a valid arg if it starts with '+' or '-'
	if ( CanBeCommandArg( m_ppParms[nIndex + 1][0] ) )
	{
		// dimhotepus: Allow negative numbers: "-5", "-1.5" should be valid values if they follow a parameter
		if ( !isdigit( static_cast< unsigned char >( m_ppParms[nIndex + 1][1] ) ) )
		{
			return nDefaultVal;
		}
	}
	return atoi( m_ppParms[nIndex + 1] );
}

float CCommandLine::ParmValue( const char *psz, float flDefaultVal ) const
{
	const int nIndex = FindParm( psz );
	if ( nIndex == 0 || nIndex == m_nParmCount - 1 )
		return flDefaultVal;

	// Probably another cmdline parameter instead of a valid arg if it starts with '+' or '-'
	if ( CanBeCommandArg( m_ppParms[nIndex + 1][0] ) )
	{
		// dimhotepus: Allow negative numbers: "-5", "-1.5" should be valid values if they follow a parameter
		if ( !isdigit( static_cast< unsigned char >( m_ppParms[nIndex + 1][1] ) ) )
		{
			return flDefaultVal;
		}
	}
	// dimhotepus: atof -> strtof
	return strtof( m_ppParms[nIndex + 1], nullptr );
}

const char *CCommandLine::ParmValueByIndex( int nIndex, const char *pDefaultVal ) const
{
	// dimhotepus: Fixed bounds checking - must validate both nIndex and nIndex+1
	if ( nIndex < 0 || nIndex >= m_nParmCount - 1 )
		return pDefaultVal;

	// Probably another cmdline parameter instead of a valid arg if it starts with '+' or '-'
	if ( CanBeCommandArg( m_ppParms[nIndex + 1][0] ) )
	{
		// dimhotepus: Allow negative numbers: "-5", "-1.5" should be valid values if they follow a parameter
		if ( !isdigit( static_cast< unsigned char >( m_ppParms[nIndex + 1][1] ) ) )
		{
			return pDefaultVal;
		}
	}
	return m_ppParms[nIndex + 1];
}
