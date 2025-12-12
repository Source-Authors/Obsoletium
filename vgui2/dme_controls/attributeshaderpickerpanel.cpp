//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/attributeshaderpickerpanel.h"
#include "dme_controls/AttributeTextEntry.h"
#include "matsys_controls/picker.h"
#include "tier1/KeyValues.h"
#include "matsys_controls/matsyscontrols.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/IShader.h"


using namespace vgui;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CAttributeShaderPickerPanel::CAttributeShaderPickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info )
{
}

CAttributeShaderPickerPanel::~CAttributeShaderPickerPanel()
{
}


//-----------------------------------------------------------------------------
// Called when it's time to show the picker
//-----------------------------------------------------------------------------
void CAttributeShaderPickerPanel::ShowPickerDialog()
{
	CPickerFrame *pShaderPickerDialog = new CPickerFrame( this, "Select Shader", "Shader", "shaderName" );

	intp nCount = vgui::MaterialSystem()->ShaderCount();
	IShader** ppShaderList = stackallocT( IShader*, nCount );
	vgui::MaterialSystem()->GetShaders( 0, nCount, ppShaderList );
	PickerList_t shaderList( 0, nCount );
	for ( intp i = 0; i < nCount; ++i )
	{
		if ( ( ppShaderList[i]->GetFlags() & SHADER_NOT_EDITABLE ) == 0 )
		{
			intp j = shaderList.AddToTail( );
			shaderList[j].m_pChoiceString = ppShaderList[i]->GetName();
			shaderList[j].m_pChoiceValue = ppShaderList[i]->GetName();
		}
	}

	pShaderPickerDialog->AddActionSignalTarget( this );
	pShaderPickerDialog->DoModal( shaderList );
}


//-----------------------------------------------------------------------------
// Called by the picker dialog if a asset was selected
//-----------------------------------------------------------------------------
void CAttributeShaderPickerPanel::OnPicked( KeyValues *pKeyValues )
{
	// Get the asset name back
	const char *pShaderName = pKeyValues->GetString( "choice", NULL );
	if ( !pShaderName || !pShaderName[ 0 ] )
		return;

	// Apply to text panel
	m_pData->SetText( pShaderName );
	SetDirty(true);
	if ( IsAutoApply() )
	{
		Apply();
	}
}
