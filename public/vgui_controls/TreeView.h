//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TREEVIEW_H
#define TREEVIEW_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utllinkedlist.h"
#include <tier1/utlvector.h>
#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

class KeyValues;

namespace vgui
{

class ExpandButton;
class TreeNode;
class TreeViewSubPanel;

// sorting function, should return true if node1 should be displayed before node2
typedef bool (*TreeViewSortFunc_t)(KeyValues *node1, KeyValues *node2);

class TreeView : public Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( TreeView, Panel );

public:
    TreeView(Panel *parent, const char *panelName);
    ~TreeView();

    void SetSortFunc(TreeViewSortFunc_t pSortFunc);

    virtual intp AddItem(KeyValues *data, intp parentItemIndex);

	virtual intp GetRootItemIndex();
	virtual intp GetNumChildren( intp itemIndex );
	virtual intp GetChild( intp iParentItemIndex, intp iChild ); // between 0 and GetNumChildren( iParentItemIndex ).

    virtual intp GetItemCount(void);
    virtual KeyValues *GetItemData(intp itemIndex);
    virtual void RemoveItem(intp itemIndex, bool bPromoteChildren, bool bRecursivelyRemove = false );
    virtual void RemoveAll();
    virtual bool ModifyItem(intp itemIndex, KeyValues *data);
	virtual intp GetItemParent(intp itemIndex);

    virtual void SetFont(HFont font);

    virtual void SetImageList(ImageList *imageList, bool deleteImageListWhenDone);

	void SetAllowMultipleSelections( bool state );
	bool IsMultipleSelectionAllowed() const;

	virtual void ClearSelection();
    virtual void AddSelectedItem( intp itemIndex, bool clearCurrentSelection, bool requestFocus = true, bool bMakeItemVisible = true );
	virtual void RemoveSelectedItem( intp itemIndex );
	virtual void SelectAll();

	virtual bool IsItemSelected( intp itemIndex );
	virtual void RangeSelectItems( intp clickedItem );
	virtual void FindNodesInRange( intp startItem, intp endItem, CUtlVector< intp >& itemIndices );

	// returns the id of the currently selected item, -1 if nothing is selected
	virtual intp GetSelectedItemCount() const;
	virtual intp GetFirstSelectedItem() const;
	virtual void GetSelectedItems( CUtlVector< intp >& list );
	virtual void GetSelectedItemData( CUtlVector< KeyValues * >& list );

	// set colors for individual elments
	virtual void SetItemFgColor(intp itemIndex, const Color& color);
	virtual void SetItemBgColor(intp itemIndex, const Color& color);
	virtual void SetItemSelectionTextColor( intp itemIndex, const Color& clr );
	virtual void SetItemSelectionBgColor( intp itemIndex, const Color& clr );
	virtual void SetItemSelectionUnfocusedBgColor( intp itemIndex, const Color& clr );

	// returns true if the itemID is valid for use
	virtual bool IsItemIDValid(intp itemIndex);

	// item iterators
	// iterate from [0..GetHighestItemID()], 
	// and check each with IsItemIDValid() before using
	virtual intp GetHighestItemID();

    virtual void ExpandItem(intp itemIndex, bool bExpand);
	virtual bool IsItemExpanded(intp itemIndex);

    virtual void MakeItemVisible(intp itemIndex);
	
	// This tells which of the visible items is the top one.
	virtual void GetVBarInfo( int &top, int &nItemsVisible, bool& hbarVisible );

	virtual HFont GetFont();

	virtual void GenerateDragDataForItem( intp itemIndex, KeyValues *msg );
	virtual void SetDragEnabledItems( bool state );

	virtual void OnLabelChanged( intp itemIndex, char const *oldString, char const *newString );
	virtual bool IsLabelEditingAllowed() const;
	virtual bool IsLabelBeingEdited() const;
	virtual void SetAllowLabelEditing( bool state );

	/* message sent

		"TreeViewItemSelected"  int "itemIndex"
			called when the selected item changes
		"TreeViewItemDeselected" int "itemIndex"
			called when item is deselected
	*/
    int GetRowHeight();
	int GetVisibleMaxWidth();
	void OnMousePressed(MouseCode code) override;

	// By default, the tree view expands nodes on left-click. This enables/disables that feature
	void EnableExpandTreeOnLeftClick( bool bEnable );

	virtual void SetLabelEditingAllowed( intp itemIndex, bool state );
	virtual void StartEditingLabel( intp itemIndex );

	virtual bool IsItemDroppable( intp itemIndex, CUtlVector< KeyValues * >& msglist );
	virtual void OnItemDropped( intp itemIndex, CUtlVector< KeyValues * >& msglist );
	virtual bool GetItemDropContextMenu( intp itemIndex, Menu *menu, CUtlVector< KeyValues * >& msglist );
	virtual HCursor GetItemDropCursor( intp itemIndex, CUtlVector< KeyValues * >& msglist );

	virtual intp		GetPrevChildItemIndex( intp itemIndex );
	virtual intp		GetNextChildItemIndex( intp itemIndex );

	void PerformLayout() override;

	// Makes the scrollbar parented to some other panel...
	ScrollBar	*SetScrollBarExternal( bool vertical, Panel *newParent );
	void		GetScrollBarSize( bool vertical, int& w, int& h );

	void		SetMultipleItemDragEnabled( bool state ); // if this is set, then clicking on one row and dragging will select a run or items, etc.
	bool		IsMultipleItemDragEnabled() const;

	intp		FindItemUnderMouse( int mx, int my );

protected:
	// functions to override
	// called when a node, marked as "Expand", needs to generate it's child nodes when expanded
	virtual void GenerateChildrenOfNode(intp) {}

	// override to open a custom context menu on a node being selected and right-clicked
	virtual void GenerateContextMenu( intp, int, int ) {}

	// overrides
	void OnMouseWheeled(int delta) override;
	void OnSizeChanged(int wide, int tall) override; 
	void ApplySchemeSettings(IScheme *pScheme) override;
	MESSAGE_FUNC_INT( OnSliderMoved, "ScrollBarSliderMoved", position );
	void SetBgColor( Color color ) override;

private:
    friend class TreeNode;
	friend class TreeNodeText;

	TreeNode* GetItem( intp itemIndex );
	virtual void RemoveChildrenOfNode( intp itemIndex );
	void SetLabelBeingEdited( bool state );

	// Clean up the image list
	void CleanUpImageList( );

	// to be accessed by TreeNodes
    IImage* GetImage(intp index);        

	// bools
	bool m_bAllowLabelEditing : 1;
	bool m_bDragEnabledItems : 1;
	bool m_bDeleteImageListWhenDone : 1;
	bool m_bLeftClickExpandsTree : 1;
	bool m_bLabelBeingEdited : 1;
	bool m_bMultipleItemDragging : 1;
	bool m_bAllowMultipleSelections : 1;

    // cross reference - no hierarchy ordering in this list
    CUtlLinkedList<TreeNode *, intp>   m_NodeList;
   	ScrollBar					*m_pHorzScrollBar, *m_pVertScrollBar;
	int							m_nRowHeight;

	ImageList					*m_pImageList;
    TreeNode					*m_pRootNode;
    TreeViewSortFunc_t			m_pSortFunc;
    HFont						m_Font;

    CUtlVector< TreeNode * >	m_SelectedItems;
    TreeViewSubPanel			*m_pSubPanel;

	intp						m_nMostRecentlySelectedItem;
	bool						m_bScrollbarExternal[ 2 ]; // 0 = vert, 1 = horz
};

}

#endif // TREEVIEW_H
