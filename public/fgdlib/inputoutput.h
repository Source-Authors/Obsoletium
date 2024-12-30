//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=============================================================================

#ifndef INPUTOUTPUT_H
#define INPUTOUTPUT_H
#ifdef _WIN32
#pragma once
#endif


#include "tier1/utlvector.h"
#include "fgdlib/EntityDefs.h"


enum InputOutputType_t
{
	iotInvalid = -1,
	iotVoid,
	iotInt,
	iotBool,
	iotString,
	iotFloat,
	iotVector,
	iotEHandle,
	iotColor,
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CClassInputOutputBase
{
	public:

		CClassInputOutputBase(void);
		CClassInputOutputBase(const char *pszName, InputOutputType_t eType);
		virtual ~CClassInputOutputBase(void);

		inline const char *GetName(void) const { return(m_szName); }
		InputOutputType_t GetType(void) const { return(m_eType);  }
		const char *GetTypeText(void) const;
		inline const char *GetDescription(void) const;

		inline void SetName(const char *szName) { V_strcpy_safe(m_szName, szName); }
		inline void SetType(InputOutputType_t eType) { m_eType = eType; }
		InputOutputType_t SetType(const char *szType);
		inline void SetDescription(char *pszDescription) { m_pszDescription = pszDescription; }

		CClassInputOutputBase &operator =(CClassInputOutputBase &);

	protected:

		static char *g_pszEmpty;

		char m_szName[MAX_IO_NAME_LEN];
		InputOutputType_t m_eType;
		char *m_pszDescription;
};


//-----------------------------------------------------------------------------
// Purpose: Returns this I/O's long description.
//-----------------------------------------------------------------------------
const char *CClassInputOutputBase::GetDescription(void) const
{
	if (m_pszDescription != NULL)
	{
		return(m_pszDescription);
	}

	return(g_pszEmpty);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CClassInput : public CClassInputOutputBase
{
	public:

		CClassInput(void);
		CClassInput(const char *pszName, InputOutputType_t eType);
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CClassOutput : public CClassInputOutputBase
{
	public:

		CClassOutput(void);
		CClassOutput(const char *pszName, InputOutputType_t eType);
};


typedef CUtlVector<CClassInput *> CClassInputList;
typedef CUtlVector<CClassOutput *> CClassOutputList;


#endif // INPUTOUTPUT_H
