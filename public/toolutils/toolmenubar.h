//========= Copyright Valve Corporation, All rights reserved. ============//
//
// This menu bar displays a couple extra labels:
// one which contains the tool name, and one which contains a arbitrary info 
//
//=============================================================================

#ifndef TOOLMENUBAR_H
#define TOOLMENUBAR_H


#include "vgui_controls/MenuBar.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace vgui
{
	class Panel;
	class Label;
}

class CBaseToolSystem;

//-----------------------------------------------------------------------------
// Main menu bar
//-----------------------------------------------------------------------------
class CToolMenuBar : public vgui::MenuBar
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CToolMenuBar, vgui::MenuBar );

public:
	CToolMenuBar( CBaseToolSystem *parent, const char *panelName );
	void PerformLayout() override;
	void SetToolName( const char *name );
	void SetInfo( const char *text );

	CBaseToolSystem *GetToolSystem();

protected:
	vgui::Label		*m_pInfo;
	vgui::Label		*m_pToolName;
	CBaseToolSystem *m_pToolSystem;
};


//-----------------------------------------------------------------------------
// Main menu bar version that stores file name on it
//-----------------------------------------------------------------------------
class CToolFileMenuBar : public CToolMenuBar
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CToolFileMenuBar, CToolMenuBar );

public:
	CToolFileMenuBar( CBaseToolSystem *parent, const char *panelName );
	void PerformLayout() override;
	void SetFileName( const char *pFileName );

private:
	vgui::Label		*m_pFileName;
};


#endif // TOOLMENUBAR_H