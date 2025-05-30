//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include "FileChangeWatcher.h"
#include "tier1/utldict.h"
#include "filesystem_tools.h"


CFileChangeWatcher::CFileChangeWatcher()
{
	m_pCallbacks = NULL;
}

CFileChangeWatcher::~CFileChangeWatcher()
{
	Term();
}

void CFileChangeWatcher::Init( ICallbacks *pCallbacks )
{
	Term();
	m_pCallbacks = pCallbacks;
}

bool CFileChangeWatcher::AddDirectory( const char *pSearchPathBase, const char *pDirName, bool bRecursive )
{
	char fullDirName[MAX_PATH];
	V_ComposeFileName( pSearchPathBase, pDirName, fullDirName );
	
	HANDLE hDir = CreateFile( fullDirName, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED, NULL );
	if ( hDir == INVALID_HANDLE_VALUE )
	{
		Warning( "Unable to watch changes in the directory '%s': %s\n",
			fullDirName, std::system_category().message(::GetLastError()).c_str() );
		return false;
	}

	// Call this once to start the ball rolling.. Next time we call it, it'll tell us the changes that
	// have happened since this call.
	CDirWatch *pDirWatch = new CDirWatch;
	V_strcpy_safe( pDirWatch->m_SearchPathBase, pSearchPathBase );
	V_strcpy_safe( pDirWatch->m_DirName, pDirName );
	V_strcpy_safe( pDirWatch->m_FullDirName, fullDirName );
	pDirWatch->m_hDir = hDir;
	pDirWatch->m_hEvent = CreateEvent( NULL, false, false, NULL );
	memset( &pDirWatch->m_Overlapped, 0, sizeof( pDirWatch->m_Overlapped ) );
	pDirWatch->m_Overlapped.hEvent = pDirWatch->m_hEvent;
	if ( !CallReadDirectoryChanges( pDirWatch ) )
	{
		if (pDirWatch->m_hEvent)
		{
			CloseHandle(pDirWatch->m_hEvent);
		}

		CloseHandle( pDirWatch->m_hDir );
		delete pDirWatch;
		return false;
	}

	m_DirWatches.AddToTail( pDirWatch );
	return true;
}

void CFileChangeWatcher::Term()
{
	for ( auto *w : m_DirWatches )
	{
		if (w->m_hEvent)
		{
			CloseHandle( w->m_hEvent );
		}
		CloseHandle( w->m_hDir );
	}
	m_DirWatches.PurgeAndDeleteElements();
	m_pCallbacks = NULL;
}

int CFileChangeWatcher::Update()
{
	CUtlDict< int, int > queuedChanges;
	int nTotalChanges = 0;
	
	// Check each CDirWatch.
	int i = 0;
	while ( i < m_DirWatches.Count() )
	{
		CDirWatch *pDirWatch = m_DirWatches[i];
	
		DWORD dwBytes = 0;
		if ( GetOverlappedResult( pDirWatch->m_hDir, &pDirWatch->m_Overlapped, &dwBytes, FALSE ) )
		{
			// Read through the notifications.
			DWORD nBytesLeft = dwBytes;
			char *pCurPos = pDirWatch->m_Buffer;
			while ( nBytesLeft >= sizeof( FILE_NOTIFY_INFORMATION ) )
			{
				auto *pNotify = (FILE_NOTIFY_INFORMATION*)pCurPos;
			
				if ( m_pCallbacks )
				{
					// Figure out what happened to this file.
					WCHAR nullTerminated[2048];
					DWORD nBytesToCopy = min( pNotify->FileNameLength, 2047UL );
					memcpy( nullTerminated, pNotify->FileName, nBytesToCopy );
					nullTerminated[nBytesToCopy/2] = 0;

					char ansiFilename[1024];
					V_UnicodeToUTF8( nullTerminated, ansiFilename, sizeof( ansiFilename ) );
					
					// Now add it to the queue.	We use this queue because sometimes Windows will give us multiple
					// of the same modified notification back to back, and we want to reduce redundant calls.
					int iExisting = queuedChanges.Find( ansiFilename );
					if ( iExisting == queuedChanges.InvalidIndex() )
					{
						iExisting = queuedChanges.Insert( ansiFilename, 0 );
						++nTotalChanges;
					}
				}
			
				if ( pNotify->NextEntryOffset == 0 )
					break;

				pCurPos += pNotify->NextEntryOffset;
				nBytesLeft -= pNotify->NextEntryOffset;
			}
			
			CallReadDirectoryChanges( pDirWatch );
			continue;	// Check again because sometimes it queues up duplicate notifications.
		}

		// Process all the entries in the queue.
		for ( auto iQueuedChange=queuedChanges.First();
			iQueuedChange != queuedChanges.InvalidIndex();
			iQueuedChange=queuedChanges.Next( iQueuedChange ) )
		{
			SendNotification( pDirWatch, queuedChanges.GetElementName( iQueuedChange ) );
		}

		queuedChanges.Purge();
		++i;
	}
	
	return nTotalChanges;
}

void CFileChangeWatcher::SendNotification( CFileChangeWatcher::CDirWatch *pDirWatch, const char *pRelativeFilename )
{
	// Use this for full filenames although you don't strictly need it..
	char fullFilename[MAX_PATH];
	V_ComposeFileName( pDirWatch->m_FullDirName, pRelativeFilename, fullFilename );

	m_pCallbacks->OnFileChange( pRelativeFilename, fullFilename );
}

BOOL CFileChangeWatcher::CallReadDirectoryChanges( CFileChangeWatcher::CDirWatch *pDirWatch )
{
	return ReadDirectoryChangesW( pDirWatch->m_hDir, 
		pDirWatch->m_Buffer, 
		sizeof( pDirWatch->m_Buffer ), 
		true, 
		FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE, 
		NULL, 
		&pDirWatch->m_Overlapped, 
		NULL );
}



