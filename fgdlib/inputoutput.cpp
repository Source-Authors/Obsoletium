//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "fgdlib/InputOutput.h"

#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


struct TypeMap_t
{
	InputOutputType_t eType;	// The enumeration of this type.
	char *pszName;				// The name of this type.
};


char *CClassInputOutputBase::g_pszEmpty = "";


//-----------------------------------------------------------------------------
// Maps type names to type enums for inputs and outputs.
//-----------------------------------------------------------------------------
static constexpr TypeMap_t TypeMap[] =
{
	{ iotVoid,		"void" },
	{ iotInt,		"integer" },
	{ iotBool,		"bool" },
	{ iotString,	"string" },
	{ iotFloat,		"float" },
	{ iotVector,	"vector" },
	{ iotEHandle,	"target_destination" },
	{ iotColor,		"color255" },
	{ iotEHandle,	"ehandle" }, // for backwards compatibility
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CClassInputOutputBase::CClassInputOutputBase(void)
{
	m_szName[0] = '\0';
	m_eType = iotInvalid;
	m_pszDescription = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszName - 
//			eType - 
//-----------------------------------------------------------------------------
CClassInputOutputBase::CClassInputOutputBase(const char *pszName, InputOutputType_t eType)
{
	m_szName[0] = '\0';
	m_eType = eType;
	m_pszDescription = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CClassInputOutputBase::~CClassInputOutputBase(void)
{
	delete m_pszDescription;
	m_pszDescription = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a string representing the type of this I/O, eg. "integer".
//-----------------------------------------------------------------------------
const char *CClassInputOutputBase::GetTypeText(void) const
{
	for (auto &tm : TypeMap)
	{
		if (tm.eType == m_eType)
		{
			return(tm.pszName);
		}
	}

	return("unknown");
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : szType - 
// Output : InputOutputType_t
//-----------------------------------------------------------------------------
InputOutputType_t CClassInputOutputBase::SetType(const char *szType)
{
	for (auto &tm : TypeMap)
	{
		if (!stricmp(tm.pszName, szType))
		{
			m_eType = tm.eType;
			return(m_eType);
		}
	}

	return(iotInvalid);
}


//-----------------------------------------------------------------------------
// Purpose: Assignment operator.
//-----------------------------------------------------------------------------
CClassInputOutputBase &CClassInputOutputBase::operator =(CClassInputOutputBase &Other)
{
	// dimhotepus: Protect from self-assignment.
	if (this == &Other)
	{
		return *this;
	}

	V_strcpy_safe(m_szName, Other.m_szName);
	m_eType = Other.m_eType;

	//
	// Copy the description.
	//
	delete m_pszDescription;
	if (Other.m_pszDescription != NULL)
	{
		const ptrdiff_t len = strlen(Other.m_pszDescription) + 1;
		m_pszDescription = new char[len];
		V_strncpy(m_pszDescription, Other.m_pszDescription, len);
	}
	else
	{
		m_pszDescription = NULL;
	}

	return(*this);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CClassInput::CClassInput(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszName - 
//			eType - 
//-----------------------------------------------------------------------------
CClassInput::CClassInput(const char *pszName, InputOutputType_t eType)
	: CClassInputOutputBase(pszName, eType)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CClassOutput::CClassOutput(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszName - 
//			eType - 
//-----------------------------------------------------------------------------
CClassOutput::CClassOutput(const char *pszName, InputOutputType_t eType)
	: CClassInputOutputBase(pszName, eType)
{
}
