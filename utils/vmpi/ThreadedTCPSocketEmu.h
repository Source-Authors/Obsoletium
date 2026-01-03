// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SE_UTILS_VMPI_THREADEDTCPSOCKETEMU_H_
#define SE_UTILS_VMPI_THREADEDTCPSOCKETEMU_H_

#include "tcpsocket.h"

// This creates a class that's based on IThreadedTCPSocket, but emulates the old
// ITCPSocket interface.  This is used for stress-testing IThreadedTCPSocket.
ITCPSocket* CreateTCPSocketEmu();

ITCPListenSocket* CreateTCPListenSocketEmu(const unsigned short port,
                                           int nQueueLength = -1);

#endif  // !SE_UTILS_VMPI_THREADEDTCPSOCKETEMU_H_
