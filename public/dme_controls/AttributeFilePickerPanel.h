//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTEFILEPICKERPANEL_H
#define ATTRIBUTEFILEPICKERPANEL_H
																												
#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/AttributeBasePickerPanel.h"
#include "vgui_controls/PHandle.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;

namespace vgui
{
	class FileOpenDialog;
}


//-----------------------------------------------------------------------------
// CAttributeFilePickerPanel
//-----------------------------------------------------------------------------
class CAttributeFilePickerPanel : public CAttributeBasePickerPanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CAttributeFilePickerPanel, CAttributeBasePickerPanel );

public:
	CAttributeFilePickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info );
	~CAttributeFilePickerPanel();

private:
	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );
	void ShowPickerDialog() override;
	virtual void SetupFileOpenDialog( vgui::FileOpenDialog *pDialog ) = 0;
};


//-----------------------------------------------------------------------------
// Macro to quickly make new attribute types
//-----------------------------------------------------------------------------
#define DECLARE_ATTRIBUTE_FILE_PICKER( _className )								\
	class _className : public CAttributeFilePickerPanel							\
	{																			\
		DECLARE_CLASS_SIMPLE_OVERRIDE( _className, CAttributeFilePickerPanel );	\
	public:																		\
		_className( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :	\
			BaseClass( parent, info ) {}										\
	private:																	\
		void SetupFileOpenDialog( vgui::FileOpenDialog *pDialog ) override;		\
	}

#define IMPLEMENT_ATTRIBUTE_FILE_PICKER( _className, _popupTitle, _assetType, _assetExt )	\
	void _className::SetupFileOpenDialog( vgui::FileOpenDialog *pDialog )		\
	{																			\
		pDialog->SetTitle( _popupTitle, true );									\
		pDialog->AddFilter( "*."  _assetExt, _assetType " (*." _assetExt ")", true ); \
		pDialog->AddFilter( "*.*", "All Files (*.*)", false );					\
	}


//-----------------------------------------------------------------------------
// File picker types
//-----------------------------------------------------------------------------
DECLARE_ATTRIBUTE_FILE_PICKER( CAttributeTgaFilePickerPanel );
DECLARE_ATTRIBUTE_FILE_PICKER( CAttributeDmeFilePickerPanel );
DECLARE_ATTRIBUTE_FILE_PICKER( CAttributeAviFilePickerPanel );
DECLARE_ATTRIBUTE_FILE_PICKER( CAttributeShtFilePickerPanel );
DECLARE_ATTRIBUTE_FILE_PICKER( CAttributeRawFilePickerPanel );


#endif // ATTRIBUTEFILEPICKERPANEL_H
