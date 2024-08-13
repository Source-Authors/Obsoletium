//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef LABELEDCOMMANDCOMBOBOX_H
#define LABELEDCOMMANDCOMBOBOX_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Panel.h>
#include "utlvector.h"

class CLabeledCommandComboBox : public vgui::ComboBox
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CLabeledCommandComboBox, vgui::ComboBox );

public:
	CLabeledCommandComboBox(vgui::Panel *parent, const char *panelName);
	~CLabeledCommandComboBox();

	virtual void DeleteAllItems();
	virtual void AddItem(char const *text, char const *engineCommand);
	void ActivateItem(int itemIndex) override;
	const char *GetActiveItemCommand();

	void SetInitialItem(int itemIndex);

	void			ApplyChanges();
	void			Reset();
	bool			HasBeenModified();
	
	enum
	{
		MAX_NAME_LEN = 256,
		MAX_COMMAND_LEN = 256
	};
	
private:
	MESSAGE_FUNC_CHARPTR( OnTextChanged, "TextChanged", text );

	struct COMMANDITEM
	{
		char			name[ MAX_NAME_LEN ];
		char			command[ MAX_COMMAND_LEN ];
		int				comboBoxID;
	};

	CUtlVector< COMMANDITEM >	m_Items;
	int		m_iCurrentSelection;
	int		m_iStartSelection;
};

#endif // LABELEDCOMMANDCOMBOBOX_H
