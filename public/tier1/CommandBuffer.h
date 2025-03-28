//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//


#ifndef COMMANDBUFFER_H
#define COMMANDBUFFER_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utllinkedlist.h"
#include "tier1/convar.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CUtlBuffer;


//-----------------------------------------------------------------------------
// Invalid command handle
//-----------------------------------------------------------------------------
typedef intp CommandHandle_t;
enum
{
	COMMAND_BUFFER_INVALID_COMMAND_HANDLE = 0
};


//-----------------------------------------------------------------------------
// A command buffer class- a queue of argc/argv based commands associated
// with a particular time
//-----------------------------------------------------------------------------
class CCommandBuffer
{
public:
	// Constructor, destructor
	CCommandBuffer( );
	~CCommandBuffer() = default;

    // Inserts text into the command buffer
	bool AddText( const char *pText, int nTickDelay = 0 );

	// Used to iterate over all commands appropriate for the current time
	void BeginProcessingCommands( int nDeltaTicks );
	bool DequeueNextCommand( );
	intp DequeueNextCommand( const char **& ppArgv );
	[[nodiscard]] intp ArgC() const;
	[[nodiscard]] const char **ArgV() const;
	[[nodiscard]] const char *ArgS() const;		// All args that occur after the 0th arg, in string form
	[[nodiscard]] const char *GetCommandString() const;	// The entire command in string form, including the 0th arg
	[[nodiscard]] const CCommand& GetCommand() const;
	void EndProcessingCommands();

	// Are we in the middle of processing commands?
	[[nodiscard]] bool IsProcessingCommands() const;

	// Delays all queued commands to execute at a later time
	void DelayAllQueuedCommands( int nTickDelay );

	// Indicates how long to delay when encoutering a 'wait' command
	void SetWaitDelayTime( int nTickDelay );

	// Returns a handle to the next command to process
	// (useful when inserting commands into the buffer during processing
	// of commands to force immediate execution of those commands,
	// most relevantly, to implement a feature where you stream a file
	// worth of commands into the buffer, where the file size is too large
	// to entirely contain in the buffer).
    [[nodiscard]] CommandHandle_t GetNextCommandHandle();

	// Specifies a max limit of the args buffer. For unittesting. Size == 0 means use default
	void LimitArgumentBufferSize( intp nSize );

	void SetWaitEnabled( bool bEnable )		{ m_bWaitEnabled = bEnable; }
	[[nodiscard]] bool IsWaitEnabled( void ) const		{ return m_bWaitEnabled; }

	[[nodiscard]] intp GetArgumentBufferSize() const { return m_nArgSBufferSize; }
	[[nodiscard]] intp GetMaxArgumentBufferSize() const { return m_nMaxArgSBufferLength; }

private:
	enum
	{
		ARGS_BUFFER_LENGTH = 8192,
	};

	struct Command_t
	{
		int m_nTick;
		int m_nFirstArgS;
		intp m_nBufferSize;
	};

	// Insert a command into the command queue at the appropriate time
	void InsertCommandAtAppropriateTime( CommandHandle_t hCommand );
						   
	// Insert a command into the command queue
	// Only happens if it's inserted while processing other commands
	void InsertImmediateCommand( CommandHandle_t hCommand );

	// Insert a command into the command queue
	bool InsertCommand( const char *pArgS, intp nCommandSize, int nTick );

	// Returns the length of the next command, as well as the offset to the next command
	void GetNextCommandLength( const char *pText, intp nMaxLen, intp *pCommandLength, intp *pNextCommandOffset );

	// Compacts the command buffer
	void Compact();

	// Parses argv0 out of the buffer
	bool ParseArgV0( CUtlBuffer &buf, char *pArgv0, intp nMaxLen, const char **pArgs );

	char	m_pArgSBuffer[ ARGS_BUFFER_LENGTH ];
	intp	m_nLastUsedArgSSize;
	intp	m_nArgSBufferSize;
	CUtlFixedLinkedList< Command_t >	m_Commands;
	int		m_nCurrentTick;
	int		m_nLastTickToProcess;
	int		m_nWaitDelayTicks;
	CommandHandle_t		m_hNextCommand;
	intp	m_nMaxArgSBufferLength;
	bool	m_bIsProcessingCommands;
	bool	m_bWaitEnabled;

	// NOTE: This is here to avoid the pointers returned by DequeueNextCommand
	// to become invalid by calling AddText. Is there a way we can avoid the memcpy?
	CCommand m_CurrentCommand;
};


//-----------------------------------------------------------------------------
// Returns the next command
//-----------------------------------------------------------------------------
inline intp CCommandBuffer::ArgC() const
{
	return m_CurrentCommand.ArgC();
}

inline const char **CCommandBuffer::ArgV() const
{
	return m_CurrentCommand.ArgV();
}

inline const char *CCommandBuffer::ArgS() const
{
	return m_CurrentCommand.ArgS();
}

inline const char *CCommandBuffer::GetCommandString() const
{
	return m_CurrentCommand.GetCommandString();
}

inline const CCommand& CCommandBuffer::GetCommand() const
{
	return m_CurrentCommand;
}

#endif // COMMANDBUFFER_H
