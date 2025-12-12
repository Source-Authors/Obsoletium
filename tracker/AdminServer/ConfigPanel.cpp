//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "ConfigPanel.h"

#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>

#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ToggleButton.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/RadioButton.h>

#include "tier1/KeyValues.h"

using namespace vgui;

static constexpr inline long RETRY_TIME = 10000;		// refresh server every 10 seconds

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CConfigPanel::CConfigPanel(vgui::Panel *parent, bool autorefresh,bool savercon,int refreshtime,
							bool graphs, int graphsrefreshtime,bool getlogs) : Frame(parent, "ConfigPanel")
{
	
	// dimhotepus: Scale UI.
	SetMinimumSize(QuickPropScale( 400 ),QuickPropScale( 240 ));
	SetSizeable(false);
	MakePopup();

	m_pOkayButton = new Button(this, "Okay", "#Okay_Button");
	m_pCloseButton = new Button(this, "Close", "#Close_Button");

	m_pRefreshCheckButton = new CheckButton(this, "RefreshCheckButton", "");

	m_pRconCheckButton = new CheckButton(this, "RconCheckButton", "");

	m_pRefreshTextEntry= new TextEntry(this,"RefreshTextEntry");

	m_pGraphsButton = new CheckButton(this, "GraphsButton", "");
	m_pGraphsRefreshTimeTextEntry= new TextEntry(this,"GraphsRefreshTimeTextEntry");

	m_pLogsButton = new CheckButton(this, "LogsButton", "");

	SetTitle("My servers - Options",true);

	LoadControlSettings("Admin\\ConfigPanel.res", "PLATFORM");

	m_pRefreshCheckButton->SetSelected(autorefresh);
	m_pRconCheckButton->SetSelected(savercon);
	m_pGraphsButton->SetSelected(graphs);
	m_pLogsButton->SetSelected(getlogs);

	m_pRefreshTextEntry->SetEnabled(m_pRefreshCheckButton->IsSelected());
	m_pRefreshTextEntry->SetEditable(m_pRefreshCheckButton->IsSelected());
	m_pGraphsRefreshTimeTextEntry->SetEnabled(m_pGraphsButton->IsSelected());
	m_pGraphsRefreshTimeTextEntry->SetEditable(m_pGraphsButton->IsSelected());

	char refreshText[16];

	V_to_chars(refreshText,refreshtime);
	m_pRefreshTextEntry->SetText(refreshText);
	
	V_to_chars(refreshText,graphsrefreshtime);
	m_pGraphsRefreshTimeTextEntry->SetText(refreshText);

	SetVisible(true);

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CConfigPanel::~CConfigPanel()
{
}

//-----------------------------------------------------------------------------
// Purpose: Activates the dialog
//-----------------------------------------------------------------------------
void CConfigPanel::Run()
{
	RequestFocus();
}


//-----------------------------------------------------------------------------
// Purpose: Deletes the dialog when it's closed
//-----------------------------------------------------------------------------
void CConfigPanel::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: turn on and off components when check boxes are checked
//-----------------------------------------------------------------------------
void CConfigPanel::OnButtonToggled(Panel *panel)
{
	if (panel == m_pRefreshCheckButton) 
		// you can only edit the refresh time if you allow auto refresh
	{
		m_pRefreshTextEntry->SetEnabled(m_pRefreshCheckButton->IsSelected());
		m_pRefreshTextEntry->SetEditable(m_pRefreshCheckButton->IsSelected());
	}
	else if (panel == m_pGraphsButton) 
		// you can only edit the refresh time if you allow auto refresh
	{
		m_pGraphsRefreshTimeTextEntry->SetEnabled(m_pGraphsButton->IsSelected());
		m_pGraphsRefreshTimeTextEntry->SetEditable(m_pGraphsButton->IsSelected());
	}


	InvalidateLayout();
}


//-----------------------------------------------------------------------------
// Purpose: Sets the text of a control by name
//-----------------------------------------------------------------------------
void CConfigPanel::SetControlText(const char *textEntryName, const char *text)
{
	TextEntry *entry = dynamic_cast<TextEntry *>(FindChildByName(textEntryName));
	if (entry)
	{
		entry->SetText(text);
	}
}



//-----------------------------------------------------------------------------
// Purpose: Parse posted messages
//			 
//-----------------------------------------------------------------------------
void CConfigPanel::OnCommand(const char *command)
{

	if(!stricmp(command,"okay"))
	{ // save away the new settings
		char timeText[20];
		int time,timeGraphs;

		m_pRefreshTextEntry->GetText(timeText);
		// dimhotepus: Check time is read.
		const bool isReadTime = sscanf(timeText,"%i",&time) == 1;
	
		BitwiseClear(timeText);
		m_pGraphsRefreshTimeTextEntry->GetText(timeText);
		// dimhotepus: Check graphs are read.
		const bool isReadGraphs = sscanf(timeText,"%i",&timeGraphs) == 1;

		if (isReadTime && time > 0 && time < 9999 &&
			isReadGraphs && timeGraphs > 0 && timeGraphs < 9999)
		{
			OnClose();
		}
		else
		{
			// dimhotepus: Scale UI.
			MessageBox *dlg = new MessageBox ("#Config_Panel", "#Config_Time_Error", this);
			dlg->DoModal();
		}
	}
	else if(!stricmp(command,"close") )
	{
		Close();
	}
}
