//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: EconItemSchema: Defines a schema for econ items
//
//=============================================================================

#include "cbase.h"
#include "econ_item_schema.h"
#include "tier1/fmtstr.h"
#include "tier1/UtlSortVector.h"
#include "tier2/tier2.h"
#include "filesystem.h"
#include "schemainitutils.h"
#include "gcsdk/gcsdk_auto.h"
#include "rtime.h"
#include "item_selection_criteria.h"
#include "crypto.h"
#include "checksum_sha1.h"

#include <google/protobuf/text_format.h>
#include <string.h>

#include "materialsystem/imaterialsystem.h"
#include "materialsystem/itexture.h"
#include "materialsystem/itexturecompositor.h"

#if ( defined( _MSC_VER ) && _MSC_VER >= 1900 )
#define timezone _timezone
#define daylight _daylight
#endif

// For holiday-limited loot lists.
#include "econ_holidays.h"

// Only used for startup testing.
#include "econ_item_tools.h"

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	#include "econ_item_system.h"
	#include "econ_item.h"
	#include "activitylist.h"

	#if defined(TF_CLIENT_DLL) || defined(TF_DLL)
		#include "tf_gcmessages.h"
	#endif
#endif

#ifdef GC_DLL
#include "gcgamebase.h"
#include <memory>					// unique_ptr
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace GCSDK;


CEconItemSchema & GEconItemSchema()
{
#if defined( EXTERNALTESTS_DLL )
	static CEconItemSchema g_econItemSchema;
	return g_econItemSchema;
#elif defined( GC_DLL )
	return *GEconManager()->GetItemSchema();
#else
	return *ItemSystem()->GetItemSchema();
#endif
}

const char *g_szDropTypeStrings[] =
{
	"",		 // Blank and none mean the same thing: stay attached to the body.
	"none",
	"drop",	 // The item drops off the body.
	"break", // Not implemented, but an example of a type that could be added.
};

const char *g_TeamVisualSections[TEAM_VISUAL_SECTIONS] = 
{
	"visuals",		// TF_TEAM_UNASSIGNED. Visual changes applied to both teams.
	NULL,			// TF_TEAM_SPECTATOR. Unused.
	"visuals_red",	// TF_TEAM_RED
	"visuals_blu",	// TF_TEAM_BLUE
	"visuals_mvm_boss",	// Hack to override things in MvM at a general level
};

int GetTeamVisualsFromString( const char *pszString )
{
	for ( int i = 0; i < TEAM_VISUAL_SECTIONS; i++ )
	{
		// There's a NULL hidden in g_TeamVisualSections
		if ( g_TeamVisualSections[i] && V_strieq( pszString, g_TeamVisualSections[i] ) )
			return i;
	}
	return -1;
}

#if defined(CLIENT_DLL) || defined(GAME_DLL)
// Used to convert strings to ints for wearable animation types
const char *g_WearableAnimTypeStrings[ NUM_WAP_TYPES ] =
{
	"on_spawn",			// WAP_ON_SPAWN,
	"start_building",	// WAP_START_BUILDING,
	"stop_building",	// WAP_STOP_BUILDING,
	"start_taunting",		// WAP_START_TAUNTING,
	"stop_taunting",	// WAP_STOP_TAUNTING,
};
#endif

const char *g_AttributeDescriptionFormats[] =
{
	"value_is_percentage",				// ATTDESCFORM_VALUE_IS_PERCENTAGE,
	"value_is_inverted_percentage",		// ATTDESCFORM_VALUE_IS_INVERTED_PERCENTAGE
	"value_is_additive",				// ATTDESCFORM_VALUE_IS_ADDITIVE
	"value_is_additive_percentage",		// ATTDESCFORM_VALUE_IS_ADDITIVE_PERCENTAGE
	"value_is_or",						// ATTDESCFORM_VALUE_IS_OR
	"value_is_date",					// ATTDESCFORM_VALUE_IS_DATE
	"value_is_account_id",				// ATTDESCFORM_VALUE_IS_ACCOUNT_ID
	"value_is_particle_index",			// ATTDESCFORM_VALUE_IS_PARTICLE_INDEX -> Could change to "string index"
	"value_is_killstreakeffect_index",	// ATTDESCFORM_VALUE_IS_KILLSTREAKEFFECT_INDEX -> Could change to "string index"
	"value_is_killstreak_idleeffect_index",  // ATTDESCFORM_VALUE_IS_KILLSTREAK_IDLEEFFECT_INDEX
	"value_is_item_def",				// ATTDESCFORM_VALUE_IS_ITEM_DEF
	"value_is_from_lookup_table",		// ATTDESCFORM_VALUE_IS_FROM_LOOKUP_TABLE
};

const char *g_EffectTypes[NUM_EFFECT_TYPES] =
{
	"unusual",		// ATTRIB_EFFECT_UNUSUAL,
	"strange",		// ATTRIB_EFFECT_STRANGE,
	"neutral",		// ATTRIB_EFFECT_NEUTRAL = 0,
	"positive",		// ATTRIB_EFFECT_POSITIVE,
	"negative",		// ATTRIB_EFFECT_NEGATIVE,
};

//-----------------------------------------------------------------------------
// Purpose: Set the capabilities bitfield based on whether the entry is true/false.
//-----------------------------------------------------------------------------
const char *g_Capabilities[] =
{
	"paintable",				// ITEM_CAP_PAINTABLE
	"nameable",					// ITEM_CAP_NAMEABLE
	"decodable",				// ITEM_CAP_DECODABLE
	"can_craft_if_purchased",	// ITEM_CAP_CAN_BE_CRAFTED_IF_PURCHASED
	"can_customize_texture",	// ITEM_CAP_CAN_CUSTOMIZE_TEXTURE
	"usable",					// ITEM_CAP_USABLE
	"usable_gc",				// ITEM_CAP_USABLE_GC
	"can_gift_wrap",			// ITEM_CAP_CAN_GIFT_WRAP
	"usable_out_of_game",		// ITEM_CAP_USABLE_OUT_OF_GAME
	"can_collect",				// ITEM_CAP_CAN_COLLECT
	"can_craft_count",			// ITEM_CAP_CAN_CRAFT_COUNT
	"can_craft_mark",			// ITEM_CAP_CAN_CRAFT_MARK
	"paintable_team_colors",	// ITEM_CAP_PAINTABLE_TEAM_COLORS
	"can_be_restored",			// ITEM_CAP_CAN_BE_RESTORED
	"strange_parts",			// ITEM_CAP_CAN_USE_STRANGE_PARTS
	"can_card_upgrade",			// ITEM_CAP_CAN_CARD_UPGRADE
	"can_strangify",			// ITEM_CAP_CAN_STRANGIFY
	"can_killstreakify",		// ITEM_CAP_CAN_KILLSTREAKIFY
	"can_consume",				// ITEM_CAP_CAN_CONSUME_ITEMS
	"can_spell_page",			// ITEM_CAP_CAN_SPELLBOOK_PAGE
	"has_slots",				// ITEM_CAP_HAS_SLOTS
	"duck_upgradable",			// ITEM_CAP_DUCK_UPGRADABLE
	"can_unusualify",			// ITEM_CAP_CAN_UNUSUALIFY
};
COMPILE_TIME_ASSERT( SE_ARRAYSIZE(g_Capabilities) == NUM_ITEM_CAPS );

#define RETURN_ATTRIBUTE_STRING( attrib_name, default_string ) \
	static CSchemaAttributeDefHandle pAttribString( attrib_name ); \
	const char *pchResultAttribString = default_string; \
	FindAttribute_UnsafeBitwiseCast< CAttribute_String >( this, pAttribString, &pchResultAttribString ); \
	return pchResultAttribString;

#define RETURN_ATTRIBUTE_STRING_F( func_name, attrib_name, default_string ) \
	const char *func_name( void ) const { RETURN_ATTRIBUTE_STRING( attrib_name, default_string ) }

static void ParseCapability( item_capabilities_t &capsBitfield, KeyValues* pEntry )
{
	int idx = StringFieldToInt(  pEntry->GetName(), g_Capabilities, SE_ARRAYSIZE(g_Capabilities) );
	if ( idx < 0 )
	{
		return;
	}
	int bit = 1 << idx;
	if ( pEntry->GetBool() )
	{
		(int&)capsBitfield |= bit;
	}
	else
	{
		(int&)capsBitfield &= ~bit;
	}
}

#ifdef GC_DLL
static bool BGetPaymentRule( KeyValues *pKVRule, EPaymentRuleType *out_pePaymentRuleType, double *out_pRevenueShare )
{
	Assert( out_pePaymentRuleType );
	Assert( out_pRevenueShare );

	struct payment_rule_lookup_t
	{
		const char *m_pszStr;
		EPaymentRuleType m_eRuleType;
	};

	static payment_rule_lookup_t s_Lookup[] =
	{
		{ "workshop_revenue_share",		kPaymentRule_SteamWorkshopFileID },
		{ "partner_revenue_share",		kPaymentRule_PartnerSteamID },
		{ "bundle_revenue_share",		kPaymentRule_Bundle },
	};

	for ( int i = 0; i < SE_ARRAYSIZE( s_Lookup ); i++ )
	{
		KeyValues *pKVKey = pKVRule->FindKey( s_Lookup[i].m_pszStr );
		if ( !pKVKey )
			continue;

		*out_pePaymentRuleType = s_Lookup[i].m_eRuleType;
		*out_pRevenueShare = atof( pKVKey->GetString() ) * 100.0;				// KeyValues doesn't support parsing a string as a double-precision value, so we do it by hand
		return true;
	}

	return false;
}
#endif // GC_DLL

//-----------------------------------------------------------------------------
// Purpose: CEconItemSeriesDefinition
//-----------------------------------------------------------------------------
CEconItemSeriesDefinition::CEconItemSeriesDefinition( void )
	: m_nValue( INT_MAX )
{
}


//-----------------------------------------------------------------------------
// Purpose:	Copy constructor
//-----------------------------------------------------------------------------
CEconItemSeriesDefinition::CEconItemSeriesDefinition( const CEconItemSeriesDefinition &that )
{
	( *this ) = that;
}


//-----------------------------------------------------------------------------
// Purpose:	Operator=
//-----------------------------------------------------------------------------
CEconItemSeriesDefinition &CEconItemSeriesDefinition::operator=( const CEconItemSeriesDefinition &rhs )
{
	m_nValue = rhs.m_nValue;
	m_strName = rhs.m_strName;

	return *this;
}


//-----------------------------------------------------------------------------
// Purpose:	Initialize the quality definition
// Input:	pKVQuality - The KeyValues representation of the quality
//			schema - The overall item schema for this attribute
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemSeriesDefinition::BInitFromKV( KeyValues *pKVSeries, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{

	m_nValue = pKVSeries->GetInt( "value", -1 );
	m_strName = pKVSeries->GetName();
	
	m_strLockKey = pKVSeries->GetString( "loc_key" );
	m_strUiFile = pKVSeries->GetString( "ui" );

	// Check for required fields
	SCHEMA_INIT_CHECK(
		NULL != pKVSeries->FindKey( "value" ),
		"Quality definition %s: Missing required field \"value\"", pKVSeries->GetName() );

	return SCHEMA_INIT_SUCCESS();
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CEconItemQualityDefinition::CEconItemQualityDefinition( void )
:	m_nValue( INT_MAX )
,	m_bCanSupportSet( false )
{
}


//-----------------------------------------------------------------------------
// Purpose:	Copy constructor
//-----------------------------------------------------------------------------
CEconItemQualityDefinition::CEconItemQualityDefinition( const CEconItemQualityDefinition &that )
{
	(*this) = that;
}


//-----------------------------------------------------------------------------
// Purpose:	Operator=
//-----------------------------------------------------------------------------
CEconItemQualityDefinition &CEconItemQualityDefinition::operator=( const CEconItemQualityDefinition &rhs )
{
	m_nValue = rhs.m_nValue; 
	m_strName =	rhs.m_strName; 
	m_bCanSupportSet = rhs.m_bCanSupportSet;

	return *this;
}


//-----------------------------------------------------------------------------
// Purpose:	Initialize the quality definition
// Input:	pKVQuality - The KeyValues representation of the quality
//			schema - The overall item schema for this attribute
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemQualityDefinition::BInitFromKV( KeyValues *pKVQuality, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{

	m_nValue = pKVQuality->GetInt( "value", -1 );
	m_strName = pKVQuality->GetName();	
	m_bCanSupportSet = pKVQuality->GetBool( "canSupportSet" );
#ifdef GC_DLL
	m_strHexColor = pKVQuality->GetString( "hexColor" );
#endif // GC_DLL

	// Check for required fields
	SCHEMA_INIT_CHECK( 
		NULL != pKVQuality->FindKey( "value" ), 
		"Quality definition %s: Missing required field \"value\"", pKVQuality->GetName() );

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	return SCHEMA_INIT_SUCCESS();
#endif // GC_DLL

	// Check for data consistency
	SCHEMA_INIT_CHECK( 
		0 != Q_stricmp( GetName(), "any" ), 
		"Quality definition any: The quality name \"any\" is a reserved keyword and cannot be used." );

	SCHEMA_INIT_CHECK( 
		m_nValue != k_unItemQuality_Any, 
		"Quality definition %s: Invalid value (%d). It is reserved for Any", GetName(), k_unItemQuality_Any );

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// CEconItemRarityDefinition
//-----------------------------------------------------------------------------
CEconItemRarityDefinition::CEconItemRarityDefinition( void )
	: m_nValue( INT_MAX )
	, m_nLootlistWeight( 0 )
{
}

//-----------------------------------------------------------------------------
// Purpose:	Initialize the rarity definition
//-----------------------------------------------------------------------------
bool CEconItemRarityDefinition::BInitFromKV( KeyValues *pKVRarity, KeyValues *pKVRarityWeights, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	m_nValue = pKVRarity->GetInt( "value", -1 );
	m_strName = pKVRarity->GetName();
	m_strLocKey = pKVRarity->GetString( "loc_key" );
	m_strWepLocKey = pKVRarity->GetString( "loc_key_weapon" );

	m_iAttribColor = GetAttribColorIndexForName( pKVRarity->GetString( "color" ) );
	m_strDropSound = pKVRarity->GetString( "drop_sound" );
	m_strNextRarity = pKVRarity->GetString( "next_rarity" ); // Not required.

#ifdef GC_DLL
	if ( pKVRarityWeights )
	{
		m_nLootlistWeight = pKVRarityWeights->GetInt( m_strName, 0 );
	}
#endif
	//

	// Check for required fields
	SCHEMA_INIT_CHECK(
		NULL != pKVRarity->FindKey( "value" ),
		"Rarity definition %s: Missing required field \"value\"", pKVRarity->GetName() );

	SCHEMA_INIT_CHECK(
		NULL != pKVRarity->FindKey( "loc_key" ),
		"Rarity definition %s: Missing required field \"loc_key\"", pKVRarity->GetName() );

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconColorDefinition::BInitFromKV( KeyValues *pKVColor, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	m_strName		= pKVColor->GetName();
	m_strColorName	= pKVColor->GetString( "color_name" );
#ifdef GC_DLL
	m_strHexColor	= pKVColor->GetString( "hex_color" );
#endif // GC_DLL

	SCHEMA_INIT_CHECK(
		!m_strColorName.IsEmpty(),
		"Quality definition %s: missing \"color_name\"", GetName() );

#ifdef GC_DLL
	SCHEMA_INIT_CHECK(
		!m_strHexColor.IsEmpty(),
		"Quality definition %s: missing \"hex_color\"", GetName() );
#endif // GC_DLL

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CEconItemSetDefinition::CEconItemSetDefinition( void )
	: m_pszName( NULL )
	, m_pszLocalizedName( NULL )
	, m_iBundleItemDef( INVALID_ITEM_DEF_INDEX )
	, m_bIsHiddenSet( false )
{
}

//-----------------------------------------------------------------------------
// Purpose:	Copy constructor
//-----------------------------------------------------------------------------
CEconItemSetDefinition::CEconItemSetDefinition( const CEconItemSetDefinition &that )
{
	(*this) = that;
}

//-----------------------------------------------------------------------------
// Purpose:	Operator=
//-----------------------------------------------------------------------------
CEconItemSetDefinition &CEconItemSetDefinition::operator=( const CEconItemSetDefinition &other )
{
	m_pszName = other.m_pszName;
	m_pszLocalizedName = other.m_pszLocalizedName;
	m_iItemDefs = other.m_iItemDefs;
	m_iAttributes = other.m_iAttributes;
	m_iBundleItemDef = other.m_iBundleItemDef;
	m_bIsHiddenSet = other.m_bIsHiddenSet;

	return *this;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CEconItemSetDefinition::BInitFromKV( KeyValues *pKVItemSet, CUtlVector<CUtlString> *pVecErrors )
{
	m_pszName = pKVItemSet->GetName();

	m_iBundleItemDef = INVALID_ITEM_DEF_INDEX;
	const char *pszBundleName = pKVItemSet->GetString( "store_bundle" );
	if ( pszBundleName && pszBundleName[0] )
	{
		CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( pszBundleName );
		if ( pDef )
		{
			m_iBundleItemDef = pDef->GetDefinitionIndex();
		}

		SCHEMA_INIT_CHECK( 
			pDef != NULL,
			"Item set %s: Bundle definition \"%s\" was not found", m_pszName, pszBundleName );
	}

	m_pszLocalizedName = pKVItemSet->GetString( "name", NULL );
	m_bIsHiddenSet = pKVItemSet->GetBool( "is_hidden_set", false );

	KeyValues *pKVItems = pKVItemSet->FindKey( "items" );
	if ( pKVItems )
	{
		FOR_EACH_SUBKEY( pKVItems, pKVItem )
		{
			const char *pszName = pKVItem->GetName();

			CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( pszName );

			SCHEMA_INIT_CHECK( 
				pDef != NULL,
				"Item set %s: Item definition \"%s\" was not found", m_pszName, pszName );

			const item_definition_index_t unDefIndex = pDef->GetDefinitionIndex();

			SCHEMA_INIT_CHECK(
				!m_iItemDefs.IsValidIndex( m_iItemDefs.Find( unDefIndex ) ),
				"Item set %s: item definition \"%s\" appears multiple times", m_pszName, pszName );
			SCHEMA_INIT_CHECK(
				!pDef->GetItemSetDefinition(),
				"Item set %s: item definition \"%s\" specified in multiple item sets", m_pszName, pszName );

			m_iItemDefs.AddToTail( unDefIndex );
			pDef->SetItemSetDefinition( this );

			// FIXME: hack to work around crafting item criteria
			pDef->GetRawDefinition()->SetString( "item_set", m_pszName );
		}
	}

	KeyValues *pKVAttributes = pKVItemSet->FindKey( "attributes" );
	if ( pKVAttributes )
	{
		FOR_EACH_SUBKEY( pKVAttributes, pKVAttribute )
		{
			const char *pszName = pKVAttribute->GetName();

			const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinitionByName( pszName );
			SCHEMA_INIT_CHECK( 
				pAttrDef != NULL,
				"Item set %s: Attribute definition \"%s\" was not found", m_pszName, pszName );
			SCHEMA_INIT_CHECK(
				pAttrDef->BIsSetBonusAttribute(),
				"Item set %s: Attribute definition \"%s\" is not a set bonus attribute", m_pszName, pszName );

			int iIndex = m_iAttributes.AddToTail();
			m_iAttributes[iIndex].m_iAttribDefIndex = pAttrDef->GetDefinitionIndex();
			m_iAttributes[iIndex].m_flValue = pKVAttribute->GetFloat( "value" );
		}
	}

	// Sanity check.
	SCHEMA_INIT_CHECK( m_pszLocalizedName != NULL,
		"Item set %s: Set contains no localized name", m_pszName );
	SCHEMA_INIT_CHECK( m_iItemDefs.Count() > 0,
		"Item set %s: Set contains no items", m_pszName );

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CEconItemSetDefinition::IterateAttributes( class IEconItemAttributeIterator *pIterator ) const
{
	FOR_EACH_VEC( m_iAttributes, i )
	{
		const itemset_attrib_t& itemsetAttrib = m_iAttributes[i];

		const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinition( itemsetAttrib.m_iAttribDefIndex );
		if ( !pAttrDef )
			continue;

		const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
		Assert( pAttrType );

		// We know (and assert) that we only need 32 bits of data to store this attribute
		// data. We don't know anything about the type but we'll let the type handle it
		// below.
		attribute_data_union_t value;
		value.asFloat = itemsetAttrib.m_flValue;

		if ( !pAttrType->OnIterateAttributeValue( pIterator, pAttrDef, value ) )
			return;
	}
}

//-----------------------------------------------------------------------------
CEconItemCollectionDefinition::CEconItemCollectionDefinition( void )
	: m_pszName( NULL )
	, m_pszLocalizedName( NULL )
	, m_pszLocalizedDesc( NULL )
	, m_iRarityMin( k_unItemRarity_Any )
	, m_iRarityMax( k_unItemRarity_Any )
{
}

//-----------------------------------------------------------------------------
// 

//-----------------------------------------------------------------------------
static int SortCollectionByRarity( item_definition_index_t const *a, item_definition_index_t const *b )
{
	Assert( a );
	Assert( *a );
	Assert( b );
	Assert( *b );

	CEconItemDefinition *pItemA = GetItemSchema()->GetItemDefinition( *a );
	CEconItemDefinition *pItemB = GetItemSchema()->GetItemDefinition( *b );

	if ( !pItemA || !pItemB )
	{
		AssertMsg( 0, "ItemDef Doesn't exist for sorting" );
		return 1;
	}
	
	// If same Rarity, leave in current position?
	if ( pItemA->GetRarity() == pItemB->GetRarity() && pItemA->GetCustomPainkKitDefinition() && pItemB->GetCustomPainkKitDefinition() )
	{
#ifdef CLIENT_DLL
		// Sort by localized name
		// paintkits sort by paintkit name
		auto paintkitA = pItemA->GetCustomPainkKitDefinition();
		auto paintkitB = pItemB->GetCustomPainkKitDefinition();
		auto paintkitALocName = paintkitA->GetLocalizeName();
		auto paintkitBLocName = paintkitB->GetLocalizeName();
		auto pkALocalized = g_pVGuiLocalize->Find( paintkitALocName );
		auto pkBLocalized = g_pVGuiLocalize->Find( paintkitBLocName );
		if ( pkALocalized )
		{
			if ( pkBLocalized )
			{
				return V_wcscmp( pkALocalized, pkBLocalized );
			}
			else
			{
				return -1;
			}
		}
		else
		{
			return pkBLocalized ? 1 : -1;
		}
#else
		return 0;
#endif
	}

	return ( pItemA->GetRarity() > pItemB->GetRarity() ) ? -1 : 1;
}


//-----------------------------------------------------------------------------
bool CEconItemCollectionDefinition::BInitFromKV( KeyValues *pKVPItemCollection, CUtlVector<CUtlString> *pVecErrors )
{
	m_pszName = pKVPItemCollection->GetName();

	m_pszLocalizedName = pKVPItemCollection->GetString( "name", NULL );
	m_pszLocalizedDesc = pKVPItemCollection->GetString( "description", NULL );

	m_bIsReferenceCollection = pKVPItemCollection->GetBool( "is_reference_collection", false );

	KeyValues *pKVItems = pKVPItemCollection->FindKey( "items" );

	// Create a 'lootlist' from this collection
	KeyValues *pCollectionLootList = NULL;
	bool bIsLootList = false;
	if ( !m_bIsReferenceCollection )
	{
		pCollectionLootList = new KeyValues( m_pszName );
	}

	if ( pKVItems )
	{
		// Traverse rarity items and set rarity
		// Create a lootlist if applicable
		FOR_EACH_TRUE_SUBKEY( pKVItems, pKVRarity )
		{
			bIsLootList = true;
			// Get the Rarity Value
			const CEconItemRarityDefinition *pRarity = GetItemSchema()->GetRarityDefinitionByName( pKVRarity->GetName() );
			SCHEMA_INIT_CHECK( pRarity != NULL, "Item collection %s: Rarity type \"%s\" was not found", m_pszName, pKVRarity->GetName() );
			
			// Create a lootlist
			if ( !m_bIsReferenceCollection )
			{
				CFmtStr lootlistname( "%s_%s", m_pszName, pRarity->GetName() );
				const char *pszName = V_strdup( lootlistname.Get() );
				pKVRarity->SetInt( "rarity", pRarity->GetDBValue() );
				SCHEMA_INIT_CHECK( GetItemSchema()->BInsertLootlist( pszName, pKVRarity, pVecErrors ), "Invalid collection lootlist %s", pszName );
				KeyValues *pTempRarityKey = pKVRarity->FindKey( "rarity" );
				Assert( pTempRarityKey );
				pKVRarity->RemoveSubKey( pTempRarityKey );
				pTempRarityKey->deleteThis();
				pCollectionLootList->SetInt( pszName, pRarity->GetLootlistWeight() );
			}

			// Items in the Rarity
			FOR_EACH_VALUE( pKVRarity, pKVItem )
			{
				const char *pszName = pKVItem->GetName();

				CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( pszName );

				SCHEMA_INIT_CHECK(
					pDef != NULL,
					"Item set %s: Item definition \"%s\" was not found", m_pszName, pszName );

				const item_definition_index_t unDefIndex = pDef->GetDefinitionIndex();

				SCHEMA_INIT_CHECK(
					!m_iItemDefs.IsValidIndex( m_iItemDefs.Find( unDefIndex ) ),
					"Item Collection %s: item definition \"%s\" appears multiple times", m_pszName, pszName );

				m_iItemDefs.AddToTail( unDefIndex );

				// Collection Reference
				if ( !m_bIsReferenceCollection )
				{
					SCHEMA_INIT_CHECK(
						!pDef->GetItemCollectionDefinition(),
						"Item Collection %s: item definition \"%s\" specified in multiple item sets", m_pszName, pszName );
					pDef->SetItemCollectionDefinition( this );
				}

				// Item Rarity
				pDef->SetRarity( pRarity->GetDBValue() );
			}
		}

		// Loose Items
		FOR_EACH_VALUE( pKVItems, pKVItem )
		{
			const char *pszName = pKVItem->GetName();

			CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( pszName );

			SCHEMA_INIT_CHECK(
				pDef != NULL,
				"Item set %s: Item definition \"%s\" was not found", m_pszName, pszName );

			const item_definition_index_t unDefIndex = pDef->GetDefinitionIndex();

			SCHEMA_INIT_CHECK(
				!m_iItemDefs.IsValidIndex( m_iItemDefs.Find( unDefIndex ) ),
				"Item Collection %s: item definition \"%s\" appears multiple times", m_pszName, pszName );
			
			m_iItemDefs.AddToTail( unDefIndex );

			if ( !m_bIsReferenceCollection )
			{
				SCHEMA_INIT_CHECK(
					!pDef->GetItemCollectionDefinition(),
					"Item Collection %s: item definition \"%s\" specified in multiple item sets", m_pszName, pszName );
				pDef->SetItemCollectionDefinition( this );
			}
		}

		// Sort by Rarity
		m_iItemDefs.Sort( &SortCollectionByRarity );
	}

	if ( !m_bIsReferenceCollection && bIsLootList )
	{
		// Insert collection lootlist
		GetItemSchema()->BInsertLootlist( m_pszName, pCollectionLootList, pVecErrors );
	}

	if ( pCollectionLootList )
	{
		pCollectionLootList->deleteThis();
	}

	// Sorted high to low
	m_iRarityMax = GetItemSchema()->GetItemDefinition( m_iItemDefs[ 0 ] )->GetRarity();
	m_iRarityMin = GetItemSchema()->GetItemDefinition( m_iItemDefs[ m_iItemDefs.Count() - 1] )->GetRarity();
	// Verify that there is no gaps in the Rarity (would cause crafting problems and makes no sense)

	if ( !m_bIsReferenceCollection )
	{
		int iRarityVerify = m_iRarityMax;
		FOR_EACH_VEC( m_iItemDefs, i )
		{
			int iNextRarity = GetItemSchema()->GetItemDefinition( m_iItemDefs[i] )->GetRarity();
			SCHEMA_INIT_CHECK( iRarityVerify - iNextRarity <= 1, "Items in Collection %s: Have a gap in rarity tiers", m_pszName );
			iRarityVerify = iNextRarity;
		}
	}

	// Sanity check.
	SCHEMA_INIT_CHECK( m_pszLocalizedName != NULL,
		"Item Collection %s: Collection contains no localized name", m_pszName );
	SCHEMA_INIT_CHECK( m_pszLocalizedDesc != NULL,
		"Item Collection %s: Collection contains no localized description", m_pszName );
	SCHEMA_INIT_CHECK( m_iItemDefs.Count() > 0,
		"Item Collection %s: Collection contains no items", m_pszName );

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// CEconItemPaintKitDefinition
//-----------------------------------------------------------------------------
CEconItemPaintKitDefinition::CEconItemPaintKitDefinition( void )
	: m_pszName( NULL )
{
}

//-----------------------------------------------------------------------------
CEconItemPaintKitDefinition::~CEconItemPaintKitDefinition( void )
{
	FOR_EACH_VEC( m_vecPaintKitWearKVP, i )
	{
		if ( m_vecPaintKitWearKVP[i] )
		{
			m_vecPaintKitWearKVP[i]->deleteThis();
		}
	}

	m_vecPaintKitWearKVP.Purge();
}

bool VerifyPaintKitComposite( KeyValues *pKVWearInputItems, const char* pName, int iWearLevel, CUtlVector<CUtlString> *pVecErrors )
{
	SCHEMA_INIT_CHECK( pKVWearInputItems != NULL, "Paint Kit %s: Does not contain Wear Level %d", pName, iWearLevel );
#ifdef CLIENT_DLL
	int w = 1;
	int h = 1;
	int seed = 0;
	ITextureCompositor* pWeaponSkinBaseCompositor = NULL;

	SafeAssign( &pWeaponSkinBaseCompositor, materials->NewTextureCompositor( w, h, pName, TF_TEAM_RED, seed, pKVWearInputItems, TEX_COMPOSITE_CREATE_FLAGS_VERIFY_SCHEMA_ONLY ) );
	SCHEMA_INIT_CHECK( pWeaponSkinBaseCompositor != NULL, "Could Not Create Weapon Skin Compositor for [%s][Wear %d][Team Red]", pName, iWearLevel);
	SafeRelease( &pWeaponSkinBaseCompositor );

	SafeAssign( &pWeaponSkinBaseCompositor, materials->NewTextureCompositor( w, h, pName, TF_TEAM_BLUE, seed, pKVWearInputItems, TEX_COMPOSITE_CREATE_FLAGS_VERIFY_SCHEMA_ONLY ) );
	SCHEMA_INIT_CHECK( pWeaponSkinBaseCompositor != NULL, "Could Not Create Weapon Skin Compositor for [%s][Wear %d][Team BLUE]", pName, iWearLevel );
	SafeRelease( &pWeaponSkinBaseCompositor );
#endif // CLIENT_DLL
	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
bool CEconItemPaintKitDefinition::BInitFromKV( KeyValues *pKVPItemPaintKit, CUtlVector<CUtlString> *pVecErrors )
{
	m_pszName = pKVPItemPaintKit->GetName();
	m_pszLocalizedName = m_pszName;		// localization key is same as paintkit name for ease of generation

	SCHEMA_INIT_CHECK( m_pszLocalizedName != NULL, "Paint Kit %s: PaintKit contains no localized name", m_pszName );

	KeyValues *pKVWearInputItems = NULL;

	pKVWearInputItems = pKVPItemPaintKit->FindKey( "wear_level_1", false );
	SCHEMA_INIT_CHECK( VerifyPaintKitComposite( pKVWearInputItems, m_pszName, 1, pVecErrors ), "Could Not Create Weapon Skin Compositor for [%s][Wear %d]", m_pszName, 1 );
	m_vecPaintKitWearKVP.AddToTail( pKVWearInputItems->MakeCopy() );
	
	pKVWearInputItems = pKVPItemPaintKit->FindKey( "wear_level_2", false );
	SCHEMA_INIT_CHECK( VerifyPaintKitComposite( pKVWearInputItems, m_pszName, 2, pVecErrors ), "Could Not Create Weapon Skin Compositor for [%s][Wear %d]", m_pszName, 2 );
	m_vecPaintKitWearKVP.AddToTail( pKVWearInputItems->MakeCopy() );

	pKVWearInputItems = pKVPItemPaintKit->FindKey( "wear_level_3", false );
	SCHEMA_INIT_CHECK( VerifyPaintKitComposite( pKVWearInputItems, m_pszName, 3, pVecErrors ), "Could Not Create Weapon Skin Compositor for [%s][Wear %d]", m_pszName, 3 );
	m_vecPaintKitWearKVP.AddToTail( pKVWearInputItems->MakeCopy() );

	pKVWearInputItems = pKVPItemPaintKit->FindKey( "wear_level_4", false );
	SCHEMA_INIT_CHECK( VerifyPaintKitComposite( pKVWearInputItems, m_pszName, 4, pVecErrors ), "Could Not Create Weapon Skin Compositor for [%s][Wear %d]", m_pszName, 4 );
	m_vecPaintKitWearKVP.AddToTail( pKVWearInputItems->MakeCopy() );

	pKVWearInputItems = pKVPItemPaintKit->FindKey( "wear_level_5", false );
	SCHEMA_INIT_CHECK( VerifyPaintKitComposite( pKVWearInputItems, m_pszName, 5, pVecErrors ), "Could Not Create Weapon Skin Compositor for [%s][Wear %d]", m_pszName, 5 );
	m_vecPaintKitWearKVP.AddToTail( pKVWearInputItems->MakeCopy() );

	return SCHEMA_INIT_SUCCESS();
}

KeyValues *CEconItemPaintKitDefinition::GetPaintKitWearKV( int nWear )
{
	// Wear is 1-5 but this vec is 0-4
	int iIndex = nWear - 1;

	if ( !m_vecPaintKitWearKVP.IsValidIndex( iIndex ) )
	{
		iIndex = 0;
		Assert( m_vecPaintKitWearKVP.IsValidIndex( iIndex ) );
		DevMsg( "Invalid Paint Kit or Paint Kit Wear Entry (%s) Wear (%d).", m_pszName, nWear );
		return NULL;
	}

	return m_vecPaintKitWearKVP[iIndex];
}

//-----------------------------------------------------------------------------
// CEconOperationDefinition
//-----------------------------------------------------------------------------
CEconOperationDefinition::CEconOperationDefinition( void )
	: m_pszName( NULL )
	, m_unRequiredItemDefIndex( INVALID_ITEM_DEF_INDEX )
	, m_unGatewayItemDefIndex( INVALID_ITEM_DEF_INDEX )
{
}

//-----------------------------------------------------------------------------
CEconOperationDefinition::~CEconOperationDefinition( void )
{
	if ( m_pKVItem )
		m_pKVItem->deleteThis();
	m_pKVItem = NULL;
}

//-----------------------------------------------------------------------------
bool CEconOperationDefinition::BInitFromKV( KeyValues *pKVPOperation, CUtlVector<CUtlString> *pVecErrors )
{
	m_unOperationID = V_atoi( pKVPOperation->GetName() );

	m_pszName = pKVPOperation->GetString( "name", NULL );
	SCHEMA_INIT_CHECK( m_pszName != NULL, "OperationDefinition %s does not have 'name'", m_pszName );

	// initialize required item def index if we specified one
	const char *pszRequiredName = pKVPOperation->GetString( "required_item_name", NULL );
	if ( pszRequiredName )
	{
		CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( pszRequiredName );
		SCHEMA_INIT_CHECK( pDef != NULL, "OperationDefinition couldn't find item def from required name '%s'", pszRequiredName );

		m_unRequiredItemDefIndex = pDef->GetDefinitionIndex();
	}

	const char *pszGatewayItemName = pKVPOperation->GetString( "gateway_item_name", NULL );
	if ( pszGatewayItemName )
	{
		CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( pszGatewayItemName );
		SCHEMA_INIT_CHECK( pDef != NULL, "OperationDefinition couldn't find item def from gateway name '%s'", pszGatewayItemName );

		m_unGatewayItemDefIndex = pDef->GetDefinitionIndex();
	}

	if ( m_unGatewayItemDefIndex != INVALID_ITEM_DEF_INDEX )
	{
		SCHEMA_INIT_CHECK( m_unRequiredItemDefIndex != INVALID_ITEM_DEF_INDEX, "If a gateway item is set, a required item must be set!  Mismatch in %d", m_unOperationID );
	}

	const char *pszOperationStartDate = pKVPOperation->GetString( "operation_start_date", NULL );
	SCHEMA_INIT_CHECK( pszOperationStartDate != NULL, "OperationDefinition %s does not have 'operation_start_date'", m_pszName );
	m_OperationStartDate = ( pszOperationStartDate && pszOperationStartDate[0] )
							? CRTime::RTime32FromFmtString( "YYYY-MM-DD hh:mm:ss" , pszOperationStartDate )
							: RTime32(0);

	const char *pszDropEndDate = pKVPOperation->GetString( "stop_giving_to_player_date", NULL );
	SCHEMA_INIT_CHECK( pszDropEndDate != NULL, "OperationDefinition %s does not have 'stop_giving_to_player_date'", m_pszName );
	m_StopGivingToPlayerDate = ( pszDropEndDate && pszDropEndDate[0] )
							? CRTime::RTime32FromFmtString( "YYYY-MM-DD hh:mm:ss" , pszDropEndDate )
							: RTime32(0);

	const char *pszOperationEndDate = pKVPOperation->GetString( "stop_adding_to_queue_date", NULL );
	SCHEMA_INIT_CHECK( pszOperationEndDate != NULL, "OperationDefinition %s does not have 'stop_adding_to_queue_date'", m_pszName );
	m_StopAddingToQueueDate = ( pszOperationEndDate && pszOperationEndDate[0] )
							? CRTime::RTime32FromFmtString( "YYYY-MM-DD hh:mm:ss" , pszOperationEndDate )
							: RTime32(0);

	m_pszQuestLogResFile	= pKVPOperation->GetString( "quest_log_res_file", NULL );
	m_pszQuestListResFile	= pKVPOperation->GetString( "quest_list_res_file", NULL );

	m_pszOperationLootList	= pKVPOperation->GetString( "operation_lootlist" );
	SCHEMA_INIT_CHECK( m_pszOperationLootList != NULL, "OperationDefinition %s does not have 'operation_lootlist'", m_pszName );

	m_bIsCampaign			= pKVPOperation->GetBool( "is_campaign" );
	m_unMaxDropCount		= pKVPOperation->GetInt( "max_drop_count" );

#ifdef GC_DLL

	m_rtQueueFreqMin	= pKVPOperation->GetFloat( "queue_freq_min" ) * k_nSecondsPerHour; // converts specified hours to seconds
	SCHEMA_INIT_CHECK( m_rtQueueFreqMin != 0.f, "OperationDefinition %s does not have 'queue_freq_min'", m_pszName );

	m_rtQueueFreqMax	= pKVPOperation->GetFloat( "queue_freq_max" ) * k_nSecondsPerHour; // converts specified hours to seconds
	SCHEMA_INIT_CHECK( m_rtQueueFreqMax != 0.f, "OperationDefinition %s does not have 'queue_freq_max'", m_pszName );

	SCHEMA_INIT_CHECK( m_rtQueueFreqMin <= m_rtQueueFreqMax, "OperationDefinition %s 'queue_freq_min' must be less than 'queue_freq_max'", m_pszName );

	m_rtDropFreqMin		= pKVPOperation->GetFloat( "drop_freq_min" ) * k_nSecondsPerHour; // converts specified hours to seconds
	SCHEMA_INIT_CHECK( m_rtDropFreqMin != 0.f, "OperationDefinition %s does not have 'drop_freq_min'", m_pszName );

	m_rtDropFreqMax		= pKVPOperation->GetFloat( "drop_freq_max" ) * k_nSecondsPerHour; // converts specified hours to seconds
	SCHEMA_INIT_CHECK( m_rtDropFreqMax != 0.f, "OperationDefinition %s does not have 'drop_freq_max'", m_pszName );

	SCHEMA_INIT_CHECK( m_rtDropFreqMin <= m_rtDropFreqMax, "OperationDefinition %s 'drop_freq_min' must be less than 'drop_freq_max'", m_pszName );
	
	m_unSeed			= pKVPOperation->GetInt( "seed_drops" );
	m_unMaxHeldDrops	= pKVPOperation->GetInt( "max_held_drops" );
	m_nMaxQueueCount	= pKVPOperation->GetInt( "max_queue_count" );
	m_unMaxDropPerThink	= pKVPOperation->GetInt( "max_drop_per_think", 1 );

	m_pszContractRewardLootlist[ REWARD_CASE ]		= pKVPOperation->GetString( "contract_reward_case_lootlist" );
	m_pszContractRewardLootlist[ REWARD_WEAPON ]	= pKVPOperation->GetString( "contract_reward_weapon_lootlist" );

#endif // GC_DLL

	m_pKVItem = pKVPOperation->MakeCopy();

	return SCHEMA_INIT_SUCCESS();
}

#ifdef GC_DLL
#ifdef STAGING_ONLY
	GCConVar gc_quick_operation_drop_name( "gc_quick_operation_drop_name", "" );
	GCConVar gc_quick_operation_drop_rate( "gc_quick_operation_drop_rate", "10" );
#endif // STAGING_ONLY

RTime32	CEconOperationDefinition::GetMinQueueFreq() const
{ 
#ifdef STAGING_ONLY
	if ( V_strieq( gc_quick_operation_drop_name.GetString(), m_pszName ) )
	{
		return gc_quick_operation_drop_rate.GetInt();
	}
#endif

	return m_rtQueueFreqMin;
}

RTime32	CEconOperationDefinition::GetMaxQueueFreq() const
{ 
#ifdef STAGING_ONLY
	if ( V_strieq( gc_quick_operation_drop_name.GetString(), m_pszName ) )
	{
		return gc_quick_operation_drop_rate.GetInt() + 2;
	}
#endif

	return m_rtQueueFreqMax;
}

RTime32	CEconOperationDefinition::GetMinDropFreq() const 
{ 
#ifdef STAGING_ONLY
	if ( V_strieq( gc_quick_operation_drop_name.GetString(), m_pszName ) )
	{
		return gc_quick_operation_drop_rate.GetInt();
	}
#endif

	return m_rtDropFreqMin;
}

RTime32	CEconOperationDefinition::GetMaxDropFreq() const 
{ 
#ifdef STAGING_ONLY
	if ( V_strieq( gc_quick_operation_drop_name.GetString(), m_pszName ) )
	{
		return gc_quick_operation_drop_rate.GetInt() + 2;
	}
#endif

	return m_rtDropFreqMax;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool BCommonInitPropertyGeneratorsFromKV( const char *pszContext, CUtlVector<const IEconItemPropertyGenerator *> *out_pvecGenerators, KeyValues *pKV, CUtlVector<CUtlString> *pVecErrors )
{
	// Forward declaration so factory functions can be wherever.
	IEconItemPropertyGenerator *CreateChangeQualityGenerator( KeyValues *, CUtlVector<CUtlString> * );
	IEconItemPropertyGenerator *CreateRandomEvenChanceAttrGenerator( KeyValues *, CUtlVector<CUtlString> * );
	IEconItemPropertyGenerator *CreateUniformLineItemLootListGenerator( KeyValues *, CUtlVector<CUtlString> * );
	IEconItemPropertyGenerator *CreateDynamicAttrsGenerator( KeyValues *, CUtlVector<CUtlString> * );
	
	// "Factory".
	struct econ_item_property_generator_factory_entry_t
	{
		const char *m_pszGeneratorName;
		IEconItemPropertyGenerator * (* m_funcCreateGeneratorInstance)( KeyValues *, CUtlVector<CUtlString> * );
	};

	static econ_item_property_generator_factory_entry_t s_Generators[] =
	{
		{ "change_quality",					&CreateChangeQualityGenerator },
		{ "random_even_chance_attr",		&CreateRandomEvenChanceAttrGenerator },
		{ "uniform_line_item_loot_list",	&CreateUniformLineItemLootListGenerator },
		{ "dynamic_attrs",					&CreateDynamicAttrsGenerator },
	};

	Assert( out_pvecGenerators );
	Assert( pVecErrors );

	// No input data means "we succeeded here".
	if ( !pKV )
		return true;

	// Handle each generator one at a time. We'll try to initialize the whole set to get as many
	// errors as possible and then return accumulated errors.
	FOR_EACH_SUBKEY( pKV, pKVGenerator )
	{
		const char *pszGeneratorName = pKVGenerator->GetName();

		IEconItemPropertyGenerator *pGenerator = NULL;
		for ( const auto& gen : s_Generators )
		{
			if ( !V_strieq( gen.m_pszGeneratorName, pszGeneratorName ) )
				continue;

			pGenerator = (*gen.m_funcCreateGeneratorInstance)( pKVGenerator, pVecErrors );
			SCHEMA_INIT_CHECK( pGenerator != nullptr, "%s: property generator \"%s\" failed to initialize.\n", pszContext, pszGeneratorName );
		
			out_pvecGenerators->AddToTail( pGenerator );
			break;
		}

		// Make sure we found a way to create this instance.
		SCHEMA_INIT_CHECK( pGenerator != nullptr, "%s: unknown type for property generator \"%s\".\n", pszContext, pszGeneratorName );
	}

	return SCHEMA_INIT_SUCCESS();
}
#endif // GC_DLL

//-----------------------------------------------------------------------------
// Dtor
//-----------------------------------------------------------------------------
CEconLootListDefinition::~CEconLootListDefinition( void )
{
#ifdef GC_DLL
	m_RandomAttribs.PurgeAndDeleteElements();
	m_PropertyGenerators.PurgeAndDeleteElements();
#endif
}

#ifdef GC_DLL
bool CEconLootListDefinition::AddRandomAtrributes( KeyValues *pRandomAttributesKV, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /*= NULL*/ )
{
	const char *pszAttrName = pRandomAttributesKV->GetName();

	// We've found the random attribute block. Parse it.
	random_attrib_t *pRandomAttr = pschema.CreateRandomAttribute( m_pszName, pRandomAttributesKV, pVecErrors );
	
	SCHEMA_INIT_CHECK(
		NULL != pRandomAttr,
		CFmtStr( "Loot List %s: Failed to create random_attrib_t '%s'", m_pszName, pszAttrName ) );

	m_RandomAttribs.AddToTail( pRandomAttr );

	return true;
}

bool CEconLootListDefinition::AddRandomAttributesFromTemplates( KeyValues *pRandomAttributesKV, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /*= NULL*/ )
{
	const char *pszAttrName = pRandomAttributesKV->GetName();

	// try to find attr by template name
	random_attrib_t *pRandomAttrTemplate = pschema.GetRandomAttributeTemplateByName( pszAttrName );
	
	SCHEMA_INIT_CHECK(
		NULL != pRandomAttrTemplate,
		CFmtStr( "Loot List %s: Couldn't find random_attrib_t '%s' from attribute_templates", m_pszName, pszAttrName ) );

	// craete a copy of the template and add to the list
	random_attrib_t *pRandomAttr = new random_attrib_t;
	*pRandomAttr = *pRandomAttrTemplate;
	m_RandomAttribs.AddToTail( pRandomAttr );

	return true;
}
#endif // GC_DLL
//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
static const char *g_pszDefaultRevolvingLootListHeader = "#Econ_Revolving_Loot_List";

bool CEconLootListDefinition::BInitFromKV( KeyValues *pKVLootList, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors )
{
	m_pszName = pKVLootList->GetName();
	m_pszLootListHeader = g_pszDefaultRevolvingLootListHeader;
	m_pszLootListFooter = NULL;
	m_pszCollectionReference = NULL;
	m_bPublicListContents = true;
#ifdef GC_DLL
	m_iNoDupesIterations = -1;				// disable no-dupes functionality by default
	m_unRarity = pKVLootList->GetInt( "rarity", k_unItemRarity_Any );
#endif // GC_DLL
	bool bCollectionLootList = false;

	FOR_EACH_SUBKEY( pKVLootList, pKVListItem )
	{
		const char *pszName = pKVListItem->GetName();
		
		if ( V_streq( pszName, "loot_list_header_desc" ) )
		{
			// Make sure we didn't specify multiple entries.
			SCHEMA_INIT_CHECK(
				g_pszDefaultRevolvingLootListHeader == m_pszLootListHeader,
				"Loot list %s: Multiple header descriptions specified", m_pszName );

			m_pszLootListHeader = pKVListItem->GetString();

			SCHEMA_INIT_CHECK(
				NULL != m_pszLootListHeader,
				"Loot list %s: Invalid header description specified", m_pszName );

			continue;
		}
		else if ( V_streq( pszName, "loot_list_footer_desc" ) )
		{
			// Make sure we didn't specify multiple entries.
			SCHEMA_INIT_CHECK(
				NULL == m_pszLootListFooter,
				"Loot list %s: Multiple footer descriptions specified", m_pszName );

			m_pszLootListFooter = pKVListItem->GetString();

			SCHEMA_INIT_CHECK(
				NULL != m_pszLootListFooter,
				"Loot list %s: Invalid header description specified", m_pszName );

			continue;
		}
		else if ( V_streq( pszName, "loot_list_collection" ) )
		{
			// Set name as the collection lootlist name
			pszName = pKVListItem->GetString();
			m_pszCollectionReference = pszName;
			bCollectionLootList = true;
		}
		else if ( V_streq( pszName, "hide_lootlist" ) )
		{
			m_bPublicListContents = !pKVListItem->GetBool( nullptr, true );
			continue;
		}
		else if ( V_streq( pszName, "rarity" ) )
		{
			// already parsed up top
			continue;
		}
#ifdef GC_DLL
		else if ( V_streq( pszName, "random_attributes" ) )
		{
			AddRandomAtrributes( pKVListItem, pschema, pVecErrors );
			continue;
		}
		else if ( V_streq( pszName, "attribute_templates" ) )
		{
			FOR_EACH_SUBKEY( pKVListItem, pKVAttributeTemplate )
			{
				if ( pKVAttributeTemplate->GetInt() == 0 )
					continue;

				bool bAdded = AddRandomAttributesFromTemplates( pKVAttributeTemplate, pschema, pVecErrors );
				SCHEMA_INIT_CHECK( bAdded, "Loot list %s: Failed to attribute_templates '%s'", m_pszName, pKVAttributeTemplate->GetName() );
			}

			continue;
		}
		else if ( V_streq( pszName, "public_list_contents" ) )
		{
			m_bPublicListContents = pKVListItem->GetBool( nullptr, true );
			continue;
		}
		else if ( V_strieq( pszName, "__no_dupes_iter_count" ) )
		{
			m_iNoDupesIterations = pKVListItem->GetInt( nullptr, -1 );
			continue;
		}
		else if ( V_streq( pszName, "additional_drop" ) )
		{
			float		fChance			   = pKVListItem->GetFloat( "chance", 0.0f );
			bool		bPremiumOnly	   = pKVListItem->GetBool( "premium_only", false );
			const char *pszLootList		   = pKVListItem->GetString( "loot_list", "" );
			const char *pszRequiredHoliday = pKVListItem->GetString( "required_holiday", NULL );
			const char *pszDropPerdiodStartDate = pKVListItem->GetString( "start_date", NULL );
			const char *pszDropPerdiodEndDate	= pKVListItem->GetString( "end_date", NULL );

			int iRequiredHolidayIndex = pszRequiredHoliday
									  ? EconHolidays_GetHolidayForString( pszRequiredHoliday )
									  : kHoliday_None;

			RTime32 dropStartDate = ( pszDropPerdiodStartDate && pszDropPerdiodStartDate[0] )
							? CRTime::RTime32FromFmtString( "YYYY-MM-DD hh:mm:ss" , pszDropPerdiodStartDate )
							: RTime32(0);	// Default to the start of time

			// Check that if we convert back to a string, we get the same value
			char rtimeBuf[k_RTimeRenderBufferSize];
			SCHEMA_INIT_CHECK(
				pszDropPerdiodStartDate == NULL || Q_strcmp( CRTime::RTime32ToString( dropStartDate, rtimeBuf ), pszDropPerdiodStartDate ) == 0,
				"Malformed start drop date \"%s\" for additional_drop in lootlist %s.  Must be of the form \"YYYY-MM-DD hh:mm:ss\"", pszDropPerdiodStartDate, m_pszName );


			RTime32 dropEndDate = ( pszDropPerdiodEndDate && pszDropPerdiodEndDate[0] )
						  ? CRTime::RTime32FromFmtString( "YYYY-MM-DD hh:mm:ss" , pszDropPerdiodEndDate )
						  : ~RTime32(0);	// Default to the end of time

			// Check that if we convert back to a string, we get the same value
			SCHEMA_INIT_CHECK(
				pszDropPerdiodEndDate == NULL || Q_strcmp( CRTime::RTime32ToString( dropEndDate, rtimeBuf ), pszDropPerdiodEndDate ) == 0,
				"Malformed end drop date \"%s\" for additional_drop in lootlist %s.  Must be of the form \"YYYY-MM-DD hh:mm:ss\"", pszDropPerdiodEndDate, m_pszName );

			SCHEMA_INIT_CHECK(
				fChance > 0.0f && fChance <= 1.0f,
				"Loot list %s: Invalid \"additional_drop\" chance %.2f", m_pszName, fChance );

			SCHEMA_INIT_CHECK(
				pszLootList && pszLootList[0],
				"Loot list %s: Missing \"additional_drop\" loot list name", m_pszName );

			SCHEMA_INIT_CHECK(
				(pszRequiredHoliday == NULL) == (iRequiredHolidayIndex == kHoliday_None),
				"Loot list %s: Unknown or missing holiday \"%s\"", m_pszName, pszRequiredHoliday ? pszRequiredHoliday : "(null)" );

			if ( pszLootList )
			{
				const CEconLootListDefinition *pLootListDef = GetItemSchema()->GetLootListByName( pszLootList );
				SCHEMA_INIT_CHECK(
					pLootListDef,
					"Loot list %s: Invalid \"additional_drop\" loot list \"%s\"", m_pszName, pszLootList );

				if ( pLootListDef )
				{
					drop_period_t dropPeriod = { dropStartDate, dropEndDate };
					loot_list_additional_drop_t additionalDrop = { fChance, bPremiumOnly, pszLootList, iRequiredHolidayIndex, dropPeriod };
					m_AdditionalDrops.AddToTail( additionalDrop );
				}
			}
			continue;
		}
		else if ( V_streq( pszName, "property_generators" ) )
		{
			SCHEMA_INIT_SUBSTEP( BCommonInitPropertyGeneratorsFromKV( m_pszName, &m_PropertyGenerators, pKVListItem, pVecErrors ) );
			continue;
		}
#endif // GC_DLL

		int iDef = 0;
		// First, see if we've got a loot list name, for embedded loot lists
		int iIdx = 0;
		if ( GetItemSchema()->GetLootListByName( pszName, &iIdx ) )
		{
			// HACKY: Store loot list indices as negatives, starting from -1, because 0 is a valid item index
			iDef = 0 - 1 - iIdx;
		}
		else
		{
			// Not a loot list. See if it's an item index. Check the first character.
			if ( pszName[0] >= '0' && pszName[0] <= '9' )
			{
				iDef = atoi( pszName );
			}
			else
			{
				// Not a number. See if we can find an item def with a matching name.
				const CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( pszName );
				if ( pDef )
				{
					iDef = pDef->GetDefinitionIndex();
				}

				SCHEMA_INIT_CHECK( 
					pDef != NULL,
					"Loot list %s: Item definition \"%s\" was not found", m_pszName, pszName );
			}
		}

		// Default to the start dropping at the start of time and end dropping at the end of time
		drop_period_t dropPeriod = { RTime32(0), ~RTime32(0) };

		// Make sure we never put non-enabled items into loot lists
		if ( iDef > 0 )
		{
			const CEconItemDefinition *pItemDef = GetItemSchema()->GetItemDefinition( iDef );
			SCHEMA_INIT_CHECK( 
				pItemDef != NULL,
				"Loot list %s: Item definition index \"%s\" (%d) was not found", m_pszName, pszName, iDef );

			static CSchemaAttributeDefHandle pAttribDef_StartDropDate( "start drop date" );
			static CSchemaAttributeDefHandle pAttribDef_EndDropDate( "end drop date" );

			CAttribute_String value;
			// Check for start drop date attribute on this item
			if ( FindAttribute( pItemDef, pAttribDef_StartDropDate, &value ) )
			{
				const char* pszStartDate = value.value().c_str();
				dropPeriod.m_DropStartDate = CRTime::RTime32FromFmtString( "YYYY-MM-DD hh:mm:ss" , pszStartDate );

				// Check that if we convert back to a string, we get the same value
				char rtimeBuf[k_RTimeRenderBufferSize];
				SCHEMA_INIT_CHECK(
					Q_strcmp( CRTime::RTime32ToString( dropPeriod.m_DropStartDate, rtimeBuf ), pszStartDate ) == 0,
					"Malformed start drop date \"%s\" for item %s.  Must be of the form \"YYYY-MM-DD hh:mm:ss\"",
					pszStartDate, pItemDef->GetDefinitionName() );
			}

			// Check for end drop date attribute on this item
			if ( FindAttribute( pItemDef, pAttribDef_EndDropDate, &value ) )
			{
				const char* pszEndDate = value.value().c_str();
				dropPeriod.m_DropEndDate = CRTime::RTime32FromFmtString( "YYYY-MM-DD hh:mm:ss" , pszEndDate );

				// Check that if we convert back to a string, we get the same value
				char rtimeBuf[k_RTimeRenderBufferSize];
				SCHEMA_INIT_CHECK(
					Q_strcmp( CRTime::RTime32ToString( dropPeriod.m_DropEndDate, rtimeBuf ), pszEndDate ) == 0,
					"Malformed end drop date \"%s\" for item %s.  Must be of the form \"YYYY-MM-DD hh:mm:ss\"", pszEndDate, pItemDef->GetDefinitionName() );
			}

#ifdef GC_DLL
			if ( pItemDef )
			{
				SCHEMA_INIT_CHECK( 
					true == pItemDef->BEnabled(), 
					"Loot list %s: Item definition \"%s\" (%d) isn't enabled, not allowed in loot lists", m_pszName, pItemDef->GetDefinitionName(), iDef );
			}
#endif // GC_DLL
		}

		float fItemWeight = 0.f;
#ifdef GC_DLL
		fItemWeight = bCollectionLootList ? 1.0f : pKVListItem->GetFloat();
		SCHEMA_INIT_CHECK( 
			fItemWeight > 0.0f,
			"Loot list %s: Item definition index \"%s\" (%d) has invalid weight %.2f", m_pszName, pszName, iDef, fItemWeight );
#endif // GC_DLL

		// Add this item
		drop_item_t dropItem = { iDef, fItemWeight, dropPeriod };
		m_DropList.AddToTail( dropItem );
	}

#ifdef GC_DLL

	int nNumTimeLimitedItems = 0;
	FOR_EACH_VEC( m_DropList, i )
	{
		// If either the start date or the end date is set, tally it up as a time limited item
		if( m_DropList[i].m_dropPeriod.m_DropStartDate != RTime32(0) || m_DropList[i].m_dropPeriod.m_DropEndDate != ~RTime32(0) )
		{
			++nNumTimeLimitedItems;
		}
	}

	// Verify that at least one item in a lootlist does not have a drop period that limits when it can drop.
	// This guarantees that we will always drop *something*
	SCHEMA_INIT_CHECK( m_DropList.Count() > nNumTimeLimitedItems, "Lootlist \"%s\" is made up entirely of limited-time items!  At least one must not be time-limited.", m_pszName );
#endif

	return SCHEMA_INIT_SUCCESS();
}

#ifdef GC_DLL
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static bool BContainsDuplicateItemDefs( const CUtlVector<CEconLootListDefinition::rolled_item_defs_t>& vecItemDefsA, const CUtlVector<CEconLootListDefinition::rolled_item_defs_t>& vecItemDefsB )
{
	CUtlHashtable<const CEconItemDefinition *> hashItemDefs;

	auto BPopulateAndLookForDupes = [&] ( const CUtlVector<CEconLootListDefinition::rolled_item_defs_t>& vecItemDefs )
	{
		for ( const auto& rolledItemDef : vecItemDefs )
		{
			if ( hashItemDefs.HasElement( rolledItemDef.m_pItemDef ) )
				return true;

			hashItemDefs.Insert( rolledItemDef.m_pItemDef );
		}

		return false;
	};

	return BPopulateAndLookForDupes( vecItemDefsA )
		|| BPopulateAndLookForDupes( vecItemDefsB );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconLootListDefinition::BGenerateSingleRollRandomItems( const CEconGameAccount *pGameAccount, bool bFreeAccount, CUtlVector<CEconItem *> *out_pvecItems, const CUtlVector< item_definition_index_t > *pVecAvoidItemDefs /*= NULL*/ ) const
{
	Assert( out_pvecItems );

	// Where is our source of random numbers coming from? If we're a no-dupe list,
	// we want to have reproducible state so we use our account ID as a unique-ish
	// seed.
	//
	// Wrap the whole thing in a smart pointer so we clean up whenever/however we
	// leave.
	std::unique_ptr<IUniformRandomStream> pRandomStream( [=]() -> IUniformRandomStream *
	{
		if ( !BIsInternalNoDupesLootList() || !pGameAccount )
			return new CDefaultUniformRandomStream;
	
	
		CUniformRandomStream *pAccountUniformRandomStream = new CUniformRandomStream;
		pAccountUniformRandomStream->SetSeed( pGameAccount->Obj().m_unAccountID );

		return pAccountUniformRandomStream;
	}() );

	// Make however many passes through our loot list code until we've generated the
	// right number of passing sets. (Most loot lists let anything pass. Some specify
	// no duplicate definitions allowed.)
	CUtlVector<rolled_item_defs_t> vecCumulativeItemDefs;			// total list of everything we've generated so far in any number of result sets
	CUtlVector<rolled_item_defs_t> vecItemDefs;						// current list under evaluation
	int iNoDupesIterations = 0;

	// This actually isn't guaranteed to converge, and is guaranteed not to converge if
	// we set up a broken lootlist. We set a really high bar here to catch broken/pathologically
	// bad cases here without grinding the whole GC to a halt.
	enum { kUpperBoundIterationSanityCheck = 2000 };
	int iTotalIterations = 0;
	
	while ( true )
	{
		// Don't runaway.
		iTotalIterations++;
		if ( iTotalIterations >= kUpperBoundIterationSanityCheck )
			return false;

		// Generate all of our item defs and their lootlists
		vecItemDefs.Purge();
		if ( !RollRandomItemsAndAdditionalItems( pRandomStream.get(), bFreeAccount, &vecItemDefs, pVecAvoidItemDefs ) )
			return false;

		// If we don't care about dupes and we got any results at all we're done.
		if ( !BIsInternalNoDupesLootList() )
			break;

		// If we do care about dupes and we have some, ignore this set of items.
		if ( BContainsDuplicateItemDefs( vecItemDefs, vecCumulativeItemDefs ) )
			continue;

		// Did we get to the right result set?
		iNoDupesIterations++;
		if ( iNoDupesIterations > m_iNoDupesIterations )
			break;
		
		// Store off the list of definition indices we've already used them so they don't get reused
		// in a later set.
		vecCumulativeItemDefs.AddVectorToTail( vecItemDefs );
	}
	
	// If we get down to here, we expect that we've rolled at least one item def
	Assert( vecItemDefs.Count() > 0 );
	FOR_EACH_VEC( vecItemDefs, i )
	{
		const rolled_item_defs_t& rolledDef = vecItemDefs[i];
		Assert( rolledDef.m_pItemDef );
		Assert( rolledDef.m_vecAffectingLootLists.Count() > 0 );

		// Create the items
		CEconItem *pItem = GEconManager()->GetItemFactory().CreateSpecificItem( pGameAccount, rolledDef.m_pItemDef->GetDefinitionIndex() );
		out_pvecItems->AddToTail( pItem );
		
		// Go through and let all the affecting lootlists attach their attributes to the item
		FOR_EACH_VEC( rolledDef.m_vecAffectingLootLists, j )
		{
			const CEconLootListDefinition *pLootList = rolledDef.m_vecAffectingLootLists[j];
			Assert( pLootList );
			if ( !pLootList->BAttachLootListAttributes( pGameAccount, pItem ) )
				return false;
		}
	}

	return ( out_pvecItems->Count() > 0 );
}

class CRollSimulator
{
public:

	CRollSimulator()
		: m_mapRarityCounts( StringLessThan )
		, m_mapCounts( DefLessFunc( item_definition_index_t ) )
		, m_mapUnusualHatEffectsCount( DefLessFunc( uint32 ) ) 
		, m_mapUnusualTauntEffectsCount( DefLessFunc( uint32 ) )
		, m_nNumIters( 0 )
	{}

	void RollLootlist( const char* pszLootListName, int nRolls, bool bWipePreviousResults = false )
	{
		const CEconLootListDefinition* pLootlist = GetItemSchema()->GetLootListByName( pszLootListName );

		if ( !pLootlist )
		{
			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "Invalid lootlist \"%s\".\n", pszLootListName );
			return;
		}

		// Clear out results first?
		if ( bWipePreviousResults )
		{
			m_mapCounts.Purge();
			m_mapRarityCounts.Purge();
			m_DropsMaxes.Clear();
			m_DropsTotals.Clear();
		}

		while( nRolls-- )
		{
			AutoYield();
			CUtlVector<CEconItem *> vecItems;
			pLootlist->BGenerateSingleRollRandomItems( NULL, false, &vecItems );

			// Tally up what we got.
			for( auto pItem : vecItems )
			{
				AutoYield();
				// Insert item def if we need
				item_definition_index_t nDefIndex = pItem->GetDefinitionIndex();
				auto idx = m_mapCounts.Find( nDefIndex );
				if ( m_mapCounts.InvalidIndex() == idx )
				{
					idx = m_mapCounts.Insert( nDefIndex );
				}

				// Insert rarity if needed
				const CEconItemRarityDefinition* pItemRarity = GetItemSchema()->GetRarityDefinition( pItem->GetItemDefinition()->GetRarity() );
				if ( pItemRarity )
				{
					const char* pszRarity = GGCGameBase()->LocalizeToken( pItemRarity->GetLocKey() , k_Lang_English );
					auto rarityIdx = m_mapRarityCounts.Find( pszRarity );
					if ( m_mapRarityCounts.InvalidIndex() == rarityIdx )
					{
						rarityIdx = m_mapRarityCounts.Insert( pszRarity, 0 );
					}

					m_mapRarityCounts[ rarityIdx ] += 1;
				}

				// Count
				DropResult_t& result = m_mapCounts[ idx ];
				++result.m_nRollCount;
				++m_DropsTotals.m_nRollCount;
				m_DropsMaxes.m_nRollCount = Max( m_DropsMaxes.m_nRollCount, result.m_nRollCount );

				// Strange count
				if ( BIsItemStrange( pItem ) )
				{
					++result.m_nStrangeCount;
					++m_DropsTotals.m_nStrangeCount;
					m_DropsMaxes.m_nStrangeCount = Max( m_DropsMaxes.m_nStrangeCount, result.m_nStrangeCount );
				}

				// Unusual count
				static CSchemaAttributeDefHandle pAttrDef_ParticleEffect( "attach particle effect" );
				static CSchemaAttributeDefHandle pAttrDef_TauntUnusualAttr( "on taunt attach particle index" );
				if ( pAttrDef_ParticleEffect && pAttrDef_TauntUnusualAttr )
				{
					uint32 nUnusualHatValue = 0;
					uint32 nUnusualTauntValue = 0;
					pItem->FindAttribute( pAttrDef_ParticleEffect, &nUnusualHatValue );
					pItem->FindAttribute( pAttrDef_TauntUnusualAttr, &nUnusualTauntValue );
					// Cant use quality cause of old legacy items.  Quality is just a quick test
					if ( nUnusualHatValue != 0 || nUnusualTauntValue != 0 )
					{
						++result.m_nUnusualCount;
						++m_DropsTotals.m_nUnusualCount;
						m_DropsMaxes.m_nUnusualCount = Max( m_DropsMaxes.m_nUnusualCount, result.m_nUnusualCount );
					}

					if ( nUnusualHatValue )
					{
						nUnusualHatValue = (uint32)((float&)nUnusualHatValue);
						auto idx = m_mapUnusualHatEffectsCount.Find( nUnusualHatValue );
						if ( idx == m_mapUnusualHatEffectsCount.InvalidIndex() )
						{
							idx = m_mapUnusualHatEffectsCount.Insert( nUnusualHatValue, 0 );
						}

						++m_mapUnusualHatEffectsCount[ idx ];
					}

					if ( nUnusualTauntValue )
					{
						nUnusualTauntValue = (uint32)((float&)nUnusualTauntValue);
						auto idx = m_mapUnusualTauntEffectsCount.Find( nUnusualTauntValue );
						if ( idx == m_mapUnusualTauntEffectsCount.InvalidIndex() )
						{
							idx = m_mapUnusualTauntEffectsCount.Insert( nUnusualTauntValue, 0 );
						}

						++m_mapUnusualTauntEffectsCount[ idx ];
					}
				}
			}

			// Delete what we got
			vecItems.PurgeAndDeleteElements();
		}
	}

	void PrintRarityBreakdwn()
	{
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "-- Rarity breakdown:\n" );
		int nMaxDigits = 0;
		while( m_mapRarityCounts.Count() )
		{
			// Go through and print the most rolled to least rolled rarities
			auto maxIdx = m_mapRarityCounts.InvalidIndex();

			// Find the most hit
			FOR_EACH_MAP_FAST( m_mapRarityCounts, i )
			{
				AutoYield();
				if ( maxIdx == m_mapRarityCounts.InvalidIndex() ||  m_mapRarityCounts[ i ] > m_mapRarityCounts[ maxIdx ] )
				{
					maxIdx = i;
				}

				nMaxDigits = Max( nMaxDigits, NumDigits( m_mapRarityCounts[ maxIdx ] ) );
			}

			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%9s %*d %.3f%%\n",
					  m_mapRarityCounts.Key( maxIdx ), nMaxDigits, m_mapRarityCounts[ maxIdx ],
					  100.f * (float)m_mapRarityCounts[ maxIdx ] / m_DropsTotals.m_nRollCount );
			m_mapRarityCounts.RemoveAt( maxIdx );
		}

		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%-*s  %*s  %*s\n", 9 + 1 + NumDigits( m_DropsMaxes.m_nRollCount ) + 6,
				  "-- Item breakdown", NumDigits( m_DropsMaxes.m_nStrangeCount ) + 1, "S", NumDigits( m_DropsMaxes.m_nUnusualCount ), "U"  );
		
	}

	void PrintUnusualCounts()
	{
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "-- Unusual Hats breakdown:\n" );
		int nTotal = PrintUnusualsForType( m_mapUnusualHatEffectsCount );
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "-- %d total unusual hats\n", nTotal );
		
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "-- Unusual Taunts breakdown:\n" );
		nTotal = PrintUnusualsForType( m_mapUnusualTauntEffectsCount );
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "-- %d total unusual taunts\n", nTotal );
	}

	void PrintTotals()
	{
		while( m_mapCounts.Count() )
		{
			// Go through and print the most rolled to least rolled
			auto maxIdx = m_mapCounts.InvalidIndex();

			// Find the most hit
			FOR_EACH_MAP_FAST( m_mapCounts, i )
			{
				AutoYield();
				if ( maxIdx == m_mapCounts.InvalidIndex() ||  m_mapCounts[ i ].m_nRollCount > m_mapCounts[ maxIdx ].m_nRollCount )
				{
					maxIdx = i;
				}
			}

			DropResult_t& result = m_mapCounts[ maxIdx ];
			CEconItemDefinition *pItemDef = GetItemSchema()->GetItemDefinition( m_mapCounts.Key( maxIdx ) );

			const CEconItemRarityDefinition* pItemRarity = GetItemSchema()->GetRarityDefinition( pItemDef->GetRarity() );
			const char* pszRarity = pItemRarity ? GGCGameBase()->LocalizeToken( pItemRarity->GetLocKey() , k_Lang_English ) : "";
			CFmtStr rollString( "%*d %2.3f%%", NumDigits( m_DropsMaxes.m_nRollCount ), result.m_nRollCount,
								100.f * (float)result.m_nRollCount / m_DropsTotals.m_nRollCount );
			CFmtStr strangeString( "%*d", NumDigits( m_DropsMaxes.m_nStrangeCount ), result.m_nStrangeCount );
			CFmtStr unusualString( "%*d", NumDigits( m_DropsMaxes.m_nUnusualCount ), result.m_nUnusualCount );
			const char* pszItemname = GGCGameBase()->LocalizeToken( pItemDef->GetCustomPainkKitDefinition() ? pItemDef->GetCustomPainkKitDefinition()->GetName() : pItemDef->GetItemBaseName(), k_Lang_English );

			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%9s %s  %s  %s  %s\n", pszRarity, rollString.Get(), strangeString.Get(), unusualString.Get(), pszItemname );

			m_mapCounts.RemoveAt( maxIdx );
		}

		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "-- Total Items: %d Strange: %d (%.3f%%) Unusual: %d (%.3f%%)\n"
			, m_DropsTotals.m_nRollCount
			, m_DropsTotals.m_nStrangeCount
			, ( 100.f * (float)m_DropsTotals.m_nStrangeCount / m_DropsTotals.m_nRollCount )
			, m_DropsTotals.m_nUnusualCount
			, ( 100.f * (float)m_DropsTotals.m_nUnusualCount / m_DropsTotals.m_nRollCount ) );
	}

	struct DropResult_t
	{
		DropResult_t() { Clear(); }

		void Clear()
		{
			m_nStrangeCount = 0;
			m_nUnusualCount = 0;
			m_nRollCount = 0;
		}

		int m_nStrangeCount;
		int m_nUnusualCount;
		int m_nRollCount;
	};

	const DropResult_t& GetTotalDrops() const { return m_DropsTotals; }
	const DropResult_t& GetMaxesDrops() const { return m_DropsMaxes; }

private:

	int PrintUnusualsForType( CUtlMap< uint32, int >& mapUnusuals )
	{
		int nMaxDigits = 0;
		int nTotal = 0;
		FOR_EACH_MAP_FAST( mapUnusuals, i )
		{
			nTotal += mapUnusuals[ i ];
		}

		while( mapUnusuals.Count() )
		{
			// Go through and print the most rolled to least rolled unusual effects
			auto maxIdx = mapUnusuals.InvalidIndex();

			// Find the most hit
			FOR_EACH_MAP_FAST( mapUnusuals, i )
			{
				AutoYield();
				if ( maxIdx == mapUnusuals.InvalidIndex() ||  mapUnusuals[ i ] > mapUnusuals[ maxIdx ] )
				{
					maxIdx = i;
				}

				nMaxDigits = Max( nMaxDigits, NumDigits( mapUnusuals[ maxIdx ] ) );
			}

			char particleNameEntry[128];
			Q_snprintf( particleNameEntry, 