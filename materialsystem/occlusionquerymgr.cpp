//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#include "pch_materialsystem.h"

#define MATSYS_INTERNAL

#include "occlusionquerymgr.h"
#include "imaterialsysteminternal.h"
#include "imatrendercontextinternal.h"

// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static COcclusionQueryMgr s_OcclusionQueryMgr;
COcclusionQueryMgr *g_pOcclusionQueryMgr = &s_OcclusionQueryMgr;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
COcclusionQueryMgr::COcclusionQueryMgr()
{
	m_nFrameCount = 0;
}


//-----------------------------------------------------------------------------
// Allocate and delete query objects.
//-----------------------------------------------------------------------------
OcclusionQueryObjectHandle_t COcclusionQueryMgr::CreateOcclusionQueryObject( )
{
	AUTO_LOCK(m_Mutex);

	intp h = m_OcclusionQueryObjects.AddToTail();
	return (OcclusionQueryObjectHandle_t)h;
}

void COcclusionQueryMgr::OnCreateOcclusionQueryObject( OcclusionQueryObjectHandle_t h )
{
	AUTO_LOCK(m_Mutex);

	for ( auto &qh : m_OcclusionQueryObjects[(intp)h].m_QueryHandle )
	{
		qh = g_pShaderAPI->CreateOcclusionQueryObject( );
	}
}

// Flushes an outstanding query
// HEY - Be very careful using this method - it causes a full pipeline flush/stall!
void COcclusionQueryMgr::FlushQuery( OcclusionQueryObjectHandle_t hOcclusionQuery, int nIndex )
{
	// Flush out any previous queries
	intp h = (intp)hOcclusionQuery;
	ShaderAPIOcclusionQuery_t hQuery = nullptr;
	
	{
		AUTO_LOCK( m_Mutex );

		const auto &object = m_OcclusionQueryObjects[h];
		if ( object.m_bHasBeenIssued[nIndex] )
		{
			hQuery = object.m_QueryHandle[nIndex];
		}
	}

	if ( hQuery )
	{
		// dimhotepus: Slow, may cause wait for a long.
		while ( OCCLUSION_QUERY_RESULT_PENDING == g_pShaderAPI->OcclusionQuery_GetNumPixelsRendered( hQuery, true ) )
			continue;
	}
}

void COcclusionQueryMgr::DestroyOcclusionQueryObject( OcclusionQueryObjectHandle_t hOcclusionQuery )
{
	intp h = (intp)hOcclusionQuery;

	AUTO_LOCK(m_Mutex);
	
	Assert( m_OcclusionQueryObjects.IsValidIndex( h ) );
	if ( m_OcclusionQueryObjects.IsValidIndex( h ) )
	{
		for ( auto &qh : m_OcclusionQueryObjects[h].m_QueryHandle )
		{
			if ( qh != INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE )
			{
				g_pShaderAPI->DestroyOcclusionQueryObject( qh );
			}
		}
		m_OcclusionQueryObjects.Remove( h );
	}
}


//-----------------------------------------------------------------------------
// Advance frame
//-----------------------------------------------------------------------------
void COcclusionQueryMgr::AdvanceFrame()
{
	++m_nFrameCount;
}


//-----------------------------------------------------------------------------
// Alt-tab support
// NOTE: This doesn't queue anything up
//-----------------------------------------------------------------------------
void COcclusionQueryMgr::AllocOcclusionQueryObjects( void )
{
	AUTO_LOCK(m_Mutex);

	FOR_EACH_LL( m_OcclusionQueryObjects, iterator )
	{
		auto &object = m_OcclusionQueryObjects[iterator];

		for ( int i = 0; i < COUNT_OCCLUSION_QUERY_STACK; i++)
		{
			object.m_QueryHandle[i] = g_pShaderAPI->CreateOcclusionQueryObject();
			object.m_bHasBeenIssued[i] = false;		// any in-flight queries are never returning
		}
	}
}

void COcclusionQueryMgr::FreeOcclusionQueryObjects( void )
{
	AUTO_LOCK(m_Mutex);

	FOR_EACH_LL( m_OcclusionQueryObjects, iterator )
	{
		auto &object = m_OcclusionQueryObjects[iterator];

		for ( int i = 0; i < COUNT_OCCLUSION_QUERY_STACK; i++)
		{
			if ( object.m_QueryHandle[i] != INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE )
			{
				g_pShaderAPI->DestroyOcclusionQueryObject( object.m_QueryHandle[i] );
				object.m_QueryHandle[i] = INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE;
				object.m_bHasBeenIssued[i] = false;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Used to make the handle think it's never had a successful query before
//-----------------------------------------------------------------------------
void COcclusionQueryMgr::ResetOcclusionQueryObject( OcclusionQueryObjectHandle_t hOcclusionQuery )
{
	intp h = (intp)hOcclusionQuery;

	AUTO_LOCK(m_Mutex);

	Assert( m_OcclusionQueryObjects.IsValidIndex( h ) );
	if ( m_OcclusionQueryObjects.IsValidIndex( h ) )
	{
		auto &object = m_OcclusionQueryObjects[h];

		// Forget we've issued any previous queries - there's no need to flush them.
		for ( auto &i : object.m_bHasBeenIssued )
		{
			i = false;
		}

		object.m_LastResult = -1;
		object.m_nFrameIssued = -1;
	}
}


//-----------------------------------------------------------------------------
// Bracket drawing with begin and end so that we can get counts next frame.
//-----------------------------------------------------------------------------
void COcclusionQueryMgr::BeginOcclusionQueryDrawing( OcclusionQueryObjectHandle_t hOcclusionQuery )
{
	intp h = (intp)hOcclusionQuery;

	m_Mutex.Lock();

	Assert( m_OcclusionQueryObjects.IsValidIndex( h ) );
	if ( m_OcclusionQueryObjects.IsValidIndex( h ) )
	{
		auto &object = m_OcclusionQueryObjects[h];

		int nCurrent = object.m_nCurrentIssue;
		ShaderAPIOcclusionQuery_t hQuery = object.m_QueryHandle[nCurrent];
		if ( hQuery != INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE )
		{
			// If it's been issued, but we haven't gotten a result when we polled last time,
			// try polling one last time, since we can't poll again after we issue again.
			if ( object.m_bHasBeenIssued[nCurrent] )
			{
				const int frameIssued = object.m_nFrameIssued;

				m_Mutex.Unlock();

				// dimhotepus: Slow, may cause wait for a long.
				int nPixels = g_pShaderAPI->OcclusionQuery_GetNumPixelsRendered( hQuery, false );
				if ( ( nPixels == OCCLUSION_QUERY_RESULT_PENDING ) && ( frameIssued == m_nFrameCount ) )
				{
					static int s_nWarnCount = 0;
					if ( s_nWarnCount++ < 5 )
					{
						DevWarning( "blocking issue in occlusion queries! Grab brian!\n" );
					}
				}

				while( !OCCLUSION_QUERY_FINISHED( nPixels ) )
				{
					// We're going to reuse this query, so issue a flush to force the query results to come back.
					nPixels = g_pShaderAPI->OcclusionQuery_GetNumPixelsRendered( hQuery, true );
				}

				AUTO_LOCK(m_Mutex);

				// dimhotepus: During unlocked mutex object can become stale, reestablish the one if any.
				Assert( m_OcclusionQueryObjects.IsValidIndex( h ) );
				if ( m_OcclusionQueryObjects.IsValidIndex( h ) )
				{
					auto &newObject = m_OcclusionQueryObjects[h];

					if ( nPixels >= 0 )
					{
						newObject.m_LastResult = nPixels;
					}
					newObject.m_bHasBeenIssued[newObject.m_nCurrentIssue] = false;
				}
			}
			else
			{
				m_Mutex.Unlock();
			}

			g_pShaderAPI->BeginOcclusionQueryDrawing( hQuery );
		}
		else
		{
			m_Mutex.Unlock();
		}
	}
	else
	{
		m_Mutex.Unlock();
	}
}


void COcclusionQueryMgr::EndOcclusionQueryDrawing( OcclusionQueryObjectHandle_t hOcclusionQuery )
{
	intp h = (intp)hOcclusionQuery;
	
	AUTO_LOCK(m_Mutex);

	Assert( m_OcclusionQueryObjects.IsValidIndex( h ) );
	if ( m_OcclusionQueryObjects.IsValidIndex( h ) )
	{
		auto &object = m_OcclusionQueryObjects[h];
		int nCurrent = object.m_nCurrentIssue;

		ShaderAPIOcclusionQuery_t hQuery = object.m_QueryHandle[nCurrent];
		if ( hQuery != INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE )
		{
			g_pShaderAPI->EndOcclusionQueryDrawing( hQuery );

			object.m_bHasBeenIssued[nCurrent] = true;
			object.m_nFrameIssued = m_nFrameCount;

			nCurrent = ( nCurrent + 1 ) % COUNT_OCCLUSION_QUERY_STACK;
			object.m_nCurrentIssue = nCurrent;
		}
	}
}


//-----------------------------------------------------------------------------
// Get the number of pixels rendered between begin and end on an earlier frame.
// Calling this in the same frame is a huge perf hit!
//-----------------------------------------------------------------------------
void COcclusionQueryMgr::OcclusionQuery_IssueNumPixelsRenderedQuery( OcclusionQueryObjectHandle_t hOcclusionQuery )
{
	intp h = (intp)hOcclusionQuery;

	m_Mutex.Lock();
	bool isLocked = true;

	Assert( m_OcclusionQueryObjects.IsValidIndex( h ) );
	if ( m_OcclusionQueryObjects.IsValidIndex( h ) )
	{
		for( int i = 0; i < COUNT_OCCLUSION_QUERY_STACK; i++ )
		{
			auto &object = m_OcclusionQueryObjects[h];

			int nIndex = ( object.m_nCurrentIssue + i ) % COUNT_OCCLUSION_QUERY_STACK;
			ShaderAPIOcclusionQuery_t hQuery = object.m_QueryHandle[nIndex];
			if ( hQuery != INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE && object.m_bHasBeenIssued[nIndex] )
			{
				if ( isLocked )
				{
					m_Mutex.Unlock();
				}

				// dimhotepus: Slow, may cause wait for a long.
				int nPixels = g_pShaderAPI->OcclusionQuery_GetNumPixelsRendered( hQuery );
				if ( nPixels == OCCLUSION_QUERY_RESULT_ERROR )
				{
					AUTO_LOCK(m_Mutex);

					// dimhotepus: During unlocked mutex object can become stale, reestablish the one if any.
					Assert( m_OcclusionQueryObjects.IsValidIndex( h ) );
					if ( m_OcclusionQueryObjects.IsValidIndex( h ) )
					{
						auto &newObject = m_OcclusionQueryObjects[h];

						const int newNIndex = ( newObject.m_nCurrentIssue + i ) % COUNT_OCCLUSION_QUERY_STACK;

						// In GL mode, it's possible for queries to fail (say when mat_queue_mode is toggled). In this case, just clear m_bHasBeenIssued and forget we ever issued this query.
						newObject.m_bHasBeenIssued[newNIndex] = false;
					}
				}
				else if ( nPixels >= 0 )
				{
					AUTO_LOCK(m_Mutex);

					// dimhotepus: During unlocked mutex object can become stale, reestablish the one if any.
					Assert( m_OcclusionQueryObjects.IsValidIndex( h ) );
					if ( m_OcclusionQueryObjects.IsValidIndex( h ) )
					{
						auto &newObject = m_OcclusionQueryObjects[h];

						const int newNIndex = ( newObject.m_nCurrentIssue + i ) % COUNT_OCCLUSION_QUERY_STACK;

						newObject.m_LastResult = nPixels;
						newObject.m_bHasBeenIssued[newNIndex] = false;
					}
				}

				m_Mutex.Lock();
				isLocked = true;
			}
		}
	}

	if ( isLocked )
	{
		m_Mutex.Unlock();
	}
}

int COcclusionQueryMgr::OcclusionQuery_GetNumPixelsRendered( OcclusionQueryObjectHandle_t h, bool bDoQuery )
{
	if ( bDoQuery )
	{
		OcclusionQuery_IssueNumPixelsRenderedQuery( h );
	}

	AUTO_LOCK(m_Mutex);

	int nPixels = m_OcclusionQueryObjects[(intp)h].m_LastResult;
	return nPixels;
}
