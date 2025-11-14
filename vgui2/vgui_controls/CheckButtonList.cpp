//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include <vgui_controls/CheckButtonList.h>

#include <tier1/KeyValues.h>

#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ScrollBar.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CheckButtonList::CheckButtonList(Panel *parent, const char *name) : BaseClass(parent, name)
{
	m_pScrollBar = new ScrollBar(this, NULL, true);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CheckButtonList::~CheckButtonList()
{
	RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: adds a check button to the list
//-----------------------------------------------------------------------------
intp CheckButtonList::AddItem(const char *itemText, bool startsSelected, KeyValues *userData)
{
	CheckItem_t newItem;
	newItem.checkButton = new vgui::CheckButton(this, NULL, itemText);
	newItem.checkButton->SetSilentMode( true );
	newItem.checkButton->SetSelected(startsSelected);
	newItem.checkButton->SetSilentMode( false );
	newItem.checkButton->AddActionSignalTarget(this);
	newItem.userData = userData;
	InvalidateLayout();
	return m_CheckItems.AddToTail(newItem);
}

//-----------------------------------------------------------------------------
// Purpose: clears the list
//-----------------------------------------------------------------------------
void CheckButtonList::RemoveAll()
{
	for (int i = 0; i < m_CheckItems.Count(); i++)
	{
		m_CheckItems[i].checkButton->MarkForDeletion();
		if (m_CheckItems[i].userData)
		{
			m_CheckItems[i].userData->deleteThis();
		}
	}

	m_CheckItems.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: returns the number of items in list that are checked
//-----------------------------------------------------------------------------
intp CheckButtonList::GetCheckedItemCount() const
{
	intp count = 0;
	for (intp i = 0; i < m_CheckItems.Count(); i++)
	{
		if (m_CheckItems[i].checkButton->IsSelected())
		{
			count++;
		}
	}

	return count;
}

//-----------------------------------------------------------------------------
// Purpose: lays out buttons
//-----------------------------------------------------------------------------
void CheckButtonList::PerformLayout()
{
	BaseClass::PerformLayout();

	// get sizes
	int x = 4, y = 4, wide = GetWide() - ((x * 2) + m_pScrollBar->GetWide()), tall = 22;

	// set scrollbar
	intp totalHeight = y + (m_CheckItems.Count() * tall);
	if (totalHeight > GetTall())
	{
		m_pScrollBar->SetRange(0, totalHeight + 1);
		m_pScrollBar->SetRangeWindow(GetTall());
		m_pScrollBar->SetVisible(true);
		m_pScrollBar->SetBounds(GetWide() - 21, 0, 19, GetTall() - 2);
		SetPaintBorderEnabled(true);
		y -= m_pScrollBar->GetValue();
	}
	else
	{
		m_pScrollBar->SetVisible(false);
		SetPaintBorderEnabled(false);
	}

	// position the items
	for (intp i = 0; i < m_CheckItems.Count(); i++)
	{
		CheckButton *btn = m_CheckItems[i].checkButton;
		btn->SetBounds(x, y, wide, tall);
		y += tall;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the border on the window
//-----------------------------------------------------------------------------
void CheckButtonList::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetBorder(pScheme->GetBorder("ButtonDepressedBorder"));
}

//-----------------------------------------------------------------------------
// Purpose: iteration
//-----------------------------------------------------------------------------
bool CheckButtonList::IsItemIDValid(intp itemID) const
{
	return m_CheckItems.IsValidIndex(itemID);
}

//-----------------------------------------------------------------------------
// Purpose: iteration
//-----------------------------------------------------------------------------
intp CheckButtonList::GetHighestItemID()  const
{
	return m_CheckItems.Count() - 1;
}

//-----------------------------------------------------------------------------
// Purpose: iteration
//-----------------------------------------------------------------------------
KeyValues *CheckButtonList::GetItemData(intp itemID)
{
	return m_CheckItems[itemID].userData;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
intp CheckButtonList::GetItemCount() const
{
	return m_CheckItems.Count();
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CheckButtonList::IsItemChecked(intp itemID)
{
	return m_CheckItems[itemID].checkButton->IsSelected();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the state of the check button
//-----------------------------------------------------------------------------
void CheckButtonList::SetItemCheckable(intp itemID, bool state)
{
	m_CheckItems[itemID].checkButton->SetCheckButtonCheckable(state);
}

//-----------------------------------------------------------------------------
// Purpose: Forwards up check button selected message
//-----------------------------------------------------------------------------
void CheckButtonList::OnCheckButtonChecked( KeyValues *pParams )
{
	vgui::Panel *pPanel = (vgui::Panel *)pParams->GetPtr( "panel" );
	intp c = m_CheckItems.Count();
	for ( intp i = 0; i < c; ++i )
	{
		if ( pPanel == m_CheckItems[i].checkButton )
		{
			KeyValues *kv = new KeyValues( "CheckButtonChecked", "itemid", i );
			kv->SetInt( "state", pParams->GetInt( "state" ) );
			PostActionSignal( kv );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: updates from scrollbar movement
//-----------------------------------------------------------------------------
void CheckButtonList::OnScrollBarSliderMoved()
{
	InvalidateLayout();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Mouse wheeled
//-----------------------------------------------------------------------------
void CheckButtonList::OnMouseWheeled(int delta)
{
	int val = m_pScrollBar->GetValue();
	// dimhotepus: Scale UI.
	val -= (delta * QuickPropScale( 15 ));
	m_pScrollBar->SetValue(val);
}
