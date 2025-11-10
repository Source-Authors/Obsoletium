//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "pch_serverbrowser.h"

#include "ModList.h"

//-----------------------------------------------------------------------------
// Purpose: Singleton accessor
//-----------------------------------------------------------------------------
CModList &ModList()
{
	static CModList s_ModList;
	return s_ModList;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CModList::CModList()
{
	ParseSteamMods();
}

//-----------------------------------------------------------------------------
// Purpose: returns number of mods 
//-----------------------------------------------------------------------------
intp CModList::ModCount() const
{
	return m_ModList.Count();
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const char *CModList::GetModName(intp index) const
{
	return m_ModList[index].description;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const char *CModList::GetModDir(intp index) const
{
	return m_ModList[index].gamedir;
}


//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const CGameID &CModList::GetAppID(intp index) const
{
	return m_ModList[index].m_GameID;
}


//-----------------------------------------------------------------------------
// Purpose: get the modlist index for this app id
//-----------------------------------------------------------------------------
intp CModList::GetIndex( const CGameID &iAppID ) const
{
	mod_t mod;
	mod.m_GameID = iAppID;
	return m_ModList.Find( mod );
}


//-----------------------------------------------------------------------------
// Purpose: returns the mod name for the associated gamedir
//-----------------------------------------------------------------------------
const char *CModList::GetModNameForModDir( const CGameID &gameID ) const
{
	intp iApp = GetIndex( gameID );
	if ( iApp != m_ModList.InvalidIndex() )
	{
		return m_ModList[iApp].description;
	}

	if ( ServerBrowserDialog().GetActiveModName() )
	{
		return ServerBrowserDialog().GetActiveGameName();
	}
	return "";
}


//-----------------------------------------------------------------------------
// Purpose: sort the mod list in alphabetical order
//-----------------------------------------------------------------------------
int CModList::ModNameCompare( const mod_t *pLeft, const mod_t *pRight )
{
	return ( Q_stricmp( pLeft->description, pRight->description ) ); 
}


//-----------------------------------------------------------------------------
// Purpose: gets list of steam games we can filter for
//-----------------------------------------------------------------------------
void CModList::ParseSteamMods()
{

}


//-----------------------------------------------------------------------------
// Purpose: load settings for an app
//-----------------------------------------------------------------------------
int CModList::LoadAppConfiguration( uint32 nAppID )
{

	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: add a vgui panel to message when the app list changes
//-----------------------------------------------------------------------------
void CModList::AddVGUIListener( vgui::VPANEL panel )
{
	m_VGUIListeners.AddToTail( panel );
}