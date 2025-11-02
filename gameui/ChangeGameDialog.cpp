//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "ChangeGameDialog.h"

#include <cstdio>

#include "posix_file_stream.h"
#include "winlite.h"

#include "ModInfo.h"
#include "EngineInterface.h"

#include "vgui_controls/ListPanel.h"
#include "tier1/KeyValues.h"
#include "tier1/strtools.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CChangeGameDialog::CChangeGameDialog(vgui::Panel *parent) : Frame(parent, "ChangeGameDialog")
{
	// dimhotepus: Scale UI.
	SetSize( QuickPropScale( 400 ), QuickPropScale( 340 ) );
	SetMinimumSize( QuickPropScale( 400 ), QuickPropScale( 340 ) );
	SetTitle("#GameUI_ChangeGame", true);

	m_pModList = new ListPanel(this, "ModList");
	m_pModList->SetEmptyListText("#GameUI_NoOtherGamesAvailable");
	// dimhotepus: Scale UI.
	m_pModList->AddColumnHeader(0, "ModName", "#GameUI_Game", QuickPropScale( 128 ) );

	LoadModList();
	LoadControlSettings("Resource/ChangeGameDialog.res");

	// if there's a mod in the list, select the first one
	if (m_pModList->GetItemCount() > 0)
	{
		m_pModList->SetSingleSelectedItem(m_pModList->GetItemIDFromRow(0));
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CChangeGameDialog::~CChangeGameDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Fills the mod list
//-----------------------------------------------------------------------------
void CChangeGameDialog::LoadModList()
{
	// look for third party games
	char szSearchPath[MAX_PATH + 5];
	V_strcpy_safe(szSearchPath, "*.*" );

	// use local filesystem since it has to look outside path system, and will never be used under steam
	WIN32_FIND_DATA wfd;
	BitwiseClear( wfd );
	
	HANDLE hResult = FindFirstFile(szSearchPath, &wfd);
	if (hResult != INVALID_HANDLE_VALUE)
	{
		BOOL bMoreFiles;
		while (1)
		{
			if ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (Q_strnicmp(wfd.cFileName, ".", 1)))
			{
				// Check for dlls\*.dll
				char szDllDirectory[MAX_PATH + 16];
				V_sprintf_safe(szDllDirectory, "%s" CORRECT_PATH_SEPARATOR_S "gameinfo.txt", wfd.cFileName);

				// dimhotepus: Use posix streams.
				auto [f, rc] = se::posix::posix_file_stream_factory::open(szDllDirectory, "rb");
				if (!rc)
				{
					// find the description
					int64_t size;
					std::tie(size, rc) = f.size();

					if (!rc && size < std::numeric_limits<size_t>::max()) 
					{
						std::unique_ptr<char[]> buf = std::make_unique<char[]>(static_cast<size_t>(size) + 1);
						size_t readSize;
						std::tie(readSize, rc) = f.read(buf.get(), static_cast<size_t>(size) + 1);

						if (!rc && size == readSize)
						{
							CModInfo modInfo;
							modInfo.LoadGameInfoFromBuffer(buf.get());

							if (strcmp(modInfo.GetGameName(), ModInfo().GetGameName()))
							{
								// Add the game directory.
								V_strlwr_safe(wfd.cFileName);

								KeyValuesAD itemData("Mod");
								itemData->SetString("ModName", modInfo.GetGameName());
								itemData->SetString("ModDir", wfd.cFileName);

								m_pModList->AddItem(itemData, 0, false, false);
							}
						}
					}
				}
			}
			bMoreFiles = FindNextFile(hResult, &wfd);
			if (!bMoreFiles)
				break;
		}
		
		FindClose(hResult);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChangeGameDialog::OnCommand(const char *command)
{
	if (!stricmp(command, "OK"))
	{
		if (m_pModList->GetSelectedItemsCount() > 0)
		{
			KeyValues *kv = m_pModList->GetItem(m_pModList->GetSelectedItem(0));
			if (kv)
			{
				// change the game dir and restart the engine
				char szCmd[256];
				V_sprintf_safe(szCmd, "_setgamedir %s\n", kv->GetString("ModDir"));
				engine->ClientCmd_Unrestricted(szCmd);

				// Force restart of entire engine
				engine->ClientCmd_Unrestricted("_restart\n");
			}
		}
	}
	else if (!stricmp(command, "Cancel"))
	{
		Close();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}






