//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DIRECTORYSELECTDIALOG_H
#define DIRECTORYSELECTDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Controls.h>
#include <vgui_controls/TreeView.h>
#include <vgui_controls/Frame.h>

namespace vgui
{

//-----------------------------------------------------------------------------
// Purpose: Used to handle dynamically populating the tree view
//-----------------------------------------------------------------------------
class DirectoryTreeView : public TreeView
{
public:
	DirectoryTreeView(DirectorySelectDialog *parent, const char *name);
	void GenerateChildrenOfNode(int itemIndex) override;

private:
	DirectorySelectDialog *m_pParent;
};

//-----------------------------------------------------------------------------
// Purpose: Utility dialog, used to let user select a directory (like during install)
//-----------------------------------------------------------------------------
class DirectorySelectDialog : public Frame
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( DirectorySelectDialog, Frame );

public:
	DirectorySelectDialog(vgui::Panel *parent, const char *title);

	// sets where it should start searching
	void SetStartDirectory(const char *path);

	// sets what name should show up by default in the create directory dialog
	void SetDefaultCreateDirectoryName(const char *defaultCreateDirName);

	// opens the dialog
	void DoModal() override;

	/* action signals

		"DirectorySelected"
			"dir"	- the directory that was selected

	*/

	// Expand the tree nodes to match a supplied path, optionally selecting the final directory
	void ExpandTreeToPath( const char *lpszPath, bool bSelectFinalDirectory = true );

protected:
	void PerformLayout() override;
	void ApplySchemeSettings(IScheme *pScheme) override;
	void OnClose() override;

	// command buttons
	void OnCommand(const char *command) override;

private:
	MESSAGE_FUNC( OnTextChanged, "TextChanged" );
	MESSAGE_FUNC( OnTreeViewItemSelected, "TreeViewItemSelected" );
	MESSAGE_FUNC_CHARPTR( OnCreateDirectory, "CreateDirectory", dir );
	void BuildDirTree();
	void BuildDriveChoices();
	void ExpandTreeNode(const char *path, int parentNodeIndex);
	void GenerateChildrenOfDirectoryNode(int nodeIndex);
	void GenerateFullPathForNode(int nodeIndex, char *path, int pathBufferSize);
	bool DoesDirectoryHaveSubdirectories(const char *path, const char *dir);

	char m_szCurrentDir[512];
	char m_szDefaultCreateDirName[64];
	char m_szCurrentDrive[16];
	vgui::TreeView *m_pDirTree;
	vgui::ComboBox *m_pDriveCombo;
	vgui::Button *m_pCancelButton;
	vgui::Button *m_pSelectButton;
	vgui::Button *m_pCreateButton;
	
	friend class DirectoryTreeView;
};

} // namespace vgui


#endif // DIRECTORYSELECTDIALOG_H
