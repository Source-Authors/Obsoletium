// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SRC_UTILS_VMPI_IPHELPERS_H_
#define SRC_UTILS_VMPI_IPHELPERS_H_

#include <cstdint>

// Useful for putting the arguments into a printf statement.
#define EXPAND_ADDR(x) (x).ip[0], (x).ip[1], (x).ip[2], (x).ip[3], (x).port

// This is a simple wrapper layer for UDP sockets.
struct IpV4 {
  IpV4();

  IpV4(const uint8_t inputIP[4], const uint16_t inputPort);
  IpV4(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3,
       unsigned short ipPort);

  bool operator==(const IpV4 &o) const noexcept;
  bool operator!=(const IpV4 &o) const noexcept;

  // Setup to send to the local machine on the specified port.
  void SetupLocal(uint16_t inPort);

  uint8_t ip[4];
  uint16_t port;
};

// The chunk walker provides an easy way to copy data out of the chunks as
// though it were a single contiguous chunk of memory.
class CChunkWalker {
 public:
  CChunkWalker(void const *const *pChunks, const ptrdiff_t *pChunkLengths,
               ptrdiff_t nChunks);

  ptrdiff_t GetTotalLength() const;
  void CopyTo(void *pOut, ptrdiff_t nBytes);

 private:
  void const *const *m_pChunks;
  const ptrdiff_t *m_pChunkLengths;
  ptrdiff_t m_nChunks;

  ptrdiff_t m_iCurChunk;
  ptrdiff_t m_iCurChunkPos;

  ptrdiff_t m_TotalLength;
};

// This class makes loop that wait on something look nicer.  ALL loops using
// this class should follow this pattern, or they can wind up with unforeseen
// delays that add a whole lot of lag.
//
// CWaitTimer waitTimer(5.0);
// while ( 1 )
// {
//  do your thing here like Recv() from a socket.
//
//  if ( waitTimer.ShouldKeepWaiting() )
//    Sleep() for some time interval like 5ms so you don't hog the CPU else
//    BREAK HERE
// }
class CWaitTimer {
 public:
  CWaitTimer(double flSeconds);

  bool ShouldKeepWaiting() const;

 private:
  unsigned long m_StartTime;
  unsigned long m_WaitMS;
};

// Helper function to get time in milliseconds.
unsigned long SampleMilliseconds();

struct ISocket {
  // Call this when you're done.
  virtual void Release() = 0;

  // Bind the socket so you can send and receive with it.
  // If you bind to port 0, then the system will select the port for you.
  virtual bool Bind(const IpV4 *pAddr) = 0;
  virtual bool BindToAny(const unsigned short port) = 0;

  // Broadcast some data.
  virtual bool Broadcast(const void *pData, const int len,
                         const unsigned short port) = 0;

  // Send a packet.
  virtual bool SendTo(const IpV4 *pAddr, const void *pData, const int len) = 0;
  virtual bool SendChunksTo(const IpV4 *pAddr, void const *const *pChunks,
                            const int *pChunkLengths, int nChunks) = 0;

  // Receive a packet. Returns the length received or -1 if no data came in.
  // If pFrom is set, then it is filled in with the sender's IP address.
  virtual int RecvFrom(void *pData, int maxDataLen, IpV4 *pFrom) = 0;

  // How long has it been since we successfully received a packet?
  virtual double GetRecvTimeout() = 0;
};

// Create a connectionless socket that you can send packets out of.
ISocket *CreateIPSocket();

// This sets up the socket to receive multicast data on the specified group.
// By default, localInterface is INADDR_ANY, but if you want to specify a
// specific interface the data should come in through, you can.
ISocket *CreateMulticastListenSocket(const IpV4 &addr,
                                     const IpV4 &localInterface = IpV4());

// Setup a IpV4 from the string.  The string can be a dotted IP address or
// a hostname, and it can be followed by a colon and a port number like
// "1.2.3.4:3443" or "myhostname.valvesoftware.com:2342".
//
// NOTE: if the string does not contain a port, then pOut->port will be left
// alone.
bool ConvertStringToIPAddr(const char *pStr, IpV4 *pOut);

// Do a DNS lookup on the IP.  You can optionally get a service name back too.
bool ConvertIPAddrToString(const IpV4 *pIn, char *pOut, int outLen);

void SockAddrToIPAddr(const struct sockaddr_in *pIn, IpV4 *pOut);
void IPAddrToSockAddr(const IpV4 *pIn, struct sockaddr_in *pOut);

#endif  // !SRC_UTILS_VMPI_IPHELPERS_H_
