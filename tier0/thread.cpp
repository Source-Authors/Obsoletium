// Copyright Valve Corporation, All rights reserved.
//
// Thread management routines.

#include "stdafx.h"

#include "tier0/valve_off.h"

#include <system_error>

#ifdef _WIN32
#include "winlite.h"
#include <TlHelp32.h>
#endif

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier0/threadtools.h"

ThreadId_t Plat_GetCurrentThreadID()
{
	return ThreadGetCurrentId();
}


#if defined(_WIN32)

namespace {

CThreadMutex s_BreakpointStateMutex;

struct X86_64HardwareBreakpointState_t
{
	const void *pAddress[4];
	char nWatchBytes[4];
	bool bBreakOnRead[4];
};
X86_64HardwareBreakpointState_t s_BreakpointState = { {0,0,0,0}, {0,0,0,0}, {false,false,false,false} };

void X86_64ApplyBreakpointsToThread( DWORD dwThreadId )
{
	X86_64HardwareBreakpointState_t *pState = &s_BreakpointState;

	CONTEXT ctx = {};
	ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	ctx.Dr0 = static_cast<decltype(ctx.Dr0)>( (DWORD_PTR) pState->pAddress[0] );
	ctx.Dr1 = static_cast<decltype(ctx.Dr1)>( (DWORD_PTR) pState->pAddress[1] );
	ctx.Dr2 = static_cast<decltype(ctx.Dr2)>( (DWORD_PTR) pState->pAddress[2] );
	ctx.Dr3 = static_cast<decltype(ctx.Dr3)>( (DWORD_PTR) pState->pAddress[3] );
	ctx.Dr7 = static_cast<decltype(ctx.Dr7)>( (DWORD_PTR) 0 );

	for ( int i = 0; i < 4; ++i )
	{
		if ( pState->pAddress[i] && pState->nWatchBytes[i] )
		{
			ctx.Dr7 |= static_cast<decltype(ctx.Dr7)>(1ULL << (i*2));

			if ( pState->bBreakOnRead[i] )
				ctx.Dr7 |= static_cast<decltype(ctx.Dr7)>(3ULL << (16 + i*4));
			else
				ctx.Dr7 |= static_cast<decltype(ctx.Dr7)>(1ULL << (16 + i*4));

			switch ( pState->nWatchBytes[i] )
			{
				case 1: ctx.Dr7 |= static_cast<decltype(ctx.Dr7)>(0ULL<<(18 + i*4)); break; //-V684
				case 2: ctx.Dr7 |= static_cast<decltype(ctx.Dr7)>(1ULL<<(18 + i*4)); break;
				case 4: ctx.Dr7 |= static_cast<decltype(ctx.Dr7)>(3ULL<<(18 + i*4)); break;
				case 8: ctx.Dr7 |= static_cast<decltype(ctx.Dr7)>(2ULL<<(18 + i*4)); break;
			}
		}
	}

	// Freeze this thread, adjust its breakpoint state
	HANDLE hThread = OpenThread( THREAD_SUSPEND_RESUME | THREAD_SET_CONTEXT, FALSE, dwThreadId );
	if ( hThread )
	{
		RunCodeAtScopeExit(CloseHandle( hThread ));

		// dimhotepus: x64 support.
		if ( SuspendThread( hThread ) != static_cast<DWORD>(-1) ) //-V720
		{
			RunCodeAtScopeExit(ResumeThread( hThread ));

			if ( !SetThreadContext( hThread, &ctx ) )
			{
				Warning( "Unable to set context for thread #%lu: %s. Hardware breakpoints will not be applied.\n",
					dwThreadId, std::system_category().message(::GetLastError()).c_str() );
			}
		}
		else
		{
			Warning( "Unable to suspend thread #%lu: %s. Hardware breakpoints will not be applied.\n",
				dwThreadId, std::system_category().message(::GetLastError()).c_str() );
		}
	}
	else
	{
		Warning( "Unable to open thread #%lu: %s. Hardware breakpoints will not be applied.\n",
			dwThreadId, std::system_category().message(::GetLastError()).c_str() );
	}
}

unsigned STDCALL ThreadProcX86_64SetDataBreakpoints( void* pvParam )
{
	// dimhotepus: Add thread name to aid debugging.
	ThreadSetDebugName("SetX86DataBreaks");

	if ( pvParam )
	{
		X86_64ApplyBreakpointsToThread( *static_cast<unsigned long*>(pvParam) );
		return 0;
	}

	// This function races against creation and destruction of new threads. Try to execute as quickly as possible.
	SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_HIGHEST );

	DWORD dwProcId = GetCurrentProcessId();
	DWORD dwThisThreadId = GetCurrentThreadId();

	if ( HANDLE hSnap = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
		 hSnap != INVALID_HANDLE_VALUE )
	{
		RunCodeAtScopeExit(CloseHandle( hSnap ));

		THREADENTRY32 threadEntry = {};
		// Thread32First/Thread32Next may adjust dwSize to be smaller. It's weird. Read the doc.
		const DWORD_PTR dwMinSize = (char*)(&threadEntry.th32OwnerProcessID + 1) - (char*)&threadEntry;

		threadEntry.dwSize = sizeof( THREADENTRY32 );

		BOOL bContinue = Thread32First( hSnap, &threadEntry );
		while ( bContinue )
		{
			if ( threadEntry.dwSize >= dwMinSize )
			{
				if ( threadEntry.th32OwnerProcessID == dwProcId && threadEntry.th32ThreadID != dwThisThreadId )
				{
					X86_64ApplyBreakpointsToThread( threadEntry.th32ThreadID );
				}
			}

			threadEntry.dwSize = sizeof( THREADENTRY32 );
			bContinue = Thread32Next( hSnap, &threadEntry );
		}
	}
	return 0;
}

}  // namespace

void Plat_SetHardwareDataBreakpoint( const void *pAddress, int nWatchBytes, bool bBreakOnRead )
{
	Assert( pAddress );
	Assert( nWatchBytes == 0 || nWatchBytes == 1 || nWatchBytes == 2 || nWatchBytes == 4 || nWatchBytes == 8 );

	AUTO_LOCK(s_BreakpointStateMutex);

	if ( nWatchBytes == 0 )
	{
		for ( int i = 0; i < 4; ++i )
		{
			if ( pAddress == s_BreakpointState.pAddress[i] )
			{
				for ( ; i < 3; ++i )
				{
					s_BreakpointState.pAddress[i] = s_BreakpointState.pAddress[i+1];
					s_BreakpointState.nWatchBytes[i] = s_BreakpointState.nWatchBytes[i+1];
					s_BreakpointState.bBreakOnRead[i] = s_BreakpointState.bBreakOnRead[i+1];
				}
				s_BreakpointState.pAddress[3] = nullptr;
				s_BreakpointState.nWatchBytes[3] = 0;
				s_BreakpointState.bBreakOnRead[3] = false;
				break;
			}
		}
	}
	else
	{
		// Replace first null entry or first existing entry at this address, or bump all entries down
		for ( int i = 0; i < 4; ++i )
		{
			if ( s_BreakpointState.pAddress[i] && s_BreakpointState.pAddress[i] != pAddress && i < 3 )
				continue;
		
			// Last iteration.

			if ( s_BreakpointState.pAddress[i] && s_BreakpointState.pAddress[i] != pAddress )
			{
				// Full up. Shift table down, drop least recently set
				for ( int j = 0; j < 3; ++j )
				{
					s_BreakpointState.pAddress[j] = s_BreakpointState.pAddress[j+1];
					s_BreakpointState.nWatchBytes[j] = s_BreakpointState.nWatchBytes[j+1];
					s_BreakpointState.bBreakOnRead[j] = s_BreakpointState.bBreakOnRead[j+1];
				}
			}
			s_BreakpointState.pAddress[i] = pAddress;
			s_BreakpointState.nWatchBytes[i] = static_cast<char>(nWatchBytes);
			s_BreakpointState.bBreakOnRead[i] = bBreakOnRead;
			break;
		}
	}
	

	HANDLE hWorkThread = (HANDLE)_beginthreadex( nullptr, 0,
		&ThreadProcX86_64SetDataBreakpoints, nullptr, 0, nullptr );
	if ( hWorkThread )
	{
		RunCodeAtScopeExit( CloseHandle( hWorkThread ) );

		WaitForSingleObject( hWorkThread, INFINITE );
	}
}

void Plat_ApplyHardwareDataBreakpointsToNewThread( unsigned long dwThreadID )
{
	AUTO_LOCK(s_BreakpointStateMutex);

	if ( dwThreadID != GetCurrentThreadId() )
	{
		X86_64ApplyBreakpointsToThread( dwThreadID );
	}
	else
	{
		HANDLE hWorkThread = (HANDLE)_beginthreadex( nullptr, 0,
			&ThreadProcX86_64SetDataBreakpoints, &dwThreadID, 0, nullptr );
		if ( hWorkThread )
		{
			RunCodeAtScopeExit( CloseHandle( hWorkThread ) );

			WaitForSingleObject( hWorkThread, INFINITE );
		}
	}
}

#else

void Plat_SetHardwareDataBreakpoint( const void *pAddress, int nWatchBytes, bool bBreakOnRead )
{
	// no impl on this platform yet
}

void Plat_ApplyHardwareDataBreakpointsToNewThread( unsigned long dwThreadID )
{
	// no impl on this platform yet
}

#endif
