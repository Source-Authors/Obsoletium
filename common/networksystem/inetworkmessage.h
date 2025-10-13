//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: INetworkMessage interface
//
// $NoKeywords: $
//===========================================================================//

#ifndef INETWORKMESSAGE_H
#define INETWORKMESSAGE_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "tier0/dbg.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class bf_read;
class bf_write;
class INetMsgHandler;
class INetMessage;
class INetChannel;


//-----------------------------------------------------------------------------
// First valid group number users of the network system can use
//-----------------------------------------------------------------------------
enum
{
	NETWORKSYSTEM_FIRST_GROUP = 1,
};


//-----------------------------------------------------------------------------
// A network message
//-----------------------------------------------------------------------------
abstract_class INetworkMessage
{
public:
	// Use these to setup who can hear whose voice.
	// Pass in client indices (which are their ent indices - 1).
	virtual void	SetNetChannel(INetChannel * netchan) = 0; // netchannel this message is from/for
	virtual void	SetReliable( bool state ) = 0;	// set to true if it's a reliable message
		
	virtual	bool	ReadFromBuffer( bf_read &buffer ) = 0; // returns true if parsing was OK
	virtual	bool	WriteToBuffer( bf_write &buffer ) = 0;	// returns true if writing was OK
		
	[[nodiscard]] virtual bool	IsReliable() const = 0;  // true, if message needs reliable handling
	
	[[nodiscard]] virtual int				GetGroup() const = 0;	// returns net message group of this message
	[[nodiscard]] virtual int				GetType() const = 0;	// returns module specific header tag eg svc_serverinfo
	[[nodiscard]] virtual const char		*GetGroupName() const = 0;	// returns network message group name
	[[nodiscard]] virtual const char		*GetName() const = 0;	// returns network message name, eg "svc_serverinfo"
	[[nodiscard]] virtual INetChannel		*GetNetChannel() const = 0;
	[[nodiscard]] virtual const char		*ToString() const = 0; // returns a human readable string about message content

	virtual void Release() = 0;

protected:
	virtual	~INetworkMessage() = default;
};


//-----------------------------------------------------------------------------
// Helper utilities for clients to create messages
//-----------------------------------------------------------------------------
#define DECLARE_BASE_MESSAGE( group, msgtype, description )		\
	public:														\
		virtual bool			ReadFromBuffer( bf_read &buffer );	\
		virtual bool			WriteToBuffer( bf_write &buffer );	\
		virtual const char		*ToString() const { return description; } \
		virtual int				GetGroup() const { return group; } \
		virtual const char		*GetGroupName( void ) const { return #group; } \
		virtual int				GetType() const { return msgtype; } \
		virtual const char		*GetName() const { return #msgtype;}\
					

//-----------------------------------------------------------------------------
// Default empty base class for net messages
//-----------------------------------------------------------------------------
class CNetworkMessage : public INetworkMessage
{
public:
	CNetworkMessage() 
	{	
		m_bReliable = true;
		m_pNetChannel = nullptr; 
	}

	void Release() override
	{
		delete this;
	}

	~CNetworkMessage() override = default;

	void SetReliable( bool state ) override 
	{ 
		m_bReliable = state;
	}

	[[nodiscard]] bool IsReliable() const override 
	{ 
		return m_bReliable;
	}

	void SetNetChannel(INetChannel * netchan) override
	{ 
		m_pNetChannel = netchan; 
	}

	virtual bool Process()
	{
		// no handler set
		Assert( 0 ); 
		return false; 
	}	

	[[nodiscard]] INetChannel	*GetNetChannel() const override
	{ 
		return m_pNetChannel; 
	}
		
protected:
	bool m_bReliable;			// true if message should be send reliable
	INetChannel *m_pNetChannel;	// netchannel this message is from/for
};


#endif // INETWORKMESSAGE_H

