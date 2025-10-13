// Copyright Valve Corporation, All rights reserved.
//
// Purpose: Utility class to help in socket creation. Works for clients + servers

#include "socketcreator.h"
#include "tier0/dbg.h"
#include "server.h"

#if defined(_WIN32)
#include <winsock.h>

#undef SetPort // winsock screws with the SetPort string... *sigh*
#define socklen_t int
#define MSG_NOSIGNAL 0
#elif POSIX
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <sys/ioctl.h>
#define closesocket close
#define WSAGetLastError() errno
#define ioctlsocket ioctl

#ifdef OSX
#define MSG_NOSIGNAL 0
#endif
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool SocketWouldBlock()
{
#ifdef _WIN32
	return WSAGetLastError() == WSAEWOULDBLOCK;
#elif POSIX
	return (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSocketCreator::CSocketCreator( ISocketCreatorListener *pListener )
{
	m_hListenSocket = kInvalidSocketHandle;
	m_pListener = pListener;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSocketCreator::~CSocketCreator()
{
	Disconnect();
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the listening socket is created and listening
//-----------------------------------------------------------------------------
bool CSocketCreator::IsListening() const
{
	return m_hListenSocket != kInvalidSocketHandle;
}

//-----------------------------------------------------------------------------
// Purpose: Bind to a TCP port and accept incoming connections
//-----------------------------------------------------------------------------
bool CSocketCreator::CreateListenSocket( const netadr_t &netAdr )
{
	CloseListenSocket();

	m_ListenAddress = netAdr;
	m_hListenSocket = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if ( m_hListenSocket == kInvalidSocketHandle )
	{
		Warning( "Create socket(IPPROTO_TCP) failed: %s.\n", NET_ErrorString( WSAGetLastError() ) );
		return false;
	}

	if ( !ConfigureSocket( m_hListenSocket ) )
	{
		CloseListenSocket();
		return false;
	}

	sockaddr_in s;
	m_ListenAddress.ToSockadr( (sockaddr *)&s );
	int ret = bind( m_hListenSocket, (sockaddr *)&s, sizeof(s) );
	if ( ret == -1 )
	{
		Warning( "Socket bind failed: %s.\n", NET_ErrorString( WSAGetLastError() ) );
		CloseListenSocket();
		return false;
	}

	ret = listen( m_hListenSocket, SOCKET_TCP_MAX_ACCEPTS );
	if ( ret == -1 )
	{
		Warning( "Socket listen failed: %s.\n", NET_ErrorString( WSAGetLastError() ) );
		CloseListenSocket();
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Configures a socket for use
//-----------------------------------------------------------------------------
bool CSocketCreator::ConfigureSocket( socket_handle sock )
{
	// disable NAGLE (rcon cmds are small in size)
	int nodelay = 1;
	int ret = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));
	if ( ret == -1 )
	{
		Warning( "Socket setsockopt(TCP_NODELAY): %s.\n", NET_ErrorString( WSAGetLastError() ) );
		// continue.
	}

	nodelay = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&nodelay, sizeof(nodelay));
	if ( ret == -1 )
	{
		Warning( "Socket setsockopt(SO_REUSEADDR): %s.\n", NET_ErrorString( WSAGetLastError() ) );
		// continue.
	}

	unsigned long opt = 1;
	ret = ioctlsocket( sock, FIONBIO, &opt ); // non-blocking
	if ( ret == -1 )
	{
		Warning( "Socket ioctlsocket(FIONBIO) failed: %s.\n", NET_ErrorString( WSAGetLastError() ) );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handle a new connection
//-----------------------------------------------------------------------------
void CSocketCreator::ProcessAccept()
{
	sockaddr sa;
	int nLengthAddr = sizeof(sa);

	socket_handle newSocket = accept( m_hListenSocket, &sa, (socklen_t *)&nLengthAddr );
	if ( newSocket == kInvalidSocketHandle )
	{
		if ( !SocketWouldBlock()
#ifdef POSIX
			&& errno != EINTR 
#endif
		 )
		{
			Warning ( "Socket accept failed: %s.\n", NET_ErrorString( WSAGetLastError() ) );
		}
		return;
	}

	if ( !ConfigureSocket( newSocket ) )
	{
		closesocket( newSocket );
		return; 
	}

	netadr_t adr;
	if ( m_pListener &&
		 ( !adr.SetFromSockadr( &sa ) || !m_pListener->ShouldAcceptSocket( newSocket, adr ) ) )
	{
		closesocket( newSocket );
		return;
	}

	// new connection TCP request, put in accepted queue
	intp nIndex = m_hAcceptedSockets.AddToTail();
	AcceptedSocket_t *pNewEntry = &m_hAcceptedSockets[nIndex];
	pNewEntry->m_hSocket = newSocket;
	pNewEntry->m_Address = adr;
	pNewEntry->m_pData = nullptr;

	void* pData = nullptr;
	if ( m_pListener )
	{
		m_pListener->OnSocketAccepted( newSocket, adr, &pData );
	}
	pNewEntry->m_pData = pData;
}


//-----------------------------------------------------------------------------
// Purpose: connect to the remote server
//-----------------------------------------------------------------------------
intp CSocketCreator::ConnectSocket( const netadr_t &netAdr, bool bSingleSocket )
{
	if ( bSingleSocket )
	{
		CloseAllAcceptedSockets();
	}

	SocketHandle_t hSocket = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );
	if ( hSocket == kInvalidSocketHandle )
	{
		Warning( "Create socket(IPPROTO_TCP) failed: %s.\n", NET_ErrorString( WSAGetLastError() ) );
		return -1;
	}

	int opt = 1;
	int ret = ioctlsocket( hSocket, FIONBIO, (unsigned long*)&opt ); // non-blocking
	if ( ret == -1 )
	{
		Warning( "Socket ioctl(FIONBIO) failed: %s.\n", NET_ErrorString( WSAGetLastError() ) );
		closesocket( hSocket );
		return -1;
	}

	// disable NAGLE (rcon cmds are small in size)
	int nodelay = 1;
	ret = setsockopt( hSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay) );
	if ( ret == -1 )
	{
		Warning( "Socket setsockopt(TCP_NODELAY): %s.\n", NET_ErrorString( WSAGetLastError() ) );
		// continue.
	}

	sockaddr_in s;
	netAdr.ToSockadr( (sockaddr *)&s );

	ret = connect( hSocket, (sockaddr *)&s, sizeof(s));
	if ( ret == -1 )
	{
		if ( !SocketWouldBlock() )
		{	
			Warning( "Socket connect failed: %s.\n", NET_ErrorString( WSAGetLastError() ) );
			closesocket( hSocket );
			return -1;
		}

		fd_set writefds;
		struct timeval tv;
		tv.tv_usec = 0;
		tv.tv_sec = 1;
		FD_ZERO( &writefds );
		FD_SET( static_cast<uintp>( hSocket ), &writefds );
		if ( select ( hSocket + 1, NULL, &writefds, NULL, &tv ) < 1 ) // block for at most 1 second
		{
			closesocket( hSocket );		// took too long to connect to, give up
			return -1;
		}
	}

	if ( m_pListener && !m_pListener->ShouldAcceptSocket( hSocket, netAdr ) )
	{
		closesocket( hSocket );
		return -1;
	}

	// new connection TCP request, put in accepted queue
	void *pData = NULL;
	intp nIndex = m_hAcceptedSockets.AddToTail();
	AcceptedSocket_t *pNewEntry = &m_hAcceptedSockets[nIndex];
	pNewEntry->m_hSocket = hSocket;
	pNewEntry->m_Address = netAdr;
	pNewEntry->m_pData = NULL;

	if ( m_pListener )
	{
		m_pListener->OnSocketAccepted( hSocket, netAdr, &pData );
	}

	pNewEntry->m_pData = pData;
	return nIndex;
}


//-----------------------------------------------------------------------------
// Purpose: close an open rcon connection
//-----------------------------------------------------------------------------
void CSocketCreator::CloseListenSocket()
{
	if ( m_hListenSocket != kInvalidSocketHandle )
	{
		closesocket( m_hListenSocket );
		m_hListenSocket = kInvalidSocketHandle;
	}
}

void CSocketCreator::CloseAcceptedSocket( intp nIndex )
{
	if ( nIndex >= m_hAcceptedSockets.Count() )
		return;

	AcceptedSocket_t& connected = m_hAcceptedSockets[nIndex];
	if ( m_pListener )
	{
		m_pListener->OnSocketClosed( connected.m_hSocket, connected.m_Address, connected.m_pData );
	}
	closesocket( connected.m_hSocket );
	m_hAcceptedSockets.Remove( nIndex );
}

void CSocketCreator::CloseAllAcceptedSockets()
{
	for ( auto &connected : m_hAcceptedSockets )
	{
		if ( m_pListener )
		{
			m_pListener->OnSocketClosed( connected.m_hSocket, connected.m_Address, connected.m_pData );
		}
		closesocket( connected.m_hSocket );
	}
	m_hAcceptedSockets.RemoveAll();
}


void CSocketCreator::Disconnect()
{
	CloseListenSocket();
	CloseAllAcceptedSockets();
}


//-----------------------------------------------------------------------------
// Purpose: accept new connections and walk open sockets and handle any incoming data
//-----------------------------------------------------------------------------
void CSocketCreator::RunFrame()
{
	if ( IsListening() )
	{
		ProcessAccept(); // handle any new connection requests
	}
}


//-----------------------------------------------------------------------------
// Returns socket info
//-----------------------------------------------------------------------------
intp CSocketCreator::GetAcceptedSocketCount() const
{
	return m_hAcceptedSockets.Count();
}

SocketHandle_t CSocketCreator::GetAcceptedSocketHandle( intp nIndex ) const
{
	return m_hAcceptedSockets[nIndex].m_hSocket;
}

const netadr_t& CSocketCreator::GetAcceptedSocketAddress( intp nIndex ) const
{
	return m_hAcceptedSockets[nIndex].m_Address;
}

void* CSocketCreator::GetAcceptedSocketData( intp nIndex )
{
	return m_hAcceptedSockets[nIndex].m_pData;
}
