//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WCKEYVALUES_H
#define WCKEYVALUES_H
#pragma once


#include "tier0/dbg.h"
#include "tier1/utlvector.h"
#include "tier1/utldict.h"
#include <fstream>


constexpr inline int KEYVALUE_MAX_KEY_LENGTH{80};
constexpr inline int KEYVALUE_MAX_VALUE_LENGTH{512};


class MDkeyvalue 
{
	public:

		//
		// Constructors/Destructor.
		//
		inline MDkeyvalue(void);
		inline MDkeyvalue(const char *pszKey, const char *pszValue);
		inline ~MDkeyvalue();

		MDkeyvalue &operator =(const MDkeyvalue &other);
		
		inline void Set(const char *pszKey, const char *pszValue);
		inline const char *Key(void) const;
		inline const char *Value(void) const;

		//
		// Serialization functions.
		//
		int SerializeRMF(std::fstream &f, BOOL bRMF);
		int SerializeMAP(std::fstream &f, BOOL bRMF);

		char szKey[KEYVALUE_MAX_KEY_LENGTH];			// The name of this key.
		char szValue[KEYVALUE_MAX_VALUE_LENGTH];		// The value of this key, stored as a string.
};


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
MDkeyvalue::MDkeyvalue(void)
{
	szKey[0] = '\0';
	szValue[0] = '\0';
}


//-----------------------------------------------------------------------------
// Purpose: Constructor with assignment.
//-----------------------------------------------------------------------------
MDkeyvalue::MDkeyvalue(const char *pszKey, const char *pszValue)
{
	szKey[0] = '\0';
	szValue[0] = '\0';

	Set(pszKey, pszValue);
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
MDkeyvalue::~MDkeyvalue() = default;


//-----------------------------------------------------------------------------
// Purpose: Assigns a key and value.
//-----------------------------------------------------------------------------
void MDkeyvalue::Set(const char *pszKey, const char *pszValue)
{
	Assert(pszKey);
	Assert(pszValue);

	V_strcpy_safe(szKey, pszKey);
	V_strcpy_safe(szValue, pszValue);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the string keyname.
//-----------------------------------------------------------------------------
const char *MDkeyvalue::Key(void) const
{
	return szKey;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the string value of this keyvalue.
//-----------------------------------------------------------------------------
const char *MDkeyvalue::Value(void) const
{
	return szValue;
}


typedef CUtlVector<MDkeyvalue> KeyValueArray;


// Used in cases where there can be duplicate key names.
class WCKVBase_Vector
{
public:
	
	// Iteration helpers.
	inline intp GetCount() const			{ return m_KeyValues.Count(); }
	inline intp GetFirst() const			{ return m_KeyValues.Count() - 1; }
	inline intp GetNext( intp i ) const		{ return i - 1; }
	static constexpr inline intp GetInvalidIndex()	{ return -1; }

	void RemoveKeyAt(intp nIndex);
	intp FindByKeyName( const char *pKeyName ) const; // Returns the same value as GetInvalidIndex if not found.

	// Special function used for non-unique keyvalue lists.
	void AddKeyValue(const char *pszKey, const char *pszValue);

protected:

	void InsertKeyValue( const MDkeyvalue &kv );

protected:
	CUtlVector<MDkeyvalue> m_KeyValues;
};

// Used for most key/value sets because it's fast. Does not allow duplicate key names.
class WCKVBase_Dict
{
public:

	// Iteration helpers. Note that there is no GetCount() because you can't iterate
	// these by incrementing a counter.
	inline unsigned short GetFirst() const			{ return m_KeyValues.First(); }
	inline unsigned short GetNext( unsigned short i ) const	{ return m_KeyValues.Next( i ); }
	static inline unsigned short GetInvalidIndex()	{ return CUtlDict<MDkeyvalue,unsigned short>::InvalidIndex(); }

	unsigned short FindByKeyName( const char *pKeyName ) const; // Returns the same value as GetInvalidIndex if not found.
	void RemoveKeyAt(unsigned short nIndex);

protected:
	void InsertKeyValue( const MDkeyvalue &kv );

protected:
	CUtlDict<MDkeyvalue,unsigned short> m_KeyValues;
};


// See below for typedefs of this class you can use.
template<class Base>
class WCKeyValuesT : public Base
{
public:

	WCKeyValuesT(void) {}
	~WCKeyValuesT(void);

	void RemoveAll(void);
	void RemoveKey(const char *pszKey);

	void SetValue(const char *pszKey, const char *pszValue);
	void SetValue(const char *pszKey, int iValue);

	const char *GetKey(unsigned short nIndex) const;
	MDkeyvalue &GetKeyValue(unsigned short nIndex);
	const MDkeyvalue& GetKeyValue(unsigned short nIndex) const;
	const char *GetValue(unsigned short nIndex) const;
	const char *GetValue(const char *pszKey, intp *piIndex = NULL) const;
};


// These have explicit template instantiations so you can use them.
typedef WCKeyValuesT<WCKVBase_Dict> WCKeyValues;
typedef WCKeyValuesT<WCKVBase_Vector> WCKeyValuesVector;


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nIndex - 
//-----------------------------------------------------------------------------
template<class Base>
inline const char *WCKeyValuesT<Base>::GetKey(unsigned short nIndex) const
{
	return(this->m_KeyValues.Element(nIndex).szKey);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nIndex - 
// Output : MDKeyValue
//-----------------------------------------------------------------------------
template<class Base>
inline MDkeyvalue &WCKeyValuesT<Base>::GetKeyValue(unsigned short nIndex)
{
	return(this->m_KeyValues.Element(nIndex));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nIndex - 
// Output : MDkeyvalue
//-----------------------------------------------------------------------------
template<class Base>
inline const MDkeyvalue& WCKeyValuesT<Base>::GetKeyValue(unsigned short nIndex) const
{
	return(this->m_KeyValues.Element(nIndex));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nIndex - 
//-----------------------------------------------------------------------------
template<class Base>
inline const char *WCKeyValuesT<Base>::GetValue(unsigned short nIndex) const
{
	return(this->m_KeyValues.Element(nIndex).szValue);
}


void StripEdgeWhiteSpace(char *psz);


#endif // WCKEYVALUES_H
