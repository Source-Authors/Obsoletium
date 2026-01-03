// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SE_UTILS_VMPI_LOOPBACK_CHANNEL_H_
#define SE_UTILS_VMPI_LOOPBACK_CHANNEL_H_

#include "ichannel.h"

// Loopback sockets receive the same data they send.
IChannel* CreateLoopbackChannel();

#endif  // !SE_UTILS_VMPI_LOOPBACK_CHANNEL_H_
