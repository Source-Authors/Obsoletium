// Copyright Valve Corporation, All rights reserved.
//
// Purpose: INetMessage interface

#ifndef SE_PUBLIC_INETMESSAGE_H_
#define SE_PUBLIC_INETMESSAGE_H_

#include "tier1/bitbuf.h"

class INetMsgHandler;
class INetMessage;
class INetChannel;

class INetMessage
{
public:
	virtual ~INetMessage() {};

	// Use these to setup who can hear whose voice.  Pass in client indices (which
	// are their ent indices - 1).

	// netchannel this message is from/for.
	virtual void SetNetChannel(INetChannel *netchan) = 0;

	// set to true if it's a reliable message.
	virtual void SetReliable(bool state) = 0;

	// calles the recently set handler to process this message
	virtual bool Process() = 0;

	// returns true if parsing was OK
	virtual bool ReadFromBuffer(class bf_read &buffer) = 0;

	// returns true if writing was OK
	virtual bool WriteToBuffer(class bf_write &buffer) = 0;

	// true, if message needs reliable handling
	virtual bool IsReliable() const = 0;

	// returns module specific header tag eg svc_serverinfo
	virtual int GetType() const = 0;

	// returns net message group of this message
	virtual int GetGroup() const = 0;

	// returns network message name, eg "svc_serverinfo"
	virtual const char *GetName() const = 0;

	// returns net channel.
	virtual INetChannel *GetNetChannel() const = 0;

	// returns a human readable string about message content
	virtual const char *ToString() const = 0;
};

#endif  // !SE_PUBLIC_INETMESSAGE_H_
