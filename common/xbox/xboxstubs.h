//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Win32 replacements for XBox.
//
//=============================================================================

#if !defined( XBOXSTUBS_H ) && !defined( _X360 )
#define XBOXSTUBS_H

#include "tier0/platform.h"

//  Content creation/open flags
#define XCONTENTFLAG_NONE                           0x00
#define XCONTENTFLAG_CREATENEW                      0x00
#define XCONTENTFLAG_CREATEALWAYS                   0x00
#define XCONTENTFLAG_OPENEXISTING                   0x00
#define XCONTENTFLAG_OPENALWAYS                     0x00
#define XCONTENTFLAG_TRUNCATEEXISTING               0x00

//  Content attributes
#define XCONTENTFLAG_NOPROFILE_TRANSFER             0x00
#define XCONTENTFLAG_NODEVICE_TRANSFER              0x00
#define XCONTENTFLAG_STRONG_SIGNED                  0x00
#define XCONTENTFLAG_ALLOWPROFILE_TRANSFER          0x00
#define XCONTENTFLAG_MOVEONLY_TRANSFER              0x00

// Console device ports
#define XDEVICE_PORT0               0
#define XDEVICE_PORT1               1
#define XDEVICE_PORT2               2
#define XDEVICE_PORT3               3
#define XUSER_MAX_COUNT				4
#define XUSER_INDEX_NONE            0x000000FE

#define XBX_CLR_DEFAULT				0xFF000000
#define XBX_CLR_WARNING				0x0000FFFF
#define XBX_CLR_ERROR				0x000000FF

#define XBOX_MINBORDERSAFE			0
#define XBOX_MAXBORDERSAFE			0

using xKey_t = enum
{
	XK_NULL,
	XK_BUTTON_UP,
	XK_BUTTON_DOWN,
	XK_BUTTON_LEFT,
	XK_BUTTON_RIGHT,
	XK_BUTTON_START,
	XK_BUTTON_BACK,
	XK_BUTTON_STICK1,
	XK_BUTTON_STICK2,
	XK_BUTTON_A,
	XK_BUTTON_B,
	XK_BUTTON_X,
	XK_BUTTON_Y,
	XK_BUTTON_LEFT_SHOULDER,
	XK_BUTTON_RIGHT_SHOULDER,
	XK_BUTTON_LTRIGGER,
	XK_BUTTON_RTRIGGER,
	XK_STICK1_UP,
	XK_STICK1_DOWN,
	XK_STICK1_LEFT,
	XK_STICK1_RIGHT,
	XK_STICK2_UP,
	XK_STICK2_DOWN,
	XK_STICK2_LEFT,
	XK_STICK2_RIGHT,
	XK_MAX_KEYS,
};

//typedef enum
//{
//	XVRB_NONE,		// off
//	XVRB_ERROR,		// fatal error
//	XVRB_ALWAYS,	// no matter what
//	XVRB_WARNING,	// non-fatal warnings
//	XVRB_STATUS,	// status reports
//	XVRB_ALL,
//} xverbose_e;

using BYTE = unsigned char; //-V677
using WORD = unsigned short; //-V677
#ifndef POSIX
using DWORD = unsigned long; //-V677
using HANDLE = void *; //-V677
using ULONGLONG = unsigned long long; //-V677
#endif

#ifdef POSIX
typedef DWORD COLORREF;
#endif

// typedef struct {
// 	IN_ADDR     ina;                            // IP address (zero if not static/DHCP)
// 	IN_ADDR     inaOnline;                      // Online IP address (zero if not online)
// 	WORD        wPortOnline;                    // Online port
// 	BYTE        abEnet[6];                      // Ethernet MAC address
// 	BYTE        abOnline[20];                   // Online identification
// } XNADDR;

using XNADDR = int;
using XUID = uint64;

using XNKID = struct {
	BYTE        ab[8];                          // xbox to xbox key identifier
};

using XNKEY = struct {
	BYTE        ab[16];                         // xbox to xbox key exchange key
};

using XSESSION_INFO = struct _XSESSION_INFO
{
	XNKID sessionID;                // 8 bytes
	XNADDR hostAddress;             // 36 bytes
	XNKEY keyExchangeKey;           // 16 bytes
};
using PXSESSION_INFO = XSESSION_INFO*;

using XUSER_DATA = struct _XUSER_DATA
{
	BYTE                                type;

	union
	{
		int                            nData;     // XUSER_DATA_TYPE_INT32
		int64                        i64Data;   // XUSER_DATA_TYPE_INT64
		double                          dblData;   // XUSER_DATA_TYPE_DOUBLE
		struct                                     // XUSER_DATA_TYPE_UNICODE
		{
			uint                       cbData;    // Includes null-terminator
			char *                      pwszData;
		} string;
		float                           fData;     // XUSER_DATA_TYPE_FLOAT
		struct                                     // XUSER_DATA_TYPE_BINARY
		{
			uint                       cbData;
			char *                       pbData;
		} binary;
	};
};
using PXUSER_DATA = XUSER_DATA*;

using XUSER_PROPERTY = struct _XUSER_PROPERTY
{
	DWORD                               dwPropertyId;
	XUSER_DATA                          value;
};
using PXUSER_PROPERTY = XUSER_PROPERTY*;

using XUSER_CONTEXT = struct _XUSER_CONTEXT
{
	DWORD                               dwContextId;
	DWORD                               dwValue;
};
using PXUSER_CONTEXT = XUSER_CONTEXT*;

using XSESSION_SEARCHRESULT = struct _XSESSION_SEARCHRESULT
{
	XSESSION_INFO   info;
	DWORD           dwOpenPublicSlots;
	DWORD           dwOpenPrivateSlots;
	DWORD           dwFilledPublicSlots;
	DWORD           dwFilledPrivateSlots;
	DWORD           cProperties;
	DWORD           cContexts;
	PXUSER_PROPERTY pProperties;
	PXUSER_CONTEXT  pContexts;
};
using PXSESSION_SEARCHRESULT = XSESSION_SEARCHRESULT*;

using XSESSION_SEARCHRESULT_HEADER = struct _XSESSION_SEARCHRESULT_HEADER
{
	DWORD dwSearchResults;
	XSESSION_SEARCHRESULT *pResults;
};
using PXSESSION_SEARCHRESULT_HEADER = XSESSION_SEARCHRESULT_HEADER*;

using XSESSION_REGISTRANT = struct _XSESSION_REGISTRANT
{
	uint64 qwMachineID;
	DWORD bTrustworthiness;
	DWORD bNumUsers;
	XUID *rgUsers;

};

using XSESSION_REGISTRATION_RESULTS = struct _XSESSION_REGISTRATION_RESULTS
{
	DWORD wNumRegistrants;
	XSESSION_REGISTRANT *rgRegistrants;
};
using PXSESSION_REGISTRATION_RESULTS = XSESSION_REGISTRATION_RESULTS*;

using XNQOSINFO = struct {
	BYTE        bFlags;                         
	BYTE        bReserved;                    
	WORD        cProbesXmit;                   
	WORD        cProbesRecv;                   
	WORD        cbData;                        
	BYTE *      pbData;                        
	WORD        wRttMinInMsecs;                
	WORD        wRttMedInMsecs;                
	DWORD       dwUpBitsPerSec;                
	DWORD       dwDnBitsPerSec;                
};

using XNQOS = struct {
	uint        cxnqos;                        
	uint        cxnqosPending;                 
	XNQOSINFO   axnqosinfo[1];                 
};

#define XSESSION_CREATE_HOST				0
#define XUSER_DATA_TYPE_INT32				0
#define XSESSION_CREATE_USES_ARBITRATION	0
#define XNET_QOS_LISTEN_ENABLE				0
#define XNET_QOS_LISTEN_DISABLE				0
#define XNET_QOS_LISTEN_SET_DATA			0

FORCEINLINE void			XBX_ProcessEvents() {}
FORCEINLINE unsigned int	XBX_GetSystemTime() { return 0; }
FORCEINLINE	int				XBX_GetPrimaryUserId() { return 0; }
FORCEINLINE	void			XBX_SetPrimaryUserId( DWORD ) {}
FORCEINLINE	DWORD			XBX_GetStorageDeviceId() { return 0; }
FORCEINLINE	void			XBX_SetStorageDeviceId( DWORD ) {}
FORCEINLINE const char		*XBX_GetLanguageString() { return ""; }
FORCEINLINE bool			XBX_IsLocalized() { return false; }

#define XCONTENT_MAX_DISPLAYNAME_LENGTH	128
#define XCONTENT_MAX_FILENAME_LENGTH	42

#define XBX_INVALID_STORAGE_ID ((DWORD) -1)
#define XBX_STORAGE_DECLINED ((DWORD) -2)

#endif // XBOXSTUBS_H
