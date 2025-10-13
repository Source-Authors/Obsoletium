//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CHECKBUTTONLIST_H
#define CHECKBUTTONLIST_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>
#include "tier1/utlvector.h"

namespace vgui
{

//-----------------------------------------------------------------------------
// Purpose: Contains a list of check boxes, displaying scrollbars if necessary
//-----------------------------------------------------------------------------
class CheckButtonList : public EditablePanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CheckButtonList, EditablePanel );

public:
	CheckButtonList(Panel *parent, const char *name);
	~CheckButtonList();

	// adds a check button to the list
	intp AddItem(const char *itemText, bool startsSelected, KeyValues *userData);

	// clears the list
	void RemoveAll();

	// number of items in list that are checked
	intp GetCheckedItemCount() const;

	// item iteration
	bool IsItemIDValid(intp itemID) const;
	intp GetHighestItemID() const;
	intp GetItemCount() const;

	// item info
	KeyValues *GetItemData(intp itemID);
	bool IsItemChecked(intp itemID);
	void SetItemCheckable(intp itemID, bool state);

	/* MESSAGES SENT
		"CheckButtonChecked" - sent when one of the check buttons state has changed

	*/

protected:
	void PerformLayout() override;
	void ApplySchemeSettings(IScheme *pScheme) override;
	void OnMouseWheeled(int delta) override;

private:
	MESSAGE_FUNC_PARAMS( OnCheckButtonChecked, "CheckButtonChecked", pParams );
	MESSAGE_FUNC( OnScrollBarSliderMoved, "ScrollBarSliderMoved" );

	struct CheckItem_t
	{
		vgui::CheckButton *checkButton;
		KeyValues *userData;
	};
	CUtlVector<CheckItem_t> m_CheckItems;
	vgui::ScrollBar *m_pScrollBar;
};

}

#endif // CHECKBUTTONLIST_H
