// Copyright Valve Corporation, All rights reserved.

#include "tier0/dbg.h"
#include "elementviewer.h"
#include "vgui_controls/Panel.h"
#include "vgui_controls/MenuBar.h"
#include "vgui_controls/Menu.h"
#include "vgui/ISurface.h"
#include "vgui/IInput.h"
#include "vgui/IVGui.h"
#include "tier0/icommandline.h"
#include "tier1/KeyValues.h"
#include "datamodel/dmelement.h"
#include "datamodel/idatamodel.h"
#include "vgui_controls/FileOpenDialog.h"
#include "filesystem.h"
#include "movieobjects/movieobjects.h"

#include "dme_controls/inotifyui.h"
#include "dme_controls/ElementPropertiesTree.h"
#include "dme_controls/filelistmanager.h"
#include "dme_controls/dmecontrols.h"

using namespace vgui;

typedef vgui::DHANDLE< CElementPropertiesTree > viewList_t;
typedef CUtlRBTree< CDmElement *, int > ElementDict_t;
 
struct ViewerDoc_t
{
	ViewerDoc_t() : m_fileid( DMFILEID_INVALID ), m_bDirty( false ) {}

	DmFileId_t	m_fileid;
	bool		m_bDirty;
};


//-----------------------------------------------------------------------------
// main editor panel
//-----------------------------------------------------------------------------
class CElementViewerPanel : public vgui::Panel, public IDmNotify, public CBaseElementPropertiesChoices
{
	DECLARE_CLASS_SIMPLE( CElementViewerPanel, vgui::Panel );

public:

	CElementViewerPanel();
	~CElementViewerPanel();

	void NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags ) override;

	// Resize this panel to match its parent
	void PerformLayout() override;

	void OnCommand( const char *cmd ) override;

	void OnThink() override;

	void	OnOpen();
	void	OnSaveAs();
	void	OnSave();
	void	OnNew();

	intp NumDocs() const
	{
		return m_Docs.Count();
	}

	CDmElement *GetRoot( intp docNum )
	{
		return GetElement< CDmElement >( g_pDataModel->GetFileRoot( m_Docs[ docNum ].m_fileid ) );
	}

protected:
	MESSAGE_FUNC_PARAMS( OnFileSelected, "FileSelected", kv );

private:
	void CreateNewView( CDmElement *pRoot, const char *title );

	vgui::MenuBar *m_pMenuBar;

	// FIXME: Is there a better way?
	// A panel that represents all area under the menu bar
	vgui::Panel *m_pClientArea;
	CFileManagerFrame *m_pFileManager;
	CUtlVector< viewList_t >	m_Views;
	CUtlVector< ViewerDoc_t >	m_Docs;
};


vgui::Panel *CreateElementViewerPanel()
{
	// add our main window
	vgui::Panel *pElementViewer = new CElementViewerPanel;
	//g_pVGuiSurface->CreatePopup(pElementViewer->GetVPanel(), false );
	pElementViewer->SetParent( g_pVGuiSurface->GetEmbeddedPanel() );
	return pElementViewer;
}

//CElementView *CreateView( vgui::Panel *parent, CDmElement *pRoot, const char *title );

CElementViewerPanel::CElementViewerPanel() : vgui::Panel( NULL, "ElementViewer" )
{
	SetElementPropertiesChoices( this );
	m_pMenuBar = new vgui::MenuBar( this, "Main Menu Bar" );
	m_pMenuBar->SetSize( 10, 28 );

	// Next create a menu
	Menu *pMenu = new Menu(NULL, "File Menu");
	pMenu->AddMenuItem("&New", new KeyValues ( "Command", "command", "OnNew"), this);
	pMenu->AddMenuItem("&Open", new KeyValues ("Command", "command", "OnOpen"), this);
	pMenu->AddMenuItem("&Save", new KeyValues ("Command", "command", "OnSave"), this);
	pMenu->AddMenuItem("Save &As", new KeyValues ("Command", "command", "OnSaveAs"), this);
	pMenu->AddMenuItem("E&xit", new KeyValues ("Command", "command", "OnExit"), this);

	m_pMenuBar->AddMenu( "&File", pMenu );

	m_pClientArea = new vgui::Panel( this, "ElementViewer Client Area" );

	m_pFileManager = new CFileManagerFrame( m_pClientArea );

	SetKeyBoardInputEnabled( true );

	// load a file from the commandline
	const char *fileName = NULL;
	CommandLine()->CheckParm("-loadDmx", &fileName );

	if ( fileName )
	{
		// trim off any quotes (paths with spaces need to be quoted on the commandline)
		char buf[ MAX_PATH ];
		V_StrSubst( fileName, "\"", "", buf );

		OnFileSelected( KeyValuesAD(new KeyValues( "OnFileSelected", "fullpath", buf )) );
	}

	g_pDataModel->InstallNotificationCallback( this );
}

void RemoveFileId( DmFileId_t fileid )
{
	Assert( fileid != DMFILEID_INVALID );
	if ( fileid != DMFILEID_INVALID )
	{
		g_pDataModel->RemoveFileId( fileid );
	}
}

void ReportElementStats()
{
	intp nCurrentElements = g_pDataModel->GetAllocatedElementCount();
	intp nTotalElements = g_pDataModel->GetElementsAllocatedSoFar();
	intp nMaxElements = g_pDataModel->GetMaxNumberOfElements();
//	int nCurrentAttributes = g_pDataModel->GetAllocatedAttributeCount();
	Msg( "element count: current = %zd max = %zd total = %zd\n", nCurrentElements, nMaxElements, nTotalElements );

	intp nElementsInFiles = 0;
	int nFiles = g_pDataModel->NumFileIds();
	for ( int fi = 0; fi < nFiles; ++fi )
	{
		DmFileId_t fileid = g_pDataModel->GetFileId( fi );
		intp nElements = g_pDataModel->NumElementsInFile( fileid );
		nElementsInFiles += nElements;
		const char *pFileName = g_pDataModel->GetFileName( fileid );
		Msg( "elements in file \"%s\" = %zd\n", pFileName, nElements );
	}
	Msg( "elements not in any file = %zd\n", nCurrentElements - nElementsInFiles );
}

CElementViewerPanel::~CElementViewerPanel()
{
	intp nDocs = m_Docs.Count();
	for ( intp i = 0; i < nDocs; ++i )
	{
		RemoveFileId( m_Docs[ i ].m_fileid );
	}
	m_Docs.RemoveAll();

	ReportElementStats();
	g_pDataModel->RemoveNotificationCallback( this );
	SetElementPropertiesChoices( NULL );
}

void CElementViewerPanel::OnThink()
{
	if ( vgui::input()->IsKeyDown( KEY_ESCAPE ) )
	{
		vgui::ivgui()->Stop();
	}
	else
	{
		BaseClass::OnThink();
	}
}

void CElementViewerPanel::CreateNewView( CDmElement *pRoot, const char *title )
{
	vgui::DHANDLE< CElementPropertiesTree > f;
	// f = CreateView( m_pClientArea, pRoot, title );
	f = new CElementPropertiesTree( m_pClientArea, this, pRoot );
	f->Init();
	f->SetPos( 10, 30 );
	f->SetSize( 600, 500 );
	f->SetVisible( true );
	m_Views.AddToTail( f );

	m_pFileManager->Refresh();
}

void CElementViewerPanel::NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
//	if ( flags & INotifyUI::NOTIFY_REFRESH_PROPERTIES_VALUES ) // FIXME - do we need new flags for file association changes?
	{
		m_pFileManager->Refresh();

		intp nViews = m_Views.Count();
		for ( intp i = 0; i < nViews; ++i )
		{
			if ( m_Views[ i ] )
			{
				m_Views[ i ]->Refresh();
			}
		}
	}
}

void CElementViewerPanel::OnCommand( const char *cmd )
{
	if ( !V_stricmp( cmd, "OnOpen" ) )
	{
		OnOpen();
	}
	else if ( !V_stricmp( cmd, "OnSave" ) )
	{
		OnSave();
	}
	else if ( !V_stricmp( cmd, "OnSaveAs" ) )
	{
		OnSaveAs();
	}
	else if ( !V_stricmp( cmd, "OnNew" ) )
	{
		OnNew();
	}
	else if ( !V_stricmp( cmd, "OnExit" ) )
	{
		// Throw up a "save" dialog?
		vgui::ivgui()->Stop();
	}
	else
	{
		BaseClass::OnCommand( cmd );
	}
}

void CElementViewerPanel::OnSaveAs()
{
	if ( m_Docs.Count() < 1 )
		return;

	DmFileId_t fileid = m_Docs[ 0 ].m_fileid;
	// Save As file
	auto *pFileOpenDialog = new FileOpenDialog( this, "Save .dmx File As", false, new KeyValues( "OnSaveAs" ) );

	const char *pFileFormat = g_pDataModel->GetFileFormat( fileid );
	const char *pDescription = ( pFileFormat && *pFileFormat ) ? g_pDataModel->GetFormatDescription( pFileFormat ) : NULL;

	if ( pDescription && *pDescription )
	{
		char description[ 256 ];
		V_sprintf_safe( description, "%s (*.dmx)", g_pDataModel->GetFormatDescription( pFileFormat ) );
		pFileOpenDialog->AddFilter( "*.dmx", description, true, pFileFormat );
	}
	else
	{
		pFileOpenDialog->AddFilter( "*.dmx", "DMX File (*.dmx)", true, "dmx" );
	}

	pFileOpenDialog->AddActionSignalTarget( this );
	pFileOpenDialog->DoModal( false );
}

void CElementViewerPanel::OnSave()
{
	intp docCount = m_Docs.Count();
	if ( docCount > 0 )
	{
		DmFileId_t fileid = m_Docs[ docCount - 1 ].m_fileid;
		const char *pFormat = g_pDataModel->GetFileFormat( fileid );
		const char *pEncoding = g_pDataModel->GetDefaultEncoding( pFormat );
		const char *pFileName = g_pDataModel->GetFileName( fileid );
		CDmElement *pRoot = GetElement< CDmElement >( g_pDataModel->GetFileRoot( fileid ) );
		g_pDataModel->SaveToFile( pFileName, NULL, pEncoding, pFormat, pRoot );
		// TODO - figure out what file type this was
	}
}

struct DataModelFilenameArray
{
	int Count() const
	{
		return g_pDataModel->NumFileIds();
	}
	const char *operator[]( int i ) const
	{
		return g_pDataModel->GetFileName( g_pDataModel->GetFileId( i ) );
	}
};

void CElementViewerPanel::OnNew()
{
	char filename[ MAX_PATH ];
	// dimhotepus: Notify unique name gen failed.
	if ( !V_GenerateUniqueName( filename, "unnamed", DataModelFilenameArray() ) )
		Warning( "Unable to generate unique file name for new tab.\n" );

	ViewerDoc_t doc;
	doc.m_fileid = g_pDataModel->FindOrCreateFileId( filename );
	CDmElement *pRoot = CreateElement< CDmElement >( "root", doc.m_fileid );
	g_pDataModel->SetFileRoot( doc.m_fileid, pRoot->GetHandle() );

	m_Docs.AddToTail( doc );

	CreateNewView( pRoot, filename );
}

void CElementViewerPanel::OnOpen()
{
	// Open file
	auto *pFileOpenDialog = new FileOpenDialog( this, "Choose .dmx file", true );
	pFileOpenDialog->AddFilter( "*.*", "All Files (*.*)", false );
	pFileOpenDialog->AddFilter( "*.dmx", "DmElement Files (*.dmx)", true );
	for ( intp i = 0; i < g_pDataModel->GetFormatCount(); ++i )
	{
		const char *pFormatName = g_pDataModel->GetFormatName(i);
		const char *pDesc = g_pDataModel->GetFormatDescription(pFormatName);
		const char *pExt = g_pDataModel->GetFormatExtension(pFormatName);

		char pExtBuf[512];
		char pDescBuf[512];
		V_sprintf_safe( pExtBuf, "*.%s", pExt );
		V_sprintf_safe( pDescBuf, "%s (*.%s)", pDesc, pExt );

		pFileOpenDialog->AddFilter( pExtBuf, pDescBuf, false );
	}
	pFileOpenDialog->AddActionSignalTarget( this );
	pFileOpenDialog->DoModal( false );
}

void CElementViewerPanel::OnFileSelected( KeyValues *pKeyValues )
{
	const char *pFullPath = pKeyValues->GetString( "fullpath" );
	if ( !pFullPath || !pFullPath[ 0 ] )
		return;

	if ( pKeyValues->FindKey( "OnSaveAs" ) )
	{
		const char *pFormat = pKeyValues->GetString( "filterinfo" );
		Assert( pFormat );
		if ( !pFormat )
			return;

		// TODO - figure out which panel is on top, and save the file associated with it

		intp docCount = m_Docs.Count();
		if ( docCount == 1 )
		{
			g_pDataModel->SetFileName( m_Docs[ 0 ].m_fileid, pFullPath );
			g_pDataModel->SaveToFile( pFullPath, NULL, g_pDataModel->GetDefaultEncoding( pFormat ), pFormat, GetElement< CDmElement >( g_pDataModel->GetFileRoot( m_Docs[ 0 ].m_fileid ) ) );
		}
		return;
	}

//	char relativepath[ 512 ];
//	g_pFileSystem->FullPathToRelativePath_safe( fullpath, relativepath );

	g_pDataModel->OnlyCreateUntypedElements( true );
	g_pDataModel->SetDefaultElementFactory( NULL );

	// Open the path as a KV and parse stuff from it...
	CDmElement *pRoot = NULL;
	DmFileId_t fileid = g_pDataModel->RestoreFromFile( pFullPath, NULL, NULL, &pRoot, CR_DELETE_NEW );
	if ( pRoot )
	{
		ViewerDoc_t doc;
		doc.m_fileid = fileid;
		m_Docs.AddToTail( doc );

		CreateNewView( pRoot, pFullPath );
	}
}

//-----------------------------------------------------------------------------
// The editor panel should always fill the space...
//-----------------------------------------------------------------------------
void CElementViewerPanel::PerformLayout()
{
	// Make the editor panel fill the space
	int iWidth, iHeight;

	vgui::VPANEL parent = GetParent() ? GetParent()->GetVPanel() : vgui::surface()->GetEmbeddedPanel(); 
	vgui::ipanel()->GetSize( parent, iWidth, iHeight );
	SetSize( iWidth, iHeight );
	m_pMenuBar->SetSize( iWidth, 28 );

	// Make the client area also fill the space not used by the menu bar
	int iTemp, iMenuHeight;
	m_pMenuBar->GetSize( iTemp, iMenuHeight );
	m_pClientArea->SetPos( 0, iMenuHeight );
	m_pClientArea->SetSize( iWidth, iHeight - iMenuHeight );
}
