//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef NETMESSAGES_H
#define NETMESSAGES_H

#include "tier1/checksum_crc.h"
#include "tier1/checksum_md5.h"
#include "tier1/utlvector.h"
#include "tier1/bitbuf.h"
#include "mathlib/vector.h"

#include "inetmessage.h"
#include "const.h"
#include "qlimits.h"
#include "soundflags.h"
#include "inetchannel.h"
#include "protocol.h"
#include "inetmsghandler.h"
#include "igameevents.h"
#include "bitvec.h"
#include "engine/iserverplugin.h"
#include "Color.h"
#include "proto_version.h"

#if !defined( _X360 )
#include "xbox/xboxstubs.h"
#endif

class SendTable;
class KeyValue;
class KeyValues;
class INetMessageHandler;
class IServerMessageHandler;
class IClientMessageHandler;

#define DECLARE_BASE_MESSAGE( msgtype )						\
	public:													\
		bool			ReadFromBuffer( bf_read &buffer ) override;	\
		bool			WriteToBuffer( bf_write &buffer ) override;	\
		const char		*ToString() const override;					\
		int				GetType() const override { return msgtype; } \
		const char		*GetName() const override { return #msgtype;}\
			
#define DECLARE_NET_MESSAGE( name )			\
	DECLARE_BASE_MESSAGE( net_##name );		\
	INetMessageHandler *m_pMessageHandler;	\
	bool Process() override { return m_pMessageHandler->Process##name( this ); }\
	
#define DECLARE_SVC_MESSAGE( name )		\
	DECLARE_BASE_MESSAGE( svc_##name );	\
	IServerMessageHandler *m_pMessageHandler;\
	bool Process() override { return m_pMessageHandler->Process##name( this ); }\
	
#define DECLARE_CLC_MESSAGE( name )		\
	DECLARE_BASE_MESSAGE( clc_##name );	\
	IClientMessageHandler *m_pMessageHandler;\
	bool Process() override { return m_pMessageHandler->Process##name( this ); }\
		
#define DECLARE_MM_MESSAGE( name )		\
	DECLARE_BASE_MESSAGE( mm_##name );	\
	IMatchmakingMessageHandler *m_pMessageHandler;\
	bool Process() override { return m_pMessageHandler->Process##name( this ); }\

class CNetMessage : public INetMessage
{
public:
	CNetMessage()  = default;

	~CNetMessage() override = default;

	[[nodiscard]] int		GetGroup() const override { return INetChannelInfo::GENERIC; }
	[[nodiscard]] INetChannel		*GetNetChannel() const override { return m_NetChannel; }
		
	void	SetReliable(bool state) override {m_bReliable = state;};
	[[nodiscard]] bool	IsReliable() const override { return m_bReliable; };
	void    SetNetChannel(INetChannel * netchan) override { m_NetChannel = netchan; }

protected:
	// dimhotepus: Reorder message to reduce size.
	INetChannel			*m_NetChannel{nullptr};	// netchannel this message is from/for
	bool				m_bReliable{true};	// true if message should be send reliable
};


///////////////////////////////////////////////////////////////////////////////////////
// bidirectional net messages:
///////////////////////////////////////////////////////////////////////////////////////


class NET_SetConVar : public CNetMessage
{
	DECLARE_NET_MESSAGE( SetConVar );

	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::STRINGCMD; }

	NET_SetConVar() : m_pMessageHandler{nullptr} {}
	NET_SetConVar(const char *name, const char *value) : NET_SetConVar{}
	{
		cvar_t localCvar = {};
		V_strcpy_safe( localCvar.name, name );
		V_strcpy_safe( localCvar.value, value );
		m_ConVars.AddToTail( localCvar );	
	}

public:	
	
	using cvar_t = struct cvar_s
	{
		char	name[MAX_OSPATH];
		char	value[MAX_OSPATH];
	};

	CUtlVector<cvar_t> m_ConVars;
};

class NET_StringCmd : public CNetMessage
{
	DECLARE_NET_MESSAGE( StringCmd );

	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::STRINGCMD; }

	NET_StringCmd() : NET_StringCmd{nullptr} {};
	explicit NET_StringCmd(const char *cmd)
		: m_pMessageHandler{nullptr},
		m_szCommand{cmd}
	{
		m_szCommandBuffer[0] = '\0';
	};

public:	
	const char	*m_szCommand;	// execute this command
	
private:
	char		m_szCommandBuffer[1024];	// buffer for received messages
	
};

class NET_Tick : public CNetMessage
{
	DECLARE_NET_MESSAGE( Tick );

	NET_Tick() : NET_Tick{ -1, 0.0f, 0.0f }
	{ 
	};

	NET_Tick( int tick, [[maybe_unused]] float hostFrametime, [[maybe_unused]] float hostFrametime_stddeviation ) : m_pMessageHandler{nullptr}
	{ 
		m_bReliable = false; 
		m_nTick = tick; 
#if PROTOCOL_VERSION > 10
		m_flHostFrameTime			= hostFrametime;
		m_flHostFrameTimeStdDeviation	= hostFrametime_stddeviation;
#endif
	};
	
public:
	int			m_nTick; 
#if PROTOCOL_VERSION > 10
	float		m_flHostFrameTime;
	float		m_flHostFrameTimeStdDeviation;
#endif
};

class NET_SignonState : public CNetMessage
{
	DECLARE_NET_MESSAGE( SignonState );

	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::SIGNON; }

	NET_SignonState() : NET_SignonState{ -1, -1 } {}
	NET_SignonState( int state, int spawncount )
		: m_pMessageHandler{nullptr},
		m_nSignonState{state},
		m_nSpawnCount{spawncount}
	{
	}

public:
	int			m_nSignonState;			// See SIGNONSTATE_ defines
	int			m_nSpawnCount;			// server spawn count (session number)
};


///////////////////////////////////////////////////////////////////////////////////////
// Client messages:
///////////////////////////////////////////////////////////////////////////////////////

class CLC_ClientInfo : public CNetMessage
{
	DECLARE_CLC_MESSAGE( ClientInfo );

public:
	CLC_ClientInfo()
		: m_pMessageHandler{nullptr}
	{
		m_bReliable = false;
		m_nSendTableCRC = 0;
		m_nServerCount = -1;
		m_bIsHLTV = false;
#if defined( REPLAY_ENABLED )
		m_bIsReplay = false;
#endif
		m_nFriendsID = 0;
		m_FriendsName[0] = '\0';
	}

	CRC32_t			m_nSendTableCRC;
	int				m_nServerCount;
	bool			m_bIsHLTV;
#if defined( REPLAY_ENABLED )
	bool			m_bIsReplay;
#endif
	uint32			m_nFriendsID;
	char			m_FriendsName[MAX_PLAYER_NAME_LENGTH];
	CRC32_t			m_nCustomFiles[MAX_CUSTOM_FILES];
};



class CLC_Move : public CNetMessage
{
	DECLARE_CLC_MESSAGE( Move );

	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::MOVE; }

	CLC_Move()
		: m_pMessageHandler{nullptr}
		
	{
		m_bReliable = false;
	}

public:
	int				m_nBackupCommands{-1};
	int				m_nNewCommands{-1};
	int				m_nLength{-1};
	bf_read			m_DataIn;
	bf_write		m_DataOut;
};

class CLC_VoiceData : public CNetMessage
{
	DECLARE_CLC_MESSAGE( VoiceData );

	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::VOICE; }

	CLC_VoiceData()
		: m_pMessageHandler{nullptr},
		
		m_xuid{std::numeric_limits<uint64>::max()}
	{
		m_bReliable = false;
	}

public:
	int				m_nLength{-1};
	bf_read			m_DataIn;
	bf_write		m_DataOut;
	uint64			m_xuid;
};

class CLC_BaselineAck : public CNetMessage
{
	DECLARE_CLC_MESSAGE( BaselineAck );

	CLC_BaselineAck() : CLC_BaselineAck{ -1, -1, } {}
	CLC_BaselineAck( int tick, int baseline )
		: m_pMessageHandler{nullptr},
		m_nBaselineTick{tick},
		m_nBaselineNr{baseline}
	{}

	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::ENTITIES; }

public:
	int		m_nBaselineTick;	// sequence number of baseline
	int		m_nBaselineNr;		// 0 or 1 		
};

class CLC_ListenEvents : public CNetMessage
{
	DECLARE_CLC_MESSAGE( ListenEvents );

	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::SIGNON; }

public:
	CBitVec<MAX_EVENT_NUMBER> m_EventArray;
};

#if defined( REPLAY_ENABLED )
class CLC_SaveReplay : public CNetMessage
{
	DECLARE_CLC_MESSAGE( SaveReplay );

	CLC_SaveReplay()
		: m_pMessageHandler{nullptr},
		m_nStartSendByte{-1},
		m_flPostDeathRecordTime{-1}
	{
		m_szFilename[0] = '\0';
	}

	int		m_nStartSendByte;
	char	m_szFilename[ MAX_OSPATH ];
	float	m_flPostDeathRecordTime;
};
#endif

class CLC_RespondCvarValue : public CNetMessage
{
public:
	DECLARE_CLC_MESSAGE( RespondCvarValue );

	QueryCvarCookie_t		m_iCookie;
	
	const char				*m_szCvarName;
	const char				*m_szCvarValue;	// The sender sets this, and it automatically points it at m_szCvarNameBuffer when receiving.

	EQueryCvarValueStatus	m_eStatusCode;

private:
	char		m_szCvarNameBuffer[256];
	char		m_szCvarValueBuffer[256];
};

class CLC_FileCRCCheck : public CNetMessage
{
public:
	DECLARE_CLC_MESSAGE( FileCRCCheck );
	char		m_szPathID[MAX_PATH];
	char		m_szFilename[MAX_PATH];
	MD5Value_t	m_MD5;
	CRC32_t		m_CRCIOs;
	int			m_eFileHashType;
	int			m_cbFileLen;
	int			m_nPackFileNumber;
	int			m_PackFileID;
	int			m_nFileFraction;
};

class CLC_FileMD5Check : public CNetMessage
{
public:
	DECLARE_CLC_MESSAGE( FileMD5Check );

	char		m_szPathID[MAX_PATH];
	char		m_szFilename[MAX_PATH];
	MD5Value_t	m_MD5;
};

class Base_CmdKeyValues : public CNetMessage
{
protected:
	explicit Base_CmdKeyValues( KeyValues *pKeyValues = nullptr ); // takes ownership
	~Base_CmdKeyValues() override;

public:
	[[nodiscard]] KeyValues * GetKeyValues() const { return m_pKeyValues; }

public:
	bool ReadFromBuffer( bf_read &buffer ) override;
	bool WriteToBuffer( bf_write &buffer ) override;
	[[nodiscard]] const char * ToString() const override;

protected:
	KeyValues *m_pKeyValues;
};

class CLC_CmdKeyValues : public Base_CmdKeyValues
{
public:
	DECLARE_CLC_MESSAGE( CmdKeyValues );

public:
	explicit CLC_CmdKeyValues( KeyValues *pKeyValues = nullptr );	// takes ownership
};

class SVC_CmdKeyValues : public Base_CmdKeyValues
{
public:
	DECLARE_SVC_MESSAGE( CmdKeyValues );

public:
	explicit SVC_CmdKeyValues( KeyValues *pKeyValues = nullptr );	// takes ownership
};

///////////////////////////////////////////////////////////////////////////////////////
// server messages:
///////////////////////////////////////////////////////////////////////////////////////



class SVC_Print : public CNetMessage
{
	DECLARE_SVC_MESSAGE( Print );

	SVC_Print() : SVC_Print{nullptr} {}

	explicit SVC_Print(const char * text)
		: m_pMessageHandler{nullptr},
		m_szText{text}
	{
		m_bReliable = false;
		m_szTextBuffer[0] = '\0';
	}

public:	
	const char	*m_szText;	// show this text
	
private:
	char		m_szTextBuffer[2048];	// buffer for received messages
};

class SVC_ServerInfo : public CNetMessage
{
	DECLARE_SVC_MESSAGE( ServerInfo );

	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::SIGNON; }

public:	// member vars are public for faster handling
	int			m_nProtocol;	// protocol version
	int			m_nServerCount;	// number of changelevels since server start
	bool		m_bIsDedicated;  // dedicated server ?	
	bool		m_bIsHLTV;		// HLTV server ?
#if defined( REPLAY_ENABLED )
	bool		m_bIsReplay;	// Replay server ?
#endif
	char		m_cOS;			// L = linux, W = Win32, O - OSX
	CRC32_t		m_nMapCRC;		// server map CRC (only used by older demos)
	MD5Value_t	m_nMapMD5;		// server map MD5
	int			m_nMaxClients;	// max number of clients on server
	int			m_nMaxClasses;	// max number of server classes
	int			m_nPlayerSlot;	// our client slot number
	float		m_fTickInterval;// server tick interval
	const char	*m_szGameDir;	// game directory eg "tf2"
	const char	*m_szMapName;	// name of current map 
	const char	*m_szSkyName;	// name of current skybox 
	const char	*m_szHostName;	// server name

private:
	char		m_szGameDirBuffer[MAX_OSPATH];// game directory eg "tf2"
	char		m_szMapNameBuffer[MAX_OSPATH];// name of current map 
	char		m_szSkyNameBuffer[MAX_OSPATH];// name of current skybox 
	char		m_szHostNameBuffer[MAX_OSPATH];// name of current skybox 
};

class SVC_SendTable : public CNetMessage
{
	DECLARE_SVC_MESSAGE( SendTable );

	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::SIGNON; }
		
public:
	bool			m_bNeedsDecoder;
	int				m_nLength;
	bf_read			m_DataIn;
	bf_write		m_DataOut;
};

class SVC_ClassInfo : public CNetMessage
{
	DECLARE_SVC_MESSAGE( ClassInfo );

	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::SIGNON; }

	SVC_ClassInfo() : SVC_ClassInfo{false, -1} {};
	SVC_ClassInfo( bool createFromSendTables, int numClasses ) 
		: m_pMessageHandler{nullptr},
		  m_bCreateOnClient{createFromSendTables},
		  m_nNumServerClasses{numClasses} {}

public:
		
	using class_t = struct class_s
	{
		int		classID;
		char	datatablename[256];
		char	classname[256];
	};

	bool					m_bCreateOnClient;	// if true, client creates own SendTables & classinfos from game.dll
	CUtlVector<class_t>		m_Classes;			
	intp					m_nNumServerClasses;
};
	

class SVC_SetPause : public CNetMessage
{
	DECLARE_SVC_MESSAGE( SetPause );
	
	SVC_SetPause() : SVC_SetPause{false} {}
	SVC_SetPause( bool state, [[maybe_unused]] float end = -1.f )
		: m_pMessageHandler{nullptr}, m_bPaused{state}
	{}
	
public:
	bool		m_bPaused;		// true or false, what else
};

class SVC_SetPauseTimed : public CNetMessage
{
	DECLARE_SVC_MESSAGE( SetPauseTimed );

	SVC_SetPauseTimed() : SVC_SetPauseTimed{false} {}
	SVC_SetPauseTimed( bool bState, float flExpireTime = -1.f )
		: m_pMessageHandler{nullptr},
		m_bPaused{bState},
		m_flExpireTime{flExpireTime}
	{}

public:
	bool		m_bPaused;
	float		m_flExpireTime;
};


class CNetworkStringTable;

class SVC_CreateStringTable : public CNetMessage
{
	DECLARE_SVC_MESSAGE( CreateStringTable );

	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::SIGNON; }

public:

	SVC_CreateStringTable();

public:	
	
	const char	*m_szTableName;
	int			m_nMaxEntries;
	int			m_nNumEntries;
	bool		m_bUserDataFixedSize;
	int			m_nUserDataSize;
	int			m_nUserDataSizeBits;
	bool		m_bIsFilenames;
	int			m_nLength;
	bf_read		m_DataIn;
	bf_write	m_DataOut;
	bool		m_bDataCompressed;

private:
	char		m_szTableNameBuffer[256];
};

class SVC_UpdateStringTable : public CNetMessage
{
	DECLARE_SVC_MESSAGE( UpdateStringTable );

	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::STRINGTABLE; }

public:
	int				m_nTableID;	// table to be updated
	int				m_nChangedEntries; // number of how many entries has changed
	int				m_nLength;	// data length in bits
	bf_read			m_DataIn;
	bf_write		m_DataOut;
};

// SVC_VoiceInit
//   v2 - 2017/02/07
//     - Can detect v2 packets by nLegacyQuality == 255 and presence of additional nSampleRate field.
//     - Added nSampleRate field. Previously, nSampleRate was hard-coded per codec type. ::ReadFromBuffer does a
//       one-time conversion of these old types (which can no longer change)
//     - Marked quality field as deprecated. This was already being ignored. v2 clients send 255
//     - Prior to this the sv_use_steam_voice convar was used to switch to steam voice. With this, we properly set
//       szVoiceCodec to "steam".  See ::ReadFromBuffer for shim to fallback to the convar for old streams.
//     - We no longer pass "svc_voiceinit NULL" as szVoiceCodec if it is not selected, just the empty string.  Nothing
//       used this that I could find.
class SVC_VoiceInit : public CNetMessage
{
	DECLARE_SVC_MESSAGE( VoiceInit );

	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::SIGNON; }

	SVC_VoiceInit()
		: m_pMessageHandler{nullptr},
		  m_nSampleRate( 0 )
	{
		m_szVoiceCodec[0] = '\0';
	}

	SVC_VoiceInit( const char * codec, int nSampleRate )
		: m_pMessageHandler{nullptr},
		  m_nSampleRate( nSampleRate )
	{
		V_strcpy_safe( m_szVoiceCodec, codec ? codec : "" );
	}


public:
	// Used voice codec for voice_init.
	//
	// This used to be a DLL name, then became a whitelisted list of codecs.
	char		m_szVoiceCodec[MAX_OSPATH];

	// DEPRECATED:
	//
	// This field used to be a custom quality setting, but it was not honored for a long time: codecs use their own
	// pre-configured quality settings. We never sent anything besides 5, which was then ignored for some codecs.
	//
	// New clients always set 255 here, old clients probably send 5. This could be re-purposed in the future, but beware
	// that very old demos may have non-5 values. It would take more archaeology to determine how to properly interpret
	// those packets -- they're probably using settings we simply don't support any longer.
	//
	// int m_nQuality;

	// The sample rate we are using
	int			m_nSampleRate;
};

class SVC_VoiceData : public CNetMessage
{
	DECLARE_SVC_MESSAGE( VoiceData );

	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::VOICE; }

	SVC_VoiceData()
		: m_pMessageHandler{nullptr},
		m_xuid{std::numeric_limits<uint64>::max()}
	{
		m_bReliable = false;
	}

public:	
	int				m_nFromClient{-1};	// client who has spoken
	bool			m_bProximity{false};
	int				m_nLength{-1};		// data length in bits
	uint64			m_xuid;			// X360 player ID

	bf_read			m_DataIn;
	void			*m_DataOut{nullptr};
};

class SVC_Sounds : public CNetMessage
{
	DECLARE_SVC_MESSAGE( Sounds );

	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::SOUNDS; }

public:	

	bool		m_bReliableSound;
	int			m_nNumSounds;
	int			m_nLength;
	bf_read		m_DataIn;
	bf_write	m_DataOut;
};

class SVC_Prefetch : public CNetMessage
{
	DECLARE_SVC_MESSAGE( Prefetch );

	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::SOUNDS; }

	enum
	{
		SOUND = 0,
	};

public:	

	unsigned short	m_fType;
	unsigned short	m_nSoundIndex;
};

class SVC_SetView : public CNetMessage
{
	DECLARE_SVC_MESSAGE( SetView );

	SVC_SetView() : SVC_SetView{-1} {}
	explicit SVC_SetView( int entity )
		: m_pMessageHandler{nullptr},
		m_nEntityIndex{entity}
	{}

public:	
	int				m_nEntityIndex;
		
};

class SVC_FixAngle: public CNetMessage
{
	DECLARE_SVC_MESSAGE( FixAngle );

	SVC_FixAngle() : SVC_FixAngle{false, QAngle{-1, -1, -1}} {}
	SVC_FixAngle( bool bRelative, QAngle angle )
		: m_pMessageHandler{nullptr},
		m_bRelative{bRelative},
		m_Angle{angle}
	{
		m_bReliable = false;
	}

public:	
	bool			m_bRelative; 
	QAngle			m_Angle;
};

class SVC_CrosshairAngle : public CNetMessage
{
	DECLARE_SVC_MESSAGE( CrosshairAngle );

	SVC_CrosshairAngle() : SVC_CrosshairAngle{QAngle{-1, -1, -1}} {}
	explicit SVC_CrosshairAngle(QAngle angle)
		: m_pMessageHandler{nullptr},
		m_Angle{angle}
	{
	}
	
public:
	QAngle			m_Angle;
};

class SVC_BSPDecal : public CNetMessage
{
	DECLARE_SVC_MESSAGE( BSPDecal );
	
public:
	Vector		m_Pos;
	int			m_nDecalTextureIndex;
	int			m_nEntityIndex;
	int			m_nModelIndex;
	bool		m_bLowPriority;
};

class SVC_GameEvent : public CNetMessage
{
	DECLARE_SVC_MESSAGE( GameEvent );

	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::EVENTS; }
	
public:
	int			m_nLength;	// data length in bits
	bf_read		m_DataIn;
	bf_write	m_DataOut;
};

class SVC_UserMessage: public CNetMessage
{
	DECLARE_SVC_MESSAGE( UserMessage );

	SVC_UserMessage()
		: m_pMessageHandler{nullptr}
	{
		m_bReliable = false;
	}

	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::USERMESSAGES; }
	
public:
	int			m_nMsgType{-1};
	int			m_nLength{-1};	// data length in bits
	bf_read		m_DataIn;
	bf_write	m_DataOut;
};

class SVC_EntityMessage : public CNetMessage
{
	DECLARE_SVC_MESSAGE( EntityMessage );

	SVC_EntityMessage()
		: m_pMessageHandler{nullptr}
	{
		m_bReliable = false;
	}

	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::ENTMESSAGES	; }

public:
	int			m_nEntityIndex{-1};
	int			m_nClassID{-1};
	int			m_nLength{-1};	// data length in bits
	bf_read		m_DataIn;
	bf_write	m_DataOut;
};

/* class SVC_SpawnBaseline: public CNetMessage
{
	DECLARE_SVC_MESSAGE( SpawnBaseline );

	int	GetGroup() const { return INetChannelInfo::SIGNON; }
	
public:
	int			m_nEntityIndex;
	int			m_nClassID;
	int			m_nLength;
	bf_read		m_DataIn;
	bf_write	m_DataOut;
	
}; */

class SVC_PacketEntities: public CNetMessage
{
	DECLARE_SVC_MESSAGE( PacketEntities );
	
	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::ENTITIES; }
	
public:

	int			m_nMaxEntries;
	int			m_nUpdatedEntries;
	bool		m_bIsDelta;	
	bool		m_bUpdateBaseline;
	int			m_nBaseline;
	int			m_nDeltaFrom;
	int			m_nLength;
	bf_read		m_DataIn;
	bf_write	m_DataOut;
};

class SVC_TempEntities: public CNetMessage
{
	DECLARE_SVC_MESSAGE( TempEntities );

	SVC_TempEntities()
		: m_pMessageHandler{nullptr}
	{
		m_bReliable = false;
	}

	[[nodiscard]] int	GetGroup() const override { return INetChannelInfo::EVENTS; }

	int			m_nNumEntries{-1};
	int			m_nLength{-1};
	bf_read		m_DataIn;
	bf_write	m_DataOut;
};

class SVC_Menu : public CNetMessage
{
public:
	DECLARE_SVC_MESSAGE( Menu );

	SVC_Menu() : m_pMessageHandler{nullptr}
	{
		m_bReliable = true;
		m_Type = DIALOG_MENU;
		m_MenuKeyValues = nullptr;
		m_iLength = 0;
	}
	SVC_Menu( DIALOG_TYPE type, KeyValues *data ); 
	~SVC_Menu() override;

	KeyValues	*m_MenuKeyValues;
	DIALOG_TYPE m_Type;
	int			m_iLength;
};

class SVC_GameEventList : public CNetMessage
{
public:
	DECLARE_SVC_MESSAGE( GameEventList );

	int			m_nNumEvents;
	int			m_nLength;
	bf_read		m_DataIn;
	bf_write	m_DataOut;
};

///////////////////////////////////////////////////////////////////////////////////////
// Matchmaking messages:
///////////////////////////////////////////////////////////////////////////////////////

class MM_Heartbeat : public CNetMessage
{
public:
	DECLARE_MM_MESSAGE( Heartbeat );
};

class MM_ClientInfo : public CNetMessage
{
public:
	DECLARE_MM_MESSAGE( ClientInfo );

	XNADDR	m_xnaddr;	// xbox net address
	uint64	m_id;		// machine ID
	uint64	m_xuids[MAX_PLAYERS_PER_CLIENT];
	byte	m_cVoiceState[MAX_PLAYERS_PER_CLIENT];
	bool	m_bInvited;
	byte	m_cPlayers;
	char	m_iControllers[MAX_PLAYERS_PER_CLIENT];
	char	m_iTeam[MAX_PLAYERS_PER_CLIENT];
	char	m_szGamertags[MAX_PLAYERS_PER_CLIENT][MAX_PLAYER_NAME_LENGTH];
};

class MM_RegisterResponse : public CNetMessage
{
public:
	DECLARE_MM_MESSAGE( RegisterResponse );
};

class MM_Mutelist : public CNetMessage
{
public:
	DECLARE_MM_MESSAGE( Mutelist );

	uint64	m_id;
	byte	m_cPlayers;
	byte	m_cRemoteTalkers[MAX_PLAYERS_PER_CLIENT];
	XUID	m_xuid[MAX_PLAYERS_PER_CLIENT];
	byte	m_cMuted[MAX_PLAYERS_PER_CLIENT];
	CUtlVector< XUID >	m_Muted[MAX_PLAYERS_PER_CLIENT];
};

class MM_Checkpoint : public CNetMessage
{
public:
	DECLARE_MM_MESSAGE( Checkpoint );

	enum eCheckpoint
	{
		CHECKPOINT_CHANGETEAM,
		CHECKPOINT_GAME_LOBBY,
		CHECKPOINT_PREGAME,
		CHECKPOINT_LOADING_COMPLETE,
		CHECKPOINT_CONNECT,
		CHECKPOINT_SESSION_DISCONNECT,
		CHECKPOINT_REPORT_STATS,
		CHECKPOINT_REPORTING_COMPLETE,
		CHECKPOINT_POSTGAME,
	};

	byte m_Checkpoint;
};

// NOTE: The following messages are not network-endian compliant, due to the
// transmission of structures instead of their component parts
class MM_JoinResponse : public CNetMessage
{
public:
	DECLARE_MM_MESSAGE( JoinResponse );

	MM_JoinResponse() : m_pMessageHandler{nullptr}
	{
		m_ResponseType = std::numeric_limits<uint>::max();
		m_id = std::numeric_limits<uint64>::max();
		m_Nonce = std::numeric_limits<uint64>::max();
		m_SessionFlags = 0;
		m_nOwnerId = std::numeric_limits<uint>::max();
		m_iTeam = std::numeric_limits<int>::max();
		m_nTotalTeams = std::numeric_limits<int>::max();
		m_ContextCount = 0;
		m_PropertyCount = 0;
	}

	enum
	{
		JOINRESPONSE_APPROVED,
		JOINRESPONSE_APPROVED_JOINGAME,
		JOINRESPONSE_SESSIONFULL,
		JOINRESPONSE_NOTHOSTING,
		JOINRESPONSE_MODIFY_SESSION,
	};
	uint	m_ResponseType;

	// host info
	uint64	m_id;				// Host's machine ID
	uint64	m_Nonce;			// Session nonce
	uint	m_SessionFlags;
	uint	m_nOwnerId;
	int		m_iTeam;
	int		m_nTotalTeams;
	int		m_PropertyCount;
	int		m_ContextCount;
	CUtlVector< XUSER_PROPERTY >m_SessionProperties;
	CUtlVector< XUSER_CONTEXT >m_SessionContexts;
};

class SVC_GetCvarValue : public CNetMessage
{
public:
	DECLARE_SVC_MESSAGE( GetCvarValue );

	QueryCvarCookie_t	m_iCookie;
	const char			*m_szCvarName;	// The sender sets this, and it automatically points it at m_szCvarNameBuffer when receiving.

private:
	char		m_szCvarNameBuffer[256];
};

#endif // NETMESSAGES_H
