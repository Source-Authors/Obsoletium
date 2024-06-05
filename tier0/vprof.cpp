// Copyright Valve Corporation, All rights reserved.
//
// Purpose: Real-Time Hierarchical Profiling

#include "pch_tier0.h"

#include "tier0/memalloc.h"
#include "tier0/valve_off.h"

#ifdef _WIN32
#pragma warning(disable:4073)
#pragma init_seg( lib )
#endif

#include <map>
#include <vector>
#include <algorithm>

#include "tier0/valve_on.h"
#include "tier0/vprof.h"
#include "tier0/l2cache.h"
#include "tier0/tslist.h"
#include "tier0/dynfunction.h"

#include "tier0/memdbgon.h"

// NOTE: Explicitly and intentionally using STL in here to not generate any
// cyclical dependencies between the low-level debug library and the higher
// level data structures (toml 01-27-03)
using namespace std;

#ifdef VPROF_ENABLED

//-----------------------------------------------------------------------------
bool g_VProfSignalSpike;

//-----------------------------------------------------------------------------

CVProfile g_VProfCurrentProfile;

int CVProfNode::s_iCurrentUniqueNodeID = 0;

CVProfNode::~CVProfNode()
{
#if !defined( _WIN32 ) && !defined( POSIX )
	delete m_pChild;
	delete m_pSibling;
#endif
}

CVProfNode *CVProfNode::GetSubNode( const tchar *pszName, int detailLevel, const tchar *pBudgetGroupName, int budgetFlags )
{
	// Try to find this sub node
	CVProfNode * child = m_pChild;
	while ( child ) 
	{
		if ( child->m_pszName == pszName ) 
		{
			return child;
		}
		child = child->m_pSibling;
	}

	// We didn't find it, so add it
	CVProfNode * node = new CVProfNode( pszName, detailLevel, this, pBudgetGroupName, budgetFlags );
	node->m_pSibling = m_pChild;
	m_pChild = node;
	return node;
}

CVProfNode *CVProfNode::GetSubNode( const tchar *pszName, int detailLevel, const tchar *pBudgetGroupName )
{
	return GetSubNode( pszName, detailLevel, pBudgetGroupName, BUDGETFLAG_OTHER );
}

//-------------------------------------

void CVProfNode::EnterScope()
{
	m_nCurFrameCalls++;
	if ( m_nRecursions++ == 0 ) 
	{
		m_Timer.Start();

		if ( g_VProfCurrentProfile.UsePME() )
		{
			m_L2Cache.Start();
		}

#ifdef VPROF_VTUNE_GROUP
		g_VProfCurrentProfile.PushGroup( m_BudgetGroupID );
#endif
	}
}

//-------------------------------------

bool CVProfNode::ExitScope()
{
	if ( --m_nRecursions == 0 && m_nCurFrameCalls != 0 ) 
	{
		m_Timer.End();
		m_CurFrameTime += m_Timer.GetDuration();

		if ( g_VProfCurrentProfile.UsePME() )
		{
			m_L2Cache.End();
			m_iCurL2CacheMiss += m_L2Cache.GetL2CacheMisses();
		}

#ifdef VPROF_VTUNE_GROUP
		g_VProfCurrentProfile.PopGroup();
#endif
	}
	return ( m_nRecursions == 0 );
}

//-------------------------------------

void CVProfNode::Pause()
{
	if ( m_nRecursions > 0 ) 
	{
		m_Timer.End();
		m_CurFrameTime += m_Timer.GetDuration();

		if ( g_VProfCurrentProfile.UsePME() )
		{
			m_L2Cache.End();
			m_iCurL2CacheMiss += m_L2Cache.GetL2CacheMisses();
		}
	}
	if ( m_pChild ) 
	{
		m_pChild->Pause();
	}
	if ( m_pSibling ) 
	{
		m_pSibling->Pause();
	}
}

//-------------------------------------

void CVProfNode::Resume()
{
	if ( m_nRecursions > 0 ) 
	{
		m_Timer.Start();

		if ( g_VProfCurrentProfile.UsePME() )
		{
			m_L2Cache.Start();
		}
	}
	if ( m_pChild ) 
	{
		m_pChild->Resume();
	}
	if ( m_pSibling ) 
	{
		m_pSibling->Resume();
	}
}

//-------------------------------------

void CVProfNode::Reset()
{
	m_nPrevFrameCalls = 0;
	m_PrevFrameTime.Init();

	m_nCurFrameCalls = 0;
	m_CurFrameTime.Init();
	
	m_nTotalCalls = 0;
	m_TotalTime.Init();
	
	m_PeakTime.Init();

	m_iPrevL2CacheMiss = 0;
	m_iCurL2CacheMiss = 0;
	m_iTotalL2CacheMiss = 0;

	if ( m_pChild ) 
	{
		m_pChild->Reset();
	}
	if ( m_pSibling ) 
	{
		m_pSibling->Reset();
	}
}


//-------------------------------------

void CVProfNode::MarkFrame()
{
	m_nPrevFrameCalls = m_nCurFrameCalls;
	m_PrevFrameTime = m_CurFrameTime;
	m_iPrevL2CacheMiss = m_iCurL2CacheMiss;
	m_nTotalCalls += m_nCurFrameCalls;
	m_TotalTime += m_CurFrameTime;
	
	if ( m_PeakTime.IsLessThan( m_CurFrameTime ) )
	{
		m_PeakTime = m_CurFrameTime;
	}
	
	m_CurFrameTime.Init();
	m_nCurFrameCalls = 0;
	m_iTotalL2CacheMiss += m_iCurL2CacheMiss;
	m_iCurL2CacheMiss = 0;

	if ( m_pChild ) 
	{
		m_pChild->MarkFrame();
	}
	if ( m_pSibling ) 
	{
		m_pSibling->MarkFrame();
	}
}

//-------------------------------------

void CVProfNode::ResetPeak()
{
	m_PeakTime.Init();

	if ( m_pChild ) 
	{
		m_pChild->ResetPeak();
	}
	if ( m_pSibling ) 
	{
		m_pSibling->ResetPeak();
	}
}


void CVProfNode::SetCurFrameTime( unsigned long milliseconds )
{
	m_CurFrameTime.Init( (float)milliseconds );
}
#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
void CVProfNode::Validate( CValidator &validator, tchar *pchName )
{
	validator.Push( _T("CVProfNode"), this, pchName );

	m_L2Cache.Validate( validator, _T("m_L2Cache") );

	if ( m_pSibling )
		m_pSibling->Validate( validator, _T("m_pSibling") );
	if ( m_pChild )
		m_pChild->Validate( validator, _T("m_pChild") );

	validator.Pop( );
}
#endif // DBGFLAG_VALIDATE



//-----------------------------------------------------------------------------

struct TimeSums_t
{
	const tchar *pszProfileScope;
	int			calls;
	double 		time;
	double 		timeLessChildren;
	double		peak;
};

static bool TimeCompare( const TimeSums_t &lhs, const TimeSums_t &rhs )
{
	return ( lhs.time > rhs.time );
}

static bool TimeLessChildrenCompare( const TimeSums_t &lhs, const TimeSums_t &rhs )
{
	return ( lhs.timeLessChildren > rhs.timeLessChildren );
}

static bool PeakCompare( const TimeSums_t &lhs, const TimeSums_t &rhs )
{
	return ( lhs.peak > rhs.peak );
}

static bool AverageTimeCompare( const TimeSums_t &lhs, const TimeSums_t &rhs )
{
	double avgLhs = ( lhs.calls ) ? lhs.time / (double)lhs.calls : 0.0;
	double avgRhs = ( rhs.calls ) ? rhs.time / (double)rhs.calls : 0.0;
	return ( avgLhs > avgRhs );
}

static bool AverageTimeLessChildrenCompare( const TimeSums_t &lhs, const TimeSums_t &rhs )
{
	double avgLhs = ( lhs.calls ) ? lhs.timeLessChildren / (double)lhs.calls : 0.0;
	double avgRhs = ( rhs.calls ) ? rhs.timeLessChildren / (double)rhs.calls : 0.0;
	return ( avgLhs > avgRhs );

}
static bool PeakOverAverageCompare( const TimeSums_t &lhs, const TimeSums_t &rhs )
{
	double avgLhs = ( lhs.calls ) ? lhs.time / (double)lhs.calls : 0.0;
	double avgRhs = ( rhs.calls ) ? rhs.time / (double)rhs.calls : 0.0;
	
	double lhsPoA = ( avgLhs != 0 ) ? lhs.peak / avgLhs : 0.0;
	double rhsPoA = ( avgRhs != 0 ) ? rhs.peak / avgRhs : 0.0;
	
	return ( lhsPoA > rhsPoA );
}

map<CVProfNode *, double> 	g_TimesLessChildren;
int							g_TotalFrames;
map<const tchar *, size_t> g_TimeSumsMap;
vector<TimeSums_t> 			g_TimeSums;
CVProfNode *				g_pStartNode;
const tchar *				g_pszSumNode;

//-------------------------------------

void CVProfile::SumTimes( CVProfNode *pNode, int budgetGroupID )
{
	if ( !pNode )
		return; // this generally only happens on a failed FindNode()

	bool bSetStartNode;
	if ( !g_pStartNode && _tcscmp( pNode->GetName(), g_pszSumNode ) == 0 )
	{
		g_pStartNode = pNode;
		bSetStartNode = true;
	}
	else
		bSetStartNode = false;

	if ( GetRoot() != pNode )
	{
		if ( g_pStartNode && pNode->GetTotalCalls() > 0 && ( budgetGroupID == -1 || pNode->GetBudgetGroupID() == budgetGroupID ) )
		{
			double timeLessChildren = pNode->GetTotalTimeLessChildren();
			
			g_TimesLessChildren.emplace( pNode, timeLessChildren );
			
			map<const tchar *, size_t>::iterator iter;
			iter = g_TimeSumsMap.find( pNode->GetName() ); // intenionally using address of string rather than string compare (toml 01-27-03)
			if ( iter == g_TimeSumsMap.end() )
			{
				TimeSums_t timeSums = { pNode->GetName(), pNode->GetTotalCalls(), pNode->GetTotalTime(), timeLessChildren, pNode->GetPeakTime() };
				g_TimeSumsMap.emplace( pNode->GetName(), g_TimeSums.size() );
				g_TimeSums.push_back( timeSums );
			}
			else
			{
				TimeSums_t &timeSums = g_TimeSums[iter->second];
				timeSums.calls += pNode->GetTotalCalls();
				timeSums.time += pNode->GetTotalTime();
				timeSums.timeLessChildren += timeLessChildren;
				if ( pNode->GetPeakTime() > timeSums.peak )
					timeSums.peak = pNode->GetPeakTime();
			}
		}
			
		if( ( !g_pStartNode || pNode != g_pStartNode ) && pNode->GetSibling() )
		{
			SumTimes( pNode->GetSibling(), budgetGroupID );
		}
	}
	
	if( pNode->GetChild() )
	{
		SumTimes( pNode->GetChild(), budgetGroupID );
	}
		
	if ( bSetStartNode )
		g_pStartNode = NULL;
}

//-------------------------------------

CVProfNode *CVProfile::FindNode( CVProfNode *pStartNode, const tchar *pszNode )
{
	if ( _tcscmp( pStartNode->GetName(), pszNode ) != 0 )
	{
		CVProfNode *pFoundNode = NULL;
		if ( pStartNode->GetSibling() )
		{
			pFoundNode = FindNode( pStartNode->GetSibling(), pszNode );
		}

		if ( !pFoundNode && pStartNode->GetChild() )
		{
			pFoundNode = FindNode( pStartNode->GetChild(), pszNode );
		}

		return pFoundNode;
	}
	return pStartNode;
}

void CVProfile::SumTimes( const tchar *pszStartNode, int budgetGroupID )
{
	if ( GetRoot()->GetChild() )
	{
		if ( pszStartNode == NULL )
			g_pStartNode = GetRoot();
		else
			g_pStartNode = NULL;

		g_pszSumNode = pszStartNode;
		SumTimes( GetRoot(), budgetGroupID );
		g_pStartNode = NULL;
	}

}

//-------------------------------------

// This array lets us generate the commonly used indent levels
// without looping. That then lets us print our vprof nodes
// in a single call, which is more efficient and works better
// with output streams like ETW where each call represents a
// 'line' of text. Indent levels beyond what is represented
// in this array are, regretfully, clamped, however the highest
// indent level seen in testing was 10.
static const char* s_indentText[] =
{
	"",											// 0
	"",											// 1
	"|  ",										// 2
	"|  |  ",									// 3
	"|  |  |  ",								// 4
	"|  |  |  |  ",								// 5
	"|  |  |  |  |  ",							// 6
	"|  |  |  |  |  |  ",						// 7
	"|  |  |  |  |  |  |  ",					// 8
	"|  |  |  |  |  |  |  |  ",					// 9
	"|  |  |  |  |  |  |  |  |  ",				// 10
	"|  |  |  |  |  |  |  |  |  |  ",			// 11
	"|  |  |  |  |  |  |  |  |  |  |  ",		// 12
	"|  |  |  |  |  |  |  |  |  |  |  |  ",		// 13
	"|  |  |  |  |  |  |  |  |  |  |  |  |  ",	// 14
};

void CVProfile::DumpNodes( CVProfNode *pNode, int indent, bool bAverageAndCountOnly )
{
	if ( !pNode )
		return; // this generally only happens on a failed FindNode()

	bool fIsRoot = ( pNode == GetRoot() );

	if ( fIsRoot || pNode == g_pStartNode )
	{
		if( bAverageAndCountOnly )
		{
			m_pOutputStream( _T(" Avg Time/Frame (ms)\n") );
			m_pOutputStream( _T("[ func+child   func ]     Count\n") );
			m_pOutputStream( _T("  ---------- ------      ------\n") );
		}
		else
		{
			m_pOutputStream( _T("       Sum (ms)         Avg Time/Frame (ms)     Avg Time/Call (ms)\n") );
			m_pOutputStream( _T("[ func+child   func ]  [ func+child   func ]  [ func+child   func ]  Count   Peak\n") );
			m_pOutputStream( _T("  ---------- ------      ---------- ------      ---------- ------   ------ ------\n") );
		}
	}

	if ( !fIsRoot )
	{
		map<CVProfNode *, double>::iterator iterTimeLessChildren = g_TimesLessChildren.find( pNode );
		
		indent = Max( indent, 0 );
		indent = Min( indent, (int)ssize( s_indentText ) - 1 );
		const char* indentText = s_indentText[ indent ];
		double dNodeTime = 0;
		if(iterTimeLessChildren != g_TimesLessChildren.end())
			dNodeTime = iterTimeLessChildren->second;

		if( bAverageAndCountOnly )
		{
			m_pOutputStream( _T("  %10.3f %6.2f      %6d  %s%s\n"), 
						 ( pNode->GetTotalCalls() > 0 ) ? pNode->GetTotalTime() / (double)NumFramesSampled() : 0, 
						 ( pNode->GetTotalCalls() > 0 ) ? dNodeTime / (double)NumFramesSampled() : 0, 
						 pNode->GetTotalCalls(), indentText, pNode->GetName() );
		}
		else
		{
			m_pOutputStream( _T("  %10.3f %6.2f      %10.3f %6.2f      %10.3f %6.2f   %6d %6.2f  %s%s\n"), 
						 pNode->GetTotalTime(), dNodeTime,
						 ( pNode->GetTotalCalls() > 0 ) ? pNode->GetTotalTime() / (double)NumFramesSampled() : 0, 
						 ( pNode->GetTotalCalls() > 0 ) ? dNodeTime / (double)NumFramesSampled() : 0, 
						 ( pNode->GetTotalCalls() > 0 ) ? pNode->GetTotalTime() / (double)pNode->GetTotalCalls() : 0, 
						 ( pNode->GetTotalCalls() > 0 ) ? dNodeTime / (double)pNode->GetTotalCalls() : 0, 
						 pNode->GetTotalCalls(), pNode->GetPeakTime(), indentText, pNode->GetName() );
		}
	}

	if( pNode->GetChild() )
	{
		DumpNodes( pNode->GetChild(), indent + 1, bAverageAndCountOnly );
	}
	
	if( !( fIsRoot || pNode == g_pStartNode ) && pNode->GetSibling() )
	{
		DumpNodes( pNode->GetSibling(), indent, bAverageAndCountOnly );
	}
}

static void DumpSorted( CVProfile::StreamOut_t outputStream, const tchar *pszHeading, double totalTime, bool (*pfnSort)( const TimeSums_t &, const TimeSums_t & ), int maxLen = 999999 )
{
	unsigned i;
	vector<TimeSums_t> sortedSums;
	sortedSums = g_TimeSums;
	sort( sortedSums.begin(), sortedSums.end(), pfnSort );

	outputStream( _T("%s\n"), pszHeading);
    outputStream( _T("  Scope                                                      Calls Calls/Frame  Time+Child    Pct        Time    Pct   Avg/Frame    Avg/Call Avg-NoChild        Peak\n"));
    outputStream( _T("  ---------------------------------------------------- ----------- ----------- ----------- ------ ----------- ------ ----------- ----------- ----------- -----------\n"));
    for ( i = 0; i < sortedSums.size() && i < (unsigned)maxLen; i++ )
    {
		double avg = ( sortedSums[i].calls ) ? sortedSums[i].time / (double)sortedSums[i].calls : 0.0;
		double avgLessChildren = ( sortedSums[i].calls ) ? sortedSums[i].timeLessChildren / (double)sortedSums[i].calls : 0.0;
		
        outputStream( _T("  %52.52s%12d%12.3f%12.3f%7.2f%12.3f%7.2f%12.3f%12.3f%12.3f%12.3f\n"), 
             sortedSums[i].pszProfileScope,
             sortedSums[i].calls,
			 (float)sortedSums[i].calls / (float)g_TotalFrames,
			 sortedSums[i].time,
			 min( ( sortedSums[i].time / totalTime ) * 100.0, 100.0 ),
			 sortedSums[i].timeLessChildren,
			 min( ( sortedSums[i].timeLessChildren / totalTime ) * 100.0, 100.0 ),
			 sortedSums[i].time / (float)g_TotalFrames,
			 avg,
			 avgLessChildren,
			 sortedSums[i].peak );
	}
}

void CVProfile::SetOutputStream( StreamOut_t outputStream )
{
	if ( outputStream != NULL )
		m_pOutputStream = outputStream;
	else
		m_pOutputStream = Msg;
}

//-------------------------------------

void CVProfile::OutputReport( int type, const tchar *pszStartNode, int budgetGroupID )
{
	m_pOutputStream( _T("******** BEGIN VPROF REPORT ********\n"));

	g_TotalFrames = max( NumFramesSampled() - 1, 1 );
	
	if ( NumFramesSampled() == 0 || GetTotalTimeSampled() == 0)
		m_pOutputStream( _T("No samples\n") );
	else
	{
		if ( type & VPRT_SUMMARY )
		{
			m_pOutputStream( _T("-- Summary --\n") );
			m_pOutputStream( _T("%d frames sampled for %.2f seconds\n"), g_TotalFrames, GetTotalTimeSampled() / 1000.0 );
			m_pOutputStream( _T("Average %.2f fps, %.2f ms per frame\n"), 1000.0 / ( GetTotalTimeSampled() / g_TotalFrames ), GetTotalTimeSampled() / g_TotalFrames );
			m_pOutputStream( _T("Peak %.2f ms frame\n"), GetPeakFrameTime() );
			
			double timeAccountedFor = 100.0 - ( m_Root.GetTotalTimeLessChildren() / m_Root.GetTotalTime() );
			m_pOutputStream( _T("%.0f pct of time accounted for\n"), min( 100.0, timeAccountedFor ) );
			m_pOutputStream( _T("\n") );
		}

		if ( pszStartNode == NULL )
		{
			pszStartNode = GetRoot()->GetName();
		}

		SumTimes( pszStartNode, budgetGroupID );
		
		// Dump the hierarchy
		if ( type & VPRT_HIERARCHY )
		{
			m_pOutputStream( _T("-- Hierarchical Call Graph --\n"));
			if ( pszStartNode == NULL )
				g_pStartNode = NULL;
			else
				g_pStartNode = FindNode( GetRoot(), pszStartNode );

			DumpNodes( (!g_pStartNode) ? GetRoot() : g_pStartNode, 0, false );
			m_pOutputStream( _T("\n") );
		}
		
		if ( type & VPRT_HIERARCHY_TIME_PER_FRAME_AND_COUNT_ONLY )
		{
			m_pOutputStream( _T("-- Hierarchical Call Graph --\n"));
			if ( pszStartNode == NULL )
				g_pStartNode = NULL;
			else
				g_pStartNode = FindNode( GetRoot(), pszStartNode );

			DumpNodes( (!g_pStartNode) ? GetRoot() : g_pStartNode, 0, true );
			m_pOutputStream( _T("\n") );
		}

		int maxLen = ( type & VPRT_LIST_TOP_ITEMS_ONLY ) ? 25 : 999999;

		if ( type & VPRT_LIST_BY_TIME )
		{
			DumpSorted( m_pOutputStream, _T("-- Profile scopes sorted by time (including children) --"), GetTotalTimeSampled(), TimeCompare, maxLen );
			m_pOutputStream( _T("\n") );
		}
		if ( type & VPRT_LIST_BY_TIME_LESS_CHILDREN )
		{
			DumpSorted( m_pOutputStream, _T("-- Profile scopes sorted by time (without children) --"), GetTotalTimeSampled(), TimeLessChildrenCompare, maxLen );
			m_pOutputStream( _T("\n") );
		}
		if ( type & VPRT_LIST_BY_AVG_TIME )
		{
			DumpSorted( m_pOutputStream, _T("-- Profile scopes sorted by average time (including children) --"), GetTotalTimeSampled(), AverageTimeCompare, maxLen );
			m_pOutputStream( _T("\n") );
		}
		if ( type & VPRT_LIST_BY_AVG_TIME_LESS_CHILDREN )
		{
			DumpSorted( m_pOutputStream, _T("-- Profile scopes sorted by average time (without children) --"), GetTotalTimeSampled(), AverageTimeLessChildrenCompare, maxLen );
			m_pOutputStream( _T("\n") );
		}
		if ( type & VPRT_LIST_BY_PEAK_TIME )
		{
			DumpSorted( m_pOutputStream, _T("-- Profile scopes sorted by peak --"), GetTotalTimeSampled(), PeakCompare, maxLen);
			m_pOutputStream( _T("\n") );
		}
		if ( type & VPRT_LIST_BY_PEAK_OVER_AVERAGE )
		{
			DumpSorted( m_pOutputStream, _T("-- Profile scopes sorted by peak over average (including children) --"), GetTotalTimeSampled(), PeakOverAverageCompare, maxLen );
			m_pOutputStream( _T("\n") );
		}
		
		// TODO: Functions by time less children
		// TODO: Functions by time averages
		// TODO: Functions by peak
		// TODO: Peak deviation from average
		g_TimesLessChildren.clear();
		g_TimeSumsMap.clear();
		g_TimeSums.clear();
	}
	m_pOutputStream( _T("******** END VPROF REPORT ********\n"));

}

//=============================================================================

CVProfile::CVProfile() 
 :	m_enabled( 0 ),
	m_fAtRoot( true ),
	m_pCurNode( nullptr ),
	m_Root( _T("Root"), 0, NULL, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, 0 ),
 	m_nFrames( 0 ),
 	m_pausedEnabledDepth( 0 ),
	m_pOutputStream( Msg )
{
	m_pCurNode = &m_Root;

#ifdef VPROF_VTUNE_GROUP
	m_GroupIDStackDepth = 1;
	m_GroupIDStack[0] = 0; // VPROF_BUDGETGROUP_OTHER_UNACCOUNTED
#endif

	m_TargetThreadId = ThreadGetCurrentId();
	
	// Go ahead and allocate 32 slots for budget group names
	MEM_ALLOC_CREDIT();
	m_pBudgetGroups = new CVProfile::CBudgetGroup[32];
	m_nBudgetGroupNames = 0;
	m_nBudgetGroupNamesAllocated = 32;

	// Add these here so that they will always be in the same order.
	// VPROF_BUDGETGROUP_OTHER_UNACCOUNTED has to be FIRST!!!!
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_OTHER_UNACCOUNTED,		BUDGETFLAG_OTHER | BUDGETFLAG_SERVER );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_WORLD_RENDERING,			BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_DISPLACEMENT_RENDERING,	BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_GAME,						BUDGETFLAG_OTHER | BUDGETFLAG_SERVER );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_PLAYER,					BUDGETFLAG_OTHER | BUDGETFLAG_SERVER );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_NPCS,						BUDGETFLAG_OTHER | BUDGETFLAG_SERVER );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_SERVER_ANIM,				BUDGETFLAG_OTHER | BUDGETFLAG_SERVER );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_CLIENT_ANIMATION,			BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_PHYSICS,					BUDGETFLAG_OTHER | BUDGETFLAG_SERVER );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_STATICPROP_RENDERING,		BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_MODEL_RENDERING,			BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_MODEL_FAST_PATH_RENDERING,BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_LIGHTCACHE,				BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_BRUSHMODEL_RENDERING,		BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_SHADOW_RENDERING,			BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_DETAILPROP_RENDERING,		BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_PARTICLE_RENDERING,		BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_ROPES,					BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_DLIGHT_RENDERING,			BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_OTHER_NETWORKING,			BUDGETFLAG_OTHER | BUDGETFLAG_SERVER );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_OTHER_SOUND,				BUDGETFLAG_OTHER | BUDGETFLAG_SERVER );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_OTHER_VGUI,				BUDGETFLAG_OTHER | BUDGETFLAG_SERVER );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_OTHER_FILESYSTEM,			BUDGETFLAG_OTHER | BUDGETFLAG_SERVER );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_PREDICTION,				BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_INTERPOLATION,			BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_SWAP_BUFFERS,				BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_OCCLUSION,				BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_OVERLAYS,					BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_TOOLS,					BUDGETFLAG_OTHER | BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_TEXTURE_CACHE,			BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_REPLAY,					BUDGETFLAG_SERVER );
//	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_DISP_HULLTRACES );

	m_bPMEInit = false;
	m_bPMEEnabled = false;
}


CVProfile::~CVProfile()
{
	Term();
}


void CVProfile::FreeNodes_R( CVProfNode *pNode )
{
	CVProfNode *pNext;
	for ( CVProfNode *pChild = pNode->GetChild(); pChild; pChild = pNext )
	{
		pNext = pChild->GetSibling();
		FreeNodes_R( pChild );
	}
	
	if ( pNode == GetRoot() )
	{
		pNode->m_pChild = NULL;
	}
	else
	{
		delete pNode;
	}
}


void CVProfile::Term()
{
	int i;
	for( i = 0; i < m_nBudgetGroupNames; i++ )
	{
		delete [] m_pBudgetGroups[i].m_pName;
	}
	delete[] m_pBudgetGroups;
	m_nBudgetGroupNames = m_nBudgetGroupNamesAllocated = 0;
	m_pBudgetGroups = NULL;

	int n;
	for( n = 0; n < m_NumCounters; n++ )
	{
		delete [] m_CounterNames[n];
		m_CounterNames[n] = NULL;
	}
	m_NumCounters = 0;

	// Free the nodes.
	if ( GetRoot() )
	{
		FreeNodes_R( GetRoot() );
	}
}


#define COLORMIN 160
#define COLORMAX 255

static int g_ColorLookup[4] = 
{
	COLORMIN, 
	COLORMAX,
	COLORMIN+(COLORMAX-COLORMIN)/3,
	COLORMIN+((COLORMAX-COLORMIN)*2)/3, 
};

#define GET_BIT( val, bitnum ) ( ( (val) >> (bitnum) ) & 0x1 )

void CVProfile::GetBudgetGroupColor( int budgetGroupID, int &r, int &g, int &b, int &a )
{
	budgetGroupID = budgetGroupID % ( 1 << 6 );

	int index;
	index = GET_BIT( budgetGroupID, 0 ) | ( GET_BIT( budgetGroupID, 5 ) << 1 );
	r = g_ColorLookup[index];
	index = GET_BIT( budgetGroupID, 1 ) | ( GET_BIT( budgetGroupID, 4 ) << 1 );
	g = g_ColorLookup[index];
	index = GET_BIT( budgetGroupID, 2 ) | ( GET_BIT( budgetGroupID, 3 ) << 1 );
	b = g_ColorLookup[index];
	a = 255;
}

// return -1 if it doesn't exist.
int CVProfile::FindBudgetGroupName( const tchar *pBudgetGroupName )
{
	int i;
	for( i = 0; i < m_nBudgetGroupNames; i++ )
	{
		if( _tcsicmp( pBudgetGroupName, m_pBudgetGroups[i].m_pName ) == 0 )
		{
			return i;
		}
	}
	return -1;
}

int CVProfile::AddBudgetGroupName( const tchar *pBudgetGroupName, int budgetFlags )
{
	MEM_ALLOC_CREDIT();
	tchar *pNewString = new tchar[ _tcslen( pBudgetGroupName ) + 1 ];
	_tcscpy( pNewString, pBudgetGroupName );
	if( m_nBudgetGroupNames + 1 > m_nBudgetGroupNamesAllocated )
	{
		m_nBudgetGroupNamesAllocated *= 2;
		m_nBudgetGroupNamesAllocated = max( m_nBudgetGroupNames + 6, m_nBudgetGroupNamesAllocated );
		
		CBudgetGroup *pNew = new CBudgetGroup[ m_nBudgetGroupNamesAllocated ];
		for ( int i=0; i < m_nBudgetGroupNames; i++ )
			pNew[i] = m_pBudgetGroups[i];
		
		delete [] m_pBudgetGroups;
		m_pBudgetGroups = pNew;
	}

	m_pBudgetGroups[m_nBudgetGroupNames].m_pName = pNewString;
	m_pBudgetGroups[m_nBudgetGroupNames].m_BudgetFlags = budgetFlags;
	m_nBudgetGroupNames++;
	if( m_pNumBudgetGroupsChangedCallBack )
	{
		(*m_pNumBudgetGroupsChangedCallBack)();
	}
	return m_nBudgetGroupNames - 1;
}

int CVProfile::BudgetGroupNameToBudgetGroupID( const tchar *pBudgetGroupName, int budgetFlagsToORIn )
{
	int budgetGroupID = FindBudgetGroupName( pBudgetGroupName );
	if( budgetGroupID == -1 )
	{
		budgetGroupID = AddBudgetGroupName( pBudgetGroupName, budgetFlagsToORIn );
	}
	else
	{
		m_pBudgetGroups[budgetGroupID].m_BudgetFlags |= budgetFlagsToORIn;
	}

	return budgetGroupID;
}

int CVProfile::BudgetGroupNameToBudgetGroupID( const tchar *pBudgetGroupName )
{
	return BudgetGroupNameToBudgetGroupID( pBudgetGroupName, BUDGETFLAG_OTHER );
}

int CVProfile::GetNumBudgetGroups( void )
{
	return m_nBudgetGroupNames;
}

void CVProfile::RegisterNumBudgetGroupsChangedCallBack( void (*pCallBack)(void) )
{
	m_pNumBudgetGroupsChangedCallBack = pCallBack;
}

void CVProfile::HideBudgetGroup( int budgetGroupID, bool bHide )
{
	if( budgetGroupID != -1 )
	{
		if ( bHide )
			m_pBudgetGroups[budgetGroupID].m_BudgetFlags |= BUDGETFLAG_HIDDEN;
		else
			m_pBudgetGroups[budgetGroupID].m_BudgetFlags &= ~BUDGETFLAG_HIDDEN;
	}
}

intp *CVProfile::FindOrCreateCounter( const tchar *pName, CounterGroup_t eCounterGroup )
{	
	Assert( m_NumCounters+1 < MAXCOUNTERS );
	if ( m_NumCounters + 1 >= MAXCOUNTERS || !InTargetThread() )
	{
		static intp dummy;
		return &dummy;
	}
	int i;
	for( i = 0; i < m_NumCounters; i++ )
	{
		if( _tcsicmp( m_CounterNames[i], pName ) == 0 )
		{
			// found it!
			return &m_Counters[i];
		}
	}

	// NOTE: These get freed in ~CVProfile.
	MEM_ALLOC_CREDIT();
	tchar *pNewName = new tchar[_tcslen( pName ) + 1];
	_tcscpy( pNewName, pName );
	m_Counters[m_NumCounters] = 0;
	m_CounterGroups[m_NumCounters] = (char)eCounterGroup;
	m_CounterNames[m_NumCounters++] = pNewName;
	return &m_Counters[m_NumCounters-1];
}

void CVProfile::ResetCounters( CounterGroup_t eCounterGroup )
{
	int i;
	for( i = 0; i < m_NumCounters; i++ )
	{
		if ( m_CounterGroups[i] == eCounterGroup )
			m_Counters[i] = 0;
	}
}

int CVProfile::GetNumCounters() const
{
	return m_NumCounters;
}

const tchar *CVProfile::GetCounterName( int index ) const
{
	Assert( index >= 0 && index < m_NumCounters );
	return m_CounterNames[index];
}

intp CVProfile::GetCounterValue( int index ) const
{
	Assert( index >= 0 && index < m_NumCounters );
	return m_Counters[index];
}

const tchar *CVProfile::GetCounterNameAndValue( int index, intp &val ) const
{
	Assert( index >= 0 && index < m_NumCounters );
	val = m_Counters[index];
	return m_CounterNames[index];
}

CounterGroup_t CVProfile::GetCounterGroup( int index ) const
{
	Assert( index >= 0 && index < m_NumCounters );
	return (CounterGroup_t)m_CounterGroups[index];
}

#ifdef DBGFLAG_VALIDATE

#ifdef _WIN64
#error the below is presumably broken on 64 bit
#endif // _WIN64

const int k_cSTLMapAllocOffset = 4;
#define GET_INTERNAL_MAP_ALLOC_PTR( pMap ) \
	( * ( (void **) ( ( ( byte * ) ( pMap ) ) + k_cSTLMapAllocOffset ) ) )
//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
void CVProfile::Validate( CValidator &validator, tchar *pchName )
{
	validator.Push( _T("CVProfile"), this, pchName );

	m_Root.Validate( validator, _T("m_Root") );

	for ( int iBudgetGroup=0; iBudgetGroup < m_nBudgetGroupNames; iBudgetGroup++ )
		validator.ClaimMemory( m_pBudgetGroups[iBudgetGroup].m_pName );

	validator.ClaimMemory( m_pBudgetGroups );

	// The std template map class allocates memory internally, but offer no way to get
	// access to their pointer.  Since this is for debug purposes only and the
	// std template classes don't change, just look at the well-known offset.  This
	// is arguably sick and wrong, kind of like marrying a squirrel.
	validator.ClaimMemory( GET_INTERNAL_MAP_ALLOC_PTR( &g_TimesLessChildren ) );
	validator.ClaimMemory( GET_INTERNAL_MAP_ALLOC_PTR( &g_TimeSumsMap ) );

	validator.Pop( );
}

#endif // DBGFLAG_VALIDATE

#endif // VPROF_ENABLED

#ifdef RAD_TELEMETRY_ENABLED

TelemetryData g_Telemetry;
static HTELEMETRY g_tmContext;
static TmU8 *g_pTmMemoryArena = NULL;
static bool g_TelemetryLoaded = false;

static unsigned int g_TelemetryFrameCount = 0;
static bool g_fTelemetryLevelChanged = false;

static const TmU32 TELEMETRY_ARENA_SIZE = 8 * 1024 * 1024; // How much memory we want Telemetry to use.

struct ThreadNameInfo_t
{
	TSLNodeBase_t base;
	ThreadId_t ThreadID;
	char szName[ 64 ];
};
static CTSSimpleList< ThreadNameInfo_t > g_ThreadNamesList;

static bool g_bThreadNameArrayChanged = false;
static int g_ThreadNameArrayCount = 0;
static ThreadNameInfo_t *g_ThreadNameArray[64];

void TelemetryThreadSetDebugName( ThreadId_t id, const char *pszName )
{
	ThreadNameInfo_t *pThreadNameInfo = new ThreadNameInfo_t;

	if( id == ( uint32 )-1 )
	{
		id = ThreadGetCurrentId();
	}

	pThreadNameInfo->ThreadID = id;
	strncpy( pThreadNameInfo->szName, pszName, std::size( pThreadNameInfo->szName ) );
	pThreadNameInfo->szName[ std::size( pThreadNameInfo->szName ) - 1 ] = 0;
	g_ThreadNamesList.Push( pThreadNameInfo );

	g_bThreadNameArrayChanged = true;
}

static void UpdateTelemetryThreadNames()
{
	if( g_bThreadNameArrayChanged )
	{
		// Go through and add any new thread names we got in our thread safe list to our thread names array.
		for( ThreadNameInfo_t *pThreadNameInfo = g_ThreadNamesList.Pop();
			  pThreadNameInfo;
			  pThreadNameInfo = g_ThreadNamesList.Pop() )
		{
			if( g_ThreadNameArrayCount < ssize( g_ThreadNameArray ) )
			{
				g_ThreadNameArray[ g_ThreadNameArrayCount ] = pThreadNameInfo;
				g_ThreadNameArrayCount++;
			}
			else
			{
				delete pThreadNameInfo;
			}
		}

		tmThreadName( g_tmContext, ThreadGetCurrentId(), "MainThrd" );

		for( int i = 0; i < g_ThreadNameArrayCount; i++ )
		{
			tmThreadName( g_tmContext, g_ThreadNameArray[i]->ThreadID, g_ThreadNameArray[i]->szName );
		}

		g_bThreadNameArrayChanged = false;
	}
}

static bool TelemetryInitialize()
{
	if( g_tmContext )
	{
		TmConnectionStatus status = tmGetConnectionStatus( g_tmContext );

		if( status == TMCS_CONNECTED || status == TMCS_CONNECTING )
			return true;
	}

	TmErrorCode retVal;

	if( !g_TelemetryLoaded )
	{
		// Pass in 0 if you want to use the release mode DLL or 1 if you want to
		// use the checked DLL.  The checked DLL is compiled with optimizations but
		// does extra run time checks and reporting.
		int nLoadTelemetry = tmLoadTelemetry( 0 );

		retVal = tmStartup();
		if ( retVal != TM_OK )
		{
			Warning( "TelemetryInit() failed: tmStartup() returned %d, tmLoadTelemetry() returned %d.\n", retVal, nLoadTelemetry );
			return false;
		}

		if( !g_pTmMemoryArena )
		{
			g_pTmMemoryArena = new TmU8[ TELEMETRY_ARENA_SIZE ];
		}

		retVal = tmInitializeContext( &g_tmContext, g_pTmMemoryArena, TELEMETRY_ARENA_SIZE );
		if ( retVal != TM_OK )
		{
			delete [] g_pTmMemoryArena;
			g_pTmMemoryArena = NULL;

			Warning( "TelemetryInit() failed: tmInitializeContext() returned %d.\n", retVal );
			return false;
		}

		g_TelemetryLoaded = true;
	}

	const char *pGameName = "tf2";

#if defined( IS_WINDOWS_PC )
	char baseExeFilename[512];
	if( GetModuleFileName ( GetModuleHandle( NULL ), baseExeFilename, sizeof( baseExeFilename ) ) )
	{
		char *pExt = strrchr( baseExeFilename, '.' );

		if( pExt )
			*pExt = 0;

		char *pSeparator = strrchr( baseExeFilename, '\\' );

		pGameName = pSeparator ? ( pSeparator + 1 ) : baseExeFilename;
	}

	// If you've got \\perforce\symbols on your _NT_SYMBOL_PATH, tmOpen() can take a massively long
	//	time in the symInitialize() routine. Since we don't really need that, kill it here.
	putenv( "_NT_SYMBOL_PATH=" );
#endif

	const char *pServerAddress = g_Telemetry.ServerAddress[0] ? g_Telemetry.ServerAddress : "localhost";
	TmConnectionType tmType = !V_tier0_stricmp( pServerAddress, "FILE" ) ? TMCT_FILE : TMCT_TCP;

	Msg( "TELEMETRY: Calling tmOpen( %s )...\n", pServerAddress );

	char szBuildInfo[ 2048 ];
	_snprintf( szBuildInfo, std::size( szBuildInfo ), "%s: %s", __DATE__ __TIME__, Plat_GetCommandLineA() );
	szBuildInfo[ std::size( szBuildInfo ) - 1 ] = 0;

	TmU32 TmOpenFlags = TMOF_DEFAULT | TMOF_MINIMAL_CONTEXT_SWITCHES;
	/* TmOpenFlags |= TMOF_DISABLE_CONTEXT_SWITCHES | TMOF_INIT_NETWORKING*/

	retVal = tmOpen( g_tmContext, pGameName, szBuildInfo, pServerAddress, tmType,
		TELEMETRY_DEFAULT_PORT, TmOpenFlags, 1000 );
	if ( retVal != TM_OK )
	{
		Warning( "TelemetryInitialize() failed: tmOpen returned %d.\n", retVal );
		return false;
	}

	Msg( "Telemetry initialized at level %u.\n", g_Telemetry.Level );

    // Make sure we set all the thread names.
	g_bThreadNameArrayChanged = true;
	UpdateTelemetryThreadNames();

	return true;
}

static void TelemetryShutdown( bool InDtor = false )
{
	if( g_tmContext )
	{
		// Msg can't be called here as tier0 may have already been shut down...
		if( !InDtor )
		{
			Msg( "Shutting down telemetry.\n" );
		}

		TmConnectionStatus status = tmGetConnectionStatus( g_tmContext );
		if( status == TMCS_CONNECTED || status == TMCS_CONNECTING )
			tmClose( g_tmContext );

		// Discontinue new usage of the context before shutting it down (multithreading).
		memset( g_Telemetry.tmContext, 0, sizeof( g_Telemetry.tmContext ) );
		HTELEMETRY hShutdown = g_tmContext;
		g_tmContext = NULL;

		tmShutdownContext( hShutdown ); 
		tmShutdown();
		g_TelemetryLoaded = false;
	}
}

// Helper class to initialize Telemetry.
class CTelemetryRegister
{
public:
	CTelemetryRegister() 	{}
	~CTelemetryRegister() 	{ TelemetryShutdown( true ); }
} g_TelemetryRegister;

PLATFORM_INTERFACE void TelemetrySetLevel( unsigned int Level )
{
	if( Level != g_Telemetry.Level )
	{
		DevMsg( "TelemetrySetLevel changed from 0x%x to 0x%x\n", g_Telemetry.Level, Level );

		g_Telemetry.Level = Level;
		g_TelemetryFrameCount = g_Telemetry.FrameCount;
		g_fTelemetryLevelChanged = true;
	}
}

static void TelemetryPlots()
{
	if( g_Telemetry.playbacktick )
	{
		tmPlotU32( TELEMETRY_LEVEL1, TMPT_INTEGER, 0, g_Telemetry.playbacktick, "game/PlaybackTick" );
		g_Telemetry.playbacktick = 0;
	}

	for( int i = 0; i < g_VProfCurrentProfile.GetNumCounters(); i++ )
	{
		if( g_VProfCurrentProfile.GetCounterGroup( i ) == COUNTER_GROUP_TELEMETRY )
		{
			int val;
			const char *name = g_VProfCurrentProfile.GetCounterNameAndValue( i, val );

			tmPlotI32( TELEMETRY_LEVEL1, TMPT_INTEGER, 0, val, name );
		}
	}

	g_VProfCurrentProfile.ResetCounters( COUNTER_GROUP_TELEMETRY );
}

PLATFORM_INTERFACE void TelemetryTick()
{
	static double s_d0 = Plat_FloatTime();
	static TmU64 s_t0 = tmFastTime();

	if( !g_Telemetry.Level && g_Telemetry.DemoTickStart && ( (uint32)g_Telemetry.playbacktick > g_Telemetry.DemoTickStart ) )
	{
		TelemetrySetLevel( 2 );
		g_Telemetry.DemoTickStart = 0;
	}
	if( g_Telemetry.Level && g_Telemetry.DemoTickEnd && ( (uint32)g_Telemetry.playbacktick > g_Telemetry.DemoTickEnd ) )
	{
		TelemetrySetLevel( 0 );
		g_Telemetry.DemoTickEnd = ( uint32 )-1;
	}

	// People can NIL out contexts in the TelemetryData structure to control
	//	the level and what sections to log. We always need to do ticks though,
	//	so use master context for this.
	if( g_tmContext )
	{
		// Update any new thread names.
		UpdateTelemetryThreadNames();

		if ( g_Telemetry.Level > 0 )
			TelemetryPlots();

		// Do a Telemetry Tick.
		tmTick( g_tmContext );

		// Update flRDTSCToMilliSeconds.
		TmU64 s_t1 = tmFastTime();
		double s_d1 = Plat_FloatTime();

		g_Telemetry.flRDTSCToMilliSeconds = 1000.0f / ( ( s_t1 - s_t0 ) / ( s_d1 - s_d0 ) );

		s_d0 = s_d1;
		s_t0 = s_t1;

		// Check if we're only supposed to run X amount of frames.
		if( g_TelemetryFrameCount && !tmIsPaused( g_tmContext ) )
		{
			g_TelemetryFrameCount--;
			if( !g_TelemetryFrameCount )
				TelemetrySetLevel( 0 );
		}
	}

	if( g_fTelemetryLevelChanged )
	{
		g_fTelemetryLevelChanged = false;
		memset( g_Telemetry.tmContext, 0, sizeof( g_Telemetry.tmContext ) );

		if( g_Telemetry.Level == 0 )
		{
			// Calling shutdown here invalidates all the telemetry context handles.
			// Background threads in the middle of Tm__Zone'd calls may crash...
			TelemetryShutdown();
		}
		else
		{
			if( !TelemetryInitialize() )
			{
				g_Telemetry.Level = 0;
			}
			else
			{
				tmPause( g_tmContext, 0 );

				uint32 Level = MIN( g_Telemetry.Level, std::size( g_Telemetry.tmContext ) );
				for( uint32 i = 0; i < Level; i++ )
				{
					g_Telemetry.tmContext[i] = g_tmContext;
				}
			}
		}

		//  	TM_SET_TIMELINE_SECTION_NAME( g_tmContext, "Level:0x%x", g_Telemetry.Level );

		// To disable various telemetry features, use the tmEnable() function as so:
		//	TM_ENABLE( g_tmContext, TMO_SUPPORT_PLOT, 0 );
	}
}

#else

// dimhotepus: Export telemetry as vphysics depends on it.
TelemetryData g_Telemetry;

#endif // RAD_TELEMETRY_ENABLED
