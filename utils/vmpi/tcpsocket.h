// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SRC_UTILS_VMPI_ITCPSOCKET_H_
#define SRC_UTILS_VMPI_ITCPSOCKET_H_

#include "ichannel.h"
#include "iphelpers.h"

class ITCPSocket : public IChannel {
 public:
  virtual void Release() = 0;

  // Bind to the specified port on any address this host has. Note that the
  // address the socket winds up with (and getsockname() returns) isn't decided
  // until you send a packet.
  virtual bool BindToAny(const unsigned short port) = 0;

  // Use these to connect to a remote listening socket without blocking.
  // Call BeginConnect, then call UpdateConnect until it returns true.
  virtual bool BeginConnect(const IpV4 &addr) = 0;
  virtual bool UpdateConnect() = 0;

  // Connection state.
  virtual bool IsConnected() = 0;

  // If IsConnected returns false, you can call this to find out why the socket
  // got disconnected.
  virtual void GetDisconnectReason(CUtlVector<char> &reason) = 0;

  // Send data. Returns true if successful.
  //
  // Note: TCP likes to clump your packets together in one, so the data multiple
  // send() calls will get concatenated and returned in one recv() call.
  // ITCPSocket FIXES this behavior so your recv() calls match your send()
  // calls.
  //
  virtual bool Send(const void *pData, ptrdiff_t size) = 0;

  // Receive data. Returns the number of bytes received.
  // This will wait as long as flTimeout for something to come in.
  // Returns false if no data was waiting.
  virtual bool Recv(CUtlVector<unsigned char> &data, double flTimeout = 0) = 0;
};

// Use these to get incoming connections.
class ITCPListenSocket {
 public:
  // Call this to stop listening for connections and delete the object.
  virtual void Release() = 0;

  // Keep calling this as long as you want to wait for connections.
  // pAddr is set to the remote process's address.
  virtual ITCPSocket *UpdateListen(IpV4 *pAddr) = 0;
};

// Use these to create the interfaces.
ITCPSocket *CreateTCPSocket();

// Create a socket to listen with. nQueueLength specifies how many connections
// to enqueue. When the queue runs out, connections can take a little longer to
// make.
ITCPListenSocket *CreateTCPListenSocket(const unsigned short port,
                                        int nQueueLength = -1);

// By default, timeouts are on. It's helpful to turn them off during debugging.
void TCPSocket_EnableTimeout(bool bEnable);

#endif  // !SRC_UTILS_VMPI_ITCPSOCKET_H_
