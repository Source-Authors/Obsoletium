//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include <vgui_controls/ScrollBar.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>

#include <KeyValues.h>
#include <vgui/MouseCode.h>
#include <vgui/KeyCode.h>
#include <vgui/IInput.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include "PanelListPanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

class VScrollBarReversedButtons : public ScrollBar
{
public:
	VScrollBarReversedButtons( Panel *parent, const char *panelName, bool vertical );
	virtual void ApplySchemeSettings( IScheme *pScheme );
};

VScrollBarReversedButtons::VScrollBarReversedButtons( Panel *parent, const char *panelName, bool vertical ) : ScrollBar( parent, panelName, vertical )
{
}

void VScrollBarReversedButtons::ApplySchemeSettings( IScheme *pScheme )
{
	ScrollBar::ApplySchemeSettings( pScheme );

	Button *pButton;
	pButton = GetButton( 0 );
	pButton->SetArmedColor(		pButton->GetSchemeColor("DimBaseText", pScheme), pButton->GetBgColor());
	pButton->SetDepressedColor(	pButton->GetSchemeColor("DimBaseText", pScheme), pButton->GetBgColor());
	pButton->SetDefaultColor(	pButton->GetFgColor(),							 pButton->GetBgColor());
	
	pButton = GetButton( 1 );
	pButton->SetArmedColor(		pButton->GetSchemeColor("DimBaseText", pScheme), pButton->GetBgColor());
	pButton->SetDepressedColor(	pButton->GetSchemeColor("DimBaseText", pScheme), pButton->GetBgColor());
	pButton->SetDefaultColor(	pButton->GetFgColor(),							 pButton->GetBgColor());
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//			y - 
//			wide - 
//			tall - 
// Output : 
//-----------------------------------------------------------------------------
CPanelListPanel::CPanelListPanel( vgui::Panel *parent, char const *panelName, bool inverseButtons ) : Panel( parent, panelName )
{
	// dimhotepus: Scale UI.
	SetBounds( 0, 0, QuickPropScale( 100 ), QuickPropScale( 100 ) );
	_sliderYOffset = 0;

	if (inverseButtons)
	{
		_vbar = new VScrollBarReversedButtons(this, "CPanelListPanelVScroll", true );
	}
	else
	{
		_vbar = new ScrollBar(this, "CPanelListPanelVScroll", true );
	}
	// dimhotepus: Scale UI.
	_vbar->SetBounds( 0, 0, QuickPropScale( 20 ), QuickPropScale( 20 ) );
	_vbar->SetVisible(false);
	_vbar->AddActionSignalTarget( this );

	_embedded = new Panel( this, "PanelListEmbedded" );
	// dimhotepus: Scale UI.
	_embedded->SetBounds( 0, 0, QuickPropScale( 20 ), QuickPropScale( 20 ) );
	_embedded->SetPaintBackgroundEnabled( false );
	_embedded->SetPaintBorderEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPanelListPanel::~CPanelListPanel()
{
	// free data from table
	DeleteAllItems();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CPanelListPanel::computeVPixelsNeeded( void )
{
	int pixels = 0;
	for ( auto *item : _dataItems )
	{
		if ( !item )
			continue;

		Panel *panel = item->panel;
		if ( !panel )
			continue;

		int w, h;
		panel->GetSize( w, h );

		pixels += h;
	}
	// dimhotepus: Scale UI.
	pixels += QuickPropScale( 5 ); // add a buffer after the last item

	return pixels;

}

//-----------------------------------------------------------------------------
// Purpose: Returns the panel to use to render a cell
// Input  : column - 
//			row - 
// Output : Panel
//-----------------------------------------------------------------------------
Panel *CPanelListPanel::GetCellRenderer( int row )
{
	DATAITEM *item = _dataItems[ row ];
	if ( item )
	{
		Panel *panel = item->panel;
		return panel;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: adds an item to the view
//			data->GetName() is used to uniquely identify an item
//			data sub items are matched against column header name to be used in the table
// Input  : *item - 
//-----------------------------------------------------------------------------
int CPanelListPanel::AddItem( Panel *panel )
{
	InvalidateLayout();

	DATAITEM *newitem = new DATAITEM;
	newitem->panel = panel;
	panel->SetParent( _embedded );
	return _dataItems.PutElement( newitem );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
int	CPanelListPanel::GetItemCount( void )
{
	return _dataItems.GetCount();
}

//-----------------------------------------------------------------------------
// Purpose: returns pointer to data the row holds
// Input  : itemIndex - 
// Output : KeyValues
//-----------------------------------------------------------------------------
Panel *CPanelListPanel::GetItem(int itemIndex)
{
	if ( itemIndex < 0 || itemIndex >= _dataItems.GetCount() )
		return NULL;

	return _dataItems[itemIndex]->panel;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : itemIndex - 
// Output : DATAITEM
//-----------------------------------------------------------------------------
CPanelListPanel::DATAITEM *CPanelListPanel::GetDataItem( int itemIndex )
{
	if ( itemIndex < 0 || itemIndex >= _dataItems.GetCount() )
		return NULL;

	return _dataItems[ itemIndex ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//-----------------------------------------------------------------------------
void CPanelListPanel::RemoveItem(int itemIndex)
{
	DATAITEM *item = _dataItems[ itemIndex ];
	delete item->panel;
	delete item;
	_dataItems.RemoveElementAt(itemIndex);

	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: clears and deletes all the memory used by the data items
//-----------------------------------------------------------------------------
void CPanelListPanel::DeleteAllItems()
{
	for (auto *item : _dataItems)
	{
		if ( item )
		{
			delete item->panel;
			delete item;
		}
	}
	_dataItems.RemoveAll();

	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPanelListPanel::OnMouseWheeled(int delta)
{
	int val = _vbar->GetValue();
	// dimhotepus: Scale UI.
	val -= (delta * QuickPropScale( 3 * 5 ));
	_vbar->SetValue(val);
}

//-----------------------------------------------------------------------------
// Purpose: relayouts out the panel after any internal changes
//-----------------------------------------------------------------------------
void CPanelListPanel::PerformLayout()
{
	int wide, tall;
	GetSize( wide, tall );

	int vpixels = computeVPixelsNeeded();

	//!! need to make it recalculate scroll positions
	_vbar->SetVisible(true);
	_vbar->SetEnabled(false);
	// dimhotepus: Scale UI.
	_vbar->SetRange( 0, vpixels - tall + QuickPropScale( 24 ));
	_vbar->SetRangeWindow( QuickPropScale( 24 ) /*vpixels / 10*/ );
	_vbar->SetButtonPressedScrollValue( QuickPropScale( 24 ) );
	// dimhotepus: Scale UI.
	_vbar->SetPos(wide - QuickPropScale( 20 ), _sliderYOffset);
	// dimhotepus: Scale UI.
	_vbar->SetSize(QuickPropScale( 18 ), tall - QuickPropScale( 2 ) - _sliderYOffset);
	_vbar->InvalidateLayout();

	int top = _vbar->GetValue();

	_embedded->SetPos( 0, -top );
	// dimhotepus: Scale UI.
	_embedded->SetSize( wide- QuickPropScale( 20 ), vpixels );

	// Now lay out the controls on the embedded panel
	int y = 0;
	int h = 0;
	for ( intp i = 0; i < _dataItems.GetCount(); i++, y += h )
	{
		DATAITEM *item = _dataItems[ i ];
		if ( !item || !item->panel )
			continue;

		h = item->panel->GetTall();
		// dimhotepus: Scale UI.
		item->panel->SetBounds( QuickPropScale( 8 ), y, wide-QuickPropScale( 36 ), h );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPanelListPanel::PaintBackground()
{
	Panel::PaintBackground();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *inResourceData - 
//-----------------------------------------------------------------------------
void CPanelListPanel::ApplySchemeSettings(IScheme *pScheme)
{
	Panel::ApplySchemeSettings(pScheme);

	SetBorder(pScheme->GetBorder("ButtonDepressedBorder"));
	SetBgColor(GetSchemeColor("Label.BgColor", GetBgColor(), pScheme));


//	_labelFgColor = GetSchemeColor("WindowFgColor");
//	_selectionFgColor = GetSchemeColor("ListSelectionFgColor", _labelFgColor);
}

void CPanelListPanel::OnSliderMoved( int position )
{
	InvalidateLayout();
	Repaint();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CPanelListPanel::SetSliderYOffset( int pixels )
{
	_sliderYOffset = pixels;
}
