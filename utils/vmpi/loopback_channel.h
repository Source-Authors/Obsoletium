// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SRC_UTILS_VMPI_LOOPBACK_CHANNEL_H_
#define SRC_UTILS_VMPI_LOOPBACK_CHANNEL_H_

#include "ichannel.h"

// Loopback sockets receive the same data they send.
IChannel* CreateLoopbackChannel();

#endif  // !SRC_UTILS_VMPI_LOOPBACK_CHANNEL_H_
