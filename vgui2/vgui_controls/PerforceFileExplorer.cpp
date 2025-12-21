//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Contains a list of files, determines their perforce status
//
// $NoKeywords: $
//===========================================================================//

#include <vgui_controls/PerforceFileExplorer.h>

#include "tier1/KeyValues.h"
#include "vgui/ISystem.h"
#include "filesystem.h"
// dimhotepus: No perforce
// #include "p4lib/ip4.h"
#include "tier2/tier2.h"

#include <vgui_controls/PerforceFileList.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Tooltip.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


using namespace vgui;


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
PerforceFileExplorer::PerforceFileExplorer( Panel *pParent, const char *pPanelName ) : 
	BaseClass( pParent, pPanelName )
{
	m_pFileList = new PerforceFileList( this, "PerforceFileList" );

	// Get the list of available drives and put them in a menu here.
	// Start with the directory we are in.
	m_pFullPathCombo = new ComboBox( this, "FullPathCombo", 8, false );
	m_pFullPathCombo->GetTooltip()->SetTooltipFormatToSingleLine();

	char pFullPath[MAX_PATH];
	g_pFullFileSystem->GetCurrentDirectory( pFullPath );
	SetCurrentDirectory( pFullPath );

	m_pFullPathCombo->AddActionSignalTarget( this );

	m_pFolderUpButton = new Button(this, "FolderUpButton", "", this);
	m_pFolderUpButton->GetTooltip()->SetText( "#FileOpenDialog_ToolTip_Up" );
	m_pFolderUpButton->SetCommand( new KeyValues( "FolderUp" ) );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
PerforceFileExplorer::~PerforceFileExplorer()
{
}


//-----------------------------------------------------------------------------
// Inherited from Frame
//-----------------------------------------------------------------------------
void PerforceFileExplorer::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	// dimhotepus: Scale UI.
	m_pFolderUpButton->AddImage( scheme()->GetImage( "resource/icon_folderup", false, IsProportional()), -3 );
}

	
//-----------------------------------------------------------------------------
// Inherited from Frame
//-----------------------------------------------------------------------------
void PerforceFileExplorer::PerformLayout()
{
	BaseClass::PerformLayout();

	int x, y, w, h;
	GetClientArea( x, y, w, h );
	
	// dimhotepus: Scale UI.
	m_pFullPathCombo->SetBounds( x, y + QuickPropScale( 6 ), w - QuickPropScale( 30 ), QuickPropScale( 24 ) ); 
	m_pFolderUpButton->SetBounds( x + w - QuickPropScale( 24 ), y + QuickPropScale( 6 ), QuickPropScale( 24 ), QuickPropScale( 24 ) );

	// dimhotepus: Scale UI.
	m_pFileList->SetBounds( x, y + QuickPropScale( 36 ), w, h - QuickPropScale( 36 ) );
}

	
//-----------------------------------------------------------------------------
// Sets the current directory
//-----------------------------------------------------------------------------
void PerforceFileExplorer::SetCurrentDirectory( const char *pFullPath )
{
	if ( !pFullPath )
		return;

	while ( isspace( *pFullPath ) )
	{
		++pFullPath;
	}

	if ( !pFullPath[0] )
		return;

	m_CurrentDirectory = pFullPath;
	m_CurrentDirectory.StripTrailingSlash();
    m_CurrentDirectory.FixSlashes();

	PopulateFileList();
	PopulateDriveList();

	char pCurrentDirectory[ MAX_PATH ];
	m_pFullPathCombo->GetText( pCurrentDirectory );
	if ( Q_stricmp( m_CurrentDirectory.Get(), pCurrentDirectory ) )
	{
		char pNewDirectory[ MAX_PATH ];
		V_sprintf_safe( pNewDirectory, "%s\\", m_CurrentDirectory.Get() );
		m_pFullPathCombo->SetText( pNewDirectory );
		m_pFullPathCombo->GetTooltip()->SetText( pNewDirectory );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PerforceFileExplorer::PopulateDriveList()
{
	char pFullPath[MAX_PATH * 4];
	char pSubDirPath[MAX_PATH * 4];
	V_strcpy_safe( pFullPath, m_CurrentDirectory.Get() );
	V_strcpy_safe( pSubDirPath, m_CurrentDirectory.Get() );

	m_pFullPathCombo->DeleteAllItems();

	// populate the drive list
	char buf[512];
	int len = system()->GetAvailableDrives(buf, 512);
	char *pBuf = buf;
	for (int i=0; i < len / 4; i++)
	{
		m_pFullPathCombo->AddItem(pBuf, NULL);

		// is this our drive - add all subdirectories
		if ( !_strnicmp( pBuf, pFullPath, 2 ) )
		{
			int indent = 0;
			char *pData = pFullPath;
			while (*pData)
			{
				if (*pData == '\\')
				{
					if (indent > 0)
					{
						memset(pSubDirPath, ' ', indent);
						memcpy(pSubDirPath+indent, pFullPath, pData-pFullPath+1);
						pSubDirPath[indent+pData-pFullPath+1] = 0;

						m_pFullPathCombo->AddItem( pSubDirPath, NULL );
					}
					indent += 2;
				}
				pData++;
			}
		}
		pBuf += 4;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Fill the filelist with the names of all the files in the current directory
//-----------------------------------------------------------------------------
void PerforceFileExplorer::PopulateFileList()
{
	// clear the current list
	m_pFileList->RemoveAllFiles();
	
	// Create filter string
	char pFullFoundPath[MAX_PATH];
	char pFilter[MAX_PATH+3];
	V_sprintf_safe( pFilter, "%s\\*.*", m_CurrentDirectory.Get() );

	// Find all files on disk
	FileFindHandle_t h = FILESYSTEM_INVALID_FIND_HANDLE;
	const char *pFileName = g_pFullFileSystem->FindFirstEx( pFilter, NULL, &h );
	RunCodeAtScopeExit(g_pFullFileSystem->FindClose( h ));

	for ( ; pFileName; pFileName = g_pFullFileSystem->FindNext( h ) )
	{
		if ( !Q_stricmp( pFileName, ".." ) || !Q_stricmp( pFileName, "." ) )
			continue;

		if ( !Q_IsAbsolutePath( pFileName ) )
		{
			V_sprintf_safe( pFullFoundPath, "%s\\%s", m_CurrentDirectory.Get(), pFileName );
			pFileName = pFullFoundPath;
		}

		int nItemID = m_pFileList->AddFile( pFileName, true );
		m_pFileList->RefreshPerforceState( nItemID, true, NULL );
	}

	// dimhotepus: No perforce
	// Now find all files in perforce
	//CUtlVector<P4File_t> &fileList = p4->GetFileList( m_CurrentDirectory );
	//int nCount = fileList.Count();
	//for ( int i = 0; i < nCount; ++i )
	//{
	//	pFileName = p4->String( fileList[i].m_sLocalFile );
	//	if ( !pFileName[0] )
	//		continue;

	//	int nItemID = m_pFileList->FindFile( pFileName );
	//	bool bFileExists = true;
	//	if ( nItemID == m_pFileList->InvalidItemID() )
	//	{
	//		// If it didn't find it, the file must not exist 
	//		// since it already would have added it above
	//		bFileExists = false;
	//		nItemID = m_pFileList->AddFile( pFileName, false, fileList[i].m_bDir );
	//	}
	//	m_pFileList->RefreshPerforceState( nItemID, bFileExists, &fileList[i] );
	//}

	m_pFileList->SortList();
}


//-----------------------------------------------------------------------------
// Purpose: Handle an item in the Drive combo box being selected
//-----------------------------------------------------------------------------
void PerforceFileExplorer::OnTextChanged( KeyValues *kv )
{
	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );

	// first check which control had its text changed!
	if ( pPanel == m_pFullPathCombo )
	{
		char pCurrentDirectory[ MAX_PATH ];
		m_pFullPathCombo->GetText( pCurrentDirectory );
		SetCurrentDirectory( pCurrentDirectory );
		return;
	}
}


//-----------------------------------------------------------------------------
// Called when the file list was doubleclicked
//-----------------------------------------------------------------------------
void PerforceFileExplorer::OnItemDoubleClicked()
{
	if ( m_pFileList->GetSelectedItemsCount() != 1 )
		return;

	int nItemID = m_pFileList->GetSelectedItem( 0 );
	if ( m_pFileList->IsDirectoryItem( nItemID ) )
	{
		const char *pDirectoryName = m_pFileList->GetFile( nItemID );
		SetCurrentDirectory( pDirectoryName );
	}
}


//-----------------------------------------------------------------------------
// Called when the folder up button was hit
//-----------------------------------------------------------------------------
void PerforceFileExplorer::OnFolderUp()
{
	char pUpDirectory[MAX_PATH];
	V_strcpy_safe( pUpDirectory, m_CurrentDirectory.Get() );
	V_StripLastDir( pUpDirectory );
	V_StripTrailingSlash( pUpDirectory );

	// This occurs at the root directory
	if ( !Q_stricmp( pUpDirectory, "." ) )
		return;
	SetCurrentDirectory( pUpDirectory ); 
}



