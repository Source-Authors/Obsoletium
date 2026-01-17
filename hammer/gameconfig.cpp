//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include <direct.h>
#include <io.h>
#include <WorldSize.h>
#include "Gameconfig.h"
#include "GlobalFunctions.h"
#include "fgdlib/HelperInfo.h"
#include "hammer.h"
#include "KeyValues.h"
#include "MapDoc.h"
#include "MapEntity.h"
#include "MapInstance.h"
#include "MapWorld.h"
#include "filesystem_tools.h"
#include "TextureSystem.h"
#include "tier1/strtools.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



constexpr inline int MAX_ERRORS = 5;


GameData *pGD;
CGameConfig g_DefaultGameConfig;
CGameConfig *g_pGameConfig = &g_DefaultGameConfig;


float g_MAX_MAP_COORD = 4096;
float g_MIN_MAP_COORD = -4096;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CGameConfig *CGameConfig::GetActiveGame(void)
{
	return g_pGameConfig;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pGame - 
//-----------------------------------------------------------------------------
void CGameConfig::SetActiveGame(CGameConfig *pGame)
{
	if (pGame != NULL)
	{
		g_pGameConfig = pGame;
		pGD = &pGame->GD;

		if (pGame->mapformat == mfHalfLife)
		{
			g_MAX_MAP_COORD = 4096;
			g_MIN_MAP_COORD = -4096;
		}
		else
		{
			g_MAX_MAP_COORD = pGD->GetMaxMapCoord();
			g_MIN_MAP_COORD = pGD->GetMinMapCoord();
		}
	}
	else
	{
		g_pGameConfig = &g_DefaultGameConfig;
		pGD = NULL;

		g_MAX_MAP_COORD = 4096;
		g_MIN_MAP_COORD = -4096;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Constructor. Maintains a static	counter uniquely identifying each
//			game configuration.
//-----------------------------------------------------------------------------
CGameConfig::CGameConfig(void)
{
	nGDFiles = 0;
	mapformat = mfQuake;
	textureformat = tfNone;
	m_fDefaultTextureScale = DEFAULT_TEXTURE_SCALE;
	m_nDefaultLightmapScale = DEFAULT_LIGHTMAP_SCALE;
	m_MaterialExcludeCount = 0;

	memset(szName, 0, sizeof(szName));
	memset(szExecutable, 0, sizeof(szExecutable));
	memset(szDefaultPoint, 0, sizeof(szDefaultPoint));
	memset(szDefaultSolid, 0, sizeof(szDefaultSolid));
	memset(szBSP, 0, sizeof(szBSP));
	memset(szLIGHT, 0, sizeof(szLIGHT));
	memset(szVIS, 0, sizeof(szVIS));
	memset(szMapDir, 0, sizeof(szMapDir));
	memset(m_szGameExeDir, 0, sizeof(m_szGameExeDir));
	memset(szBSPDir, 0, sizeof(szBSPDir));
	memset(m_szModDir, 0, sizeof(m_szModDir));
	V_strcpy_safe(m_szCordonTexture, "BLACK");

	m_szSteamDir[0] = '\0';
	m_szSteamUserDir[0] = '\0';
	m_szSteamAppID[0] = '\0';

	static DWORD __dwID = 0;
	dwID = __dwID++;
}


//-----------------------------------------------------------------------------
// Purpose: Imports an old binary GameCfg.wc file.
// Input  : file - 
//			fVersion - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CGameConfig::Import(std::fstream& file, float fVersion)
{
	file.read(szName, sizeof szName);
	file.read((char*)&nGDFiles, sizeof nGDFiles);
	file.read((char*)&textureformat, sizeof textureformat);

	if (fVersion >= 1.1f)
	{
		file.read((char*)&mapformat, sizeof mapformat);
	}
	else
	{
		mapformat = mfQuake;
	}

	//
	// If reading an old (pre 1.4) format file, skip past the obselete palette
	// file path.
	//
	if (fVersion < 1.4f)
	{
		char szPalette[128];
		file.read(szPalette, sizeof szPalette);
	}

	file.read(szExecutable, sizeof szExecutable);
	file.read(szDefaultSolid, sizeof szDefaultSolid);
	file.read(szDefaultPoint, sizeof szDefaultPoint);

	if (fVersion >= 1.2f)
	{
		file.read(szBSP, sizeof szBSP);
		file.read(szLIGHT, sizeof szLIGHT);
		file.read(szVIS, sizeof szVIS);
		file.read(m_szGameExeDir, sizeof m_szGameExeDir);
		file.read(szMapDir, sizeof szMapDir);
	}

	if (fVersion >= 1.3f)
	{
		file.read(szBSPDir, sizeof(szBSPDir));
	}

	if (fVersion >= 1.4f)
	{
		// CSG setting is gone now.
		char szTempCSG[128];
		file.read(szTempCSG, sizeof(szTempCSG));

		file.read(m_szModDir, sizeof(m_szModDir));
		
		// gamedir is gone now.
		char tempGameDir[128];
		file.read(tempGameDir, sizeof(tempGameDir));
	}

	// read game data files
	char szBuf[128];
	for(int i = 0; i < nGDFiles; i++)
	{
		file.read(szBuf, sizeof szBuf);
		GDFiles.Add(CString(szBuf));
	}

	LoadGDFiles();
	
	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Loads this game configuration from a keyvalue block.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGameConfig::Load(KeyValues *pkv)
{
	char szKey[MAX_PATH];

	// We should at least be able to get the game name and the game dir.
	Q_strncpy(szName, pkv->GetName(), sizeof(szName));
	Q_strncpy(m_szModDir, pkv->GetString("GameDir"), sizeof(m_szModDir));

	// Try to get the Hammer settings.
	KeyValues *pkvHammer = pkv->FindKey("Hammer");
	if (!pkvHammer)
		return true;

	//
	// Load the game data filenames from the "GameData0..GameDataN" keys.
	//
	nGDFiles = 0;
	bool bAdded = true;
	do //-V1044
	{
		V_sprintf_safe(szKey, "GameData%d", nGDFiles);
		const char *pszGameData = pkvHammer->GetString(szKey);
		if (pszGameData[0] != '\0')
		{
			GDFiles.Add(pszGameData);
			nGDFiles++;
		}
		else
		{
			bAdded = false;
		}

	} while (bAdded);

	textureformat = (TEXTUREFORMAT)pkvHammer->GetInt("TextureFormat", tfVMT);
	mapformat = (MAPFORMAT)pkvHammer->GetInt("MapFormat", mfHalfLife2);

	m_fDefaultTextureScale = pkvHammer->GetFloat("DefaultTextureScale", DEFAULT_TEXTURE_SCALE);
	if (m_fDefaultTextureScale == 0)
	{
		m_fDefaultTextureScale = DEFAULT_TEXTURE_SCALE;
	}

	m_nDefaultLightmapScale = pkvHammer->GetInt("DefaultLightmapScale", DEFAULT_LIGHTMAP_SCALE);

	Q_strncpy(szExecutable, pkvHammer->GetString("GameExe"), sizeof(szExecutable));
	Q_strncpy(szDefaultSolid, pkvHammer->GetString("DefaultSolidEntity"), sizeof(szDefaultSolid));
	Q_strncpy(szDefaultPoint, pkvHammer->GetString("DefaultPointEntity"), sizeof(szDefaultPoint));

	Q_strncpy(szBSP, pkvHammer->GetString("BSP"), sizeof(szBSP));
	Q_strncpy(szVIS, pkvHammer->GetString("Vis"), sizeof(szVIS));
	Q_strncpy(szLIGHT, pkvHammer->GetString("Light"), sizeof(szLIGHT));
	Q_strncpy(m_szGameExeDir, pkvHammer->GetString("GameExeDir"), sizeof(m_szGameExeDir));
	Q_strncpy(szMapDir, pkvHammer->GetString("MapDir"), sizeof(szMapDir));
	Q_strncpy(szBSPDir, pkvHammer->GetString("BSPDir"), sizeof(szBSPDir));

	SetCordonTexture( pkvHammer->GetString("CordonTexture", "BLACK") );

	char szExcludeDir[MAX_PATH];
	m_MaterialExcludeCount = pkvHammer->GetInt( "MaterialExcludeCount" );
	for ( int i = 0; i < m_MaterialExcludeCount; i++ )
	{
		V_sprintf_safe( szExcludeDir, "-MaterialExcludeDir%d", i );
		intp index = m_MaterialExclusions.AddToTail();
		Q_strncpy( m_MaterialExclusions[index].szDirectory, pkvHammer->GetString( szExcludeDir ), sizeof( m_MaterialExclusions[index].szDirectory ) ); 
		Q_StripTrailingSlash( m_MaterialExclusions[index].szDirectory );
		m_MaterialExclusions[index].bUserGenerated = true;
	}
	
	LoadGDFiles();
	
	return(true);
}


//-----------------------------------------------------------------------------
// Purpose: Saves this config's data into a keyvalues object.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGameConfig::Save(KeyValues *pkv)
{
	pkv->SetName(szName);
	pkv->SetString("GameDir", m_szModDir);


	// Try to get the Hammer settings.
	KeyValues *pkvHammer = pkv->FindKey("Hammer");
	if (pkvHammer)
	{
		pkv->RemoveSubKey(pkvHammer);
		pkvHammer->deleteThis();
	}

	pkvHammer = pkv->CreateNewKey();
	if (!pkvHammer)
		return false;

	pkvHammer->SetName("Hammer");

	//
	// Load the game data filenames from the "GameData0..GameDataN" keys.
	//
	for (int i = 0; i < nGDFiles; i++)
	{
		char szKey[MAX_PATH];
		V_sprintf_safe(szKey, "GameData%d", i);
		pkvHammer->SetString(szKey, GDFiles.GetAt(i));
	}

	pkvHammer->SetInt("TextureFormat", textureformat);
	pkvHammer->SetInt("MapFormat", mapformat);
	pkvHammer->SetFloat("DefaultTextureScale", m_fDefaultTextureScale);
	pkvHammer->SetInt("DefaultLightmapScale", m_nDefaultLightmapScale);

	pkvHammer->SetString("GameExe", szExecutable);
	pkvHammer->SetString("DefaultSolidEntity", szDefaultSolid);
	pkvHammer->SetString("DefaultPointEntity", szDefaultPoint);

	pkvHammer->SetString("BSP", szBSP);
	pkvHammer->SetString("Vis", szVIS);
	pkvHammer->SetString("Light", szLIGHT);
	pkvHammer->SetString("GameExeDir", m_szGameExeDir);
	pkvHammer->SetString("MapDir", szMapDir);
	pkvHammer->SetString("BSPDir", szBSPDir);

	pkvHammer->SetString("CordonTexture", m_szCordonTexture);

	char szExcludeDir[MAX_PATH];
	pkvHammer->SetInt("MaterialExcludeCount", m_MaterialExcludeCount);
	for (int i = 0; i < m_MaterialExcludeCount; i++)
	{
		V_sprintf_safe(szExcludeDir, "-MaterialExcludeDir%d", i );
		pkvHammer->SetString(szExcludeDir, m_MaterialExclusions[i].szDirectory);
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
//-----------------------------------------------------------------------------
void CGameConfig::Save(std::fstream &file)
{
	file.write(szName, sizeof szName);
	file.write((char*)&nGDFiles, sizeof nGDFiles);
	file.write((char*)&textureformat, sizeof textureformat);
	file.write((char*)&mapformat, sizeof mapformat);
	file.write(szExecutable, sizeof szExecutable);
	file.write(szDefaultSolid, sizeof szDefaultSolid);
	file.write(szDefaultPoint, sizeof szDefaultPoint);

	// 1.2
	file.write(szBSP, sizeof szBSP);
	file.write(szLIGHT, sizeof szLIGHT);
	file.write(szVIS, sizeof szVIS);
	file.write(m_szGameExeDir, sizeof(m_szGameExeDir));
	file.write(szMapDir, sizeof szMapDir);
	
	// 1.3
	file.write(szBSPDir, sizeof szBSPDir);

	// 1.4
	char tempCSG[128] = "";
	file.write(tempCSG, sizeof(tempCSG));

	file.write(m_szModDir, sizeof(m_szModDir));
	
	char tempGameDir[128] = "";
	file.write(tempGameDir, sizeof(tempGameDir));

	// write game data files
	char szBuf[128];
	for(int i = 0; i < nGDFiles; i++)
	{
		V_strcpy_safe(szBuf, GDFiles[i]);
		file.write(szBuf, sizeof szBuf);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pConfig - 
//-----------------------------------------------------------------------------
void CGameConfig::CopyFrom(CGameConfig *pConfig)
{
	nGDFiles = pConfig->nGDFiles;

	GDFiles.RemoveAll();
	GDFiles.Append(pConfig->GDFiles);

	V_strcpy_safe(szName, pConfig->szName);
	V_strcpy_safe(szExecutable, pConfig->szExecutable);
	V_strcpy_safe(szDefaultPoint, pConfig->szDefaultPoint);
	V_strcpy_safe(szDefaultSolid, pConfig->szDefaultSolid);
	V_strcpy_safe(szBSP, pConfig->szBSP);
	V_strcpy_safe(szLIGHT, pConfig->szLIGHT);
	V_strcpy_safe(szVIS, pConfig->szVIS);
	V_strcpy_safe(szMapDir, pConfig->szMapDir);
	V_strcpy_safe(m_szGameExeDir, pConfig->m_szGameExeDir);
	V_strcpy_safe(szBSPDir, pConfig->szBSPDir);
	V_strcpy_safe(m_szModDir, pConfig->m_szModDir);

	pConfig->m_MaterialExcludeCount = m_MaterialExcludeCount;
	for( int i = 0; i < m_MaterialExcludeCount; i++ )
	{
		V_strcpy_safe( m_MaterialExclusions[i].szDirectory, pConfig->m_MaterialExclusions[i].szDirectory );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pEntity - 
//			pGD - 
// Output : Returns TRUE to keep enumerating.
//-----------------------------------------------------------------------------
static BOOL UpdateClassPointer(CMapEntity *pEntity, const GameData *gd)
{
	GDclass *pClass = gd->ClassForName(pEntity->GetClassName());
	pEntity->SetClass(pClass);
	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameConfig::LoadGDFiles(void)
{
	GD.ClearData();
	
	// Save the old working directory
	char szOldDir[MAX_PATH];
	if ( !_getcwd( szOldDir, sizeof(szOldDir) ) )
	{
		Warning( "Can't get current dir for load GDF files: %s.\n",
			std::generic_category().message(errno).c_str() );
	}

	// Set our working directory properly
	char szAppDir[MAX_PATH];
	APP()->GetDirectory( DIR_PROGRAM, szAppDir );
	if ( _chdir( szAppDir ) )
	{
		Warning( "Can't change current dir to '%s' for load GDF files: %s.\n",
			szAppDir, std::generic_category().message(errno).c_str() );
	}

	for (int i = 0; i < nGDFiles; i++)
	{
		GD.Load(GDFiles[i]);
	}

	// Reset our old working directory
	if ( _chdir( szOldDir ) )
	{
		Warning( "Can't restore current dir to '%s' for load GDF files: %s.\n",
			szOldDir, std::generic_category().message(errno).c_str() );
	}

	// All the class pointers have changed - now we have to
	// reset all the class pointers in each map doc that 
	// uses this game.
	for ( intp i=0; i<CMapDoc::GetDocumentCount(); i++ )
	{
		CMapDoc *pDoc = CMapDoc::GetDocument(i);

		if (pDoc->GetGame() == this)
		{
			CMapWorld *pWorld = pDoc->GetMapWorld();
			pWorld->SetClass(GD.ClassForName(pWorld->GetClassName()));
			pWorld->EnumChildren(&UpdateClassPointer, const_cast<const GameData*>(&GD));
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Searches for the given filename, starting in szStartDir and looking
//			up the directory tree.
// Input:	szFile - the name of the file to search for.
//			szStartDir - the folder to start searching from, towards the root.
//			szFoundPath - receives the full path of the FOLDER where szFile was found.
// Output : Returns true if the file was found, false if not. If the file was
//			found the full path (not including the filename) is returned in szFoundPath.
//-----------------------------------------------------------------------------
template<size_t foundPathLen>
bool FindFileInTree(const char *szFile, const char *szStartDir, char (&szFoundPath)[foundPathLen])
{
	if ((szFile == NULL) || (szStartDir == NULL))
	{
		return false;
	}

	char szRoot[MAX_PATH];
	V_strcpy_safe(szRoot, szStartDir);
	V_AppendSlash(szRoot);

	char szTemp[MAX_PATH];
	do
	{
		V_strcpy_safe(szTemp, szRoot);
		V_strcat_safe(szTemp, szFile);

		if (!_access(szTemp, 0))
		{
			V_strcpy_safe(szFoundPath, szRoot);
			V_StripTrailingSlash(szFoundPath);
			return true;
		}

	} while (V_StripLastDir(szRoot));

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szDir - 
//			*szSteamDir - 
//			*szSteamUserDir - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
template<size_t steamDirSize, size_t steamUserDirSize>
bool FindSteamUserDir(const char *szAppDir, const char (&szSteamDir)[steamDirSize], char (&szSteamUserDir)[steamUserDirSize])
{
	if (szAppDir == NULL)
	{
		return false;
	}

	// If the szAppDir was run from within the steam tree, we should be able to find the steam user dir.
	intp nSteamDirLen = V_strlen(szSteamDir);
	if (!Q_strnicmp(szAppDir, szSteamDir, nSteamDirLen ) && (szAppDir[nSteamDirLen] == '\\'))
	{
		V_strcpy_safe(szSteamUserDir, szAppDir);

		char *pszSlash = strchr(&szSteamUserDir[nSteamDirLen + 1], '\\');
		if (pszSlash)
		{
			pszSlash++;

			pszSlash = strchr(pszSlash, '\\');
			if (pszSlash)
			{
				*pszSlash = '\0';
				return true;
			}
		}
	}

	szSteamUserDir[0] = '\0';

	return false;
}

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgoff.h"

//-----------------------------------------------------------------------------
// Purpose: Loads the settings from <mod dir>\gameinfo.txt into data members.
//-----------------------------------------------------------------------------
void CGameConfig::ParseGameInfo()
{
	auto pkv = KeyValues::AutoDelete("gameinfo.txt");
	if (!pkv->LoadFromFile(g_pFileSystem, "gameinfo.txt", "GAME"))
	{
		return;
	}

	KeyValues *pKey = pkv->FindKey("FileSystem");
	if (pKey)
	{
		V_strcpy_safe( m_szSteamAppID, pKey->GetString( "SteamAppId", "" ) );
	}

	const char *InstancePath = pkv->GetString( "InstancePath", NULL );
	if ( InstancePath )
	{
		CMapInstance::SetInstancePath( InstancePath );
	}

	char szAppDir[MAX_PATH];
	APP()->GetDirectory(DIR_PROGRAM, szAppDir);
	if (!FindFileInTree("steam.exe", szAppDir, m_szSteamDir))
	{
		// Couldn't find steam.exe in the hammer tree
		m_szSteamDir[0] = '\0';
	}

	if (!FindSteamUserDir(szAppDir, m_szSteamDir, m_szSteamUserDir))
	{
		m_szSteamUserDir[0] = '\0';
	}
}


//-----------------------------------------------------------------------------
// Accessor methods to get at the mod + the game (*not* full paths)
//-----------------------------------------------------------------------------
const char *CGameConfig::GetMod()
{
	// Strip path from modDir
	char szModPath[MAX_PATH];
	static char szMod[MAX_PATH];
	V_strcpy_safe( szModPath, m_szModDir );
	Q_StripTrailingSlash( szModPath );
	if ( Q_isempty(szModPath) )
	{
		V_strcpy_safe( szModPath, "hl2" );
	}

	Q_FileBase( szModPath, szMod );

	return szMod;
}

const char *CGameConfig::GetGame()
{
	return "hl2";

//	// Strip path from modDir
//	char szGamePath[MAX_PATH];
//	static char szGame[MAX_PATH];
//	Q_strncpy( szGamePath, m_szGameDir, MAX_PATH );
//	Q_StripTrailingSlash( szGamePath );
//	Q_FileBase( szGamePath, szGame );

//	return szGame;
}


