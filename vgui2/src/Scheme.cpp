//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <vgui/IScheme.h>

#include <vgui/VGUI.h>
#include <tier1/KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/IPanel.h>
#include <vgui/ISystem.h>
#include <vstdlib/IKeyValuesSystem.h>
#include <vgui/IVGui.h>

#include "tier1/utlvector.h"
#include "tier1/utlrbtree.h"
#include "tier1/utldict.h"
#include "VGUI_Border.h"
#include "ScalableImageBorder.h"
#include "ImageBorder.h"
#include "vgui_internal.h"
#include "bitmap.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;
#define FONT_ALIAS_NAME_LENGTH 64

//-----------------------------------------------------------------------------
// Purpose: Implementation of global scheme interface
//-----------------------------------------------------------------------------
class CScheme : public IScheme
{
public:
	CScheme();

	// gets a string from the default settings section
	const char *GetResourceString(const char *stringName) override;

	// returns a pointer to an existing border
	IBorder *GetBorder(const char *borderName) override;

	// returns a pointer to an existing font
	HFont GetFont(const char *fontName, bool proportional) override;

	// m_pkvColors
	Color GetColor( const char *colorName, Color defaultColor) override;


	void Shutdown( bool full );
	void LoadFromFile( VPANEL sizingPanel, const char *filename, const char *tag, KeyValues *inKeys );

	// Gets at the scheme's name
	const char *GetName() const { return tag; }
	const char *GetFileName() const { return fileName; }

	char const *GetFontName( const HFont& font ) override;

	void ReloadFontGlyphs();

	VPANEL		GetSizingPanel() { return m_SizingPanel; }

	void SpewFonts();

	bool GetFontRange( const char *fontname, int &nMin, int &nMax );
	void SetFontRange( const char *fontname, int nMin, int nMax );
	
	// Get the number of borders
	int GetBorderCount() const override;

	// Get the border at the given index
	IBorder *GetBorderAtIndex( int iIndex ) override;

	// Get the number of fonts
	int GetFontCount() const override;

	// Get the font at the given index
	HFont GetFontAtIndex( int iIndex ) override;
	
	// Get color data
	const KeyValues *GetColorData() const override;

private:
	const char *LookupSchemeSetting(const char *pchSetting);
	const char *GetMungedFontName( const char *fontName, const char *scheme, bool proportional);
	void LoadFonts();
	void LoadBorders();
	HFont FindFontInAliasList( const char *fontName );
	int GetMinimumFontHeightForCurrentLanguage();

	char fileName[256];
	char tag[64];

	KeyValues *m_pData;
	KeyValues *m_pkvBaseSettings;
	KeyValues *m_pkvColors;
	int		   m_nColorCount;

	struct SchemeBorder_t
	{
		IBorder *border;
		HKeySymbol borderSymbol;
		bool bSharedBorder;
	};
	CUtlVector<SchemeBorder_t> m_BorderList;
	IBorder  *m_pBaseBorder;	// default border to use if others not found
	KeyValues *m_pkvBorders;
#pragma pack(1)
	struct fontalias_t
	{
		CUtlSymbol _trueFontName;
		HFont _font : 15;
		unsigned short m_bProportional : 1;
	};
#pragma pack()
	friend struct fontalias_t;

	CUtlDict< fontalias_t, int >	m_FontAliases;
	VPANEL m_SizingPanel;
	int			m_nScreenWide;
	int			m_nScreenTall;

	struct fontrange_t
	{
		int _min;
		int _max;
	};
	CUtlDict< fontrange_t, int >	m_FontRanges;
};


//-----------------------------------------------------------------------------
// Purpose: Implementation of global scheme interface
//-----------------------------------------------------------------------------
class CSchemeManager : public ISchemeManager
{
public:
	CSchemeManager();
	~CSchemeManager();

	// loads a scheme from a file
	// first scheme loaded becomes the default scheme, and all subsequent loaded scheme are derivitives of that
	// tag is friendly string representing the name of the loaded scheme
	virtual HScheme LoadSchemeFromFile(const char *fileName, const char *tag);
	// first scheme loaded becomes the default scheme, and all subsequent loaded scheme are derivitives of that
	virtual HScheme LoadSchemeFromFileEx( VPANEL sizingPanel, const char *fileName, const char *tag);

	// reloads the schemes from the file
	virtual void ReloadSchemes();
	virtual void ReloadFonts();

	// returns a handle to the default (first loaded) scheme
	virtual HScheme GetDefaultScheme();

	// returns a handle to the scheme identified by "tag"
	virtual HScheme GetScheme(const char *tag);

	// returns a pointer to an image
	virtual IImage *GetImage(const char *imageName, bool hardwareFiltered);
	virtual HTexture GetImageID(const char *imageName, bool hardwareFiltered);

	virtual IScheme *GetIScheme( HScheme scheme );

	virtual void Shutdown( bool full );

	// gets the proportional coordinates for doing screen-size independant panel layouts
	// use these for font, image and panel size scaling (they all use the pixel height of the display for scaling)
	virtual int GetProportionalScaledValue(int normalizedValue);
	virtual int GetProportionalNormalizedValue(int scaledValue);

	// gets the proportional coordinates for doing screen-size independant panel layouts
	// use these for font, image and panel size scaling (they all use the pixel height of the display for scaling)
	virtual int GetProportionalScaledValueEx( HScheme scheme, int normalizedValue );
	virtual int GetProportionalNormalizedValueEx( HScheme scheme, int scaledValue );

	virtual bool DeleteImage( const char *pImageName );

	// gets the proportional coordinates for doing screen-size independant panel layouts
	// use these for font, image and panel size scaling (they all use the pixel height of the display for scaling)
	int GetProportionalScaledValueEx( CScheme *pScheme, int normalizedValue );
	int GetProportionalNormalizedValueEx( CScheme *pScheme, int scaledValue );

	void SpewFonts();

private:

	int GetProportionalScaledValue_( int rootWide, int rootTall, int normalizedValue );
	int GetProportionalNormalizedValue_( int rootWide, int rootTall, int scaledValue );

	// Search for already-loaded schemes
	HScheme FindLoadedScheme(const char *fileName);

	CUtlVector<CScheme *> m_Schemes;

	static const char *s_pszSearchString;
	struct CachedBitmapHandle_t
	{
		Bitmap *pBitmap;
	};
	static bool BitmapHandleSearchFunc(const CachedBitmapHandle_t &, const CachedBitmapHandle_t &);
	CUtlRBTree<CachedBitmapHandle_t, int> m_Bitmaps;
};

const char *CSchemeManager::s_pszSearchString = NULL;

//-----------------------------------------------------------------------------
// Purpose: search function for stored bitmaps
//-----------------------------------------------------------------------------
bool CSchemeManager::BitmapHandleSearchFunc(const CachedBitmapHandle_t &lhs, const CachedBitmapHandle_t &rhs)
{
	// a NULL bitmap indicates to use the search string instead
	if (lhs.pBitmap && rhs.pBitmap)
	{
		return stricmp(lhs.pBitmap->GetName(), rhs.pBitmap->GetName()) > 0;
	}
	else if (lhs.pBitmap)
	{
		return stricmp(lhs.pBitmap->GetName(), s_pszSearchString) > 0;
	}
	return stricmp(s_pszSearchString, rhs.pBitmap->GetName()) > 0;
}



CSchemeManager g_Scheme;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CSchemeManager, ISchemeManager, VGUI_SCHEME_INTERFACE_VERSION, g_Scheme);


namespace vgui
{
vgui::ISchemeManager *g_pScheme = &g_Scheme;
} // namespace vgui

CON_COMMAND( vgui_spew_fonts, "" )
{
	g_Scheme.SpewFonts();
}
 
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSchemeManager::CSchemeManager()
{
	// 0th element is null, since that would be an invalid handle
	CScheme *nullScheme = new CScheme();
	m_Schemes.AddToTail(nullScheme);
	m_Bitmaps.SetLessFunc(&BitmapHandleSearchFunc);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSchemeManager::~CSchemeManager()
{
	for ( auto *scheme : m_Schemes )
	{
		delete scheme;
	}
	m_Schemes.RemoveAll();

	for ( int i = 0; i < m_Bitmaps.MaxElement(); i++ )
	{
		if (m_Bitmaps.IsValidIndex(i))
		{
			delete m_Bitmaps[i].pBitmap;
		}
	}
	m_Bitmaps.RemoveAll();

	Shutdown( false );
}

//-----------------------------------------------------------------------------
// Purpose: Reload the fonts in all schemes
//-----------------------------------------------------------------------------
void CSchemeManager::ReloadFonts()
{
	for (intp i = 1; i < m_Schemes.Count(); i++)
	{
		m_Schemes[i]->ReloadFontGlyphs();
	}
}

//-----------------------------------------------------------------------------
// Converts the handle into an interface
//-----------------------------------------------------------------------------
IScheme *CSchemeManager::GetIScheme( HScheme scheme )
{
	if ( scheme < (uintp)m_Schemes.Count() )
	{
		return m_Schemes[scheme];
	}

	AssertMsg( false, "Invalid scheme requested." );
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSchemeManager::Shutdown( bool full )
{
	// Full shutdown kills the null scheme
	for( intp i = full ? 0 : 1; i < m_Schemes.Count(); i++ )
	{
		m_Schemes[i]->Shutdown( full );
	}

	if ( full )
	{
		m_Schemes.RemoveAll();
	}
}

//-----------------------------------------------------------------------------
// Purpose: loads a scheme from disk
//-----------------------------------------------------------------------------
HScheme CSchemeManager::FindLoadedScheme(const char *fileName)
{
	// Find the scheme in the list of already loaded schemes
	for (intp i = 1; i < m_Schemes.Count(); i++)
	{
		char const *schemeFileName = m_Schemes[i]->GetFileName();
		if (!stricmp(schemeFileName, fileName))
			return i;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CScheme::CScheme()
{
	fileName[ 0 ] = 0;
	tag[ 0 ] = 0;

	m_pData = NULL;
	m_pkvBaseSettings = NULL;
	m_pkvColors = NULL;
	m_nColorCount = 0;

	m_pBaseBorder = NULL;	// default border to use if others not found
	m_pkvBorders = NULL;
	m_SizingPanel = 0;
	m_nScreenWide = -1;
	m_nScreenTall = -1;
}

// first scheme loaded becomes the default scheme, and all subsequent loaded scheme are derivitives of that
HScheme  CSchemeManager::LoadSchemeFromFileEx( VPANEL sizingPanel, const char *fileName, const char *tag)
{
	// Look to see if we've already got this scheme...
	HScheme hScheme = FindLoadedScheme(fileName);
	if (hScheme != 0)
	{
		CScheme *pScheme = static_cast< CScheme * >( GetIScheme( hScheme ) );
		if ( pScheme )
		{
			pScheme->ReloadFontGlyphs();
		}
		return hScheme;
	}

	KeyValues *data = new KeyValues("Scheme");
	data->UsesEscapeSequences( true );	// VGUI uses this
	
	// Look first in game directory
	bool result = data->LoadFromFile( g_pFullFileSystem, fileName, "GAME" );
	if ( !result )
	{
		// look in any directory
		result = data->LoadFromFile( g_pFullFileSystem, fileName, NULL );
	}

	if (!result)
	{
		data->deleteThis();
		return 0;
	}
	
	ConVarRef cl_hud_minmode( "cl_hud_minmode", true );
	if ( cl_hud_minmode.IsValid() && cl_hud_minmode.GetBool() )
	{
		data->ProcessResolutionKeys( "_minmode" );
	}

	if ( g_pIVgui->GetVRMode() )
	{
		data->ProcessResolutionKeys( "_vrmode" );
	}

	CScheme *newScheme = new CScheme();
	newScheme->LoadFromFile( sizingPanel, fileName, tag, data );

	return m_Schemes.AddToTail(newScheme);
}

//-----------------------------------------------------------------------------
// Purpose: loads a scheme from disk
//-----------------------------------------------------------------------------
HScheme CSchemeManager::LoadSchemeFromFile(const char *fileName, const char *tag)
{
	return LoadSchemeFromFileEx( 0, fileName, tag );
}

//-----------------------------------------------------------------------------
// Purpose: Table of scheme file entries, translation from old goldsrc schemes to new src schemes
//-----------------------------------------------------------------------------
struct SchemeEntryTranslation_t
{
	const char *pchNewEntry;
	const char *pchOldEntry;
	const char *pchDefaultValue;
};
SchemeEntryTranslation_t g_SchemeTranslation[] =
{
	// dimhotepus: Fixed translation for BorderDark and BorderSelection.
    // the lit side of a control
	{ "Border.Bright",					"BorderBright",		"200 200 200 196" },
    // the dark/unlit side of a control
	{ "Border.Dark",					"BorderDark",		"40 40 40 196" },
    // the additional border color for displaying the default/selected button
	{ "Border.Selection",				"BorderSelection",	"0 0 0 196" },

	{ "Button.TextColor",				"ControlFG",		"White" },
	{ "Button.BgColor",					"ControlBG",		"Blank" },
	{ "Button.ArmedTextColor",			"ControlFG",		nullptr },
	{ "Button.ArmedBgColor",			"ControlBG",		nullptr },
	{ "Button.DepressedTextColor",		"ControlFG",		nullptr },
	{ "Button.DepressedBgColor",		"ControlBG",		nullptr },
	{ "Button.FocusBorderColor",		"0 0 0 255",		nullptr },

	{ "CheckButton.TextColor",			"BaseText",				nullptr },
	{ "CheckButton.SelectedTextColor",	"BrightControlText",	nullptr },
	{ "CheckButton.BgColor",			"CheckBgColor",			nullptr },
	{ "CheckButton.Border1",  			"CheckButtonBorder1",	nullptr },
	{ "CheckButton.Border2",  			"CheckButtonBorder2",	nullptr },
	{ "CheckButton.Check",				"CheckButtonCheck",		nullptr },

	{ "ComboBoxButton.ArrowColor",		"LabelDimText",					nullptr },
	{ "ComboBoxButton.ArmedArrowColor",	"MenuButton/ArmedArrowColor",	nullptr },
	{ "ComboBoxButton.BgColor",			"MenuButton/ButtonBgColor",		nullptr },
	{ "ComboBoxButton.DisabledBgColor",	"ControlBG",					nullptr },

	{ "Frame.TitleTextInsetX",			NULL,								"32" },
	{ "Frame.ClientInsetX",				NULL,								"8" },
	{ "Frame.ClientInsetY",				NULL,								"6" },
	{ "Frame.BgColor",					"BgColor",							nullptr },
	{ "Frame.OutOfFocusBgColor",		"BgColor",							nullptr },
	{ "Frame.FocusTransitionEffectTime",NULL,								"0" },
	{ "Frame.TransitionEffectTime",		NULL,								"0" },
	{ "Frame.AutoSnapRange",			NULL,								"8" },
	{ "FrameGrip.Color1",				"BorderBright",						nullptr },
	{ "FrameGrip.Color2",				"BorderSelection",					nullptr },
	{ "FrameTitleButton.FgColor",		"TitleButtonFgColor",				nullptr },
	{ "FrameTitleButton.BgColor",		"TitleButtonBgColor",				nullptr },
	{ "FrameTitleButton.DisabledFgColor",	"TitleButtonDisabledFgColor",	nullptr },
	{ "FrameTitleButton.DisabledBgColor",	"TitleButtonDisabledBgColor",	nullptr },
	{ "FrameSystemButton.FgColor",		"TitleBarBgColor",					nullptr },
	{ "FrameSystemButton.BgColor",		"TitleBarBgColor",					nullptr },
	{ "FrameSystemButton.Icon",			"TitleBarIcon",						nullptr },
	{ "FrameSystemButton.DisabledIcon",	"TitleBarDisabledIcon",				nullptr },
	{ "FrameTitleBar.Font",				NULL,								"Default" },
	{ "FrameTitleBar.TextColor",		"TitleBarFgColor",					nullptr },
	{ "FrameTitleBar.BgColor",			"TitleBarBgColor",					nullptr },
	{ "FrameTitleBar.DisabledTextColor","TitleBarDisabledFgColor",			nullptr },
	{ "FrameTitleBar.DisabledBgColor",	"TitleBarDisabledBgColor",			nullptr },

	{ "GraphPanel.FgColor",				"BrightControlText",				nullptr },
	{ "GraphPanel.BgColor",				"WindowBgColor",					nullptr },

	{ "Label.TextDullColor",			"LabelDimText",						nullptr },
	{ "Label.TextColor",				"BaseText",							nullptr },
	{ "Label.TextBrightColor",			"BrightControlText",				nullptr },
	{ "Label.SelectedTextColor",		"BrightControlText",				nullptr },
	{ "Label.BgColor",					"LabelBgColor",						nullptr },
	{ "Label.DisabledFgColor1",			"DisabledFgColor1",					nullptr },
	{ "Label.DisabledFgColor2",			"DisabledFgColor2",					nullptr },

	{ "ListPanel.TextColor",				"WindowFgColor",     nullptr },
	{ "ListPanel.TextBgColor",				"Menu/ArmedBgColor",     nullptr },
	{ "ListPanel.BgColor",					"ListBgColor",     nullptr },
	{ "ListPanel.SelectedTextColor",		"ListSelectionFgColor",     nullptr },
	{ "ListPanel.SelectedBgColor",			"Menu/ArmedBgColor",     nullptr },
	{ "ListPanel.SelectedOutOfFocusBgColor","SelectionBG2",     nullptr },
	{ "ListPanel.EmptyListInfoTextColor",	"LabelDimText",     nullptr },
	{ "ListPanel.DisabledTextColor",		"LabelDimText",     nullptr },
	{ "ListPanel.DisabledSelectedTextColor","ListBgColor",     nullptr },

	{ "Menu.TextColor",					"Menu/FgColor",     nullptr },
	{ "Menu.BgColor",					"Menu/BgColor",     nullptr },
	{ "Menu.ArmedTextColor",			"Menu/ArmedFgColor",     nullptr },
	{ "Menu.ArmedBgColor",				"Menu/ArmedBgColor",     nullptr },
	{ "Menu.TextInset",					NULL,		"6" },

	{ "Panel.FgColor",					"FgColor",     nullptr },
	{ "Panel.BgColor",					"BgColor",     nullptr },

	{ "ProgressBar.FgColor",				"BrightControlText",     nullptr },
	{ "ProgressBar.BgColor",				"WindowBgColor",     nullptr },

	{ "PropertySheet.TextColor",			"FgColorDim",     nullptr },
	{ "PropertySheet.SelectedTextColor",	"BrightControlText",     nullptr },
	{ "PropertySheet.TransitionEffectTime",	NULL,		"0" },

	{ "RadioButton.TextColor",			"FgColor",     nullptr },
	{ "RadioButton.SelectedTextColor",	"BrightControlText",     nullptr },

	{ "RichText.TextColor",				"WindowFgColor",     nullptr },
	{ "RichText.BgColor",				"WindowBgColor",     nullptr },
	{ "RichText.SelectedTextColor",		"SelectionFgColor",     nullptr },
	{ "RichText.SelectedBgColor",		"SelectionBgColor",     nullptr },

	{ "ScrollBar.Wide",					NULL,		"19" },

	{ "ScrollBarButton.FgColor",			"DimBaseText",     nullptr },
	{ "ScrollBarButton.BgColor",			"ControlBG",     nullptr },
	{ "ScrollBarButton.ArmedFgColor",		"BaseText",     nullptr },
	{ "ScrollBarButton.ArmedBgColor",		"ControlBG",     nullptr },
	{ "ScrollBarButton.DepressedFgColor",	"BaseText",     nullptr },
	{ "ScrollBarButton.DepressedBgColor",	"ControlBG",     nullptr },

	{ "ScrollBarSlider.FgColor",				"ScrollBarSlider/ScrollBarSliderFgColor",     nullptr },
	{ "ScrollBarSlider.BgColor",				"ScrollBarSlider/ScrollBarSliderBgColor",     nullptr },

	{ "SectionedListPanel.HeaderTextColor",	"SectionTextColor",     nullptr },
	{ "SectionedListPanel.HeaderBgColor",	"BuddyListBgColor",     nullptr },
	{ "SectionedListPanel.DividerColor",	"SectionDividerColor",     nullptr },
	{ "SectionedListPanel.TextColor",		"BuddyButton/FgColor1",     nullptr },
	{ "SectionedListPanel.BrightTextColor",	"BuddyButton/ArmedFgColor1",     nullptr },
	{ "SectionedListPanel.BgColor",			"BuddyListBgColor",     nullptr },
	{ "SectionedListPanel.SelectedTextColor",			"BuddyButton/ArmedFgColor1",     nullptr },
	{ "SectionedListPanel.SelectedBgColor",				"BuddyButton/ArmedBgColor",     nullptr },
	{ "SectionedListPanel.OutOfFocusSelectedTextColor",	"BuddyButton/ArmedFgColor2",     nullptr },
	{ "SectionedListPanel.OutOfFocusSelectedBgColor",	"SelectionBG2",     nullptr },

	{ "Slider.NobColor",			"SliderTickColor",     nullptr },
	{ "Slider.TextColor",			"Slider/SliderFgColor",     nullptr },
	{ "Slider.TrackColor",			"SliderTrackColor",     nullptr },
	{ "Slider.DisabledTextColor1",	"DisabledFgColor1",     nullptr },
	{ "Slider.DisabledTextColor2",	"DisabledFgColor2",     nullptr },

	{ "TextEntry.TextColor",		"WindowFgColor",     nullptr },
	{ "TextEntry.BgColor",			"WindowBgColor",     nullptr },
	{ "TextEntry.CursorColor",		"TextCursorColor",     nullptr },
	{ "TextEntry.DisabledTextColor","WindowDisabledFgColor",     nullptr },
	{ "TextEntry.DisabledBgColor",	"ControlBG",     nullptr },
	{ "TextEntry.SelectedTextColor","SelectionFgColor",     nullptr },
	{ "TextEntry.SelectedBgColor",	"SelectionBgColor",     nullptr },
	{ "TextEntry.OutOfFocusSelectedBgColor",	"SelectionBG2",     nullptr },
	{ "TextEntry.FocusEdgeColor",	"BorderSelection",              nullptr },

	{ "ToggleButton.SelectedTextColor",	"BrightControlText",     nullptr },

	{ "Tooltip.TextColor",			"BorderSelection",     nullptr },
	{ "Tooltip.BgColor",			"SelectionBG",         nullptr },

	{ "TreeView.BgColor",			"ListBgColor",         nullptr },

	{ "WizardSubPanel.BgColor",		"SubPanelBgColor",     nullptr },
};

//-----------------------------------------------------------------------------
// Purpose: loads a scheme from from disk into memory
//-----------------------------------------------------------------------------
void CScheme::LoadFromFile( VPANEL sizingPanel, const char *inFilename, const char *inTag, KeyValues *inKeys )
{
	COM_TimestampedLog( "CScheme::LoadFromFile( %s )", inFilename );

	Q_strncpy(fileName, inFilename, sizeof(fileName) );
	
	m_SizingPanel = sizingPanel;

	m_pData = inKeys;
	m_pkvBaseSettings = m_pData->FindKey("BaseSettings", true);
	m_pkvColors = m_pData->FindKey("Colors", true);

	// override the scheme name with the tag name
	KeyValues *name = m_pData->FindKey("Name", true);
	name->SetString("Name", inTag);

	if ( inTag )
	{
		Q_strncpy( tag, inTag, sizeof( tag ) );
	}
	else
	{
		AssertMsg( false, "You need to name the scheme!" );
		Q_strncpy( tag, "default", sizeof( tag ) );
	}

	// translate format from goldsrc scheme to new scheme
	for ( const auto &schemeTranslation : g_SchemeTranslation )
	{
		if (!m_pkvBaseSettings->FindKey(schemeTranslation.pchNewEntry, false))
		{
			const char *pchColor;

			if (schemeTranslation.pchOldEntry)
			{
				pchColor = LookupSchemeSetting(schemeTranslation.pchOldEntry);
			}
			else
			{
				pchColor = schemeTranslation.pchDefaultValue;
			}

			Assert( pchColor );

			m_pkvBaseSettings->SetString(schemeTranslation.pchNewEntry, pchColor);
		}
	}

	// need to copy tag before loading fonts
	LoadFonts();
	LoadBorders();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CScheme::GetFontRange( const char *fontname, int &nMin, int &nMax )
{
	auto i = m_FontRanges.Find( fontname );
	if ( i != m_FontRanges.InvalidIndex() )
	{
		nMin = m_FontRanges[i]._min;
		nMax = m_FontRanges[i]._max;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CScheme::SetFontRange( const char *fontname, int nMin, int nMax )
{
	auto i = m_FontRanges.Find( fontname );
	if ( i != m_FontRanges.InvalidIndex() )
	{
		m_FontRanges[i]._min = nMin;
		m_FontRanges[i]._max = nMax;
		return;
	}

	// not already in our list
	auto iNew = m_FontRanges.Insert( fontname );
	
	m_FontRanges[iNew]._min = nMin;
	m_FontRanges[iNew]._max = nMax;
}

//-----------------------------------------------------------------------------
// Purpose: adds all the font specifications to the surface
//-----------------------------------------------------------------------------
void CScheme::LoadFonts()
{
	char language[64];
	memset( language, 0, sizeof( language ) );

	// get our language
	bool bValid = vgui::g_pSystem->GetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Source\\Language", language, sizeof( language ) - 1 );
	if ( !bValid )
	{
		Q_strncpy( language, "english", sizeof( language ) );
	}

	// add our custom fonts
	for (KeyValues *kv = m_pData->FindKey("CustomFontFiles", true)->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey())
	{
		const char *fontFile = kv->GetString();
		if (fontFile && *fontFile)
		{
			g_pSurface->AddCustomFontFile( NULL, fontFile );
		}
		else
		{
			// we have a block to read
			int nRangeMin = 0, nRangeMax = 0;
			const char *pszName = NULL;
			bool bUseRange = false;

			for ( KeyValues *pData = kv->GetFirstSubKey(); pData != NULL; pData = pData->GetNextKey() )
			{
				const char *pszKey = pData->GetName();
				if ( !Q_stricmp( pszKey, "font" ) )
				{
					fontFile = pData->GetString();
				}
				else if ( !Q_stricmp( pszKey, "name" ) )
				{
					pszName = pData->GetString();
				}
				else
				{
					// we must have a language
					if ( Q_stricmp( language, pszKey ) == 0 ) // matches the language we're running?
					{
						// get the range
						KeyValues *pRange = pData->FindKey( "range" );
						if ( pRange )
						{
							bUseRange = true;
							// dimhotepus: Check font range is valid before swap.
							if ( sscanf( pRange->GetString(), "%x %x", &nRangeMin, &nRangeMax ) == 2 &&
								 nRangeMin > nRangeMax )
							{
								std::swap(nRangeMin, nRangeMax);
							}
						}
					}
				}
			}

			if ( fontFile && *fontFile )
			{
				g_pSurface->AddCustomFontFile( pszName, fontFile );

				if ( bUseRange )
				{
					SetFontRange( pszName, nRangeMin, nRangeMax );
				}
			}
		}
	}

	// add bitmap fonts
	for (KeyValues *kv = m_pData->FindKey("BitmapFontFiles", true)->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey())
	{
		const char *fontFile = kv->GetString();
		if (fontFile && *fontFile)
		{
			bool bSuccess = g_pSurface->AddBitmapFontFile( fontFile );
			if ( bSuccess )
			{
				// refer to the font by a user specified symbol
				g_pSurface->SetBitmapFontName( kv->GetName(), fontFile );
			}
		}
	}

	// create the fonts
	for (KeyValues *kv = m_pData->FindKey("Fonts", true)->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey())
	{
		for ( int i = 0; i < 2; i++ )
		{
			// create the base font
			bool proportionalFont = static_cast<bool>( i );
			const char *fontName = GetMungedFontName( kv->GetName(), tag, proportionalFont ); // first time it adds a normal font, and then a proportional one
			HFont font = g_pSurface->CreateFont();

			auto j = m_FontAliases.Insert( fontName );
			m_FontAliases[j]._trueFontName = kv->GetName();
			m_FontAliases[j]._font = font;
			m_FontAliases[j].m_bProportional = proportionalFont;
		}
	}

	// load in the font glyphs
	ReloadFontGlyphs();
}

//-----------------------------------------------------------------------------
// Purpose: Reloads all scheme fonts
//-----------------------------------------------------------------------------
void CScheme::ReloadFontGlyphs()
{
	COM_TimestampedLog( "ReloadFontGlyphs(): Start" );

	// get our current resolution
	if ( m_SizingPanel != 0 )
	{
		g_pIPanel->GetSize( m_SizingPanel, m_nScreenWide, m_nScreenTall );
	}
	else
	{
		g_pSurface->GetScreenSize( m_nScreenWide, m_nScreenTall );
	}

	// check our language; some have minimum sizes
	int minimumFontHeight = GetMinimumFontHeightForCurrentLanguage();

	// add the data to all the fonts
	KeyValues *fonts = m_pData->FindKey("Fonts", true);
	FOR_EACH_DICT_FAST( m_FontAliases, i )
	{
		KeyValues *kv = fonts->FindKey( m_FontAliases[i]._trueFontName.String(), true );
	
		// walk through creating adding the first matching glyph set to the font
		for (KeyValues *fontdata = kv->GetFirstSubKey(); fontdata != NULL; fontdata = fontdata->GetNextKey())
		{
			// skip over fonts not meant for this resolution
			int fontYResMin = 0, fontYResMax = 0;
			sscanf(fontdata->GetString("yres", ""), "%d %d", &fontYResMin, &fontYResMax);
			if (fontYResMin)
			{
				if (!fontYResMax)
				{
					fontYResMax = fontYResMin;
				}
				// check the range
				if (m_nScreenTall < fontYResMin || m_nScreenTall > fontYResMax)
					continue;
			}

			int flags = 0;
			if (fontdata->GetInt( "italic" ))
			{
				flags |= ISurface::FONTFLAG_ITALIC;
			}
			if (fontdata->GetInt( "underline" ))
			{
				flags |= ISurface::FONTFLAG_UNDERLINE;
			}
			if (fontdata->GetInt( "strikeout" ))
			{
				flags |= ISurface::FONTFLAG_STRIKEOUT;
			}
			if (fontdata->GetInt( "symbol" ))
			{
				flags |= ISurface::FONTFLAG_SYMBOL;
			}
			if (fontdata->GetInt( "antialias" ) && g_pSurface->SupportsFeature(ISurface::ANTIALIASED_FONTS))
			{
				flags |= ISurface::FONTFLAG_ANTIALIAS;
			}
			if (fontdata->GetInt( "cleartype" ) && g_pSurface->SupportsFeature(ISurface::CLEARTYPE_FONTS))
			{
				flags |= ISurface::FONTFLAG_CLEARTYPE;
			}
			if (fontdata->GetInt( "cleartype_natural" ) && g_pSurface->SupportsFeature(ISurface::CLEARTYPE_FONTS))
			{
				flags |= ISurface::FONTFLAG_CLEARTYPE_NATURAL;
			}
			if (fontdata->GetInt( "dropshadow" ) && g_pSurface->SupportsFeature(ISurface::DROPSHADOW_FONTS))
			{
				flags |= ISurface::FONTFLAG_DROPSHADOW;
			}
			if (fontdata->GetInt( "outline" ) && g_pSurface->SupportsFeature(ISurface::OUTLINE_FONTS))
			{
				flags |= ISurface::FONTFLAG_OUTLINE;
			}
			if (fontdata->GetInt( "custom" ))
			{
				flags |= ISurface::FONTFLAG_CUSTOM;
			}
			if (fontdata->GetInt( "bitmap" ))
			{
				flags |= ISurface::FONTFLAG_BITMAP;
			}
			if (fontdata->GetInt( "rotary" ))
			{
				flags |= ISurface::FONTFLAG_ROTARY;
			}
			if (fontdata->GetInt( "additive" ))
			{
				flags |= ISurface::FONTFLAG_ADDITIVE;
			}

			int tall = fontdata->GetInt( "tall" );
			int blur = fontdata->GetInt( "blur" );
			int scanlines = fontdata->GetInt( "scanlines" );
			float scalex = fontdata->GetFloat( "scalex", 1.0f );
			float scaley = fontdata->GetFloat( "scaley", 1.0f );

			// only grow this font if it doesn't have a resolution filter specified
			if ( ( !fontYResMin && !fontYResMax ) && m_FontAliases[i].m_bProportional )
			{
				tall = g_Scheme.GetProportionalScaledValueEx( this, tall );
				blur = g_Scheme.GetProportionalScaledValueEx( this, blur );
				scanlines = g_Scheme.GetProportionalScaledValueEx( this, scanlines ); 
				scalex = g_Scheme.GetProportionalScaledValueEx( this, scalex * 10000.0f ) * 0.0001f;
				scaley = g_Scheme.GetProportionalScaledValueEx( this, scaley * 10000.0f ) * 0.0001f;
			}

			// clip the font size so that fonts can't be too big
			if ( tall > 127 )
			{
				tall = 127;
			}

			// check our minimum font height
			if ( tall < minimumFontHeight )
			{
				tall = minimumFontHeight;
			}
			
			if ( flags & ISurface::FONTFLAG_BITMAP )
			{
				// add the new set
				g_pSurface->SetBitmapFontGlyphSet(
					m_FontAliases[i]._font,
					g_pSurface->GetBitmapFontName( fontdata->GetString( "name" ) ), 
					scalex,
					scaley,
					flags);
			}
			else
			{
				int nRangeMin, nRangeMax;

				if ( GetFontRange( fontdata->GetString( "name" ), nRangeMin, nRangeMax ) )
				{
					// add the new set
					g_pSurface->SetFontGlyphSet(
						m_FontAliases[i]._font,
						fontdata->GetString( "name" ), 
						tall, 
						fontdata->GetInt( "weight" ), 
						blur,
						scanlines,
						flags,
						nRangeMin,
						nRangeMax);					
				}
				else
				{
					// add the new set
					g_pSurface->SetFontGlyphSet(
						m_FontAliases[i]._font,
						fontdata->GetString( "name" ), 
						tall, 
						fontdata->GetInt( "weight" ), 
						blur,
						scanlines,
						flags);
				}
			}

			// don't add any more
			break;
		}
	}

	COM_TimestampedLog( "ReloadFontGlyphs(): End" );
}

//-----------------------------------------------------------------------------
// Purpose: create the Border objects from the scheme data
//-----------------------------------------------------------------------------
void CScheme::LoadBorders()
{
	m_pkvBorders = m_pData->FindKey("Borders", true);
	{for ( KeyValues *kv = m_pkvBorders->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey())
	{
		if (kv->GetDataType() == KeyValues::TYPE_STRING)
		{
			// it's referencing another border, ignore for now
		}
		else
		{
			auto i = m_BorderList.AddToTail();

			IBorder *border = NULL;
			const char *pszBorderType = kv->GetString( "bordertype", NULL );
			if ( pszBorderType && pszBorderType[0] )
			{
				if ( !stricmp(pszBorderType,"image") )
				{
					border = new ImageBorder();
				}
				else if ( !stricmp(pszBorderType,"scalable_image") )
				{
					border = new ScalableImageBorder();
				}
				else
				{
					Assert(0);
					// Fall back to the base border type. See below.
					pszBorderType = NULL;
				}
			}

			if ( !pszBorderType || !pszBorderType[0] )
			{
				border = new Border();
			}

			border->SetName(kv->GetName());
			border->ApplySchemeSettings(this, kv);

			m_BorderList[i].border = border;
			m_BorderList[i].bSharedBorder = false;
			m_BorderList[i].borderSymbol = kv->GetNameSymbol();
		}
	}}

	// iterate again to get the border references
	for ( KeyValues *kv = m_pkvBorders->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey())
	{
		if (kv->GetDataType() == KeyValues::TYPE_STRING)
		{
			// it's referencing another border, find it
			Border *border = (Border *)GetBorder(kv->GetString());
//			const char *str = kv->GetName();
			Assert(border);

			// add an entry that just references the existing border
			auto &vborder = m_BorderList[m_BorderList.AddToTail()];
			vborder.border = border;
			vborder.bSharedBorder = true;
			vborder.borderSymbol = kv->GetNameSymbol();
		}
	}
	
	m_pBaseBorder = GetBorder("BaseBorder");
}

void CScheme::SpewFonts( void )
{
	Msg( "Scheme: %s (%s)\n", GetName(), GetFileName() );
	FOR_EACH_DICT_FAST( m_FontAliases, i )
	{
		const fontalias_t& FontAlias = m_FontAliases[ i ];
		HFont Font = FontAlias._font;
		const char *szFontName = g_pSurface->GetFontName( Font );
		const char *szFontFamilyName = g_pSurface->GetFontFamilyName( Font );
		const char *szTrueFontName = FontAlias._trueFontName.String();
		const char *szFontAlias = m_FontAliases.GetElementName( i );

		Msg( "  %2d: HFont:0x%8.8x, %s, %s, font:%s, tall:%d(%d). %s\n", 
			i, 
			Font,
			szTrueFontName ? szTrueFontName : "??", 
			szFontAlias ? szFontAlias : "??",
			szFontName ? szFontName : "??", 
			g_pSurface->GetFontTall( Font ),
			g_pSurface->GetFontTallRequested( Font ),
			szFontFamilyName ? szFontFamilyName : "" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: reloads the scheme from the file
//-----------------------------------------------------------------------------
void CSchemeManager::ReloadSchemes()
{
	intp count = m_Schemes.Count();
	Shutdown( false );
	
	// reload the scheme
	for (intp i = 1; i < count; i++)
	{
		LoadSchemeFromFile(m_Schemes[i]->GetFileName(), m_Schemes[i]->GetName());
	}
}

//-----------------------------------------------------------------------------
// Purpose: kills all the schemes
//-----------------------------------------------------------------------------
void CScheme::Shutdown( bool full )
{
	for ( auto &b : m_BorderList )
	{
		// delete if it's not shared
		if (!b.bSharedBorder)
		{
			IBorder *border = b.border;
			delete border;
		}
	}

	m_pBaseBorder = NULL;
	m_BorderList.RemoveAll();
	m_pkvBorders = NULL;

 	if (full && m_pData)
	{
		m_pData->deleteThis();
		m_pData = NULL;
		delete this;
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns a handle to the default (first loaded) scheme
//-----------------------------------------------------------------------------
HScheme CSchemeManager::GetDefaultScheme()
{
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: returns a handle to the scheme identified by "tag"
//-----------------------------------------------------------------------------
HScheme CSchemeManager::GetScheme(const char *tag)
{
	for (intp i=1;i<m_Schemes.Count();i++)
	{
		if ( !stricmp(tag,m_Schemes[i]->GetName()) )
		{
			return i;
		}
	}
	return 1; // default scheme
}

int CSchemeManager::GetProportionalScaledValue_( int, int rootTall, int normalizedValue )
{
	int proH, proW;
	g_pSurface->GetProportionalBase( proW, proH );
	float scale = (float)rootTall / (float)proH;

	return (int)( normalizedValue * scale );
}

int CSchemeManager::GetProportionalNormalizedValue_( int, int rootTall, int scaledValue )
{
	int proH, proW;
	g_pSurface->GetProportionalBase( proW, proH );
	float scale = (float)rootTall / (float)proH;

	return (int)( scaledValue / scale );
}

//-----------------------------------------------------------------------------
// Purpose: converts a value into proportional mode
//-----------------------------------------------------------------------------
int CSchemeManager::GetProportionalScaledValue(int normalizedValue)
{
	int wide, tall;
	g_pSurface->GetScreenSize( wide, tall );
	return GetProportionalScaledValue_( wide, tall, normalizedValue );
}

//-----------------------------------------------------------------------------
// Purpose: converts a value out of proportional mode
//-----------------------------------------------------------------------------
int CSchemeManager::GetProportionalNormalizedValue(int scaledValue)
{
	int wide, tall;
	g_pSurface->GetScreenSize( wide, tall );
	return GetProportionalNormalizedValue_( wide, tall, scaledValue );
}

// gets the proportional coordinates for doing screen-size independant panel layouts
// use these for font, image and panel size scaling (they all use the pixel height of the display for scaling)
int CSchemeManager::GetProportionalScaledValueEx( CScheme *pScheme, int normalizedValue )
{
	VPANEL sizing = pScheme->GetSizingPanel();
	if ( !sizing )
	{
		return GetProportionalScaledValue( normalizedValue );
	}

	int w, h;
	g_pIPanel->GetSize( sizing, w, h );
	return GetProportionalScaledValue_( w, h, normalizedValue );
}

int CSchemeManager::GetProportionalNormalizedValueEx( CScheme *pScheme, int scaledValue )
{
	VPANEL sizing = pScheme->GetSizingPanel();
	if ( !sizing )
	{
		return GetProportionalNormalizedValue( scaledValue );
	}

	int w, h;
	g_pIPanel->GetSize( sizing, w, h );
	return GetProportionalNormalizedValue_( w, h, scaledValue );
}

int CSchemeManager::GetProportionalScaledValueEx( HScheme scheme, int normalizedValue )
{
	IScheme *pscheme = GetIScheme( scheme );
	if ( !pscheme )
	{
		Assert( 0 );
		return GetProportionalScaledValue( normalizedValue );
	}

	CScheme *p = static_cast< CScheme * >( pscheme );
	return GetProportionalScaledValueEx( p, normalizedValue );
}

int CSchemeManager::GetProportionalNormalizedValueEx( HScheme scheme, int scaledValue )
{
	IScheme *pscheme = GetIScheme( scheme );
	if ( !pscheme )
	{
		Assert( 0 );
		return GetProportionalNormalizedValue( scaledValue );
	}

	CScheme *p = static_cast< CScheme * >( pscheme );
	return GetProportionalNormalizedValueEx( p, scaledValue );
}

void CSchemeManager::SpewFonts( void )
{
	for ( auto *s : m_Schemes )
	{
		s->SpewFonts();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CScheme::GetResourceString(const char *stringName)
{
	return m_pkvBaseSettings->GetString(stringName);
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to an image
//-----------------------------------------------------------------------------
IImage *CSchemeManager::GetImage(const char *imageName, bool hardwareFiltered)
{
	if ( !imageName || Q_isempty(imageName) ) // frame icons and the like are in the scheme file and may not be defined, so if this is null then fail silently
	{
		return NULL; 
	}

	// set up to search for the bitmap
	CachedBitmapHandle_t searchBitmap;
	searchBitmap.pBitmap = NULL;

	// Prepend 'vgui/'. Resource files try to load images assuming they live in the vgui directory.
	// Used to do this in Bitmap::Bitmap, moved so that the s_pszSearchString is searching for the
	// filename with 'vgui/' already added.
	char szFileName[MAX_PATH];

	if ( Q_stristr( imageName, ".pic" ) )
	{
		Q_snprintf( szFileName, sizeof(szFileName), "%s", imageName );
	}
	else
	{
		Q_snprintf( szFileName, sizeof(szFileName), "vgui/%s", imageName );
	}

	s_pszSearchString = szFileName;
	auto i = m_Bitmaps.Find( searchBitmap );
	if (m_Bitmaps.IsValidIndex( i ) )
	{
		return m_Bitmaps[i].pBitmap;
	}

	// couldn't find the image, try and load it
	CachedBitmapHandle_t hBitmap = { new Bitmap( szFileName, hardwareFiltered ) };
	m_Bitmaps.Insert( hBitmap );
	return hBitmap.pBitmap;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
HTexture CSchemeManager::GetImageID(const char *imageName, bool hardwareFiltered)
{
	IImage *img = GetImage(imageName, hardwareFiltered);
	return ((Bitmap *)img)->GetID();
}

//-----------------------------------------------------------------------------
// Delete a managed image
//-----------------------------------------------------------------------------
bool CSchemeManager::DeleteImage( const char *pImageName )
{
	if ( !pImageName )
	{
		// nothing to do
		return false;
	}

	// set up to search for the bitmap
	CachedBitmapHandle_t searchBitmap;
	searchBitmap.pBitmap = NULL;

	// Prepend 'vgui/'. Resource files try to load images assuming they live in the vgui directory.
	// Used to do this in Bitmap::Bitmap, moved so that the s_pszSearchString is searching for the
	// filename with 'vgui/' already added.
	char szFileName[256];
	if ( Q_stristr( pImageName, ".pic" ) )
	{
		Q_snprintf( szFileName, sizeof(szFileName), "%s", pImageName );
	}
	else
	{
		Q_snprintf( szFileName, sizeof(szFileName), "vgui/%s", pImageName );
	}
	s_pszSearchString = szFileName;

	auto i = m_Bitmaps.Find( searchBitmap );
	if ( !m_Bitmaps.IsValidIndex( i ) )
	{
		// not found
		return false;
	}
		
	// no way to know if eviction occured, assume it does
	m_Bitmaps[i].pBitmap->Evict();
	delete  m_Bitmaps[i].pBitmap;	
	m_Bitmaps.RemoveAt( i );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to an existing border
//-----------------------------------------------------------------------------
IBorder *CScheme::GetBorder(const char *borderName)
{
	auto symbol = KeyValuesSystem()->GetSymbolForString(borderName);
	for ( auto &b : m_BorderList )
	{
		if (b.borderSymbol == symbol)
		{
			return b.border;
		}
	}

	return m_pBaseBorder;
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of borders
//-----------------------------------------------------------------------------
int CScheme::GetBorderCount() const
{
	return m_BorderList.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Get the border at the given index
//-----------------------------------------------------------------------------
IBorder *CScheme::GetBorderAtIndex( int iIndex )
{
	if ( !m_BorderList.IsValidIndex( iIndex ) )
		return NULL;

	return m_BorderList[ iIndex ].border;
}

//-----------------------------------------------------------------------------
// Finds a font in the alias list
//-----------------------------------------------------------------------------
HFont CScheme::FindFontInAliasList( const char *fontName )
{
	auto i = m_FontAliases.Find( fontName );
	if ( i != m_FontAliases.InvalidIndex() )
	{
		return m_FontAliases[i]._font;
	}

	// No dice
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : font - 
// Output : char const
//-----------------------------------------------------------------------------
char const *CScheme::GetFontName( const HFont& font )
{
	for (auto i = m_FontAliases.Count(); --i >= 0; )
	{
		HFont fnt = m_FontAliases[i]._font;
		if ( fnt == font )
			return m_FontAliases[i]._trueFontName.String();
	}

	return "<Unknown font>";
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to an existing font, proportional=false means use default
//-----------------------------------------------------------------------------
HFont CScheme::GetFont( const char *fontName, bool proportional )
{
	// First look in the list of aliases...
	return FindFontInAliasList( GetMungedFontName( fontName, tag, proportional ) );
}

//-----------------------------------------------------------------------------
// Purpose:Get the number of fonts
//-----------------------------------------------------------------------------
int CScheme::GetFontCount() const
{
	return m_FontAliases.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Get the font at the given index
//-----------------------------------------------------------------------------
HFont CScheme::GetFontAtIndex( int iIndex )
{
	if ( !m_FontAliases.IsValidIndex( iIndex ) )
		return INVALID_FONT;
	
	return m_FontAliases[ iIndex ]._font;
}

//-----------------------------------------------------------------------------
// Purpose: returns a char string of the munged name this font is stored as in the font manager
//-----------------------------------------------------------------------------
const char *CScheme::GetMungedFontName( const char *fontName, const char *scheme, bool proportional )
{
	static char mungeBuffer[ 64 ];
	if ( scheme )
	{
		Q_snprintf( mungeBuffer, sizeof( mungeBuffer ), "%s%s-%s", fontName, scheme, proportional ? "p" : "no" );
	}
	else
	{ 
		Q_snprintf( mungeBuffer, sizeof( mungeBuffer ), "%s-%s", fontName, proportional ? "p" : "no" ); // we don't want the "(null)" snprintf appends
	}
	return mungeBuffer;
}

//-----------------------------------------------------------------------------
// Purpose: Gets a color from the scheme file
//-----------------------------------------------------------------------------
Color CScheme::GetColor(const char *colorName, Color defaultColor)
{
	const char *pchT = LookupSchemeSetting(colorName);
	if (!pchT)
		return defaultColor;

	int r = 0, g = 0, b = 0, a = 0;
	if (sscanf(pchT, "%d %d %d %d", &r, &g, &b, &a) >= 3)
		return Color(r, g, b, a);

	return defaultColor;
}

//-----------------------------------------------------------------------------
// Purpose: Get the color at the given index
//-----------------------------------------------------------------------------
const KeyValues *CScheme::GetColorData() const
{
	return m_pkvColors;
}

//-----------------------------------------------------------------------------
// Purpose: recursively looks up a setting
//-----------------------------------------------------------------------------
const char *CScheme::LookupSchemeSetting(const char *pchSetting)
{
	// try parse out the color
	int r, g, b, a = 0;
	int res = sscanf(pchSetting, "%d %d %d %d", &r, &g, &b, &a);
	if (res >= 3)
	{
		return pchSetting;
	}

	// check the color area first
	const char *colStr = m_pkvColors->GetString(pchSetting, NULL);
	if (colStr)
		return colStr;

	// check base settings
	colStr = m_pkvBaseSettings->GetString(pchSetting, NULL);
	if (colStr)
	{
		return LookupSchemeSetting(colStr);
	}

	return pchSetting;
}

//-----------------------------------------------------------------------------
// Purpose: gets the minimum font height for the current language
//-----------------------------------------------------------------------------
int CScheme::GetMinimumFontHeightForCurrentLanguage()
{
	char language[64];
	bool bValid = vgui::g_pSystem->GetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Source\\Language", language, sizeof(language)-1 );
	if ( bValid )
	{
		if (!stricmp(language, "korean")
			|| !stricmp(language, "tchinese")
			|| !stricmp(language, "schinese")
			|| !stricmp(language, "japanese"))
		{
			// the bitmap-based fonts for these languages simply don't work with a pt. size of less than 9 (13 pixels)
			return 13;
		}

		if ( !stricmp(language, "thai" ) )
		{
			// thai has problems below 18 pts
			return 18;
		}
	}

	// no special minimum height required
	return 0;
}
