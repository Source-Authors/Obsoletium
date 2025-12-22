//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//


#include "BaseSaveGameDialog.h"
#include "filesystem.h"
#include "savegame_version.h"
#include "vgui_controls/PanelListPanel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Button.h"
#include "tier1/strtools.h"
#include "tier1/utlbuffer.h"
#include "qlimits.h"

#include "MouseMessageForwardingPanel.h"
#include "TGAImagePanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

#define TGA_IMAGE_PANEL_WIDTH 180
#define TGA_IMAGE_PANEL_HEIGHT 100

#define MAX_LISTED_SAVE_GAMES	128

//-----------------------------------------------------------------------------
// Purpose: Describes the layout of a same game pic
//-----------------------------------------------------------------------------
class CSaveGamePanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CSaveGamePanel, vgui::EditablePanel );
public:
	CSaveGamePanel( PanelListPanel *parent, const char *name, intp saveGameListItemID ) : BaseClass( parent, name )
	{
		m_iSaveGameListItemID = saveGameListItemID;
		m_pParent = parent;
		// dimhotepus: Scale UI.
		m_pSaveGameImage = new CTGAImagePanel( this, "SaveGameImage", TGA_IMAGE_PANEL_WIDTH, TGA_IMAGE_PANEL_HEIGHT );
		m_pAutoSaveImage = new ImagePanel( this, "AutoSaveImage" );
		m_pSaveGameScreenshotBackground = new ImagePanel( this, "SaveGameScreenshotBackground" );
		m_pChapterLabel = new Label( this, "ChapterLabel", "" );
		m_pTypeLabel = new Label( this, "TypeLabel", "" );
		m_pElapsedTimeLabel = new Label( this, "ElapsedTimeLabel", "" );
		m_pFileTimeLabel = new Label( this, "FileTimeLabel", "" );

		auto *panel = new CMouseMessageForwardingPanel(this, NULL);
		panel->SetZPos(2);

		// dimhotepus: Scale UI.
		SetSize( QuickPropScale( 200 ), QuickPropScale( 140 ) );

		LoadControlSettings( "resource/SaveGamePanel.res" );

		m_FillColor = m_pSaveGameScreenshotBackground->GetFillColor();
	}

	void SetSaveGameInfo( SaveGameDescription_t &save )
	{
		// set the bitmap to display
		char tga[MAX_PATH];
		V_strcpy_safe( tga, save.szFileName );
		char *ext = strstr( tga, ".sav" );
		if ( ext )
		{
			V_strncpy( ext, ".tga", 5 );
		}

		// If a TGA file exists then it is a user created savegame
		if ( g_pFullFileSystem->FileExists( tga ) )
		{
			m_pSaveGameImage->SetTGA( tga );
		}
		// If there is no TGA then it is either an autosave or the user TGA file has been deleted
		else
		{
			m_pSaveGameImage->SetVisible( false );
			m_pAutoSaveImage->SetVisible( true );
			m_pAutoSaveImage->SetImage( "resource\\autosave" );
		}

		// set the title text
		m_pChapterLabel->SetText( save.szComment );

		// type
		SetControlString( "TypeLabel", save.szType );
		SetControlString( "ElapsedTimeLabel", save.szElapsedTime );
		SetControlString( "FileTimeLabel", save.szFileTime );
	}

	MESSAGE_FUNC_INT( OnPanelSelected, "PanelSelected", state )
	{
		if ( state )
		{
			// set the text color to be orange, and the pic border to be orange
			m_pSaveGameScreenshotBackground->SetFillColor( m_SelectedColor );
			m_pChapterLabel->SetFgColor( m_SelectedColor );
			m_pTypeLabel->SetFgColor( m_SelectedColor );
			m_pElapsedTimeLabel->SetFgColor( m_SelectedColor );
			m_pFileTimeLabel->SetFgColor( m_SelectedColor );
		}
		else
		{
			m_pSaveGameScreenshotBackground->SetFillColor( m_FillColor );
			m_pChapterLabel->SetFgColor( m_TextColor );
			m_pTypeLabel->SetFgColor( m_TextColor );
			m_pElapsedTimeLabel->SetFgColor( m_TextColor );
			m_pFileTimeLabel->SetFgColor( m_TextColor );
		}

		PostMessage( m_pParent->GetVParent(), new KeyValues("PanelSelected") );
	}

	void OnMousePressed( vgui::MouseCode code ) override
	{
		m_pParent->SetSelectedPanel( this );
	}

	void ApplySchemeSettings( IScheme *pScheme ) override
	{
		m_TextColor = pScheme->GetColor( "NewGame.TextColor", Color(255, 255, 255, 255) );
		m_SelectedColor = pScheme->GetColor( "NewGame.SelectionColor", Color(255, 255, 255, 255) );

		BaseClass::ApplySchemeSettings( pScheme );
	}

	void OnMouseDoublePressed( vgui::MouseCode code ) override
	{
		// call the panel
		OnMousePressed( code );
		PostMessage( m_pParent->GetParent(), new KeyValues("Command", "command", "loadsave") );
	}

	intp GetSaveGameListItemID() const
	{
		return m_iSaveGameListItemID;
	}

private:
	vgui::PanelListPanel *m_pParent;
	vgui::Label *m_pChapterLabel;
	CTGAImagePanel *m_pSaveGameImage;
	ImagePanel *m_pAutoSaveImage;
	
	// things to change color when the selection changes
	ImagePanel *m_pSaveGameScreenshotBackground;
	Label *m_pTypeLabel;
	Label *m_pElapsedTimeLabel;
	Label *m_pFileTimeLabel;
	Color m_TextColor, m_FillColor, m_SelectedColor;

	intp m_iSaveGameListItemID;
};


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBaseSaveGameDialog::CBaseSaveGameDialog( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	CreateSavedGamesList();
	ScanSavedGames();

	m_pLoadButton = new vgui::Button( this, "loadsave", "" );
	SetControlEnabled( "loadsave", false );
}

//-----------------------------------------------------------------------------
// Purpose: Creates the load game display list
//-----------------------------------------------------------------------------
void CBaseSaveGameDialog::CreateSavedGamesList()
{
	m_pGameList = new vgui::PanelListPanel( this, "listpanel_loadgame" );
	m_pGameList->SetFirstColumnWidth( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: returns the save file name of the selected item
//-----------------------------------------------------------------------------
intp CBaseSaveGameDialog::GetSelectedItemSaveIndex()
{
	CSaveGamePanel *panel = dynamic_cast<CSaveGamePanel *>(m_pGameList->GetSelectedPanel());
	if ( panel )
	{
		// find the panel in the list
		for ( intp i = 0; i < m_SaveGames.Count(); i++ )
		{
			if ( i == panel->GetSaveGameListItemID() )
			{
				return i;
			}
		}
	}
	return m_SaveGames.InvalidIndex();
}

//-----------------------------------------------------------------------------
// Purpose: builds save game list from directory
//-----------------------------------------------------------------------------
void CBaseSaveGameDialog::ScanSavedGames()
{
	// populate list box with all saved games on record:
	char	szDirectory[MAX_PATH];
	V_sprintf_safe( szDirectory, "%s/*.sav", SAVE_DIR );

	// clear the current list
	m_pGameList->DeleteAllItems();
	m_SaveGames.RemoveAll();
	
	// iterate the saved files
	FileFindHandle_t handle = FILESYSTEM_INVALID_FIND_HANDLE;
	const char *pFileName = g_pFullFileSystem->FindFirstEx( szDirectory, MOD_DIR, &handle );
	while (pFileName)
	{
		if ( !Q_strnicmp(pFileName, "HLSave", std::size( "HLSave" ) - 1 ) )
		{
			pFileName = g_pFullFileSystem->FindNext( handle );
			continue;
		}

		char szFileName[MAX_PATH];
		V_sprintf_safe(szFileName, "%s/%s", SAVE_DIR, pFileName);

		// Only load save games from the current mod's save dir
		if( !g_pFullFileSystem->FileExists( szFileName, MOD_DIR ) )
		{
			pFileName = g_pFullFileSystem->FindNext( handle );
			continue;
		}
		
		SaveGameDescription_t save;
		if ( ParseSaveData( szFileName, pFileName, save ) )
		{
			m_SaveGames.AddToTail( save );
		}
		
		pFileName = g_pFullFileSystem->FindNext( handle );
	}
	
	g_pFullFileSystem->FindClose( handle );

	// notify derived classes that save games are being scanned (so they can insert their own)
	OnScanningSaveGames();

	// sort the save list
	std::sort( m_SaveGames.begin(), m_SaveGames.end(), SaveGameSortFunc );

	// add to the list
	for ( intp saveIndex = 0; saveIndex < m_SaveGames.Count() && saveIndex < MAX_LISTED_SAVE_GAMES; saveIndex++ )
	{
		// add the item to the panel
		AddSaveGameItemToList( saveIndex );
	}

	// display a message if there are no save games
	if ( !m_SaveGames.Count() )
	{
		vgui::Label *pNoSavesLabel = SETUP_PANEL(new Label(m_pGameList, "NoSavesLabel", "#GameUI_NoSaveGamesToDisplay"));
		pNoSavesLabel->SetTextColorState(vgui::Label::CS_DULL);
		// dimhotepus: Ensure label looks good regarding of UI scaling.
		pNoSavesLabel->SizeToContents();
		m_pGameList->AddItem( NULL, pNoSavesLabel );
	}

	SetControlEnabled( "loadsave", false );
	SetControlEnabled( "delete", false );
}

//-----------------------------------------------------------------------------
// Purpose: Adds an item to the list
//-----------------------------------------------------------------------------
void CBaseSaveGameDialog::AddSaveGameItemToList( intp saveIndex )
{
	// create the new panel and add to the list
	CSaveGamePanel *saveGamePanel = new CSaveGamePanel( m_pGameList, "SaveGamePanel", saveIndex );
	saveGamePanel->SetSaveGameInfo( m_SaveGames[saveIndex] );
	m_pGameList->AddItem( NULL, saveGamePanel );
}

//-----------------------------------------------------------------------------
// Purpose: Parses the save game info out of the .sav file header
//-----------------------------------------------------------------------------
bool CBaseSaveGameDialog::ParseSaveData( char const *pszFileName, char const *pszShortName, SaveGameDescription_t &save )
{
	char    szMapName[SAVEGAME_MAPNAME_LEN];
	char    szComment[SAVEGAME_COMMENT_LEN];
	char    szElapsedTime[SAVEGAME_ELAPSED_LEN];

	if ( !pszFileName || !pszShortName )
		return false;

	V_strcpy_safe( save.szShortName, pszShortName );
	V_strcpy_safe( save.szFileName, pszFileName );

	// dimhotepus: This can take a while, put up a waiting cursor.
	const vgui::ScopedPanelWaitCursor scopedWaitCursor{this};

	FileHandle_t fh = g_pFullFileSystem->Open( pszFileName, "rb", "MOD" );
	if (!fh)
		return false;

	RunCodeAtScopeExit(g_pFullFileSystem->Close(fh));

	int readok = SaveReadNameAndComment( fh, szMapName, szComment );
	if ( !readok )
	{
		return false;
	}

	V_strcpy_safe( save.szMapName, szMapName );

	// Elapsed time is the last 6 characters in comment. (mmm:ss)
	intp i = V_strlen( szComment );
	V_strcpy_safe( szElapsedTime, "??" );
	if (i >= 6)
	{
		Q_strncpy( szElapsedTime, (char *)&szComment[i - 6], 7 );
		szElapsedTime[6] = '\0';

		// parse out
		int minutes = atoi( szElapsedTime );
		int seconds = atoi( szElapsedTime + 4);

		// reformat
		if ( minutes )
		{
			V_sprintf_safe( szElapsedTime, "%d %s %d seconds", minutes, minutes > 1 ? "minutes" : "minute", seconds );
		}
		else
		{
			V_sprintf_safe( szElapsedTime, "%d seconds", seconds );
		}

		// Chop elapsed out of comment.
		intp n = i - 6;
		szComment[n] = '\0';
	
		n--;

		// Strip back the spaces at the end.
		while ((n >= 1) &&
			szComment[n] &&
			szComment[n] == ' ')
		{
			szComment[n--] = '\0';
		}
	}

	// calculate the file name to print
	const char *pszType = "";
	if (strstr(pszFileName, "quick"))
	{
		pszType = "#GameUI_QuickSave";
	}
	else if (strstr(pszFileName, "autosave"))
	{
		pszType = "#GameUI_AutoSave";
	}

	V_strcpy_safe( save.szType, pszType );
	V_strcpy_safe( save.szComment, szComment );
	V_strcpy_safe( save.szElapsedTime, szElapsedTime );

	// Now get file time stamp.
	time_t fileTime = g_pFullFileSystem->GetFileTime(pszFileName);
	char szFileTime[32];
	g_pFullFileSystem->FileTimeToString(szFileTime, fileTime);
	char *newline = strchr(szFileTime, '\n');
	if (newline)
	{
		*newline = '\0';
	}
	V_strcpy_safe( save.szFileTime, szFileTime );
	save.iTimestamp = fileTime;
	return true;
}

void CBaseSaveGameDialog::OnKeyCodeTyped( vgui::KeyCode code )
{
	if ( code == KEY_ESCAPE )
	{
		OnCommand( "Close" );
		return;
	}

	BaseClass::OnKeyCodeTyped( code );
}

void CBaseSaveGameDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( code == KEY_XBUTTON_B )
	{
		OnCommand( "Close" );
		return;
	}
	else if ( code == KEY_XSTICK1_DOWN || 
			  code == KEY_XSTICK2_DOWN || 
			  code == KEY_XBUTTON_DOWN || 
			  code == KEY_DOWN )
	{
		if ( m_pGameList->GetItemCount() )
		{
			Panel *pSelectedPanel = m_pGameList->GetSelectedPanel();
			if ( !pSelectedPanel )
			{
				m_pGameList->SetSelectedPanel( m_pGameList->GetItemPanel( m_pGameList->FirstItem() ) );
				m_pGameList->ScrollToItem( m_pGameList->FirstItem() );
				return;
			}
			else
			{
				int nNextPanelID = m_pGameList->FirstItem();
				while ( nNextPanelID != m_pGameList->InvalidItemID() )
				{
					if ( m_pGameList->GetItemPanel( nNextPanelID ) == pSelectedPanel )
					{
						nNextPanelID = m_pGameList->NextItem( nNextPanelID );
						if ( nNextPanelID != m_pGameList->InvalidItemID() )
						{
							m_pGameList->SetSelectedPanel( m_pGameList->GetItemPanel( nNextPanelID ) );
							m_pGameList->ScrollToItem( nNextPanelID );
							return;
						}

						break;
					}

					nNextPanelID = m_pGameList->NextItem( nNextPanelID );
				}
			}
		}
	}
	else if ( code == KEY_XSTICK1_UP || 
			  code == KEY_XSTICK2_UP || 
			  code == KEY_XBUTTON_UP || 
			  code == KEY_UP )
	{
		if ( m_pGameList->GetItemCount() )
		{
			Panel *pSelectedPanel = m_pGameList->GetSelectedPanel();
			if ( !pSelectedPanel )
			{
				m_pGameList->SetSelectedPanel( m_pGameList->GetItemPanel( m_pGameList->FirstItem() ) );
				m_pGameList->ScrollToItem( m_pGameList->FirstItem() );
				return;
			}
			else
			{
				int nNextPanelID = m_pGameList->FirstItem();
				if ( m_pGameList->GetItemPanel( nNextPanelID ) != pSelectedPanel )
				{
					while ( nNextPanelID != m_pGameList->InvalidItemID() )
					{
						int nOldPanelID = nNextPanelID;
						nNextPanelID = m_pGameList->NextItem( nNextPanelID );

						if ( nNextPanelID != m_pGameList->InvalidItemID() )
						{
							if ( m_pGameList->GetItemPanel( nNextPanelID ) == pSelectedPanel )
							{
								m_pGameList->SetSelectedPanel( m_pGameList->GetItemPanel( nOldPanelID ) );
								m_pGameList->ScrollToItem( nOldPanelID  );
								return;
							}
						}
					}
				}
			}
		}
	}
	else if ( code == KEY_ENTER || code == KEY_XBUTTON_A || code == STEAMCONTROLLER_A )
	{
		Panel *pSelectedPanel = m_pGameList->GetSelectedPanel();
		if ( pSelectedPanel )
		{
			if ( code == KEY_XBUTTON_A || code == STEAMCONTROLLER_A )
			{
				ConVarRef var( "joystick" );
				if ( var.IsValid() && !var.GetBool() )
				{
					var.SetValue( true );
				}

				ConVarRef var2( "hud_fastswitch" );
				if ( var2.IsValid() && var2.GetInt() != 2 )
				{
					var2.SetValue( 2 );
				}
			}

			m_pLoadButton->DoClick();
			return;
		}
	}
	// dimhotepus: Support deletion of saves by delete button.
	else if ( code == KEY_DELETE )
	{
		OnCommand( "Delete" );
	}

	BaseClass::OnKeyCodePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose: timestamp sort function for savegames
//-----------------------------------------------------------------------------
bool CBaseSaveGameDialog::SaveGameSortFunc( const SaveGameDescription_t &s1, const SaveGameDescription_t &s2 )
{
	if (s1.iTimestamp < s2.iTimestamp)
		return false;

	if (s1.iTimestamp > s2.iTimestamp)
		return true;

	// timestamps are equal, so just sort by filename
	return strcmp(s1.szFileName, s2.szFileName) < 0;
}

int SaveReadNameAndComment( FileHandle_t f,	OUT_Z_CAP(nameSize) char *name,	int nameSize, OUT_Z_CAP(commentSize) char *comment, int commentSize )
{
	int tag, size, tokenSize, tokenCount;
	char *pFieldName;

	name[0] = '\0';
	comment[0] = '\0';

	g_pFullFileSystem->Read( &tag, sizeof(int), f );
	if ( tag != MAKEID('J','S','A','V') )
	{
		return 0;
	}
		
	g_pFullFileSystem->Read( &tag, sizeof(int), f );
	if ( tag != SAVEGAME_VERSION )				// Enforce version for now
	{
		return 0;
	}

	g_pFullFileSystem->Read( &size, sizeof(int), f );
	
	g_pFullFileSystem->Read( &tokenCount, sizeof(int), f );	// These two ints are the token list
	g_pFullFileSystem->Read( &tokenSize, sizeof(int), f );
	size += tokenSize;

	// Sanity Check.
	if ( tokenCount < 0 || tokenCount > 1024*1024*32  )
	{
		return 0;
	}

	if ( tokenSize < 0 || tokenSize > 1024*1024*32  )
	{
		return 0;
	}

	std::unique_ptr<char[]> pSaveData = std::make_unique<char[]>(size);
	g_pFullFileSystem->Read(pSaveData.get(), size, f);

	int nNumberOfFields;
	int nFieldSize;
	
	char *pData = pSaveData.get();
	std::unique_ptr<char*[]> pTokenList;

	// Allocate a table for the strings, and parse the table
	if ( tokenSize > 0 )
	{
		pTokenList = std::make_unique<char*[]>(tokenCount);

		// Make sure the token strings pointed to by the pToken hashtable.
		for( int i=0; i<tokenCount; i++ )
		{
			pTokenList[i] = *pData ? pData : NULL;	// Point to each string in the pToken table
			while( *pData++ );				// Find next token (after next null)
		}
	}
	else
		pTokenList = NULL;

	// short, short (size, index of field name)
	nFieldSize = *(short *)pData;
	pData += sizeof(short);
	pFieldName = pTokenList[ *(short *)pData ];

	if (stricmp(pFieldName, "GameHeader"))
	{
		return 0;
	};

	// int (fieldcount)
	pData += sizeof(short);
	nNumberOfFields = *(int*)pData;
	pData += nFieldSize;

	// Each field is a short (size), short (index of name), binary string of "size" bytes (data)
	for (int i = 0; i < nNumberOfFields; i++)
	{
		// Data order is:
		// Size
		// szName
		// Actual Data

		nFieldSize = *(short *)pData;
		pData += sizeof(short);

		pFieldName = pTokenList[ *(short *)pData ];
		pData += sizeof(short);

		if (!stricmp(pFieldName, "comment"))
		{
			int copySize = MAX(commentSize, nFieldSize);
			Q_strncpy(comment, pData, copySize);
		}
		else if (!stricmp(pFieldName, "mapName"))
		{
			int copySize = MAX(nameSize, nFieldSize);
			Q_strncpy(name, pData, copySize);
		};

		// Move to Start of next field.
		pData += nFieldSize;
	};

	return name[0] && comment[0] ? 1 : 0;
}

//-----------------------------------------------------------------------------
// Purpose: deletes an existing save game
//-----------------------------------------------------------------------------
void CBaseSaveGameDialog::DeleteSaveGame( const char *fileName )
{
	if ( Q_isempty( fileName ) )
		return;

	// delete the save game file
	g_pFullFileSystem->RemoveFile( fileName, "MOD" );

	// delete the associated tga
	char tga[MAX_PATH];
	V_strcpy_safe( tga, fileName );
	char *ext = strstr( tga, ".sav" );
	if ( ext )
	{
		V_strncpy( ext, ".tga", 5 );
	}
	g_pFullFileSystem->RemoveFile( tga, "MOD" );
}

//-----------------------------------------------------------------------------
// Purpose: One item has been selected
//-----------------------------------------------------------------------------
void CBaseSaveGameDialog::OnPanelSelected()
{
	SetControlEnabled( "loadsave", true );
	SetControlEnabled( "delete", true );
}
