//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/FileListManager.h"
#include "vgui_controls/FileOpenDialog.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/MessageBox.h"
#include "datamodel/idatamodel.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmattributevar.h"
#include "vgui/ISurface.h"
#include <vgui/IInput.h>
#include "vgui/MouseCode.h"
#include "tier1/strtools.h"
#include "tier1/KeyValues.h"
#include "tier2/tier2.h"
// dimhotepus: Drop Perforce support.
// #include "p4lib/ip4.h"
#include "filesystem.h"
#include "dme_controls/inotifyui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

template < int C >
int ListPanelStringSortFunc( vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 );

struct ColumnInfo_t
{
	char const	*columnName;
	char const	*columnText;
	int			startingWidth;
	int			flags;
	vgui::SortFunc			*pfnSort;
	vgui::Label::Alignment	alignment;
};

enum ColumnIndex_t
{
	CI_FILENAME,
	CI_PATH,
	CI_LOADED,
	CI_NUMELEMENTS,
	CI_CHANGED,
	CI_INPERFORCE,
	CI_OPENFOREDIT,
};

const int baseflags = vgui::ListPanel::COLUMN_UNHIDABLE;
const int fixedflags = vgui::ListPanel::COLUMN_UNHIDABLE | vgui::ListPanel::COLUMN_FIXEDSIZE;

static ColumnInfo_t g_ColInfo[] =
{
	{	"filename",		 "#BxFileManager_Filename",   150, baseflags,  ListPanelStringSortFunc< CI_FILENAME >,	  vgui::Label::a_west },
	{	"path",			 "#BxFileManager_Path",	      240, baseflags,  ListPanelStringSortFunc< CI_PATH >,		  vgui::Label::a_west },
	{	"loaded",		 "#BxFileManager_Loaded",      40, fixedflags, ListPanelStringSortFunc< CI_LOADED >,	  vgui::Label::a_center },
	{	"numelements",	 "#BxFileManager_NumElements", 60, fixedflags, ListPanelStringSortFunc< CI_NUMELEMENTS >, vgui::Label::a_east },
	{	"changed",		 "#BxFileManager_Changed",     50, fixedflags, ListPanelStringSortFunc< CI_CHANGED >,	  vgui::Label::a_center },
	{	"in_perforce",	 "#BxFileManager_P4Exists",    35, fixedflags, ListPanelStringSortFunc< CI_INPERFORCE >,  vgui::Label::a_center },
	{	"open_for_edit", "#BxFileManager_P4Edit",      40, fixedflags, ListPanelStringSortFunc< CI_OPENFOREDIT >, vgui::Label::a_center },
};

const char *GetKey( ColumnIndex_t ci )
{
	return g_ColInfo[ ci ].columnName;
}

template < int C >
int ListPanelStringSortFunc( [[maybe_unused]] vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	const char *pKey = GetKey( ( ColumnIndex_t )C );
	const char *string1 = item1.kv->GetString( pKey );
	const char *string2 = item2.kv->GetString( pKey );

	return Q_stricmp( string1, string2 );
}


CFileListManager::CFileListManager( vgui::Panel *parent ) : BaseClass( parent, "FileListManager" )
{
	SetMultiselectEnabled( true );
	SetVisible( true );
	m_bRefreshRequired = false;

	// dimhotepus: Scale UI.
	SetSize( QuickPropScale( 800 ), QuickPropScale( 200 ) );
	SetPos( QuickPropScale( 100 ), QuickPropScale( 100 ) );

	AddColumn( CI_FILENAME );
	AddColumn( CI_PATH );
	AddColumn( CI_LOADED );
	AddColumn( CI_NUMELEMENTS );
	AddColumn( CI_CHANGED );
	AddColumn( CI_INPERFORCE );
	AddColumn( CI_OPENFOREDIT );

	SetSortColumn( 0 );

	Refresh();

	SetScheme( vgui::scheme()->LoadSchemeFromFile( "Resource/BoxRocket.res", "BoxRocket" ) );
//	LoadControlSettings( "resource/BxFileListManager.res" );
}

intp CFileListManager::AddItem( DmFileId_t fileid, const char *pFilename, const char *pPath, bool bLoaded, int nElements, bool bChanged, bool bInPerforce, bool bOpenForEdit )
{
	KeyValuesAD kv( new KeyValues( "", GetKey( CI_FILENAME ), pFilename, GetKey( CI_PATH ), pPath ) );
	kv->SetInt   ( GetKey( CI_NUMELEMENTS ), nElements );
	kv->SetString( GetKey( CI_LOADED ),		 bLoaded	  ? "Y" : "N" );
	kv->SetString( GetKey( CI_CHANGED ),	 bChanged	  ? "Y" : "N" );
	kv->SetString( GetKey( CI_INPERFORCE ),  bInPerforce  ? "Y" : "N" );
	kv->SetString( GetKey( CI_OPENFOREDIT ), bOpenForEdit ? "Y" : "N" );
	intp itemID = BaseClass::AddItem( kv, fileid, false, false );
	return itemID;
}

void CFileListManager::SetLoaded( DmFileId_t fileid, bool bLoaded )
{
	CNotifyScopeGuard notify( "CFileListManager::SetLoaded", NOTIFY_SOURCE_FILE_LIST_MANAGER, NOTIFY_SETDIRTYFLAG );

	if ( bLoaded )
	{
		const char *pFilename = g_pDataModel->GetFileName( fileid );
		Assert( pFilename );
		if ( !pFilename )
			return;

		CDisableUndoScopeGuard guard;
		CDmElement *pRoot = NULL;
		g_pDataModel->RestoreFromFile( pFilename, NULL, NULL, &pRoot, CR_DELETE_NEW );
	}
	else
	{
		CDisableUndoScopeGuard guard;
		g_pDataModel->UnloadFile( fileid );
	}
}

void CFileListManager::OnMousePressed( vgui::MouseCode code )
{
	// determine where we were pressed
	int x, y, row, column;
	vgui::input()->GetCursorPos( x, y );
	GetCellAtPos( x, y, row, column );

	if ( code == MOUSE_LEFT )
	{
		bool bIsFakeToggleButton = column == CI_LOADED;
		if ( bIsFakeToggleButton && row >= 0 && row < GetItemCount() )
		{
			int itemID = GetItemIDFromRow( row );
			KeyValues *kv = GetItem( itemID );

			const char *pStr = kv->GetString( GetKey( ( ColumnIndex_t )column ), "" );
			Assert( *pStr == 'Y' || *pStr == 'N' );
			bool bSet = *pStr == 'N'; // bSet is the NEW state, not the old one
			kv->SetString( GetKey( ( ColumnIndex_t )column ), bSet ? "Y" : "N" );

			SetLoaded( ( DmFileId_t )GetItemUserData( itemID ), bSet );

			// get the key focus
			RequestFocus();
			return;
		}
	}
	else if ( code == MOUSE_RIGHT )
	{
		int itemID = -1;
		if ( row >= 0 && row < GetItemCount() )
		{
			itemID = GetItemIDFromRow( row );

			if ( !IsItemSelected( itemID ) )
			{
				SetSingleSelectedItem( itemID );
			}
		}

		KeyValuesAD kv( new KeyValues( "OpenContextMenu", "itemID", itemID ) );
		OnOpenContextMenu( kv );
		return;
	}

	BaseClass::OnMousePressed( code );
}

intp AddMenuItemHelper( vgui::Menu *pMenu, const char *pItemName, const char *pKVName, vgui::Panel *pTarget, bool bEnabled )
{
	intp id = pMenu->AddMenuItem( pItemName, new KeyValues( pKVName ), pTarget );
	pMenu->SetItemEnabled( id, bEnabled );
	return id;
}

void CFileListManager::OnOpenContextMenu( KeyValues *pParams )
{
	if ( m_hContextMenu.Get() )
	{
		delete m_hContextMenu.Get();
		m_hContextMenu = NULL;
	}

	m_hContextMenu = new vgui::Menu( this, "ContextMenu" );

	int itemID = pParams->GetInt( "itemID", -1 );
	if ( itemID < 0 )
	{
		AddMenuItemHelper( m_hContextMenu, "Open File...", "open", this, true ); // Is this how we should load other files???
	}
	else
	{
		// dimhotepus: Drop Perforce support.
		// bool bP4Connected = p4->IsConnectedToServer();

		int nSelected = GetSelectedItemsCount();
		int nLoaded = 0;
		int nChanged = 0;
		int nOnDisk = 0;
		int nInPerforce = 0;
		int nOpenForEdit = 0;
		for ( int i = 0; i < nSelected; ++i )
		{
			int itemId = GetSelectedItem( i );
			DmFileId_t fileid = ( DmFileId_t )GetItemUserData( itemId );
			if ( g_pDataModel->IsFileLoaded( fileid ) )
			{
				++nLoaded;
				++nChanged; // TODO - find out for real
			}
			const char *pFilename = g_pDataModel->GetFileName( fileid );
			if ( g_pFullFileSystem->FileExists( pFilename ) )
			{
				++nOnDisk;
			}

			// dimhotepus: Drop Perforce support.
			// if ( bP4Connected )
			// {
			// 	if ( p4->IsFileInPerforce( pFilename ) )
			// 	{
			// 		++nInPerforce;
			// 		if ( p4->GetFileState( pFilename ) != P4FILE_UNOPENED )
			// 		{
			// 			++nOpenForEdit;
			// 		}
			// 	}
			// }
		}

		AddMenuItemHelper( m_hContextMenu, "Load", "load", this, nLoaded < nSelected && nOnDisk > 0 );
		AddMenuItemHelper( m_hContextMenu, "Unload", "unload", this, nLoaded > 0 );
		AddMenuItemHelper( m_hContextMenu, "Save", "save", this, nChanged > 0 && nOnDisk == nSelected );
		AddMenuItemHelper( m_hContextMenu, "Save As...", "saveas", this, nLoaded == 1 && nSelected == 1 );
		AddMenuItemHelper( m_hContextMenu, "Add To Perforce", "p4add", this, nInPerforce < nSelected && nOnDisk > 0 );
		AddMenuItemHelper( m_hContextMenu, "Open For Edit", "p4edit", this, nOpenForEdit < nSelected && nOnDisk > 0 );
	}

	vgui::Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
}

void CFileListManager::OnLoadFiles( KeyValues * )
{
	CNotifyScopeGuard notify( "CFileListManager::OnLoadFiles", NOTIFY_SOURCE_FILE_LIST_MANAGER, NOTIFY_SETDIRTYFLAG );

	int nSelected = GetSelectedItemsCount();
	for ( int i = 0; i < nSelected; ++i )
	{
		int itemId = GetSelectedItem( i );
		DmFileId_t fileid = ( DmFileId_t )GetItemUserData( itemId );
		if ( !g_pDataModel->IsFileLoaded( fileid ) )
		{
			SetLoaded( fileid, true );
		}
	}

	Refresh();
}

void CFileListManager::OnUnloadFiles( KeyValues * )
{
	CNotifyScopeGuard notify( "CFileListManager::OnUnloadFiles", NOTIFY_SOURCE_FILE_LIST_MANAGER, NOTIFY_SETDIRTYFLAG );

	int nSelected = GetSelectedItemsCount();
	for ( int i = 0; i < nSelected; ++i )
	{
		int itemId = GetSelectedItem( i );
		DmFileId_t fileid = ( DmFileId_t )GetItemUserData( itemId );
		if ( g_pDataModel->IsFileLoaded( fileid ) )
		{
			SetLoaded( fileid, false );
		}
	}

	Refresh();
}

void CFileListManager::OnSaveFiles( KeyValues * )
{
	int nSelected = GetSelectedItemsCount();
	for ( int i = 0; i < nSelected; ++i )
	{
		int itemId = GetSelectedItem( i );
		DmFileId_t fileid = ( DmFileId_t )GetItemUserData( itemId );
		if ( !g_pDataModel->IsFileLoaded( fileid ) )
			continue;

		const char *pFilename = g_pDataModel->GetFileName( fileid );
		Assert( pFilename );
		if ( !pFilename )
			continue;

		CDmElement *pRoot = GetElement< CDmElement >( g_pDataModel->GetFileRoot( fileid ) );
		Assert( pRoot );
		if ( !pRoot )
			continue;

		const char *pFileFormat = g_pDataModel->GetFileFormat( fileid );
		const char *pEncoding = g_pDataModel->GetDefaultEncoding( pFileFormat );
		g_pDataModel->SaveToFile( pFilename, NULL, pEncoding, pFileFormat, pRoot );
	}

	Refresh();
}

void CFileListManager::OnOpenFile( KeyValues * )
{
	KeyValues *pContextKeyValues = new KeyValues( "OnOpen" );
	vgui::FileOpenDialog *pFileOpenDialog = new vgui::FileOpenDialog( this, "Save .dmx File As", false, pContextKeyValues );
	pFileOpenDialog->AddFilter( "*.dmx", "DmElements File (*.dmx)", true );
	pFileOpenDialog->AddActionSignalTarget( this );
	pFileOpenDialog->SetDeleteSelfOnClose( true );
	pFileOpenDialog->DoModal( false );
}

void CFileListManager::OnSaveFileAs( KeyValues * )
{
	int nSelected = GetSelectedItemsCount();
	Assert( nSelected == 1 );
	if ( nSelected != 1 )
		return;

	KeyValues *pContextKeyValues = new KeyValues( "OnSaveAs" );
	pContextKeyValues->SetInt( "itemId", GetSelectedItem( 0 ) );
	DmFileId_t fileid = ( DmFileId_t )GetItemUserData( GetSelectedItem( 0 ) );
	const char *pFileFormat = g_pDataModel->GetFileFormat( fileid );

	vgui::FileOpenDialog *pFileOpenDialog = new vgui::FileOpenDialog( this, "Save .dmx File As", false, pContextKeyValues );
	// if this control is moved to vgui_controls, change the default format to "dmx", the generic dmx format
	pFileOpenDialog->AddFilter( "*.dmx", "Generic MovieObjects File (*.dmx)", false, "movieobjects" );
	if ( V_strcmp( pFileFormat, "movieobjects" ) != 0 )
	{
		char description[ 256 ];
		V_snprintf( description, sizeof( description ), "%s (*.dmx)", g_pDataModel->GetFormatDescription( pFileFormat ) );
		pFileOpenDialog->AddFilter( "*.dmx", description, true, pFileFormat );
	}
	pFileOpenDialog->AddActionSignalTarget( this );
	pFileOpenDialog->SetDeleteSelfOnClose( true );
	pFileOpenDialog->DoModal( false );
}

void CFileListManager::OnFileSelected( KeyValues *pParams )
{
	const char *pFullPath = pParams->GetString( "fullpath" );
	if ( !pFullPath || !pFullPath[ 0 ] )
		return;

	KeyValues *pSaveAsKey = pParams->FindKey( "OnSaveAs" );
	if ( pSaveAsKey )
	{
		int itemId = pSaveAsKey->GetInt( "itemId", -1 );
		Assert( itemId != -1 );
		if ( itemId == -1 )
			return;

		DmFileId_t fileid = ( DmFileId_t )GetItemUserData( itemId );
		Assert( fileid != DMFILEID_INVALID );
		if ( fileid == DMFILEID_INVALID )
			return;

		CDmElement *pRoot = GetElement< CDmElement >( g_pDataModel->GetFileRoot( fileid ) );
		Assert( pRoot );
		if ( !pRoot )
			return;

		const char *pFormat = pParams->GetString( "filterinfo" );
		Assert( pFormat );
		if ( !pFormat )
			return;

		g_pDataModel->SetFileName( fileid, pFullPath );
		g_pDataModel->SaveToFile( pFullPath, NULL, g_pDataModel->GetDefaultEncoding( pFormat ), pFormat, pRoot );

		Refresh();
		return;
	}

	KeyValues *pOpenKey = pParams->FindKey( "OnOpen" );
	if ( pOpenKey )
	{
		CDmElement *pRoot = NULL;
		g_pDataModel->RestoreFromFile( pFullPath, NULL, NULL, &pRoot );

		Refresh();
		return;
	}
}

void CFileListManager::OnAddToPerforce( KeyValues * )
{
	//int nFileCount = 0;
	int nSelected = GetSelectedItemsCount();
	const char **ppFileNames = ( const char** )_alloca( nSelected * sizeof( char* ) );
	for ( int i = 0; i < nSelected; ++i )
	{
		int itemId = GetSelectedItem( i );
		DmFileId_t fileid = ( DmFileId_t )GetItemUserData( itemId );
		const char *pFilename = g_pDataModel->GetFileName( fileid );
		Assert( pFilename );
		if ( !pFilename )
			continue;

		//++nFileCount;
		ppFileNames[ i ] = pFilename;
	}
	
	// dimhotepus: Drop Perforce support.
	constexpr bool bSuccess = false; // p4->OpenFilesForAdd( nFileCount, ppFileNames );
	if constexpr ( !bSuccess )
	{
		// dimhotepus: Drop Perforce support.
		vgui::MessageBox *pError = new vgui::MessageBox( "Perforce Error!", "No Perforce Support" /*p4->GetLastError()*/, GetParent() );
		pError->SetSmallCaption( true );
		pError->DoModal();
	}

	Refresh();
}

void CFileListManager::OnOpenForEdit( KeyValues * )
{
	//int nFileCount = 0;
	int nSelected = GetSelectedItemsCount();
	const char **ppFileNames = ( const char** )_alloca( nSelected * sizeof( char* ) );
	for ( int i = 0; i < nSelected; ++i )
	{
		int itemId = GetSelectedItem( i );
		DmFileId_t fileid = ( DmFileId_t )GetItemUserData( itemId );
		const char *pFilename = g_pDataModel->GetFileName( fileid );
		Assert( pFilename );
		if ( !pFilename )
			continue;

		//++nFileCount;
		ppFileNames[ i ] = pFilename;
	}
	
	// dimhotepus: Drop Perforce support.
	constexpr bool bSuccess = false; // p4->OpenFilesForEdit( nFileCount, ppFileNames );
	if constexpr ( !bSuccess )
	{
		// dimhotepus: Drop Perforce support.
		vgui::MessageBox *pError = new vgui::MessageBox( "Perforce Error!", "No Perforce Support" /*p4->GetLastError()*/, GetParent() );
		pError->SetSmallCaption( true );
		pError->DoModal();
	}

	Refresh();
}

void CFileListManager::OnDataChanged( KeyValues *pParams )
{
	int nNotifyFlags = pParams->GetInt( "notifyFlags" );
	if ( ( nNotifyFlags & NOTIFY_CHANGE_TOPOLOGICAL ) == 0 )
		return;

	int nNotifySource = pParams->GetInt( "source" );
	if ( nNotifySource == NOTIFY_SOURCE_FILE_LIST_MANAGER )
		return;

	if ( !IsVisible() )
	{
		m_bRefreshRequired = true;
		return;
	}

	int nCount = GetItemCount();
	int nFiles = g_pDataModel->NumFileIds();
	bool bPerformFullRefresh = ( nCount != nFiles );
	if ( !bPerformFullRefresh )
	{
		const char *pNameKey = GetKey( CI_FILENAME );

		for ( int i = 0; i < nCount; ++i )
		{
			DmFileId_t fileid = g_pDataModel->GetFileId( i );
			const char *pFileName = g_pDataModel->GetFileName( fileid );
			if ( !pFileName || !*pFileName )
			{
				bPerformFullRefresh = true;
				break;
			}
			pFileName = V_UnqualifiedFileName( pFileName );

			KeyValues *pKeyValues = GetItem( i );
			bPerformFullRefresh = ( fileid != (DmFileId_t)GetItemUserData(i) ) || Q_stricmp( pFileName, pKeyValues->GetString( pNameKey ) );
			if ( bPerformFullRefresh )
				break;

			pKeyValues->SetInt   ( GetKey( CI_NUMELEMENTS ), g_pDataModel->NumElementsInFile( fileid ) );
			pKeyValues->SetString( GetKey( CI_LOADED ),		 g_pDataModel->IsFileLoaded( fileid )	  ? "Y" : "N" );
			pKeyValues->SetString( GetKey( CI_CHANGED ),	 false									  ? "Y" : "N" );
			ApplyItemChanges( i );
		}
	}

	if ( bPerformFullRefresh )
	{
		Refresh();
		return;
	}
}

void CFileListManager::Refresh()
{
	m_bRefreshRequired = false;
	RemoveAll();

	// dimhotepus: Drop Perforce support.
	const bool bP4Connected = /* p4 ? p4->IsConnectedToServer() :*/ false;

	int nFiles = g_pDataModel->NumFileIds();
	for ( int i = 0; i < nFiles; ++i )
	{
		DmFileId_t fileid = g_pDataModel->GetFileId( i );
		const char *pFileName = g_pDataModel->GetFileName( fileid );
		if ( !pFileName || !*pFileName )
			continue; // skip DMFILEID_INVALID and the default fileid ""

		bool bLoaded = g_pDataModel->IsFileLoaded( fileid );
		int nElements = g_pDataModel->NumElementsInFile( fileid );
		bool bChanged = false; // TODO - find out for real
		// dimhotepus: Drop Perforce support.
		bool bInPerforce = bP4Connected && false; // p4->IsFileInPerforce( pFileName );
		bool bOpenForEdit = bInPerforce && false; // p4->GetFileState( pFileName ) != P4FILE_UNOPENED;

		char path[ 256 ];
		V_ExtractFilePath( pFileName, path );

		AddItem( fileid, V_UnqualifiedFileName( pFileName ), path, bLoaded, nElements, bChanged, bInPerforce, bOpenForEdit );
	}
}

void CFileListManager::OnThink( )
{
	BaseClass::OnThink();
	if ( m_bRefreshRequired && IsVisible() )
	{
		Refresh();
	}
}

void CFileListManager::OnCommand( const char *cmd )
{
	// if ( !Q_stricmp( cmd, "foo" ) ) ...
	BaseClass::OnCommand( cmd );
}

void CFileListManager::AddColumn( int ci )
{
	// dimhotepus: Scale UI.
	AddColumnHeader(ci, g_ColInfo[ci].columnName, g_ColInfo[ci].columnText, QuickPropScale( g_ColInfo[ci].startingWidth ), g_ColInfo[ci].flags);
	SetSortFunc(ci, g_ColInfo[ci].pfnSort);
	SetColumnTextAlignment(ci, g_ColInfo[ci].alignment);
}


//-----------------------------------------------------------------------------
//
// CFileManagerFrame methods 
//
//-----------------------------------------------------------------------------
CFileManagerFrame::CFileManagerFrame( vgui::Panel *parent ) : BaseClass( parent, "FileManagerFrame" )
{
	SetTitle( "#BxFileManagerFrame", true );

	SetSizeable( true );
	SetCloseButtonVisible( false );
	// dimhotepus: Scale UI.
	SetMinimumSize( QuickPropScale( 200 ), QuickPropScale( 200 ) );

	SetVisible( true );
	
	// dimhotepus: Scale UI.
	SetSize( QuickPropScale( 800 ), QuickPropScale( 200 ) );
	SetPos( QuickPropScale( 100 ), QuickPropScale( 100 ) );

	m_pFileListManager = new CFileListManager( this );
	Refresh();

	SetScheme( vgui::scheme()->LoadSchemeFromFile( "Resource/BoxRocket.res", "BoxRocket" ) );
}

void CFileManagerFrame::Refresh()
{
	m_pFileListManager->Refresh();
}

void CFileManagerFrame::OnCommand( const char *cmd )
{
	BaseClass::OnCommand( cmd );
	m_pFileListManager->OnCommand( cmd );
}

void CFileManagerFrame::PerformLayout()
{
	BaseClass::PerformLayout();

	int iWidth, iHeight;
	GetSize( iWidth, iHeight );
	// dimhotepus: Scale UI.
	m_pFileListManager->SetPos( 0, QuickPropScale( GetCaptionHeight() ) );
	m_pFileListManager->SetSize( iWidth, iHeight - QuickPropScale( GetCaptionHeight() ) );
}
