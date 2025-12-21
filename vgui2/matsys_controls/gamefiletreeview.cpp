//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "matsys_controls/gamefiletreeview.h"
#include "filesystem.h"
#include "tier1/KeyValues.h"
#include "vgui/ISurface.h"
#include "vgui/Cursor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;


//-----------------------------------------------------------------------------
// list of all tree view icons
//-----------------------------------------------------------------------------
enum
{
	IMAGE_FOLDER = 1,
	IMAGE_OPENFOLDER,
	IMAGE_FILE,
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CGameFileTreeView::CGameFileTreeView( Panel *parent, const char *name, const char *pRootFolderName, const char *pRootDir, const char *pExtension ) : BaseClass(parent, name), m_Images( false )
{
	m_RootDir = pRootDir;
	
	m_Ext = pExtension;
	m_bUseExt = ( pExtension != NULL );
	
	m_RootFolderName = pRootFolderName;
	// dimhotepus: Scale UI.
	// build our list of images
	m_Images.AddImage( scheme()->GetImage( "resource/icon_folder", false, IsProportional() ) );
	m_Images.AddImage( scheme()->GetImage( "resource/icon_folder_selected", false, IsProportional() ) );
	m_Images.AddImage( scheme()->GetImage( "resource/icon_file", false, IsProportional() ) );
	SetImageList( &m_Images, false );
}


//-----------------------------------------------------------------------------
// Purpose: Refreshes the active file list
//-----------------------------------------------------------------------------
void CGameFileTreeView::RefreshFileList()
{
	RemoveAll();
	SetFgColor(Color(216, 222, 211, 255));

	// add the base node
	KeyValuesAD pkv( "root" );
	pkv->SetString( "text", m_RootFolderName );
	pkv->SetInt( "root", 1 );
	pkv->SetInt( "expand", 1 );
	intp iRoot = AddItem( pkv, GetRootItemIndex() );
	ExpandItem( iRoot, true );
}

	
//-----------------------------------------------------------------------------
// Selects the root folder
//-----------------------------------------------------------------------------
void CGameFileTreeView::SelectRoot()
{
	AddSelectedItem( GetRootItemIndex(), true );
}


//-----------------------------------------------------------------------------
// Gets the number of root directories
//-----------------------------------------------------------------------------
intp CGameFileTreeView::GetRootDirectoryCount()
{
	return GetNumChildren( GetRootItemIndex() );
}


//-----------------------------------------------------------------------------
// Gets the ith root directory
//-----------------------------------------------------------------------------
const char *CGameFileTreeView::GetRootDirectory( intp nIndex )
{
	intp nItemIndex = GetChild( GetRootItemIndex(), nIndex );
	KeyValues *kv = GetItemData( nItemIndex );
	if ( !kv )
		return NULL;
	return kv->GetString( "path", NULL );
}


//-----------------------------------------------------------------------------
// Populate the root node (necessary since tree view can't have multiple roots)
//-----------------------------------------------------------------------------
void CGameFileTreeView::PopulateRootNode( intp itemIndex )
{
	AddDirectoriesOfNode( itemIndex, m_RootDir );

	if ( m_bUseExt )
	{
		AddFilesOfNode( itemIndex, m_RootDir, m_Ext );
	}
}


//-----------------------------------------------------------------------------
// Populate the root node with directories
//-----------------------------------------------------------------------------
bool CGameFileTreeView::DoesDirectoryHaveSubdirectories( const char *pFilePath )
{
	char pSearchString[MAX_PATH];
	V_sprintf_safe( pSearchString, "%s\\*", pFilePath );

	// get the list of files
	FileFindHandle_t findHandle = FILESYSTEM_INVALID_FIND_HANDLE;

	// generate children
	// add all the items
	const char *pszFileName = g_pFullFileSystem->FindFirstEx( pSearchString, "GAME", &findHandle );
	RunCodeAtScopeExit(g_pFullFileSystem->FindClose( findHandle ));

	while ( pszFileName )
	{
		bool bIsDirectory = g_pFullFileSystem->FindIsDirectory( findHandle );
		if ( bIsDirectory && Q_strnicmp( pszFileName, ".", 2 ) && Q_strnicmp( pszFileName, "..", 3 ) )
			return true;

		pszFileName = g_pFullFileSystem->FindNext( findHandle );
	}

	return false;
}

	
//-----------------------------------------------------------------------------
// Populate the root node with directories
//-----------------------------------------------------------------------------
void CGameFileTreeView::AddDirectoriesOfNode( intp itemIndex, const char *pFilePath )
{
	char pSearchString[MAX_PATH];
	V_sprintf_safe( pSearchString, "%s\\*", pFilePath );

	// get the list of files
	FileFindHandle_t findHandle = FILESYSTEM_INVALID_FIND_HANDLE;

	// generate children
	// add all the items
	const char *pszFileName = g_pFullFileSystem->FindFirstEx( pSearchString, "GAME", &findHandle );
	RunCodeAtScopeExit(g_pFullFileSystem->FindClose( findHandle ));

	while ( pszFileName )
	{
		bool bIsDirectory = g_pFullFileSystem->FindIsDirectory( findHandle );
		if ( bIsDirectory && Q_strnicmp( pszFileName, ".", 2 ) && Q_strnicmp( pszFileName, "..", 3 ) )
		{
			KeyValuesAD kv( new KeyValues( "node", "text", pszFileName ) );
			 
			char pFullPath[MAX_PATH];
			V_sprintf_safe( pFullPath, "%s/%s", pFilePath, pszFileName );
			Q_FixSlashes( pFullPath );
			Q_strlower( pFullPath );
			bool bHasSubdirectories = DoesDirectoryHaveSubdirectories( pFullPath );
			kv->SetString( "path", pFullPath );

			kv->SetInt( "expand", bHasSubdirectories );
			kv->SetInt( "dir", 1 );
			kv->SetInt( "image", IMAGE_FOLDER );

			intp itemID = AddItem(kv, itemIndex);

			// mark directories in orange
			SetItemColorForDirectories( itemID );
		}

		pszFileName = g_pFullFileSystem->FindNext( findHandle );
	}
}


//-----------------------------------------------------------------------------
// Populate the root node with files
//-----------------------------------------------------------------------------
void CGameFileTreeView::AddFilesOfNode( intp itemIndex, const char *pFilePath, const char *pExt )
{
	char pSearchString[MAX_PATH];
	V_sprintf_safe( pSearchString, "%s\\*.%s", pFilePath, pExt );

	// get the list of files
	FileFindHandle_t findHandle = FILESYSTEM_INVALID_FIND_HANDLE;

	// generate children
	// add all the items
	const char *pszFileName = g_pFullFileSystem->FindFirst( pSearchString, &findHandle );
	RunCodeAtScopeExit(g_pFullFileSystem->FindClose( findHandle ));

	char pFullPath[MAX_PATH];

	while ( pszFileName )
	{
		if ( !g_pFullFileSystem->FindIsDirectory( findHandle ) )
		{
			KeyValuesAD kv( new KeyValues( "node", "text", pszFileName ) );

			V_sprintf_safe( pFullPath, "%s\\%s", pFilePath, pszFileName );
			kv->SetString( "path", pFullPath );
			kv->SetInt( "image", IMAGE_FILE );

			AddItem(kv, itemIndex);
		}

		pszFileName = g_pFullFileSystem->FindNext( findHandle );
	}
}


//-----------------------------------------------------------------------------
// override to incremental request and show p4 directories
//-----------------------------------------------------------------------------
void CGameFileTreeView::GenerateChildrenOfNode(intp itemIndex)
{
	KeyValues *pkv = GetItemData(itemIndex);
	if ( pkv->GetInt("root") )
	{
		PopulateRootNode( itemIndex );
		return;
	}

	if (!pkv->GetInt("dir"))
		return;

	const char *pFilePath = pkv->GetString("path", "");
	if (!pFilePath[0])
		return;

	surface()->SetCursor(dc_waitarrow);

	AddDirectoriesOfNode( itemIndex, pFilePath );

	if ( m_bUseExt )
	{
		AddFilesOfNode( itemIndex, pFilePath, m_Ext );
	}
}


//-----------------------------------------------------------------------------
// setup a context menu whenever a directory is clicked on
//-----------------------------------------------------------------------------
void CGameFileTreeView::GenerateContextMenu( [[maybe_unused]] intp itemIndex, [[maybe_unused]] int x, [[maybe_unused]] int y ) 
{
	return;

	/*
	KeyValues *pkv = GetItemData(itemIndex);
	const char *pFilePath = pkv->GetString("path", "");
	if (!pFilePath[0])
		return;

	Menu *pContext = new Menu(this, "FileContext");
	pContext->AddMenuItem("Cloak folder", new KeyValues("CloakFolder", "item", itemIndex), GetParent(), NULL);

	// show the context menu
	pContext->SetPos(x, y);
	pContext->SetVisible(true);
	*/
}


//-----------------------------------------------------------------------------
// Sets an item to be colored as if its a menu
//-----------------------------------------------------------------------------
void CGameFileTreeView::SetItemColorForDirectories( intp itemID )
{
	// mark directories in orange
	SetItemFgColor( itemID, Color(224, 192, 0, 255) );
}


//-----------------------------------------------------------------------------
// setup a smaller font
//-----------------------------------------------------------------------------
void CGameFileTreeView::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetFont( pScheme->GetFont("DefaultSmall") );
}

