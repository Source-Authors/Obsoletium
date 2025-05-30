//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "ThreadedTCPSocketEmu.h"

#include "winlite.h"
#include "tcpsocket.h"
#include "IThreadedTCPSocket.h"
#include "threadhelpers.h"



// ---------------------------------------------------------------------------------------- //
// CThreadedTCPSocketEmu. This uses IThreadedTCPSocket to emulate the polling-type interface
// in ITCPSocket.
// ---------------------------------------------------------------------------------------- //

// This class uses the IThreadedTCPSocket interface to emulate the old ITCPSocket.
class CThreadedTCPSocketEmu : public ITCPSocket, public ITCPSocketHandler, public IHandlerCreator
{
public:
	
	CThreadedTCPSocketEmu()
	{
		m_pSocket = NULL;
		m_LocalPort = 0xFFFF;
		m_pConnectSocket = NULL;
		m_RecvPacketsEvent.Init( false, false );
		m_bError = false;
	}

	virtual ~CThreadedTCPSocketEmu()
	{
		Term();
	}

	void Init( IThreadedTCPSocket *pSocket ) override
	{
		m_pSocket = pSocket;
	}

	void Term()
	{
		if ( m_pSocket )
		{
			m_pSocket->Release();
			m_pSocket = NULL;
		}
		
		if ( m_pConnectSocket )
		{
			m_pConnectSocket->Release();
			m_pConnectSocket = NULL;
		}
	}


// ITCPSocketHandler implementation.
private:


	void OnPacketReceived( CTCPPacket *pPacket ) override
	{
		CCriticalSectionLock csLock( &m_RecvPacketsCS );
		csLock.Lock();

			m_RecvPackets.AddToTail( pPacket );
			m_RecvPacketsEvent.SetEvent();
	}


	void OnError( int errorCode, const char *pErrorString ) override
	{
		CCriticalSectionLock csLock( &m_ErrorStringCS );
		csLock.Lock();

		m_ErrorString.CopyArray( pErrorString, strlen( pErrorString ) + 1 );
		m_ErrorCode = errorCode;
		m_bError = true;
	}


// IHandlerCreator implementation.
public:
	
	// This is used for connecting.
	ITCPSocketHandler* CreateNewHandler() override
	{
		return this;
	}


// ITCPSocket implementation.
public:

	void	Release() override
	{
		delete this;
	}

	bool	BindToAny( const unsigned short port ) override
	{
		m_LocalPort = port;
		return true;
	}

	bool	BeginConnect( const IpV4 &addr ) override
	{
		// They should have "bound" to a port before trying to connect.
		Assert( m_LocalPort != 0xFFFF );
		
		if ( m_pConnectSocket )
			m_pConnectSocket->Release();

		m_pConnectSocket = ThreadedTCP_CreateConnector( 
			addr,
			IpV4( 0, 0, 0, 0, m_LocalPort ),
			this );

		return m_pConnectSocket != 0;
	}

	bool UpdateConnect() override
	{
		Assert( !m_pSocket );
		if ( !m_pConnectSocket )
			return false;

		if ( m_pConnectSocket->Update( &m_pSocket ) )
		{
			if ( m_pSocket )
			{
				// Ok, we're connected now.
				m_pConnectSocket->Release();
				m_pConnectSocket = NULL;
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			Assert( false );
			m_pConnectSocket->Release();
			m_pConnectSocket = NULL;
			return false;
		}
	}

	bool	IsConnected() override
	{
		if ( m_bError )
		{
			Term();
			return false;
		}
		else
		{
			return m_pSocket != NULL;
		}
	}

	void	GetDisconnectReason( CUtlVector<char> &reason ) override
	{
		CCriticalSectionLock csLock( &m_ErrorStringCS );
		csLock.Lock();

		reason = m_ErrorString;
	}

	bool	Send( const void *pData, ptrdiff_t size ) override
	{
		Assert( m_pSocket );
		if ( !m_pSocket )
			return false;

		return m_pSocket->Send( pData, size );
	}

	bool	SendChunks( void const * const *pChunks, const ptrdiff_t *pChunkLengths, ptrdiff_t nChunks ) override
	{
		Assert( m_pSocket );
		if ( !m_pSocket || !m_pSocket->IsValid() )
			return false;

		return m_pSocket->SendChunks( pChunks, pChunkLengths, nChunks );
	}

	bool	Recv( CUtlVector<unsigned char> &data, double flTimeout ) override
	{
		// Use our m_RecvPacketsEvent event to determine if there is data to receive yet.
		DWORD nMilliseconds = (DWORD)( flTimeout * 1000.0f );
		DWORD ret = WaitForSingleObject( m_RecvPacketsEvent.GetEventHandle(), nMilliseconds );
		if ( ret == WAIT_OBJECT_0 )
		{
			// Ok, there's a packet.
			CCriticalSectionLock csLock( &m_RecvPacketsCS );
			csLock.Lock();
		
			Assert( m_RecvPackets.Count() > 0 );
			
			int iHead = m_RecvPackets.Head();
			CTCPPacket *pPacket = m_RecvPackets[ iHead ];
			
			data.CopyArray( (const unsigned char*)pPacket->GetData(), pPacket->GetLen() );
			
			pPacket->Release();
			m_RecvPackets.Remove( iHead );
				
			// Re-set the event if there are more packets left to receive.
			if ( m_RecvPackets.Count() > 0 )
			{
				m_RecvPacketsEvent.SetEvent();
			}

			return true;
		}
		else
		{
			return false;
		}
	}


private:
	
	IThreadedTCPSocket *m_pSocket;
	
	unsigned short m_LocalPort;	// The port we bind to when we want to connect.
	ITCPConnectSocket *m_pConnectSocket;


	// All the received data is stored in here.
	CEvent m_RecvPacketsEvent;
	CCriticalSection m_RecvPacketsCS;
	CUtlLinkedList<CTCPPacket*, int> m_RecvPackets;

	CCriticalSection m_ErrorStringCS;
	CUtlVector<char> m_ErrorString;
	int m_ErrorCode;
	bool m_bError; // Set to true when there's an error. Next chance we get in the main thread, we'll close the socket.
};


ITCPSocket* CreateTCPSocketEmu()
{
	return new CThreadedTCPSocketEmu;
}


// ---------------------------------------------------------------------------------------- //
// CThreadedTCPListenSocketEmu implementation.
// ---------------------------------------------------------------------------------------- //

class CThreadedTCPListenSocketEmu : public ITCPListenSocket, public IHandlerCreator
{
public:
	CThreadedTCPListenSocketEmu()
	{
		m_pListener = NULL;
		m_pLastCreatedSocket = NULL;
	}

	virtual ~CThreadedTCPListenSocketEmu()
	{
		if ( m_pListener )
			m_pListener->Release();
	}

	bool StartListening( const unsigned short port, int nQueueLength )
	{
		m_pListener = ThreadedTCP_CreateListener( 
			this,
			port,
			nQueueLength );

		return m_pListener != 0;
	}


// ITCPListenSocket implementation.
private:

	virtual void Release()
	{
		delete this;
	}

	virtual ITCPSocket*	UpdateListen( IpV4 *pAddr )
	{
		if ( !m_pListener )
			return NULL;

		IThreadedTCPSocket *pSocket;
		if ( m_pListener->Update( &pSocket ) && pSocket )
		{
			*pAddr = pSocket->GetRemoteAddr();
			
			// This is pretty hacky, but this stuff is just around for test code.
			CThreadedTCPSocketEmu *pLast = m_pLastCreatedSocket;
			pLast->Init( pSocket );
			m_pLastCreatedSocket = NULL;
			return pLast;
		}
		else
		{
			return NULL;
		}
	}


// IHandlerCreator implementation.
private:

	virtual ITCPSocketHandler* CreateNewHandler()
	{
		m_pLastCreatedSocket = new CThreadedTCPSocketEmu;
		return m_pLastCreatedSocket;
	}


private:

	ITCPConnectSocket *m_pListener;
	CThreadedTCPSocketEmu *m_pLastCreatedSocket;
};

ITCPListenSocket* CreateTCPListenSocketEmu( const unsigned short port, int nQueueLength )
{
	CThreadedTCPListenSocketEmu *pSocket = new CThreadedTCPListenSocketEmu;
	if ( pSocket->StartListening( port, nQueueLength ) )
	{
		return pSocket;
	}
	else
	{
		delete pSocket;
		return NULL;
	}	
}

