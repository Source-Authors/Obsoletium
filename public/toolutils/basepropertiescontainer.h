//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef BASEPROPERTIESCONTAINER_H
#define BASEPROPERTIESCONTAINER_H

#include "dme_controls/ElementPropertiesTree.h"

class CBasePropertiesContainer : public CElementPropertiesTreeInternal
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CBasePropertiesContainer, CElementPropertiesTreeInternal);

public:

	CBasePropertiesContainer( vgui::Panel *parent, IDmNotify *pNotify, CDmeEditorTypeDictionary *pDict = NULL );

	bool IsDroppable( CUtlVector< KeyValues * >& msglist ) override;
	void OnPanelDropped( CUtlVector< KeyValues * >& msglist ) override;
};

#endif // BASEPROPERTIESCONTAINER_H
