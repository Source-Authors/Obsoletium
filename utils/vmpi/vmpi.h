// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SRC_UTILS_VMPI_VMPI_H_
#define SRC_UTILS_VMPI_VMPI_H_

#include "vmpi_defs.h"
#include "messbuf.h"
#include "iphelpers.h"

// These are called to handle incoming messages.  Return true if you handled the
// message and false otherwise.  NOTE: the first byte in each message is the
// packet ID.
using VMPIDispatchFn = bool (*)(MessageBuffer *pBuf, int iSource,
                                int iPacketID);

using VMPI_Disconnect_Handler = void (*)(int procID, const char *pReason);

// Which machine is the master.
constexpr inline int VMPI_MASTER_ID{0};

constexpr inline int VMPI_SEND_TO_ALL{-2};

// If this is set as the destination for a packet, it is sent to all
// workers, and also to new workers that connect.
constexpr inline int VMPI_PERSISTENT{-3};

constexpr inline int MAX_VMPI_PACKET_IDS{32};

constexpr inline unsigned long VMPI_TIMEOUT_INFINITE{0xFFFFFFFF};

// Instantiate one of these to register a dispatch.
struct CDispatchReg {
  CDispatchReg(int iPacketID, VMPIDispatchFn fn);
};

// Hidden in SDK mode.
constexpr inline int VMPI_PARAM_SDK_HIDDEN{0x0001};

#define VMPI_PARAM(paramName, paramFlags, helpText) paramName,
enum class EVMPICmdLineParam {
  k_eVMPICmdLineParam_FirstParam = 0,
  k_eVMPICmdLineParam_VMPIParam,

#include "vmpi_parameters.h"

  k_eVMPICmdLineParam_LastParam
};
#undef VMPI_PARAM

// Shared by all the tools.
extern bool g_bUseMPI;
extern bool g_bMPIMaster;  // Set to true if we're the master in a VMPI session.
extern int g_iVMPIVerboseLevel;  // Higher numbers make it spit out more data.

extern bool g_bMPI_Stats;            // Send stats to the MySQL database?
extern bool g_bMPI_StatsTextOutput;  // Send text output in the stats?

// These can be watched or modified to check bandwidth statistics.
extern int g_nBytesSent;
extern int g_nMessagesSent;
extern int g_nBytesReceived;
extern int g_nMessagesReceived;

extern int g_nMulticastBytesSent;
extern int g_nMulticastBytesReceived;

extern int g_nMaxWorkerCount;

enum class VMPIRunMode {
  VMPI_RUN_NETWORKED,
  VMPI_RUN_LOCAL  // Just make a local process and have it do the work.
};

enum class VMPIFileSystemMode {
  VMPI_FILESYSTEM_MULTICAST,  // Multicast out, find workers, have them do work.
  VMPI_FILESYSTEM_BROADCAST,  // Broadcast out, find workers, have them do work.
  VMPI_FILESYSTEM_TCP         // TCP filesystem.
};

// If this precedes the dependency filename, then it will transfer all the files
// in the specified directory.
constexpr inline char VMPI_DEPENDENCY_DIRECTORY_TOKEN{'*'};

// It's good to specify a disconnect handler here immediately. If you don't have
// a handler and the master disconnects, you'll lockup forever inside a dispatch
// loop because you never handled the master disconnecting.
//
// Note: runMode is only relevant for the VMPI master. The worker always
// connects to the master the same way.
bool VMPI_Init(int &argc, char **&argv, const char *pDependencyFilename,
               VMPI_Disconnect_Handler handler = NULL,
               VMPIRunMode runMode =
                   VMPIRunMode::VMPI_RUN_NETWORKED,  // Networked or local?,
               bool bConnectingAsService = false);

// Used when hosting a patch.
void VMPI_Init_PatchMaster(int argc, char **argv);

void VMPI_Finalize();

VMPIRunMode VMPI_GetRunMode();
VMPIFileSystemMode VMPI_GetFileSystemMode();

// Note: this number can change on the master.
int VMPI_GetCurrentNumberOfConnections();

// Dispatch messages until it gets one with the specified packet ID.
// If subPacketID is not set to -1, then the second byte must match that as
// well.
//
// Note: this WILL dispatch packets with matching packet IDs and give them a
// chance to handle packets first.
//
// If bWait is true, then this function either succeeds or Error() is called. If
// it's false, then if the first available message is handled by a dispatch,
// this function returns false.
bool VMPI_DispatchUntil(MessageBuffer *pBuf, int *pSource, int packetID,
                        int subPacketID = -1, bool bWait = true);

// This waits for the next message and dispatches it.
// You can specify a timeout in milliseconds. If the timeout expires, the
// function returns false.
bool VMPI_DispatchNextMessage(unsigned long timeout = VMPI_TIMEOUT_INFINITE);

// This should be called periodically in modal loops that don't call other VMPI
// functions. This will check for disconnected sockets and call disconnect
// handlers so the app can error out if it loses all of its connections.
//
// This can be used in place of a Sleep() call by specifying a timeout value.
void VMPI_HandleSocketErrors(unsigned long timeout = 0);

enum VMPISendFlags { k_eVMPISendFlags_GroupPackets = 0x0001 };

// Use these to send data to one of the machines.
// If iDest is VMPI_SEND_TO_ALL, then the message goes to all the machines.
// Flags is a combination of the VMPISendFlags enums.
bool VMPI_SendData(void *pData, ptrdiff_t nBytes, int iDest,
                   int fVMPISendFlags = 0);
bool VMPI_SendChunks(void const *const *pChunks, const ptrdiff_t *pChunkLengths,
                     ptrdiff_t nChunks, int iDest, int fVMPISendFlags = 0);
bool VMPI_Send2Chunks(const void *pChunk1, ptrdiff_t chunk1Len,
                      const void *pChunk2, ptrdiff_t chunk2Len, int iDest,
                      int fVMPISendFlags = 0);  // for convenience..
bool VMPI_Send3Chunks(const void *pChunk1, ptrdiff_t chunk1Len,
                      const void *pChunk2, ptrdiff_t chunk2Len,
                      const void *pChunk3, ptrdiff_t chunk3Len, int iDest,
                      int fVMPISendFlags = 0);

// Flush any groups that were queued with k_eVMPISendFlags_GroupPackets.
// If msInterval is > 0, then it will check a timer and only flush that often
// (so you can call this a lot, and have it check).
void VMPI_FlushGroupedPackets(unsigned long msInterval = 0);

// This registers a function that gets called when a connection is terminated
// ungracefully.
void VMPI_AddDisconnectHandler(VMPI_Disconnect_Handler handler);

// Returns false if the process has disconnected ungracefully (disconnect
// handlers would have been called for it too).
bool VMPI_IsProcConnected(int procID);

// Returns true if the process is just a service (in which case it should only
// get file IO traffic).
bool VMPI_IsProcAService(int procID);

// Simple wrapper for Sleep() so people can avoid including windows.h
void VMPI_Sleep(unsigned long ms);

// VMPI sends machine names around first thing.
const char *VMPI_GetLocalMachineName();
const char *VMPI_GetMachineName(int iProc);
bool VMPI_HasMachineNameBeenSet(int iProc);

// Returns 0xFFFFFFFF if the ID hasn't been set.
unsigned long VMPI_GetJobWorkerID(int iProc);
void VMPI_SetJobWorkerID(int iProc, unsigned long jobWorkerID);

// Search a command line to find arguments. Looks for pName, and if it finds it,
// returns the argument following it. If pName is the last argument, it returns
// pDefault. If it doesn't find pName, returns NULL.
const char *VMPI_FindArg(int argc, char **argv, const char *pName,
                         const char *pDefault = "");

// (Threadsafe) get and set the current stage. This info winds up in the VMPI
// database.
void VMPI_GetCurrentStage(char *pOut, int strLen);
void VMPI_SetCurrentStage(const char *pCurStage);

// VMPI is always broadcasting this job in the background.  This changes the
// password to 'debugworker' and allows more workers in.  This can be used if
// workers are dying on certain work units.  Then a programmer can run
// vmpi_service with -superdebug and debug the whole thing.
void VMPI_InviteDebugWorkers();

bool VMPI_IsSDKMode();

// Lookup a command line parameter string.
const char *VMPI_GetParamString(EVMPICmdLineParam eParam);

int VMPI_GetParamFlags(EVMPICmdLineParam eParam);

const char *VMPI_GetParamHelpString(EVMPICmdLineParam eParam);

// Returns true if the specified parameter is on the command line.
bool VMPI_IsParamUsed(EVMPICmdLineParam eParam);

// Can be called from error handlers and if -mpi_Restart is used, it'll
// automatically restart the process.
bool VMPI_HandleAutoRestart();

#endif  // !SRC_UTILS_VMPI_VMPI_H_
