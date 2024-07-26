// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SRC_UTILS_COMMON_VMPI_TOOLS_SHARED_H_
#define SRC_UTILS_COMMON_VMPI_TOOLS_SHARED_H_

// Subpacket IDs.
enum VmpiSubpacketId {
  // qdir directories.
  VMPI_SUBPACKETID_DIRECTORIES = 0,
  // MySQL database info.
  VMPI_SUBPACKETID_DBINFO = 1,
  // A worker saying it crashed.
  VMPI_SUBPACKETID_CRASH = 3,
  // Filesystem multicast address.
  VMPI_SUBPACKETID_MULTICAST_ADDR = 4
};

// Send/receive the qdir info.
void SendQDirInfo();
void RecvQDirInfo();

void SendDBInfo(const class CDBInfo *pInfo, unsigned long jobPrimaryID);
void RecvDBInfo(class CDBInfo *pInfo, unsigned long *pJobPrimaryID);

void SendMulticastIP(const struct IpV4 *pAddr);
void RecvMulticastIP(struct IpV4 *pAddr);

void VMPI_HandleCrash(const char *pMessage, void *pvExceptionInfo,
                      bool bAssert);

// Call this from an exception handler (set by SetUnhandledExceptionHandler).
// uCode			= ExceptionInfo->ExceptionRecord->ExceptionCode.
// pvExceptionInfo	= ExceptionInfo
void VMPI_ExceptionFilter(unsigned long uCode, void *pvExceptionInfo);

void HandleMPIDisconnect(int procID, const char *pReason);

#endif  // !SRC_UTILS_COMMON_VMPI_TOOLS_SHARED_H_
