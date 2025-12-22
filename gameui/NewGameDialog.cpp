//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "BasePanel.h"
#include "NewGameDialog.h"
#include "EngineInterface.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/CheckButton.h"
#include "KeyValues.h"
#include "vgui/ISurface.h"
#include "vgui/IInput.h"
#include "vgui/ILocalize.h"
#include <vgui/ISystem.h>
#include "vgui_controls/RadioButton.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/ControllerMap.h"
#include "filesystem.h"
#include "ModInfo.h"
#include "tier1/convar.h"
#include "GameUI_Interface.h"
#include "tier0/icommandline.h"
#include "vgui_controls/AnimationController.h"
#include "CommentaryExplanationDialog.h"
#include "vgui_controls/BitmapImagePanel.h"
#include "BonusMapsDatabase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static float	g_ScrollSpeedSlow;
static float	g_ScrollSpeedFast;

// sort function used in displaying chapter list
struct chapter_t
{
	char filename[32];
	intp length;
};
static bool ChapterSortFunc(const chapter_t &c1, const chapter_t &c2)
{
	// compare chapter number first
	constexpr intp chapterlen = ssize("chapter") - 1;
	if (atoi(c1.filename + chapterlen) > atoi(c2.filename + chapterlen))
		return false;
	if (atoi(c1.filename + chapterlen) < atoi(c2.filename + chapterlen))
		return true;

	const size_t len1 = c1.length;
	const size_t len2 = c2.length;

	// compare length second (longer string show up later in the list, eg. chapter9 before chapter9a)
	if (len1 > len2)
		return false;
	if (len1 < len2)
		return true;

	// compare strings third
	return strcmp(c1.filename, c2.filename) < 0;
}

//-----------------------------------------------------------------------------
// Purpose: invisible panel used for selecting a chapter panel
//-----------------------------------------------------------------------------
class CSelectionOverlayPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CSelectionOverlayPanel, Panel );
	int m_iChapterIndex;
	CNewGameDialog *m_pSelectionTarget;
public:
	CSelectionOverlayPanel( Panel *parent, CNewGameDialog *selectionTarget, int chapterIndex ) : BaseClass( parent, NULL )
	{
		m_iChapterIndex = chapterIndex;
		m_pSelectionTarget = selectionTarget;
		SetPaintEnabled(false);
		SetPaintBackgroundEnabled(false);
	}

	void OnMousePressed( vgui::MouseCode code ) override 
	{
		if (GetParent()->IsEnabled())
		{
			m_pSelectionTarget->SetSelectedChapterIndex( m_iChapterIndex );
		}
	}

	void OnMouseDoublePressed( vgui::MouseCode code ) override
	{
		// call the panel
		OnMousePressed( code );
		if (GetParent()->IsEnabled())
		{
			PostMessage( m_pSelectionTarget, new KeyValues("Command", "command", "play") );
		}
	}
};

//-----------------------------------------------------------------------------
// Purpose: selectable item with screenshot for an individual chapter in the dialog
//-----------------------------------------------------------------------------
class CGameChapterPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CGameChapterPanel, vgui::EditablePanel );

	ImagePanel *m_pLevelPicBorder;
	ImagePanel *m_pLevelPic;
	ImagePanel *m_pCommentaryIcon;
	Label *m_pChapterLabel;
	Label *m_pChapterNameLabel;

	Color m_TextColor;
	Color m_DisabledColor;
	Color m_SelectedColor;
	Color m_FillColor;

	char m_szConfigFile[MAX_PATH];
	char m_szChapter[32];

	bool m_bTeaserChapter;
	bool m_bHasBonus;
	bool m_bCommentaryMode;

	bool m_bIsSelected;

public:
	CGameChapterPanel( CNewGameDialog *parent, const char *name, const char *chapterName, int chapterIndex, const char *chapterNumber, const char *chapterConfigFile, bool bCommentary ) : BaseClass( parent, name )
	{
		V_strcpy_safe( m_szConfigFile, chapterConfigFile );
		V_strcpy_safe( m_szChapter, chapterNumber );

		m_pLevelPicBorder = SETUP_PANEL( new ImagePanel( this, "LevelPicBorder" ) );
		m_pLevelPic = SETUP_PANEL( new ImagePanel( this, "LevelPic" ) );
		m_pCommentaryIcon = NULL;
		m_bCommentaryMode = bCommentary;
		m_bIsSelected = false;

		wchar_t text[32];
		wchar_t num[32];
		wchar_t *chapter = g_pVGuiLocalize->Find("#GameUI_Chapter");
		g_pVGuiLocalize->ConvertANSIToUnicode( chapterNumber, num );
		V_swprintf_safe( text, L"%ls %ls", chapter ? chapter : L"CHAPTER", num );

		if ( ModInfo().IsSinglePlayerOnly() )
		{
			m_pChapterLabel = new Label( this, "ChapterLabel", text );
			m_pChapterNameLabel = new Label( this, "ChapterNameLabel", chapterName );
		}
		else
		{
			m_pChapterLabel = new Label( this, "ChapterLabel", chapterName );
			m_pChapterNameLabel = new Label( this, "ChapterNameLabel", "#GameUI_LoadCommentary" );
		}

		SetPaintBackgroundEnabled( false );

		// the image has the same name as the config file
		char szMaterial[ MAX_PATH ];
		V_sprintf_safe( szMaterial, "chapters/%s", chapterConfigFile );
		char *ext = strchr( szMaterial, '.' );
		if ( ext )
		{
			*ext = '\0';
		}
		m_pLevelPic->SetImage( szMaterial );

		LoadControlSettings( "Resource/NewGameChapterPanel.res", NULL, nullptr );

		int px, py;
		m_pLevelPicBorder->GetPos( px, py );
		SetSize( m_pLevelPicBorder->GetWide(), py + m_pLevelPicBorder->GetTall() );

		// create a selection panel the size of the page
		CSelectionOverlayPanel *overlay = new CSelectionOverlayPanel( this, parent, chapterIndex );
		overlay->SetBounds(0, 0, GetWide(), GetTall());
		overlay->MoveToFront();

		// HACK: Detect new episode teasers by the "Coming Soon" text
		wchar_t w_szStrTemp[256];
		m_pChapterNameLabel->GetText( w_szStrTemp );
		m_bTeaserChapter = !wcscmp(w_szStrTemp, L"Coming Soon");
		m_bHasBonus = false;
	}

	void ApplySchemeSettings( IScheme *pScheme ) override
	{
		m_TextColor = pScheme->GetColor( "NewGame.TextColor", Color(255, 255, 255, 255) );
		m_FillColor = pScheme->GetColor( "NewGame.FillColor", Color(255, 255, 255, 255) );
		m_DisabledColor = pScheme->GetColor( "NewGame.DisabledColor", Color(255, 255, 255, 255) );
		m_SelectedColor = pScheme->GetColor( "NewGame.SelectionColor", Color(255, 255, 255, 255) );

		BaseClass::ApplySchemeSettings( pScheme );

		// Hide chapter numbers for new episode teasers
		if ( m_bTeaserChapter )
		{
			m_pChapterLabel->SetVisible( false );
		}

		m_pCommentaryIcon = dynamic_cast<ImagePanel*>( FindChildByName( "CommentaryIcon" ) );
		if ( m_pCommentaryIcon )
			m_pCommentaryIcon->SetVisible( m_bCommentaryMode );
	}

	bool IsSelected( void ) const { return m_bIsSelected; }

	void SetSelected( bool state )
	{
		m_bIsSelected = state;

		// update the text/border colors
		if ( !IsEnabled() )
		{
			m_pChapterLabel->SetFgColor( m_DisabledColor );
			m_pChapterNameLabel->SetFgColor( Color(0, 0, 0, 0) );
			m_pLevelPicBorder->SetFillColor( m_DisabledColor );
			m_pLevelPic->SetAlpha( 128 );
			return;
		}

		if ( state )
		{
			m_pChapterLabel->SetFgColor( m_SelectedColor );
			m_pChapterNameLabel->SetFgColor( m_SelectedColor );
			m_pLevelPicBorder->SetFillColor( m_SelectedColor );
		}
		else
		{
			m_pChapterLabel->SetFgColor( m_TextColor );
			m_pChapterNameLabel->SetFgColor( m_TextColor );
			m_pLevelPicBorder->SetFillColor( m_FillColor );
		}
		m_pLevelPic->SetAlpha( 255 );
	}

	const char *GetConfigFile()
	{
		return m_szConfigFile;
	}

	const char *GetChapter()
	{
		return m_szChapter;
	}

	bool IsTeaserChapter()
	{
		return m_bTeaserChapter;
	}

	bool HasBonus()
	{
		return m_bHasBonus;
	}

	void SetCommentaryMode( bool bCommentaryMode )
	{
		m_bCommentaryMode = bCommentaryMode;
		if ( m_pCommentaryIcon )
			m_pCommentaryIcon->SetVisible( m_bCommentaryMode );
	}
};

const char *COM_GetModDirectory()
{
	static char modDir[MAX_PATH] = {};
	if ( Q_isempty( modDir ) )
	{
		const char *gamedir = CommandLine()->ParmValue("-game", CommandLine()->ParmValue( "-defaultgamedir", "hl2" ) );
		V_strcpy_safe( modDir, gamedir );
		if ( strchr( modDir, '/' ) || strchr( modDir, '\\' ) )
		{
			V_StripLastDir( modDir );
			const intp dirlen = V_strlen( modDir );
			V_strncpy( modDir, gamedir + dirlen, sizeof(modDir) - dirlen );
		}
	}

	return modDir;
}

//-----------------------------------------------------------------------------
// Purpose: new game chapter selection
//-----------------------------------------------------------------------------
CNewGameDialog::CNewGameDialog(vgui::Panel *parent, bool bCommentaryMode) : BaseClass(parent, "NewGameDialog")
{
	SetDeleteSelfOnClose(true);
	SetBounds(0, 0, QuickPropScale( 372 ), QuickPropScale( 160 ));
	SetSizeable( false );
	m_iSelectedChapter = -1;

	m_bCommentaryMode = bCommentaryMode;
	m_bScrolling = false;
	m_ScrollCt = 0;
	m_ScrollSpeed = 0.f;
	m_ScrollDirection = SCROLL_NONE;
	m_pCommentaryLabel = NULL;

	m_iBonusSelection = 0;

	SetTitle("#GameUI_NewGame", true);

	m_pNextButton = new Button( this, "Next", "#gameui_next" );
	m_pPrevButton = new Button( this, "Prev", "#gameui_prev" );
	m_pPlayButton = new CNewGamePlayButton( this, "Play", "#GameUI_Play" );
	m_pPlayButton->SetCommand( "Play" );

	vgui::Button *cancel = new vgui::Button( this, "Cancel", "#GameUI_Cancel" );
	cancel->SetCommand( "Close" );

	m_pCenterBg = SETUP_PANEL( new Panel( this, "CenterBG" ) );
	m_pCenterBg->SetVisible( false );

	// parse out the chapters off disk
	constexpr int MAX_CHAPTERS = 32;
	chapter_t chapters[MAX_CHAPTERS];

	char szFullFileName[MAX_PATH];
	int chapterIndex = 0;

	{
		// dimhotepus: This can take a while, put up a waiting cursor.
    	const vgui::ScopedPanelWaitCursor scopedWaitCursor{this};

		FileFindHandle_t findHandle = FILESYSTEM_INVALID_FIND_HANDLE;
		const char *fileName = g_pFullFileSystem->FindFirst( "cfg/chapter*.cfg", &findHandle );
		while ( fileName && chapterIndex < MAX_CHAPTERS )
		{
			if ( fileName[0] )
			{
				// Only load chapter configs from the current mod's cfg dir
				// or else chapters appear that we don't want!
				V_sprintf_safe( szFullFileName, "cfg/%s", fileName );
				FileHandle_t f = g_pFullFileSystem->Open( szFullFileName, "rb", "MOD" );
				if ( f )
				{
					RunCodeAtScopeExit(g_pFullFileSystem->Close( f ));

					// don't load chapter files that are empty, used in the demo
					if ( g_pFullFileSystem->Size(f) > 0	)
					{
						V_strcpy_safe(chapters[chapterIndex].filename, fileName);
						chapters[chapterIndex].length = V_strlen(chapters[chapterIndex].filename);
						++chapterIndex;
					}
				}
			}
			fileName = g_pFullFileSystem->FindNext(findHandle);
		}
	}

	// sort the chapters
	std::sort(std::begin(chapters), std::begin(chapters) + chapterIndex, ChapterSortFunc);

	// work out which chapters are unlocked
	ConVarRef var( "sv_unlockedchapters" );

	const char *unlockedChapter = var.IsValid() ? var.GetString() : "1";
	int iUnlockedChapter = atoi(unlockedChapter);

	// add chapters to combobox
	for (int i = 0; i < chapterIndex; i++)
	{
		const char *fileName = chapters[i].filename;

		char chapterID[32] = { 0 };
		sscanf(fileName, "chapter%31s", chapterID);
		chapterID[ssize(chapterID) - 1] = '\0';

		V_sprintf_safe( szFullFileName, "%s", fileName );

		// strip the extension
		char *ext = V_stristr(chapterID, ".cfg");
		if (ext)
		{
			*ext = 0;
		}

		const char *pGameDir = COM_GetModDirectory();

		char chapterName[64];
		V_sprintf_safe(chapterName, "#%s_Chapter%s_Title", pGameDir, chapterID);

		CGameChapterPanel *chapterPanel = SETUP_PANEL( new CGameChapterPanel( this, NULL, chapterName, i, chapterID, szFullFileName, m_bCommentaryMode ) );
		chapterPanel->SetVisible( false );

		UpdatePanelLockedStatus( iUnlockedChapter, i + 1, chapterPanel );

		m_ChapterPanels.AddToTail( chapterPanel );
	}

	LoadControlSettings( "Resource/NewGameDialog.res", NULL, nullptr );

	// Reset all properties
	for ( auto &idx : m_PanelIndex )
	{
		idx = INVALID_INDEX;
	}

	if ( !m_ChapterPanels.Count() )
	{
		UpdateMenuComponents( SCROLL_NONE );
		return;
	}

	// Layout panel positions relative to the dialog center.
	int panelWidth = m_ChapterPanels[0]->GetWide() + QuickPropScale( 16 );
	int dialogWidth = GetWide();
	
	m_PanelXPos[2] = ( dialogWidth - panelWidth ) / 2 + QuickPropScale( 8 );
	
	if (m_ChapterPanels.Count() > 1)
	{
		m_PanelXPos[1] = m_PanelXPos[2] - panelWidth;
		m_PanelXPos[0] = m_PanelXPos[1];
		m_PanelXPos[3] = m_PanelXPos[2] + panelWidth;
		m_PanelXPos[4] = m_PanelXPos[3];
	}
	else
	{
		m_PanelXPos[0] = m_PanelXPos[1] = m_PanelXPos[3] =
		m_PanelXPos[4] = m_PanelXPos[2];
	}


	m_PanelAlpha[0] = 0;
	m_PanelAlpha[1] = 255;
	m_PanelAlpha[2] = 255;
	m_PanelAlpha[3] = 255;
	m_PanelAlpha[4] = 0;

	int panelHeight;
	m_ChapterPanels[0]->GetSize( panelWidth, panelHeight );
	m_pCenterBg->SetWide( panelWidth + QuickPropScale( 16 ) );
	m_pCenterBg->SetPos( m_PanelXPos[2] - QuickPropScale( 8 ), m_PanelYPos[2] - (m_pCenterBg->GetTall() - panelHeight) + QuickPropScale( 8 ) );
	m_pCenterBg->SetBgColor( Color( 190, 115, 0, 255 ) );

	// start the first item selected
	SetSelectedChapterIndex( 0 );
}

CNewGameDialog::~CNewGameDialog()
{
}

void CNewGameDialog::Activate( void )
{
	// Commentary stuff is set up on activate because in XBox the new game menu is never deleted
	SetTitle( ( ( m_bCommentaryMode ) ? ( "#GameUI_LoadCommentary" ) : ( "#GameUI_NewGame") ), true);

	if ( m_pCommentaryLabel )
		m_pCommentaryLabel->SetVisible( m_bCommentaryMode );

	// work out which chapters are unlocked
	ConVarRef var( "sv_unlockedchapters" );
	const char *unlockedChapter = var.IsValid() ? var.GetString() : "1";
	int iUnlockedChapter = atoi(unlockedChapter);

	for ( intp i = 0; i < m_ChapterPanels.Count(); i++)
	{
		CGameChapterPanel *pChapterPanel = m_ChapterPanels[ i ];

		if ( pChapterPanel )
		{
			pChapterPanel->SetCommentaryMode( m_bCommentaryMode );

			UpdatePanelLockedStatus( iUnlockedChapter, i + 1, pChapterPanel );
		}
	}

	BaseClass::Activate();
}

//-----------------------------------------------------------------------------
// Purpose: Apply special properties of the menu
//-----------------------------------------------------------------------------
void CNewGameDialog::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	int ypos = QuickPropScale( inResourceData->GetInt( "chapterypos", 40 ) );
	for ( auto &idx : m_PanelYPos )
	{
		idx = ypos;
	}

	// dimhotepus: Scale UI,
	m_pCenterBg->SetTall( QuickPropScale( inResourceData->GetInt( "centerbgtall", 0 ) ) );

	g_ScrollSpeedSlow = inResourceData->GetFloat( "scrollslow", 0.0f );
	g_ScrollSpeedFast = inResourceData->GetFloat( "scrollfast", 0.0f );
	SetFastScroll( false );
}

void CNewGameDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	UpdateMenuComponents( SCROLL_NONE );

	m_pCommentaryLabel = dynamic_cast<vgui::Label*>( FindChildByName( "CommentaryUnlock" ) );
	if ( m_pCommentaryLabel )
		m_pCommentaryLabel->SetVisible( m_bCommentaryMode );
}

//-----------------------------------------------------------------------------
// Purpose: sets the correct properties for visible components
//-----------------------------------------------------------------------------
void CNewGameDialog::UpdateMenuComponents( EScrollDirection dir )
{
	// This is called prior to any scrolling, 
	// so we need to look ahead to the post-scroll state
	int centerIdx = SLOT_CENTER;
	if ( dir == SCROLL_LEFT )
	{
		++centerIdx;
	}
	else if ( dir == SCROLL_RIGHT )
	{
		--centerIdx;
	}
	int leftIdx = centerIdx - 1;
	int rightIdx = centerIdx + 1;

	if ( m_PanelIndex[leftIdx] == INVALID_INDEX || m_PanelIndex[leftIdx] == 0 )
	{
		m_pPrevButton->SetVisible( false );
		m_pPrevButton->SetEnabled( false );
	}
	else
	{
		m_pPrevButton->SetVisible( true );
		m_pPrevButton->SetEnabled( true );
	}

	if ( m_ChapterPanels.Count() < 4 ) // if there are less than 4 chapters show the next button but disabled
	{
		m_pNextButton->SetVisible( true );
		m_pNextButton->SetEnabled( false );
	}
	else if ( m_PanelIndex[rightIdx] == INVALID_INDEX || m_PanelIndex[rightIdx] == m_ChapterPanels.Count()-1 )
	{
		m_pNextButton->SetVisible( false );
		m_pNextButton->SetEnabled( false );
	}
	else
	{
		m_pNextButton->SetVisible( true );
		m_pNextButton->SetEnabled( true );
	}
}

void CNewGameDialog::UpdateBonusSelection( void )
{
	int iNumChallenges = 0;
	if ( m_pBonusMapDescription )
	{
		if ( m_pBonusMapDescription->m_pChallenges )
			iNumChallenges = m_pBonusMapDescription->m_pChallenges->Count();

		// Wrap challenge selection to fit number of possible selections
		if ( m_iBonusSelection < 0 )
			m_iBonusSelection = iNumChallenges + 1;
		else if ( m_iBonusSelection >= iNumChallenges + 2 )
			m_iBonusSelection = 0;
	}
	else
	{
		// No medals to show
		SetControlVisible( "ChallengeEarnedMedal", false );
		SetControlVisible( "ChallengeBestLabel", false );
		SetControlVisible( "ChallengeNextMedal", false );
		SetControlVisible( "ChallengeNextLabel", false );
		return;
	}

	if ( m_iBonusSelection == 0 )
	{
		m_pBonusSelection->SetText( "#GameUI_BonusMapsStandard" );
		SetControlVisible( "ChallengeEarnedMedal", false );
		SetControlVisible( "ChallengeBestLabel", false );
		SetControlVisible( "ChallengeNextMedal", false );
		SetControlVisible( "ChallengeNextLabel", false );
	}
	else if ( m_iBonusSelection == 1 )
	{
		m_pBonusSelection->SetText( "#GameUI_BonusMapsAdvanced" );
		SetControlVisible( "ChallengeEarnedMedal", false );
		SetControlVisible( "ChallengeBestLabel", false );
		SetControlVisible( "ChallengeNextMedal", false );
		SetControlVisible( "ChallengeNextLabel", false );

		char szMapAdvancedName[ 256 ] = "";
		if ( m_pBonusMapDescription )
		{
			V_sprintf_safe( szMapAdvancedName, "%s_advanced", m_pBonusMapDescription->szMapFileName );
		}

		BonusMapDescription_t *pAdvancedDescription = NULL;

		// Find the bonus description for this panel
		for ( intp iBonus = 0; iBonus < BonusMapsDatabase()->BonusCount(); ++iBonus )
		{
			pAdvancedDescription = BonusMapsDatabase()->GetBonusData( iBonus );
			if ( Q_stricmp( szMapAdvancedName, pAdvancedDescription->szMapFileName ) == 0 )
				break;
		}

		if ( pAdvancedDescription && pAdvancedDescription->bComplete )
		{
			CBitmapImagePanel *pBitmap = dynamic_cast<CBitmapImagePanel*>( FindChildByName( "ChallengeEarnedMedal" ) );
			pBitmap->SetVisible( true );
			pBitmap->setTexture( "hud/icon_complete" );
		}
	}
	else
	{
		int iChallenge = m_iBonusSelection - 2;
		ChallengeDescription_t *pChallengeDescription = &((*m_pBonusMapDescription->m_pChallenges)[ iChallenge ]);

		// Set the display text for the selected challenge
		m_pBonusSelection->SetText( pChallengeDescription->szName );

		int iBest, iEarnedMedal, iNext, iNextMedal;
		GetChallengeMedals( pChallengeDescription, iBest, iEarnedMedal, iNext, iNextMedal );

		char szBuff[ 512 ];

		// Set earned medal
		if ( iEarnedMedal > -1 && iBest != -1 )
		{
			if ( iChallenge < 10 )
				V_sprintf_safe( szBuff, "medals/medal_0%i_%s", iChallenge, g_pszMedalNames[ iEarnedMedal ] );
			else
				V_sprintf_safe( szBuff, "medals/medal_%i_%s", iChallenge, g_pszMedalNames[ iEarnedMedal ] );

			CBitmapImagePanel *pBitmap = dynamic_cast<CBitmapImagePanel*>( FindChildByName( "ChallengeEarnedMedal" ) );
			pBitmap->SetVisible( true );
			pBitmap->setTexture( szBuff );
		}
		else
		{
			CBitmapImagePanel *pBitmap = dynamic_cast<CBitmapImagePanel*>( FindChildByName( "ChallengeEarnedMedal" ) );
			pBitmap->SetVisible( false );
		}

		// Set next medal
		if ( iNextMedal > 0 )
		{
			if ( iChallenge < 10 )
				V_sprintf_safe( szBuff, "medals/medal_0%i_%s", iChallenge, g_pszMedalNames[ iNextMedal ] );
			else
				V_sprintf_safe( szBuff, "medals/medal_%i_%s", iChallenge, g_pszMedalNames[ iNextMedal ] );

			CBitmapImagePanel *pBitmap = dynamic_cast<CBitmapImagePanel*>( FindChildByName( "ChallengeNextMedal" ) );
			pBitmap->SetVisible( true );
			pBitmap->setTexture( szBuff );
		}
		else
		{
			SetControlVisible( "ChallengeNextMedal", false );
		}

		wchar_t szWideBuff[ 64 ];
		wchar_t szWideBuff2[ 64 ];

		// Best label
		if ( iBest != -1 )
		{
			V_to_chars( szBuff, iBest );
			g_pVGuiLocalize->ConvertANSIToUnicode( szBuff, szWideBuff2 );
			g_pVGuiLocalize->ConstructString_safe( szWideBuff, g_pVGuiLocalize->Find( "#GameUI_BonusMapsBest" ), 1, szWideBuff2 );
			g_pVGuiLocalize->ConvertUnicodeToANSI( szWideBuff, szBuff );

			SetControlString( "ChallengeBestLabel", szBuff );
			SetControlVisible( "ChallengeBestLabel", true );
		}
		else
		{
			SetControlVisible( "ChallengeBestLabel", false );
		}

		// Next label
		if ( iNext != -1 )
		{
			V_to_chars( szBuff, iNext );
			g_pVGuiLocalize->ConvertANSIToUnicode( szBuff, szWideBuff2 );
			g_pVGuiLocalize->ConstructString_safe( szWideBuff, g_pVGuiLocalize->Find( "#GameUI_BonusMapsGoal" ), 1, szWideBuff2 );
			g_pVGuiLocalize->ConvertUnicodeToANSI( szWideBuff, szBuff );

			SetControlString( "ChallengeNextLabel", szBuff );
			SetControlVisible( "ChallengeNextLabel", true );
		}
		else
		{
			SetControlVisible( "ChallengeNextLabel", false );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets a chapter as selected
//-----------------------------------------------------------------------------
void CNewGameDialog::SetSelectedChapterIndex( intp index )
{
	m_iSelectedChapter = index;

	for (intp i = 0; i < m_ChapterPanels.Count(); i++)
	{
		m_ChapterPanels[i]->SetSelected( i == index );
	}

	if ( m_pPlayButton )
	{
		m_pPlayButton->SetEnabled( true );
	}

	// Setup panels to the left of the selected panel
	intp selectedSlot = index % 3 + 1;
	intp currIdx = index;
	for ( intp i = selectedSlot; i >= 0 && currIdx >= 0; --i )
	{
		m_PanelIndex[i] = currIdx;
		--currIdx;
		InitPanelIndexForDisplay( i );
	}

	// Setup panels to the right of the selected panel
	currIdx = index + 1;
	for ( intp i = selectedSlot + 1; i < NUM_SLOTS && currIdx < m_ChapterPanels.Count(); ++i )
	{
		m_PanelIndex[i] = currIdx;
		++currIdx;
		InitPanelIndexForDisplay( i );
	}

	UpdateMenuComponents( SCROLL_NONE );
}

//-----------------------------------------------------------------------------
// Purpose: sets a chapter as selected
//-----------------------------------------------------------------------------
void CNewGameDialog::SetSelectedChapter( const char *chapter )
{
	Assert( chapter );
	for (intp i = 0; i < m_ChapterPanels.Count(); i++)
	{
		if ( chapter && !Q_stricmp(m_ChapterPanels[i]->GetChapter(), chapter) )
		{
			m_iSelectedChapter = i;
			m_ChapterPanels[m_iSelectedChapter]->SetSelected( true );
		}
		else
		{
			m_ChapterPanels[i]->SetSelected( false );
		}
	}

	if ( m_pPlayButton )
	{
		m_pPlayButton->SetEnabled( true );
	}
}


//-----------------------------------------------------------------------------
// iUnlockedChapter - the value of sv_unlockedchapters, 1-based. A value of 0
//		is treated as a 1, since at least one chapter must be unlocked.
//
// i - the 1-based index of the chapter we're considering.
//-----------------------------------------------------------------------------
void CNewGameDialog::UpdatePanelLockedStatus( int iUnlockedChapter, int i, CGameChapterPanel *pChapterPanel )
{
	if ( iUnlockedChapter <= 0 )
	{
		iUnlockedChapter = 1;
	}

	// Commentary mode requires chapters to be finished before they can be chosen
	bool bLocked = false;

	if ( m_bCommentaryMode )
	{
		bLocked = ( iUnlockedChapter <= i );
	}
	else
	{
		if ( iUnlockedChapter < i )
		{
			// Never lock the first chapter
			bLocked = ( i != 0 );
		}
	}

	pChapterPanel->SetEnabled( !bLocked );
}

//-----------------------------------------------------------------------------
// Purpose: Called before a panel scroll starts.
//-----------------------------------------------------------------------------
void CNewGameDialog::PreScroll( EScrollDirection dir )
{
	intp hideIdx = INVALID_INDEX;
	if ( dir == SCROLL_LEFT )
	{
		hideIdx = m_PanelIndex[SLOT_LEFT];
	}
	else if ( dir == SCROLL_RIGHT )
	{
		hideIdx = m_PanelIndex[SLOT_RIGHT];
	}
	if ( hideIdx != INVALID_INDEX )
	{
		// Push back the panel that's about to be hidden
		// so the next panel scrolls over the top of it.
		m_ChapterPanels[hideIdx]->SetZPos( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called after a panel scroll finishes.
//-----------------------------------------------------------------------------
void CNewGameDialog::PostScroll( EScrollDirection dir )
{
	intp index = INVALID_INDEX;
	if ( dir == SCROLL_LEFT )
	{
		index = m_PanelIndex[SLOT_RIGHT];
	}
	else if ( dir == SCROLL_RIGHT )
	{
		index = m_PanelIndex[SLOT_LEFT];
	}

	// Fade in the revealed panel
	if ( index != INVALID_INDEX )
	{
		CGameChapterPanel *panel = m_ChapterPanels[index];
		panel->SetZPos( 50 );
		GetAnimationController()->RunAnimationCommand( panel, "alpha", 255, 0, m_ScrollSpeed, vgui::AnimationController::INTERPOLATOR_LINEAR );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Initiates a panel scroll and starts the animation.
//-----------------------------------------------------------------------------
void CNewGameDialog::ScrollSelectionPanels( EScrollDirection dir )
{
	// Only initiate a scroll if panels aren't currently scrolling
	if ( !m_bScrolling )
	{
		// Handle any pre-scroll setup
		PreScroll( dir );

		if ( dir == SCROLL_LEFT)
		{
			m_ScrollCt += SCROLL_LEFT;
		}
		else if ( dir == SCROLL_RIGHT && m_PanelIndex[SLOT_CENTER] != 0 )
		{
			m_ScrollCt += SCROLL_RIGHT;
		}

		m_bScrolling = true;
		AnimateSelectionPanels();

		// Update the arrow colors, help text, and buttons. Doing it here looks better than having
		// the components change after the entire scroll animation has finished.
		UpdateMenuComponents( m_ScrollDirection );
	}
}

void CNewGameDialog::ScrollBonusSelection( EScrollDirection dir )
{
	// Don't scroll if there's no bonuses for this panel
	if ( !m_pBonusMapDescription )
		return;

	m_iBonusSelection += dir;

	vgui::surface()->PlaySound( "UI/buttonclick.wav" );

	UpdateBonusSelection();
}

//-----------------------------------------------------------------------------
// Purpose: Initiates the scripted scroll and fade effects of all five slotted panels 
//-----------------------------------------------------------------------------
void CNewGameDialog::AnimateSelectionPanels( void )
{
	int idxOffset = 0;
	int startIdx = SLOT_LEFT;
	int endIdx = SLOT_RIGHT;

	// Don't scroll outside the bounds of the panel list
	if ( m_ScrollCt >= SCROLL_LEFT )
	{
		idxOffset = -1;
		endIdx = SLOT_OFFRIGHT;
		m_ScrollDirection = SCROLL_LEFT;
	}
	else if ( m_ScrollCt <= SCROLL_RIGHT )
	{
		idxOffset = 1;
		startIdx = SLOT_OFFLEFT;
		m_ScrollDirection = SCROLL_RIGHT;
	}

	if ( 0 == idxOffset )
	{
		// Kill the scroll, it's outside the bounds
		m_ScrollCt = 0;
		m_bScrolling = false;
		m_ScrollDirection = SCROLL_NONE;
		vgui::surface()->PlaySound( "player/suit_denydevice.wav" );
		return;
	}

	// Should never happen
	if ( startIdx > endIdx )
		return;

	for ( int i = startIdx; i <= endIdx; ++i )
	{
		if ( m_PanelIndex[i] != INVALID_INDEX )
		{
			int nextIdx = i + idxOffset;
			CGameChapterPanel *panel = m_ChapterPanels[ m_PanelIndex[i] ];
			GetAnimationController()->RunAnimationCommand( panel, "xpos",  m_PanelXPos[nextIdx],  0, m_ScrollSpeed, vgui::AnimationController::INTERPOLATOR_LINEAR );
			GetAnimationController()->RunAnimationCommand( panel, "ypos",  m_PanelYPos[nextIdx],  0, m_ScrollSpeed, vgui::AnimationController::INTERPOLATOR_LINEAR );
			GetAnimationController()->RunAnimationCommand( panel, "alpha", m_PanelAlpha[nextIdx], 0, m_ScrollSpeed, vgui::AnimationController::INTERPOLATOR_LINEAR );
		}
	}

	PostMessage( this, new KeyValues( "FinishScroll" ), m_ScrollSpeed );
}

//-----------------------------------------------------------------------------
// Purpose: After a scroll, each panel slot holds the index of a panel that has 
//			scrolled to an adjacent slot. This function updates each slot so
//			it holds the index of the panel that is actually in that slot's position.
//-----------------------------------------------------------------------------
void CNewGameDialog::ShiftPanelIndices( int offset )
{
	// Shift all the elements over one slot, then calculate what the last slot's index should be.
	int lastSlot = NUM_SLOTS - 1;
	if ( offset > 0 )
	{
		// Hide the panel that's dropping out of the slots
		if ( IsValidPanel( m_PanelIndex[0] ) )
		{
			m_ChapterPanels[ m_PanelIndex[0] ]->SetVisible( false );
		}

		// Scrolled panels to the right, so shift the indices one slot to the left
		Q_memmove( &m_PanelIndex[0], &m_PanelIndex[1], lastSlot * sizeof( m_PanelIndex[0] ) );
		if ( m_PanelIndex[lastSlot] != INVALID_INDEX )
		{
			int num = m_PanelIndex[ lastSlot ] + 1;
			if ( IsValidPanel( num ) )
			{
				m_PanelIndex[lastSlot] = num;
				InitPanelIndexForDisplay( lastSlot );
			}
			else
			{
				m_PanelIndex[lastSlot] = INVALID_INDEX;
			}
		}
	}
	else
	{
		// Hide the panel that's dropping out of the slots
		if ( IsValidPanel( m_PanelIndex[lastSlot] ) )
		{
			m_ChapterPanels[ m_PanelIndex[lastSlot] ]->SetVisible( false );
		}

		// Scrolled panels to the left, so shift the indices one slot to the right
		Q_memmove( &m_PanelIndex[1], &m_PanelIndex[0], lastSlot * sizeof( m_PanelIndex[0] ) );
		if ( m_PanelIndex[0] != INVALID_INDEX )
		{
			int num = m_PanelIndex[0] - 1;
			if ( IsValidPanel( num ) )
			{
				m_PanelIndex[0] = num;
				InitPanelIndexForDisplay( 0 );
			}
			else
			{
				m_PanelIndex[0] = INVALID_INDEX;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Validates an index into the selection panels vector
//-----------------------------------------------------------------------------
bool CNewGameDialog::IsValidPanel( const intp idx )
{
	if ( idx < 0 || idx >= m_ChapterPanels.Count() )
		return false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Sets up a panel's properties before it is displayed
//-----------------------------------------------------------------------------
void CNewGameDialog::InitPanelIndexForDisplay( const intp idx )
{
	CGameChapterPanel *panel = m_ChapterPanels[ m_PanelIndex[idx] ];
	if ( panel )
	{
		panel->SetPos( m_PanelXPos[idx], m_PanelYPos[idx] );
		panel->SetAlpha( m_PanelAlpha[idx] );
		panel->SetVisible( true );
		if ( m_PanelAlpha[idx] )
		{
			panel->SetZPos( 50 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets which scroll speed should be used
//-----------------------------------------------------------------------------
void CNewGameDialog::SetFastScroll( bool fast )
{
	m_ScrollSpeed = fast ? g_ScrollSpeedFast : g_ScrollSpeedSlow;
}

//-----------------------------------------------------------------------------
// Purpose: Checks if a button is being held down, and speeds up the scroll 
//-----------------------------------------------------------------------------
void CNewGameDialog::ContinueScrolling( void )
{
	if ( m_PanelIndex[SLOT_CENTER-1] % 3 )
	{
		ScrollSelectionPanels( m_ScrollDirection );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when a scroll distance of one slot has been completed
//-----------------------------------------------------------------------------
void CNewGameDialog::FinishScroll( void )
{
	// Fade the center bg panel back in
	GetAnimationController()->RunAnimationCommand( m_pCenterBg, "alpha", 255, 0, m_ScrollSpeed * 0.25f, vgui::AnimationController::INTERPOLATOR_LINEAR );

	ShiftPanelIndices( m_ScrollDirection );
	m_bScrolling = false;
	m_ScrollCt = 0;
	
	// End of scroll step
	PostScroll( m_ScrollDirection );

	// Continue scrolling if necessary
	ContinueScrolling();
}

//-----------------------------------------------------------------------------
// Purpose: starts the game at the specified skill level
//-----------------------------------------------------------------------------
void CNewGameDialog::StartGame( void )
{
	if ( m_ChapterPanels.IsValidIndex( m_iSelectedChapter ) )
	{
		char mapcommand[512];
		V_sprintf_safe( mapcommand, "disconnect\ndeathmatch 0\nprogress_enable\nexec %s\n", m_ChapterPanels[m_iSelectedChapter]->GetConfigFile() );

		// Set commentary
		ConVarRef commentary( "commentary" );
		commentary.SetValue( m_bCommentaryMode );

		ConVarRef sv_cheats( "sv_cheats" );
		sv_cheats.SetValue( m_bCommentaryMode );

		// If commentary is on, we go to the explanation dialog (but not for teaser trailers)
		if ( m_bCommentaryMode && !m_ChapterPanels[m_iSelectedChapter]->IsTeaserChapter() )
		{
			// Check our current state and disconnect us from any multiplayer server we're connected to.
			// This fixes an exploit where players would click "start" on the commentary dialog to enable
			// sv_cheats on the client (via the code above) and then hit <esc> to get out of the explanation dialog.
			if ( GameUI().IsInMultiplayer() )
			{
				engine->ExecuteClientCmd( "disconnect" );
			}

			DHANDLE<CCommentaryExplanationDialog> hCommentaryExplanationDialog;
			if ( !hCommentaryExplanationDialog.Get() )
			{
				hCommentaryExplanationDialog = new CCommentaryExplanationDialog( BasePanel(), mapcommand );
			}
			hCommentaryExplanationDialog->Activate();
		}
		else
		{
			// start map
			BasePanel()->FadeToBlackAndRunEngineCommand( mapcommand );
		}

		OnClose();
	}
}

void CNewGameDialog::OnClose( void )
{
	m_KeyRepeat.Reset();

	BaseClass::OnClose();
}

//-----------------------------------------------------------------------------
// Purpose: handles button commands
//-----------------------------------------------------------------------------
void CNewGameDialog::OnCommand( const char *command )
{
	bool bReset = true;

	if ( !stricmp( command, "Play" ) )
	{
		StartGame();
	}
	else if ( !stricmp( command, "Next" ) )
	{
		ScrollSelectionPanels( SCROLL_LEFT );
		bReset = false;
	}
	else if ( !stricmp( command, "Prev" ) )
	{
		ScrollSelectionPanels( SCROLL_RIGHT );
		bReset = false;
	}
	else if ( !stricmp( command, "Mode_Next" ) )
	{
		ScrollBonusSelection( SCROLL_LEFT );
		bReset = false;
	}
	else if ( !stricmp( command, "Mode_Prev" ) )
	{
		ScrollBonusSelection( SCROLL_RIGHT );
		bReset = false;
	}
	else if ( !Q_stricmp( command, "ReleaseModalWindow" ) )
	{
		vgui::surface()->RestrictPaintToSinglePanel(NULL);
	}
	else
	{
		BaseClass::OnCommand( command );
	}

	if ( bReset )
	{
		m_KeyRepeat.Reset();
	}
}

void CNewGameDialog::OnKeyCodePressed( KeyCode code )
{
	switch ( code )
	{
	case KEY_XBUTTON_LEFT:
	case KEY_XSTICK1_LEFT:
	case KEY_XSTICK2_LEFT:
	case KEY_LEFT:
	case STEAMCONTROLLER_DPAD_LEFT:
		if ( !m_bScrolling )
		{
			for ( intp i = 0; i < m_ChapterPanels.Count(); ++i )
			{
				if ( m_ChapterPanels[ i ]->IsSelected() )
				{
					intp nNewChapter = i - 1;
					if ( nNewChapter >= 0 )
					{
						if ( nNewChapter < m_PanelIndex[ SLOT_LEFT ] && m_PanelIndex[ SLOT_LEFT ] != -1 )
						{
							ScrollSelectionPanels( SCROLL_RIGHT );
						}
						else if ( m_ChapterPanels[ nNewChapter ]->IsEnabled() )
						{
							SetSelectedChapterIndex( nNewChapter );
						}
					}
					break;
				}
			}
		}
		return;

	case KEY_XBUTTON_RIGHT:
	case KEY_XSTICK1_RIGHT:
	case KEY_XSTICK2_RIGHT:
	case KEY_RIGHT:
	case STEAMCONTROLLER_DPAD_RIGHT:
		if ( !m_bScrolling )
		{
			for ( intp i = 0; i < m_ChapterPanels.Count(); ++i )
			{
				if ( m_ChapterPanels[ i ]->IsSelected() )
				{
					int nNewChapter = i + 1;
					if ( nNewChapter < m_ChapterPanels.Count() )
					{
						if ( nNewChapter > m_PanelIndex[ SLOT_RIGHT ] && m_PanelIndex[ SLOT_RIGHT ] != -1 )
						{
							ScrollSelectionPanels( SCROLL_LEFT );
						}
						else if ( m_ChapterPanels[ nNewChapter ]->IsEnabled() )
						{
							SetSelectedChapterIndex( nNewChapter );
						}
					}
					break;
				}
			}
		}
		return;

	case KEY_XBUTTON_B:
	case STEAMCONTROLLER_B:
		OnCommand( "Close" );
		return;

	case KEY_XBUTTON_A:
	case STEAMCONTROLLER_A:
		OnCommand( "Play" );
		return;
	}

	m_KeyRepeat.KeyDown( code );

	BaseClass::OnKeyCodePressed( code );
}

void CNewGameDialog::OnKeyCodeReleased( vgui::KeyCode code )
{
	m_KeyRepeat.KeyUp( code );

	BaseClass::OnKeyCodeReleased( code );
}

void CNewGameDialog::OnThink()
{
	vgui::KeyCode code = m_KeyRepeat.KeyRepeated();
	if ( code )
	{
		OnKeyCodeTyped( code );
	}

	BaseClass::OnThink();
}
