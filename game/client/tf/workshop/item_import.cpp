//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
//
// TODO:
// * Bring in Jeff Hameluck's translucency support:
//       QC with any $translucent 1 VMT should have $mostlyopaque
// * Add help icons with tooltips on valid asset constraints
//
// Maybe TODO:
// * Add wireframe view to preview
// * Fix issues with gesture support (no animation blending/easing, no weapon state logic)
//
//=============================================================================

#include "cbase.h"

#include "tier2/tier2.h"
#include "tier2/p4helpers.h"
#include "tier2/fileutils.h"
#include "ienginevgui.h"
#include "imageutils.h"
#include "filesystem.h"
#include "vgui_controls/FileOpenDialog.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Label.h"
#include "vgui_bitmapimage.h"
#include "bitmap/bitmap.h"
#include "itemtest/itemtest.h"

#include "econ_controls.h"
#include "econ_item_system.h"
#include "tf_item_inventory.h"
#include "c_tf_player.h"
#include "confirm_dialog.h"
#include "steampublishedfiles/publish_file_dialog.h"
#include "vgui/store/v2/tf_store_preview_item2.h"
#include "vgui/tf_playermodelpanel.h"
#include "bone_setup.h"

#include "scenefilecache/ISceneFileCache.h"

#include "workshop/item_import.h"

#include "choreoscene.h"
#include "choreoactor.h"
#include "choreochannel.h"

#include "../public/zip_utils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


static const char *kClassFolders[TF_LAST_NORMAL_CLASS] = {
	"",			// TF_CLASS_UNDEFINED
	"scout",	// TF_CLASS_SCOUT
	"sniper",	// TF_CLASS_SNIPER,
	"soldier",	// TF_CLASS_SOLDIER,
	"demo",		// TF_CLASS_DEMOMAN,
	"medic",	// TF_CLASS_MEDIC,
	"heavy",	//TF_CLASS_HEAVYWEAPONS,
	"pyro",		// TF_CLASS_PYRO,
	"spy",		// TF_CLASS_SPY,
	"engineer",	// TF_CLASS_ENGINEER,
};
static const char *kClassFolderMulticlass = "all_class";

static const int NUM_IMPORT_LODS = 3;
static const int NUM_IMPORT_MATERIALS_PER_TEAM = 2;
static const int MAX_MATERIAL_COUNT = NUM_IMPORT_MATERIALS_PER_TEAM * 2;
static const int MAX_TEXT_EDIT_SIZE = 8192;
static const int MAX_TAUNT_DURATION = 30.f;

static const int kStartWorkshopItemIndex = 30000;

static const char *kBodygroupArray[] =
{
	"backpack",
	"dogtags",
	"grenades",
	"hands",
	"hat",
	"head",
	"headphones",
	"shoes",
	"shoes_socks",
};

extern IFileSystem *g_pFullFileSystem;

// This should be a constant since it's in the game directory hierarchy now
//static ConVar tf_steam_workshop_dir( "tf_steam_workshop_dir", "workshop", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Directory used to hold import sessions for the Steam Workshop." );
static const char *kSteamWorkshopDir = "%s/import_source";
static const char *kWorkshopSchemaFile = "scripts/items/unencrypted/_items_workshop.txt";
static const char *kWorkshopSoundScriptFile = "scripts/game_sounds_taunt_workshop.txt";

static ConVar tf_steam_workshop_import_icon_path( "tf_steam_workshop_import_icon_path", "", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Default location to load backpack icons from" );
static ConVar tf_steam_workshop_import_model_path( "tf_steam_workshop_import_model_path", "", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Default location to load models from" );
static ConVar tf_steam_workshop_import_material_path( "tf_steam_workshop_import_material_path", "", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Default location to load materials from" );

#ifdef STAGING_ONLY
static ConVar tf_steam_workshop_max_taunt_duration( "tf_steam_workshop_max_taunt_duration", "-1" );
#endif // STAGING_ONLY

// Item data tokens
static const char *kItemName = "name";
static const char *kWorkshopName = "import_name";
static const char *kTFEnglishName = "tf_english_name";
static const char *kItemPrefab = "prefab";
static const char *kItemIcon = "icon";
static const char *kItemPaintable = "paintable%d";
static const char *kAnimationLoopable = "loopable";
static const char *kClassQC = "Classes/%s/QC";
static const char *kClassQCI = "Classes/%s/QCI";
static const char *kClassLODN = "Classes/%s/LOD%d";
static const char *kClassLODNFile = "Classes/%s/LOD%d/file";
static const char *kClassAnimation = "Classes/%s/Animation";
static const char *kClassAnimationSourceFile = "Classes/%s/Animation/source_file";
static const char *kClassAnimationVCDFile = "Classes/%s/Animation/vcd_file";
static const char *kClassAnimationDuration = "Classes/%s/Animation/taunt_duration";
static const char *kMaterialN = "Materials/Material%d";
static const char *kMaterialSkinN = "Materials/Skins/%s/Material%d";
static const char *kMaterialSkinNFile = "Materials/Skins/%s/Material%d/%s_texture_file";
static const char *kMaterialSkinNVMT = "Materials/Skins/%s/Material%d/vmt";
static const char *kItemSchema = "ItemSchema";
static const char *kBuildOutput = "output";
static const char *kSkinType = "skin_type";
static const char *kEquipRegion = "equip_region";
static const char *kBodygroup = "bodygroup";
static const char *kPaintDefIndex = "paint_def_index";
static const char *kWorkshopID = "workshop_id";

// The default triangle budget for items
static const int kDefaultTriangleBudget[] = { 1400, 1000, 700 };
COMPILE_TIME_ASSERT( SE_ARRAYSIZE( kDefaultTriangleBudget ) == NUM_IMPORT_LODS );

static const int kDefaultBoneBudget = 5;

static const char *kBuildResultMessages[] = {
	"",
	"#TF_ImportFile_BuildFailedNoSDK",
	"#TF_ImportFile_BuildFailedNoName",
	"#TF_ImportFile_BuildFailedNoType",
	"#TF_ImportFile_BuildFailedNoModels",
	"#TF_ImportFile_BuildFailedNumLODMismatch",
	"#TF_ImportFile_BuildFailedNoMaterials",
	"#TF_ImportFile_BuildFailedNumMaterialMismatch",
	"#TF_ImportFile_BuildFailedNoBackpackIcon",
	"#TF_ImportFile_BuildFailedBadName",
	"TF English Name already exists in item schema.",
	"#TF_ImportFile_BuildFailedBadType",
	"#TF_ImportFile_BuildFailedBadModel",
	"#TF_ImportFile_BuildFailedBadMaterialType",
	"#TF_ImportFile_BuildFailedBadMaterial",
	"#TF_ImportFile_BuildFailedMaterialMissingShader",
	"#TF_ImportFile_BuildFailedMaterialMissingCloak",
	"#TF_ImportFile_BuildFailedMaterialMissingBurning",
	"#TF_ImportFile_BuildFailedMaterialMissingJarate",
	"#TF_ImportFile_BuildFailedMaterialMissingPaintable",
	"#TF_ImportFile_BuildFailedMissingModel",
	"#TF_ImportFile_BuildFailedNeedMoreLOD",
	"#TF_ImportFile_BuildFailedComplexModel",
	"#TF_ImportFile_BuildFailedBadImage",
	"#TF_ImportFile_BuildFailedCompile",
	"#TF_ImportFile_BuildFailedNoWorkshopID",
	"#TF_ImportFile_ImageUnsupportedFileType",
	"#TF_ImportFile_ImageResolutionNotPowerOf2",
	"#TF_ImportFile_ImageResolutionOverLimit",
	"#TF_ImportFile_BuildFailedNoTaunts",
	"#TF_ImportFile_BuildFailedBadVCD",
	"#TF_ImportFile_BuildFailedVCDMissingEventSequence",
	"#TF_ImportFile_BuildFailedVCDEventSequenceTooLong",
};
COMPILE_TIME_ASSERT( SE_ARRAYSIZE( kBuildResultMessages ) == CTFFileImportDialog::NUM_BUILD_RESULTS );

static const char *kLoadResultMessages[ CTFFileImportDialog::NUM_LOAD_RESULTS ] = {
	"",
	"#TF_ImportFile_LoadFailedBadFile",
	"#TF_ImportFile_LoadFailedBadFile",
	"#TF_ImportFile_BuildFailedComplexModel",
	"#TF_ImportFile_LoadFailedTooManyBones",
	"#TF_ImportFile_LoadFailedBadFile",
	"#TF_ImportFile_LoadFailedTooManyMaterials",
	"#TF_ImportFile_LoadFailedMaterialCountMismatch",
	"#TF_ImportFile_LoadFailedAnimationTooLong",
};
COMPILE_TIME_ASSERT( SE_ARRAYSIZE( kLoadResultMessages ) == CTFFileImportDialog::NUM_LOAD_RESULTS );

static const char *kSaveResultMessages[ CTFFileImportDialog::NUM_SAVE_RESULTS ] = {
	"",
	"#TF_ImportFile_SaveFailedBadFile",
};
COMPILE_TIME_ASSERT( SE_ARRAYSIZE( kSaveResultMessages ) == CTFFileImportDialog::NUM_SAVE_RESULTS );

static const char *kWarningMessages[ CTFFileImportDialog::NUM_WARNINGS ] =
{
	"TF_ImportFile_Warning_BaseAlphaMask",
};
COMPILE_TIME_ASSERT( SE_ARRAYSIZE( kWarningMessages ) == CTFFileImportDialog::NUM_WARNINGS );

static const char *kLODLevels[] =
{
	"HIGH LOD",
	"MEDIUM LOD",
	"LOW LOD"
};
COMPILE_TIME_ASSERT( SE_ARRAYSIZE( kLODLevels ) == NUM_IMPORT_LODS );

static const char *kPrefabs[] =
{
	"hat",
	"misc",
	"taunt"
};
COMPILE_TIME_ASSERT( SE_ARRAYSIZE( kPrefabs ) == CTFFileImportDialog::PREFAB_COUNT );

static const char* kDefaultPrefab = "hat";
static const char* kDefaultEquipRegion = "hat";

// Poses that your character can do in the item preview
static const char *kModelPoses[] = {
	"REF",
	"STAND",
	"CROUCH",
	"RUN",
};
static const int kDefaultModelPoseIndex = 1;

static const char *kModelActions[] = {
	"taunt_laugh.vcd",
	// The attack and reload animations don't work properly with the Heavy and Pyro
	//"gesture_attack",
	//"gesture_reload",
};
static const int kDefaultModelActionIndex = 0;

static const char* s_pszMaterialFilePrefixes[] =
{
	"base",
	"normal",
	"phongexponent",
	"selfillum"
};
COMPILE_TIME_ASSERT( SE_ARRAYSIZE( s_pszMaterialFilePrefixes ) == NUM_MATERIAL_TEXTURE_FILE_TYPE );

// Each preview needs a unique model name so it actually gets reloaded if it changes
static int s_nLastPreviewModel;

static const char* kDefaultBumpmap = "models/player/shared/shared_normal";

struct LightwarpInfo_t
{
	const char *pszName;
	const char *pszPath;
};

static LightwarpInfo_t kLightwarps[] =
{
	{ "pyro", "models/player/pyro/pyro_lightwarp" },
	{ "weapon", "models/lightwarps/weapon_lightwarp" },
	{ "robot", "models/lightwarps/robot_lightwarp" },
	{ "sentry1", "models/buildables/sentry1/sentry1_lightwarp" },
	{ "sentry2", "models/buildables/sentry2/sentry2_lightwarp" },
	{ "sentry3", "models/buildables/sentry3/sentry3_lightwarp" },
	{ "ambassador", "models/weapons/c_items/c_ambassador_lightwarp" },
	{ "jarate", "models/lightwarps/jarate_lightwarp" },
};
static const char *kLightwarpPath = "lightwarp_path";

struct EnvmapInfo_t
{
	const char *pszName;
	const char *pszPath;
};

static EnvmapInfo_t kEnvmaps[] =
{
	{ "none", "" },
	{ "world cube map", "env_cubemap" },
	{ "saxxy gold", "effects/saxxy/saxxy_gold" },
	{ "pyro land goggles", "effects/pyrocube" }
};
static const char *kEnvmapPath = "envmap_path";

struct EnvmapMaskInfo_t
{
	const char *pszDisplayName;
	const char *pszVarName;
};

static EnvmapMaskInfo_t kEnvmapMasks[] =
{
	{ "none", "" },
	{ "base alpha", "$basealphaenvmapmask" },
	{ "normal map alpha", "$normalmapalphaenvmapmask" }
};
static const char *kEnvmapMaskVarName = "envmapmask_varname";


static float GetMaxTauntDuration()
{
#ifdef STAGING_ONLY
	if ( tf_steam_workshop_max_taunt_duration.GetFloat() > 0 )
	{
		return tf_steam_workshop_max_taunt_duration.GetFloat();
	}
#endif // STAGING_ONLY
	return MAX_TAUNT_DURATION;
}


static const char *GetWorkshopSoundScriptFile()
{
	static char szCorrectCaseFilePath[MAX_PATH];
	char szItemWorkshopSoundScriptAbsPath[MAX_PATH];
	if ( GenerateFullPath( kWorkshopSoundScriptFile, "MOD", szItemWorkshopSoundScriptAbsPath, SE_ARRAYSIZE( szItemWorkshopSoundScriptAbsPath ) ) )
	{
		g_pFullFileSystem->GetCaseCorrectFullPath( szItemWorkshopSoundScriptAbsPath, szCorrectCaseFilePath );
	}
	else
	{
		Warning( "Failed to GenerateFullPath %s\n", kWorkshopSoundScriptFile );
		return NULL;
	}

	return szCorrectCaseFilePath;
}


//-----------------------------------------------------------------------------
static void SetMessageFileVariable( KeyValues *pData, const char *pszFilePath )
{
	if ( !pData )
		return;

	wchar_t unicodeFile[MAX_PATH];
	g_pVGuiLocalize->ConvertANSIToUnicode( pszFilePath, unicodeFile, sizeof(unicodeFile) );
	pData->SetWString( "file", unicodeFile );
}

//-----------------------------------------------------------------------------
static void ShowMessageBoxWithFile( const char *pTitle, const char *pText, const char *pszFilePath)
{
	KeyValuesAD pData( "data" );
	SetMessageFileVariable( pData, pszFilePath );
	ShowMessageBox( pTitle, pText, pData );
}

static void SaveBrowsePath( ConVar &conVar, const char *pszFilePath )
{
	char pszDirPath[MAX_PATH];
	V_ExtractFilePath( pszFilePath, pszDirPath, sizeof(pszDirPath) );
	conVar.SetValue( pszDirPath );
}

//-----------------------------------------------------------------------------
// Purpose: Save build output for presenting to the user
//-----------------------------------------------------------------------------
class CBuildLog : public CItemLog
{
public:
	CBuildLog() : CItemLog(), m_log( 0, 0, CUtlBuffer::TEXT_BUFFER ) { }

	virtual void Log( ItemtestLogLevel_t nLogLevel, const char *pszMessage ) const
	{
		static const char *pszIgnoredMessages[] = {
			"CDynamicFunction: "
		};

		// See if this is a message we want to ignore in user output
		for ( int i = 0; i < SE_ARRAYSIZE(pszIgnoredMessages); ++i )
		{
			if ( V_strncmp( pszMessage, pszIgnoredMessages[ i ], V_strlen( pszIgnoredMessages[ i ] ) ) == 0 )
				return;
		}

		switch ( nLogLevel )
		{
		case kItemtest_Log_Info:
			::Msg( "%s", pszMessage );
			break;
		case kItemtest_Log_Warning:
			::Warning( "%s", pszMessage );
			break;
		case kItemtest_Log_Error:
			::Error( "%s", pszMessage );
			break;
		}
		m_log.PutString( pszMessage );
	}

	const char *Get() { return (char*)m_log.Base(); }

protected:
	mutable CUtlBuffer m_log;
};


//-----------------------------------------------------------------------------
// Purpose: Import text edit dialog
//-----------------------------------------------------------------------------
class CTFFileImportTextEditDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CTFFileImportTextEditDialog, Frame );

public:
	CTFFileImportTextEditDialog( vgui::Panel *parent, const char *pszTitle, const char *pszCommand = NULL );

	virtual ~CTFFileImportTextEditDialog() { }

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	virtual void OnCommand( const char *command );

	void HideCancelButton() { m_bShowCancelButton = false; }

	void SetText( const char *pszText );
	bool GetText( char *pszText, int nMaxSize );

protected:
	bool m_bShowCancelButton;
	CUtlString m_sTitle;
	CUtlString m_sText;
	CUtlString m_sCommand;
	vgui::TextEntry *m_pTextEntry;
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFFileImportTextEditDialog::CTFFileImportTextEditDialog( vgui::Panel *parent, const char *pszTitle, const char *pszCommand ) : Frame( parent, "ImportFileTextEditDialog" )
,	m_bShowCancelButton( true )
,	m_sTitle( pszTitle )
,	m_sText()
,	m_sCommand( pszCommand )
,	m_pTextEntry( NULL )
{
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/ClientScheme.res", "ClientScheme" );
	SetScheme(scheme);
	SetProportional( true );
	SetDeleteSelfOnClose( true );
	SetTitle( "", false );
	SetSizeable( false );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFileImportTextEditDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/ImportFileTextEditDialog.res" );

	vgui::Label *pLabel = FindControl<vgui::Label>( "TitleLabel" );
	if ( pLabel )
	{
		pLabel->SetText( m_sTitle );
	}

	m_pTextEntry = FindControl<vgui::TextEntry>( "TextEntry" );
	if ( m_pTextEntry )
	{
		m_pTextEntry->RequestFocus();
		m_pTextEntry->SetMultiline( true );
		m_pTextEntry->SetCatchEnterKey( true );
		m_pTextEntry->SetVerticalScrollbar( true );
		m_pTextEntry->SetText( m_sText );
	}

	if ( !m_bShowCancelButton )
	{
		vgui::Panel *pButton = FindControl<vgui::Panel>( "ButtonClose" );
		if ( pButton )
		{
			pButton->SetVisible( false );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFileImportTextEditDialog::OnCommand( const char *command )
{
	if ( V_strieq( command, "Done" ) )
	{
		if ( m_sCommand.IsEmpty() )
		{
			OnCommand( "Close" );
		}
		else
		{
			GetParent()->OnCommand( m_sCommand );
		}
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFileImportTextEditDialog::SetText( const char *pszText )
{
	m_sText = pszText;

	if ( m_pTextEntry )
	{
		m_pTextEntry->SetText( pszText );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFFileImportTextEditDialog::GetText( char *pszText, int nMaxSize )
{
	if ( m_pTextEntry )
	{
		m_pTextEntry->GetText( pszText, nMaxSize );
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Import material edit dialog
//-----------------------------------------------------------------------------
class CTFImportMaterialEditDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CTFImportMaterialEditDialog, Frame );

public:
	CTFImportMaterialEditDialog( vgui::Panel *parent, int nSkinIndex, int nMaterialIndex, KeyValues *pItemValues );

	virtual ~CTFImportMaterialEditDialog() { }

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	virtual void OnCommand( const char *command );

	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );
	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );

	void SetVMTKeyValues( const char *pszRedVMTText, const char *pszBlueVMTText );
	KeyValues* GetVMTKeyValues( int nSkinIndex );

	const char* GetBaseTextureFile( int nSkinIndex ) const { return m_strBaseTextureFile[nSkinIndex].String(); }
	const char* GetNormalTextureFile() const { return m_strNormalTextureFile.String(); }
	const char* GetPhongExponentTextureFile() const { return m_strPhongExponentTextureFile.String(); }
	const char* GetSelfIllumTextureFile() const { return m_bSelfIllumEnabled ? m_strSelfIllumTextureFile.String() : ""; }

private:
	void OnCommandEditSkin( int nSkinIndex );
	void OnCommandBrowseMaterial( MATERIAL_FILE_TYPE fileType );

	void SetColorTintBase( int nSkinIndex, const char* pszColorTintBase );
	void SetSelfIllumTint( int nSkinIndex, const char* pszSelfIllumTint );

	void UpdateBackgroundTexture( int nSkinIndex );

	void UpdateBaseTexture( const char *pszBaseTextureFile );
	void UpdateNormalTexture( const char *pszNormalTextureFile );
	void UpdatePhongExponentTexture( const char* pszPhongExponentTextureFile );
	void UpdateSelfIllumTexture( const char *pszSelfIllumTextureFile );

	void UpdateBaseMapAlphaPhongMask();
	void UpdateRimMask();
	void UpdateRimMaskDisplay( bool bEnable );
	void UpdateHalfLambert();
	void UpdateBlendTintByBaseAlpha();
	void UpdateBlendTintColorOverBaseDisplay( bool bEnable );
	void UpdateColorTintBase( int nSkinIndex, const char* pszColor );
	void UpdateAdditive();
	void UpdateTranslucent();
	void UpdateAlphaTest();
	void UpdateAlphaTestDiaplay( bool bEnable );
	void UpdateSelfIllum();
	void UpdateSelfIllumDisplay( bool bEnable );
	void UpdateSelfIllumTint( int nSkinIndex, const char* pszColor );
	void UpdateEnvmapDisplay( bool bEnable );

	void UpdateUniqueLabels();

	CUtlString m_sCommand;
	KeyValuesAD m_VMTKeyValues;
	int m_nSkinIndex;
	int m_nMaterialIndex;
	KeyValues *m_pItemValues;
	
	// unique values per skin
	CUtlString m_strBaseTextureFile[NUM_IMPORT_MATERIALS_PER_TEAM];
	CUtlString m_strColorTintBase[NUM_IMPORT_MATERIALS_PER_TEAM];
	CUtlString m_strSelfIllumTint[NUM_IMPORT_MATERIALS_PER_TEAM];

	CUtlString m_strNormalTextureFile;
	CUtlString m_strPhongExponentTextureFile;
	bool m_bSelfIllumEnabled;
	CUtlString m_strSelfIllumTextureFile;
	MATERIAL_FILE_TYPE m_browseMaterialFileType;

	vgui::Button *m_pRedTeamButton;
	vgui::Button *m_pBlueTeamButton;

	vgui::Label *m_pBaseTextureFileLabel;
	vgui::Label *m_pNormalTextureFileLabel;
	vgui::Label *m_pPhongExponentTextureFileLabel;
	vgui::Label *m_pSelfIllumTextureFileLabel;

	// Lighting
	vgui::ComboBox *m_pLightwarpComboBox;
	vgui::CheckButton *m_pBaseMapAlphaPhongMaskCheckButton;
	vgui::TextEntry *m_pPhongExponentTextEntry;
	vgui::TextEntry *m_pPhongBoostTextEntry;
	vgui::TextEntry *m_pRimLightExponentTextEntry;
	vgui::TextEntry *m_pRimLightBoostTextEntry;
	vgui::CheckButton *m_pRimMaskCheckButton;
	vgui::CheckButton *m_pHalfLambertCheckButton;

	// Paint
	vgui::CheckButton *m_pBlendTintByBaseAlphaCheckButton;
	vgui::TextEntry *m_pBlendTintColorOverBaseTextEntry;
	vgui::TextEntry *m_pColorTintBaseRedTextEntry;
	vgui::TextEntry *m_pColorTintBaseGreenTextEntry;
	vgui::TextEntry *m_pColorTintBaseBlueTextEntry;

	// Translucent
	vgui::CheckButton *m_pAdditiveCheckButton;
	vgui::CheckButton *m_pTranslucentCheckButton;
	vgui::CheckButton *m_pAlphaTestCheckButton;

	// Cube map
	vgui::ComboBox *m_pEnvmapComboBox;
	vgui::ComboBox *m_pEnvmapAlphaMaskComboBox;
	vgui::TextEntry *m_pCubemapTintRedTextEntry;
	vgui::TextEntry *m_pCubemapTintGreenTextEntry;
	vgui::TextEntry *m_pCubemapTintBlueTextEntry;

	// Self Illum
	vgui::CheckButton *m_pSelfIllumCheckButton;
	vgui::TextEntry *m_pSelfIllumTintRedTextEntry;
	vgui::TextEntry *m_pSelfIllumTintGreenTextEntry;
	vgui::TextEntry *m_pSelfIllumTintBlueTextEntry;
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFImportMaterialEditDialog::CTFImportMaterialEditDialog( vgui::Panel *parent, int nSkinIndex, int nMaterialIndex, KeyValues* pItemValues ) : Frame( parent, "ImportMaterialEditDialog" )
	,	m_sCommand( CFmtStr( "EditMaterialDone%d,%d", nSkinIndex, nMaterialIndex ) )
	,	m_VMTKeyValues( "VMT" )
	,	m_nSkinIndex( nSkinIndex )
	,	m_nMaterialIndex( nMaterialIndex )
	,	m_pItemValues( pItemValues ) 
{
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/ClientScheme.res", "ClientScheme" );
	SetScheme(scheme);
	SetProportional( true );
	SetDeleteSelfOnClose( true );
	SetTitle( "", false );
	SetSizeable( false );

	bool bSetSharedTextures = false;
	for ( int nSkin=0; nSkin<CItemUpload::Manifest()->GetNumMaterialSkins(); ++nSkin )
	{
		KeyValues* pMaterialKey = m_pItemValues->FindKey( CFmtStr( kMaterialSkinN, CItemUpload::Manifest()->GetMaterialSkin( nSkin ), m_nMaterialIndex ) );
		if ( pMaterialKey )
		{
			m_strBaseTextureFile[nSkin] = pMaterialKey->GetString( CFmtStr( "%s_texture_file", s_pszMaterialFilePrefixes[MATERIAL_FILE_BASETEXTURE] ) );
			if ( !bSetSharedTextures )
			{
				m_strNormalTextureFile = pMaterialKey->GetString( CFmtStr( "%s_texture_file", s_pszMaterialFilePrefixes[MATERIAL_FILE_NORMAL] ) );
				m_strPhongExponentTextureFile = pMaterialKey->GetString( CFmtStr( "%s_texture_file", s_pszMaterialFilePrefixes[MATERIAL_FILE_PHONGEXPONENT] ) );
				m_strSelfIllumTextureFile = pMaterialKey->GetString( CFmtStr( "%s_texture_file", s_pszMaterialFilePrefixes[MATERIAL_FILE_SELFILLUM] ) );
				bSetSharedTextures = true;
			}
		}
	}

	m_pRedTeamButton = NULL;
	m_pBlueTeamButton = NULL;
	m_pBaseTextureFileLabel = NULL;
	m_pNormalTextureFileLabel = NULL;
	m_pPhongExponentTextureFileLabel = NULL;
	m_bSelfIllumEnabled = false;
	m_pSelfIllumTextureFileLabel = NULL;
	m_pLightwarpComboBox = NULL;
	m_pBaseMapAlphaPhongMaskCheckButton = NULL;
	m_pPhongExponentTextEntry = NULL;
	m_pPhongBoostTextEntry = NULL;
	m_pRimLightExponentTextEntry = NULL;
	m_pRimLightBoostTextEntry = NULL;
	m_pRimMaskCheckButton = NULL;
	m_pHalfLambertCheckButton = NULL;
	m_pBlendTintByBaseAlphaCheckButton = NULL;
	m_pBlendTintColorOverBaseTextEntry = NULL;
	m_pColorTintBaseRedTextEntry = NULL;
	m_pColorTintBaseGreenTextEntry = NULL;
	m_pColorTintBaseBlueTextEntry = NULL;
	m_pAdditiveCheckButton = NULL;
	m_pTranslucentCheckButton = NULL;
	m_pAlphaTestCheckButton = NULL;
	m_pEnvmapComboBox = NULL;
	m_pEnvmapAlphaMaskComboBox = NULL;
	m_pCubemapTintRedTextEntry = NULL;
	m_pCubemapTintGreenTextEntry = NULL;
	m_pCubemapTintBlueTextEntry = NULL;
	m_pSelfIllumCheckButton = NULL;
	m_pSelfIllumTintRedTextEntry = NULL;
	m_pSelfIllumTintGreenTextEntry = NULL;
	m_pSelfIllumTintBlueTextEntry = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/ImportMaterialEditDialog.res" );

	bool bAllTeam = m_pItemValues->GetInt( kSkinType ) == 0;

	if ( !bAllTeam )
	{
		m_pRedTeamButton = FindControl<vgui::Button>( "RedTeamButton" );
		if ( m_pRedTeamButton )
		{
			m_pRedTeamButton->SetVisible( true );
			m_pRedTeamButton->SetEnabled( true );
		}

		m_pBlueTeamButton = FindControl<vgui::Button>( "BlueTeamButton" );
		if ( m_pBlueTeamButton )
		{
			m_pBlueTeamButton->SetVisible( true );
			m_pBlueTeamButton->SetEnabled( true );
		}

		UpdateUniqueLabels();
	}

	UpdateBackgroundTexture( m_nSkinIndex );

	vgui::Label *pTitleLabel = FindControl<vgui::Label>( "TitleLabel" );
	if ( pTitleLabel )
	{
		pTitleLabel->SetText( CFmtStr( "#TF_ImportFile_EditVMT%d", m_nMaterialIndex ) );
	}

	m_pBaseTextureFileLabel = FindControl<vgui::Label>( "BaseTextureFileLabel" );
	m_pNormalTextureFileLabel = FindControl< vgui::Label >( "NormalTextureFileLabel" );
	m_pPhongExponentTextureFileLabel = FindControl< vgui::Label >( "PhongExponentTextureFileLabel" );
	m_pSelfIllumTextureFileLabel = FindControl< vgui::Label >( "SelfIllumTextureFileLabel" );

	m_pLightwarpComboBox = FindControl<vgui::ComboBox>( "LightwarpComboBox" );
	if ( m_pLightwarpComboBox )
	{
		m_pLightwarpComboBox->RemoveAll();

		KeyValuesAD pKeyValues( "data" );
		for ( int i=0; i<SE_ARRAYSIZE( kLightwarps ); ++i )
		{
			pKeyValues->SetString( kLightwarpPath, kLightwarps[i].pszPath );
			m_pLightwarpComboBox->AddItem( kLightwarps[i].pszName, pKeyValues );
		}

		m_pLightwarpComboBox->AddActionSignalTarget( this );
	}

	// Lighting
	m_pBaseMapAlphaPhongMaskCheckButton = FindControl<vgui::CheckButton>( "BaseMapAlphaPhongMaskCheckButton" );
	if ( m_pBaseMapAlphaPhongMaskCheckButton )
	{
		m_pBaseMapAlphaPhongMaskCheckButton->AddActionSignalTarget( this );
	}

	m_pPhongExponentTextEntry = FindControl<vgui::TextEntry>( "PhongExponentTextEntry" );
	if ( m_pPhongExponentTextEntry )
	{
		m_pPhongExponentTextEntry->AddActionSignalTarget( this );
	}

	m_pPhongBoostTextEntry = FindControl<vgui::TextEntry>( "PhongBoostTextEntry" );
	if ( m_pPhongBoostTextEntry )
	{
		m_pPhongBoostTextEntry->AddActionSignalTarget( this );
	}

	m_pRimLightExponentTextEntry = FindControl<vgui::TextEntry>( "RimLightExponentTextEntry" );
	if ( m_pRimLightExponentTextEntry )
	{
		m_pRimLightExponentTextEntry->AddActionSignalTarget( this );
	}

	m_pRimLightBoostTextEntry = FindControl<vgui::TextEntry>( "RimLightBoostTextEntry" );
	if ( m_pRimLightBoostTextEntry )
	{
		m_pRimLightBoostTextEntry->AddActionSignalTarget( this );
	}

	m_pRimMaskCheckButton = FindControl<vgui::CheckButton>( "RimMaskCheckButton" );
	if ( m_pRimMaskCheckButton )
	{
		m_pRimMaskCheckButton->AddActionSignalTarget( this );
		m_pRimMaskCheckButton->SetCheckButtonCheckable( false );
	}

	m_pHalfLambertCheckButton = FindControl<vgui::CheckButton>( "HalfLambertCheckButton" );
	if ( m_pHalfLambertCheckButton )
	{
		m_pHalfLambertCheckButton->AddActionSignalTarget( this );
	}

	// Paint
	m_pBlendTintByBaseAlphaCheckButton = FindControl<vgui::CheckButton>( "BlendTintByBaseAlphaCheckButton" );
	if ( m_pBlendTintByBaseAlphaCheckButton )
	{
		m_pBlendTintByBaseAlphaCheckButton->AddActionSignalTarget( this );
		m_pBlendTintByBaseAlphaCheckButton->SetCheckButtonCheckable( false );
	}

	m_pBlendTintColorOverBaseTextEntry = FindControl<vgui::TextEntry>( "BlendTintColorOverBaseTextEntry" );
	if ( m_pBlendTintColorOverBaseTextEntry )
	{
		m_pBlendTintColorOverBaseTextEntry->AddActionSignalTarget( this );
	}

	m_pColorTintBaseRedTextEntry = FindControl<vgui::TextEntry>( "ColorTintBaseRedTextEntry" );
	if ( m_pColorTintBaseRedTextEntry )
	{
		m_pColorTintBaseRedTextEntry->AddActionSignalTarget( this );
	}

	m_pColorTintBaseGreenTextEntry = FindControl<vgui::TextEntry>( "ColorTintBaseGreenTextEntry" );
	if ( m_pColorTintBaseGreenTextEntry )
	{
		m_pColorTintBaseGreenTextEntry->AddActionSignalTarget( this );
	}

	m_pColorTintBaseBlueTextEntry = FindControl<vgui::TextEntry>( "ColorTintBaseBlueTextEntry" );
	if ( m_pColorTintBaseBlueTextEntry )
	{
		m_pColorTintBaseBlueTextEntry->AddActionSignalTarget( this );
	}

	// enable paintable UI
	const char* pszPaintable = CFmtStr( kItemPaintable, m_nMaterialIndex );
	if ( m_pItemValues->GetBool( pszPaintable ) )
	{
		m_pBlendTintByBaseAlphaCheckButton->SetEnabled( true );
		m_pBlendTintByBaseAlphaCheckButton->SetCheckButtonCheckable( true );
		m_pBlendTintColorOverBaseTextEntry->SetEnabled( true );
		m_pColorTintBaseRedTextEntry->SetEnabled( true );
		m_pColorTintBaseGreenTextEntry->SetEnabled( true );
		m_pColorTintBaseBlueTextEntry->SetEnabled( true );

		static const char* paintableUILabels[] =
		{
			"BlendTintByBaseAlphaLabel",
			"BlendTintColorOverBaseLabel",
			"ColorTintBaseLabel"
		};

		for ( int i=0; i<SE_ARRAYSIZE( paintableUILabels ); ++i )
		{
			vgui::Label *pLabel = FindControl<vgui::Label>( paintableUILabels[i] );
			if ( pLabel )
			{
				pLabel->SetEnabled( true );
			}
		}
	}

	// Translucent
	m_pAdditiveCheckButton = FindControl<vgui::CheckButton>( "AdditiveCheckButton" );
	if ( m_pAdditiveCheckButton )
	{
		m_pAdditiveCheckButton->AddActionSignalTarget( this );
	}
	m_pTranslucentCheckButton = FindControl<vgui::CheckButton>( "TranslucentCheckButton" );
	if ( m_pTranslucentCheckButton )
	{
		m_pTranslucentCheckButton->AddActionSignalTarget( this );
	}
	m_pAlphaTestCheckButton = FindControl<vgui::CheckButton>( "AlphaTestCheckButton" );
	if ( m_pAlphaTestCheckButton )
	{
		m_pAlphaTestCheckButton->AddActionSignalTarget( this );
		m_pAlphaTestCheckButton->SetCheckButtonCheckable( false );
	}

	// Cube map
	m_pEnvmapComboBox = FindControl<vgui::ComboBox>( "EnvmapComboBox" );
	if ( m_pEnvmapComboBox )
	{
		KeyValuesAD pKeyValues( "data" );
		for ( int i=0; i<SE_ARRAYSIZE(kEnvmaps); ++i )
		{
			pKeyValues->SetString( kEnvmapPath, kEnvmaps[i].pszPath );
			m_pEnvmapComboBox->AddItem( kEnvmaps[i].pszName, pKeyValues );
		}

		m_pEnvmapComboBox->AddActionSignalTarget( this );
	}

	m_pEnvmapAlphaMaskComboBox = FindControl<vgui::ComboBox>( "EnvmapAlphaMaskComboBox" );
	if ( m_pEnvmapAlphaMaskComboBox )
	{
		KeyValuesAD pKeyValues( "data" );
		for ( int i=0; i<SE_ARRAYSIZE(kEnvmapMasks); ++i )
		{
			pKeyValues->SetString( kEnvmapMaskVarName, kEnvmapMasks[i].pszVarName );
			m_pEnvmapAlphaMaskComboBox->AddItem( kEnvmapMasks[i].pszDisplayName, pKeyValues );
		}

		m_pEnvmapAlphaMaskComboBox->AddActionSignalTarget( this );
	}

	m_pCubemapTintRedTextEntry = FindControl<vgui::TextEntry>( "CubemapTintRedTextEntry" );
	if ( m_pCubemapTintRedTextEntry )
	{
		m_pCubemapTintRedTextEntry->AddActionSignalTarget( this );
	}

	m_pCubemapTintGreenTextEntry = FindControl<vgui::TextEntry>( "CubemapTintGreenTextEntry" );
	if ( m_pCubemapTintGreenTextEntry )
	{
		m_pCubemapTintGreenTextEntry->AddActionSignalTarget( this );
	}

	m_pCubemapTintBlueTextEntry = FindControl<vgui::TextEntry>( "CubemapTintBlueTextEntry" );
	if ( m_pCubemapTintBlueTextEntry )
	{
		m_pCubemapTintBlueTextEntry->AddActionSignalTarget( this );
	}

	// Self Illum
	m_pSelfIllumCheckButton = FindControl<vgui::CheckButton>( "SelfIllumCheckButton" );
	if ( m_pSelfIllumCheckButton )
	{
		m_pSelfIllumCheckButton->AddActionSignalTarget( this );
	}

	m_pSelfIllumTintRedTextEntry = FindControl<vgui::TextEntry>( "SelfIllumTintRedTextEntry" );
	if ( m_pSelfIllumTintRedTextEntry )
	{
		m_pSelfIllumTintRedTextEntry->AddActionSignalTarget( this );
	}

	m_pSelfIllumTintGreenTextEntry = FindControl<vgui::TextEntry>( "SelfIllumTintGreenTextEntry" );
	if ( m_pSelfIllumTintGreenTextEntry )
	{
		m_pSelfIllumTintGreenTextEntry->AddActionSignalTarget( this );
	}

	m_pSelfIllumTintBlueTextEntry = FindControl<vgui::TextEntry>( "SelfIllumTintBlueTextEntry" );
	if ( m_pSelfIllumTintBlueTextEntry )
	{
		m_pSelfIllumTintBlueTextEntry->AddActionSignalTarget( this );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::OnCommand( const char *command )
{
	if ( FStrEq( command, "Done" ) )
	{
		if ( m_sCommand.IsEmpty() )
		{
			OnCommand( "Close" );
		}
		else if ( GetParent() )
		{
			GetParent()->OnCommand( m_sCommand );
		}
	}
	else if ( V_strncasecmp( command, "EditSkin", V_strlen( "EditSkin" ) ) == 0 )
	{
		int index = V_atoi( &command[ V_strlen( "EditSkin" ) ] );
		OnCommandEditSkin( index );
	}
	else if ( FStrEq( command, "BrowseMaterial" ) )
	{
		OnCommandBrowseMaterial( MATERIAL_FILE_BASETEXTURE );
	}
	else if ( FStrEq( command, "BrowseNormalTexture" ) )
	{
		OnCommandBrowseMaterial( MATERIAL_FILE_NORMAL );
	}
	else if ( FStrEq( command, "BrowsePhongExponentTexture" ) )
	{
		OnCommandBrowseMaterial( MATERIAL_FILE_PHONGEXPONENT );
	}
	else if ( FStrEq( command, "BrowseSelfIllumTexture" ) )
	{
		OnCommandBrowseMaterial( MATERIAL_FILE_SELFILLUM );
	}
	else if ( FStrEq( command, "ClearNormalTexture" ) )
	{
		UpdateNormalTexture( "" );
	}
	else if ( FStrEq( command, "ClearPhongExponentTexture" ) )
	{
		UpdatePhongExponentTexture( "" );
	}
	else if ( FStrEq( command, "ClearSelfIllumTexture" ) )
	{
		UpdateSelfIllumTexture( "" );
	}
	else if ( FStrEq( command, "UpdateBaseMapAlphaPhongMask" ) )
	{
		UpdateBaseMapAlphaPhongMask();
	}
	else if ( FStrEq( command, "UpdateRimMask" ) )
	{
		UpdateRimMask();
	}
	else if ( FStrEq( command, "UpdateHalfLambert" ) )
	{
		UpdateHalfLambert();
	}
	else if ( FStrEq( command, "UpdateBlendTintByBaseAlpha" ) )
	{
		UpdateBlendTintByBaseAlpha();
	}
	else if ( FStrEq( command, "UpdateAdditive" ) )
	{
		UpdateAdditive();
	}
	else if ( FStrEq( command, "UpdateTranslucent" ) )
	{
		UpdateTranslucent();
	}
	else if ( FStrEq( command, "UpdateAlphaTest" ) )
	{
		UpdateAlphaTest();
	}
	else if ( FStrEq( command, "UpdateSelfIllum" ) )
	{
		UpdateSelfIllum();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::OnCommandEditSkin( int nSkinIndex )
{
	if ( m_nSkinIndex == nSkinIndex )
	{
		return;
	}

	m_nSkinIndex = nSkinIndex;

	UpdateBackgroundTexture( m_nSkinIndex );
	UpdateBaseTexture( m_strBaseTextureFile[m_nSkinIndex] );
	UpdateColorTintBase( m_nSkinIndex, m_strColorTintBase[m_nSkinIndex] );
	UpdateSelfIllumTint( m_nSkinIndex, m_strSelfIllumTint[m_nSkinIndex] );
	UpdateUniqueLabels();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::OnCommandBrowseMaterial( MATERIAL_FILE_TYPE fileType )
{
	vgui::FileOpenDialog *pDialog = new vgui::FileOpenDialog( NULL, "#TF_ImportFile_SelectMaterial", vgui::FOD_OPEN );
	pDialog->AddFilter( "*.tga,*.psd", "#TF_ImportFile_MaterialFileType", true);
	pDialog->AddActionSignalTarget( this );
	const char *pszStartPath = tf_steam_workshop_import_material_path.GetString();
	if ( pszStartPath && *pszStartPath )
	{
		pDialog->SetStartDirectory( pszStartPath );
	}
	pDialog->DoModal();
	pDialog->Activate();

	m_browseMaterialFileType = fileType;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFFileImportDialog::BUILD_RESULT CheckImageSize( const char *fullpath, MATERIAL_FILE_TYPE materialFileType, KeyValues *pMessageVariables )
{
	CAssetTF asset;
	CTargetTGA image( &asset, NULL );
	if ( !image.SetInputFile( fullpath ) )
	{
		SetMessageFileVariable( pMessageVariables, image.GetInputFile() );
		return CTFFileImportDialog::BUILD_FAILED_IMAGEUNSUPPORTEDFILETYPE;
	}

	int nLimitWidth = 0;
	int nLimitHeight = 0;

	switch ( materialFileType )
	{
	case MATERIAL_FILE_BASETEXTURE:
		{
			// 512x512
			nLimitWidth = nLimitHeight = 512;
		}
		break;
	case MATERIAL_FILE_NORMAL:
		{
			// 512x512
			nLimitWidth = nLimitHeight = 512;
		}
		break;
	case MATERIAL_FILE_PHONGEXPONENT:
		{
			// 256x256
			nLimitWidth = nLimitHeight = 256;
		}
		break;
	case MATERIAL_FILE_SELFILLUM:
		{
			// 256x256
			nLimitWidth = nLimitHeight = 256;
		}
		break;
	}

	if ( image.GetWidth() > nLimitWidth || image.GetHeight() > nLimitHeight )
	{
		SetMessageFileVariable( pMessageVariables, image.GetInputFile() );
		pMessageVariables->SetInt( "width", nLimitWidth );
		pMessageVariables->SetInt( "height", nLimitHeight );
		return CTFFileImportDialog::BUILD_FAILED_IMAGERESOLUTIONOVERLIMIT;
	}

	if ( !IsPowerOfTwo( image.GetWidth() ) || !IsPowerOfTwo( image.GetHeight() ) )
	{
		SetMessageFileVariable( pMessageVariables, image.GetInputFile() );
		return CTFFileImportDialog::BUILD_FAILED_IMAGERESOLUTIONNOTPOWEROF2;
	}

	return CTFFileImportDialog::BUILD_OKAY;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::OnFileSelected( char const *fullpath )
{
	SaveBrowsePath( tf_steam_workshop_import_material_path, fullpath );

	KeyValuesAD pKV("data");
	CTFFileImportDialog::BUILD_RESULT result = CheckImageSize( fullpath, m_browseMaterialFileType, pKV );
	if ( result != CTFFileImportDialog::BUILD_OKAY )
	{
		ShowMessageBox( "#TF_ImportFile_LoadFailed", kBuildResultMessages[result], pKV );
		return;
	}

	switch ( m_browseMaterialFileType )
	{
	case MATERIAL_FILE_BASETEXTURE:
		{
			UpdateBaseTexture( fullpath );
		}
		break;
	case MATERIAL_FILE_NORMAL:
		{
			UpdateNormalTexture( fullpath );
		}
		break;
	case MATERIAL_FILE_PHONGEXPONENT:
		{
			UpdatePhongExponentTexture( fullpath );
		}
		break;
	case MATERIAL_FILE_SELFILLUM:
		{
			UpdateSelfIllumTexture( fullpath );
		}
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::OnTextChanged( KeyValues *data )
{
	Panel *pPanel = reinterpret_cast<vgui::Panel *>( data->GetPtr( "panel" ) );

	vgui::ComboBox *pComboBox = dynamic_cast< vgui::ComboBox * >( pPanel );
	if ( pComboBox )
	{
		if( pComboBox == m_pLightwarpComboBox )
		{
			KeyValues *pData = m_pLightwarpComboBox->GetActiveItemUserData();
			m_VMTKeyValues->SetString( "$lightwarptexture", pData->GetString( kLightwarpPath ) );
		}
		else if ( pComboBox == m_pEnvmapComboBox )
		{
			KeyValues *pData = m_pEnvmapComboBox->GetActiveItemUserData();
			const char* pszEnvmapPath = pData->GetString( kEnvmapPath );
			m_VMTKeyValues->SetString( "$envmap", pszEnvmapPath );
			UpdateEnvmapDisplay( !FStrEq( pszEnvmapPath, "" ) );
		}
		else if ( pComboBox == m_pEnvmapAlphaMaskComboBox )
		{
			KeyValues *pData = m_pEnvmapAlphaMaskComboBox->GetActiveItemUserData();
			const char* pszEnvmapMaskVarName = pData->GetString( kEnvmapMaskVarName );
			for ( int i=0; i<SE_ARRAYSIZE(kEnvmapMasks); ++i )
			{
				const char* pszCurrentVarName = kEnvmapMasks[i].pszVarName;
				if ( FStrEq( pszCurrentVarName, "" ) )
				{
					continue;
				}

				bool bSet = FStrEq( pszCurrentVarName, pszEnvmapMaskVarName );
				m_VMTKeyValues->SetBool( pszCurrentVarName, bSet );
			}

			// pop a warning
			static bool bWarned = false;
			if  ( FStrEq( pszEnvmapMaskVarName, "$basealphaenvmapmask" ) )
			{
				bool bHasNormal = !m_strNormalTextureFile.IsEmpty();
				if ( !bWarned && bHasNormal )
				{
					bWarned = true;
					ShowMessageBox( "#TF_ImportFile_Warning", kWarningMessages[ CTFFileImportDialog::WARNING_BASEALPHAMASK ] );
				}
			}
			else
			{
				bWarned = false;
			}
		}

		return;
	}

	vgui::TextEntry *pTextEntry = dynamic_cast< vgui::TextEntry * >( pPanel );
	if ( pTextEntry )
	{
		if ( pTextEntry == m_pPhongExponentTextEntry )
		{
			m_VMTKeyValues->SetFloat( "$phongexponent", m_pPhongExponentTextEntry->GetValueAsFloat() );
		}
		else if ( pTextEntry == m_pPhongBoostTextEntry )
		{
			m_VMTKeyValues->SetFloat( "$phongboost", m_pPhongBoostTextEntry->GetValueAsFloat() );
		}
		else if ( pTextEntry == m_pRimLightExponentTextEntry )
		{
			m_VMTKeyValues->SetFloat( "$rimlightexponent", m_pRimLightExponentTextEntry->GetValueAsFloat() );
		}
		else if ( pTextEntry == m_pRimLightBoostTextEntry )
		{
			m_VMTKeyValues->SetFloat( "$rimlightboost", m_pRimLightBoostTextEntry->GetValueAsFloat() );
		}
		else if ( pTextEntry == m_pBlendTintColorOverBaseTextEntry )
		{
			float flClampedValue = clamp( m_pBlendTintColorOverBaseTextEntry->GetValueAsFloat(), 0.f, 1.f );
			m_VMTKeyValues->SetFloat( "$blendtintcoloroverbase", flClampedValue );
		}
		else if (	pTextEntry == m_pColorTintBaseRedTextEntry ||
					pTextEntry == m_pColorTintBaseGreenTextEntry ||
					pTextEntry == m_pColorTintBaseBlueTextEntry )
		{
			const char* pszColor = CFmtStr( "{ %d %d %d }",
											clamp( m_pColorTintBaseRedTextEntry->GetValueAsInt(), 0, 255 ),
											clamp( m_pColorTintBaseGreenTextEntry->GetValueAsInt(), 0, 255 ),
											clamp( m_pColorTintBaseBlueTextEntry->GetValueAsInt(), 0, 255 ) );
			SetColorTintBase( m_nSkinIndex, pszColor );
		}
		else if (	pTextEntry == m_pCubemapTintRedTextEntry ||
					pTextEntry == m_pCubemapTintGreenTextEntry ||
					pTextEntry == m_pCubemapTintBlueTextEntry )
		{
			float flR = (float)clamp( m_pCubemapTintRedTextEntry->GetValueAsInt(), 0, 255 ) / 255.f;
			float flG = (float)clamp( m_pCubemapTintGreenTextEntry->GetValueAsInt(), 0, 255 ) / 255.f;
			float flB = (float)clamp( m_pCubemapTintBlueTextEntry->GetValueAsInt(), 0, 255 ) / 255.f;
			const char* pszColor = CFmtStr( "[%f %f %f]", flR, flG, flB );
			m_VMTKeyValues->SetString( "$envmaptint", pszColor );
		}
		else if (	pTextEntry == m_pSelfIllumTintRedTextEntry ||
					pTextEntry == m_pSelfIllumTintGreenTextEntry ||
					pTextEntry == m_pSelfIllumTintBlueTextEntry )
		{
			float flR = m_pSelfIllumTintRedTextEntry->GetValueAsFloat() / 255.f;
			float flG = m_pSelfIllumTintGreenTextEntry->GetValueAsFloat() / 255.f;
			float flB = m_pSelfIllumTintBlueTextEntry->GetValueAsFloat() / 255.f;
			const char* pszColor = CFmtStr( "[%f %f %f]", flR, flG, flB );
			SetSelfIllumTint( m_nSkinIndex, pszColor );
		}

		return;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::SetColorTintBase( int nSkinIndex, const char* pszColorTintBase )
{
	m_VMTKeyValues->SetString( "$colortint_base", pszColorTintBase );
	m_VMTKeyValues->SetString( "$color2", pszColorTintBase );

	m_strColorTintBase[nSkinIndex] = pszColorTintBase;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::SetSelfIllumTint( int nSkinIndex, const char* pszSelfIllumTint )
{
	m_VMTKeyValues->SetString( "$selfillumtint", pszSelfIllumTint );

	m_strSelfIllumTint[nSkinIndex] = pszSelfIllumTint;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::SetVMTKeyValues( const char *pszRedVMTText, const char *pszBlueVMTText )
{
	m_VMTKeyValues->Clear();

	// read the unique data
	const char* pszVMTs[NUM_IMPORT_MATERIALS_PER_TEAM] = { pszRedVMTText, pszBlueVMTText };
	for ( int i=0; i<SE_ARRAYSIZE( pszVMTs ); ++i )
	{
		KeyValuesAD pKV( "VMT" );
		if ( pKV->LoadFromBuffer( "VMT", pszVMTs[i] ) )
		{
			m_strColorTintBase[i] = pKV->GetString( "$colortint_base" );
			m_strSelfIllumTint[i] = pKV->GetString( "$selfillumtint" );
		}
	}

	const char* pszVMTText = m_nSkinIndex == 0 ? pszRedVMTText : pszBlueVMTText;
	if ( m_VMTKeyValues->LoadFromBuffer( "VMT", pszVMTText ) )
	{
		// update the UI here
		UpdateBaseTexture( m_strBaseTextureFile[m_nSkinIndex].String() );
		UpdateNormalTexture( m_strNormalTextureFile.String() );
		UpdatePhongExponentTexture( m_strPhongExponentTextureFile.String() );
		UpdateSelfIllumTexture( m_strSelfIllumTextureFile.String() );

		if ( m_pLightwarpComboBox )
		{
			const char* pszCurrentLightwarpPath = m_VMTKeyValues->GetString( "$lightwarptexture" );
			for ( int i=0; i<SE_ARRAYSIZE(kLightwarps); ++i )
			{
				if ( FStrEq( pszCurrentLightwarpPath, kLightwarps[i].pszPath ) )
				{
					m_pLightwarpComboBox->ActivateItemByRow( i );
					break;
				}
			}
		}

		const char* pszFloatFormat = "%.3f";

		// Lighting
		if ( m_pBaseMapAlphaPhongMaskCheckButton )
		{
			m_pBaseMapAlphaPhongMaskCheckButton->SetSelected( m_VMTKeyValues->GetBool( "$basemapalphaphongmask" ) );
		}
		if ( m_pPhongExponentTextEntry )
		{
			m_pPhongExponentTextEntry->SetText( CFmtStr( pszFloatFormat, m_VMTKeyValues->GetFloat( "$phongexponent" ) ) );
		}
		if ( m_pPhongBoostTextEntry )
		{
			m_pPhongBoostTextEntry->SetText( CFmtStr( pszFloatFormat, m_VMTKeyValues->GetFloat( "$phongboost" ) ) );
		}
		if ( m_pRimLightExponentTextEntry )
		{
			m_pRimLightExponentTextEntry->SetText( CFmtStr( pszFloatFormat, m_VMTKeyValues->GetFloat( "$rimlightexponent" ) ) );
		}
		if ( m_pRimLightBoostTextEntry )
		{
			m_pRimLightBoostTextEntry->SetText( CFmtStr( pszFloatFormat, m_VMTKeyValues->GetFloat( "$rimlightboost" ) ) );
		}
		if ( m_pRimMaskCheckButton )
		{
			UpdateRimMaskDisplay( !m_strPhongExponentTextureFile.IsEmpty() );
			m_pRimMaskCheckButton->SetSelected( m_VMTKeyValues->GetBool( "$rimmask" ) );
		}
		if ( m_pHalfLambertCheckButton )
		{
			m_pHalfLambertCheckButton->SetSelected( m_VMTKeyValues->GetBool( "$halflambert" ) );
		}

		// Paint
		if ( m_pBlendTintByBaseAlphaCheckButton )
		{
			m_pBlendTintByBaseAlphaCheckButton->SetSelected( m_VMTKeyValues->GetBool( "$blendtintbybasealpha" ) );
		}
		if ( m_pBlendTintColorOverBaseTextEntry )
		{
			m_pBlendTintColorOverBaseTextEntry->SetText( CFmtStr( pszFloatFormat, m_VMTKeyValues->GetFloat( "$blendtintcoloroverbase" ) ) );
		}

		UpdateColorTintBase( m_nSkinIndex, m_VMTKeyValues->GetString( "$colortint_base" ) );

		// Translucent
		if ( m_pAdditiveCheckButton )
		{
			m_pAdditiveCheckButton->SetSelected( m_VMTKeyValues->GetBool( "$additive" ) );
		}
		if ( m_pTranslucentCheckButton )
		{
			m_pTranslucentCheckButton->SetSelected( m_VMTKeyValues->GetBool( "$translucent" ) );
		}
		if ( m_pAlphaTestCheckButton )
		{
			bool bTranslucent = m_pTranslucentCheckButton->IsSelected();
			UpdateAlphaTestDiaplay( bTranslucent );
			m_pAlphaTestCheckButton->SetSelected( m_VMTKeyValues->GetBool( "$alphatest" ) );
		}

		// Cube map
		if ( m_pEnvmapComboBox )
		{
			const char *pszEnvmap = m_VMTKeyValues->GetString( "$envmap" );
			for ( int i=0; i<m_pEnvmapComboBox->GetItemCount(); ++i )
			{
				KeyValues *pKey = m_pEnvmapComboBox->GetItemUserData( i );
				const char *pszPath = pKey->GetString( kEnvmapPath );
				if ( FStrEq( pszEnvmap, pszPath ) )
				{
					m_pEnvmapComboBox->ActivateItemByRow( i );
					break;
				}
			}
		}

		if ( m_pEnvmapAlphaMaskComboBox )
		{
			int nRow = 0;
			for ( int i=0; i<m_pEnvmapAlphaMaskComboBox->GetItemCount(); ++i )
			{
				KeyValues *pKey = m_pEnvmapAlphaMaskComboBox->GetItemUserData( i );
				const char *pszMask = pKey->GetString( kEnvmapMaskVarName );
				if ( pKey && m_VMTKeyValues->GetBool( pszMask ) )
				{
					nRow = i;
					break;
				}
			}
			m_pEnvmapAlphaMaskComboBox->ActivateItemByRow( nRow );
		}

		const char *pszStrippedEnvmapTintColor = m_VMTKeyValues->GetString( "$envmaptint" ) + 1;
		float entmapTint[3];
		UTIL_StringToFloatArray( entmapTint, SE_ARRAYSIZE( entmapTint ), pszStrippedEnvmapTintColor );
		if ( m_pCubemapTintRedTextEntry )
		{
			m_pCubemapTintRedTextEntry->SetText( CFmtStr( "%d", int( entmapTint[0] * 255.f ) ) );
		}
		if ( m_pCubemapTintGreenTextEntry )
		{
			m_pCubemapTintGreenTextEntry->SetText( CFmtStr( "%d", int( entmapTint[1] * 255.f )  ) );
		}
		if ( m_pCubemapTintBlueTextEntry )
		{
			m_pCubemapTintBlueTextEntry->SetText( CFmtStr( "%d", int( entmapTint[2] * 255.f ) ) );
		}

		// Self Illum
		bool bSelfIllum = m_VMTKeyValues->GetBool( ">=DX90/$selfillum" );
		if ( m_pSelfIllumCheckButton )
		{
			m_pSelfIllumCheckButton->SetSelected( bSelfIllum );
			UpdateSelfIllumDisplay( bSelfIllum );
		}

		UpdateSelfIllumTint( m_nSkinIndex, m_VMTKeyValues->GetString( "$selfillumtint" ) );
	}
	else
	{
		Warning( "CTFImportMaterialEditDialog::SetKeyValues failed to LoadFromBuffer\n" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
KeyValues* CTFImportMaterialEditDialog::GetVMTKeyValues( int nSkinIndex )
{
	//set unique values for the skin index here
	m_VMTKeyValues->SetString( "$colortint_base", m_strColorTintBase[nSkinIndex] );
	m_VMTKeyValues->SetString( "$color2", m_strColorTintBase[nSkinIndex] );
	m_VMTKeyValues->SetString( "$selfillumtint", m_strSelfIllumTint[nSkinIndex] );

	return m_VMTKeyValues;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdateBackgroundTexture( int nSkinIndex )
{
	static const char* pszBGs[] = { "RedBG", "BlueBG" };
	for ( int i=0; i<SE_ARRAYSIZE(pszBGs); ++i )
	{
		vgui::ImagePanel *pBG = FindControl<vgui::ImagePanel>( pszBGs[i] );
		if ( pBG )
		{
			bool bVisible = nSkinIndex == i;
			pBG->SetVisible( bVisible );
			pBG->SetEnabled( bVisible );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdateBaseTexture( const char *pszBaseTextureFile )
{
	if ( m_pBaseTextureFileLabel && pszBaseTextureFile )
	{
		m_pBaseTextureFileLabel->InvalidateLayout( true, true );

		if ( *pszBaseTextureFile )
		{
			char file[MAX_PATH];
			V_FileBase( pszBaseTextureFile, file, sizeof(file) );
			m_pBaseTextureFileLabel->SetText( file );
			m_pBaseTextureFileLabel->SetFgColor( Color( 255, 255, 255, 255 ) );
		}
		else
		{
			m_pBaseTextureFileLabel->SetText( "#TF_PublishFile_NoFileSelected" );
			m_pBaseTextureFileLabel->SetFgColor( Color( 255, 0, 0, 255 ) );
		}

		m_strBaseTextureFile[m_nSkinIndex] = pszBaseTextureFile;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdateNormalTexture( const char *pszNormalTextureFile )
{
	if ( m_pNormalTextureFileLabel && pszNormalTextureFile )
	{
		if ( *pszNormalTextureFile )
		{
			char file[MAX_PATH];
			V_FileBase( pszNormalTextureFile, file, sizeof(file) );
			m_pNormalTextureFileLabel->SetText( file );
		}
		else
		{
			m_pNormalTextureFileLabel->SetText( "#TF_PublishFile_Optional" );
		}

		m_strNormalTextureFile = pszNormalTextureFile;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdatePhongExponentTexture( const char* pszPhongExponentTextureFile )
{
	if ( m_pPhongExponentTextureFileLabel && pszPhongExponentTextureFile )
	{
		if ( *pszPhongExponentTextureFile )
		{
			char file[MAX_PATH];
			V_FileBase( pszPhongExponentTextureFile, file, sizeof(file) );
			m_pPhongExponentTextureFileLabel->SetText( file );
		}
		else
		{
			m_pPhongExponentTextureFileLabel->SetText( "#TF_PublishFile_Optional" );
		}

		m_strPhongExponentTextureFile = pszPhongExponentTextureFile;

		UpdateRimMaskDisplay( !m_strPhongExponentTextureFile.IsEmpty() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdateSelfIllumTexture( const char *pszSelfIllumTextureFile )
{
	if ( m_pSelfIllumTextureFileLabel && pszSelfIllumTextureFile )
	{
		if ( *pszSelfIllumTextureFile )
		{
			char file[MAX_PATH];
			V_FileBase( pszSelfIllumTextureFile, file, sizeof(file) );
			m_pSelfIllumTextureFileLabel->SetText( file );
		}
		else
		{
			m_pSelfIllumTextureFileLabel->SetText( "#TF_PublishFile_Optional" );
		}

		m_strSelfIllumTextureFile = pszSelfIllumTextureFile;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdateBaseMapAlphaPhongMask()
{
	if ( m_pBaseMapAlphaPhongMaskCheckButton )
	{
		m_VMTKeyValues->SetBool( "$basemapalphaphongmask", m_pBaseMapAlphaPhongMaskCheckButton->IsSelected() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdateRimMask()
{
	if ( m_pRimMaskCheckButton )
	{
		m_VMTKeyValues->SetBool( "$rimmask", m_pRimMaskCheckButton->IsSelected() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdateRimMaskDisplay( bool bEnable )
{
	if ( m_pRimMaskCheckButton )
	{
		m_pRimMaskCheckButton->SetEnabled( bEnable );
		m_pRimMaskCheckButton->SetCheckButtonCheckable( bEnable );
	}

	vgui::Label* pLabel = FindControl< vgui::Label >( "RimMaskLabel" );
	if ( pLabel )
	{
		pLabel->SetEnabled( bEnable );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdateHalfLambert()
{
	if ( m_pHalfLambertCheckButton )
	{
		m_VMTKeyValues->SetBool( "$halflambert", m_pHalfLambertCheckButton->IsSelected() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdateBlendTintByBaseAlpha()
{
	if ( m_pBlendTintByBaseAlphaCheckButton )
	{
		bool bSelected = m_pBlendTintByBaseAlphaCheckButton->IsSelected();
		m_VMTKeyValues->SetBool( "$blendtintbybasealpha", bSelected );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdateBlendTintColorOverBaseDisplay( bool bEnable )
{
	if ( m_pBlendTintColorOverBaseTextEntry )
	{
		if ( !bEnable )
		{
			m_VMTKeyValues->SetFloat( "$blendtintcoloroverbase", 0.f );
		}
		m_pBlendTintColorOverBaseTextEntry->SetText( CFmtStr( "%.3f", m_VMTKeyValues->GetFloat( "$blendtintcoloroverbase" ) ) );
		m_pBlendTintColorOverBaseTextEntry->SetEnabled( bEnable );

		vgui::Label *pLabel = FindControl<vgui::Label>( "BlendTintColorOverBaseLabel" );
		if ( pLabel )
		{
			pLabel->SetEnabled( bEnable );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdateColorTintBase( int nSkinIndex, const char* pszColor )
{
	m_strColorTintBase[nSkinIndex] = pszColor;

	int colorTintBase[3];
	// color string +2 to skip "{ " or "[ "
	const char *pszStrippedColorTintBase = pszColor + 2;
	UTIL_StringToIntArray( colorTintBase, 3, pszStrippedColorTintBase );
	if ( m_pColorTintBaseRedTextEntry )
	{
		m_pColorTintBaseRedTextEntry->SetText( CFmtStr( "%d", colorTintBase[0] ) );
	}
	if ( m_pColorTintBaseGreenTextEntry )
	{
		m_pColorTintBaseGreenTextEntry->SetText( CFmtStr( "%d", colorTintBase[1] ) );
	}
	if ( m_pColorTintBaseBlueTextEntry )
	{
		m_pColorTintBaseBlueTextEntry->SetText( CFmtStr( "%d", colorTintBase[2] ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdateAdditive()
{
	if ( m_pAdditiveCheckButton )
	{
		m_VMTKeyValues->SetBool( "$additive", m_pAdditiveCheckButton->IsSelected() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdateTranslucent()
{
	if ( m_pTranslucentCheckButton )
	{
		bool bSelected = m_pTranslucentCheckButton->IsSelected();
		UpdateAlphaTestDiaplay( bSelected );
		m_VMTKeyValues->SetBool( "$translucent", bSelected );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdateAlphaTest()
{
	if ( m_pAlphaTestCheckButton )
	{
		m_VMTKeyValues->SetBool( "$alphatest", m_pAlphaTestCheckButton->IsSelected() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdateAlphaTestDiaplay( bool bEnable )
{
	if ( m_pAlphaTestCheckButton )
	{
		m_pAlphaTestCheckButton->SetEnabled( bEnable );
		m_pAlphaTestCheckButton->SetCheckButtonCheckable( bEnable );
	}

	vgui::Label *pLabel = FindControl<vgui::Label>( "AlphaTestLabel" );
	if ( pLabel )
	{
		pLabel->SetEnabled( bEnable );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdateSelfIllum()
{
	if ( m_pSelfIllumCheckButton )
	{
		bool bSelected = m_pSelfIllumCheckButton->IsSelected();
		m_VMTKeyValues->SetBool( ">=DX90/$selfillum", bSelected );
		UpdateSelfIllumDisplay( bSelected );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdateSelfIllumDisplay( bool bEnable )
{
	m_bSelfIllumEnabled = bEnable;

	static const char* pszSelfIllumLabels[] =
	{
		"SelfIllumTintLabel",
		"SelfIllumTextureLabel"
	};

	for ( int i=0; i<SE_ARRAYSIZE(pszSelfIllumLabels); ++i )
	{
		vgui::Label *pLabel = FindControl<vgui::Label>( pszSelfIllumLabels[i] );
		if ( pLabel )
		{
			pLabel->SetEnabled( bEnable );
		}
	}

	static const char* pszSelfIllumButtons[] =
	{
		"SelfIllumTextureBrowse",
		"SelfIllumTextureClear"
	};

	for ( int i=0; i<SE_ARRAYSIZE(pszSelfIllumButtons); ++i )
	{
		vgui::Button *pButton = FindControl<vgui::Button>( pszSelfIllumButtons[i] );
		if ( pButton )
		{
			pButton->SetEnabled( bEnable );
		}
	}

	m_pSelfIllumTextureFileLabel->SetEnabled( bEnable );
	m_pSelfIllumTintRedTextEntry->SetEnabled( bEnable );
	m_pSelfIllumTintGreenTextEntry->SetEnabled( bEnable );
	m_pSelfIllumTintBlueTextEntry->SetEnabled( bEnable );

	// blend tint color over base is not compatible with selfillum. must turn it off
	UpdateBlendTintColorOverBaseDisplay( !bEnable );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdateSelfIllumTint( int nSkinIndex, const char* pszColor )
{
	m_strSelfIllumTint[nSkinIndex] = pszColor;

	// +1 to skip [
	const char *pszStrippedSelfIllumTintColor = pszColor + 1;
	float selfIllumTint[3];
	UTIL_StringToFloatArray( selfIllumTint, SE_ARRAYSIZE( selfIllumTint ), pszStrippedSelfIllumTintColor );
	if ( m_pSelfIllumTintRedTextEntry )
	{
		m_pSelfIllumTintRedTextEntry->SetText( CFmtStr( "%d", int( selfIllumTint[0] * 255.f ) ) );
	}
	if ( m_pSelfIllumTintGreenTextEntry )
	{
		m_pSelfIllumTintGreenTextEntry->SetText( CFmtStr( "%d", int( selfIllumTint[1] * 255.f )  ) );
	}
	if ( m_pSelfIllumTintBlueTextEntry )
	{
		m_pSelfIllumTintBlueTextEntry->SetText( CFmtStr( "%d", int( selfIllumTint[2] * 255.f ) ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdateEnvmapDisplay( bool bEnable )
{
	static const char* pszEnvmapLabels[] =
	{
		"EnvmapAlphaMaskLabel",
		"EnvmapTintLabel"
	};

	for ( int i=0; i<SE_ARRAYSIZE(pszEnvmapLabels); ++i )
	{
		vgui::Label *pLabel = FindControl<vgui::Label>( pszEnvmapLabels[i] );
		if ( pLabel )
		{
			pLabel->SetEnabled( bEnable );
		}
	}

	m_pEnvmapAlphaMaskComboBox->SetEnabled( bEnable );
	m_pCubemapTintRedTextEntry->SetEnabled( bEnable );
	m_pCubemapTintGreenTextEntry->SetEnabled( bEnable );
	m_pCubemapTintBlueTextEntry->SetEnabled( bEnable );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFImportMaterialEditDialog::UpdateUniqueLabels()
{
	static Color color[NUM_IMPORT_MATERIALS_PER_TEAM] = { Color( 189, 59, 59, 255 ), Color( 57, 92, 120, 255 ) };
	static const char* pszLabelNames[] =
	{
		"BaseTextureLabel",
		"ColorTintBaseLabel",
		"SelfIllumTintLabel"
	};

	for ( int i=0; i<SE_ARRAYSIZE( pszLabelNames ); ++i )
	{
		vgui::Label* pLabel = FindControl<vgui::Label>( pszLabelNames[i] );
		if ( pLabel )
		{
			pLabel->InvalidateLayout( true, true );
			pLabel->SetFgColor( color[m_nSkinIndex] );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Preview an item after building it
//-----------------------------------------------------------------------------
class CImportPreviewItemPanel : public CTFStorePreviewItemPanel2
{
	DECLARE_CLASS_SIMPLE( CImportPreviewItemPanel, CTFStorePreviewItemPanel2 );

public:
	CImportPreviewItemPanel( vgui::Panel *parent, KeyValues *pItemValues, int nSelectedClass );
	virtual ~CImportPreviewItemPanel();

	virtual void	PreviewItem( int iClass, CEconItemView *pItem, const econ_store_entry_t* pEntry = NULL ) OVERRIDE;

protected:
	void ResetHandles();

	virtual void OnThink();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnCommand( const char *command );
	virtual void OnKeyCodeTyped( KeyCode code );
	virtual void PerformLayout( void );
	virtual void OnClose();

	virtual void UpdatePlayerModelButtons();
	void UpdateActivityUI();
	void UpdateActivity();
	void UpdateMaterialButtons();

	virtual bool	AllowUnusualPreview() const { return true; }

	int GetSequence( const char *pszGesture = NULL );
	void StartAction();
	void StartGesture( const char *pszGesture );
	void EndGestures();

	bool ClassHasModels( int nClassIndex )
	{
		for ( int nModelIndex = 0; nModelIndex < NUM_IMPORT_LODS; ++nModelIndex )
		{
			if ( !V_strieq( m_pItemValues->GetString( CFmtStr( kClassLODNFile, kClassFolders[ nClassIndex ], nModelIndex ) ), "" ) )
				return true;
		}
		return false;
	}

	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );
	MESSAGE_FUNC_PARAMS( OnClassIconSelected, "ClassIconSelected", data );

	void ResetCamera();

	vgui::ComboBox	*m_pLODComboBox;
	vgui::ComboBox	*m_pLoadoutComboBox;
	vgui::ComboBox	*m_pPoseComboBox;
	vgui::ComboBox	*m_pActionComboBox;
	vgui::ComboBox	*m_pEffectComboBox;
	CUtlVector< vgui::Button* > m_pMaterialButtons;

	KeyValues *m_pItemValues;
	int m_nCurrentLOD;
	loadout_positions_t m_nCurrentLoadout;
	CUtlString m_sCurrentPose;
	CUtlString m_sCurrentAction;
	bool m_bIsVCDFileNameOnly;
	float m_flGestureEndTime;
	int m_nSelectedClass;

	int m_nCacheModelDetail;
	bool m_bIsTaunt;
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CImportPreviewItemPanel::CImportPreviewItemPanel( vgui::Panel *parent, KeyValues *pItemValues, int nSelectedClass )
: CTFStorePreviewItemPanel2( parent, "resource/ui/ImportPreviewItemPanel.res", "importpreviewitem", NULL )
, m_pItemValues( pItemValues )
, m_nCurrentLOD( 0 )
, m_nCurrentLoadout( LOADOUT_POSITION_PRIMARY )
, m_sCurrentPose( kModelPoses[ kDefaultModelPoseIndex ] )
, m_bIsVCDFileNameOnly( true )
, m_flGestureEndTime( 0.0f )
, m_nSelectedClass( nSelectedClass )
{							   
	ResetHandles();

	static ConVarRef r_rootlod( "r_rootlod" );
	if ( r_rootlod.IsValid() )
	{
		m_nCacheModelDetail = r_rootlod.GetInt();
		r_rootlod.SetValue( 0 );
	}

	m_bIsTaunt = FStrEq( m_pItemValues->GetString( kItemPrefab ), "taunt" );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CImportPreviewItemPanel::~CImportPreviewItemPanel()
{
	g_PlayerPreviewEffect.Reset();

	static ConVarRef r_rootlod( "r_rootlod" );
	if ( r_rootlod.IsValid() )
	{
		r_rootlod.SetValue( m_nCacheModelDetail );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CImportPreviewItemPanel::PreviewItem( int iClass, CEconItemView *pItem, const econ_store_entry_t* pEntry )
{
	if ( m_pPlayerModelPanel && m_iState == PS_PLAYER )
	{
		// Preserve state when refreshing the preview
		Vector vecSavedCameraOffset;
		Vector vecSavedCameraPos;
		QAngle angSavedCameraDir;
		m_pPlayerModelPanel->GetCameraOffset( vecSavedCameraOffset );
		m_pPlayerModelPanel->GetCameraPositionAndAngles( vecSavedCameraPos, angSavedCameraDir );

		int iSavedClass = m_iCurrentClass;

		BaseClass::PreviewItem( iClass, pItem, pEntry );

		KeyValuesAD pKeyValues( "data" );
		pKeyValues->SetInt( "class", iSavedClass );
		OnClassIconSelected( pKeyValues );

		m_pPlayerModelPanel->ResetCameraPivot();
		m_pPlayerModelPanel->SetCameraOffset( vecSavedCameraOffset );
		m_pPlayerModelPanel->SetCameraPositionAndAngles( vecSavedCameraPos, angSavedCameraDir );
		m_pPlayerModelPanel->SetForcedCameraPosition( true );

		m_pPlayerModelPanel->SetLOD( m_nCurrentLOD );
	}
	else
	{
		BaseClass::PreviewItem( iClass, pItem, pEntry );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CImportPreviewItemPanel::ResetHandles()
{
	m_pLODComboBox = NULL;
	m_pLoadoutComboBox = NULL;
	m_pPoseComboBox = NULL;
	m_pActionComboBox = NULL;
	m_pEffectComboBox = NULL;
	m_pMaterialButtons.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CImportPreviewItemPanel::OnThink()
{
	// We want to skip CTFStorePreviewItemPanel2::OnThink() because it closes when the mouse is clicked outside the dialog.
	CTFStorePreviewItemPanelBase::OnThink();

	// There's no event when the team color changes, but this is a super cheap call so we'll poll.
	g_PlayerPreviewEffect.SetTeam( GetPreviewTeam() );

	if ( m_flGestureEndTime && gpGlobals->curtime >= m_flGestureEndTime )
	{
		EndGestures();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CImportPreviewItemPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	ResetHandles();

	BaseClass::ApplySchemeSettings( pScheme );

	// We don't use the scrollbar in this view
	if ( m_pScrollBar )
	{
		m_pScrollBar->MarkForDeletion();
		m_pScrollBar = NULL;
	}

	vgui::Button *pButton = FindControl<vgui::Button>( "AddToCartButton", true );
	if ( pButton )
	{
		pButton->SetVisible( false );
	}

	m_pLODComboBox = FindControl<vgui::ComboBox>( "LODComboBox", true );
	if ( m_pLODComboBox )
	{
		m_pLODComboBox->AddActionSignalTarget( this );
	}

	m_pLoadoutComboBox = FindControl<vgui::ComboBox>( "LoadoutComboBox", true );
	if ( m_pLoadoutComboBox )
	{
		m_pLoadoutComboBox->AddActionSignalTarget( this );
	}

	m_pPoseComboBox = FindControl<vgui::ComboBox>( "PoseComboBox", true );
	if ( m_pPoseComboBox )
	{
		m_pPoseComboBox->AddActionSignalTarget( this );
	}

	m_pActionComboBox = FindControl<vgui::ComboBox>( "ActionComboBox", true );
	if ( m_pActionComboBox )
	{
		m_pActionComboBox->AddActionSignalTarget( this );
	}

	m_pEffectComboBox = FindControl<vgui::ComboBox>( "EffectComboBox", true );
	if ( m_pEffectComboBox )
	{
		m_pEffectComboBox->RemoveAll();

		KeyValuesAD pKeyValues( "data" );
		for ( int iEffect = 0; iEffect < C_TFPlayerPreviewEffect::NUM_PREVIEW_EFFECTS; ++iEffect )
		{
			pKeyValues->SetInt( "effect", iEffect );
			int iRow = m_pEffectComboBox->AddItem( CFmtStr( "#TF_ImportPreview_Effect%d", iEffect ), pKeyValues );
			if ( g_PlayerPreviewEffect.GetEffect() == iEffect )
			{
				m_pEffectComboBox->ActivateItemByRow( iRow );
			}
		}

		m_pEffectComboBox->AddActionSignalTarget( this );
	}

	// Default to the character view, because that's the most valuable for preview here
	const CTFItemDefinition *pItemData = m_item.GetItemDefinition();
	if ( pItemData ) 
	{
		for ( int iClass = TF_FIRST_NORMAL_CLASS; iClass < TF_LAST_NORMAL_CLASS; iClass++ )
		{
			if ( !pItemData->CanBeUsedByClass(iClass) )
				continue;

			KeyValuesAD pKeyValues( "data" );
			pKeyValues->SetInt( "class", iClass );
			OnClassIconSelected( pKeyValues );
			break;
		}
	}

	pButton = FindControl<vgui::Button>( "ButtonAction", true );
	if ( pButton )
	{
		pButton->AddActionSignalTarget( this );
	}

	pButton = FindControl<vgui::Button>( "ButtonEditQC", true );
	if ( pButton )
	{
		pButton->AddActionSignalTarget( this );
	}

	int nSkinType = m_pItemValues->GetInt( kSkinType );
	m_pMaterialButtons.SetCount( MAX_MATERIAL_COUNT );
	for ( int i = 0; i < MAX_MATERIAL_COUNT; ++i )
	{
		m_pMaterialButtons[i] = FindControl<vgui::Button>( CFmtStr( "ButtonEditMaterial%d", i ), true );
		if ( m_pMaterialButtons[i] )
		{
			int nSkinIndex = i / NUM_IMPORT_MATERIALS_PER_TEAM;
			int nMaterialIndex = i % NUM_IMPORT_MATERIALS_PER_TEAM;
			const char* pszTeam;
			if ( nSkinType == 0 )
			{
				pszTeam = "ALL";
			}
			else
			{
				pszTeam = nSkinIndex == 0 ? "RED" : "BLU";
			}
			m_pMaterialButtons[i]->SetText( CFmtStr( "Edit VMT%d %s", nMaterialIndex + 1, pszTeam ) );
			m_pMaterialButtons[i]->AddActionSignalTarget( this );
		}
	}

	// Fix up the fullscreen panel with our desired control set
	if ( m_pFullscreenPanel )
	{
		pButton = m_pFullscreenPanel->FindControl<vgui::Button>( "RotateLeftButton", true );
		if ( pButton )
		{
			pButton->SetVisible( false );
		}

		pButton = m_pFullscreenPanel->FindControl<vgui::Button>( "RotateRightButton", true );
		if ( pButton )
		{
			pButton->SetVisible( false );
		}

		pButton = m_pFullscreenPanel->FindControl<vgui::Button>( "ZoomButton", true );
		if ( pButton )
		{
			pButton->SetVisible( false );
		}
	}

	pButton = FindControl<vgui::Button>( "ButtonEditQCI", true );
	if ( pButton )
	{
		pButton->SetVisible( m_bIsTaunt );
	}

	vgui::Label *pLabel = FindControl<vgui::Label>( "AdvancedLabel", true );
	if ( pLabel )
	{
		pLabel->SetVisible( !m_bIsTaunt );
	}
	vgui::Panel *pPanel = FindControl<vgui::Panel>( "AdvancedFrame", true );
	if ( pPanel )
	{
		pPanel->SetVisible( !m_bIsTaunt );
	}

	UpdateActivityUI();
	UpdateMaterialButtons();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CImportPreviewItemPanel::OnCommand( const char *command )
{
	if ( V_strieq( command, "show_explanations" ) )
	{
		CExplanationPopup *pPopup = dynamic_cast<CExplanationPopup*>( FindChildByName("StartExplanation") );
		if ( pPopup )
		{
			pPopup->Popup();
		}
	}
	else if ( V_strieq( command, "action" ) )
	{
		StartAction();
	}
	else if ( V_strieq( command, "BuildPreview" ) ||
			  V_strieq( command, "EditQC" ) ||
			  V_strieq( command, "EditQCI" ) ||
			  V_strncasecmp( command, "EditMaterial", V_strlen( "EditMaterial" ) ) == 0 )
	{
		// Dispatch directly to our parent because the base class tries to run the command through the console interpreter
		GetParent()->OnCommand( command );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void  CImportPreviewItemPanel::OnKeyCodeTyped( KeyCode code )
{
	if ( code == KEY_F5 )
	{
		OnCommand( "BuildPreview" );
		return;
	}

	BaseClass::OnKeyCodeTyped( code );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CImportPreviewItemPanel::PerformLayout( void )
{
	CTFStorePreviewItemPanel2::PerformLayout();

	PlaceControl( m_pDetailsViewChild, NULL, "UsedByLabel", m_iSmallVerticalBreakSize, true );
	PlaceControl( m_pDetailsViewChild, "UsedByLabel", "UsedByTextLabel", m_iHorizontalBreakSize, false );
	PlaceControl( m_pDetailsViewChild, "UsedByLabel", "SlotLabel", m_iSmallVerticalBreakSize, true );
	PlaceControl( m_pDetailsViewChild, "SlotLabel", "SlotTextLabel", m_iHorizontalBreakSize, false );
	PlaceControl( m_pDetailsViewChild, "SlotLabel", "AttributesLabel", m_iBigVerticalBreakSize, true );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CImportPreviewItemPanel::OnClose()
{
	GetParent()->OnCommand( "PreviewDone" );
	MarkForDeletion();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CImportPreviewItemPanel::UpdatePlayerModelButtons()
{
	BaseClass::UpdatePlayerModelButtons();

	UpdateActivityUI();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CImportPreviewItemPanel::UpdateActivityUI()
{
	if ( m_pLODComboBox )
	{
		m_pLODComboBox->RemoveAll();
		m_pLODComboBox->SetText( "" );

		Assert( m_nSelectedClass != TF_CLASS_UNDEFINED );
		if ( m_iState == PS_PLAYER )
		{
			KeyValuesAD pKeyValues( "data" );
			int nDefaultRow = -1;
			int nLastValidLOD = 0;
			for ( int nLOD = 0; nLOD < NUM_IMPORT_LODS; ++nLOD )
			{
				KeyValues *pKey = m_pItemValues->FindKey( CFmtStr( kClassLODN, kClassFolders[ m_nSelectedClass ], nLOD ) );
				if ( pKey && *pKey->GetString( "file" ) )
				{
					nLastValidLOD = nLOD;
				}

				pKeyValues->SetInt( "LOD", nLOD );
				int nRow = m_pLODComboBox->AddItem( CFmtStr( "LOD%d", nLOD ), pKeyValues );
				if ( m_nCurrentLOD == nLOD )
				{
					nDefaultRow = nRow;
				}
			}
			Assert( nDefaultRow >= 0 );
			m_pLODComboBox->ActivateItemByRow( nDefaultRow );
		}
	}

	if ( m_pLoadoutComboBox )
	{
		m_pLoadoutComboBox->RemoveAll();
		m_pLoadoutComboBox->SetText( "" );

		if ( m_iState == PS_PLAYER )
		{
			KeyValuesAD pKeyValues( "data" );
			int nDefaultRow = -1;
			loadout_positions_t nFirstLoadout = LOADOUT_POSITION_INVALID;
			for ( int iLoadoutPosition = 0; iLoadoutPosition < CLASS_LOADOUT_POSITION_COUNT; ++iLoadoutPosition )
			{
				CEconItemView *pItem = TFInventoryManager()->GetBaseItemForClass( m_iCurrentClass, iLoadoutPosition );
				if ( pItem && pItem->IsValid() )
				{
					const char *pszLabel = ItemSystem()->GetItemSchema()->GetLoadoutStringsForDisplay( pItem->GetItemDefinition()->GetEquipType() )[ iLoadoutPosition ];
					pKeyValues->SetInt( "loadout", iLoadoutPosition );
					int nRow = m_pLoadoutComboBox->AddItem( pszLabel, pKeyValues );
					if ( iLoadoutPosition == m_nCurrentLoadout )
					{
						nDefaultRow = nRow;
					}
					if ( nFirstLoadout == LOADOUT_POSITION_INVALID )
					{
						nFirstLoadout = (loadout_positions_t)iLoadoutPosition;
					}
				}
			}
			Assert(nFirstLoadout != LOADOUT_POSITION_INVALID);

			if (nDefaultRow < 0)
			{
				// Didn't find our current loadout position, pick the first one
				nDefaultRow = 0;
				m_nCurrentLoadout = nFirstLoadout;
			}
			m_pLoadoutComboBox->ActivateItemByRow( nDefaultRow );
		}
	}

	if ( m_pPoseComboBox )
	{
		m_pPoseComboBox->RemoveAll();
		m_pPoseComboBox->SetText( "" );

		if ( m_iState == PS_PLAYER )
		{
			KeyValuesAD pKeyValues( "data" );
			int nDefaultRow = -1;
			for ( int iPose = 0; iPose < 