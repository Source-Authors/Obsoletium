// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SE_COMMON_INETAPI_H_
#define SE_COMMON_INETAPI_H_

#include "tier0/platform.h"
#include "tier0/vcrmode.h"
#include "tier1/netadr.h"

//-----------------------------------------------------------------------------
// Purpose: Internal winsock helpers for launcher
//-----------------------------------------------------------------------------
abstract_class INetAPI
{
public:
	// Convert a netadr_t to sockaddr
	virtual void		NetAdrToSockAddr( netadr_t *a, struct sockaddr *s ) = 0;
	// Convert a sockaddr to netadr_t
	virtual void		SockAddrToNetAdr( struct sockaddr *s, netadr_t *a ) = 0;

	// Convert a netadr_t to a string
	virtual char		*AdrToString( netadr_t *a ) = 0;
	// Convert a string address to a netadr_t, doing DNS if needed
	virtual bool		StringToAdr( const char *s, netadr_t *a ) = 0;
	// Look up IP address for socket
	virtual void		GetSocketAddress( socket_handle socket, netadr_t *a ) = 0;

	virtual bool		CompareAdr( netadr_t *a, netadr_t *b ) =0;

	// return the IP of the local host
	virtual void	GetLocalIP(netadr_t *a)=0;

};

extern INetAPI *net;

#endif  // !SE_COMMON_INETAPI_H_
