//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef GAMEDATA_H
#define GAMEDATA_H
#ifdef _WIN32
#pragma once
#endif

#include <fstream>
#include "gdclass.h"
#include "inputoutput.h"
#include "tier1/tokenreader.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"


class MDkeyvalue;
class GameData;
class KeyValues;

enum TEXTUREFORMAT;

// dimhotepus: Moved from Hammer to be type-safe.
enum MWMSGTYPE { mwStatus, mwError, mwWarning };
// dimhotepus: Return old function.
using GameDataMessageFunc_t = void (*)(MWMSGTYPE level, PRINTF_FORMAT_STRING const char *fmt, ...);

// FGD-based AutoMaterialExclusion data

struct FGDMatExlcusions_s
{
	char szDirectory[MAX_PATH];		// Where we store the material exclusion directories
	bool bUserGenerated;			// If the user specified this ( default:  false -- FGD defined )
};

// FGD-based AutoVisGroup data

struct FGDVisGroupsBaseClass_s
{
	char szClass[MAX_PATH];				// i.e. Scene Logic, Sounds, etc   "Custom\Point Entities\Lights"
	CUtlStringList szEntities;			// i.e. func_viscluster
};

struct FGDAutoVisGroups_s
{
	char szParent[MAX_PATH];								// i.e. Custom, SFM, etc
	CUtlVector< FGDVisGroupsBaseClass_s >	m_Classes;		// i.e. Scene Logic, Sounds, etc
};

#define MAX_DIRECTORY_SIZE	32


//-----------------------------------------------------------------------------
// Purpose: Contains the set of data that is loaded from a single FGD file.
//-----------------------------------------------------------------------------
class GameData
{
	public:
		enum TNameFixup
		{
			NAME_FIXUP_PREFIX = 0,
			NAME_FIXUP_POSTFIX,
			NAME_FIXUP_NONE
		};

		GameData();
		~GameData();

		BOOL Load(const char *pszFilename);

		GDclass *ClassForName(const char *pszName, intp *piIndex = NULL);

		void ClearData();

		inline int GetMaxMapCoord() const;
		inline int GetMinMapCoord() const;

		inline intp GetClassCount() const;
		inline GDclass *GetClass(intp nIndex) const;

		GDclass *BeginInstanceRemap( const char *pszClassName, const char *pszInstancePrefix, Vector &Origin, QAngle &Angle );
		bool	RemapKeyValue( const char *pszKey, const char *pszInValue, OUT_Z_CAP(outLen) char *pszOutValue, intp outLen, TNameFixup NameFixup );
		template<intp outSize>
		bool	RemapKeyValue( const char *pszKey, const char *pszInValue, OUT_Z_ARRAY char (&pszOutValue)[outSize], TNameFixup NameFixup )
		{
			return RemapKeyValue( pszKey, pszInValue, pszOutValue, outSize, NameFixup );
		}
		bool	RemapNameField( const char *pszInValue, OUT_Z_CAP(outLen) char *pszOutValue, intp outLen, TNameFixup NameFixup );
		template<intp outSize>
		bool	RemapNameField( const char *pszInValue, OUT_Z_ARRAY char (&pszOutValue)[outSize], TNameFixup NameFixup )
		{
			return RemapNameField ( pszInValue, pszOutValue, outSize, NameFixup );
		}
		bool	LoadFGDMaterialExclusions( TokenReader &tr );
		bool	LoadFGDAutoVisGroups( TokenReader &tr );
		

		CUtlVector< FGDMatExlcusions_s >	m_FGDMaterialExclusions;

		CUtlVector< FGDAutoVisGroups_s >	m_FGDAutoVisGroups;

	private:

		bool ParseMapSize(TokenReader &tr);

		CUtlVector<GDclass *> m_Classes;

		int m_nMinMapCoord;		// Min & max map bounds as defined by the FGD.
		int m_nMaxMapCoord;

		// Instance Remapping
		Vector		m_InstanceOrigin;			// the origin offset of the instance
		QAngle		m_InstanceAngle;			// the rotation of the the instance
		matrix3x4_t	m_InstanceMat;				// matrix of the origin and rotation of rendering
		char		m_InstancePrefix[ 128 ];	// the prefix used for the instance name remapping
		GDclass		*m_InstanceClass;			// the entity class that is being remapped
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline intp GameData::GetClassCount() const
{
	return m_Classes.Count();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline GDclass *GameData::GetClass(intp nIndex) const
{
	if (nIndex >= m_Classes.Count())
		return nullptr;
		
	return m_Classes.Element(nIndex);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int GameData::GetMinMapCoord() const
{
	return m_nMinMapCoord;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int GameData::GetMaxMapCoord() const
{
	return m_nMaxMapCoord;
}

// dimhotepus: Return old func.
GameDataMessageFunc_t GDSetMessageFunc(GameDataMessageFunc_t pFunc);
bool GDError(TokenReader &tr, PRINTF_FORMAT_STRING const char *error, ...);
bool GDSkipToken(TokenReader &tr, trtoken_t ttexpecting = TOKENNONE, const char *pszExpecting = NULL);
bool GDGetToken(TokenReader &tr, OUT_Z_CAP(nSize) char *pszStore, intp nSize, trtoken_t ttexpecting = TOKENNONE, const char *pszExpecting = NULL);
template<intp size>
bool GDGetToken(TokenReader &tr, OUT_Z_ARRAY char (&pszStore)[size], trtoken_t ttexpecting = TOKENNONE, const char *pszExpecting = NULL)
{
	return GDGetToken(tr, pszStore, size, ttexpecting, pszExpecting);
}
bool GDGetTokenDynamic(TokenReader &tr, char **pszStore, trtoken_t ttexpecting, const char *pszExpecting = NULL);


#endif // GAMEDATA_H
