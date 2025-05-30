//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=============================================================================

#include "fgdlib/gdvar.h"

#include "fgdlib/fgdlib.h"
#include "fgdlib/gamedata.h"
#include "fgdlib/wckeyvalues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


struct TypeMap_t
{
	GDIV_TYPE eType;		// The enumeration of this type.
	const char *pszName;	// The name of this type.
	trtoken_t eStoreAs;		// How this type is stored (STRING, INTEGER, etc).
};


//-----------------------------------------------------------------------------
// Maps type names to type enums and parsing logic for values.
//-----------------------------------------------------------------------------
static constexpr TypeMap_t TypeMap[] =
{
	{ ivAngle,				"angle",				STRING },
	{ ivChoices,			"choices",				STRING },
	{ ivColor1,				"color1",				STRING },
	{ ivColor255,			"color255",				STRING },
	{ ivDecal,				"decal",				STRING },
	{ ivFlags,				"flags",				INTEGER },
	{ ivInteger,			"integer",				INTEGER },
	{ ivSound,				"sound",				STRING },
	{ ivSprite,				"sprite",				STRING },
	{ ivString,				"string",				STRING },
	{ ivStudioModel,		"studio",				STRING },
	{ ivTargetDest,			"target_destination",	STRING },
	{ ivTargetSrc,			"target_source",		STRING },
	{ ivTargetNameOrClass,	"target_name_or_class",	STRING },	// Another version of target_destination that accepts class names
	{ ivVector,				"vector",				STRING },
	{ ivNPCClass,			"npcclass",				STRING },
	{ ivFilterClass,		"filterclass",			STRING },
	{ ivFloat,				"float",				STRING },
	{ ivMaterial,			"material",				STRING },
	{ ivScene,				"scene",				STRING },
	{ ivSide,				"side",					STRING },
	{ ivSideList,			"sidelist",				STRING },
	{ ivOrigin,				"origin",				STRING },
	{ ivAxis,				"axis",					STRING },
	{ ivVecLine,			"vecline",				STRING },
	{ ivPointEntityClass,	"pointentityclass",		STRING },
	{ ivNodeDest,			"node_dest",			INTEGER },
	{ ivInstanceFile,		"instance_file",		STRING },
	{ ivAngleNegativePitch,	"angle_negative_pitch",	STRING },
	{ ivInstanceVariable,	"instance_variable",	STRING },
	{ ivInstanceParm,		"instance_parm",		STRING },
};


const char *GDinputvariable::m_pszEmpty = "";


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
GDinputvariable::GDinputvariable(void)
{
	m_szName[0] = '\0';
	m_szLongName[0] = '\0';
	m_pszDescription = NULL;
	m_eType = ivBadType;
	m_nDefault = 0;
	m_szDefault[0] = '\0';
	m_nValue = 0;
	m_szValue[0] = '\0';
	m_bReportable = FALSE;
	m_bReadOnly = false;
}


//-----------------------------------------------------------------------------
// Purpose: construct generally used for creating a temp instance parm type
// Input  : szType - the textual type of this variable
//			szName - the name description of this variable
//-----------------------------------------------------------------------------
GDinputvariable::GDinputvariable( const char *szType, const char *szName )
{
	m_szLongName[0] = 0;
	m_pszDescription = NULL;
	m_nDefault = 0;
	m_szDefault[0] = 0;
	m_nValue = 0;
	m_szValue[0] = 0;
	m_bReportable = FALSE;
	m_bReadOnly = false;

	m_eType = GetTypeFromToken( szType );
	V_strcpy_safe( m_szName, szName );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
GDinputvariable::~GDinputvariable(void)
{
	delete [] m_pszDescription;
	m_Items.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Implements the copy operator.
//-----------------------------------------------------------------------------
GDinputvariable &GDinputvariable::operator =(GDinputvariable &Other)
{
	// dimhotepus: Protect from self-assignment.
	if (this == &Other)
	{
		return *this;
	}

	m_eType = Other.GetType();
	V_strcpy_safe(m_szName, Other.m_szName);
	V_strcpy_safe(m_szLongName, Other.m_szLongName);
	V_strcpy_safe(m_szDefault, Other.m_szDefault);

	//
	// Copy the description.
	//
	delete [] m_pszDescription;
	if (Other.m_pszDescription != NULL)
	{
		m_pszDescription = V_strdup(Other.m_pszDescription);
	}
	else
	{
		m_pszDescription = NULL;
	}

	m_nDefault = Other.m_nDefault;
	m_bReportable = Other.m_bReportable;
	m_bReadOnly = Other.m_bReadOnly;

	m_Items.RemoveAll();
	
	for (const auto &i : Other.m_Items)
	{
		m_Items.AddToTail(i);
	}

	return(*this);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the storage format of a given variable type.
// Input  : pszToken - Sting containing the token.
// Output : GDIV_TYPE corresponding to the token in the string, ivBadType if the
//			string does not correspond to a valid type.
//-----------------------------------------------------------------------------
trtoken_t GDinputvariable::GetStoreAsFromType(GDIV_TYPE eType) const
{
	for (auto &tm : TypeMap)
	{
		if (tm.eType == eType)
		{
			return(tm.eStoreAs);
		}
	}

	Assert(FALSE);
	return(STRING);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the enumerated type of a string token.
// Input  : pszToken - Sting containing the token.
// Output : GDIV_TYPE corresponding to the token in the string, ivBadType if the
//			string does not correspond to a valid type.
//-----------------------------------------------------------------------------
GDIV_TYPE GDinputvariable::GetTypeFromToken(const char *pszToken) const
{
	for (auto &tm : TypeMap)
	{
		if (IsToken(pszToken, tm.pszName))
		{
			return(tm.eType);
		}
	}

	return(ivBadType);
}


//-----------------------------------------------------------------------------
// Purpose: Returns a string representing the type of this variable, eg. "integer".
//-----------------------------------------------------------------------------
const char *GDinputvariable::GetTypeText(void) const
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
// Input  : tr - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL GDinputvariable::InitFromTokens(TokenReader& tr)
{
	char szToken[128];

	if (!GDGetToken(tr, m_szName, IDENT))
	{
		return FALSE;
	}

	if (!GDSkipToken(tr, OPERATOR, "("))
	{
		return FALSE;
	}

	// check for "reportable" marker
	trtoken_t ttype = tr.NextToken(szToken);
	if (ttype == OPERATOR)
	{
		if (!strcmp(szToken, "*"))
		{
			m_bReportable = true;
		}
	}
	else
	{
		tr.Stuff(ttype, szToken);
	}

	// get type
	if (!GDGetToken(tr, szToken, IDENT))
	{
		return FALSE;
	}

	if (!GDSkipToken(tr, OPERATOR, ")"))
	{
		return FALSE;
	}

	//
	// Check for known variable types.
	//
	m_eType = GetTypeFromToken(szToken);
	if (m_eType == ivBadType)
	{
		GDError(tr, "'%s' is not a valid variable type", szToken);
		return FALSE;
	}

	//
	// Look ahead at the next token.
	//
	ttype = tr.PeekTokenType(szToken);

	//
	// Check for the "readonly" specifier.
	//
	if ((ttype == IDENT) && IsToken(szToken, "readonly"))
	{
		// dimhotepus: Skip readonly token.
		(void)tr.NextToken(szToken);
		m_bReadOnly = true;

		//
		// Look ahead at the next token.
		//
		ttype = tr.PeekTokenType(szToken);
	}

	//
	// Check for the ':' indicating a long name.
	//
	if (ttype == OPERATOR && IsToken(szToken, ":"))
	{
		//
		// Eat the ':'.
		//
		(void)tr.NextToken(szToken);

		if (m_eType == ivFlags)
		{
			GDError(tr, "flag sets do not have long names");
			return FALSE;
		}

		//
		// Get the long name.
		//
		if (!GDGetToken(tr, m_szLongName, STRING))
		{
			return(FALSE);
		}

		//
		// Look ahead at the next token.
		//
		ttype = tr.PeekTokenType(szToken);

		//
		// Check for the ':' indicating a default value.
		//
		if (ttype == OPERATOR && IsToken(szToken, ":"))
		{
			//
			// Eat the ':'.
			//
			(void)tr.NextToken(szToken);

			//
			// Look ahead at the next token.
			//
			ttype = tr.PeekTokenType(szToken);
			if (ttype == OPERATOR && IsToken(szToken, ":"))
			{
				//
				// No default value provided, skip to the description.
				//
			}
			else
			{
				//
				// Determine how to parse the default value. If this is a choices field, the
				// default could either be a string or an integer, so we must look ahead and
				// use whichever is there.
				//
				trtoken_t eStoreAs = GetStoreAsFromType(m_eType);

				if (eStoreAs == STRING)
				{
					if (!GDGetToken(tr, m_szDefault, STRING))
					{
						return(FALSE);
					}
				}
				else if (eStoreAs == INTEGER)
				{
					if (!GDGetToken(tr, szToken, INTEGER))
					{
						return(FALSE);
					}

					m_nDefault = atoi(szToken);
				}

				//
				// Look ahead at the next token.
				//
				ttype = tr.PeekTokenType(szToken);
			}
		}

		//
		// Check for the ':' indicating a description.
		//
		if (ttype == OPERATOR && IsToken(szToken, ":"))
		{
			//
			// Eat the ':'.
			//
			(void)tr.NextToken(szToken);

			//
			// Read the description.
			//

			// If we've already read a description then free it to avoid memory leaks.
			if ( m_pszDescription )
			{
				delete [] m_pszDescription;
				m_pszDescription = NULL;
			}
			if (!GDGetTokenDynamic(tr, &m_pszDescription, STRING))
			{
				return(FALSE);
			}

			//
			// Look ahead at the next token.
			//
			ttype = tr.PeekTokenType(szToken);
		}
	}
	else
	{
		//
		// Default long name is short name.
		//
		V_strcpy_safe(m_szLongName, m_szName);
	}

	//
	// Check for the ']' indicating the end of the class definition.
	//
	if ((ttype == OPERATOR && IsToken(szToken, "]")) ||	ttype != OPERATOR)
	{
		if (m_eType == ivFlags || m_eType == ivChoices)
		{
			//
			// Can't define a flags or choices variable without providing any flags or choices.
			//
			GDError(tr, "no %s specified", m_eType == ivFlags ? "flags" : "choices");
			return(FALSE);
		}
		return(TRUE);
	}

	if (!GDSkipToken(tr, OPERATOR, "="))
	{
		return(FALSE);
	}

	if (m_eType != ivFlags && m_eType != ivChoices)
	{
		GDError(tr, "didn't expect '=' here");
		return(FALSE);
	}

	// should be '[' to start flags/choices
	if (!GDSkipToken(tr, OPERATOR, "["))
	{
		return(FALSE);
	}

	// get flags?
	if (m_eType == ivFlags)
	{
		GDIVITEM ivi;

		while (1)
		{
			ttype = tr.PeekTokenType(nullptr, 0);
			if (ttype != INTEGER)
			{
				break;
			}

			// store bitflag value
			GDGetToken(tr, szToken, INTEGER);
			// dimhotepus: Expect bitflag to be present.
			if ( sscanf( szToken, "%lu", &ivi.iValue ) != 1 )
			{
				return FALSE;
			}

			// colon..
			if (!GDSkipToken(tr, OPERATOR, ":"))
			{
				return FALSE;
			}

			// get description
			if (!GDGetToken(tr, szToken, STRING))
			{
				return FALSE;
			}
			V_strcpy_safe(ivi.szCaption, szToken);

			// colon..
			if (!GDSkipToken(tr, OPERATOR, ":"))
			{
				return FALSE;
			}

			// get default setting
			if (!GDGetToken(tr, szToken, INTEGER))
			{
				return FALSE;
			}
			ivi.bDefault = atoi(szToken) ? TRUE : FALSE;

			// add item to array of items
			m_Items.AddToTail(ivi);
		}
		
		// Set the default value.
		unsigned long nDefault = 0;
		for (const auto &i : m_Items)
		{
			if (i.bDefault)
				nDefault |= i.iValue;
		}
		m_nDefault = (int)nDefault;
		V_sprintf_safe( m_szDefault, "%d", m_nDefault );
	}
	else if (m_eType == ivChoices)
	{
		GDIVITEM ivi;

		while (1)
		{
			ttype = tr.PeekTokenType(nullptr, 0);
			if ((ttype != INTEGER) && (ttype != STRING))
			{
				break;
			}

			// store choice value
			GDGetToken(tr, szToken, ttype);
			ivi.iValue = 0;
			V_strcpy_safe(ivi.szValue, szToken);

			// colon
			if (!GDSkipToken(tr, OPERATOR, ":"))
			{
				return FALSE;
			}

			// get description
			if (!GDGetToken(tr, szToken, STRING))
			{
				return FALSE;
			}

			V_strcpy_safe(ivi.szCaption, szToken);

			m_Items.AddToTail(ivi);
		}
	}

	if (!GDSkipToken(tr, OPERATOR, "]"))
	{
		return FALSE;
	}

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Decodes a key value from a string.
// Input  : pkv - Pointer to the key value object containing string encoded value.
//-----------------------------------------------------------------------------
void GDinputvariable::FromKeyValue(MDkeyvalue *pkv)
{
	trtoken_t eStoreAs = GetStoreAsFromType(m_eType);

	if (eStoreAs == STRING)
	{
		V_strcpy_safe(m_szValue, pkv->szValue);
	}
	else if (eStoreAs == INTEGER)
	{
		m_nValue = atoi(pkv->szValue);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Determines whether the given flag is set (assuming this is an ivFlags).
// Input  : uCheck - Flag to check.
// Output : Returns TRUE if flag is set, FALSE if not.
//-----------------------------------------------------------------------------
BOOL GDinputvariable::IsFlagSet(unsigned int uCheck) const
{
	Assert(m_eType == ivFlags);
	return (((unsigned int)m_nValue & uCheck) == uCheck) ? TRUE : FALSE;
}


//-----------------------------------------------------------------------------
// Purpose: Combines the flags or choices items from another variable into our
//			list of flags or choices. Ours take priority if collisions occur.
// Input  : Other - The variable whose items are being merged with ours.
//-----------------------------------------------------------------------------
void GDinputvariable::Merge(GDinputvariable &Other)
{
	//
	// Only valid if we are of the same type.
	//
	if (Other.GetType() != GetType())
	{
		return;
	}

	//
	// Add Other's items to this ONLY if there is no same-value entry
	// for a specific item.
	//
	bool bFound = false;
	intp nOurItems = m_Items.Count();
	for (auto &TheirItem : Other.m_Items)
	{
		for (intp j = 0; j < nOurItems; j++)
		{
			GDIVITEM &OurItem = m_Items[j];
			if (TheirItem.iValue == OurItem.iValue)
			{
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			//
			// Not found in our list - add their item to our list.
			//
			m_Items.AddToTail(TheirItem);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Determines whether the given flag is set (assuming this is an ivFlags).
// Input  : uFlags - Flags to set.
//			bSet - TRUE to set the flags, FALSE to clear them.
//-----------------------------------------------------------------------------
void GDinputvariable::SetFlag(unsigned int uFlags, BOOL bSet)
{
	Assert(m_eType == ivFlags);
	if (bSet)
	{
		m_nValue |= uFlags;
	}
	else
	{
		m_nValue &= ~uFlags;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets this keyvalue to its default value.
//-----------------------------------------------------------------------------
void GDinputvariable::ResetDefaults(void)
{
	if (m_eType == ivFlags)
	{
		m_nValue = 0;

		//
		// Run thru flags and set any default flags.
		//
		intp nCount = m_Items.Count();
		for (intp i = 0; i < nCount; i++)
		{
			if (m_Items[i].bDefault)
			{
				m_nValue |= GetFlagMask(i);
			}
		}
	}
	else
	{
		m_nValue = m_nDefault;
		V_strcpy_safe(m_szValue, m_szDefault);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Encodes a key value as a string.
// Input  : pkv - Pointer to the key value object to receive the encoded string.
//-----------------------------------------------------------------------------
void GDinputvariable::ToKeyValue(MDkeyvalue *pkv)
{
	V_strcpy_safe(pkv->szKey, m_szName);

	trtoken_t eStoreAs = GetStoreAsFromType(m_eType);

	if (eStoreAs == STRING)
	{
		V_strcpy_safe(pkv->szValue, m_szValue);
	}
	else if (eStoreAs == INTEGER)
	{
		// dimhotepus: itoa -> V_to_chars.
		V_to_chars(pkv->szValue, m_nValue, 10);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns the description string that corresponds to a value string
//			for a choices list.
// Input  : pszString - The choices value string.
// Output : Returns the description string.
//-----------------------------------------------------------------------------
const char *GDinputvariable::ItemStringForValue(const char *szValue) const
{
	for (const auto &i : m_Items)
	{
		if (!stricmp(i.szValue, szValue))
		{
			return(i.szCaption);
		}
	}

	return(NULL);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the value string that corresponds to a description string
//			for a choices list.
// Input  : pszString - The choices description string.
// Output : Returns the value string.
//-----------------------------------------------------------------------------
const char *GDinputvariable::ItemValueForString(const char *szString) const
{
	for (const auto &i : m_Items)
	{
		if (!strcmpi(i.szCaption, szString))
		{
			return(i.szValue);
		}
	}

	return(NULL);
}


//-----------------------------------------------------------------------------
// Purpose: this function will let you iterate through the text names of the variable types
// Input  : eType - the type to get the text of
// Output : returns the textual name
//-----------------------------------------------------------------------------
const char *GDinputvariable::GetVarTypeName( GDIV_TYPE eType )
{
	return TypeMap[ eType ].pszName;
}


