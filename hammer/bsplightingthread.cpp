//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "bsplightingthread.h"

#include "tier0/threadtools.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



// --------------------------------------------------------------------------- //
// Global functions.
// --------------------------------------------------------------------------- //
IBSPLightingThread* CreateBSPLightingThread( IVRadDLL *pDLL )
{
	auto *pRet = new CBSPLightingThread;
	if ( pRet->Init( pDLL ) )
	{
		return pRet;
	}

	delete pRet;
	return 0;
}

static unsigned __stdcall ThreadMainLoop_Static( void* lpParameter )
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	// dimhotepus: Add thread name to simplify debugging.
	ThreadSetDebugName("BSPLighting");

	return ((CBSPLightingThread*)lpParameter)->ThreadMainLoop();
}


// --------------------------------------------------------------------------- //
// 
// --------------------------------------------------------------------------- //

CBSPLightingThread::CBSPLightingThread()
{
	m_pVRadDLL = nullptr;
	m_hThread = 0;
	
	m_ThreadCmd = THREADCMD_NONE;
	m_ThreadState = STATE_IDLE;
}


CBSPLightingThread::~CBSPLightingThread()
{
	if( m_hThread )
	{
		// Stop the current lighting process if one is going on.
		Interrupt();

		// Tell the thread to exit.
		SetThreadCmd( THREADCMD_EXIT );

		// Wait for the thread to exit.
		WaitForSingleObject( m_hThread, INFINITE );

		// Now we can close the thread handle.
		CloseHandle( m_hThread );
		m_hThread = NULL;
	}
}


void CBSPLightingThread::Release()
{
	delete this;
}


void CBSPLightingThread::StartLighting( char const *pVMFFileWithEntities )
{
	// First, kill any lighting going on.
	Interrupt();

	// Store the VMF file data for the thread.
	intp len = strlen( pVMFFileWithEntities ) + 1;
	m_VMFFileWithEntities.CopyArray( pVMFFileWithEntities, len );

	// Tell the thread to start lighting.
	SetThreadState( STATE_LIGHTING );
	SetThreadCmd( THREADCMD_LIGHT );
}


int CBSPLightingThread::GetCurrentState() const
{
	return GetThreadState();
}


void CBSPLightingThread::Interrupt()
{
	if( GetThreadState() == STATE_LIGHTING )
	{
		m_pVRadDLL->Interrupt();
		
		while( GetThreadState() == STATE_LIGHTING )		
			Sleep( 10 );
	}
}


float CBSPLightingThread::GetPercentComplete()
{
	return m_pVRadDLL->GetPercentComplete();
}


bool CBSPLightingThread::Init( IVRadDLL *pDLL )
{
	m_pVRadDLL = pDLL;

	m_hThread = (HANDLE)_beginthreadex(
		nullptr,
		0,
		ThreadMainLoop_Static,
		this,
		0,
		nullptr );

	if( !m_hThread )
		return false;

	SetThreadPriority( m_hThread, THREAD_PRIORITY_LOWEST );
	return true;
}


DWORD CBSPLightingThread::ThreadMainLoop()
{
	while( 1 )
	{
		int cmd = GetThreadCmd();

		if( cmd == THREADCMD_NONE )
		{
			// Keep waiting for a new command.
			Sleep( 10 );
		}
		else if( cmd == THREADCMD_LIGHT )
		{
			if( m_pVRadDLL->DoIncrementalLight( m_VMFFileWithEntities.Base() ) )
				SetThreadState( STATE_FINISHED );
			else
				SetThreadState( STATE_IDLE );
		}
		else if( cmd == THREADCMD_EXIT )
		{
			return 0;
		}
	}
}


int CBSPLightingThread::GetThreadCmd()
{
	return m_ThreadCmd.exchange(THREADCMD_NONE);
}


void CBSPLightingThread::SetThreadCmd( int cmd )
{
	m_ThreadCmd.store(cmd, std::memory_order::memory_order_relaxed);
}


int CBSPLightingThread::GetThreadState() const
{
	return m_ThreadState.load(std::memory_order::memory_order_relaxed);
}


void CBSPLightingThread::SetThreadState( int state )
{
	m_ThreadState.store(state, std::memory_order::memory_order_relaxed);
}


