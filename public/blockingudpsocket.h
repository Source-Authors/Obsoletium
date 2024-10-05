//========= Copyright Valve Corporation, All rights reserved. ============//
//
//

#ifndef BLOCKINGUDPSOCKET_H
#define BLOCKINGUDPSOCKET_H

#include "tier0/vcrmode.h"
#include "tier1/netadr.h"

class CBlockingUDPSocket
{
public:
	explicit CBlockingUDPSocket();
	virtual ~CBlockingUDPSocket();

	bool WaitForMessage( float timeOutInSeconds );
	unsigned int ReceiveSocketMessage( struct sockaddr_in *packet_from, unsigned char *buf, size_t bufsize );
	bool SendSocketMessage( const struct sockaddr_in& rRecipient, const unsigned char *buf, size_t bufsize );

	bool IsValid() const { return m_Socket != kInvalidSocketHandle; }

protected:
	bool CreateSocket();

	class Impl;
	Impl				*m_pImpl;

	netadr_t			m_cserIP;
	socket_handle		m_Socket;
};

#endif // BLOCKINGUDPSOCKET_H
