//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef BASESAVEGAMEDIALOG_H
#define BASESAVEGAMEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"
#include "vgui/MouseCode.h"
#include "KeyValues.h"
#include "utlvector.h"


#define SAVEGAME_MAPNAME_LEN 32
#define SAVEGAME_COMMENT_LEN 80
#define SAVEGAME_ELAPSED_LEN 32

namespace vgui
{
	class Button;
};


struct SaveGameDescription_t
{
	char szShortName[64];
	char szFileName[128];
	char szMapName[SAVEGAME_MAPNAME_LEN];
	char szComment[SAVEGAME_COMMENT_LEN];
	char szType[64];
	char szElapsedTime[SAVEGAME_ELAPSED_LEN];
	char szFileTime[32];
#ifdef PLATFORM_64BITS
	// dimhotepus: unsigned -> time_t. x64 saves stored in a different dir and do not compatible with x86 anyway.
	time_t iTimestamp;
#else
	unsigned iTimestamp;
#endif
	unsigned int iSize;
};


int SaveReadNameAndComment( FileHandle_t f, OUT_Z_CAP(nameSize) char *name, int nameSize, OUT_Z_CAP(commentSize) char *comment, int commentSize );

template<int nameSize, int commentSize>
int SaveReadNameAndComment( FileHandle_t f, OUT_Z_ARRAY char (&name)[nameSize], OUT_Z_ARRAY char (&comment)[commentSize] )
{
	return SaveReadNameAndComment( f, name, nameSize, comment, commentSize );
}

//-----------------------------------------------------------------------------
// Purpose: Base class for save & load game dialogs
//-----------------------------------------------------------------------------
class CBaseSaveGameDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CBaseSaveGameDialog, vgui::Frame );

public:
	CBaseSaveGameDialog( vgui::Panel *parent, const char *name );
	static bool SaveGameSortFunc( const SaveGameDescription_t &lhs, const SaveGameDescription_t &rhs );

protected:
	CUtlVector<SaveGameDescription_t> m_SaveGames;
	vgui::PanelListPanel *m_pGameList;

	virtual void OnScanningSaveGames() {}

	void DeleteSaveGame( const char *fileName );
	void ScanSavedGames();
	void CreateSavedGamesList();
	intp GetSelectedItemSaveIndex();
	void AddSaveGameItemToList( intp saveIndex );

	bool ParseSaveData( char const *pszFileName, char const *pszShortName, SaveGameDescription_t &save );

	void OnKeyCodeTyped( vgui::KeyCode code ) override;
	void OnKeyCodePressed( vgui::KeyCode code ) override;

private:
	MESSAGE_FUNC( OnPanelSelected, "PanelSelected" );

	vgui::Button *m_pLoadButton;
};

constexpr inline char MOD_DIR[]{"MOD"};

constexpr inline char SAVE_DIR[]
{
  // dimhotepus: Dropped / at the end to unify all places.
#ifdef PLATFORM_64BITS
  "save/x64"
#else
  "save"
#endif
};

#endif // BASESAVEGAMEDIALOG_H
