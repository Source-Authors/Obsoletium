//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Handles joining clients together in a matchmaking session before a multiplayer
//			game, tracking new players and dropped players during the game, and reporting
//			game results and stats after the game is complete.
//
//=============================================================================//

#include "proto_oob.h"
#include "vgui_baseui_interface.h"
#include "cdll_engine_int.h"
#include "matchmaking.h"
#include "Session.h"
#include "convar.h"
#include "cmd.h"

extern IVEngineClient *engineClient;

// TODO: remove when UI sets all properties
#include "hl2orange.spa.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IXboxSystem *g_pXboxSystem;

//-----------------------------------------------------------------------------
// Purpose: Start a matchmaking client 
//-----------------------------------------------------------------------------
void CMatchmaking::StartClient( bool bSystemLink )
{
	NET_SetMutiplayer( true );

	InitializeLocalClient( false );

	m_Session.SetIsSystemLink( bSystemLink );

	// SearchForSession is an async call
	if ( !SearchForSession() )
	{
		// The call failed
		SessionNotification( SESSION_NOTIFY_FAIL_CREATE );
		return;
	}

	// Session search is underway
	SwitchToState( MMSTATE_SEARCHING );
}

//-----------------------------------------------------------------------------
// Purpose: Set up to search for a system link host
//-----------------------------------------------------------------------------
bool CMatchmaking::StartSystemLinkSearch()
{
	// Create an random identifier and reset the send timer
	m_fSendTimer = GetTime();
	m_nSendCount = 0;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handle replies from system link servers
//-----------------------------------------------------------------------------
void CMatchmaking::HandleSystemLinkReply( netpacket_t *pPacket )
{
	if ( !m_Session.IsSystemLink() || m_Session.IsHost() )
		return;

	uint64 nonce = pPacket->message.ReadLongLong();

	if ( nonce != m_Nonce )
	{
		// Reply isn't a response to our request
		return;
	}

	// Store the session information
	bf_read &msg = pPacket->message;

	alignas(systemLinkInfo_s*) char *pData = new char[MAX_ROUTABLE_PAYLOAD];

	systemLinkInfo_s *pResultInfo = (systemLinkInfo_s*)pData;
	XSESSION_SEARCHRESULT *pResult = &pResultInfo->Result;

	msg.ReadBytes( pResult->info );

	// Don't accept multiple replies from the same host
	for ( intp i = 0; i < m_pSystemLinkResults.Count(); ++i )
	{
		XSESSION_SEARCHRESULT *pCheck = &((systemLinkInfo_s*)m_pSystemLinkResults[i])->Result;
		if ( Q_memcmp( &pCheck->info.sessionID, &pResult->info.sessionID, sizeof( pResult->info.sessionID ) ) == 0 )
		{
			// Already have this session
			delete [] pData;
			return;
		}
	}

	pResult->dwOpenPublicSlots		= msg.ReadByte();
	pResult->dwOpenPrivateSlots		= msg.ReadByte();
	pResult->dwFilledPublicSlots	= msg.ReadByte();
	pResult->dwFilledPrivateSlots	= msg.ReadByte();

	m_nTotalTeams					= msg.ReadByte();
	pResultInfo->gameState			= msg.ReadByte();
	pResultInfo->gameTime			= msg.ReadByte();
	msg.ReadBytes( pResultInfo->szHostName );
	msg.ReadBytes( pResultInfo->szScenario );

	pResult->cProperties			= msg.ReadByte();
	pResult->cContexts				= msg.ReadByte();
	const uint propSize = pResult->cProperties * sizeof( XUSER_PROPERTY );
	const uint ctxSize = pResult->cContexts * sizeof( XUSER_CONTEXT );

	pResult->pProperties = (XUSER_PROPERTY*)( (byte*)pResult + sizeof( XSESSION_SEARCHRESULT ) );
	pResult->pContexts	 = (XUSER_CONTEXT*)( (byte*)pResult->pProperties + propSize );

	msg.ReadBytes( pResult->pProperties, propSize );
	msg.ReadBytes( pResult->pContexts, ctxSize);

	pResultInfo->iScenarioIndex = msg.ReadByte();
	pResultInfo->xuid = msg.ReadLongLong();

	m_pSystemLinkResults.AddToTail( pData );

	Msg( "Found a matching game\n" );
	DevMsg( "Result #%zd: %lu open public, %lu open private\n", m_pSystemLinkResults.Count()-1, pResult->dwOpenPublicSlots, pResult->dwOpenPrivateSlots );
}

//-----------------------------------------------------------------------------
// Purpose: Search for a session to join 
//-----------------------------------------------------------------------------
bool CMatchmaking::SearchForSession()
{
	if ( m_Session.IsSystemLink() )
	{
		return StartSystemLinkSearch();
	}

	m_pSearchResults = NULL;
	uint cbResultsBytes = 0;
	uint ret = 0;

	// Call once to get the necessary buffer size
	ret = g_pXboxSystem->SessionSearch( 
		SESSION_MATCH_QUERY_PLAYER_MATCH,
		XBX_GetPrimaryUserId(),
		MAX_SEARCHRESULTS,
		1,
		0,
		0,
		NULL,
		NULL,
		&cbResultsBytes,	// must be 0
		m_pSearchResults,			// must be NULL
		false						// synchronous
		);

	m_hSearchHandle = g_pXboxSystem->CreateAsyncHandle();

	// Allocate the buffer and call again
	m_pSearchResults = (XSESSION_SEARCHRESULT_HEADER*)malloc( cbResultsBytes );
	ret = g_pXboxSystem->SessionSearch( 
		SESSION_MATCH_QUERY_PLAYER_MATCH,	// Procedure index
		XBX_GetPrimaryUserId(),				// User index
		MAX_SEARCHRESULTS,					// Maximum results
		m_Local.m_cPlayers,					// Number of local players
 		m_SessionProperties.Count(),		// Number of properties
 		m_SessionContexts.Count(),			// Number of contexts
		m_SessionProperties.Base(),			// Properties
 		m_SessionContexts.Base(),			// Contexts
		&cbResultsBytes,					// Size of result buffer
		m_pSearchResults,					// Pointer to results
		true,
		&m_hSearchHandle
		);

	if ( ret != ERROR_IO_PENDING )
	{
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Check for session search results
//-----------------------------------------------------------------------------
void CMatchmaking::UpdateSearch()
{
	if ( !m_Session.IsSystemLink() )
	{
		// Check if the search has finished
		DWORD ret = g_pXboxSystem->GetOverlappedResult( m_hSearchHandle, NULL, false );
		if ( ret == ERROR_IO_INCOMPLETE )
		{
			// Still waiting
			return;
		}
		else
		{
			if ( ret == ERROR_SUCCESS && m_pSearchResults && m_pSearchResults->dwSearchResults )
			{
				// A list of matching sessions was found.
				Msg( "Found %d matching games\n", m_pSearchResults->dwSearchResults );
				SwitchToState( MMSTATE_WAITING_QOS );
			}
			else
			{
				SessionNotification( SESSION_NOTIFY_FAIL_SEARCH );
			}

			g_pXboxSystem->ReleaseAsyncHandle( m_hSearchHandle );
		}
	}
	else
	{
		if ( GetTime() - m_fSendTimer > SYSTEMLINK_RETRYINTERVAL && m_nSendCount < SYSTEMLINK_MAXRETRIES )
		{
			// Send out a search for lan servers
			ALIGN4 char	 msg_buffer[MAX_ROUTABLE_PAYLOAD] ALIGN4_POST;
			bf_write msg( msg_buffer, sizeof(msg_buffer) );

			msg.WriteLong( CONNECTIONLESS_HEADER );
			msg.WriteByte( PTH_SYSTEMLINK_SEARCH );
			msg.WriteLongLong( m_Nonce );		// 64 bit

			// Send message
			netadr_t adr;
			adr.SetType( NA_BROADCAST );
			adr.SetPort( PORT_SYSTEMLINK );

			NET_SendPacket( NULL, NS_SYSTEMLINK, adr, msg.GetData(), msg.GetNumBytesWritten() );

			m_fSendTimer = GetTime();
			++m_nSendCount;
		}
		else if ( m_nSendCount >= SYSTEMLINK_MAXRETRIES )
		{
			if ( m_pSystemLinkResults.Count() )
			{
				SessionNotification( SESSION_NOTIFY_SEARCH_COMPLETED );

				// Send the session info to gameui
				for ( int i = 0; i < m_pSystemLinkResults.Count(); ++i )
				{
					systemLinkInfo_s *pSearchInfo = (systemLinkInfo_s*)m_pSystemLinkResults[i];
					XSESSION_SEARCHRESULT *pResult = &pSearchInfo->Result;
					
					hostData_s hostData;
					hostData.gameState = pSearchInfo->gameState;
					hostData.gameTime = pSearchInfo->gameTime;
					hostData.xuid = pSearchInfo->xuid;
					Q_strncpy( hostData.hostName, pSearchInfo->szHostName, sizeof( hostData.hostName ) );
					Q_strncpy( hostData.scenario, pSearchInfo->szScenario, sizeof( hostData.scenario ) );

					EngineVGui()->SessionSearchResult( i, &hostData, pResult, -1 );
				}
			}
			else
			{
				SessionNotification( SESSION_NOTIFY_FAIL_SEARCH );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check for QOS Results
//-----------------------------------------------------------------------------
void CMatchmaking::UpdateQosLookup()
{
}

//-----------------------------------------------------------------------------
// Purpose: Cancel the current search operation
//-----------------------------------------------------------------------------
void CMatchmaking::CancelSearch()
{
	SwitchToState( MMSTATE_INITIAL );
}

//-----------------------------------------------------------------------------
// Purpose: Cancel the current search operation
//-----------------------------------------------------------------------------
void CMatchmaking::CancelQosLookup()
{
	SwitchToState( MMSTATE_INITIAL );
}

//-----------------------------------------------------------------------------
// Purpose: User has selected a session to join, create a local session with the same properties
//-----------------------------------------------------------------------------
void CMatchmaking::SelectSession( uint idx )
{
	XSESSION_SEARCHRESULT *pResult = NULL;
	if ( m_Session.IsSystemLink() )
	{
		systemLinkInfo_s* pInfo = (systemLinkInfo_s*)m_pSystemLinkResults[idx];
		pResult = &pInfo->Result;
	}
	else
	{
		pResult = &m_pSearchResults->pResults[idx];
	}

	if ( !pResult )
		return;

	m_Session.SetSessionInfo( &pResult->info );

	ApplySessionProperties( pResult->cContexts, pResult->cProperties, pResult->pContexts, pResult->pProperties );

	m_Session.SetIsHost( false );
	m_Session.SetSessionSlots( SLOTS_TOTALPUBLIC, pResult->dwOpenPublicSlots + pResult->dwFilledPublicSlots );
	m_Session.SetSessionSlots( SLOTS_TOTALPRIVATE, pResult->dwOpenPrivateSlots + pResult->dwFilledPrivateSlots );

	if ( !m_Session.CreateSession() )
	{
		SessionNotification( SESSION_NOTIFY_FAIL_CREATE );
		return;
	}

	// Waiting for session creation results
	SwitchToState( MMSTATE_CREATING );
}

//-----------------------------------------------------------------------------
// Purpose: Join a session when invited to.
//-----------------------------------------------------------------------------
void CMatchmaking::JoinInviteSession( XSESSION_INFO *pHostInfo )
{
	if ( !pHostInfo )
	{
		Msg( "[JoinInviteSession] resetting.\n" );
		InviteCancel();
		return;
	}

	// Fetch our current session id
	XNKID nSessionID = m_Session.GetSessionId();

	// Check our invite state
	switch ( m_InviteState )
	{
	case INVITE_NONE:
		// Initial invite call
		Msg( "[JoinInviteSession:INVITE_NONE] Initial call to join invite session.\n" );

		// Don't bother if we're invited to join the same session
		if ( !Q_memcmp( &(pHostInfo->sessionID), &(nSessionID), sizeof(nSessionID) ) )
		{
			Msg( "[JoinInviteSession:INVITE_NONE] Rejecting invite session since it is the current session.\n" );
			return;
		}

		// Leave our current session, if we have one
		KickPlayerFromSession( 0 );

		if ( &m_InviteSessionInfo != pHostInfo )
			memcpy( &m_InviteSessionInfo, pHostInfo, sizeof( XSESSION_INFO ) );

		m_InviteState = INVITE_PENDING;

		// If we are currently in progress of doing something, let it finish
		if ( m_bInitialized )
		{
			if ( MMSTATE_INITIAL != m_CurrentState )
			{
				Msg( "[JoinInviteSession:INVITE_NONE] Yielding, current state = %d.\n", m_CurrentState );
				return;
			}
		}
		else
		{
			// We can be uninitialized due to the commentary mode - perform the disconnect to be sure
			ConVarRef commentary( "commentary" );
			if ( commentary.IsValid() && commentary.GetBool() )
			{
				Msg( "[JoinInviteSession:INVITE_NONE] Stopping commentary mode first.\n" );
				engineClient->ClientCmd( "disconnect" );
				Cbuf_Execute();
				return;
			}
		}
		// otherwise fall through to join

	case INVITE_PENDING:
		// While the invite is pending and user changed an invite, obey the user
		if ( pHostInfo != &m_InviteSessionInfo )
			memcpy( &m_InviteSessionInfo, pHostInfo, sizeof( XSESSION_INFO ) );

		// Wait for the previous matchmaking session to finish and cleanup
		if ( m_bInitialized && ( MMSTATE_INITIAL != m_CurrentState ) )
		{
			Msg( "[JoinInviteSession:INVITE_PENDING] Waiting, current state = %d.\n", m_CurrentState );
			return;
		}

		// Accept the invite right away
		m_InviteState = INVITE_ACCEPTING;
		// fall through

	case INVITE_ACCEPTING:
		// Everything will finish this frame
		Msg( "[JoinInviteSession:INVITE_ACCEPTING] Accepting the invite.\n" );
		break;

			default:
		Msg( "[JoinInviteSession:UnknownState=%d] Aborting.\n", m_InviteState );
		InviteCancel();
		return;
	}

	// Initialize our state to accept the new connection
	NET_SetMutiplayer( true );
	InitializeLocalClient( false );

	// Allow us to access private channels due to invite
	m_Local.m_bInvited = true;

	// Create the session and kick off our UI
	if ( !m_Session.CreateSession() )
	{
		SessionNotification( SESSION_NOTIFY_FAIL_CREATE );
		return;
	}

	// Waiting for session creation results
	SwitchToState( MMSTATE_CREATING );
	InviteCancel();
}

void CMatchmaking::InviteCancel()
{
	m_InviteState = INVITE_NONE;
	memset( &m_InviteWaitingInfo, 0, sizeof( m_InviteWaitingInfo ) );
}

//-----------------------------------------------------------------------------
// Purpose: Search for a session by ID and connect to it (done for cross-game invites)
//-----------------------------------------------------------------------------
void CMatchmaking::JoinInviteSessionByID( XNKID nSessionID )
{
	}

//-----------------------------------------------------------------------------
// Purpose: Tell a session host we'd like to join the session
//-----------------------------------------------------------------------------
void CMatchmaking::SendJoinRequest( netadr_t *adr )
{
	ALIGN4 char	 msg_buffer[MAX_ROUTABLE_PAYLOAD] ALIGN4_POST;
	bf_write msg( msg_buffer, sizeof(msg_buffer) );

	// Send local player info
	msg.WriteLong( CONNECTIONLESS_HEADER );
	msg.WriteByte( PTH_CONNECT );
	msg.WriteLongLong( m_Local.m_id );		// 64 bit
	msg.WriteByte( m_Local.m_cPlayers );
	msg.WriteOneBit( m_Local.m_bInvited );
	msg.WriteBytes( &m_Local.m_xnaddr, sizeof( m_Local.m_xnaddr ) );

	for ( int i = 0; i < m_Local.m_cPlayers; ++i )
	{
		msg.WriteLongLong( m_Local.m_xuids[i] );	// 64 bit
		msg.WriteBytes( &m_Local.m_cVoiceState, sizeof( m_Local.m_cVoiceState ) );	// TODO: has voice
		msg.WriteString( m_Local.m_szGamertags[i] );
	}

	// Send message
	NET_SendPacket( NULL, NS_MATCHMAKING, *adr, msg.GetData(), msg.GetNumBytesWritten() );
}

//-----------------------------------------------------------------------------
// Purpose: Process the session host's response to our join request 
//-----------------------------------------------------------------------------
bool CMatchmaking::ProcessJoinResponse( MM_JoinResponse *pMsg )
{
	switch( pMsg->m_ResponseType )
	{
	case MM_JoinResponse::JOINRESPONSE_NOTHOSTING:
		if ( m_CurrentState != MMSTATE_SESSION_CONNECTING )
		{
			return true;
		}
		Msg( "This game is no longer available.\n" );
		SessionNotification( SESSION_NOTIFY_CONNECT_NOTAVAILABLE );
		break;

	case MM_JoinResponse::JOINRESPONSE_SESSIONFULL:
		if ( m_CurrentState != MMSTATE_SESSION_CONNECTING )
		{
			return true;
		}
		Msg( "This game is full.\n" );
		SessionNotification( SESSION_NOTIFY_CONNECT_SESSIONFULL );
		break;

	case MM_JoinResponse::JOINRESPONSE_APPROVED:
	case MM_JoinResponse::JOINRESPONSE_APPROVED_JOINGAME:
		if ( m_CurrentState != MMSTATE_SESSION_CONNECTING )
		{
			return true;
		}
		// Fill in host data
		m_Host.m_id = pMsg->m_id;					// 64 bit
		m_Session.SetSessionNonce( pMsg->m_Nonce );	// 64 bit
		m_Session.SetSessionFlags( pMsg->m_SessionFlags );
		m_Session.SetOwnerId( pMsg->m_nOwnerId );

		m_nHostOwnerId = pMsg->m_nOwnerId;

		ApplySessionProperties( pMsg->m_ContextCount, pMsg->m_PropertyCount, pMsg->m_SessionContexts.Base(), pMsg->m_SessionProperties.Base() );
		
		for ( int i = 0; i < m_Local.m_cPlayers; ++i )
		{
			m_Local.m_iTeam[i] = pMsg->m_iTeam;
		}

		m_nTotalTeams = pMsg->m_nTotalTeams;

		if ( pMsg->m_ResponseType == pMsg->JOINRESPONSE_APPROVED )
		{
			SessionNotification( SESSION_NOTIFY_CONNECTED_TOSESSION );
			SendPlayerInfoToLobby( &m_Local );
		}
		break;

	case MM_JoinResponse::JOINRESPONSE_MODIFY_SESSION:
		if ( !m_Session.IsHost() )
		{
			if ( m_CurrentState != MMSTATE_SESSION_CONNECTED )
			{
				return true;
			}
			
			// Host has sent us some new session properties
			ApplySessionProperties( pMsg->m_ContextCount, pMsg->m_PropertyCount, pMsg->m_SessionContexts.Base(), pMsg->m_SessionProperties.Base() );

			MM_JoinResponse response;
			response.m_ResponseType = MM_JoinResponse::JOINRESPONSE_MODIFY_SESSION;
			response.m_id = m_Local.m_id;
			SendMessage( &response, &m_Host );

			SessionNotification( SESSION_NOTIFY_MODIFYING_COMPLETED_CLIENT );
		}
		else
		{
			if ( m_CurrentState != MMSTATE_MODIFYING )
			{
				return true;
			}

			// Handle this client response
			bool bWaiting = false;
			for ( int i = 0; i < m_Remote.Count(); ++i )
			{
				if ( m_Remote[i]->m_id == pMsg->m_id )
				{
					m_Remote[i]->m_bModified = true;
				}
				else
				{
					if ( !m_Remote[i]->m_bModified )
					{
						bWaiting = true;
					}
				}
			}

			if ( !bWaiting )
			{
				// Everyone has modified their session
				EndSessionModify();
			}
		}
		break;

	default:
		break;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Apply the contexts and properties that came from the host, and build out keyvalues for GameUI
//-----------------------------------------------------------------------------
void CMatchmaking::ApplySessionProperties( int numContexts, int numProperties, XUSER_CONTEXT *pContexts, XUSER_PROPERTY *pProperties )
{
	// Clear our existing properties, as they should be completely replaced by these new ones
	m_SessionContexts.RemoveAll();
	m_SessionProperties.RemoveAll();

	char szBuffer[MAX_PATH];
	uint nGameTypeId = g_ClientDLL->GetPresenceID( "CONTEXT_GAME_TYPE" );

	// Update the session properties
	for ( int i = 0; i < numContexts; ++i )
	{
		XUSER_CONTEXT &ctx = pContexts[i];
		m_SessionContexts.AddToTail( ctx );

		const char *pID = g_ClientDLL->GetPropertyIdString( ctx.dwContextId );
		g_ClientDLL->GetPropertyDisplayString( ctx.dwContextId, ctx.dwValue, szBuffer, sizeof( szBuffer ) );

		// Set the display string for gameUI
		KeyValues *pContextKey = m_pSessionKeys->FindKey( pID, true );
		pContextKey->SetName( pID );
		pContextKey->SetString( "displaystring", szBuffer );

		// We need to set the game type
		if ( ctx.dwContextId == nGameTypeId )
		{
			m_Session.SetContext( ctx.dwContextId, ctx.dwValue, false );
		}
	}

	for ( int i = 0; i < numProperties; ++i )
	{
		XUSER_PROPERTY &prop = pProperties[i];
		m_SessionProperties.AddToTail( prop );

		const char *pID = g_ClientDLL->GetPropertyIdString( prop.dwPropertyId );
		g_ClientDLL->GetPropertyDisplayString( prop.dwPropertyId, prop.value.nData, szBuffer, sizeof( szBuffer ) );

		// Set the display string for gameUI
		KeyValues *pPropertyKey = m_pSessionKeys->FindKey( pID, true );
		pPropertyKey->SetName( pID );
		pPropertyKey->SetString( "displaystring", szBuffer );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Send a join request to the session host 
//-----------------------------------------------------------------------------
bool CMatchmaking::ConnectToHost()
{
	AddPlayersToSession( &m_Local );

	XSESSION_INFO info;
	m_Session.GetSessionInfo( &info );

	// Initiate the network channel
	AddRemoteChannel( &m_Host.m_adr );

	SendJoinRequest( &m_Host.m_adr );

	m_fWaitTimer = GetTime();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Waiting for a connection response from the session host 
//-----------------------------------------------------------------------------
void CMatchmaking::UpdateConnecting()
{
	if ( GetTime() - m_fWaitTimer > JOINREPLY_WAITTIME )
	{
		SessionNotification( SESSION_NOTIFY_CONNECT_NOTAVAILABLE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Clean up the search results arrays
//-----------------------------------------------------------------------------
void CMatchmaking::ClearSearchResults()
{
	if ( m_pSearchResults )
	{
		free( m_pSearchResults );
		m_pSearchResults = NULL;
	}

	// This will call delete and we should technically be calling delete []
	m_pSystemLinkResults.PurgeAndDeleteElements();
}

CON_COMMAND( mm_select_session, "Select a session" )
{
	if ( args.ArgC() >= 2 )
	{
		g_pMatchmaking->SelectSession( atoi( args[1] ) );
	}
}

