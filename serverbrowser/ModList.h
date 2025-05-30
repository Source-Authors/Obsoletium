//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef MODLIST_H
#define MODLIST_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Handles parsing of half-life directory for mod info
//-----------------------------------------------------------------------------
class CModList final
{
public:
	CModList();

	intp GetIndex( const CGameID &iAppID ) const;
	void AddVGUIListener( vgui::VPANEL panel );

	// returns number of mods 
	intp ModCount() const;

	// returns the full name of the mod, index valid in range [0, ModCount)
	const char *GetModName( intp index ) const;

	// returns mod directory string
	const char *GetModDir( intp index ) const;

	const CGameID &GetAppID( intp index ) const;

	// returns the mod name for the associated gamedir
	const char *GetModNameForModDir( const CGameID &iAppID ) const;

private:
	struct mod_t
	{
		char description[64];
		char gamedir[64];
		CGameID	m_GameID;
		int m_InternalAppId;
		bool operator==( const mod_t& rhs ) const { return rhs.m_GameID == m_GameID; }
	};

	static int ModNameCompare( const mod_t *pLeft, const mod_t *pRight );
	void ParseInstalledMods();
	void ParseSteamMods();
	int LoadAppConfiguration( uint32 nAppID );

	CUtlVector<mod_t> m_ModList;
	CUtlVector<vgui::VPANEL> m_VGUIListeners;
};

// singleton accessor
extern CModList &ModList();


#endif // MODLIST_H
