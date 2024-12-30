//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Handles host migration for a session (not for the game server)
//
//=============================================================================//

#include "matchmaking.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Start a Matchmaking session as the host 
//-----------------------------------------------------------------------------
CClientInfo *CMatchmaking::SelectNewHost()
{
	// For now, just grab the first guy in the list
	CClientInfo *pClient = &m_Local;
	for ( int i = 0; i < m_Remote.Count(); ++i )
	{
		if ( m_Remote[i]->m_id > pClient->m_id )
		{
			pClient = m_Remote[i];
		}
	}
	return pClient;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMatchmaking::StartHostMigration()
{
	SwitchToState( MMSTATE_HOSTMIGRATE_STARTINGMIGRATION );

	m_pNewHost = SelectNewHost();
	if ( m_pNewHost == &m_Local )
	{
		// We're the new host, so start hosting
		Msg( "Starting new host" );
		BeginHosting();
	}
	else
	{
		Msg( "Waiting for a new host" );
		SwitchToNewHost();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMatchmaking::BeginHosting()
{
	m_Session.SetIsHost( true );
	m_Host = m_Local;

	// Move into private slots
	if ( !m_Local.m_bInvited )
	{
		RemovePlayersFromSession( &m_Local );
		m_Local.m_bInvited = true;
		AddPlayersToSession( &m_Local );
	}

	if ( !m_Session.MigrateHost() )
	{
		Warning( "Session migrate failed!\n" );

		SessionNotification( SESSION_NOTIFY_FAIL_MIGRATE );
		return;
	}

	SwitchToState( MMSTATE_HOSTMIGRATE_MIGRATING );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMatchmaking::TellClientsToMigrate()
{
	Msg( "Sending migrate request\n" );

	m_fSendTimer = GetTime();
	++m_nSendCount;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMatchmaking::SwitchToNewHost()
{
	// Set a timer to wait for the host to contact us
	m_fWaitTimer = GetTime();

	// Get rid of the current host net channel
	MarkChannelForRemoval( &m_Host.m_adr );

	AddRemoteChannel( &m_pNewHost->m_adr );

	SwitchToState( MMSTATE_HOSTMIGRATE_WAITINGFORHOST );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMatchmaking::EndMigration()
{
	Msg( "Migration complete\n" );

	if ( m_Session.IsHost() )
	{
		// Drop any clients that failed to migrate
		for ( int i = m_Remote.Count()-1; i >= 0; --i )
		{
			ClientDropped( m_Remote[i] );
		}

		// Update the lobby to show the new host
		SendPlayerInfoToLobby( &m_Local, 0 );

		// X360TBD: Figure out what state we should be in
		int newState = m_PreMigrateState;
		switch( m_PreMigrateState )
		{
		case MMSTATE_SESSION_CONNECTING:
			newState = MMSTATE_ACCEPTING_CONNECTIONS;
			break;

		default:
			Warning( "Unhandled post-migrate state transition" );
		}

		// Don't use SwitchToState() to set our new state because when changing 
		// from a client to a host the state transition is usually invalid.
		m_CurrentState = newState;
	}
	else
	{
		// Still a client, just restore our previous state
		m_CurrentState =  m_PreMigrateState;
	}
}

void CMatchmaking::TestStats()
{

}
