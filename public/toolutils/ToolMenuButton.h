//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Core Movie Maker UI API
//
//=============================================================================

#ifndef TOOLMENUBUTTON_H
#define TOOLMENUBUTTON_H

#include "vgui_controls/MenuButton.h"
#include "tier1/utldict.h"
#include "tier1/utlsymbol.h"


//-----------------------------------------------------------------------------
// Base class for tools menus
//-----------------------------------------------------------------------------
class CToolMenuButton : public vgui::MenuButton
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CToolMenuButton, vgui::MenuButton );
public:
	CToolMenuButton( vgui::Panel *parent, const char *panelName, const char *text, vgui::Panel *actionTarget );

	void OnShowMenu(vgui::Menu *menu) override;

	vgui::Menu	*GetMenu();

	// Add a simple text item to the menu
	virtual int AddMenuItem( char const *itemName, const char *itemText, KeyValues *message, Panel *target, const KeyValues *userData = NULL, char const *kbcommandname = NULL );
	virtual int AddCheckableMenuItem( char const *itemName, const char *itemText, KeyValues *message, Panel *target, const KeyValues *userData = NULL, char const *kbcommandname = NULL );

	// Wide-character version to add a simple text item to the menu
	virtual int AddMenuItem( char const *itemName, const wchar_t *itemText, KeyValues *message, Panel *target, const KeyValues *userData = NULL, char const *kbcommandname = NULL );
	virtual int AddCheckableMenuItem( char const *itemName, const wchar_t *itemText, KeyValues *message, Panel *target, const KeyValues *userData = NULL, char const *kbcommandname = NULL );

	virtual int FindMenuItem( char const *itemName );
	virtual void AddSeparatorAfterItem( char const *itemName );
	virtual void MoveMenuItem( int itemID, int moveBeforeThisItemID );

	virtual void SetItemEnabled( int itemID, bool state );

	// Pass in a NULL binding to clear it
	virtual void SetCurrentKeyBindingLabel( char const *itemName, char const *binding );

	virtual void AddSeparator();

	void		Reset();

protected:
	void		UpdateMenuItemKeyBindings();

	vgui::Menu	*m_pMenu;
	vgui::Panel	*m_pActionTarget;

	struct MenuItem_t
	{
		MenuItem_t()
			: m_ItemID( 0 ),
			  m_KeyBinding( UTL_INVAL_SYMBOL )
		{
		}
		// dimhotepus: unsigned short -> int.
		int	m_ItemID;
		CUtlSymbol		m_KeyBinding;
	};
	
	// dimhotepus: unsigned short -> int.
	CUtlDict< MenuItem_t, int >	m_Items;
};


#endif // TOOLMENUBUTTON_H

