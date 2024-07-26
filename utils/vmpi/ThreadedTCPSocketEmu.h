// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SRC_UTILS_VMPI_THREADEDTCPSOCKETEMU_H_
#define SRC_UTILS_VMPI_THREADEDTCPSOCKETEMU_H_

#include "tcpsocket.h"

// This creates a class that's based on IThreadedTCPSocket, but emulates the old
// ITCPSocket interface.  This is used for stress-testing IThreadedTCPSocket.
ITCPSocket* CreateTCPSocketEmu();

ITCPListenSocket* CreateTCPListenSocketEmu(const unsigned short port,
                                           int nQueueLength = -1);

#endif  // !SRC_UTILS_VMPI_THREADEDTCPSOCKETEMU_H_
