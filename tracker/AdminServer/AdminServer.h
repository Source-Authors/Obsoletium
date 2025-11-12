//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef ADMINSERVER_H
#define ADMINSERVER_H
#ifdef _WIN32
#pragma once
#endif

#include "IAdminServer.h"
#include "IVGuiModule.h"

#include "tier1/utlvector.h"

class CServerPage;

//-----------------------------------------------------------------------------
// Purpose: Handles the UI and pinging of a half-life game server list
//-----------------------------------------------------------------------------
class CAdminServer : public IAdminServer, public IVGuiModule
{
public:
	CAdminServer();
	virtual ~CAdminServer();

	// IVGui module implementation
	bool Initialize(CreateInterfaceFn *factorylist, intp numFactories) override;
	bool PostInitialize(CreateInterfaceFn *modules, intp factoryCount) override;
	vgui::VPANEL GetPanel() override;
	bool Activate() override;
	bool IsValid() override;
	void Shutdown() override;
	void Deactivate() override;
	void Reactivate() override;
	void SetParent(vgui::VPANEL parent) override;

	// IAdminServer implementation
	// opens a manage server dialog for a local server
	ManageServerUIHandle_t OpenManageServerDialog(const char *serverName, const char *gameDir) override;

	// opens a manage server dialog to a remote server
	ManageServerUIHandle_t OpenManageServerDialog(unsigned int gameIP, unsigned int gamePort, const char *password) override;

	// forces the game info dialog closed
	void CloseManageServerDialog(ManageServerUIHandle_t gameDialog) override;

	// Gets a handle to the interface
	IManageServer *GetManageServerInterface(ManageServerUIHandle_t handle) override;

private:
	struct OpenedManageDialog_t
	{
		// dimhotepus: unsigned long -> HPanel for x86-64 portability.
		vgui::HPanel handle;
		IManageServer *manageInterface;
	};
	CUtlVector<OpenedManageDialog_t> m_OpenedManageDialog;
	vgui::VPANEL m_hParent;
};


class IVProfExport;
extern IVProfExport *g_pVProfExport;


#endif // AdminServer_H
