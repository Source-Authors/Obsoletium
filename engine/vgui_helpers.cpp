//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "client_pch.h"

#include "vgui_helpers.h"
#include <vgui_controls/TreeView.h>
#include <vgui_controls/ListPanel.h>
#include <vgui/ILocalize.h>
#include <vgui/ISystem.h>
#include "KeyValues.h"
#include "convar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CConVarCheckButton::CConVarCheckButton( vgui::Panel *parent, const char *panelName, const char *text ) : 
	vgui::CheckButton( parent, panelName, text )
{
	m_pConVar = NULL;
}

void CConVarCheckButton::SetConVar( ConVar *pVar )
{
	m_pConVar = pVar;
	SetSelected( m_pConVar->GetBool() );
}

void CConVarCheckButton::SetSelected( bool state )
{
	BaseClass::SetSelected( state );
	
	m_pConVar->SetValue( state );
}


void IncrementalUpdateTree_R( 
	vgui::TreeView *pTree, 
	intp iCurTreeNode,
	KeyValues *pValues,
	bool &bChanges,
	UpdateItemStateFn fn )
{
	// Add new items.
	intp iCurChild = 0;
	intp nChildren = pTree->GetNumChildren( iCurTreeNode );
	KeyValues *pSub = pValues->GetFirstSubKey();

	while ( iCurChild < nChildren || pSub )
	{
		// The items in the tree are keyed by the panel pointer.
		if ( pSub )
		{
			const char *pSubText = pSub->GetString( "Text", NULL );
			if ( pSubText )
			{
				if ( iCurChild < nChildren )
				{
					// Compare the items here.
					intp iChildItemId = pTree->GetChild( iCurTreeNode, iCurChild );
					
					if ( fn( pTree, iChildItemId, pSub ) )
						bChanges = true;

					IncrementalUpdateTree_R( pTree, iChildItemId, pSub, bChanges, fn );
				}
				else
				{
					// This means that the KeyValues has an extra node..
					bChanges = true;
					intp iChildItemId = pTree->AddItem( pSub, iCurTreeNode );
					
					if ( fn( pTree, iChildItemId, pSub ) )
						bChanges = true;

					IncrementalUpdateTree_R( pTree, iChildItemId, pSub, bChanges, fn );
				}
				
				++iCurChild;
			}

			pSub = pSub->GetNextKey();
		}
		else
		{
			// This means that the tree view has extra ones at the end. Get rid of them.
			intp iChildItemId = pTree->GetChild( iCurTreeNode, iCurChild );
			--nChildren;
			bChanges = true;

			// HACK: I put a hack in there so if you give a negative number for the item, it'll 
			// delete the panels immediately. This gets around a bug in the TreeView where the
			// panels don't always get deleted when using MarkPanelForDeletion.
			pTree->RemoveItem( -iChildItemId, false );
		}
	}
}


bool IncrementalUpdateTree( 
	vgui::TreeView *pTree, 
	KeyValues *pValues,
	UpdateItemStateFn fn,
	intp iRoot )
{
	if ( iRoot == -1 )
	{
		iRoot = pTree->GetRootItemIndex();
		if ( iRoot == -1 )
		{
			// Add a root if there isn't one yet.
			KeyValuesAD pTempValues( "" );
			pTempValues->SetString( "Text", "" );
			iRoot = pTree->AddItem( pTempValues, iRoot );
		}
	}

	bool bChanges = false;
	IncrementalUpdateTree_R( pTree, iRoot, pValues, bChanges, fn );
	return bChanges;
}


void CopyListPanelToClipboard( vgui::ListPanel *pListPanel )
{
	CUtlVector<char> textBuf;

	// Write the headers.
	intp nColumns = pListPanel->GetNumColumnHeaders();
	for ( intp i=0; i < nColumns; i++ )
	{
		if ( i != 0 )
			textBuf.AddToTail( '\t' );
		
		char tempText[512];
		if ( !pListPanel->GetColumnHeaderText( i, tempText ) )
			Error( "GetColumHeaderText( %zd ) failed", i );
		
		textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
	}
	textBuf.AddToTail( '\n' );

	// Now write the rows.
	intp iCur = pListPanel->FirstItem();
	while ( iCur != pListPanel->InvalidItemID() )
	{
		// Write the columns for this row.
		for ( intp i=0; i < nColumns; i++ )
		{
			if ( i != 0 )
				textBuf.AddToTail( '\t' );
		
			wchar_t tempTextWC[512];
			char tempText[512];

			pListPanel->GetCellText( iCur, i, tempTextWC );
			g_pVGuiLocalize->ConvertUnicodeToANSI( tempTextWC, tempText );

			textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
		}
		textBuf.AddToTail( '\n' );

		iCur = pListPanel->NextItem( iCur );
	}
	textBuf.AddToTail( 0 );

	// Set the clipboard text.
	vgui::system()->SetClipboardText( textBuf.Base(), textBuf.Count() );
}
