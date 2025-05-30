//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MYSQLDATABASE_H
#define MYSQLDATABASE_H
#ifdef _WIN32
#pragma once
#endif

#include "ISQLDBReplyTarget.h"
#include "tier1/utlvector.h"
#include "tier1/utllinkedlist.h"

class ISQLDBCommand;

typedef struct _RTL_CRITICAL_SECTION RTL_CRITICAL_SECTION;
typedef RTL_CRITICAL_SECTION CRITICAL_SECTION;

//-----------------------------------------------------------------------------
// Purpose: Generic MySQL accessing database
//			Provides threaded I/O queue functionality for accessing a mysql db
//-----------------------------------------------------------------------------
class CMySqlDatabase
{
public:
	// constructor
	CMySqlDatabase();
	virtual ~CMySqlDatabase();

	// initialization - must be called before this object can be used
	bool Initialize();

	// Dispatches responses to SQLDB queries
	bool RunFrame();

	// load info - returns the number of sql db queries waiting to be processed
	virtual int QueriesInOutQueue();

	// number of queries finished processing, waiting to be responded to
	virtual int QueriesInFinishedQueue();

	// activates the thread
	void RunThread();

	// command queues
	void AddCommandToQueue(ISQLDBCommand *cmd, ISQLDBReplyTarget *replyTarget, int returnState = 0);

private:

	// threading data
	std::atomic_bool m_bRunThread;
	CRITICAL_SECTION *m_pcsThread;
	CRITICAL_SECTION *m_pcsInQueue;
	CRITICAL_SECTION *m_pcsOutQueue;
	CRITICAL_SECTION *m_pcsDBAccess;

	// wait event
	HANDLE m_hEvent;

	struct msg_t
	{
		ISQLDBCommand *cmd;
		ISQLDBReplyTarget *replyTarget;
		int result;
		int returnState;
	};

	// command queues
	CUtlLinkedList<msg_t, int> m_InQueue;
	CUtlLinkedList<msg_t, int> m_OutQueue;
};

class Connection;

//-----------------------------------------------------------------------------
// Purpose: Interface to a command
//-----------------------------------------------------------------------------
class ISQLDBCommand
{
public:
	// makes the command run (blocking), returning the success code
	virtual int RunCommand() = 0;

	// return data
	virtual void *GetReturnData() { return NULL; }

	// returns the command ID
	virtual int GetID() { return 0; }

	// gets information about the command for if it failed
	virtual void GetDebugInfo(char *buf, int bufSize) { buf[0] = 0; }

	// use to delete
	virtual void deleteThis() = 0;

protected:
	// protected destructor, so that it has to be deleted through deleteThis()
	virtual ~ISQLDBCommand() {}
};


#endif // MYSQLDATABASE_H
