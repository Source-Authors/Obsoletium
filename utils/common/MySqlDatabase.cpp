//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "MySqlDatabase.h"

#include "winlite.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMySqlDatabase::CMySqlDatabase()
{
	m_bRunThread.store(false, std::memory_order::memory_order_relaxed);
	m_pcsThread   = new CRITICAL_SECTION;
	m_pcsInQueue  = new CRITICAL_SECTION;
	m_pcsOutQueue = new CRITICAL_SECTION;
	m_pcsDBAccess = new CRITICAL_SECTION;

	m_hEvent = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//			blocks until db process thread has stopped
//-----------------------------------------------------------------------------
CMySqlDatabase::~CMySqlDatabase()
{
	// flag the thread to stop
	m_bRunThread.store(false, std::memory_order::memory_order_acq_rel);

	// pulse the thread to make it run
	::SetEvent(m_hEvent);

	// make sure it's done
	::EnterCriticalSection(m_pcsThread);
	::LeaveCriticalSection(m_pcsThread);
	
	delete m_pcsDBAccess;
	delete m_pcsOutQueue;
	delete m_pcsInQueue;
	delete m_pcsThread;
}

//-----------------------------------------------------------------------------
// Purpose: Thread access function
//-----------------------------------------------------------------------------
static unsigned WINAPI staticThreadFunc(void *param)
{
	// dimhotepus: Add thread name to aid debugging.
	ThreadSetDebugName("MySQLQueryRunner");

	((CMySqlDatabase *)param)->RunThread();
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Establishes connection to the database and sets up this object to handle db command
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMySqlDatabase::Initialize()
{
	// prepare critical sections
	// always succeeds since Windows Vista.
	(void)::InitializeCriticalSectionAndSpinCount(m_pcsThread, 4000);
	(void)::InitializeCriticalSectionAndSpinCount(m_pcsInQueue, 4000);
	(void)::InitializeCriticalSectionAndSpinCount(m_pcsOutQueue, 4000);
	(void)::InitializeCriticalSectionAndSpinCount(m_pcsDBAccess, 4000);

	// initialize wait calls
	m_hEvent = ::CreateEvent(NULL, false, true, NULL);

	// start the DB-access thread
	m_bRunThread.store(true, std::memory_order::memory_order_acq_rel);

	::_beginthreadex(NULL, 0, staticThreadFunc, this, 0, nullptr);

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Main thread loop
//-----------------------------------------------------------------------------
void CMySqlDatabase::RunThread()
{
	::EnterCriticalSection(m_pcsThread);
	while (m_bRunThread.load(std::memory_order_acq_rel))
	{
		if (m_InQueue.Count() > 0)
		{
			// get a dispatched DB request
			::EnterCriticalSection(m_pcsInQueue);
			// pop the front of the queue
			auto headIndex = m_InQueue.Head();
			msg_t msg = m_InQueue[headIndex];
			m_InQueue.Remove(headIndex);
			::LeaveCriticalSection(m_pcsInQueue);

			::EnterCriticalSection(m_pcsDBAccess);
			// run sqldb command
			msg.result = msg.cmd->RunCommand();
			::LeaveCriticalSection(m_pcsDBAccess);

			if (msg.replyTarget)
			{
				// put the results in the outgoing queue
				::EnterCriticalSection(m_pcsOutQueue);
				m_OutQueue.AddToTail(msg);
				::LeaveCriticalSection(m_pcsOutQueue);

				// wake up out queue
				msg.replyTarget->WakeUp();
			}
			else
			{
				// there is no return data from the call, so kill the object now
				msg.cmd->deleteThis();
			}
		}
		else
		{
			// nothing in incoming queue, so wait until we get the signal
			::WaitForSingleObject(m_hEvent, INFINITE);
		}

		// check the size of the outqueue; if it's getting too big, sleep to let the main thread catch up
		if (m_OutQueue.Count() > 50)
		{
			::Sleep(2);
		}
	}
	::LeaveCriticalSection(m_pcsThread);
}

//-----------------------------------------------------------------------------
// Purpose: Adds a database command to the queue, and wakes the db thread
//-----------------------------------------------------------------------------
void CMySqlDatabase::AddCommandToQueue(ISQLDBCommand *cmd, ISQLDBReplyTarget *replyTarget, int returnState)
{
	::EnterCriticalSection(m_pcsInQueue);
	// add to the queue
	msg_t msg = { cmd, replyTarget, 0, returnState };
	m_InQueue.AddToTail(msg);
	::LeaveCriticalSection(m_pcsInQueue);

	// signal the thread to start running
	::SetEvent(m_hEvent);
}

//-----------------------------------------------------------------------------
// Purpose: Dispatches responses to SQLDB queries
//-----------------------------------------------------------------------------
bool CMySqlDatabase::RunFrame()
{
	bool doneWork = false;

	while (m_OutQueue.Count() > 0)
	{
		::EnterCriticalSection(m_pcsOutQueue);
		// pop the first item in the queue
		auto headIndex = m_OutQueue.Head();
		msg_t msg = m_OutQueue[headIndex];
		m_OutQueue.Remove(headIndex);
		::LeaveCriticalSection(m_pcsOutQueue);

		// run result
		if (msg.replyTarget)
		{
			msg.replyTarget->SQLDBResponse(msg.cmd->GetID(), msg.returnState, msg.result, msg.cmd->GetReturnData());

			// kill command
			// it would be a good optimization to be able to reuse these
			msg.cmd->deleteThis();
		}

		doneWork = true;
	}

	return doneWork;
}

//-----------------------------------------------------------------------------
// Purpose: load info - returns the number of sql db queries waiting to be processed
//-----------------------------------------------------------------------------
int CMySqlDatabase::QueriesInOutQueue()
{
	// the queue names are from the DB point of view, not the server - thus the reversal
	return m_InQueue.Count();
}

//-----------------------------------------------------------------------------
// Purpose: number of queries finished processing, waiting to be responded to
//-----------------------------------------------------------------------------
int CMySqlDatabase::QueriesInFinishedQueue()
{
	return m_OutQueue.Count();
}
