//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "fgdlib/wckeyvalues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Purpose: Assignment operator.
//-----------------------------------------------------------------------------
MDkeyvalue &MDkeyvalue::operator =(const MDkeyvalue &other)
{
	V_strcpy_safe(szKey, other.szKey);
	V_strcpy_safe(szValue, other.szValue);

	return(*this);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void WCKVBase_Vector::RemoveKeyAt(intp nIndex)
{
	Assert(nIndex >= 0);
	Assert(nIndex < m_KeyValues.Count());

	if ((nIndex >= 0) && (nIndex < m_KeyValues.Count()))
	{
		m_KeyValues.Remove(nIndex);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Adds the key to the keyvalue array. Allows duplicate keys.
//
//			NOTE: This should only be used for keyvalue lists that do not require
//			unique key names! If you use this function then you should use GetCount
//			and GetKey/Value by index rather than GetValue by key name.
//-----------------------------------------------------------------------------
void WCKVBase_Vector::AddKeyValue(const char *pszKey, const char *pszValue)
{
	if (!pszKey || !pszValue)
	{
		return;
	}

	char szTmpKey[KEYVALUE_MAX_KEY_LENGTH];
	char szTmpValue[KEYVALUE_MAX_VALUE_LENGTH];

	V_strcpy_safe(szTmpKey, pszKey);
	V_strcpy_safe(szTmpValue, pszValue);

	StripEdgeWhiteSpace(szTmpKey);
	StripEdgeWhiteSpace(szTmpValue);

	//
	// Add the keyvalue to our list.
	//
	MDkeyvalue newkv;
	V_strcpy_safe(newkv.szKey, szTmpKey);
	V_strcpy_safe(newkv.szValue, szTmpValue);
	m_KeyValues.AddToTail(newkv);
}

intp WCKVBase_Vector::FindByKeyName( const char *pKeyName ) const
{
	for ( intp i=0; i < m_KeyValues.Count(); i++ )
	{
		if ( V_stricmp( m_KeyValues[i].szKey, pKeyName ) == 0 )
			return i;
	}
	return GetInvalidIndex();
}

void WCKVBase_Vector::InsertKeyValue( const MDkeyvalue &kv )
{
	m_KeyValues.AddToTail( kv );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void WCKVBase_Dict::RemoveKeyAt(unsigned short nIndex)
{
	m_KeyValues.RemoveAt(nIndex);
}


unsigned short WCKVBase_Dict::FindByKeyName( const char *pKeyName ) const
{
	return m_KeyValues.Find( pKeyName );
}

void WCKVBase_Dict::InsertKeyValue( const MDkeyvalue &kv )
{
	m_KeyValues.Insert( kv.szKey, kv );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Deletes the contents of this keyvalue array.
//-----------------------------------------------------------------------------
template<class Base>
WCKeyValuesT<Base>::~WCKeyValuesT(void)
{
	//int i = 0;
	//while (i < m_KeyValues.GetSize())
	//{
	//	delete m_KeyValues.GetAt(i++);
	//}

	RemoveAll();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template<class Base>
const char *WCKeyValuesT<Base>::GetValue(const char *pszKey, intp *piIndex) const
{
	auto i = this->FindByKeyName( pszKey );
	if ( i == this->GetInvalidIndex() )
	{
		return NULL;
	}
	else
	{
		if(piIndex)
			piIndex[0] = i;
			
		return this->m_KeyValues[i].szValue;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template<class Base>
void WCKeyValuesT<Base>::RemoveKey(const char *pszKey)
{
	SetValue(pszKey, (const char *)NULL);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template<class Base>
void WCKeyValuesT<Base>::SetValue(const char *pszKey, int iValue)
{
	char szValue[16];
	// dimhotepus: itoa -> V_to_chars.
	V_to_chars(szValue, iValue);

	SetValue(pszKey, szValue);
}


//-----------------------------------------------------------------------------
// Purpose: Strips leading and trailing whitespace from the string.
// Input  : psz - 
//-----------------------------------------------------------------------------
void StripEdgeWhiteSpace(char *psz)
{
	if (!psz || !*psz)
		return;

	char *pszBase = psz;

	while (V_isspace(*psz))
	{
		psz++;
	}

	intp iLen = V_strlen(psz) - 1;
	if ( iLen >= 0 )
	{
		while (V_isspace(psz[iLen]))
		{
			psz[iLen--] = '\0';
		}
	}

	if (psz != pszBase)
	{
		memmove(pszBase, psz, iLen + 2);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszKey - 
//			pszValue - 
//-----------------------------------------------------------------------------
template<class Base>
void WCKeyValuesT<Base>::SetValue(const char *pszKey, const char *pszValue)
{
	char szTmpKey[KEYVALUE_MAX_KEY_LENGTH];
	char szTmpValue[KEYVALUE_MAX_VALUE_LENGTH];

	V_strcpy_safe(szTmpKey, pszKey);

	if (pszValue != NULL)
	{
		V_strcpy_safe(szTmpValue, pszValue);
	}
	else
	{
		szTmpValue[0] = 0;
	}

	StripEdgeWhiteSpace(szTmpKey);
	StripEdgeWhiteSpace(szTmpValue);

	auto i = this->FindByKeyName( szTmpKey );
	if ( i == this->GetInvalidIndex() )
	{
		if ( pszValue )
		{
			//
			// Add the keyvalue to our list.
			//
			MDkeyvalue newkv;
			V_strcpy_safe( newkv.szKey, szTmpKey );
			V_strcpy_safe( newkv.szValue, szTmpValue );
			this->InsertKeyValue( newkv );
		}
	}
	else
	{
		if (pszValue != NULL)
		{
			V_strcpy_safe(this->m_KeyValues[i].szValue, szTmpValue);
		}
		//
		// If we are setting to a NULL value, delete the key.
		//
		else
		{
			this->RemoveKeyAt( i );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template<class Base>
void WCKeyValuesT<Base>::RemoveAll(void)
{
	this->m_KeyValues.RemoveAll();
}


// Explicit instantiations.
template class WCKeyValuesT<WCKVBase_Dict>;
template class WCKeyValuesT<WCKVBase_Vector>;



