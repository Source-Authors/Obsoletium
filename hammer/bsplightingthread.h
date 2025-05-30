//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BSPLIGHTINGTHREAD_H
#define BSPLIGHTINGTHREAD_H
#ifdef _WIN32
#pragma once
#endif


#include "ibsplightingthread.h"
#include "tier1/utlvector.h"

#include <atomic>

class CBSPLightingThread : public IBSPLightingThread
{
public:
	CBSPLightingThread();
	virtual		~CBSPLightingThread();

// IBSPLightingThread functions.
	void		Release() override;
	void		StartLighting( char const *pVMFFileWithEntities ) override;
	int			GetCurrentState() const override;
	void		Interrupt() override;
	float		GetPercentComplete() override;


// Other functions.
public:

	// This is called immediately after the constructor. It creates the thread.
	bool				Init( IVRadDLL *pDLL );



// Threadsafe functions.
public:

	enum
	{
		THREADCMD_NONE=0,
		THREADCMD_LIGHT=1,
		THREADCMD_EXIT=2
	};

	// Get the current command to the thread. Resets to THREADCMD_NONE on exit.
	int					GetThreadCmd();
	void				SetThreadCmd( int cmd );

	// Returns an IBSPLightingThread::STATE_ define.
	int					GetThreadState() const;
	void				SetThreadState( int state );
	

public:

	// The thread's run function.
	DWORD				ThreadMainLoop();


public:

	std::atomic_int				m_ThreadCmd;		// Next command for the thread to run.
	std::atomic_int				m_ThreadState;		// Current state of the thread.

	CUtlVector<char>	m_VMFFileWithEntities;


public:
	IVRadDLL			*m_pVRadDLL;

	HANDLE				m_hThread;
};


#endif // BSPLIGHTINGTHREAD_H
