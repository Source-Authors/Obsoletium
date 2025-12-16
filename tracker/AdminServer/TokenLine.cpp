//========= Copyright Valve Corporation, All rights reserved. ============//
/*
** The copyright to the contents herein is the property of Valve, L.L.C.
** The contents may be used and/or copied only with the written permission of
** Valve, L.L.C., or in accordance with the terms and conditions stipulated in
** the agreement/contract under which the contents have been supplied.
**
*******************************************************************************
**
** Contents:
**
**		TokenLine.cpp: implementation of the TokenLine class.
**
******************************************************************************/

#include "TokenLine.h"

#include "tier1/strtools.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


TokenLine::~TokenLine() = default;


bool TokenLine::SetLine(const char * newLine)
{
	m_tokenNumber = 0;

	if (!newLine || ( V_strlen(newLine) >= (MAX_LINE_CHARS-1) ) )
	{
		BitwiseClear( m_fullLine );
		BitwiseClear( m_tokenBuffer );
		return false;
	}

	V_strcpy_safe( m_fullLine, newLine );
	V_strcpy_safe( m_tokenBuffer, newLine );

	// parse tokens 
	char * charPointer = m_tokenBuffer;
	
	while (*charPointer && (m_tokenNumber < MAX_LINE_TOKENS))
	{
		while (*charPointer && ((*charPointer <= 32) || (*charPointer > 126)))
			charPointer++;

		if (*charPointer)
		{
			m_token[m_tokenNumber] = charPointer;

			// special treatment for quotes
			if (*charPointer == '\"')
			{
				charPointer++;
				m_token[m_tokenNumber] = charPointer;
				while (*charPointer && (*charPointer != '\"') )
					charPointer++;
			}
			else 
			{
				while (*charPointer && ((*charPointer > 32) && (*charPointer <= 126)))
					charPointer++;
			}

			m_tokenNumber++;

			if (*charPointer)
			{	
				*charPointer=0;
				charPointer++;
			}
		}
	}

	return (m_tokenNumber != MAX_LINE_TOKENS);
}

char * TokenLine::GetLine() 
{
	return m_fullLine;
}

char * TokenLine::GetToken(intp i) const
{
	if (i >= m_tokenNumber)
		return nullptr;
	return m_token[i];
}

// if the given parm is not present return nullptr
// otherwise return the address of the following token, or an empty string
char* TokenLine::CheckToken(const char * parm) const
{
	for (intp i = 0 ; i < m_tokenNumber; i ++)
	{
		if (!m_token[i])
			continue; 
		if ( !strcmp (parm,m_token[i]) )
		{
			char * ret = m_token[i+1];	
			// if this token doesn't exist, since index i was the last
			// return an empty string
			if ( (i+1) == m_tokenNumber ) ret = "";
			return ret;
		}
			
	}
		
	return nullptr;
}

intp TokenLine::CountToken() const
{
	intp c = 0;
	for (intp i = 0 ; i < m_tokenNumber; i ++)
	{
		if (m_token[i])
			c++;
	}
	return c;
}

char* TokenLine::GetRestOfLine(intp i)
{
	if (i >= m_tokenNumber)
		return nullptr;
	return m_fullLine + (m_token[i] - m_tokenBuffer);
}

TokenLine::TokenLine(char * string)
{
	BitwiseClear(m_token);
	SetLine(string);
}

TokenLine::TokenLine()
{
	BitwiseClear(m_tokenBuffer);
	BitwiseClear(m_fullLine);
	BitwiseClear(m_token);
	m_tokenNumber = 0;
}
