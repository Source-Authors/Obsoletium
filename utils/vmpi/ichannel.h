// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SRC_UTILS_VMPI_ICHANNEL_H_
#define SRC_UTILS_VMPI_ICHANNEL_H_

#include "tier1/utlvector.h"

struct IChannel {
  // Note: this also releases any channels contained inside. So if you make a
  // reliable channel that contains an unreliable channel and release the
  // reliable one, it will automatically release the unreliable one it contains.
  virtual void Release() = 0;

  // Send data to the destination.
  virtual bool Send(const void *pData, ptrdiff_t len) = 0;

  // This version puts all the chunks into one packet and ships it off.
  virtual bool SendChunks(void const *const *pChunks,
                          const ptrdiff_t *pChunkLengths,
                          ptrdiff_t nChunks) = 0;

  // Check for any packets coming in from the destination.
  // Returns false if no packet was received.
  //
  // flTimeout can be used to make it wait for data.
  //
  // Note: this is most efficient if you keep the buffer around between calls so
  // it only reallocates it when it needs more space.
  virtual bool Recv(CUtlVector<unsigned char> &data, double flTimeout = 0) = 0;

  // Returns false if the connection has been broken.
  virtual bool IsConnected() = 0;

  // If IsConnected returns false, you can call this to find out why the socket
  // got disconnected.
  virtual void GetDisconnectReason(CUtlVector<char> &reason) = 0;
};

#endif  // !SRC_UTILS_VMPI_ICHANNEL_H_
