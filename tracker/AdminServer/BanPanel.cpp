//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "BanPanel.h"
#include "PlayerContextMenu.h"
#include "DialogAddBan.h"

#include <vgui/Cursor.h>
#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <vgui/IVGui.h>
#include <tier1/KeyValues.h>

#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/QueryBox.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/FileOpenDialog.h>

#include "filesystem.h"
#include "posix_file_stream.h"

#include "DialogCvarChange.h"
#include "tokenline.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBanPanel::CBanPanel(vgui::Panel *parent, const char *name) : PropertyPage(parent, name)
{
	m_pBanListPanel = new ListPanel(this, "BanList");
	// dimhotepus: Scale UI.
	m_pBanListPanel->AddColumnHeader(0, "type", "#Ban_List_Type", QuickPropScale( 150 ) );
	m_pBanListPanel->AddColumnHeader(1, "id", "#Ban_List_ID", QuickPropScale( 200 ) );
	m_pBanListPanel->AddColumnHeader(2, "time", "#Ban_List_Time", QuickPropScale( 200 ) );
	m_pBanListPanel->SetSortColumn(2);
	m_pBanListPanel->SetEmptyListText("#Ban_List_Empty");

	m_pAddButton = new Button(this, "Add", "#Ban_List_Add");
	m_pRemoveButton = new Button(this, "Remove", "#Ban_List_Remove");
	m_pChangeButton = new Button(this, "Change", "#Ban_List_Edit");
	m_pImportButton = new Button(this, "Import", "#Ban_List_Import");

	m_pAddButton->SetCommand(new KeyValues("addban"));
	m_pRemoveButton->SetCommand(new KeyValues("removeban"));
	m_pChangeButton->SetCommand(new KeyValues("changeban"));
	m_pImportButton->SetCommand(new KeyValues("importban"));

	m_pBanContextMenu = new CBanContextMenu(this);
	m_pBanContextMenu->SetVisible(false);

	m_flUpdateTime = 0.0;
	m_bPageViewed = false;

	LoadControlSettings("Admin/BanPanel.res", "PLATFORM");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBanPanel::~CBanPanel()
{
}

//-----------------------------------------------------------------------------
// Purpose: Activates the page
//-----------------------------------------------------------------------------
void CBanPanel::OnPageShow()
{
	BaseClass::OnPageShow();
	OnItemSelected();
	if (!m_bPageViewed)
	{
		m_bPageViewed = true;
		// force update on first page view
		m_flUpdateTime = 0.0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Requests new data set from server
//-----------------------------------------------------------------------------
void CBanPanel::OnResetData()
{
	RemoteServer().RequestValue(this, "banlist");
	// update once every 1 minute
	m_flUpdateTime = system()->GetFrameTime() + 60.0;
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if the page data should be refreshed
//-----------------------------------------------------------------------------
void CBanPanel::OnThink()
{
	if (m_flUpdateTime < system()->GetFrameTime())
	{
		OnResetData();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Wrap g_pVGuiLocalize->Find() and not return NULL
//-----------------------------------------------------------------------------
static const wchar_t * LocalizeFind( const char *identifier, const wchar_t *defaultText )
{
	const wchar_t *str = g_pVGuiLocalize->Find(identifier);
	if ( !str )
		str = defaultText;
	return str;
}

//-----------------------------------------------------------------------------
// Purpose: Received response from server containing data
//-----------------------------------------------------------------------------
void CBanPanel::OnServerDataResponse(const char *value, const char *response)
{
	// build the list
	if (!stricmp(value, "banlist"))
	{
		// clear current list
		m_pBanListPanel->DeleteAllItems();

		// scan through response for all items
		int item = 0;
		float banTime = 0.0f;
		char id[64] = { 0 };
		// dimhotepus: Ensure id does not overflow.
		// dimhotepus: double banTime -> float.
		while (3 == sscanf(response, "%i %63s : %f min\n", &item, id, &banTime))
		{
			id[std::size(id) - 1] = '\0';

			KeyValuesAD ban("ban");

			// determine type
			if (IsIPAddress(id))
			{
				// ip address
				ban->SetWString("type", LocalizeFind("#Ban_IP", L"IP Address"));
			}
			else
			{
				// must be a userID
				ban->SetWString("type", LocalizeFind("#Ban_Auth_ID", L"AuthID"));
			}
			ban->SetString("id", id);

			if (banTime > 0.0)
			{
				ban->SetFloat("time", banTime);
			}
			else
			{
				ban->SetWString("time", LocalizeFind("#Ban_Permanent", L"permanent"));
			}
			
			// add to list
			m_pBanListPanel->AddItem(ban, 0, false, false);

			// move to the next item
			response = strchr(response, '\n');
			if (!response)
				break;

			response++;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Refreshes the list on the user hitting F5
//-----------------------------------------------------------------------------
void CBanPanel::OnKeyCodeTyped(vgui::KeyCode code)
{
	if (code == KEY_F5)
	{
		OnResetData();
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}


//-----------------------------------------------------------------------------
// Purpose: opens context menu (user right clicked on a server)
//-----------------------------------------------------------------------------
void CBanPanel::OnOpenContextMenu(int row)
{
/* CONTEXT MENU CODE TEMPORARILY DISABLED UNTIL VERIFIED AS WORKING
	if (m_pBanListPanel->IsVisible() && m_pBanListPanel->IsCursorOver()
		&& m_pBanListPanel->GetNumSelectedRows())
	// show the ban changing menu IF its the visible panel and the cursor is
	// over it 
	{
	
		unsigned int banID =m_pBanListPanel->GetSelectedRow(0);
			
		// activate context menu
		m_pBanContextMenu->ShowMenu(this, banID);
	} 
	else
	{
		m_pBanContextMenu->ShowMenu(this, -1);
	}
*/
}

//-----------------------------------------------------------------------------
// Purpose: Manually adds a new ban
//-----------------------------------------------------------------------------
void CBanPanel::AddBan()
{
	CDialogAddBan *box = new CDialogAddBan(this);
	box->AddActionSignalTarget(this);
	box->Activate("addban", "", "");
}

//-----------------------------------------------------------------------------
// Purpose: prompts the user to remove an existing ban
//-----------------------------------------------------------------------------
void CBanPanel::RemoveBan()
{
	intp itemID = m_pBanListPanel->GetSelectedItem(0);
	if ( itemID == -1 )
		return;

	// ask the user whether or not they want to remove the ban
	KeyValues *kv = m_pBanListPanel->GetItem(itemID);
	if (kv != NULL)
	{
		// build the message
		wchar_t id[256];
		g_pVGuiLocalize->ConvertANSIToUnicode(kv->GetString("id"), id);
		wchar_t message[256];
		g_pVGuiLocalize->ConstructString_safe(message, g_pVGuiLocalize->Find("#Ban_Remove_Msg"), 1, id);

		// activate the confirmation dialog
		// dimhotepus: Own query box to scale it.
		QueryBox *box = new QueryBox(g_pVGuiLocalize->Find("#Ban_Title_Remove"), message, this);
		box->SetOKCommand(new KeyValues("removebanbyid", "id", kv->GetString("id"))); //-V2017
		box->AddActionSignalTarget(this);
		box->DoModal();
	}
}

//-----------------------------------------------------------------------------
// Purpose: change the time length of a ban
//-----------------------------------------------------------------------------
void CBanPanel::ChangeBan()
{	
	intp itemID = m_pBanListPanel->GetSelectedItem(0);
	if (itemID == -1)
		return;

	KeyValues *kv = m_pBanListPanel->GetItem(itemID);
	if (kv != NULL)
	{	
		char timeText[20];
		float time = kv->GetFloat("time");
		V_sprintf_safe(timeText, "%0.2f", time);

		// open a dialog asking them what time to change the ban lenght to
		CDialogCvarChange *box = new CDialogCvarChange(this);
		box->AddActionSignalTarget(this);
		box->SetTitle("#Ban_Title_Change", true);
		box->Activate(kv->GetString("id"), timeText, "changeban", "#Ban_Change_Time");
	}
}

//-----------------------------------------------------------------------------
// Purpose: Removes the specified ban
//-----------------------------------------------------------------------------
void CBanPanel::RemoveBanByID(const char *id)
{
	Assert(id && *id);
	if (!id || !*id)
		return;

	// send down the command
	char cmd[512];
	V_sprintf_safe(cmd, "%s %s\n", IsIPAddress(id) ? "removeip" : "removeid", id);
	RemoteServer().SendCommand(cmd);

	// force the file to be written
	if (IsIPAddress(id))
	{
		RemoteServer().SendCommand("writeip");
	}
	else
	{
		RemoteServer().SendCommand("writeid");
	}

	// refresh
	OnResetData();
}

//-----------------------------------------------------------------------------
// Purpose: Changes a ban
//-----------------------------------------------------------------------------
void CBanPanel::ChangeBanTimeByID(const char *id, const char *newtime)
{
	Assert(id && *id);
	if (!id || !*id)
		return;

	// if the newtime string is not valid, then set it to 0 (permanent ban)
	// dimhotepus: atof -> strtof
	if (!newtime || strtof(newtime, nullptr) < 0.001f)
	{
		newtime = "0";
	}

	// send down the command
	char cmd[512];
	V_sprintf_safe(cmd, "%s %s %s\n", IsIPAddress(id) ? "addip" : "banid", newtime, id);
	RemoteServer().SendCommand(cmd);
	if (IsIPAddress(id))
	{
		RemoteServer().SendCommand("writeip");
	}
	else
	{
		RemoteServer().SendCommand("writeid");
	}

	// refresh
	OnResetData();
}

//-----------------------------------------------------------------------------
// Purpose: Changes a ban
//-----------------------------------------------------------------------------
void CBanPanel::OnCvarChangeValue( KeyValues *kv )
{
	const char *idText = kv->GetString( "player", "" );
	const char *durationText = kv->GetString( "value", "0" );

	ChangeBanTimeByID( idText, durationText );
}

//-----------------------------------------------------------------------------
// Purpose: called when a row on the list panel is selected.
//-----------------------------------------------------------------------------
void CBanPanel::OnItemSelected()
{
	intp itemID = m_pBanListPanel->GetSelectedItem(0);
	if (itemID == -1)
	{
		m_pRemoveButton->SetEnabled(false);
		m_pChangeButton->SetEnabled(false);
	}
	else
	{
		m_pRemoveButton->SetEnabled(true);
		m_pChangeButton->SetEnabled(true);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Asks the user for the ban file to import
//-----------------------------------------------------------------------------
void CBanPanel::ImportBanList()
{
	// Pop up the dialog
	FileOpenDialog *pFileDialog = new FileOpenDialog(this, "#Ban_Find_Ban_File", true);
	pFileDialog->AddFilter( "*.cfg", "#Config_files", true );
	pFileDialog->AddFilter( "*.*", "#All_files", false );
	pFileDialog->DoModal(true);
	pFileDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: When a file is selected print out its full path in the debugger
//-----------------------------------------------------------------------------
void CBanPanel::OnFileSelected(const char *fullpath)
{
	char line[255];
	TokenLine tok;

	// this can take a while, put up a waiting cursor
	const vgui::ScopedSurfaceWaitCursor scopedWaitCursor(surface());

	// we don't use filesystem() here because we want to let the user pick
	// a file from anywhere on their filesystem... so we use stdio
	auto [f, rc] = se::posix::posix_file_stream_factory::open(fullpath, "rb");
	if (!rc)
	{
		while (std::get<char *>(f.gets(line)))
		{
			// parse each line of the config file adding the ban
			tok.SetLine(line);
			if (tok.CountToken() == 3)
			{
				// add the ban
				const char *id = tok.GetToken(2);
				ChangeBanTimeByID(id, "0");
			}
		}
	}
	else
	{
		Warning("Unable to load bans from %s: %s.\n", fullpath, rc.message().c_str());
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the id string is an IP address, false if it's a WON or STEAM ID
//-----------------------------------------------------------------------------
[[nodiscard]] bool CBanPanel::IsIPAddress(const char *id)
{
	int s1, s2, s3, s4;
	return (4 == sscanf(id, "%d.%d.%d.%d", &s1, &s2, &s3, &s4));
}
