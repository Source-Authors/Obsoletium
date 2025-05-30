//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TOKENREADER_H
#define TOKENREADER_H
#ifdef _WIN32
#pragma once
#endif

#include <fstream>

#include "tier0/annotations.h"
#include "tier0/basetypes.h"
#include "tier0/valve_minmax_on.h"



enum trtoken_t
{
	TOKENSTRINGTOOLONG = -4,
	TOKENERROR = -3,
	TOKENNONE = -2,
	TOKENEOF = -1,
	OPERATOR,
	INTEGER,
	STRING,
	IDENT
};


[[nodiscard]]
inline bool IsToken(const char* s1, const char* s2) { return !strcmpi(s1, s2); }

#define MAX_TOKEN (128 + 1)
#define MAX_IDENT (64 + 1)
#define MAX_STRING (128 + 1)


class TokenReader : private std::ifstream
{
public:

	TokenReader();

	[[nodiscard]] bool Open(const char *pszFilename);
	[[nodiscard]] trtoken_t NextToken(OUT_Z_CAP(nSize) char *pszStore, intp nSize);
	template<intp size>
	[[nodiscard]] trtoken_t NextToken(OUT_Z_ARRAY char (&pszStore)[size])
	{
		return NextToken(pszStore, size);
	}
	[[nodiscard]] trtoken_t NextTokenDynamic(char **ppszStore);
	void Close();

	void IgnoreTill(trtoken_t ttype, const char *pszToken);
	void Stuff(trtoken_t ttype, const char *pszToken);
	[[nodiscard]] bool Expecting(trtoken_t ttype, const char *pszToken);
	[[nodiscard]] const char *Error(char *error, ...);
	// dimhotepus: Breaking change, drop = nullptr and = 0 by default to allow use safe-bounds overload
	[[nodiscard]] trtoken_t PeekTokenType(OUT_Z_CAP(maxlen) char* pszStore, intp maxlen);
	template<intp size>
	[[nodiscard]] trtoken_t PeekTokenType(OUT_Z_ARRAY char (&pszStore)[size])
	{
		return PeekTokenType(pszStore, size);
	}

	inline int GetErrorCount(void) const;

private:
	// compiler can't generate an assignment operator since descended from std::ifstream
	inline TokenReader(TokenReader const &) = delete;
	inline int operator=(TokenReader const &) = delete;

	trtoken_t GetString(char *pszStore, intp nSize);
	bool SkipWhiteSpace();

	int m_nLine;
	int m_nErrorCount;

	char m_szFilename[128];
	char m_szStuffed[128];
	bool m_bStuffed;
	trtoken_t m_eStuffed;
};


//-----------------------------------------------------------------------------
// Purpose: Returns the total number of parsing errors since this file was opened.
//-----------------------------------------------------------------------------
int TokenReader::GetErrorCount(void) const
{
	return(m_nErrorCount);
}


#endif // TOKENREADER_H
