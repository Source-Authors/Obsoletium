//========= Copyright, Port maintainers, All rights reserved. ============//
//
// Purpose:
//   Portability shim mapping the small WinSock surface this module relies on
//   onto POSIX sockets.
//
//   The module is written against WinSock 2 (SOCKET, INVALID_SOCKET,
//   SOCKET_ERROR, closesocket, ioctlsocket, WSAStartup/WSACleanup,
//   S_un.S_addr). BSD sockets provide everything except the spellings, so this
//   header supplies the missing names instead of forking each source file.
//
//===========================================================================//

#ifndef NETWORKSYSTEM_SOCKETAPI_H
#define NETWORKSYSTEM_SOCKETAPI_H

#ifdef _WIN32

#include <winsock.h>

#else // !_WIN32

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// sockaddr_in::sin_addr is spelled sin_addr.s_addr on BSD; S_un is a Microsoft
// extension. Every use of it in this module is a plain assignment of
// INADDR_ANY, so a helper reads better than sprinkling #ifdef through code.
#define CUDPSocketSetAnyAddr( pAddr ) ( (pAddr)->s_addr = INADDR_ANY )

// recvfrom()/sendto() take socklen_t* on POSIX but int* on Windows.
typedef socklen_t SockLen_t;

#define INVALID_SOCKET		( -1 )
#define SOCKET_ERROR		( -1 )
#define closesocket			close

// Windows-only spellings used by the error paths.
// LPHOSTENT is the WinSock spelling of a hostent pointer.
#define LPHOSTENT			hostent *

#define SocketLastError		errno
#define SocketWouldBlock	( EWOULDBLOCK )

// WinSock needs explicit library initialisation; BSD sockets do not, so these
// exist purely to keep the call sites (and the surrounding bookkeeping) intact.
#define MAKEWORD( a, b )		( ( (a) << 8 ) | (b) )

struct WSAData
{
	unsigned short	wVersion;
	unsigned short	wHighVersion;
	char			szDescription[ 257 ];
	char			szSystemStatus[ 129 ];
};

inline int WSAStartup( unsigned short, WSAData * )			{ return 0; }
inline int WSACleanup()									{ return 0; }

#endif // else !_WIN32

#endif // NETWORKSYSTEM_SOCKETAPI_H
