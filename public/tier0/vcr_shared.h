// Copyright Valve Corporation, All rights reserved.

#ifndef SE_PUBLIC_TIER0_VCR_SHARED_H_
#define SE_PUBLIC_TIER0_VCR_SHARED_H_

constexpr inline int VCRFILE_VERSION{2};


// Identifiers for the things we record. When playing back, these things should
// be asked for in the exact same order (otherwise, the engine isn't making all
// the calls in the same order).
enum VCREvent
{
	VCREvent_Sys_FloatTime=0,
	VCREvent_recvfrom,
	VCREvent_SyncToken,
	VCREvent_GetCursorPos,
	VCREvent_SetCursorPos,
	VCREvent_ScreenToClient,
	VCREvent_Cmd_Exec,
	VCREvent_CmdLine,
	VCREvent_RegOpenKeyEx,
	VCREvent_RegSetValueEx,
	VCREvent_RegQueryValueEx,
	VCREvent_RegCreateKeyEx,
	VCREvent_RegCloseKey,
	VCREvent_PeekMessage,
	VCREvent_GameMsg,
	VCREvent_GetNumberOfConsoleInputEvents,
	VCREvent_ReadConsoleInput,
	VCREvent_GetKeyState,
	VCREvent_recv,
	VCREvent_send,
	VCREvent_Generic,
	VCREvent_CreateThread,
	VCREvent_WaitForSingleObject,
	VCREvent_EnterCriticalSection,
	VCREvent_Time,
	VCREvent_LocalTime,
	VCREvent_GenericString,
	VCREvent_NUMEVENTS
};


#endif  // !SE_PUBLIC_TIER0_VCR_SHARED_H_
