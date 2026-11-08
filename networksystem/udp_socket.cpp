//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "udp_socket.h"
#include "tier1/utlbuffer.h"
#include "tier1/strtools.h"

#include "socketapi.h"

class CUDPSocket::CImpl	
{
public:
	struct sockaddr_in	m_SocketIP;
};

CUDPSocket::CUDPSocket( ) :
	m_socketIP(),
	m_Socket( INVALID_SOCKET ),
	m_Port( 0 ),
	m_pImpl( new CImpl )
{
	Q_memset( &m_pImpl->m_SocketIP, 0, sizeof( m_pImpl->m_SocketIP ) );
	Q_memset( &m_socketIP, 0, sizeof( m_socketIP ) );
}

CUDPSocket::~CUDPSocket()
{
	delete m_pImpl;
}

bool CUDPSocket::Init( unsigned short nBindToPort )
{
	m_Port = nBindToPort;
	struct sockaddr_in	address;

	m_Socket = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if ( m_Socket == INVALID_SOCKET )
		return false;

	address = m_pImpl->m_SocketIP;
	address.sin_family = AF_INET;
	address.sin_port = htons( m_Port );
#ifdef _WIN32
	address.sin_addr.S_un.S_addr = INADDR_ANY;
#else
	CUDPSocketSetAnyAddr( &address.sin_addr );
#endif

	if ( SOCKET_ERROR == bind( m_Socket, ( struct sockaddr * )&address, sizeof( address ) ) )
	{
		return false;
	}

	// Set to non-blocking i/o
#ifdef _WIN32
	unsigned long value = 1;
	if ( SOCKET_ERROR == ioctlsocket( m_Socket, FIONBIO, &value ) )
	{
		return false;
	}
#else
	int flags = fcntl( m_Socket, F_GETFL, 0 );
	if ( flags == -1 || fcntl( m_Socket, F_SETFL, flags | O_NONBLOCK ) == -1 )
	{
		return false;
	}
#endif

	return true;
}

void CUDPSocket::Shutdown()
{
	if ( m_Socket != INVALID_SOCKET )
	{
		closesocket( m_Socket ); 
	}
}

bool CUDPSocket::RecvFrom( netadr_t& packet_from, CUtlBuffer& data )
{
	sockaddr from;
	SockLen_t nFromLen = sizeof( from );

	int nMaxBytesToRead = data.Size() - data.TellPut();
	char *pBuffer = (char*)data.PeekPut();

	int nBytesRead = recvfrom( m_Socket, pBuffer, nMaxBytesToRead, 0, &from, &nFromLen );
	if ( nBytesRead == SOCKET_ERROR )
	{
		int nError = SocketLastError;
		if ( nError != SocketWouldBlock )
		{
			Warning( "Socket error '%i'\n", nError );
		}
		return false;
	}

	packet_from.SetFromSockadr( &from );
	data.SeekPut( CUtlBuffer::SEEK_CURRENT, nBytesRead );
	return true;
}

bool CUDPSocket::SendTo( const netadr_t &recipient, const CUtlBuffer& data  )
{
	return SendTo( recipient, (const byte *)data.Base(), (size_t)data.TellPut() );
}

bool CUDPSocket::SendTo( const netadr_t &recipient, const byte *data, size_t datalength )
{
	sockaddr dest;
	recipient.ToSockadr( &dest );

	// Send data
	int bytesSent = sendto
		(
		m_Socket, 
		(const char *)data, 
		(int)datalength, 
		0, 
		reinterpret_cast< const sockaddr * >( &dest ), 
		(SockLen_t)sizeof( dest )
		);

	if ( SOCKET_ERROR == bytesSent )
	{
		return false;
	}

	return true;
}
