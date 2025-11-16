//========= Copyright Valve Corporation, All rights reserved. ============//
// TokenLine.h: interface for the CommandLine class.
//
//////////////////////////////////////////////////////////////////////

#ifndef TOKENLINE_H
#define TOKENLINE_H

#include "tier0/basetypes.h"

constexpr inline int MAX_LINE_TOKENS{128};
constexpr inline int MAX_LINE_CHARS{2048};

class TokenLine
{

public:
	TokenLine();
	TokenLine( char * string );
	virtual ~TokenLine();

	char *	GetRestOfLine(intp i);	// returns all chars after token i
	intp	CountToken() const;			// returns number of token
	// dimhotepus: Make const.
	char *	CheckToken(const char * parm) const;// returns token after token parm or ""
	// dimhotepus: Make const.
	char *	GetToken(intp i) const;		// returns token i
	char *	GetLine();				// returns full line
	bool	SetLine(const char * newLine);	// set new token line and parses it

	char	m_tokenBuffer[MAX_LINE_CHARS];
	char	m_fullLine[MAX_LINE_CHARS];
	char *	m_token[MAX_LINE_TOKENS];
	intp	m_tokenNumber;

};

#endif // !defined TOKENLINE_H
