// Copyright Valve Corporation, All rights reserved.
//
// IP Support layer.

#include <system_error>

#include "tier0/etwprof.h"
#include "tier0/vprof.h"
#include "tier1/fmtstr.h"

#include "net_ws_headers.h"
#include "net_ws_queued_packet_sender.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define NET_COMPRESSION_STACKBUF_SIZE 4096 

static ConVar net_showudp_wire( "net_showudp_wire", "0", 0, "Show incoming packet information" );

#define UDP_SO_RCVBUF_SIZE 131072

static ConVar net_udp_rcvbuf( "net_udp_rcvbuf", NETSTRING( UDP_SO_RCVBUF_SIZE ), FCVAR_ALLOWED_IN_COMPETITIVE, "Default UDP receive buffer size", true, 8192, true, 128 * 1024 );

static ConVar net_showsplits( "net_showsplits", "0", 0, "Show info about packet splits" );

static ConVar net_splitrate( "net_splitrate", "1", 0, "Number of fragments for a splitpacket that can be sent per frame" );

static ConVar ipname        ( "ip", "localhost", FCVAR_ALLOWED_IN_COMPETITIVE, "Overrides IP for multihomed hosts" );
static ConVar hostport      ( "hostport", NETSTRING( PORT_SERVER ) , FCVAR_ALLOWED_IN_COMPETITIVE, "Host game server port" );
static ConVar hostip		( "hostip", "", FCVAR_ALLOWED_IN_COMPETITIVE, "Host game server ip" );

static ConVar clientport    ( "clientport", NETSTRING( PORT_CLIENT ), FCVAR_ALLOWED_IN_COMPETITIVE, "Host game client port" );
static ConVar hltvport		( "tv_port", NETSTRING( PORT_HLTV ), FCVAR_ALLOWED_IN_COMPETITIVE, "Host SourceTV port" );
static ConVar matchmakingport( "matchmakingport", NETSTRING( PORT_MATCHMAKING ), FCVAR_ALLOWED_IN_COMPETITIVE, "Host Matchmaking port" );
static ConVar systemlinkport( "systemlinkport", NETSTRING( PORT_SYSTEMLINK ), FCVAR_ALLOWED_IN_COMPETITIVE, "System Link port" );

static ConVar fakelag		( "net_fakelag", "0", FCVAR_CHEAT, "Lag all incoming network data (including loopback) by this many milliseconds." );
static ConVar fakeloss		( "net_fakeloss", "0", FCVAR_CHEAT, "Simulate packet loss as a percentage (negative means drop 1/n packets)" ); 
static ConVar droppackets	( "net_droppackets", "0", FCVAR_CHEAT, "Drops next n packets on client" ); 
static ConVar fakejitter	( "net_fakejitter", "0", FCVAR_CHEAT, "Jitter fakelag packet time" );

static ConVar net_compressvoice( "net_compressvoice", "0", 0, "Attempt to compress out of band voice payloads (360 only)." );
ConVar net_usesocketsforloopback( "net_usesocketsforloopback", "0", 0, "Use network sockets layer even for listen server local player's packets (multiplayer only)." );

#ifdef _DEBUG
static ConVar fakenoise		( "net_fakenoise", "0", FCVAR_CHEAT, "Simulate corrupt network packets (changes n bits per packet randomly)" ); 
static ConVar fakeshuffle	( "net_fakeshuffle", "0", FCVAR_CHEAT, "Shuffles order of every nth packet (needs net_fakelag)" ); 
static ConVar recvpackets	( "net_recvpackets", "-1", FCVAR_CHEAT, "Receive exactly next n packets if >= 0" ); 
static ConVar	net_savelargesplits( "net_savelargesplits", "-1", 0, "If not -1, then if a split has this many or more split parts, save the entire packet to disc for analysis." );
#endif

extern ConVar net_showudp;
extern ConVar net_showtcp;
extern ConVar net_blocksize;
extern ConVar host_timescale;
extern int host_framecount;

void NET_ClearQueuedPacketsForChannel( INetChannel *chan );

#define DEF_LOOPBACK_SIZE 2048

struct netsocket_t
{
	int			nPort;		// UDP/TCP use same port number
	bool		bListening;	// true if TCP port is listening
	socket_handle	hUDP;		// handle to UDP socket from socket()
	socket_handle	hTCP;		// handle to TCP socket from socket()
};

struct pendingsocket_t
{
	socket_handle	newsock;	// handle of new socket
	intp			netsock;	// index of listen socket
	// dimhotepus: float -> double.
	double			time;
	netadr_t		addr;
};


#include "tier0/memdbgoff.h"

struct loopback_t
{
	char		*data;		// loopback buffer
	int			datalen;	// current data length
	char		defbuffer[ DEF_LOOPBACK_SIZE ];

	DECLARE_FIXEDSIZE_ALLOCATOR( loopback_t );
};

#include "tier0/memdbgon.h"

DEFINE_FIXEDSIZE_ALLOCATOR( loopback_t, 2, CUtlMemoryPool::GROW_SLOW );

// Split long packets.  Anything over 1460 is failing on some routers
struct LONGPACKET
{
	int		currentSequence;
	int		splitCount;
	int		totalSize;
	int		nExpectedSplitSize;
	char	buffer[ NET_MAX_MESSAGE ];	// This has to be big enough to hold the largest message
};

// Use this to pick apart the network stream, must be packed
#pragma pack(1)
struct SPLITPACKET
{
	int		netID;
	int		sequenceNumber;
	int		packetID : 16;
	int		nSplitSize : 16;
};
#pragma pack()

#define MIN_USER_MAXROUTABLE_SIZE	576  // ( X.25 Networks )
#define MAX_USER_MAXROUTABLE_SIZE	MAX_ROUTABLE_PAYLOAD


#define MAX_SPLIT_SIZE	(MAX_USER_MAXROUTABLE_SIZE - static_cast<int>(sizeof( SPLITPACKET )))
#define MIN_SPLIT_SIZE	(MIN_USER_MAXROUTABLE_SIZE - static_cast<int>(sizeof( SPLITPACKET )))

// For metering out splitpackets, don't do them too fast as remote UDP socket will drop some payloads causing them to always fail to be reconstituted
// This problem is largely solved by increasing the buffer sizes for UDP sockets on Windows
#define SPLITPACKET_MAX_DATA_BYTES_PER_SECOND V_STRINGIFY(DEFAULT_RATE)

static ConVar sv_maxroutable
	( 
	"sv_maxroutable", 
	"1260", 
	0, 
	"Server upper bound on net_maxroutable that a client can use.", 
	true, MIN_USER_MAXROUTABLE_SIZE, 
	true, MAX_USER_MAXROUTABLE_SIZE 
	);

ConVar net_maxroutable
	( 
	"net_maxroutable", 
	"1260", 
	FCVAR_ARCHIVE | FCVAR_USERINFO, 
	"Requested max packet size before packets are 'split'.", 
	true, MIN_USER_MAXROUTABLE_SIZE, 
	true, MAX_USER_MAXROUTABLE_SIZE 
	);

netadr_t	net_local_adr;
double		net_time = 0.0f;	// current time, updated each frame

static	CUtlVector<netsocket_t> net_sockets;	// the 4 sockets, Server, Client, HLTV, Matchmaking
static	CUtlVector<netpacket_t>	net_packets;

static	bool net_multiplayer = false;	// if true, configured for Multiplayer
static	bool net_noip = false;	// Disable IP support, can't switch to MP mode
static	bool net_nodns = false;	// Disable DNS request to avoid long timeouts
static  bool net_notcp = true;	// Disable TCP support
static	bool net_nohltv = false; // disable HLTV support
static	bool net_dedicated = false;	// true is dedicated system


static CUtlVectorMT< CUtlVector< CNetChan* > >			s_NetChannels;
static CUtlVectorMT< CUtlVector< pendingsocket_t > >	s_PendingSockets;

CTSQueue<loopback_t *> s_LoopBacks[LOOPBACK_SOCKETS];
static netpacket_t*	s_pLagData[MAX_SOCKETS];  // List of lag structures, if fakelag is set.

unsigned short NET_HostToNetShort( unsigned short us_in )
{
	return htons( us_in );
}

unsigned short NET_NetToHostShort( unsigned short us_in )
{
	return ntohs( us_in );
}

// This macro is used to capture the return value of a function call while recording
// a VCR file. During playback, it will get the return value out of the VCR file
// instead of actually calling the function.
#if !defined( NO_VCR )
#define VCR_NONPLAYBACKFN( call, resultVar, eventName ) \
	{ \
		if ( VCRGetMode() != VCR_Playback ) \
			resultVar = call; \
		\
		VCRGenericValue( eventName, &resultVar, sizeof( resultVar ) ); \
	}
#else
#define VCR_NONPLAYBACKFN( call, resultVar, eventName ) \
	{ \
		if ( VCRGetMode() != VCR_Playback ) \
			resultVar = call; \
		\
	}
#endif

/*
====================
NET_ErrorString
====================
*/
const char *NET_ErrorString (int code)
{
#if defined( _WIN32 )
	const std::string message = std::system_category().message(code);
	switch (code)
	{
	case WSAEINTR: return va("WSAEINTR: %s", message.c_str());
	case WSAEBADF: return va("WSAEBADF: %s", message.c_str());
	case WSAEACCES: return va("WSAEACCES: %s", message.c_str());
	case WSAEDISCON: return va("WSAEDISCON: %s", message.c_str());
	case WSAEFAULT: return va("WSAEFAULT: %s", message.c_str());
	case WSAEINVAL: return va("WSAEINVAL: %s", message.c_str());
	case WSAEMFILE: return va("WSAEMFILE: %s", message.c_str());
	case WSAEWOULDBLOCK: return va("WSAEWOULDBLOCK: %s", message.c_str());
	case WSAEINPROGRESS: return va("WSAEINPROGRESS: %s", message.c_str());
	case WSAEALREADY: return va("WSAEALREADY: %s", message.c_str());
	case WSAENOTSOCK: return va("WSAENOTSOCK: %s", message.c_str());
	case WSAEDESTADDRREQ: return va("WSAEDESTADDRREQ: %s", message.c_str());
	case WSAEMSGSIZE: return va("WSAEMSGSIZE: %s", message.c_str());
	case WSAEPROTOTYPE: return va("WSAEPROTOTYPE: %s", message.c_str());
	case WSAENOPROTOOPT: return va("WSAENOPROTOOPT: %s", message.c_str());
	case WSAEPROTONOSUPPORT: return va("WSAEPROTONOSUPPORT: %s", message.c_str());
	case WSAESOCKTNOSUPPORT: return va("WSAESOCKTNOSUPPORT: %s", message.c_str());
	case WSAEOPNOTSUPP: return va("WSAEOPNOTSUPP: %s", message.c_str());
	case WSAEPFNOSUPPORT: return va("WSAEPFNOSUPPORT: %s", message.c_str());
	case WSAEAFNOSUPPORT: return va("WSAEAFNOSUPPORT: %s", message.c_str());
	case WSAEADDRINUSE: return va("WSAEADDRINUSE: %s", message.c_str());
	case WSAEADDRNOTAVAIL: return va("WSAEADDRNOTAVAIL: %s", message.c_str());
	case WSAENETDOWN: return va("WSAENETDOWN: %s", message.c_str());
	case WSAENETUNREACH: return va("WSAENETUNREACH: %s", message.c_str());
	case WSAENETRESET: return va("WSAENETRESET: %s", message.c_str());
	case WSAECONNABORTED: return va("WSWSAECONNABORTEDAEINTR: %s", message.c_str());
	case WSAECONNRESET: return va("WSAECONNRESET: %s", message.c_str());
	case WSAENOBUFS: return va("WSAENOBUFS: %s", message.c_str());
	case WSAEISCONN: return va("WSAEISCONN: %s", message.c_str());
	case WSAENOTCONN: return va("WSAENOTCONN: %s", message.c_str());
	case WSAESHUTDOWN: return va("WSAESHUTDOWN: %s", message.c_str());
	case WSAETOOMANYREFS: return va("WSAETOOMANYREFS: %s", message.c_str());
	case WSAETIMEDOUT: return va("WSAETIMEDOUT: %s", message.c_str());
	case WSAECONNREFUSED: return va("WSAECONNREFUSED: %s", message.c_str());
	case WSAELOOP: return va("WSAELOOP: %s", message.c_str());
	case WSAENAMETOOLONG: return va("WSAENAMETOOLONG: %s", message.c_str());
	case WSAEHOSTDOWN: return va("WSAEHOSTDOWN: %s", message.c_str());
	case WSASYSNOTREADY: return va("WSASYSNOTREADY: %s", message.c_str());
	case WSAVERNOTSUPPORTED: return va("WSAVERNOTSUPPORTED: %s", message.c_str());
	case WSANOTINITIALISED: return va("WSANOTINITIALISED: %s", message.c_str());
	case WSAHOST_NOT_FOUND: return va("WSAHOST_NOT_FOUND: %s", message.c_str());
	case WSATRY_AGAIN: return va("WSATRY_AGAIN: %s", message.c_str());
	case WSANO_RECOVERY: return va("WSANO_RECOVERY: %s", message.c_str());
	case WSANO_DATA: return va("WSANO_DATA: %s", message.c_str());
	default: return va("UNKNOWN ERROR: %s", message.c_str());
	}
#else
	return strerror( code );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *s - 
//			*sadr - 
// Output : bool	NET_StringToSockaddr
//-----------------------------------------------------------------------------
bool NET_StringToSockaddr( const char *s, struct sockaddr *sadr )
{
	char	*colon;
	char	copy[128];
	
	Q_memset (sadr, 0, sizeof(*sadr));
	((struct sockaddr_in *)sadr)->sin_family = AF_INET;
	((struct sockaddr_in *)sadr)->sin_port = 0;

	V_strcpy_safe( copy, s );
	// strip off a trailing :port if present
	for (colon = copy ; *colon ; colon++)
	{
		if (*colon == ':')
		{
			*colon = 0;
			((struct sockaddr_in *)sadr)->sin_port = NET_HostToNetShort((short)atoi(colon+1));
		}
	}
	
	if (copy[0] >= '0' && copy[0] <= '9' && Q_strstr( copy, "." ) )
	{
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(copy);
	}
	else
	{
		if ( net_nodns )
			return false;	// DNS names disabled

		struct hostent	*h;
		if ( (h = gethostbyname(copy)) == NULL )
			return false;
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
	}
	
	return true;
}


static int NET_GetLastError()
{
#if defined( _WIN32 )
	int net_error = WSAGetLastError();
#else
	int net_error = errno;
#endif
#if !defined( NO_VCR )
	VCRGenericValue( "WSAGetLastError", &net_error, sizeof( net_error ) );
#endif
	return net_error;
}

/*
==================
NET_ClearLaggedList

==================
*/
void NET_ClearLaggedList(netpacket_t **pList)
{
	netpacket_t * p = (*pList);

	while ( p )
	{
		netpacket_t * n = p->pNext;

		if ( p->data )
		{
			delete[] p->data;
			p->data = NULL;
		}
		delete p;
		p = n;
	}

	(*pList) = NULL;
}

void NET_ClearLagData( intp sock )
{
	if ( sock < MAX_SOCKETS && s_pLagData[sock] )
	{
		NET_ClearLaggedList( &s_pLagData[sock] );
	}
}

/*
=============
NET_StringToAdr

localhost
idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
=============
*/
bool NET_StringToAdr ( const char *s, netadr_t *a)
{
	struct sockaddr saddr;

	char address[128];
	V_strcpy_safe( address, s );

	if ( !Q_strncmp( address, "localhost", 10 ) || !Q_strncmp( address, "localhost:", 10 ) )
	{
		// subsitute 'localhost' with '127.0.0.1", both have 9 chars
		// this way we can resolve 'localhost' without DNS and still keep the port
		Q_memcpy( address, "127.0.0.1", 9 );
	}

	
	if ( !NET_StringToSockaddr (address, &saddr) )
		return false;

	
	// dimhotepus: Should check address assigned.
	return a->SetFromSockadr( &saddr );
}

CNetChan *NET_FindNetChannel(intp socket, netadr_t &adr)
{
	AUTO_LOCK( s_NetChannels );

	for ( auto *chan : s_NetChannels )
	{
		// sockets must match
		if ( socket != chan->GetSocket() )
			continue;

		// and the IP:Port address 
		if ( adr.CompareAdr( chan->GetRemoteAddress() )  )
		{
			return chan;	// found it
		}
	}

	return NULL;	// no channel found
}

void NET_CloseSocket( socket_handle hSocket, intp sock = -1)
{
	if ( !hSocket )
		return;

	// close socket handle
	int ret;
	VCR_NONPLAYBACKFN( closesocket( hSocket ), ret, "closesocket" );
	if ( ret == -1 )
	{
		ConWarning ("NET_CloseSocket: %s.\n", NET_ErrorString(NET_GetLastError()));
	}

	// if hSocket mapped to hTCP, clear hTCP
	if ( sock >= 0 )
	{
		if ( net_sockets[sock].hTCP == hSocket )
		{
			net_sockets[sock].hTCP = 0;
			net_sockets[sock].bListening = false;
		}
	}
}

/*
====================
NET_IPSocket
====================
*/
socket_handle NET_OpenSocket ( const char *net_interface, int& port, int protocol )
{
	struct sockaddr_in	address;
	unsigned int		opt;
	socket_handle		newsocket = kInvalidSocketHandle;

	if ( protocol == IPPROTO_TCP )
	{
		VCR_NONPLAYBACKFN( socket (PF_INET, SOCK_STREAM, IPPROTO_TCP), newsocket, "socket(IPPROTO_TCP)" );
	}
	else // as UDP or VDP
	{
		VCR_NONPLAYBACKFN( socket (PF_INET, SOCK_DGRAM, protocol), newsocket, "socket()" );
	}

	if ( newsocket == -1 )
	{
		const int net_error = NET_GetLastError();
		if ( net_error != WSAEAFNOSUPPORT )
			Warning ("NET_OpenSocket: socket(protocol %d) failed: %s.\n", protocol, NET_ErrorString(net_error));

		return 0;
	}
	
	opt =  1; // make it non-blocking
	int ret;
	VCR_NONPLAYBACKFN( ioctlsocket (newsocket, FIONBIO, (unsigned long*)&opt), ret, "ioctlsocket(FIONBIO)" );
	if ( ret == -1 )
	{
		Warning ("NET_OpenSocket: ioctlsocket FIONBIO: %s\n", NET_ErrorString(NET_GetLastError()) );
	}
	
	if ( protocol == IPPROTO_TCP )
	{
		opt = 1; // set TCP options: keep TCP connection alive
		VCR_NONPLAYBACKFN( setsockopt(newsocket, SOL_SOCKET, SO_KEEPALIVE, (char *)&opt, sizeof(opt)), ret, "setsockopt(SO_KEEPALIVE)" );
		if (ret == -1)
		{
			Warning ("NET_OpenSocket: setsockopt SO_KEEPALIVE: %s\n", NET_ErrorString(NET_GetLastError()));
			return 0;
		}

		linger optlinger;	// set TCP options: Does not block close waiting for unsent data to be sent
		optlinger.l_linger = 0;
		optlinger.l_onoff = 0;
		VCR_NONPLAYBACKFN( setsockopt(newsocket, SOL_SOCKET, SO_LINGER, (char *)&optlinger, sizeof(optlinger)), ret, "setsockopt(SO_LINGER)" );
		if (ret == -1)
		{
			Warning ("NET_OpenSocket: setsockopt SO_LINGER: %s\n", NET_ErrorString(NET_GetLastError()));
			return 0;
		}

		opt = 1; // set TCP options: Disables the Nagle algorithm for send coalescing.
		VCR_NONPLAYBACKFN( setsockopt(newsocket, IPPROTO_TCP, TCP_NODELAY, (char *)&opt, sizeof(opt)), ret, "setsockopt(TCP_NODELAY)" );
		if (ret == -1)
		{
			Warning ("NET_OpenSocket: setsockopt TCP_NODELAY: %s\n", NET_ErrorString(NET_GetLastError()));
			return 0;
		}

		opt = NET_MAX_MESSAGE; // set TCP options: set send buffer size
		VCR_NONPLAYBACKFN( setsockopt(newsocket, SOL_SOCKET, SO_SNDBUF, (char *)&opt, sizeof(opt)), ret, "setsockopt(SO_SNDBUF)" );
		if (ret == -1)
		{
			Warning ("NET_OpenSocket: setsockopt SO_SNDBUF: %s\n", NET_ErrorString(NET_GetLastError()));
			return 0;
		}

		opt = NET_MAX_MESSAGE; // set TCP options: set receive buffer size
		VCR_NONPLAYBACKFN( setsockopt(newsocket, SOL_SOCKET, SO_RCVBUF, (char *)&opt, sizeof(opt)), ret, "setsockopt(SO_RCVBUF)" );
		if (ret == -1)
		{
			Warning ("NET_OpenSocket: setsockopt SO_RCVBUF: %s\n", NET_ErrorString(NET_GetLastError()));
			return 0;
		}

		return newsocket;	// don't bind TCP sockets by default
	}

	// rest is UDP only

	opt = 0;
	socklen_t len = sizeof( opt );
	VCR_NONPLAYBACKFN( getsockopt( newsocket, SOL_SOCKET, SO_RCVBUF, (char *)&opt, &len ), ret, "getsockopt(SO_RCVBUF)" );
	if ( ret == -1 )
	{
		Warning ("NET_OpenSocket: getsockopt SO_RCVBUF: %s\n", NET_ErrorString(NET_GetLastError()));
		return 0;
	}

	if ( net_showudp.GetBool() )
	{
		static bool bFirst = true;
		if ( bFirst )
		{
			Msg( "UDP socket SO_RCVBUF size %u bytes, changing to %d\n", opt, net_udp_rcvbuf.GetInt() );
		}
		bFirst = false;
	}

	opt = net_udp_rcvbuf.GetInt(); // set UDP receive buffer size
	VCR_NONPLAYBACKFN( setsockopt(newsocket, SOL_SOCKET, SO_RCVBUF, (char *)&opt, sizeof(opt)), ret, "setsockopt(SO_RCVBUF)" );
	if (ret == -1)
	{
		Warning ("NET_OpenSocket: setsockopt SO_RCVBUF: %s\n", NET_ErrorString(NET_GetLastError()));
		return 0;
	}

	opt = net_udp_rcvbuf.GetInt(); // set UDP send buffer size
	VCR_NONPLAYBACKFN( setsockopt(newsocket, SOL_SOCKET, SO_SNDBUF, (char *)&opt, sizeof(opt)), ret, "setsockopt(SO_SNDBUF)" );
	if (ret == -1)
	{
		Warning ("NET_OpenSocket: setsockopt SO_SNDBUF: %s\n", NET_ErrorString(NET_GetLastError()));
		return 0;
	}

 	{
		opt = 1; // set UDP options: make it broadcast capable
		VCR_NONPLAYBACKFN( setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(opt)), ret, "setsockopt(SO_BROADCAST)" );
		if (ret == -1)
		{
			Warning ("NET_OpenSocket: setsockopt SO_BROADCAST: %s\n", NET_ErrorString(NET_GetLastError()));
			return 0;
		}
	}
	
	if ( CommandLine()->FindParm( "-reuse" ) )
	{
		opt = 1; // make it reusable
		VCR_NONPLAYBACKFN( setsockopt(newsocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)), ret, "setsockopt(SO_REUSEADDR)" );
		if (ret == -1)
		{
			Warning ("WARNING: NET_OpenSocket: setsockopt SO_REUSEADDR: %s\n", NET_ErrorString(NET_GetLastError()));
			return 0;
		}
	}

	if (!net_interface || !net_interface[0] || !Q_strcmp(net_interface, "localhost"))
	{
		address.sin_addr.s_addr = INADDR_ANY;
	}
	else
	{
		NET_StringToSockaddr (net_interface, (struct sockaddr *)&address);
	}

	address.sin_family = AF_INET;

	int port_offset;	// try binding socket to port, try next 10 is port is already used

	for ( port_offset = 0; port_offset < PORT_TRY_MAX; port_offset++ )
	{
		if ( port == PORT_ANY )
		{
			address.sin_port = 0;	// = INADDR_ANY
		}
		else
		{
			address.sin_port = NET_HostToNetShort((short)( port + port_offset ));
		}

		VCR_NONPLAYBACKFN( bind (newsocket, (struct sockaddr *)&address, sizeof(address)), ret, "bind" );
		if ( ret != -1 )
		{
			if ( port != PORT_ANY && port_offset != 0 )
			{
				port += port_offset;	// update port
				ConDMsg( "Socket bound to non-default port %i because original port was already in use.\n", port );
			}
			break;
		}

		const int net_error = NET_GetLastError();

		if ( port == PORT_ANY || net_error != WSAEADDRINUSE )
		{
			Warning ("NET_OpenSocket: bind failure %s.\n", NET_ErrorString(net_error));
			NET_CloseSocket(newsocket,-1);
			return 0;
		}

		// Try next port
	}

	const bool bStrictBind = CommandLine()->FindParm( "-strictportbind" );
	if ( port_offset == PORT_TRY_MAX && !bStrictBind )
	{
		Warning( "UDP_OpenSocket: unable to bind socket.\n" );
		NET_CloseSocket( newsocket,-1 );
		return 0;
	}

	if ( port_offset > 0 )
	{
		if ( bStrictBind )
		{
			// The server op wants to exit if the desired port was not avialable.
			Sys_Exit( "ERROR: Port %i was unavailable - quitting due to \"-strictportbind\" command-line flag!\n", port - port_offset );
		}
		else
		{
			Warning( "Port %i was unavailable - bound to port %i instead.\n", port - port_offset, port );
		}
	}
	
	return newsocket;
}

socket_handle NET_ConnectSocket( intp sock, const netadr_t &addr )
{
	Assert( sock >= 0 && sock < net_sockets.Count() );

	netsocket_t *netsock = &net_sockets[sock];
	if ( netsock->hTCP )
	{
		NET_CloseSocket( netsock->hTCP, sock );
	}

	if ( net_notcp )
		return 0;

	sockaddr saddr;
	addr.ToSockadr( &saddr );

	int anyport = PORT_ANY;

	netsock->hTCP = NET_OpenSocket( ipname.GetString(), anyport, true );
	if ( !netsock->hTCP )
	{
		Warning( "NET_ConnectSocket failed opening socket %zd, port %i.\n", sock, net_sockets[sock].nPort );
		return false;
	}

	int ret;
	VCR_NONPLAYBACKFN( connect( netsock->hTCP, &saddr, sizeof(saddr) ), ret, "connect" );
	if ( ret == -1 )
	{
		const int net_error = NET_GetLastError();
		if ( net_error != WSAEWOULDBLOCK )
		{
			Warning( "NET_ConnectSocket: %s.\n", NET_ErrorString( net_error ) );
			return 0;
		}
	}

	return net_sockets[sock].hTCP;
}

int NET_SendStream( socket_handle nSock, const char * buf, int len, int flags )
{
	int ret = VCRHook_send( nSock, buf, len, flags );
	if ( ret == -1 )
	{
		const int net_error = NET_GetLastError();

		if ( net_error == WSAEWOULDBLOCK )
		{
			return 0; // ignore EWOULDBLOCK
		}

		Msg ("NET_SendStream: %s\n", NET_ErrorString( net_error ) );
	}

	return ret;
}

int NET_ReceiveStream( socket_handle nSock, char * buf, int len, int flags )
{
	int ret = VCRHook_recv( nSock, buf, len, flags );
	if ( ret == -1 )
	{
		const int net_error = NET_GetLastError();
		if ( net_error == WSAEWOULDBLOCK || 
			 net_error == WSAENOTCONN )
		{
			return 0; // ignore EWOULDBLOCK
		}

		Msg ("NET_ReceiveStream: %s\n", NET_ErrorString( net_error ) );
	}

	return ret;
}

INetChannel *NET_CreateNetChannel(intp socket, netadr_t *adr, const char * name, INetChannelHandler * handler, bool bForceNewChannel/*=false*/,
								  int nProtocolVersion/*=PROTOCOL_VERSION*/)
{
	CNetChan *chan = NULL;

	if ( !bForceNewChannel && adr != NULL )
	{
		// try to find real network channel if already existing
		if ( ( chan = NET_FindNetChannel( socket, *adr ) ) != NULL )
		{
			// channel already known, clear any old stuff before Setup wipes all
			chan->Clear();
		}
	}

	if ( !chan )
	{
		// create new channel
		chan = new CNetChan();

		AUTO_LOCK( s_NetChannels );
		s_NetChannels.AddToTail( chan );
	}

	NET_ClearLagData( socket );

	// just reset and return
	chan->Setup( socket, adr, name, handler, nProtocolVersion );

	return chan;
}

void NET_RemoveNetChannel(INetChannel *netchan, bool bDeleteNetChan)
{
	if ( !netchan )
	{
		return;
	}

	AUTO_LOCK( s_NetChannels );
	if ( s_NetChannels.Find( static_cast<CNetChan*>(netchan) ) == s_NetChannels.InvalidIndex() )
	{
		DevMsg(1, "NET_CloseNetChannel: unknown channel.\n");
		return;
	}

	s_NetChannels.FindAndRemove( static_cast<CNetChan*>(netchan) );

	NET_ClearQueuedPacketsForChannel( netchan );
	
	if ( bDeleteNetChan )
		delete netchan;
}


/*
=============================================================================

LOOPBACK BUFFERS FOR LOCAL PLAYER

=============================================================================
*/


void NET_SendLoopPacket (intp sock, int length, const unsigned char *data, const netadr_t &to)
{
	loopback_t	*loop;

	if ( length > NET_MAX_PAYLOAD )
	{
		DevMsg( "NET_SendLoopPacket:  packet too big (%i).\n", length );
		return;
	}

	loop = new loopback_t;

	if ( length <= DEF_LOOPBACK_SIZE )
	{
		loop->data = loop->defbuffer;
	}
	else
	{
		loop->data  = new char[ length ];
	}

	Q_memcpy (loop->data, data, length);
	loop->datalen = length;

	if ( sock == NS_SERVER )
	{
		s_LoopBacks[NS_CLIENT].PushItem( loop );
	}
	else if ( sock == NS_CLIENT )
	{
		s_LoopBacks[NS_SERVER].PushItem( loop );
	}
	else
	{
		DevMsg( "NET_SendLoopPacket:  invalid socket (%zd).\n", sock );
		return;
	}
}

//=============================================================================

int NET_CountLaggedList( netpacket_t *pList )
{
	int c = 0;
	netpacket_t *p = pList;
	
	while ( p )
	{
		c++;
		p = p->pNext;
	}

	return c;
}

/*
===================
NET_AddToLagged

===================
*/
void NET_AddToLagged( netpacket_t **pList, netpacket_t *pPacket )
{
	if ( pPacket->pNext )
	{
		Msg("NET_AddToLagged::Packet already linked\n");
		return;
	}

	// first copy packet

	netpacket_t *newPacket = new netpacket_t;

	(*newPacket) = (*pPacket);  // copy packet infos
	newPacket->data = new unsigned char[ pPacket->size ];	// create new data buffer
	Q_memcpy( newPacket->data, pPacket->data, pPacket->size ); // copy packet data
	newPacket->pNext = NULL;

	// if list is empty, this is our first element
	if ( (*pList) == NULL )
	{
		(*pList) = newPacket;	// put packet in top of list
	}
	else
	{
		netpacket_t *last = (*pList);

		while ( last->pNext )
		{
			// got to end of list
			last = last->pNext;
		}

		// add at end
		last->pNext = newPacket;
	}
}

// Actual lag to use in msec
static float s_FakeLag = 0.0;

float NET_GetFakeLag()
{
	return s_FakeLag;
}

// How quickly we converge to a new value for fakelag
#define FAKELAG_CONVERGE	200  // ms per second

/*
==============================
NET_AdjustLag

==============================
*/
void NET_AdjustLag( void )
{
	static double s_LastTime = 0;
	
	// Bound time step
	
	const float dt = static_cast<float>( clamp( net_time - s_LastTime, 0.0, 0.2 ) );
	
	s_LastTime = net_time;

	// Already converged?
	if ( fakelag.GetFloat() == s_FakeLag )
		return;

	// Figure out how far we have to go
	float diff = fakelag.GetFloat() - s_FakeLag;

	// How much can we converge this frame
	float converge = FAKELAG_CONVERGE * dt;

	// Last step, go the whole way
	if ( converge > fabs( diff ) )
	{
		converge = fabsf( diff );
	}

	// Converge toward fakelag.GetFloat()
	if ( diff < 0.0F )
	{
		// Converge toward fakelag.GetFloat()
		s_FakeLag -= converge;
	}
	else
	{
		s_FakeLag += converge;
	}
}


bool NET_LagPacket (bool newdata, netpacket_t * packet)
{
	static int losscount[MAX_SOCKETS];

	if ( packet->source >= MAX_SOCKETS )
		return newdata; // fake lag not supported for extra sockets

	if ( (droppackets.GetInt() > 0)  && newdata && (packet->source == NS_CLIENT) )
	{
		droppackets.SetValue( droppackets.GetInt() - 1 );
		return false;
	}

	if ( fakeloss.GetFloat() && newdata )
	{
		losscount[packet->source]++;

		if ( fakeloss.GetFloat() > 0.0f )
		{
			// Act like we didn't hear anything if we are going to lose the packet.
			// Depends on random # generator.
			if (RandomInt(0,100) <= (int)fakeloss.GetFloat())
				return false;
		}
		else
		{
			int ninterval;

			ninterval = (int)(fabs( fakeloss.GetFloat() ) );
			ninterval = max( 2, ninterval );

			if ( !( losscount[packet->source] % ninterval ) )
			{
				return false;
			}
		}
	}

	if (s_FakeLag <= 0.0)
	{
		// Never leave any old msgs around
		for ( int i=0; i<MAX_SOCKETS; i++ )
		{
			NET_ClearLagData( i );
		}
		return newdata;
	}

	// if new packet arrived in fakelag list
	if ( newdata )
	{
		NET_AddToLagged( &s_pLagData[packet->source], packet );
	}

	// Now check the correct list and feed any message that is old enough.
	netpacket_t *p = s_pLagData[packet->source]; // current packet

	if ( !p )
		return false;	// no packet in lag list

	float target = s_FakeLag;
	float maxjitter = min( fakejitter.GetFloat(), target * 0.5f );
	target += RandomFloat( -maxjitter, maxjitter );

	if ( (p->received + (target/1000.0f)) > net_time )
		return false;	// not time yet for this packet

#ifdef _DEBUG
	if ( fakeshuffle.GetInt() && p->pNext )
	{
		if ( !RandomInt( 0, fakeshuffle.GetInt() ) )
		{
			// swap p and p->next
			netpacket_t * t = p->pNext;
			p->pNext = t->pNext;
			t->pNext = p;
			p = t; 
		}
	}
#endif
	
	// remove packet p from list (is head)
	s_pLagData[packet->source] = p->pNext;
		
	// copy & adjust content
	packet->source	= p->source;	
	packet->from	= p->from;		
	packet->pNext	= NULL;			// no next
	packet->received = net_time;	// new time
	packet->size	= p->size;		
	packet->wiresize = p->wiresize;
	packet->stream	= p->stream;
			
	Q_memcpy( packet->data, p->data, p->size );

	// free lag packet
					
	delete[] p->data;
	delete p;
			
	return true;
}

// Calculate MAX_SPLITPACKET_SPLITS according to the smallest split size
#define MAX_SPLITPACKET_SPLITS ( NET_MAX_MESSAGE / MIN_SPLIT_SIZE )
#define SPLIT_PACKET_STALE_TIME		2.0f
#define SPLIT_PACKET_TRACKING_MAX 256  // most number of outstanding split packets to allow

class CSplitPacketEntry
{
public:
	CSplitPacketEntry()
	{
		memset( &from, 0, sizeof( from ) );

		for ( int i = 0; i < MAX_SPLITPACKET_SPLITS; i++ )
		{
			splitflags[ i ] = -1;
		}

		memset( &netsplit, 0, sizeof( netsplit ) );
		lastactivetime = 0.0;
	}

public:
	netadr_t		from;
	int				splitflags[ MAX_SPLITPACKET_SPLITS ];
	LONGPACKET		netsplit;
	// host_time the last time any entry was received for this entry
	// dimhotepus: float -> double.
	double			lastactivetime;
};

typedef CUtlVector< CSplitPacketEntry > vecSplitPacketEntries_t;
static CUtlVector<vecSplitPacketEntries_t> net_splitpackets;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void NET_DiscardStaleSplitpackets( const intp sock )
{
	vecSplitPacketEntries_t &splitPacketEntries = net_splitpackets[sock];
	intp i;
	for ( i = splitPacketEntries.Count() - 1; i >= 0; i-- )
	{
		CSplitPacketEntry *entry = &splitPacketEntries[ i ];
		Assert( entry );

		if ( net_time < ( entry->lastactivetime + SPLIT_PACKET_STALE_TIME ) )
			continue;

		splitPacketEntries.Remove( i );
	}

	if ( splitPacketEntries.Count() > SPLIT_PACKET_TRACKING_MAX )
	{
		while ( splitPacketEntries.Count() > SPLIT_PACKET_TRACKING_MAX )
		{
			CSplitPacketEntry *entry = &splitPacketEntries[ i ];
			if ( net_time != entry->lastactivetime )
				splitPacketEntries.Remove(0); // we add to tail each time, so head is the oldest entry, kill them first
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *from - 
// Output : CSplitPacketEntry
//-----------------------------------------------------------------------------
CSplitPacketEntry *NET_FindOrCreateSplitPacketEntry( const intp sock, netadr_t *from )
{
	vecSplitPacketEntries_t &splitPacketEntries = net_splitpackets[sock];
	intp i = 0, count = splitPacketEntries.Count();
	for ( auto &entry : splitPacketEntries )
	{
		if ( from->CompareAdr(entry.from) )
			break;

		++i;
	}
	
	CSplitPacketEntry *entry = NULL;
	if ( i >= count )
	{
		CSplitPacketEntry newentry;
		newentry.from = *from;

		splitPacketEntries.AddToTail( newentry );

		entry = &splitPacketEntries[ splitPacketEntries.Count() - 1 ];
	}

	Assert( entry );
	return entry;
}

static char const *DescribeSocket( intp sock )
{
	switch ( sock )
	{
	default:
		break;
	case NS_CLIENT:
		return "cl ";
	case NS_SERVER:
		return "sv ";
	case NS_HLTV:
		return "htv";
	case NS_MATCHMAKING:
		return "mat";
	case NS_SYSTEMLINK:
		return "lnk";
#ifdef LINUX
	case NS_SVLAN:
		return "lan";
#endif
	}

	return "??";
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pData - 
//			size - 
//			*outSize - 
// Output : bool
//-----------------------------------------------------------------------------
bool NET_GetLong( const intp sock, netpacket_t *packet )
{
	int				packetNumber, packetCount, sequenceNumber, offset;
	short			packetID;
	SPLITPACKET		*pHeader;
	
	if ( packet->size < static_cast<int>(sizeof(SPLITPACKET)) ) 
	{
		Msg( "Invalid split packet length %i\n", packet->size );
		return false;
	}

	pHeader = ( SPLITPACKET * )packet->data;
	// pHeader is network endian correct
	sequenceNumber	= LittleLong( pHeader->sequenceNumber );
	packetID		= LittleShort( (short)pHeader->packetID );
	// High byte is packet number
	packetNumber	= ( packetID >> 8 );	
	// Low byte is number of total packets
	packetCount		= ( packetID & 0xff );	

	int nSplitSizeMinusHeader = (int)LittleShort( (short)pHeader->nSplitSize );
	if ( nSplitSizeMinusHeader < MIN_SPLIT_SIZE ||
		 nSplitSizeMinusHeader > MAX_SPLIT_SIZE )
	{
		Msg( "NET_GetLong:  Split packet from %s with invalid split size (number %i/ count %i) where size %i is out of valid range [%llu - %llu]\n", 
			packet->from.ToString(), 
			packetNumber, 
			packetCount, 
			nSplitSizeMinusHeader,
			(uint64)MIN_SPLIT_SIZE,
			(uint64)MAX_SPLIT_SIZE );
		return false;
	}

	if ( packetNumber >= MAX_SPLITPACKET_SPLITS ||
		 packetCount > MAX_SPLITPACKET_SPLITS )
	{
		Msg( "NET_GetLong:  Split packet from %s with too many split parts (number %i/ count %i) where %llu is max count allowed\n", 
			packet->from.ToString(), 
			packetNumber, 
			packetCount, 
			(uint64)MAX_SPLITPACKET_SPLITS );
		return false;
	}

	CSplitPacketEntry *entry = NET_FindOrCreateSplitPacketEntry( sock, &packet->from );
	Assert( entry );
	if ( !entry )
		return false;

	entry->lastactivetime = net_time;
	Assert( packet->from.CompareAdr( entry->from ) );

	// First packet in split series?
	if ( entry->netsplit.currentSequence == -1 || 
		sequenceNumber != entry->netsplit.currentSequence )
	{
		entry->netsplit.currentSequence	= sequenceNumber;
		entry->netsplit.splitCount		= packetCount;
		entry->netsplit.nExpectedSplitSize = nSplitSizeMinusHeader;
	}

	if ( entry->netsplit.nExpectedSplitSize != nSplitSizeMinusHeader )
	{
		Msg( "NET_GetLong:  Split packet from %s with inconsistent split size (number %i/ count %i) where size %i not equal to initial size of %i\n", 
			packet->from.ToString(), 
			packetNumber, 
			packetCount, 
			nSplitSizeMinusHeader,
			entry->netsplit.nExpectedSplitSize
			);
		entry->lastactivetime = net_time + SPLIT_PACKET_STALE_TIME;
		return false;
	}

	int size = packet->size - sizeof(SPLITPACKET);

	if ( entry->splitflags[ packetNumber ] != sequenceNumber )
	{
		// Last packet in sequence? set size
		if ( packetNumber == (packetCount-1) )
		{
			entry->netsplit.totalSize = (packetCount-1) * nSplitSizeMinusHeader + size;
		}

		entry->netsplit.splitCount--;		// Count packet
		entry->splitflags[ packetNumber ] = sequenceNumber;

		if ( net_showsplits.GetInt() && net_showsplits.GetInt() != 3 )
		{
			Msg( "<-- [%s] Split packet %4i/%4i seq %5i size %4i mtu %4llu from %s\n", 
				DescribeSocket( sock ),
				packetNumber + 1, 
				packetCount, 
				sequenceNumber,
				size, 
				(uint64)(nSplitSizeMinusHeader + sizeof( SPLITPACKET )), 
				packet->from.ToString() );
		}
	}
	else
	{
		Msg( "NET_GetLong:  Ignoring duplicated split packet %i of %i ( %i bytes ) from %s\n", packetNumber + 1, packetCount, size, packet->from.ToString() );
	}


	// Copy the incoming data to the appropriate place in the buffer
	offset = (packetNumber * nSplitSizeMinusHeader);
	memcpy( entry->netsplit.buffer + offset, packet->data + sizeof(SPLITPACKET), size );
	
	// Have we received all of the pieces to the packet?
	if ( entry->netsplit.splitCount <= 0 )
	{
		entry->netsplit.currentSequence = -1;	// Clear packet
		if ( entry->netsplit.totalSize > static_cast<int>(sizeof(entry->netsplit.buffer)) )
		{
			Msg("Split packet too large! %d bytes from %s\n", entry->netsplit.totalSize, packet->from.ToString() );
			return false;
		}

		Q_memcpy( packet->data, entry->netsplit.buffer, entry->netsplit.totalSize );
		packet->size = entry->netsplit.totalSize;
		packet->wiresize = entry->netsplit.totalSize;
		return true;
	}

	return false;
}


bool NET_GetLoopPacket ( netpacket_t * packet )
{
	Assert ( packet );

	loopback_t	*loop;

	if ( packet->source > NS_SERVER )
		return false;
		
	if ( !s_LoopBacks[packet->source].PopItem( &loop ) )
	{
		return false;
	}

	if (loop->datalen == 0)
	{
		// no packet in loopback buffer
		delete loop;
		return ( NET_LagPacket( false, packet ) );
	}

	// copy data from loopback buffer to packet 
	packet->from.SetType( NA_LOOPBACK );
	packet->size = loop->datalen;
	packet->wiresize = loop->datalen;
	Q_memcpy ( packet->data, loop->data, packet->size );
	
	loop->datalen = 0; // buffer is avalibale again

	if ( loop->data != loop->defbuffer )
	{
		delete[] loop->data;
		loop->data = loop->defbuffer;
	}

	delete loop;

	// allow lag system to modify packet
	return ( NET_LagPacket( true, packet ) );	
}

bool NET_ReceiveDatagram ( const intp sock, netpacket_t * packet )
{
	VPROF_BUDGET( "NET_ReceiveDatagram", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	Assert ( packet );
	Assert ( net_multiplayer );

	struct sockaddr	from;
	int				fromlen = sizeof(from);
	socket_handle	net_socket = net_sockets[packet->source].hUDP;

	int ret = 0;
	{
		VPROF_BUDGET( "recvfrom", VPROF_BUDGETGROUP_OTHER_NETWORKING );
		ret = VCRHook_recvfrom(net_socket, (char *)packet->data, NET_MAX_MESSAGE, 0, &from, &fromlen );
	}
	if ( ret >= NET_MIN_MESSAGE )
	{
		packet->wiresize = ret;
		if ( !packet->from.SetFromSockadr( &from ) )
		{
			Warning( "Unable to set IPv4 address with family %hu.\n", from.sa_family );
		}

		packet->size = ret;

		if ( net_showudp_wire.GetBool() )
		{
			Msg( "WIRE:  UDP sz=%d tm=%f rt %f from %s\n", ret, net_time, Plat_FloatTime(), packet->from.ToString() );
		}

		MEM_ALLOC_CREDIT();
		CUtlMemoryFixedGrowable< byte, NET_COMPRESSION_STACKBUF_SIZE > bufVoice( NET_COMPRESSION_STACKBUF_SIZE );

		if ( ret < NET_MAX_MESSAGE )
		{
			// Check for split message
			if ( LittleLong( *(int *)packet->data ) == NET_HEADER_FLAG_SPLITPACKET )	
			{
				if ( !NET_GetLong( sock, packet ) )
					return false;
			}
			
			// Next check for compressed message
			if ( LittleLong( *(int *)packet->data) == NET_HEADER_FLAG_COMPRESSEDPACKET )
			{
				char *pCompressedData = (char*)packet->data + sizeof( unsigned int );
				unsigned nCompressedDataSize = packet->wiresize - sizeof( unsigned int );

				// Decompress
				int actualSize = COM_GetUncompressedSize( pCompressedData, nCompressedDataSize );
				if ( actualSize <= 0 || actualSize > NET_MAX_PAYLOAD )
					return false;

				MEM_ALLOC_CREDIT();
				CUtlMemoryFixedGrowable< byte, NET_COMPRESSION_STACKBUF_SIZE > memDecompressed( NET_COMPRESSION_STACKBUF_SIZE );
				memDecompressed.EnsureCapacity( actualSize );

				unsigned uDecompressedSize = (unsigned)actualSize;
				COM_BufferToBufferDecompress( memDecompressed.Base(), &uDecompressedSize, pCompressedData, nCompressedDataSize );
				if ( uDecompressedSize == 0 || ((unsigned int)actualSize) != uDecompressedSize )
				{
					if ( net_showudp.GetBool() )
					{
						Msg( "UDP:  discarding %d bytes from %s due to decompression error [%d decomp, actual %d] at tm=%f rt=%f\n", ret, packet->from.ToString(), uDecompressedSize, actualSize, 
							net_time, Plat_FloatTime() );
					}
					return false;
				}

				// packet->wiresize is already set
				Q_memcpy( packet->data, memDecompressed.Base(), uDecompressedSize );

				packet->size = uDecompressedSize;
			}

			return NET_LagPacket( true, packet );
		}
		else
		{
			ConDMsg ( "NET_ReceiveDatagram:  Oversize packet from %s\n", packet->from.ToString() );
		}
	}
	else if ( ret == -1  )									// error?
	{
		const int net_error = NET_GetLastError();
		switch ( net_error )
		{
		case WSAEWOULDBLOCK:
		case WSAECONNRESET:
		case WSAECONNREFUSED:
			break;
		case WSAEMSGSIZE:
			ConDMsg ("NET_ReceivePacket: %s\n", NET_ErrorString(net_error));
			break;
		default:
			// Let's continue even after errors
			ConDMsg ("NET_ReceivePacket: %s\n", NET_ErrorString(net_error));
			break;
		}
	}

	return false;
}

bool NET_ReceiveValidDatagram ( const intp sock, netpacket_t * packet )
{
#ifdef _DEBUG
	if ( recvpackets.GetInt() >= 0 )
	{
		unsigned long bytes = 0;

		ioctlsocket( net_sockets[ sock ].hUDP , FIONREAD, &bytes );

		if ( bytes <= 0 )
			return false;

		if ( recvpackets.GetInt() == 0 )
			return false;

		recvpackets.SetValue( recvpackets.GetInt() - 1 );
	}
#endif

	// Failsafe: never call recvfrom more than a fixed number of times per frame.
	// We don't like the potential for infinite loops. Yes this means that 66000
	// invalid packets per frame will effectively DOS the server, but at that point
	// you're basically flooding the network and you need to solve this at a higher
	// firewall or router level instead which is beyond the scope of our netcode.
	// --henryg 10/12/2011
	for ( int i = 1000; i > 0; --i )
	{
		// Attempt to receive a valid packet.
		if ( NET_ReceiveDatagram ( sock, packet ) )
		{
			// Received a valid packet.
			return true;
		}
		// NET_ReceiveDatagram calls NET_GetLastError() in case of socket errors
		// or a would-have-blocked-because-there-is-no-data-to-read condition.
		if ( NET_GetLastError() )
		{
			break;
		}
	}
	return false;
}


netpacket_t *NET_GetPacket (intp sock, byte *scratch )
{
	VPROF_BUDGET( "NET_GetPacket", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	// Each socket has its own netpacket to allow multithreading
	netpacket_t &inpacket = net_packets[sock];

	NET_AdjustLag();
	NET_DiscardStaleSplitpackets( sock );

	// setup new packet
	inpacket.from.SetType( NA_IP );
	inpacket.from.Clear();
	inpacket.received = net_time;
	inpacket.source = sock;	
	inpacket.data = scratch;
	inpacket.size = 0;
	inpacket.wiresize = 0;
	inpacket.pNext = NULL;
	inpacket.message.SetDebugName("inpacket.message");

	// Check loopback first
	if ( !NET_GetLoopPacket( &inpacket ) )
	{
		if ( !NET_IsMultiplayer() )
		{
			return NULL;
		}

		// then check UDP data 
		if ( !NET_ReceiveValidDatagram( sock, &inpacket ) )
		{
			// at last check if the lag system has a packet for us
			if ( !NET_LagPacket (false, &inpacket) )
			{
				return NULL;	// we don't have any new packet
			}
		}
	}
	
	Assert ( inpacket.size ); 

#ifdef _DEBUG
	if ( fakenoise.GetInt() > 0 )
	{
		// dimhotepus: Restrict noise bound to packet size to prevent overflow.
		COM_AddNoise( inpacket.data, inpacket.size, std::min(inpacket.size, fakenoise.GetInt()) );
	}
#endif
	
	// prepare bitbuffer for reading packet with new size
	inpacket.message.StartReading( inpacket.data, inpacket.size );

	return &inpacket;
}

void NET_ProcessPending( void )
{
	AUTO_LOCK( s_PendingSockets );
	for ( intp i=0; i<s_PendingSockets.Count();i++ )
	{
		pendingsocket_t * psock = &s_PendingSockets[i];

		ALIGN4 char	headerBuf[5] ALIGN4_POST;

		if ( (net_time - psock->time) > TCP_CONNECT_TIMEOUT )
		{
			NET_CloseSocket( psock->newsock );
			s_PendingSockets.Remove( i );
			continue;
		}

		int ret = NET_ReceiveStream( psock->newsock, headerBuf, sizeof(headerBuf), 0 );

		if ( ret == 0 )
		{
			continue;	// nothing received
		}
		else if ( ret == -1 )
		{
			NET_CloseSocket( psock->newsock );
			s_PendingSockets.Remove( i );
			continue;	// connection closed somehow
		}
		
		bf_read		header( headerBuf, sizeof(headerBuf) );

		int cmd = header.ReadByte();
		// dimhotepus: unsigned long -> uint32
		uint32 challengeNr = header.ReadLong();
		bool bOK = false;	

		if ( cmd == STREAM_CMD_ACKN )
		{
			AUTO_LOCK( s_NetChannels );
			for ( auto *chan : s_NetChannels )
			{
				if ( chan->GetSocket() != psock->netsock )
					continue;

				if ( challengeNr == chan->GetChallengeNr() && !chan->m_StreamSocket )
				{
					if ( psock->addr.CompareAdr( chan->remote_address, true ) )
					{
						chan->m_StreamSocket = psock->newsock;
						chan->m_StreamActive = true;
						
						chan->ResetStreaming();

						bOK = true;

						if ( net_showtcp.GetInt() )
						{
							Msg ("TCP <- %s: connection accepted\n", psock->addr.ToString() );
						}
						
						break;
					}
					else
					{
						Msg ("TCP <- %s: IP address mismatch.\n", psock->addr.ToString() );
					}
				}
			}
		}

		if ( !bOK )
		{
			Msg ("TCP <- %s: invalid connection request.\n", psock->addr.ToString() );
			NET_CloseSocket( psock->newsock );
		}

		s_PendingSockets.Remove( i );
	}
}

static void NET_ProcessListen(netsocket_t &netsock, intp sock)
{
	if ( !netsock.bListening )
		return;

	sockaddr sa;
	int nLengthAddr = sizeof(sa);
		
	socket_handle newSocket;

	VCR_NONPLAYBACKFN( accept( netsock.hTCP, &sa, &nLengthAddr), newSocket, "accept" );
#if !defined( NO_VCR )
	VCRGenericValue( "sockaddr", &sa, sizeof( sa ) );
#endif
	if ( newSocket == kInvalidSocketHandle )
	{
		const int net_error = NET_GetLastError();
		if ( net_error != WSAEWOULDBLOCK )
		{
			ConDMsg ("NET_ThreadListen: %s\n", NET_ErrorString(net_error));
		}
		return;
	}

	// new connection TCP request, put in pending queue

	pendingsocket_t psock;
	psock.newsock = newSocket;
	psock.netsock = sock;
	psock.time = net_time;
	if ( !psock.addr.SetFromSockadr( &sa ) )
	{
		Warning( "Unable to set IPv4 address with family %hu.\n", sa.sa_family );
	}

	{
		// dimhotepus: Reduce lock scope.
		AUTO_LOCK( s_PendingSockets );
		s_PendingSockets.AddToTail( psock );
	}

	// tell client to send challenge number to identify

	char authcmd = STREAM_CMD_AUTH;

	NET_SendStream( newSocket, &authcmd, 1 , 0 );

	if ( net_showtcp.GetInt() )
	{
		Msg ("TCP <- %s: connection request.\n", psock.addr.ToString() );
	}
}

struct TSLIST_NODE_ALIGN NetScratchBuffer_t : public CAlignedNewDelete<TSLIST_NODE_ALIGNMENT, TSLNodeBase_t>
{
	byte data[NET_MAX_MESSAGE];
};
CTSSimpleList<NetScratchBuffer_t> g_NetScratchBuffers;

void NET_ProcessSocket( intp sock, IConnectionlessPacketHandler *handler )
{
	VPROF_BUDGET( "NET_ProcessSocket", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	netpacket_t * packet;
	
	Assert ( sock >= 0 && sock<net_sockets.Count() );

	// Scope for the auto_lock
	{
		AUTO_LOCK( s_NetChannels );

		// get streaming data from channel sockets
		intp numChannels = s_NetChannels.Count();

		for ( intp i = (numChannels-1); i >= 0 ; i-- )
		{
			CNetChan *netchan = s_NetChannels[i];

			// sockets must match
			if ( sock != netchan->GetSocket() )
				continue;

			if ( !netchan->ProcessStream() )
			{
				netchan->GetMsgHandler()->ConnectionCrashed("TCP connection failed.");
			}
		}
	}

	// now get datagrams from sockets
	NetScratchBuffer_t *scratch = g_NetScratchBuffers.Pop();
	if ( !scratch )
	{
		scratch = new NetScratchBuffer_t;
	}
	while ( ( packet = NET_GetPacket ( sock, scratch->data ) ) != NULL )
	{
		if ( Filter_ShouldDiscard ( packet->from ) )	// filtering is done by network layer
		{
			Filter_SendBan( packet->from );	// tell them we aren't listening...
			continue;
		} 

		// check for connectionless packet (0xffffffff) first
		if ( LittleLong( *(unsigned int *)packet->data ) == CONNECTIONLESS_HEADER )
		{
			(void)packet->message.ReadLong();	// read the -1

			if ( net_showudp.GetInt() )
			{
				Msg("UDP <- %s: sz=%i OOB '%c' wire=%i\n", packet->from.ToString(), packet->size, packet->data[4], packet->wiresize );
			}

			handler->ProcessConnectionlessPacket( packet );
			continue;
		}

		// check for packets from connected clients
		
		CNetChan * netchan = NET_FindNetChannel( sock, packet->from );

		if ( netchan )
		{
			netchan->ProcessPacket( packet, true );
		}
		/* else	// Not an error that may happen during connect or disconnect
		{
			Msg ("Sequenced packet without connection from %s\n" , packet->from.ToString() );
		}*/
	}
	g_NetScratchBuffers.Push( scratch );
}

void NET_LogBadPacket(netpacket_t * packet)
{
	int i = 0;
	char filename[ MAX_OSPATH ];
	bool done = false;

	// dimhotepus: 1000 -> 1024.
	while ( i < 1024 && !done )
	{
		Q_snprintf( filename, sizeof( filename ), "badpacket%03i.dat", i );
		FileHandle_t fp = g_pFileSystem->Open( filename, "rb" );
		if ( !fp )
		{
			fp = g_pFileSystem->Open( filename, "wb" );
			// dimhotepus: Only if file is open.
			if ( fp )
			{
				RunCodeAtScopeExit(g_pFileSystem->Close( fp ));

				g_pFileSystem->Write( packet->data, packet->size, fp );

				done = true;
			}
		}
		else
		{
			g_pFileSystem->Close( fp );
		}
		i++;
	}

	if ( i < 1024 )
	{
		Msg( "Error buffer for %s written to %s\n", packet->from.ToString(), filename );
	}
	else
	{
		Msg( "Couldn't write error buffer, delete error###.dat files to make space\n" );
	}
}

int NET_SendToImpl( SOCKET s, const char FAR * buf, int len, const struct sockaddr FAR * to, int tolen, int iGameDataLength )
{
	int nSend = sendto( s, buf, len, 0, to, tolen );
	return nSend;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sock - 
//			s - 
//			buf - 
//			len - 
//			flags - 
//			to - 
//			tolen - 
// Output : int
//-----------------------------------------------------------------------------
int NET_SendTo( bool verbose, SOCKET s, const char FAR * buf, int len, const struct sockaddr FAR * to, int tolen, int iGameDataLength )
{	
	int nSend = 0;

	VPROF_BUDGET( "NET_SendTo", VPROF_BUDGETGROUP_OTHER_NETWORKING );
	
	// If it's 0.0.0.0:0, then it's a fake player + sv_stressbots and we've plumbed everything all 
	// the way through here, where we finally bail out.
	const sockaddr_in *pInternetAddr = (const sockaddr_in*)to;
#ifdef _WIN32
	if ( pInternetAddr->sin_addr.S_un.S_addr == 0
#else
	if ( pInternetAddr->sin_addr.s_addr == 0 
#endif
		&& pInternetAddr->sin_port == 0 )
	{		
		return len;
	}

	// Normally, we shouldn't need to write this data to the file, but it can help catch
	// out-of-sync errors earlier.
	if ( VCRGetMode() != VCR_Disabled && vcr_verbose.GetInt() )
	{
#if !defined( NO_VCR )
		VCRGenericValue( "senddata", &len, sizeof( len ) );
		VCRGenericValue( "senddata2", (char*)buf, len );
#endif
	}

	// Don't send anything out in VCR mode.. it just annoys other people testing in multiplayer.
	if ( VCRGetMode() != VCR_Playback )
	{
		nSend = NET_SendToImpl
		( 
			s, 
			buf,
			len,
			to, 
			tolen, 
			iGameDataLength 
		);
	}

#if defined( _DEBUG )
	if ( verbose && 
		( nSend > 0 ) && 
		( len > MAX_ROUTABLE_PAYLOAD ) )
	{
		ConDMsg( "NET_SendTo:  Packet length (%i) > (%i) bytes\n", len, MAX_ROUTABLE_PAYLOAD );
	}
#endif
	return nSend;
}

#if defined( _DEBUG )

#include "filesystem.h"
#include "filesystem_engine.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
char const *NET_GetDebugFilename( char const *prefix )
{
	static char filename[ MAX_OSPATH ];

	int i;

	for ( i = 0; i < 10000; i++ )
	{
		Q_snprintf( filename, sizeof( filename ), "debug/%s%04i.dat", prefix, i );
		if ( g_pFileSystem->FileExists( filename ) )
			continue;

		return filename;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//			*buf - 
//			len - 
//-----------------------------------------------------------------------------
void NET_StorePacket( char const *filename, byte const *buf, int len )
{
	g_pFileSystem->CreateDirHierarchy( "debug/", "DEFAULT_WRITE_PATH" );

	FileHandle_t fh = g_pFileSystem->Open( filename, "wb" );
	if ( fh )
	{
		RunCodeAtScopeExit(g_pFileSystem->Close( fh ));

		g_pFileSystem->Write( buf, len, fh );
	}
}

#endif // _DEBUG

struct SendQueueItem_t
{
	SendQueueItem_t() :
		m_pChannel( NULL ),
		m_Socket( kInvalidSocketHandle )
	{
	}

	CNetChan	*m_pChannel;
	socket_handle	m_Socket;
	CUtlBuffer	m_Buffer;
	CUtlBuffer	m_To;
};

struct SendQueue_t
{
	SendQueue_t() : 
		m_nHostFrame( 0 )
	{
	}
	int									m_nHostFrame;
	CUtlLinkedList< SendQueueItem_t >	m_SendQueue;
};

static SendQueue_t g_SendQueue;

int NET_QueuePacketForSend( CNetChan *chan, bool verbose, socket_handle s, const char FAR *buf, int len, const struct sockaddr FAR * to, int tolen, uint32 msecDelay )
{
	// If net_queued_packet_thread was -1 at startup, then we don't even have a thread.
	if ( net_queued_packet_thread.GetInt() && g_pQueuedPackedSender->IsRunning() )
	{
		g_pQueuedPackedSender->QueuePacket( chan, s, buf, len, to, tolen, msecDelay );
	}
	else
	{
		Assert( chan );
		// Set up data structure
		SendQueueItem_t *sq = &g_SendQueue.m_SendQueue[ g_SendQueue.m_SendQueue.AddToTail() ];
		sq->m_Socket = s;
		sq->m_pChannel = chan;
		sq->m_Buffer.Put( (const void *)buf, len );
		sq->m_To.Put( (const void *)to, tolen );
		sq->m_pChannel->IncrementQueuedPackets();
	}
	
	return len;
}

void NET_SendQueuedPacket( SendQueueItem_t *sq )
{
	// Msg( "Send queued packet %d\n", sq->m_Buffer.TellPut() );
	NET_SendTo
	( 
		false, 
		sq->m_Socket, 
		( const char FAR * )sq->m_Buffer.Base(), 
		sq->m_Buffer.TellPut(), 
		( const struct sockaddr FAR * )sq->m_To.Base(), 
		sq->m_To.TellPut() , -1
	);

	sq->m_pChannel->DecrementQueuedPackets();
}

void NET_ClearQueuedPacketsForChannel( INetChannel *channel )
{
	CUtlLinkedList< SendQueueItem_t >& list = g_SendQueue.m_SendQueue;

	for ( unsigned short i = list.Head(); i != list.InvalidIndex();  )
	{
		unsigned short n = list.Next( i );
		SendQueueItem_t &e = list[ i ];
		if ( e.m_pChannel == channel )
		{
			list.Remove( i );
		}
		i = n;
	}
}

void NET_SendQueuedPackets()
{
	// Only do this once per frame
	if ( host_framecount == g_SendQueue.m_nHostFrame )
		return;
	g_SendQueue.m_nHostFrame = host_framecount;

	CUtlLinkedList< SendQueueItem_t >& list = g_SendQueue.m_SendQueue;

	int nRemaining = net_splitrate.GetInt();
	while ( nRemaining )
	{
		if ( list.IsValidIndex( list.Head() ) )
		{
			SendQueueItem_t *sq = &list[ list.Head() ];
			NET_SendQueuedPacket( sq );
			list.Remove( list.Head() );
			--nRemaining;
		}
		else
		{
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sock - 
//			s - 
//			buf - 
//			len - 
//			flags - 
//			to - 
//			tolen - 
// Output : int
//-----------------------------------------------------------------------------
static CInterlockedInt s_SplitPacketSequenceNumber[ MAX_SOCKETS ] = {1};
static ConVar net_splitpacket_maxrate( "net_splitpacket_maxrate", SPLITPACKET_MAX_DATA_BYTES_PER_SECOND, 0, "Max bytes per second when queueing splitpacket chunks", true, MIN_RATE, true, MAX_RATE );

int NET_SendLong( INetChannel *chan, intp sock, socket_handle s, const char FAR * buf, int len, const struct sockaddr FAR * to, int tolen, int nMaxRoutableSize )
{
	VPROF_BUDGET( "NET_SendLong", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	CNetChan *netchan = dynamic_cast< CNetChan * >( chan );

	short nSplitSizeMinusHeader = nMaxRoutableSize - static_cast<int>( sizeof( SPLITPACKET ) );

	int nSequenceNumber = -1;
	if ( netchan )
	{
		nSequenceNumber = netchan->IncrementSplitPacketSequence();
	}
	else
	{
		nSequenceNumber = ++s_SplitPacketSequenceNumber[ sock ];
	}

	const char *sendbuf = buf;
	int sendlen = len;

	alignas(SPLITPACKET) char			packet[ MAX_ROUTABLE_PAYLOAD ];
	SPLITPACKET		*pPacket = (SPLITPACKET *)packet;

	// Make pPacket data network endian correct
	pPacket->netID = LittleLong( NET_HEADER_FLAG_SPLITPACKET );
	pPacket->sequenceNumber = LittleLong( nSequenceNumber );
	pPacket->nSplitSize = LittleShort( nSplitSizeMinusHeader );
	
	int nPacketCount = (sendlen + nSplitSizeMinusHeader - 1) / nSplitSizeMinusHeader;

#if defined( _DEBUG )
	if ( net_savelargesplits.GetInt() != -1 && nPacketCount >= net_savelargesplits.GetInt() )
	{
		char const *filename = NET_GetDebugFilename( "splitpacket" );
		if ( filename )
		{
			Msg( "Saving split packet of %i bytes and %i packets to file %s\n",
				sendlen, nPacketCount, filename );

			NET_StorePacket( filename, (byte const *)sendbuf, sendlen );
		}
		else
		{
			Msg( "Too many files in debug directory, clear out old data!\n" );
		}
	}
#endif

	int nBytesLeft = sendlen;
	int nPacketNumber = 0;
	int nTotalBytesSent = 0;
	int nFragmentsSent = 0;

	while ( nBytesLeft > 0 )
	{
		int size = min( (int)nSplitSizeMinusHeader, nBytesLeft );

		pPacket->packetID = LittleShort( (short)(( nPacketNumber << 8 ) + nPacketCount) );
		
		Q_memcpy( packet + sizeof(SPLITPACKET), sendbuf + (nPacketNumber * nSplitSizeMinusHeader), size );
		
		int ret = 0;

		// Setting net_queued_packet_thread to NET_QUEUED_PACKET_THREAD_DEBUG_VALUE goes into a mode where all packets are queued.. can be used to stress-test it.
		// Linux threads aren't prioritized well enough for this to work well (i.e. the queued packet thread doesn't get enough
		// attention to flush itself well). The behavior the queue fixes is that if you send too many DP packets
		// without giving up your timeslice, it'll just discard the 7th and later packets until you Sleep() (issue might be on client recipient side, need to
		// snif packets to double check)

		if ( netchan && (nFragmentsSent >= net_splitrate.GetInt() || net_queued_packet_thread.GetInt() == NET_QUEUED_PACKET_THREAD_DEBUG_VALUE) )
		{
			// Don't let this rate get too high (SPLITPACKET_MAX_DATA_BYTES_PER_SECOND == 15000 bytes/sec) 
			// or user's won't be able to receive all of the parts since they'll be too close together.
			/// XXX(JohnS): (float)cv.GetInt() is just preserving what this was doing before to avoid changing the
			///             semantics of this convar
			float flMaxSplitpacketDataRateBytesPerSecond = min( (float)netchan->GetDataRate(), (float)net_splitpacket_maxrate.GetInt() );

			// Calculate the delay (measured from now) for when this packet should be sent.
			uint32 delay = (int)( 1000.0f * ( (float)( nPacketNumber * ( nMaxRoutableSize + UDP_HEADER_SIZE ) ) / flMaxSplitpacketDataRateBytesPerSecond ) + 0.5f );

			ret = NET_QueuePacketForSend( netchan, false, s, packet, size + sizeof(SPLITPACKET), to, tolen, delay );
		}
		else
		{
			// Also, we send the first packet no matter what
			// w/o a netchan, if there are too many splits, its possible the packet can't be delivered.  However, this would only apply to out of band stuff like
			//  server query packets, which should never require splitting anyway.
			ret = NET_SendTo( false, s, packet, size + sizeof(SPLITPACKET), to, tolen, -1 );
		}

		// First split send
		++nFragmentsSent;

		if ( ret < 0 )
		{
			return ret;
		}

		if ( ret >= size )
		{
			nTotalBytesSent += size;
		}

		nBytesLeft -= size;
		++nPacketNumber;

		// Always bitch about split packets in debug
		if ( net_showsplits.GetInt() && net_showsplits.GetInt() != 2 )
		{
			netadr_t adr;

			Msg( "--> [%s] Split packet %4i/%4i seq %5i size %4i mtu %4i to %s [ total %4i ]\n",
				DescribeSocket( sock ),
				nPacketNumber, 
				nPacketCount, 
				nSequenceNumber,
				size,
				nMaxRoutableSize,
				adr.SetFromSockadr( to ) ? adr.ToString() : "N/A",
				sendlen );
		}
	}
	
	return nTotalBytesSent;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sock - 
//			length - 
//			*data - 
//			to - 
// Output : void NET_SendPacket
//-----------------------------------------------------------------------------

int NET_SendPacket ( INetChannel *chan, intp sock,  const netadr_t &to, const unsigned char *data, int length, bf_write *pVoicePayload /* = NULL */, bool bUseCompression /*=false*/ )
{
	VPROF_BUDGET( "NET_SendPacket", VPROF_BUDGETGROUP_OTHER_NETWORKING );
	ETWSendPacket( to.ToString() , length , 0 , 0 );

	int		ret;
	struct sockaddr	addr;
	socket_handle	net_socket;

	if ( net_showudp.GetInt() && (*(const unsigned int*)data == CONNECTIONLESS_HEADER) )
	{
		Assert( !bUseCompression );
		Msg("UDP -> %s: sz=%i OOB '%c'\n", to.ToString(), length, data[4] );
	}

	if ( !NET_IsMultiplayer() || to.type == NA_LOOPBACK || ( to.IsLocalhost() && !net_usesocketsforloopback.GetBool() ) )
	{
		Assert( !pVoicePayload );

		NET_SendLoopPacket (sock, length, data, to);
		return length;
	}

	if ( to.type == NA_BROADCAST )
	{
		net_socket = net_sockets[sock].hUDP;
		if (!net_socket)
			return length;
	}
	else if ( to.type == NA_IP )
	{
		net_socket = net_sockets[sock].hUDP;
		if (!net_socket)
			return length;
	}
	else
	{
		DevMsg("NET_SendPacket: bad address type (%i)\n", to.type );
		return length;
	}

	if ( (droppackets.GetInt() < 0)  && sock == NS_CLIENT )
	{
		droppackets.SetValue( droppackets.GetInt() + 1 );
		return length;
	}

	if ( fakeloss.GetFloat() > 0.0f )
	{
		// simulate sending this packet
		if (RandomInt(0,100) <= (int)fakeloss.GetFloat())
			return length;
	}

	to.ToSockadr ( &addr );

	MEM_ALLOC_CREDIT();
	CUtlMemoryFixedGrowable< byte, NET_COMPRESSION_STACKBUF_SIZE > memCompressed( NET_COMPRESSION_STACKBUF_SIZE );
	CUtlMemoryFixedGrowable< byte, NET_COMPRESSION_STACKBUF_SIZE > memCompressedVoice( NET_COMPRESSION_STACKBUF_SIZE );

	int iGameDataLength = pVoicePayload ? length : -1;

	bool bWroteVoice = false;
	unsigned int nVoiceBytes = 0;

	if ( pVoicePayload )
	{
		VPROF_BUDGET( "NET_SendPacket_CompressVoice", VPROF_BUDGETGROUP_OTHER_NETWORKING );
		unsigned int nCompressedLength = COM_GetIdealDestinationCompressionBufferSize_Snappy( pVoicePayload->GetNumBytesWritten() );
		memCompressedVoice.EnsureCapacity( nCompressedLength + sizeof( unsigned short ) );

		uint8 *pVoice = memCompressedVoice.Base();

		unsigned short usVoiceBits = pVoicePayload->GetNumBitsWritten();
		*( unsigned short * )pVoice = LittleShort( usVoiceBits );
		pVoice += sizeof( unsigned short );
		
		byte *pOutput = NULL;
		if ( net_compressvoice.GetBool() )
		{
			if ( COM_BufferToBufferCompress_Snappy( pVoice, &nCompressedLength, pVoicePayload->GetData(), pVoicePayload->GetNumBytesWritten()
				&& ( (int)nCompressedLength < pVoicePayload->GetNumBytesWritten() ) ) )
			{
				pOutput = pVoice;
			}
		}
		if ( !pOutput )
		{
			Q_memcpy( pVoice, pVoicePayload->GetData(), pVoicePayload->GetNumBytesWritten() );
		}

		nVoiceBytes = nCompressedLength + sizeof( unsigned short );
	}

	if ( bUseCompression )
	{
		VPROF_BUDGET( "NET_SendPacket_Compress", VPROF_BUDGETGROUP_OTHER_NETWORKING );
		unsigned int nCompressedLength = COM_GetIdealDestinationCompressionBufferSize_Snappy( length );
	
		memCompressed.EnsureCapacity( nCompressedLength + nVoiceBytes + sizeof( unsigned int ) );

		*(int *)memCompressed.Base() = LittleLong( NET_HEADER_FLAG_COMPRESSEDPACKET );

		if ( COM_BufferToBufferCompress_Snappy( memCompressed.Base() + sizeof( unsigned int ), &nCompressedLength, data, length )
			&& (int)nCompressedLength < length )
		{
			data	= memCompressed.Base();
			length	= nCompressedLength + sizeof( unsigned int );

			if ( pVoicePayload && pVoicePayload->GetNumBitsWritten() > 0 )
			{
				uint8 *pVoice = memCompressed.Base() + length;
				Q_memcpy( pVoice, memCompressedVoice.Base(), nVoiceBytes );
			}
			
			iGameDataLength = length;

			length += nVoiceBytes;

			bWroteVoice = true;
		}
	}
	
	if ( !bWroteVoice && pVoicePayload && pVoicePayload->GetNumBitsWritten() > 0 )
	{
		memCompressed.EnsureCapacity( length + nVoiceBytes );

		uint8 *pVoice = memCompressed.Base();
		Q_memcpy( pVoice, (const void *)data, length );
		pVoice += length;
		Q_memcpy( pVoice, memCompressedVoice.Base(), nVoiceBytes );
		data	= memCompressed.Base();

		length  += nVoiceBytes;
	}

	// Do we need to break this packet up?
	int nMaxRoutable = MAX_ROUTABLE_PAYLOAD;
	if ( chan )
	{
		nMaxRoutable = clamp( chan->GetMaxRoutablePayloadSize(), MIN_USER_MAXROUTABLE_SIZE, min( sv_maxroutable.GetInt(), MAX_USER_MAXROUTABLE_SIZE ) );
	}

	if ( length <= nMaxRoutable && 
		!(net_queued_packet_thread.GetInt() == NET_QUEUED_PACKET_THREAD_DEBUG_VALUE && chan ) )	
	{
		// simple case, small packet, just send it
		ret = NET_SendTo( true, net_socket, (const char *)data, length, &addr, sizeof(addr), iGameDataLength );
	}
	else
	{
		// split packet into smaller pieces
		ret = NET_SendLong( chan, sock, net_socket, (const char *)data, length, &addr, sizeof(addr), nMaxRoutable );
	}
	
	if (ret == -1)
	{
		const int net_error = NET_GetLastError();
		
		// wouldblock is silent
		if ( net_error == WSAEWOULDBLOCK )
			return 0;

		if ( net_error == WSAECONNRESET )
			return 0;

		// some PPP links dont allow broadcasts
		if ( ( net_error == WSAEADDRNOTAVAIL) && ( to.type == NA_BROADCAST ) )
			return 0;

		ConDMsg ("NET_SendPacket Warning: %s : %s\n", NET_ErrorString(net_error), to.ToString() );
		ret = length;
	}
	

	return ret;
}

void NET_OutOfBandPrintf(intp sock, const netadr_t &adr, PRINTF_FORMAT_STRING const char *format, ...) FMTFUNCTION( 3, 4 )
{
	va_list		argptr;
	alignas(unsigned int) char		string[MAX_ROUTABLE_PAYLOAD];
	
	*(unsigned int*)string = CONNECTIONLESS_HEADER;

	va_start (argptr, format); //-V2019 //-V2018
	V_vsnprintf (string+4, sizeof( string ) - 4, format,argptr);
	va_end (argptr);

	intp length = Q_strlen(string+4) + 5;

	NET_SendPacket ( NULL, sock, adr, (byte *)string, length );
}

/*
====================
NET_CloseAllSockets
====================
*/
void NET_CloseAllSockets (void)
{
	// shut down any existing and open sockets
	for (int i=0 ; i<net_sockets.Count() ; i++)
	{
		if ( net_sockets[i].nPort )
		{
			NET_CloseSocket( net_sockets[i].hUDP );
			NET_CloseSocket( net_sockets[i].hTCP );

			net_sockets[i].nPort = 0;
			net_sockets[i].bListening = false;
			net_sockets[i].hUDP = 0;
			net_sockets[i].hTCP = 0;
		}
	}

	// shut down all pending sockets
	AUTO_LOCK( s_PendingSockets );
	for( auto &psock : s_PendingSockets )
	{
		NET_CloseSocket( psock.newsock );
	}

	s_PendingSockets.RemoveAll();
}

/*
====================
NET_FlushAllSockets
====================
*/
void NET_FlushAllSockets( void )
{
	// drain any packets that my still lurk in our incoming queue
	char data[2048];
	struct sockaddr	from;
	int	fromlen = sizeof(from);
	
	for (int i=0 ; i<net_sockets.Count() ; i++)
	{
		if ( net_sockets[i].hUDP )
		{
			int bytes = 1;

			// loop until no packets are pending anymore
			while ( bytes > 0  )
			{
				bytes = VCRHook_recvfrom( net_sockets[i].hUDP, data, sizeof(data), 0, (struct sockaddr *)&from, (int *)&fromlen );
			}
		}
	}
}

enum
{
	OSOCKET_FLAG_USE_IPNAME  = 0x00000001, // Use ipname convar for net_interface.
	OSOCKET_FLAG_FAIL        = 0x00000002, // Call Sys_exit on error.
};

static bool OpenSocketInternal( intp nModule, int nSetPort, int nDefaultPort, const char *pName, int nProtocol, bool bTryAny,
								int flags = ( OSOCKET_FLAG_USE_IPNAME | OSOCKET_FLAG_FAIL ) )
{
	int port = nSetPort ? nSetPort : nDefaultPort;
	socket_handle *handle = NULL;

	if( nProtocol == IPPROTO_TCP )
	{
		handle = &net_sockets[nModule].hTCP;
	}
	else if ( nProtocol == IPPROTO_UDP )
	{
		handle = &net_sockets[nModule].hUDP;
	}
	else
	{
		Sys_Error( "Unrecognized protocol type %d", nProtocol );
		return false;
	}

	if ( !net_sockets[nModule].nPort )
	{
		const char *netinterface = ( flags & OSOCKET_FLAG_USE_IPNAME ) ? ipname.GetString() : NULL;

		*handle = NET_OpenSocket (netinterface, port, nProtocol );
		if ( !*handle && bTryAny )
		{
			port = PORT_ANY;	// try again with PORT_ANY
			*handle = NET_OpenSocket ( netinterface, port, nProtocol );
		}

		if ( !*handle )
		{
			if ( flags & OSOCKET_FLAG_FAIL )
				Sys_Exit( "Couldn't allocate any %s IP port", pName );
			return false;
		}

		net_sockets[nModule].nPort = port;
	}
	else
	{
		Msg( "WARNING: NET_OpenSockets: %s port %i already open.\n", pName, net_sockets[nModule].nPort );
		return false;
	}

	return ( net_sockets[nModule].nPort != 0 );
}

/*
====================
NET_OpenSockets
====================
*/
void NET_OpenSockets (void)
{	
	const int nProtocol = IPPROTO_UDP;

	OpenSocketInternal( NS_SERVER, hostport.GetInt(), PORT_SERVER, "server", nProtocol, false );
	OpenSocketInternal( NS_CLIENT, clientport.GetInt(), PORT_SERVER, "client", nProtocol, true );

	if ( !net_nohltv )
	{
		OpenSocketInternal( NS_HLTV, hltvport.GetInt(), PORT_HLTV, "hltv", nProtocol, false );
	}

	if ( IsX360() )
	{
		OpenSocketInternal( NS_MATCHMAKING, matchmakingport.GetInt(), PORT_MATCHMAKING, "matchmaking", nProtocol, false );
		OpenSocketInternal( NS_SYSTEMLINK, systemlinkport.GetInt(), PORT_SYSTEMLINK, "systemlink", IPPROTO_UDP, false );
	}

#ifdef LINUX
	// On Linux, if you bind to a specific address then you will NOT receive broadcast messages.
	// This means that if you do a +ip X.X.X.X, your game will not show up on the LAN server browser page.
	// To workaround this, if the user has specified sv_lan and an IP address, we open an INADDR_ANY port.
	// See https://developerweb.net/viewtopic.php?id=5722 for more information.
	extern ConVar sv_lan;
	if ( sv_lan.GetBool() )
	{
		const char *net_interface = ipname.GetString();
		// If net_interface was specified and it's not localhost...
		if ( net_interface[ 0 ] && ( Q_strcmp( net_interface, "localhost" ) != 0 ) )
		{
			// From clientdll/matchmaking/ServerList.cpp, the ports queried are:
			//   27015 - 27020, 26900 - 26905
			//   4242: RDKF, 27215: Lost Planet
			static int s_ports[] =
			{
				26900, 26901, 26902, 26903, 26904, 26905,
				27015, 27016, 27017, 27018, 27019, 27020
			};

			for ( size_t iport = 0; iport < ARRAYSIZE( s_ports ); iport++ )
			{
				bool bPortUsed = false;
				for ( int i = NS_CLIENT; i < NS_SVLAN; i++ )
				{
					// Move along if this port is already used.
					if ( net_sockets[ i ].nPort == s_ports[ iport ] )
					{
						bPortUsed = true;
						break;
					}
				}

				if ( !bPortUsed )
				{
					// Try to open the socket and break if we succeeded.
					if ( OpenSocketInternal( NS_SVLAN, s_ports[ iport ], PORT_SERVER, "lan", nProtocol, false, 0 ) )
						break;
				}
			}

			if ( net_sockets[ NS_SVLAN ].nPort )
				Msg( "Opened sv_lan port %d\n", net_sockets[ NS_SVLAN ].nPort );
			else
				Warning( "%s, Failed to open sv_lan port.\n", __FUNCTION__ );
		}
	}
#endif // LINUX
}

intp NET_AddExtraSocket( int port )
{
	intp newSocket = net_sockets.AddToTail();

	Q_memset( &net_sockets[newSocket], 0, sizeof(netsocket_t) );

	OpenSocketInternal( newSocket, port, PORT_ANY, "extra", IPPROTO_UDP, true );

	net_packets.EnsureCount( newSocket+1 );
	net_splitpackets.EnsureCount( newSocket+1 );

	return newSocket;
}

void NET_RemoveAllExtraSockets()
{
	for (int i=MAX_SOCKETS ; i<net_sockets.Count() ; i++)
	{
		if ( net_sockets[i].nPort )
		{
			NET_CloseSocket( net_sockets[i].hUDP );
			NET_CloseSocket( net_sockets[i].hTCP );
		}
	}
	net_sockets.RemoveMultiple( MAX_SOCKETS, net_sockets.Count()-MAX_SOCKETS );

	Assert( net_sockets.Count() == MAX_SOCKETS );
}

unsigned short NET_GetUDPPort(intp socket)
{
	if ( socket < 0 || socket >= net_sockets.Count() )
		return 0;

	return net_sockets[socket].nPort;
}


/*
================
NET_GetLocalAddress

Returns the servers' ip address as a string.
================
*/
void NET_GetLocalAddress (void)
{
	net_local_adr.Clear();

	if ( net_noip )
	{
		Msg("TCP/UDP Disabled.\n");
	}
	else
	{
		char	buff[512];

		// If we have changed the ip var from the command line, use that instead.
		if ( Q_strcmp(ipname.GetString(), "localhost") )
		{
			Q_strncpy(buff, ipname.GetString(), sizeof( buff ) );	// use IP set with ipname
		}
		else
		{
			gethostname( buff, sizeof(buff) );	// get own IP address
			buff[sizeof(buff)-1] = 0;			// Ensure that it doesn't overrun the buffer
		}

		NET_StringToAdr (buff, &net_local_adr);

		int ipaddr = ( net_local_adr.ip[0] << 24 ) + 
					 ( net_local_adr.ip[1] << 16 ) + 
					 ( net_local_adr.ip[2] << 8 ) + 
					   net_local_adr.ip[3];

		hostip.SetValue( ipaddr );
	}
}


/*
====================
NET_IsConfigured

Is winsock ip initialized?
====================
*/
bool NET_IsMultiplayer( void )
{
	return net_multiplayer;
}

bool NET_IsDedicated( void )
{
	return net_dedicated;
}

/*
====================
NET_SetTime

Updates net_time
====================
*/
void NET_SetTime( double flRealtime )
{
	static double s_last_realtime = 0;

	double frametime = flRealtime - s_last_realtime;
	s_last_realtime = flRealtime;

	if ( frametime > 1.0f )
	{
		// if we have very long frame times because of loading stuff
		// don't apply that to net time to avoid unwanted timeouts
		frametime = 1.0f;
	}
	else if ( frametime < 0.0f )
	{
		frametime = 0.0f;
	}

	// adjust network time so fakelag works with host_timescale
	net_time += frametime * host_timescale.GetFloat();
}

/*
====================
NET_RunFrame

RunFrame must be called each system frame before reading/sending on any socket
====================
*/
void NET_RunFrame( double flRealtime )
{
	NET_SetTime( flRealtime );

	RCONServer().RunFrame();

#ifdef ENABLE_RPT
	RPTServer().RunFrame();
#endif // ENABLE_RPT

#ifndef SWDS
	RCONClient().RunFrame();
#ifdef ENABLE_RPT
	RPTClient().RunFrame();
#endif // ENABLE_RPT

#endif // SWDS

	if ( !NET_IsMultiplayer() || net_notcp )
		return;

	intp i{0};

	// process TCP sockets:
	for ( auto &ns : net_sockets )
	{
		if ( ns.hTCP && ns.bListening )
		{
			NET_ProcessListen( ns, i );
		}

		++i;
	}

	NET_ProcessPending();
}

void NET_ClearLoopbackBuffers()
{
	for ( auto &lb : s_LoopBacks )
	{
		loopback_t *loop;

		while ( lb.PopItem( &loop ) )
		{
			if ( loop->data && loop->data != loop->defbuffer )
			{
				delete [] loop->data;
			}
			delete loop;
		}
	}
}

void NET_ConfigLoopbackBuffers( bool bAlloc )
{
	NET_ClearLoopbackBuffers();
}

/*
====================
NET_Config

A single player game will only use the loopback code
====================
*/

void NET_Config ( void )
{
	// free anything
	NET_CloseAllSockets();	// close all UDP/TCP sockets

	net_time = 0.0f;

	// now reconfiguare

	if ( net_multiplayer )
	{
		// don't allocate loopback buffers
		NET_ConfigLoopbackBuffers( false );

		// get localhost IP address
		NET_GetLocalAddress();

		// reopen sockets if in MP mode
		NET_OpenSockets();

		// setup the rcon server sockets
		if ( net_dedicated || CommandLine()->FindParm( "-usercon" ) )
		{
			netadr_t rconAddr = net_local_adr;
			rconAddr.SetPort( net_sockets[NS_SERVER].nPort );
			RCONServer().SetAddress( rconAddr.ToString() );
			RCONServer().CreateSocket();
		}
	}
	else
	{
		// allocate loopback buffers
		NET_ConfigLoopbackBuffers( true );
	}

	Msg( "Network: IP %s, mode %s, dedicated %s, ports %i SV / %i CL\n",
		net_local_adr.ToString(true), net_multiplayer?"MP":"SP", net_dedicated?"Yes":"No",
		net_sockets[NS_SERVER].nPort, net_sockets[NS_CLIENT].nPort );
}

/*
====================
NET_SetDedicated

A single player game will only use the loopback code
====================
*/

void NET_SetDedicated ()
{
	if ( net_noip )
	{
		Warning( "Dedicated not possible with -noip parameter.\n");
		return;
	}

	net_dedicated = true;
}

void NET_ListenSocket( intp sock, bool bListen )
{
	Assert( sock >= 0 && sock < net_sockets.Count() );

	netsocket_t * netsock = &net_sockets[sock];

	if ( netsock->hTCP )
	{
		NET_CloseSocket( netsock->hTCP, sock );
	}

	if ( !NET_IsMultiplayer() || net_notcp )
		return;

	if ( bListen )
	{
		const char * net_interface = ipname.GetString();

		netsock->hTCP = NET_OpenSocket( net_interface, netsock->nPort, true );

		if ( !netsock->hTCP )
		{
			Warning( "NET_ListenSocket failed opening socket %zd, port %i.\n", sock, net_sockets[sock].nPort );
			return;
		}

		struct sockaddr_in	address;

		if (!net_interface || !net_interface[0] || !Q_strcmp(net_interface, "localhost"))
		{
			address.sin_addr.s_addr = INADDR_ANY;
		}
		else
		{
			NET_StringToSockaddr (net_interface, (struct sockaddr *)&address);
		}

		address.sin_family = AF_INET;
		address.sin_port = NET_HostToNetShort((short)( netsock->nPort ));

		int ret;
		VCR_NONPLAYBACKFN( bind( netsock->hTCP, (struct sockaddr *)&address, sizeof(address)), ret, "bind" );
		if ( ret == -1 )
		{
			Warning ("NET_ListenSocket bind failed on socket %zd, port %i: %s.\n",
				netsock->hTCP, netsock->nPort, NET_ErrorString(NET_GetLastError()) );
			return;
		}

		VCR_NONPLAYBACKFN( listen( netsock->hTCP, TCP_MAX_ACCEPTS), ret, "listen" );
		if ( ret == -1 )
		{
			Warning ("NET_ListenSocket listen failed on socket %zd, port %i: %s.\n",
				netsock->hTCP, netsock->nPort, NET_ErrorString(NET_GetLastError()) );
			return;
		}

		netsock->bListening = true;
	}
}

void NET_SetMutiplayer(bool multiplayer)
{
	if ( net_noip && multiplayer )
	{
		Warning( "Multiplayer mode not available with -noip parameter.\n");
		return;
	}

	if ( net_dedicated && !multiplayer )
	{
		Warning( "Singleplayer mode not available on dedicated server.\n");
		return;
	}

	// reconfigure if changed
	if ( net_multiplayer != multiplayer )
	{
		net_multiplayer = multiplayer;
		NET_Config();
	}

	// clear loopback buffer in single player mode
	if ( !multiplayer )
	{
		NET_ClearLoopbackBuffers();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bIsDedicated - 
//-----------------------------------------------------------------------------
void NET_Init( bool bIsDedicated )
{
	if ( CommandLine()->FindParm( "-NoQueuedPacketThread" ) )
		Warning( "Found -NoQueuedPacketThread, so no queued packet thread will be created.\n" );
	else
		g_pQueuedPackedSender->Setup();


	if (CommandLine()->FindParm("-nodns"))
	{
		net_nodns = true;
	}

	if (CommandLine()->FindParm("-usetcp"))
	{
		net_notcp = false;
	}

	if (CommandLine()->FindParm("-nohltv"))
	{
		net_nohltv = true;
	}

	if (CommandLine()->FindParm("-noip"))
	{
		net_noip = true;
	}

	COMPILE_TIME_ASSERT( SVC_LASTMSG < (1<<NETMSG_TYPE_BITS) );
	COMPILE_TIME_ASSERT( MAX_FILE_SIZE < (1<<MAX_FILE_SIZE_BITS) );

	net_time = 0.0f;

	int hPort = CommandLine()->ParmValue( "-port", -1 );
	if ( hPort == -1 )
	{
		hPort = CommandLine()->ParmValue( "+port", -1 ); // check if they used +port by mistake
	}

	if ( hPort != -1 )
	{
		hostport.SetValue( hPort );
	}

	// clear static stuff
	net_sockets.EnsureCount( MAX_SOCKETS );
	net_packets.EnsureCount( MAX_SOCKETS );
	net_splitpackets.EnsureCount( MAX_SOCKETS );

	for ( int i = 0; i < MAX_SOCKETS; ++i )
	{
		s_pLagData[i] = NULL;
		Q_memset( &net_sockets[i], 0, sizeof(netsocket_t) );
	}

	const char *ip = CommandLine()->ParmValue( "-ip" );

	if ( ip ) // if they had a command line option for IP
	{
		ipname.SetValue( ip );  // update the cvar right now, this will get overwritten by "stuffcmds" later
	}

	if ( bIsDedicated )
	{
		// set dedicated MP mode
		NET_SetDedicated();
	}
	else
	{
		// set SP mode
		NET_ConfigLoopbackBuffers( true );
	}
}

/*
====================
NET_Shutdown

====================
*/
void NET_Shutdown ()
{
	for ( auto &ld : s_pLagData )
	{
		NET_ClearLaggedList( &ld );
	}

	g_pQueuedPackedSender->Shutdown();

	net_multiplayer = false;
	net_dedicated = false;

	NET_CloseAllSockets();
	NET_ConfigLoopbackBuffers( false );

	Assert( s_NetChannels.Count() == 0 );
	Assert( s_PendingSockets.Count() == 0);
}

void NET_PrintChannelStatus( INetChannel * chan )
{
	Msg( "NetChannel '%s':\n", chan->GetName() );
	Msg( "- remote IP: %s %s\n", chan->GetAddress(), chan->IsPlayback()?"(Demo)":"" );
	Msg( "- online: %s\n", COM_FormatSeconds( chan->GetTimeConnected() ) );
	Msg( "- reliable: %s\n", chan->HasPendingReliableData()?"pending data":"available" );
	Msg( "- latency: %.1f, loss %.2f\n", chan->GetAvgLatency(FLOW_OUTGOING), chan->GetAvgLoss(FLOW_INCOMING) );
	Msg( "- packets: in %.1f/s, out %.1f/s\n", chan->GetAvgPackets(FLOW_INCOMING), chan->GetAvgPackets(FLOW_OUTGOING) );
	Msg( "- choke: in %.2f, out %.2f\n", chan->GetAvgChoke(FLOW_INCOMING), chan->GetAvgChoke(FLOW_OUTGOING) );
	Msg( "- flow: in %.1f, out %.1f KiB/s\n", chan->GetAvgData(FLOW_INCOMING)/1024.0f, chan->GetAvgData(FLOW_OUTGOING)/1024.0f );
	Msg( "- total: in %.1f, out %.1f MiB\n\n", (float)chan->GetTotalData(FLOW_INCOMING)/(1024*1024), (float)chan->GetTotalData(FLOW_OUTGOING)/(1024*1024) );
}

CON_COMMAND( net_channels, "Shows net channel info" )
{
	intp numChannels = s_NetChannels.Count();

	if ( numChannels == 0 )
	{
		ConMsg( "No active net channels.\n" );
		return;
	}

	AUTO_LOCK( s_NetChannels );
	for ( auto *chan : s_NetChannels )
	{
		NET_PrintChannelStatus( chan );
	}
}

CON_COMMAND( net_start, "Inits multiplayer network sockets" )
{
	net_multiplayer = true;
	NET_Config();
}

CON_COMMAND( net_status, "Shows current network status" )
{
	AUTO_LOCK( s_NetChannels );

	intp numChannels = s_NetChannels.Count();

	ConMsg("Net status for host %s:\n", 
		net_local_adr.ToString(true) );

	ConMsg("- Config: %s, %s, %zd connections\n",
		net_multiplayer?"Multiplayer":"Singleplayer",
		net_dedicated?"dedicated":"listen",
		numChannels	);

	CFmtStrN<128> lan_str;
#ifdef LINUX
	lan_str.sprintf( ", Lan %u", net_sockets[NS_SVLAN].nPort );
#endif

	ConMsg("- Ports: Client %u, Server %u, HLTV %u, Matchmaking %u, Systemlink %u%s\n",
		net_sockets[NS_CLIENT].nPort,
		net_sockets[NS_SERVER].nPort, 
		net_sockets[NS_HLTV].nPort, 
		net_sockets[NS_MATCHMAKING].nPort, 
		net_sockets[NS_SYSTEMLINK].nPort,
		lan_str.Get() );

	if ( numChannels <= 0 )
	{
		return;
	}

	// gather statistics:

	float avgLatencyOut = 0;
	float avgLatencyIn = 0;
	float avgPacketsOut = 0;
	float avgPacketsIn = 0;
	float avgLossOut = 0;
	float avgLossIn = 0;
	float avgDataOut = 0;
	float avgDataIn = 0;

	for ( auto *chan : s_NetChannels )
	{
		avgLatencyOut += chan->GetAvgLatency(FLOW_OUTGOING);
		avgLatencyIn += chan->GetAvgLatency(FLOW_INCOMING);

		avgLossIn += chan->GetAvgLoss(FLOW_INCOMING);
		avgLossOut += chan->GetAvgLoss(FLOW_OUTGOING);

		avgPacketsIn += chan->GetAvgPackets(FLOW_INCOMING);
		avgPacketsOut += chan->GetAvgPackets(FLOW_OUTGOING);
		
		avgDataIn += chan->GetAvgData(FLOW_INCOMING);
		avgDataOut += chan->GetAvgData(FLOW_OUTGOING);
	}

	ConMsg( "- Latency: avg out %.2fs, in %.2fs\n",  avgLatencyOut/numChannels, avgLatencyIn/numChannels );
 	ConMsg( "- Loss:    avg out %.1f, in %.1f\n", avgLossOut/numChannels, avgLossIn/numChannels );
	ConMsg( "- Packets: net total out  %.1f/s, in %.1f/s\n", avgPacketsOut, avgPacketsIn );
	ConMsg( "           per client out %.1f/s, in %.1f/s\n", avgPacketsOut/numChannels, avgPacketsIn/numChannels );
	ConMsg( "- Data:    net total out  %.1f, in %.1f KiB/s\n", avgDataOut/1024.0f, avgDataIn/1024.0f );
	ConMsg( "           per client out %.1f, in %.1f KiB/s\n", (avgDataOut/numChannels)/1024.0f, (avgDataIn/numChannels)/1024.0f );
}
