//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "LoadCommentaryDialog.h"
#include "EngineInterface.h"
#include "IGameUIFuncs.h"

#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "tier1/KeyValues.h"
#include "tier1/convar.h"
#include "filesystem.h"

#include "vgui_controls/PanelListPanel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/QueryBox.h"

#include <stdio.h>
#include <stdlib.h>

#include "BasePanel.h"

#include "GameUI_Interface.h"

#include "MouseMessageForwardingPanel.h"
#include "TGAImagePanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;


void OpenLoadCommentaryDialog( void )
{
	DHANDLE<CLoadCommentaryDialog> hCommentaryDialog;
	if ( !hCommentaryDialog.Get() )
	{
		hCommentaryDialog = new CLoadCommentaryDialog( BasePanel() );
	}

	GameUI().ActivateGameUI();
	hCommentaryDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CC_LoadCommentary_Test( void )
{
	OpenLoadCommentaryDialog();
}
static ConCommand commentary_testfirstrun("loadcommentary", CC_LoadCommentary_Test, 0 );

//-----------------------------------------------------------------------------
// Purpose: Describes the layout of a commentary map list item
//-----------------------------------------------------------------------------
class CCommentaryItemPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CCommentaryItemPanel, vgui::EditablePanel );
public:
	CCommentaryItemPanel( PanelListPanel *parent, const char *name, int iListItemID ) : BaseClass( parent, name )
	{
		m_iListItemID = iListItemID;
		m_pParent = parent;
		m_pCommentaryScreenshot = new CTGAImagePanel( this, "CommentaryMapScreenshot" );
		m_pCommentaryScreenshotBackground = new ImagePanel( this, "CommentaryScreenshotBackground" );

		// map name
		m_pMapNameLabel = new Label( this, "MapName", "" );

		// description
		m_pDescriptionLabel = new Label( this, "Description", "" );

		CMouseMessageForwardingPanel *panel = new CMouseMessageForwardingPanel(this, NULL);
		panel->SetZPos(2);

		SetSize( 200, 140 );

		LoadControlSettings( "resource/CommentaryItem.res" );

		m_FillColor = m_pCommentaryScreenshotBackground->GetFillColor();
	}

	void SetCommentaryInfo( CommentaryItem_t &item )
	{
		// set the bitmap to display
		char tga[MAX_PATH];
		V_strcpy_safe( tga, item.szMapFileName );
		char *ext = strstr( tga, ".sav" );
		if ( ext )
		{
			V_strncpy( ext, ".tga", 5 );
		}
		m_pCommentaryScreenshot->SetTGA( tga );

		// set the title text
		m_pMapNameLabel->SetText( item.szPrintName );
		m_pDescriptionLabel->SetText( item.szDescription );
	}

	MESSAGE_FUNC_INT( OnPanelSelected, "PanelSelected", state )
	{
		if ( state )
		{
			// set the text color to be orange, and the pic border to be orange
			m_pCommentaryScreenshotBackground->SetFillColor( m_SelectedColor );
			m_pMapNameLabel->SetFgColor( m_SelectedColor );
			m_pDescriptionLabel->SetFgColor( m_SelectedColor );
		}
		else
		{
			m_pCommentaryScreenshotBackground->SetFillColor( m_FillColor );
			m_pMapNameLabel->SetFgColor( m_TextColor );
			m_pDescriptionLabel->SetFgColor( m_TextColor );
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
		PostMessage( m_pParent->GetParent(), new KeyValues("Command", "command", "loadcommentary") );
	}

	int GetListItemID()
	{
		return m_iListItemID;
	}

private:
	vgui::PanelListPanel *m_pParent;

	vgui::Label *m_pMapNameLabel;
	vgui::Label *m_pDescriptionLabel;
	CTGAImagePanel *m_pCommentaryScreenshot;
	ImagePanel *m_pCommentaryScreenshotBackground;
	
	Color m_TextColor, m_FillColor, m_SelectedColor;

	int m_iListItemID;
};

//-----------------------------------------------------------------------------
// Purpose:Constructor
//-----------------------------------------------------------------------------
CLoadCommentaryDialog::CLoadCommentaryDialog(vgui::Panel *parent) : BaseClass(parent, "LoadCommentaryDialog")
{
	SetDeleteSelfOnClose(true);
	SetBounds(0, 0, 512, 384);
	SetMinimumSize( 256, 300 );
	SetSizeable( true );

	SetTitle("#GameUI_LoadCommentary", true);

	vgui::Button *cancel = new vgui::Button( this, "Cancel", "#GameUI_Cancel" );
	cancel->SetCommand( "Close" );

	CreateCommentaryItemList();

	new vgui::Button( this, "loadcommentary", "" );
	SetControlEnabled( "loadcommentary", false );

	LoadControlSettings("resource/LoadCommentaryDialog.res");

	// Look for commentary .txt files in the map directory
	ScanCommentaryFiles();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLoadCommentaryDialog::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "loadcommentary" ) )
	{
		intp itemIndex = GetSelectedItemIndex();
		if ( m_CommentaryItems.IsValidIndex(itemIndex) )
		{
			const char *mapName = m_CommentaryItems[itemIndex].szMapName;
			if ( mapName && mapName[ 0 ] )
			{
				// Load the game, return to top and switch to engine
				char sz[ 256 ];
				Q_snprintf(sz, sizeof( sz ), "progress_enable\ncommentary 1\nmap %s\n", mapName );

				// Close this dialog
				OnClose();

				engine->ClientCmd_Unrestricted( sz );
			}
		}
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void CLoadCommentaryDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( code == KEY_XBUTTON_A || code == STEAMCONTROLLER_A )
	{
		OnCommand( "loadcommentary" );
		return;
	}
	else if ( code == KEY_XBUTTON_B )
	{
		OnCommand( "Close" );
		return;
	}

	BaseClass::OnKeyCodePressed(code);
}

//-----------------------------------------------------------------------------
// Purpose: Creates the load game display list
//-----------------------------------------------------------------------------
void CLoadCommentaryDialog::CreateCommentaryItemList()
{
	m_pGameList = new vgui::PanelListPanel( this, "listpanel_commentary" );
	m_pGameList->SetFirstColumnWidth( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: returns the save file name of the selected item
//-----------------------------------------------------------------------------
intp CLoadCommentaryDialog::GetSelectedItemIndex()
{
	CCommentaryItemPanel *panel = dynamic_cast<CCommentaryItemPanel *>(m_pGameList->GetSelectedPanel());
	if ( panel )
	{
		// find the panel in the list
		for ( intp i = 0; i < m_CommentaryItems.Count(); i++ )
		{
			if ( i == panel->GetListItemID() )
			{
				return i;
			}
		}
	}
	return m_CommentaryItems.InvalidIndex();
}

//-----------------------------------------------------------------------------
// Purpose: builds save game list from directory
//-----------------------------------------------------------------------------
void CLoadCommentaryDialog::ScanCommentaryFiles()
{
	// populate list box with all saved games on record:
	char	szDirectory[MAX_PATH];
	Q_snprintf( szDirectory, sizeof( szDirectory ), "maps/*commentary.txt" );

	// clear the current list
	m_pGameList->DeleteAllItems();
	m_CommentaryItems.RemoveAll();

	// iterate the files
	FileFindHandle_t handle;
	const char *pFileName = g_pFullFileSystem->FindFirst( szDirectory, &handle );
	while (pFileName)
	{
		char szFileName[MAX_PATH];
		Q_snprintf(szFileName, sizeof( szFileName ), "maps/%s", pFileName);

		// Only load save games from the current mod's save dir
		if( !g_pFullFileSystem->FileExists( szFileName, "MOD" ) )
		{
			pFileName = g_pFullFileSystem->FindNext( handle );
			continue;
		}

		ParseCommentaryFile( szFileName, pFileName );		

		pFileName = g_pFullFileSystem->FindNext( handle );
	}

	g_pFullFileSystem->FindClose( handle );

	// sort the save list
	std::sort( m_CommentaryItems.begin(), m_CommentaryItems.end(), []( const CommentaryItem_t &s1, const CommentaryItem_t &s2 )
	{
		// Sort by map name
		return Q_stricmp( s1.szPrintName, s2.szPrintName ) < 0;
	});

	// add to the list
	for ( int saveIndex = 0; saveIndex < m_CommentaryItems.Count() && saveIndex < MAX_LISTED_COMMENTARY_ITEMS; saveIndex++ )
	{
		// add the item to the panel
		AddCommentaryItemToList( saveIndex );
	}

	// display a message if there are no save games
	if ( !m_CommentaryItems.Count() )
	{
		vgui::Label *pNoCommentaryItemsLabel = SETUP_PANEL(new Label(m_pGameList, "NoCommentaryItemsLabel", "#GameUI_NoCommentaryItemsToDisplay"));
		pNoCommentaryItemsLabel->SetTextColorState(vgui::Label::CS_DULL);
		m_pGameList->AddItem( NULL, pNoCommentaryItemsLabel );
	}

	SetControlEnabled( "loadcommentary", false );
}

//-----------------------------------------------------------------------------
// Purpose: Adds an item to the list
//-----------------------------------------------------------------------------
void CLoadCommentaryDialog::AddCommentaryItemToList( int itemIndex )
{
	// create the new panel and add to the list
	CCommentaryItemPanel *commentaryItemPanel = new CCommentaryItemPanel( m_pGameList, "CommentaryItemPanel", itemIndex );
	commentaryItemPanel->SetCommentaryInfo( m_CommentaryItems[itemIndex] );
	m_pGameList->AddItem( NULL, commentaryItemPanel );
}

//-----------------------------------------------------------------------------
// Purpose: Parses the save game info out of the .sav file header
//-----------------------------------------------------------------------------
void CLoadCommentaryDialog::ParseCommentaryFile( char const *pszFileName, char const *pszShortName )
{
	if ( !pszFileName || !pszShortName )
		return;

	// load the file as keyvalues
	KeyValuesAD pData( "commentary_data" );
	if ( !pData->LoadFromFile( g_pFullFileSystem, pszFileName, "MOD" ) )
	{
		return;
	}
	
	// walk the platform menu loading all the interfaces
	KeyValues *menuKeys = pData->FindKey("trackinfo", false);
	if ( menuKeys )
	{
		for (KeyValues *track = menuKeys->GetFirstSubKey(); track != NULL; track = track->GetNextKey())
		{
			//Msg( "track found: %s %s\n", track->GetString("map", "?"), track->GetString( "description", "asdf" ) );

			CommentaryItem_t item;
			V_strcpy_safe( item.szMapFileName, pszFileName );
			V_strcpy_safe( item.szMapName, track->GetString( "map", "" ) );
			V_strcpy_safe( item.szPrintName, track->GetString( "printname", "" ) );
			V_strcpy_safe( item.szDescription, track->GetString( "description", "" ) );

			//item.iChannel = track->GetInt( "channel" );

			m_CommentaryItems.AddToTail( item );
		}
	}
	else
	{
		CommentaryItem_t item;
		V_strcpy_safe( item.szMapFileName, pszFileName );

		char mapname[MAX_PATH];
		V_strcpy_safe( mapname, pszFileName );
		char *ext = strstr( mapname, "_commentary" );
		if ( !ext )
			return;
		
		*ext = '\0';
		V_FileBase( mapname, item.szMapName );

		V_strcpy_safe( item.szPrintName, "No trackinfo found." );
		V_strcpy_safe( item.szDescription, "No trackinfo found." );
		m_CommentaryItems.AddToTail( item );
	}
}


//-----------------------------------------------------------------------------
// Purpose: One item has been selected
//-----------------------------------------------------------------------------
void CLoadCommentaryDialog::OnPanelSelected()
{
	SetControlEnabled( "loadcommentary", true );
}
