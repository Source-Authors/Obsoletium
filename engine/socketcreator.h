// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SE_ENGINE_SOCKET_CREATOR_H_
#define SE_ENGINE_SOCKET_CREATOR_H_

#include "tier0/vcrmode.h"
#include "tier1/utlvector.h"
#include "tier1/utlbuffer.h"
#include "tier1/utllinkedlist.h"
#include "tier1/netadr.h"
#include "igameserverdata.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using SocketHandle_t = socket_handle;


struct ISocketCreatorListener
{
public:
	// Methods to allow other classes to allocate data associated w/ sockets
	// Return false to disallow socket acceptance
	virtual bool ShouldAcceptSocket( SocketHandle_t hSocket, const netadr_t &netAdr ) = 0; 
	virtual void OnSocketAccepted( SocketHandle_t hSocket, const netadr_t &netAdr, void** ppData ) = 0; 
	virtual void OnSocketClosed( SocketHandle_t hSocket, const netadr_t &netAdr, void* pData ) = 0;
};


//-----------------------------------------------------------------------------
// container class to handle network streams
//-----------------------------------------------------------------------------
class CSocketCreator 
{
public:
	CSocketCreator( ISocketCreatorListener *pListener = NULL );
	~CSocketCreator();

	// Call this once per frame
	void RunFrame();

	// This method is used to put the socket in a mode where it's listening
	// for connections and a connection is made once the request is received
	bool CreateListenSocket( const netadr_t &netAdr );
	void CloseListenSocket();
	bool IsListening() const;

	// This method is used to connect to/disconnect from an external listening socket creator
	// Returns accepted socket index, or -1 if it failed.
	// Use GetAcceptedSocket* methods to access this socket's data
	// if bSingleSocket == true, all accepted sockets are closed before the new one is opened
	// NOTE: Closing an accepted socket will re-index all the sockets with higher indices
	intp ConnectSocket( const netadr_t &netAdr, bool bSingleSocket );
	void CloseAcceptedSocket( intp nIndex );
	void CloseAllAcceptedSockets();
	intp GetAcceptedSocketCount() const;
	SocketHandle_t GetAcceptedSocketHandle( intp nIndex ) const;
	const netadr_t& GetAcceptedSocketAddress( intp nIndex ) const;
	void* GetAcceptedSocketData( intp nIndex );

	// Closes all open sockets (listen + accepted)
	void Disconnect();

private:
	enum
	{
		SOCKET_TCP_MAX_ACCEPTS = 2
	};

	void ProcessAccept();
	bool ConfigureSocket( socket_handle sock );

public:
	struct AcceptedSocket_t
	{
		SocketHandle_t	m_hSocket;
		netadr_t		m_Address;
		void			*m_pData;

		bool operator==( const AcceptedSocket_t &rhs ) const
		{
			return ( m_Address.CompareAdr( rhs.m_Address ) == 0 );
		}
	};

	ISocketCreatorListener *m_pListener;
	CUtlVector< AcceptedSocket_t > m_hAcceptedSockets;
	SocketHandle_t	m_hListenSocket;	// Used to accept connections
	netadr_t		m_ListenAddress;	// Address used to listen on
};


//-----------------------------------------------------------------------------
// Returns true if the socket would block because of the last socket command
//-----------------------------------------------------------------------------
bool SocketWouldBlock();

#endif // SOCKET_CREATOR_H
