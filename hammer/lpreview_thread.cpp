//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The thread which performs lighting preview
//
//===========================================================================//

#include "stdafx.h"

#include "lpreview_thread.h"
#include "mathlib/simdvectormatrix.h"
#include "raytrace.h"
#include "hammer.h"
#include "mainfrm.h"
#include "lprvwindow.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

std::atomic_int n_gbufs_queued;
std::atomic_int n_result_bms_queued;

// the current lighting preview output, if we have one
Bitmap_t *g_pLPreviewOutputBitmap;

enum IncrementalLightState
{
	INCR_STATE_NO_RESULTS=0,							// we threw away the results for this light
	INCR_STATE_PARTIAL_RESULTS=1,							// have done some but not all
	INCR_STATE_NEW=2,										// we know nothing about this light
	INCR_STATE_HAVE_FULL_RESULTS=3,							// we are done
};


class CLightingPreviewThread;

class CIncrementalLightInfo
{
public:
	CIncrementalLightInfo *m_pNext;
	CLightingPreviewLightDescription *m_pLight;
	// incremental lighting tracking information
	int m_nObjectID;
	int m_PartialResultsStage;
	IncrementalLightState m_eIncrState;
	CSIMDVectorMatrix m_CalculatedContribution;
	float m_fTotalContribution;								// current magnitude of light effect
	int m_nBitmapGenerationCounter;							// set on receive of new data from master
	float m_fDistanceToEye;
	int m_nMostRecentNonZeroContributionTimeStamp;

	CIncrementalLightInfo()
		: m_pNext{nullptr},
		m_pLight{nullptr},
		m_nObjectID{-1},
		m_PartialResultsStage{0},
		m_eIncrState{INCR_STATE_NEW},
		m_fTotalContribution{0.f},
		m_nBitmapGenerationCounter{-1},
		m_fDistanceToEye{-1},
		m_nMostRecentNonZeroContributionTimeStamp{0}
	{
	}

	void DiscardResults()
	{
		m_CalculatedContribution.SetSize(0,0);
		if ( m_eIncrState != INCR_STATE_NEW )
			m_eIncrState = INCR_STATE_NO_RESULTS;
	}

	void ClearIncremental()
	{
		m_eIncrState = INCR_STATE_NEW;
		// free calculated lighting matrix
		DiscardResults();
	}

	bool HasWorkToDo() const
	{
		return m_eIncrState != INCR_STATE_HAVE_FULL_RESULTS;
	}
	
	bool IsLowerPriorityThan( CLightingPreviewThread *pLPV,
							  CIncrementalLightInfo const &other ) const;

	bool IsHighPriority( CLightingPreviewThread *pLPV ) const;
};

#define N_INCREMENTAL_STEPS 32

class CLightingPreviewThread
{
public:
	CUtlVector<CLightingPreviewLightDescription> *m_pLightList;
	
	CSIMDVectorMatrix m_Positions;
	CSIMDVectorMatrix m_Normals;
	CSIMDVectorMatrix m_Albedos;
	CSIMDVectorMatrix m_ResultImage;

	RayTracingEnvironment *m_pRtEnv;
	CIncrementalLightInfo *m_pIncrementalLightInfoList;

	bool m_bAccStructureBuilt;
	Vector m_LastEyePosition;

	bool m_bResultChangedSinceLastSend;
	double m_fLastSendTime;

	int m_LineMask[N_INCREMENTAL_STEPS];
	int m_ClosestLineOffset[N_INCREMENTAL_STEPS][N_INCREMENTAL_STEPS];
	int m_nBitmapGenerationCounter;
	int m_nContributionCounter;

	// bounidng box of the rendered scene+ the eye
	Vector m_MinViewCoords;
	Vector m_MaxViewCoords;
	
	CLightingPreviewThread()
	{
		m_pLightList = NULL;
		m_pRtEnv = NULL;
		m_pIncrementalLightInfoList = NULL;
		
		m_bAccStructureBuilt = false;
		m_LastEyePosition = vec3_invalid;

		m_bResultChangedSinceLastSend = false;
		m_fLastSendTime = DBL_MIN;

		m_nBitmapGenerationCounter = -1;
		m_nContributionCounter = 1000000;

		InitIncrementalInformation();
	}
	
	void InitIncrementalInformation();

	~CLightingPreviewThread()
	{
		delete m_pLightList;
		while ( m_pIncrementalLightInfoList )
		{
			auto *n = m_pIncrementalLightInfoList->m_pNext;
			delete m_pIncrementalLightInfoList;

			m_pIncrementalLightInfoList = n;
		}
	}

	// check if the master has new work for us to do, meaning we should abort rendering
	bool ShouldAbort() const
	{
		return g_HammerToLPreviewMsgQueue.MessageWaiting();
	}

	// main loop
	void Run();

	// handle new g-buffers from master
	void HandleGBuffersMessage( MessageToLPreview &msg_in );
	
	// accept triangle list from master
	void HandleGeomMessage( MessageToLPreview &msg_in );

	// send one of our output images back
	void SendVectorMatrixAsRendering( CSIMDVectorMatrix const &src );

	// calculate m_MinViewCoords, m_MaxViewCoords - the bounding box of the rendered pixels+the eye
	void CalculateSceneBounds();

	// inner lighting loop. meant to be multithreaded on dual-core (or more)
	void CalculateForLightTask( int nLineMask, int nLineMatch,
								CLightingPreviewLightDescription &l,
								int calc_mask, 
								float *fContributionOut );

	void CalculateForLight( CLightingPreviewLightDescription &l );

	// send our current output back
	void SendResult();

	void UpdateIncrementalForNewLightList();

	void DiscardResults()
	{
		// invalidate all per light result data
		for( auto *i = m_pIncrementalLightInfoList; i; i = i->m_pNext)
		{
			i->DiscardResults();
		}

		// bump time stamp
		m_nContributionCounter++;
		// update distances to lights
		if ( m_pLightList )
			for ( auto &l : *m_pLightList )
			{
				CIncrementalLightInfo *l_info = l.m_pIncrementalInfo;
				if ( l.m_Type == MATERIAL_LIGHT_DIRECTIONAL )
					l_info->m_fDistanceToEye = 0;			// high priority
				else
					l_info->m_fDistanceToEye = m_LastEyePosition.DistTo( l.m_Position );
			}

		m_bResultChangedSinceLastSend = true;
		m_fLastSendTime = Plat_FloatTime()-9;				// force send
	}
	
	// handle a message. returns true if the thread shuold exit
	bool HandleAMessage();

	// returns whether or not there is useful work to do
	bool AnyUsefulWorkToDo();

	// do some work, like a rendering for one light
	void DoWork();

	Vector EstimatedUnshotAmbient()
	{
//		return Vector( 1,1,1 );
		float sum_weights = 0.0001f;
		Vector sum_colors( sum_weights, sum_weights, sum_weights );
		// calculate an ambient color based on light calculcated so far
		if ( m_pLightList )
			for (const auto &l : *m_pLightList )
			{
				const CIncrementalLightInfo *l_info = l.m_pIncrementalInfo;

				if ( l_info &&
					( l_info->m_eIncrState==INCR_STATE_HAVE_FULL_RESULTS ||
					  l_info->m_eIncrState==INCR_STATE_PARTIAL_RESULTS ) )
				{
					float contribution = l_info->m_fTotalContribution;

					sum_weights += contribution;

					sum_colors.x += contribution * l.m_Color.x;
					sum_colors.y += contribution * l.m_Color.y;
					sum_colors.z += contribution * l.m_Color.z;
				}
			}

		sum_colors.NormalizeInPlace();
		sum_colors *= 0.05f;

		return sum_colors;
	}
};


bool CIncrementalLightInfo::IsHighPriority( CLightingPreviewThread *pLPV ) const
{
	// is this lighjt prioirty-boosted in some way?
	if ( m_eIncrState == INCR_STATE_NEW )
	{
		// uncalculated lights within the view range are highest priority
		if ( m_pLight->m_Position.WithinAABox( pLPV->m_MinViewCoords, 
											   pLPV->m_MaxViewCoords ) )
			return true;
	}
	return false;

}

bool CIncrementalLightInfo::IsLowerPriorityThan( CLightingPreviewThread *pLPV,
												 CIncrementalLightInfo const &other ) const
{
	// a NEW light within the view volume is highest priority
	bool highpriority=IsHighPriority( pLPV );
	bool other_highpriority=other.IsHighPriority( pLPV );

	if ( highpriority && (! other_highpriority ) )
		return false;
	if ( other_highpriority && (! highpriority ) )
		return true;
				 
	int state_combo = m_eIncrState + 16*other.m_eIncrState;
	switch ( state_combo )
	{
		case INCR_STATE_NEW+16*INCR_STATE_NEW:
		{
			// if both are new, closest to eye is best
			return ( m_fDistanceToEye > other.m_fDistanceToEye );
		}

		case INCR_STATE_NEW+16*INCR_STATE_NO_RESULTS:
		{
			// new loses to something we know is probably going to contribute light
			return ( other.m_fTotalContribution > 0 );
		}

		case INCR_STATE_NEW+16*INCR_STATE_PARTIAL_RESULTS:
		{
			return false;
		}

		case INCR_STATE_PARTIAL_RESULTS+16*INCR_STATE_NEW:
		{
			return true;
		}

		case INCR_STATE_NO_RESULTS+16*INCR_STATE_NEW:
		{
			// partial or discarded with no brightness loses to new
			return ( m_fTotalContribution == 0 );
		}


		case INCR_STATE_PARTIAL_RESULTS+16*INCR_STATE_PARTIAL_RESULTS:
		{
			// if incrmental vs incremental, and no light from either, do most recently lit one
			if (( m_fTotalContribution == 0.0) && (other.m_fTotalContribution == 0.0) &&
				( other.m_nMostRecentNonZeroContributionTimeStamp > m_nMostRecentNonZeroContributionTimeStamp ) )
				return true;

			// if other is black, keep this one
			if ( (other.m_fTotalContribution == 0.0) && (m_fTotalContribution >0 ) )
				return false;
			if ( (m_fTotalContribution == 0.0) && (other.m_fTotalContribution >0 ) )
				return true;

			// if incremental states are close, do brightest
			if ( abs( m_PartialResultsStage-other.m_PartialResultsStage)<=1 )
				return ( m_fTotalContribution < other.m_fTotalContribution );

			// else do least refined
			return ( m_PartialResultsStage > other.m_PartialResultsStage );
		}
		case INCR_STATE_PARTIAL_RESULTS+16*INCR_STATE_NO_RESULTS:
		{
			if ( other.m_fTotalContribution )
				return true;
			if (( m_fTotalContribution == 0.0) && (other.m_fTotalContribution == 0.0) )
				return ( other.m_nMostRecentNonZeroContributionTimeStamp > m_nMostRecentNonZeroContributionTimeStamp );
			return ( m_fTotalContribution < other.m_fTotalContribution );
		}
		case INCR_STATE_NO_RESULTS+16*INCR_STATE_PARTIAL_RESULTS:
		{
			if ( m_fTotalContribution )
				return false;
			if (( m_fTotalContribution == 0.0) && (other.m_fTotalContribution == 0.0) )
				return ( other.m_nMostRecentNonZeroContributionTimeStamp > m_nMostRecentNonZeroContributionTimeStamp );
			return ( m_fTotalContribution < other.m_fTotalContribution );
		}
		case INCR_STATE_NO_RESULTS*16+INCR_STATE_NO_RESULTS:
		{
			// if incrmental vs discarded, brightest or most recently bright wins
			if (( m_fTotalContribution == 0.0) && (other.m_fTotalContribution == 0.0) )
				return ( other.m_nMostRecentNonZeroContributionTimeStamp > m_nMostRecentNonZeroContributionTimeStamp );
			return ( m_fTotalContribution < other.m_fTotalContribution );
		}
	}
	return false;
}

void CLightingPreviewThread::InitIncrementalInformation( void )
{
	int calculated_bit_mask=0;
	for(int i=0;i<N_INCREMENTAL_STEPS;i++)
	{
		// bit reverse i
		int msk=0;
		int msk_or=1;
		int msk_test=(N_INCREMENTAL_STEPS >> 1);
		while( msk_test )
		{
			if ( i & msk_test )
				msk |= msk_or;
			msk_or <<= 1;
			msk_test >>= 1;
		}
		calculated_bit_mask |= (1<< msk);
		m_LineMask[i] = calculated_bit_mask;
	}
	// now, find which line to use when resampling a partial result
	for( int lvl=0; lvl < N_INCREMENTAL_STEPS; lvl++)
	{
		for(int linemod=0; linemod < N_INCREMENTAL_STEPS; linemod++)
		{
			int closest_line=std::numeric_limits<int>::max();
			for( int chk=0; chk <= linemod; chk++)
				if ( m_LineMask[lvl] & ( 1 << chk ))
				{
					if (abs( chk-linemod ) < abs( closest_line-linemod ) )
						closest_line = chk;
				}
			m_ClosestLineOffset[lvl][linemod] = closest_line;
		}
	}
}

// constexpr inline float cg[3]={ 1,0,0 };
// constexpr inline float cr[3]={ 0,1,0 };
// constexpr inline float cb[3]={ 0,0,1 };

void CLightingPreviewThread::HandleGeomMessage( MessageToLPreview &msg_in )
{
	delete m_pRtEnv;
	m_pRtEnv = nullptr;

	const CUtlVector<Vector> &tris = msg_in.m_shadowTriangleList;
	if (tris.Count())
	{
//		FILE *fp = fopen( "c:\\gl.out", "w" );
		m_pRtEnv = new RayTracingEnvironment;
		for (intp i = 0; i < tris.Count(); i += 3)
		{
//			fprintf(fp,"3\n");
// 			for(int j=0;j<3;j++)
// 				fprintf( fp,"%f %f %f %f %f %f\n", tris[j+i].x,tris[j+i].y,tris[j+i].z, cr[j],cg[j],cb[j] );
			m_pRtEnv->AddTriangle( i, tris[i],tris[1+i],tris[2+i], Vector( .5,.5,.5) );
		}
//		fclose( fp );
	}

	msg_in.m_shadowTriangleList.Purge();

	m_bAccStructureBuilt = false;

	DiscardResults();
}


void CLightingPreviewThread::CalculateSceneBounds()
{
	FourVectors minbound, maxbound;
	minbound.DuplicateVector( m_LastEyePosition );
	maxbound.DuplicateVector( m_LastEyePosition );
	for(int y=0;y<m_Positions.m_nHeight;y++)
	{
		FourVectors const *cptr= &(m_Positions.CompoundElement(0, y ) );
		for(int x=0; x<m_Positions.m_nPaddedWidth; x++)
		{
			minbound.x=MinSIMD( cptr->x, minbound.x);
			minbound.y=MinSIMD( cptr->y, minbound.y);
			minbound.z=MinSIMD( cptr->z, minbound.z);

			maxbound.x=MaxSIMD( cptr->x, maxbound.x);
			maxbound.y=MaxSIMD( cptr->y, maxbound.y);
			maxbound.z=MaxSIMD( cptr->z, maxbound.z);
			cptr++;
		}
	}
	m_MinViewCoords=minbound.Vec(0);
	m_MaxViewCoords=maxbound.Vec(0);
	for(int v=1; v<4; v++)
	{
		m_MinViewCoords=m_MinViewCoords.Min( minbound.Vec(v) );
		m_MaxViewCoords=m_MaxViewCoords.Max( maxbound.Vec(v) );
	}
}


void CLightingPreviewThread::UpdateIncrementalForNewLightList( void )
{
	for( int iLight=0; iLight<m_pLightList->Count(); iLight++)
	{
		CLightingPreviewLightDescription &descr=(*m_pLightList)[iLight];
		// see if we know about this light
		for( CIncrementalLightInfo *i=m_pIncrementalLightInfoList; i; i=i->m_pNext)
		{
			if (i->m_nObjectID == descr.m_nObjectID )
			{
				// found it!
				descr.m_pIncrementalInfo = i;
				i->m_pLight = &descr;
				break;
			}
		}

		if ( ! descr.m_pIncrementalInfo )
		{
			descr.m_pIncrementalInfo = new CIncrementalLightInfo;
			descr.m_pIncrementalInfo->m_nObjectID = descr.m_nObjectID;
			descr.m_pIncrementalInfo->m_pLight = &descr;

			// add to list
			descr.m_pIncrementalInfo->m_pNext = m_pIncrementalLightInfoList;
			m_pIncrementalLightInfoList = descr.m_pIncrementalInfo;
		}
	}
}


void CLightingPreviewThread::Run()
{
	bool should_quit = false;
	while ( !should_quit )
	{
		while ( 
			!should_quit &&
			( !AnyUsefulWorkToDo() || g_HammerToLPreviewMsgQueue.MessageWaiting() ) )
		{
			should_quit |= HandleAMessage();
		}

		if ( !should_quit && AnyUsefulWorkToDo() )
			DoWork();

		if ( m_bResultChangedSinceLastSend )
		{
			double newtime = Plat_FloatTime();

			// dimhotepus: Update every 1 second, not every 10 seconds.
			if ( newtime - m_fLastSendTime > 1.0 || !AnyUsefulWorkToDo() )
			{
				SendResult();

				m_bResultChangedSinceLastSend = false;
				m_fLastSendTime = newtime;
			}
		}

	}
}

bool CLightingPreviewThread::HandleAMessage()
{
	MessageToLPreview msg_in;
	g_HammerToLPreviewMsgQueue.WaitMessage( &msg_in );

	switch( msg_in.m_MsgType)
	{
		case LPREVIEW_MSG_EXIT:
			return true;									// return from thread
					
		case LPREVIEW_MSG_LIGHT_DATA:
		{
			delete m_pLightList;
			m_pLightList = msg_in.m_pLightList;
			m_LastEyePosition = msg_in.m_EyePosition;
			UpdateIncrementalForNewLightList();
			DiscardResults();
		}
		break;

		case LPREVIEW_MSG_GEOM_DATA:
			HandleGeomMessage( msg_in );
			DiscardResults();
			break;

		case LPREVIEW_MSG_G_BUFFERS:
			HandleGBuffersMessage( msg_in );
			DiscardResults();
			break;
	}
	return false;
}

bool CLightingPreviewThread::AnyUsefulWorkToDo()
{
	if (  m_pLightList ) 
	{
		for( const auto &l : *m_pLightList )
		{
			const CIncrementalLightInfo *l_info=l.m_pIncrementalInfo;
			if ( l_info->HasWorkToDo() )
				return true;
		}
	}
	return false;
}

void CLightingPreviewThread::DoWork()
{
	// dimhotepus: Do not do anything till ray tracing env is ready (when world data arrives).
	if ( m_pLightList && m_pRtEnv ) 
	{
		CLightingPreviewLightDescription *best_l=nullptr;
		CIncrementalLightInfo *best_l_info=nullptr;

		for ( auto &l : *m_pLightList )
		{
			CIncrementalLightInfo *l_info = l.m_pIncrementalInfo;
			if ( l_info->HasWorkToDo() )
			{
				if ( !best_l ||
					 best_l->m_pIncrementalInfo->IsLowerPriorityThan( this, *l_info ) )
				{
					best_l_info = l_info;
					best_l = &l;
				}
			}
		}
		if ( best_l )
		{
			CalculateForLight( *best_l );
			if ( best_l->m_pIncrementalInfo->m_fTotalContribution )
			{
				m_bResultChangedSinceLastSend = true;
			}
			return;
		}
	}
}


void CLightingPreviewThread::HandleGBuffersMessage( MessageToLPreview &msg_in )
{
	m_Albedos.CreateFromRGBA_FloatImageData( 
		msg_in.m_pDefferedRenderingBMs[0]->Width,msg_in.m_pDefferedRenderingBMs[0]->Height,
		msg_in.m_pDefferedRenderingBMs[0]->RGBAData);
	m_Normals.CreateFromRGBA_FloatImageData( 
		msg_in.m_pDefferedRenderingBMs[1]->Width,msg_in.m_pDefferedRenderingBMs[1]->Height,
		msg_in.m_pDefferedRenderingBMs[1]->RGBAData);
	m_Positions.CreateFromRGBA_FloatImageData( 
		msg_in.m_pDefferedRenderingBMs[2]->Width,msg_in.m_pDefferedRenderingBMs[2]->Height,
		msg_in.m_pDefferedRenderingBMs[2]->RGBAData);

	m_LastEyePosition = msg_in.m_EyePosition;
	for( auto *&bm : msg_in.m_pDefferedRenderingBMs )
	{
		delete bm;
		bm = nullptr;
	}

	n_gbufs_queued.fetch_sub(1, std::memory_order::memory_order_relaxed);
	m_nBitmapGenerationCounter = msg_in.m_nBitmapGenerationCounter;

	CalculateSceneBounds();
}


void CLightingPreviewThread::SendResult()
{
	m_ResultImage = m_Albedos;
 	m_ResultImage *= EstimatedUnshotAmbient();

	for( int i = 0 ; i < m_pLightList->Count(); i ++ )
	{
		CLightingPreviewLightDescription & l = ( *m_pLightList )[i];
		CIncrementalLightInfo * l_info = l.m_pIncrementalInfo;

		if ( ( l_info->m_fTotalContribution > 0.0 ) &&
			 ( l_info->m_eIncrState >= INCR_STATE_PARTIAL_RESULTS ) )
		{
			// need to add partials, replicated to handle undone lines
			CSIMDVectorMatrix & src = l_info->m_CalculatedContribution;
			for( int y = 0;y < m_ResultImage.m_nHeight;y ++ )
			{
				int yo = y & ( N_INCREMENTAL_STEPS - 1 );
				int src_y = ( y & ~( N_INCREMENTAL_STEPS - 1 ))
					+ m_ClosestLineOffset[l_info->m_PartialResultsStage][yo];
				FourVectors const * cptr = &( src.CompoundElement( 0, src_y ));
				FourVectors * dest =& ( m_ResultImage.CompoundElement( 0, y ));
				FourVectors const *pAlbedo =&( m_Albedos.CompoundElement( 0, y ));
				for( int x = 0;x < m_ResultImage.m_nPaddedWidth;x ++ )
				{
					FourVectors albedo_value = *( pAlbedo++ );
					albedo_value *= *( cptr++ );
					* ( dest++ ) += albedo_value;
				}
			}
		}
	}
	SendVectorMatrixAsRendering( m_ResultImage );
	m_fLastSendTime = Plat_FloatTime();
	m_bResultChangedSinceLastSend = false;
}

void CLightingPreviewThread::CalculateForLightTask( int nLineMask, int nLineMatch,
													CLightingPreviewLightDescription &l,
													int calc_mask, 
													float *fContributionOut )
{
	FourVectors zero_vector;
	zero_vector.x=Four_Zeros;
	zero_vector.y=Four_Zeros;
	zero_vector.z=Four_Zeros;

	FourVectors total_light=zero_vector;
   
	CIncrementalLightInfo *l_info=l.m_pIncrementalInfo;
	CSIMDVectorMatrix &rslt=l_info->m_CalculatedContribution;
	// figure out what lines to do
	fltx4 ThresholdBrightness=ReplicateX4( 0.1 / 1024.0f );
	FourVectors LastLinesTotalLight=zero_vector;
	int work_line_number=0;									// for task masking
	for(int y=0;y<rslt.m_nHeight;y++)
	{
		FourVectors ThisLinesTotalLight=zero_vector;
		int ybit=(1<<(y & (N_INCREMENTAL_STEPS-1) ) );
		if ( (ybit & calc_mask)==0)	// do this line?
			ThisLinesTotalLight=LastLinesTotalLight;
		else
		{
			if ( (work_line_number & nLineMatch) == nLineMatch)
			{
				for(int x=0;x<rslt.m_nPaddedWidth;x++)
				{
					// shadow check
					FourVectors pos=m_Positions.CompoundElement( x, y );
					FourVectors normal=m_Normals.CompoundElement( x, y );

					FourVectors l_add=zero_vector;
					l.ComputeLightAtPoints( pos, normal, l_add, false );
					fltx4 v_or=OrSIMD( l_add.x, OrSIMD( l_add.y, l_add.z ) );
					if ( ! IsAllZeros( v_or ) )
					{
						FourVectors lpos;
						lpos.DuplicateVector( l.m_Position );

						FourRays myray = {};
						myray.direction=lpos;
						myray.direction-=pos;
						fltx4 len=myray.direction.length();
						myray.direction *= ReciprocalSIMD( len );

						// slide towards light to avoid self-intersection
						myray.origin=myray.direction;
						myray.origin *= 0.02f;
						myray.origin += pos;

						RayTracingResult r_rslt;
						m_pRtEnv->Trace4Rays( myray, Four_Zeros, ReplicateX4( 1.0e9f ), &r_rslt );

						for(int c=0;c<4;c++)					// !!speed!! use sse logic ops here
						{
							if ( (r_rslt.HitIds[c] != -1) &&
								 (r_rslt.HitDistance.m128_f32[c] < len.m128_f32[c] ) )
							{
								l_add.x.m128_f32[c]=0.0f;
								l_add.y.m128_f32[c]=0.0f;
								l_add.z.m128_f32[c]=0.0f;
							}
						}
						rslt.CompoundElement( x, y ) = l_add;
						l_add *= m_Albedos.CompoundElement( x, y );
						// now, supress brightness < threshold so as to not falsely think
						// far away lights are interesting
						l_add.x = AndSIMD( l_add.x, CmpGtSIMD( l_add.x, ThresholdBrightness ) );
						l_add.y = AndSIMD( l_add.y, CmpGtSIMD( l_add.y, ThresholdBrightness ) );
						l_add.z = AndSIMD( l_add.z, CmpGtSIMD( l_add.z, ThresholdBrightness ) );
						ThisLinesTotalLight += l_add;
					}
					else
						rslt.CompoundElement( x, y ) = l_add;
				}
				total_light += ThisLinesTotalLight;
			}
			work_line_number++;
		}
	}
	fltx4 lmag=total_light.length();
	*(fContributionOut)=DirectX::XMVectorGetX(DirectX::XMVectorSum(lmag));
}

void CLightingPreviewThread::CalculateForLight( CLightingPreviewLightDescription &l )
{
	if ( m_pRtEnv && !m_bAccStructureBuilt )
	{
		m_pRtEnv->SetupAccelerationStructure();
		m_bAccStructureBuilt = true;
	}

	CIncrementalLightInfo *l_info=l.m_pIncrementalInfo;
	Assert( l_info );
	l_info->m_CalculatedContribution.SetSize( m_Albedos.m_nWidth, m_Albedos.m_nHeight );

	// figure out which lines need to be calculated
	int prev_msk=0;
	int new_incr_level=0;
	if ( l_info->m_eIncrState == INCR_STATE_PARTIAL_RESULTS )
	{
		new_incr_level = 1+l_info->m_PartialResultsStage;
		prev_msk = m_LineMask[l_info->m_PartialResultsStage]; 
	}
	int calc_mask=m_LineMask[new_incr_level] &~ prev_msk;

	// multihread here
	float total_light;
	CalculateForLightTask( 0, 0, l, calc_mask, &total_light );
	l_info->m_fTotalContribution = total_light;
	
	// throw away light array if no contribution
	if ( total_light == 0.0f )
		l_info->m_CalculatedContribution.SetSize( 0, 0 );
	else
	{
		l_info->m_nMostRecentNonZeroContributionTimeStamp = m_nContributionCounter;
	}

	l_info->m_PartialResultsStage = new_incr_level;
	if ( new_incr_level == N_INCREMENTAL_STEPS-1)
		l_info->m_eIncrState = INCR_STATE_HAVE_FULL_RESULTS;
	else
		l_info->m_eIncrState = INCR_STATE_PARTIAL_RESULTS;
}

void CLightingPreviewThread::SendVectorMatrixAsRendering( CSIMDVectorMatrix const &src )
{
	auto *ret_bm = new Bitmap_t;
	ret_bm->Init( src.m_nWidth, src.m_nHeight, IMAGE_FORMAT_RGBA8888 );

	// lets copy into the output bitmap
	for(int y=0;y<src.m_nHeight;y++)
		for(int x=0;x<src.m_nWidth;x++)
		{
			Vector color = src.Element( x, y );

			*(ret_bm->GetPixel( x, y )+0)= (uint8) min(255, (int)(255.0*pow(color.z,(float) (1/2.2))));
			*(ret_bm->GetPixel( x, y )+1)= (uint8) min(255, (int)(255.0*pow(color.y,(float) (1/2.2))));
			*(ret_bm->GetPixel( x, y )+2)= (uint8) min(255, (int)(255.0*pow(color.x,(float) (1/2.2))));
			*(ret_bm->GetPixel( x, y )+3)=0;

			/*DirectX::XMVECTOR colorVec = DirectX::XMLoadFloat3(color.XmBase());
			DirectX::XMVECTOR powerVec = DirectX::XMVectorReplicate(1/2.2f);
			DirectX::XMVECTOR resultVec = DirectX::XMVectorMin
			(
				DirectX::XMConvertVectorFloatToInt
				(
					DirectX::XMVectorScale( DirectX::XMVectorPow( colorVec, powerVec ), 255.f ),
					0
				),
				DirectX::XMVectorReplicateInt( 255 )
			);

			unsigned char* pixel = ret_bm->GetPixel( x, y );

			*(pixel+0)= (uint8) DirectX::XMVectorGetZ( resultVec );
			*(pixel+1)= (uint8) DirectX::XMVectorGetY( resultVec );
			*(pixel+2)= (uint8) DirectX::XMVectorGetX( resultVec );
			*(pixel+3)=0;*/
		}

	MessageFromLPreview ret_msg( LPREVIEW_MSG_DISPLAY_RESULT );
	ret_msg.m_pBitmapToDisplay = ret_bm;
	ret_msg.m_nBitmapGenerationCounter = m_nBitmapGenerationCounter;

	// n_result_bms_queued.fetch_add(1, std::memory_order::memory_order_relaxed);

	g_LPreviewToHammerMsgQueue.QueueMessage( ret_msg );
}




// master side of lighting preview
unsigned LightingPreviewThreadFN( void * )
{
	// dimhotepus: Add thread name to aid debugging.
	ThreadSetDebugName("LightingPreview");
	ThreadSetPriority( -2 );								// low

	CLightingPreviewThread thread;
	thread.Run();

	return 0;
}

namespace {

BOOL CalcWindowRectForDpi(CWnd *window, LPRECT rect,
                          UINT nAdjustType = CWnd::adjustBorder) {
  DWORD ex_style{window->GetExStyle()};
  if (nAdjustType == 0) ex_style &= ~WS_EX_CLIENTEDGE;

  const UINT window_dpi{::GetDpiForWindow(window->GetSafeHwnd())};
  return ::AdjustWindowRectExForDpi(rect, window->GetStyle(),
                                    window->GetMenu() ? TRUE : FALSE,
                                    ex_style,
                                    window_dpi);
}

}  // namespace 

void HandleLightingPreview()
{
	auto *&previewWindow = GetMainWnd()->m_pLightingPreviewOutputWindow; 
	if ( previewWindow && !GetMainWnd()->m_bLightingPreviewOutputWindowShowing )
	{
		delete previewWindow;
		previewWindow = nullptr;
	}

	// called during main loop
	while ( g_LPreviewToHammerMsgQueue.MessageWaiting() )
	{
		MessageFromLPreview msg;
		g_LPreviewToHammerMsgQueue.WaitMessage( &msg );

		switch( msg.m_MsgType )
		{
			case LPREVIEW_MSG_DISPLAY_RESULT:
			{
				n_result_bms_queued.fetch_sub(1, std::memory_order::memory_order_relaxed);

				delete g_pLPreviewOutputBitmap;
				// g_pLPreviewOutputBitmap = nullptr;

//				if ( msg.m_nBitmapGenerationCounter == g_nBitmapGenerationCounter )
			{
				g_pLPreviewOutputBitmap = msg.m_pBitmapToDisplay;

				if ( g_pLPreviewOutputBitmap && g_pLPreviewOutputBitmap->Width() > 10 )
				{
					SignalUpdate( EVTYPE_BITMAP_RECEIVED_FROM_LPREVIEW );

					auto *&w = GetMainWnd()->m_pLightingPreviewOutputWindow;
					if ( !GetMainWnd()->m_bLightingPreviewOutputWindowShowing )
					{
						w = new CLightingPreviewResultsWindow;
						w->Create( GetMainWnd(), GetMainWnd()->GetTitle() );

						GetMainWnd()->m_bLightingPreviewOutputWindowShowing = true;
					}

					if ( !w->IsWindowVisible() )
						w->ShowWindow( SW_SHOW );

					RECT existing_rect;
					w->GetClientRect( &existing_rect );

					const int expected_width{g_pLPreviewOutputBitmap->Width()-1};
					const int expected_height{g_pLPreviewOutputBitmap->Height()-1};

					if (
						existing_rect.right != expected_width ||
						existing_rect.bottom != expected_height )
					{
						CRect new_rect{
							0,
							0,
							expected_width,
							expected_height
						};
						CalcWindowRectForDpi(w, &new_rect);

						w->SetWindowPos(
							nullptr, 0, 0,
							new_rect.Width(), new_rect.Height(),
							SWP_NOMOVE | SWP_NOZORDER );
					}
					
					w->Invalidate( false );
					w->UpdateWindow();
				}
			}
// 				else
// 					delete msg.m_pBitmapToDisplay;			// its old
			break;
			}
		}
	}
}


