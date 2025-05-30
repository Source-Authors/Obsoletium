//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GDVAR_H
#define GDVAR_H
#pragma once

#include "tier1/utlvector.h"
#include "tier1/tokenreader.h" // dvs: for MAX_STRING. Fix.


class MDkeyvalue;


enum GDIV_TYPE
{
	ivBadType = -1,
	ivAngle,
	ivTargetDest,
	ivTargetNameOrClass,
	ivTargetSrc,
	ivInteger,
	ivString,
	ivChoices,
	ivFlags,
	ivDecal,
	ivColor255,		// components are 0-255
	ivColor1,		// components are 0-1
	ivStudioModel,
	ivSprite,
	ivSound,
	ivVector,
	ivNPCClass,
	ivFilterClass,
	ivFloat,
	ivMaterial,
	ivScene,
	ivSide,			// One brush face ID.
	ivSideList,		// One or more brush face IDs, space delimited.
	ivOrigin,		// The origin of an entity, in the form "x y z".
	ivVecLine,		// An origin that draws a line back to the parent entity.
	ivAxis,			// The axis of rotation for a rotating entity, in the form "x0 y0 z0, x1 y1 z1".
	ivPointEntityClass,
	ivNodeDest,
	ivInstanceFile,			// used for hammer to know this field should display a browse button to find map files
	ivAngleNegativePitch,	// used for instance rotating when just a pitch value is present
	ivInstanceVariable,		// used for instance variables for easy hammer editing
	ivInstanceParm,			// used for instance parameter declaration

	ivMax					// count of types
};


//-----------------------------------------------------------------------------
// Defines an element in a choices/flags list. Choices values are strings;
// flags values are integers, hence the iValue and szValue members.
//-----------------------------------------------------------------------------
struct GDIVITEM
{
	unsigned long iValue;		// Bitflag value for ivFlags
	char szValue[MAX_STRING];	// String value for ivChoices
	char szCaption[MAX_STRING];	// Name of this choice
	BOOL bDefault;				// Flag set by default?
};


class GDinputvariable
{
	public:

		GDinputvariable();
		GDinputvariable( const char *szType, const char *szName );
		~GDinputvariable();

		BOOL InitFromTokens(TokenReader& tr);
		
		// functions:
		inline const char *GetName() const { return m_szName; }
		inline const char *GetLongName(void) const { return m_szLongName; }
		inline const char *GetDescription(void) const;

		inline intp GetFlagCount() const { return m_Items.Count(); }
		inline int GetFlagMask(intp nFlag) const;
		inline const char *GetFlagCaption(intp nFlag) const;
		
		inline intp GetChoiceCount() const { return m_Items.Count(); } //-V524
		inline const char *GetChoiceCaption(intp nChoice) const;

		inline GDIV_TYPE GetType() const { return m_eType; }
		const char *GetTypeText(void) const;
		
		inline void GetDefault(int *pnStore, intp storeLen) const
		{
			// dimhotepus: Ensure buffer has room.
			if (storeLen > 0)
				pnStore[0] = m_nDefault;
		}

		inline void GetDefault(char *pszStore, intp storeLen) const
		{ 
			V_strncpy(pszStore, m_szDefault, storeLen); 
		}
		
		GDIV_TYPE GetTypeFromToken(const char *pszToken) const;
		trtoken_t GetStoreAsFromType(GDIV_TYPE eType) const;

		const char *ItemStringForValue(const char *szValue) const;
		const char *ItemValueForString(const char *szString) const;

		BOOL IsFlagSet(unsigned int) const;
		void SetFlag(unsigned int, BOOL bSet);

		void ResetDefaults();

		void ToKeyValue(MDkeyvalue* pkv);
		void FromKeyValue(MDkeyvalue* pkv);

		inline bool IsReportable(void) const;
		inline bool IsReadOnly(void) const;

		GDinputvariable &operator =(GDinputvariable &Other);
		void Merge(GDinputvariable &Other);

		static const char *GetVarTypeName( GDIV_TYPE eType );

	private:

		// for choices/flags:
		CUtlVector<GDIVITEM> m_Items;

		static const char *m_pszEmpty;

		char m_szName[MAX_IDENT];
		char m_szLongName[MAX_STRING];
		char *m_pszDescription;

		GDIV_TYPE m_eType;

		int m_nDefault;
		char m_szDefault[MAX_STRING];

		int m_nValue;
		char m_szValue[MAX_STRING];

		bool m_bReportable;
		bool m_bReadOnly;

		friend class GDclass;
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *GDinputvariable::GetDescription(void) const
{
	if (m_pszDescription != NULL)
	{	
		return(m_pszDescription);
	}

	return(m_pszEmpty);
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether or not this variable is read only. Read only variables
//			cannot be edited in the Entity Properties dialog.
//-----------------------------------------------------------------------------
bool GDinputvariable::IsReadOnly(void) const
{
	return(m_bReadOnly);
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether or not this variable should be displayed in the Entity
//			Report dialog.
//-----------------------------------------------------------------------------
bool GDinputvariable::IsReportable(void) const
{
	return(m_bReportable);
}


//-----------------------------------------------------------------------------
// Returns the flag mask (eg 4096) for the flag at the given index. The
// array is packed, so it isn't just 1 >> nFlag.
//-----------------------------------------------------------------------------
int GDinputvariable::GetFlagMask(intp nFlag) const
{
	Assert(m_eType == ivFlags);
	return m_Items.Element(nFlag).iValue;
}


//-----------------------------------------------------------------------------
// Returns the caption text (eg "Only break on trigger") for the flag at the given index.
//-----------------------------------------------------------------------------
const char *GDinputvariable::GetFlagCaption(intp nFlag) const
{
	Assert(m_eType == ivFlags);
	return m_Items.Element(nFlag).szCaption;
}


//-----------------------------------------------------------------------------
// Returns the caption text (eg "Yes") for the choice at the given index.
//-----------------------------------------------------------------------------
const char *GDinputvariable::GetChoiceCaption(intp nChoice) const
{
	Assert(m_eType == ivChoices);
	return m_Items.Element(nChoice).szCaption;
}


#endif // GDVAR_H
