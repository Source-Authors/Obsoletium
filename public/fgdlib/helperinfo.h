//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=============================================================================

#ifndef HELPERINFO_H
#define HELPERINFO_H
#pragma once

#include "tier0/dbg.h"
#include "tier1/utlvector.h"


#define MAX_HELPER_NAME_LEN			256


typedef CUtlVector<char *> CParameterList;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHelperInfo
{
	public:

		inline CHelperInfo(void);
		inline ~CHelperInfo(void);

		inline const char *GetName(void) const { return(m_szName); }
		inline void SetName(const char *pszName);

		inline bool AddParameter(const char *pszParameter);

		inline intp GetParameterCount(void) const { return(m_Parameters.Count()); }
		inline const char *GetParameter(intp nIndex) const;

	protected:

		char m_szName[MAX_HELPER_NAME_LEN];
		CParameterList m_Parameters;
};



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline CHelperInfo::CHelperInfo(void)
{
	m_szName[0] = '\0';
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline CHelperInfo::~CHelperInfo(void)
{
	m_Parameters.PurgeAndDeleteElementsArray();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : char *pszParameter - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
inline bool CHelperInfo::AddParameter(const char *pszParameter)
{
	if (pszParameter && !Q_isempty(pszParameter))
	{
		m_Parameters.AddToTail(V_strdup(pszParameter));
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline const char *CHelperInfo::GetParameter(intp nIndex) const
{
	if (nIndex >= m_Parameters.Count())
		return NULL;
	
	return m_Parameters.Element(nIndex);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : char *pszName - 
//-----------------------------------------------------------------------------
inline void CHelperInfo::SetName(const char *pszName)
{
	if (pszName != NULL)
	{	
		V_strcpy_safe(m_szName, pszName);
	}
}


typedef CUtlVector<CHelperInfo *> CHelperInfoList;


#endif // HELPERINFO_H
