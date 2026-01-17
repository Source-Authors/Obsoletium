//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Texture management functions. Exposes a list of available textures,
//			texture groups, and Most Recently Used textures.
//
//=============================================================================//

#ifndef TEXTURESYSTEM_H
#define TEXTURESYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "IEditorTexture.h"
#include "Material.h"
#include "tier1/utlvector.h"
#include "tier1/utldict.h"
#include "FileChangeWatcher.h"


class CGameConfig;
class CTextureSystem;


//-----------------------------------------------------------------------------
// Purpose: Defines the interface to a set of textures of a given texture format.
//			The textures are stored as an index into the global array of textures.
//-----------------------------------------------------------------------------
class CTextureGroup
{
public:
	CTextureGroup(const char *pszName);

	inline const char *GetName()
	{
		return(m_szName);
	}

	inline intp GetCount(void)
	{
		return m_Textures.Count();
	}

	inline TEXTUREFORMAT GetTextureFormat(void)
	{
		return(m_eTextureFormat);
	}

	inline void SetTextureFormat(TEXTUREFORMAT eTextureFormat)
	{
		m_eTextureFormat = eTextureFormat;
	}

	void AddTexture(IEditorTexture *pTexture);
	void Sort(void);

	IEditorTexture *GetTexture(intp nIndex);
	IEditorTexture* GetTexture( char const* pName );

	// Fast find texture..
	IEditorTexture* FindTextureByName( const char *pName, int *piIndex, TEXTUREFORMAT eDesiredFormat );

	// Used to lazily load in all the textures
	void LazyLoadTextures();

protected:

	char m_szName[MAX_PATH];
	TEXTUREFORMAT m_eTextureFormat;
	CUtlVector<IEditorTexture *> m_Textures;
	CUtlDict<intp,intp> m_TextureNameMap;	// Maps the texture name to an index into m_Textures (the key is IEditorTexture::GetName).

	// Used to lazily load the textures in the group
	intp	m_nTextureToLoad;
};


typedef CUtlVector<CTextureGroup *> TextureGroupList_t;


typedef struct tagGF
{
	char filename[MAX_PATH];
	DWORD id;
	int fd;
	TEXTUREFORMAT format;
	BOOL bLoaded;

} GRAPHICSFILESTRUCT;


//
// When the user switches game configs, all the textures and materials are switched.
// This structure holds all the context necessary to accomplish this.
//
struct TextureContext_t
{
	CGameConfig *pConfig;			// The game config that this texture context corresponds to.
	CTextureGroup *pAllGroup;
	TextureGroupList_t Groups;
	EditorTextureList_t MRU;		// List of Most Recently Used textures, first is the most recent.
	EditorTextureList_t Dummies;	// List of Dummy textures - textures that were created to hold the place of missing textures.
};


class CMaterialFileChangeWatcher : private CFileChangeWatcher::ICallbacks
{
public:
	// dimhotepus: Add members init.
	CMaterialFileChangeWatcher() : m_pTextureSystem{nullptr}, m_Context{-1} {}

	void Init( CTextureSystem *pSystem, intp context );
	void Update();	// Call this periodically to update.

private:
	// CFileChangeWatcher::ICallbacks..
	virtual void OnFileChange( const char *pRelativeFilename, const char *pFullFilename );

private:
	CFileChangeWatcher m_Watcher;
	CTextureSystem *m_pTextureSystem;
	// dimhotepus: x86-64 port.
	intp m_Context;	
};


class CTextureSystem : public IMaterialEnumerator
{
public:
	friend class CMaterialFileChangeWatcher;
	
	CTextureSystem(void);
	virtual ~CTextureSystem(void);

	bool Initialize(HWND hwnd);
	void ShutDown(void);

	void SetActiveConfig(CGameConfig *pConfig);

	//
	// Exposes a list of all texture (WAD) files.
	//
	inline intp FilesGetCount(void) const;
	inline void FilesGetInfo(GRAPHICSFILESTRUCT *pFileInfo, intp nIndex) const;
	bool FindGraphicsFile(GRAPHICSFILESTRUCT *pFileInfo, DWORD id, intp *piIndex = NULL);

	//
	// Exposes a list of texture groups (sets of textures of a given format).
	//
	void SetActiveGroup(const char *pcszName);
	inline intp GroupsGetCount() const;
	inline CTextureGroup *GroupsGet(intp nIndex) const;

	//
	// Exposes a list of active textures based on the currently active texture group.
	//
	inline intp GetActiveTextureCount(void) const;
	inline IEditorTexture *GetActiveTexture(intp nIndex) const;
	IEditorTexture *EnumActiveTextures(int *piIndex, TEXTUREFORMAT eDesiredFormat) const;
	IEditorTexture *FindActiveTexture(LPCSTR pszName, int *piIndex = NULL, BOOL bDummy = TRUE);
	bool HasTexturesForConfig(CGameConfig *pConfig);

	//
	// Exposes a list of Most Recently Used textures.
	//
	void AddMRU(IEditorTexture *pTex);
	inline intp MRUGetCount() const;
	inline IEditorTexture *MRUGet(intp nIndex) const;

	//
	// Exposes a list of all unique keywords found in the master texture list.
	//
	intp GetNumKeywords();
	const char *GetKeyword(intp index);

	//
	// Holds a list of placeholder textures used when a map refers to missing textures.
	//
	IEditorTexture *AddDummy(LPCTSTR pszName, TEXTUREFORMAT eFormat);

	//
	// Load graphics files from options list.
	//
	void LoadAllGraphicsFiles(void);
	void InformPaletteChanged(void);

	// IMaterialEnumerator interface, Used to add all the world materials into the material list.
	bool EnumMaterial( const char *pMaterialName, intp nContext ) override;

	// Used to lazily load in all the textures during app idle.
	void LazyLoadTextures();

	// Registers the keywords as existing in a particular material.
	void RegisterTextureKeywords( IEditorTexture *pTexture );

	// Opens the source file associated with a material.
	void OpenSource( const char *pMaterialName );

	// Reload individual textures.
	void ReloadTextures( const char *pFilterName );

	// bind local cubemap again
	void RebindDefaultCubeMap();
	
	void UpdateFileChangeWatchers();

	// Gets tools/toolsnodraw
	IEditorTexture* GetNoDrawTexture() { return m_pNoDrawTexture; }

	intp AddTexture( IEditorTexture *pTexture );

protected:

// CMaterialFileChangeWatcher stuff - watches for changes to VMTs or VTFs and handles them.

	enum EFileType
	{
		k_eFileTypeVMT,
		k_eFileTypeVTF
	};
	void OnFileChange( const char *pFilename, intp context, EFileType eFileType );
	void ReloadMaterialsUsingTexture( ITexture *pTestTexture );

	static bool GetFileTypeFromFilename( const char *pFilename, CTextureSystem::EFileType *pFileType );
	
	CUtlVector<CMaterialFileChangeWatcher*> m_ChangeWatchers;
			
// Internal stuff.

	void FreeAllTextures();

	TextureContext_t *AddTextureContext();
	TextureContext_t *FindTextureContextForConfig(CGameConfig *pConfig);

	void LoadMaterials(CGameConfig *pConfig);
	void LoadWADFiles(CGameConfig *pConfig);
	

	DWORD LoadGraphicsFile(const char *pFilename);
	void LoadGraphicsFileWAD3(GRAPHICSFILESTRUCT *pFile, int fd, CTextureGroup *pGroup);

	//
	// Array of open graphics files.
	//
	CUtlVector<GRAPHICSFILESTRUCT> m_GraphicsFiles;

	//
	// Master array of textures.
	//
	CUtlVector<IEditorTexture *> m_Textures;

	IEditorTexture *m_pLastTex;
	int m_nLastIndex;

	//
	// List of groups (sets of textures of a given texture format). Only one
	// group can be active at a time, based on the game configuration.
	//
	CUtlVector<TextureContext_t> m_TextureContexts;		// One per game config.
	TextureContext_t *m_pActiveContext;					// Points to the active entry in m_TextureContexts.
	CTextureGroup *m_pActiveGroup;						// Points to the active entry in m_TextureContexts.

	//
	// List of keywords found in all textures.
	//
	CUtlVector<const char *> m_Keywords;

	// default cubemap
	ITexture *m_pCubemapTexture;

	// tools/toolsnodraw
	IEditorTexture* m_pNoDrawTexture;
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
intp CTextureSystem::FilesGetCount(void) const
{
	return(m_GraphicsFiles.Count());
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pFileInfo - 
//			nIndex - 
//-----------------------------------------------------------------------------
void CTextureSystem::FilesGetInfo(GRAPHICSFILESTRUCT *pFileInfo, intp nIndex) const
{
	if (pFileInfo != NULL)
	{
		*pFileInfo = m_GraphicsFiles[nIndex];
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns the number of textures in the active group.
//-----------------------------------------------------------------------------
intp CTextureSystem::GetActiveTextureCount(void) const
{
	if (m_pActiveGroup != NULL)
	{
		return m_pActiveGroup->GetCount();
	}

	return(0);
}


IEditorTexture *CTextureSystem::GetActiveTexture(intp nIndex) const
{
	if (m_pActiveGroup != NULL)
	{
		return m_pActiveGroup->GetTexture(nIndex);
	}

	return NULL;
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
intp CTextureSystem::GroupsGetCount() const
{
	if (!m_pActiveContext)
		return 0;

	return m_pActiveContext->Groups.Count();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTextureGroup *CTextureSystem::GroupsGet(intp nIndex) const
{
	if (!m_pActiveContext)
		return NULL;

	return m_pActiveContext->Groups.Element(nIndex);
}


//-----------------------------------------------------------------------------
// Purpose: Initiates an iteration of the MRU list.
//-----------------------------------------------------------------------------
intp CTextureSystem::MRUGetCount() const
{
	if (!m_pActiveContext)
		return NULL;

	return m_pActiveContext->MRU.Count();
}


//-----------------------------------------------------------------------------
// Purpose: Returns the next texture in the MRU of the given format.
// Input  : pos - Iterator.
//			eDesiredFormat - Texture format to return.
// Output : Pointer to the texture.
//-----------------------------------------------------------------------------
IEditorTexture *CTextureSystem::MRUGet(intp nIndex) const
{
	if (!m_pActiveContext)
		return NULL;

	return m_pActiveContext->MRU.Element(nIndex);
}


extern CTextureSystem g_Textures;


#endif // TEXTURESYSTEM_H
