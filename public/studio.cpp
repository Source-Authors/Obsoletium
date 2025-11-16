//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "studio.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "tier1/convar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

mstudioanimdesc_t &studiohdr_t::pAnimdesc( intp i ) const
{ 
	if (numincludemodels == 0)
	{
		return *pLocalAnimdesc( i );
	}

	const virtualmodel_t *pVModel = GetVirtualModel();
	Assert( pVModel );

	const virtualgroup_t &pGroup = pVModel->m_group[ pVModel->m_anim[i].group ];
	const studiohdr_t *pStudioHdr = pGroup.GetStudioHdr();
	Assert( pStudioHdr );

	return *pStudioHdr->pLocalAnimdesc( pVModel->m_anim[i].index );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

const mstudio_rle_anim_t *mstudioanimdesc_t::pAnimBlock( intp block, intp index ) const
{
	if (block == -1)
	{
		return nullptr;
	}

	if (block == 0)
	{
		return reinterpret_cast<const mstudio_rle_anim_t *>(reinterpret_cast<const byte *>(this) + index);
	}

	const byte *pAnimBlock = pStudiohdr()->GetAnimBlock( block );
	if ( pAnimBlock )
	{
		return reinterpret_cast<const mstudio_rle_anim_t *>(pAnimBlock + index);
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

static ConVar mod_load_showstall( "mod_load_showstall", "0", 0, "1 - show hitches , 2 - show stalls" );
const mstudio_rle_anim_t *mstudioanimdesc_t::pAnim( int *piFrame ) const
{
	float flStall;
	return pAnim( piFrame, flStall );
}

const mstudio_rle_anim_t *mstudioanimdesc_t::pAnim( int *piFrame, float &flStall ) const
{
	const mstudio_rle_anim_t *panim = nullptr;

	int block = animblock;
	int index = animindex;
	int section = 0;

	if (sectionframes != 0)
	{
		if (numframes > sectionframes && *piFrame == numframes - 1)
		{
			// last frame on long anims is stored separately
			*piFrame = 0;
			section = (numframes / sectionframes) + 1;
		}
		else
		{
			section = *piFrame / sectionframes;
			*piFrame -= section * sectionframes;
		}

		block = pSection( section )->animblock;
		index = pSection( section )->animindex;
	}

	if (block == -1)
	{
		// model needs to be recompiled
		return nullptr;
	}

	panim = pAnimBlock( block, index );

	// force a preload on the next block
	if ( sectionframes != 0 )
	{
		int count = ( numframes / sectionframes) + 2;
		for ( int i = section + 1; i < count; i++ )
		{
			if ( pSection( i )->animblock != block )
			{
				pAnimBlock( pSection( i )->animblock, pSection( i )->animindex );
				break;
			}
		}
	}

	if (panim == nullptr)
	{
		if (section > 0 && mod_load_showstall.GetInt() > 0)
		{
			Msg("[%8.3f] hitch on %s:%s:%d:%d\n", Plat_FloatTime(), pStudiohdr()->pszName(), pszName(), section, block );
		}

		// back up until a previously loaded block is found
		while (--section >= 0)
		{
			block = pSection( section )->animblock;
			index = pSection( section )->animindex;
			panim = pAnimBlock( block, index );
			if (panim)
			{
				// set it to the last frame in the last valid section
				*piFrame = sectionframes - 1;
				break;
			}
		}
	}

	// try to guess a valid stall time interval (tuned for the X360)
	flStall = 0.0f;
	if (panim == nullptr && section <= 0)
	{
		// dimhotepus: Can't make this double as it breaks file format.
		zeroframestalltime = Plat_FloatTime();
		flStall = 1.0f;
	}
	else if (panim != nullptr && zeroframestalltime != 0.0f)
	{
		float dt = Plat_FloatTime() - zeroframestalltime;
		if (dt >= 0.0f)
		{
			flStall = SimpleSpline( clamp( (0.200f - dt) * 5.0f, 0.0f, 1.0f ) );
		}

		if (flStall == 0.0f)
		{
			// disable stalltime
			zeroframestalltime = 0.0f;
		}
		else if (mod_load_showstall.GetInt() > 1)
		{
			Msg("[%8.3f] stall blend %.2f on %s:%s:%d:%d\n", Plat_FloatTime(), flStall, pStudiohdr()->pszName(), pszName(), section, block );
		}
	}

	if (panim == nullptr && mod_load_showstall.GetInt() > 1)
	{
		Msg("[%8.3f] stall on %s:%s:%d:%d\n", Plat_FloatTime(), pStudiohdr()->pszName(), pszName(), section, block );
	}

	return panim;
}

const mstudioikrule_t *mstudioanimdesc_t::pIKRule( intp i ) const
{
	if (ikruleindex)
	{
		return reinterpret_cast<const mstudioikrule_t *>(reinterpret_cast<const byte *>(this) + ikruleindex) + i;
	}

	if (animblockikruleindex)
	{
		if (animblock == 0)
		{
			return reinterpret_cast<const mstudioikrule_t *>(reinterpret_cast<const byte *>(this) + animblockikruleindex) + i;
		}
		
		const byte *pAnimBlocks = pStudiohdr()->GetAnimBlock( animblock );
		if ( pAnimBlocks )
		{
			return reinterpret_cast<const mstudioikrule_t *>(pAnimBlocks + animblockikruleindex) + i;
		}
	}

	return nullptr;
}


const mstudiolocalhierarchy_t *mstudioanimdesc_t::pHierarchy( intp i ) const
{
	if (localhierarchyindex)
	{
		if (animblock == 0)
		{
			return reinterpret_cast<const mstudiolocalhierarchy_t *>(reinterpret_cast<const byte *>(this) + localhierarchyindex) + i;
		}

		const byte *pAnimBlocks = pStudiohdr()->GetAnimBlock( animblock );
		if ( pAnimBlocks )
		{
			return reinterpret_cast<const mstudiolocalhierarchy_t *>(pAnimBlocks + localhierarchyindex) + i;
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

bool studiohdr_t::SequencesAvailable() const
{
	if (numincludemodels == 0)
	{
		return true;
	}

	return GetVirtualModel() != nullptr;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

intp studiohdr_t::GetNumSeq() const
{
	if (numincludemodels == 0)
	{
		return numlocalseq;
	}

	const virtualmodel_t *pVModel = GetVirtualModel();
	Assert( pVModel );

	return pVModel->m_seq.Count();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

mstudioseqdesc_t &studiohdr_t::pSeqdesc( intp i ) const
{
	if (numincludemodels == 0)
	{
		return *pLocalSeqdesc( i );
	}

	const virtualmodel_t *pVModel = GetVirtualModel();
	Assert( pVModel );

	if ( !pVModel )
	{
		return *pLocalSeqdesc( i );
	}

	const virtualgroup_t &pGroup = pVModel->m_group[ pVModel->m_seq[i].group ];
	const studiohdr_t *pStudioHdr = pGroup.GetStudioHdr();
	Assert( pStudioHdr );

	return *pStudioHdr->pLocalSeqdesc( pVModel->m_seq[i].index );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int studiohdr_t::iRelativeAnim( intp baseseq, int relanim ) const
{
	if (numincludemodels == 0)
	{
		return relanim;
	}

	const virtualmodel_t *pVModel = GetVirtualModel();
	Assert( pVModel );

	const virtualgroup_t &pGroup = pVModel->m_group[ pVModel->m_seq[baseseq].group ];

	return pGroup.masterAnim[ relanim ];
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int studiohdr_t::iRelativeSeq( intp baseseq, int relseq ) const
{
	if (numincludemodels == 0)
	{
		return relseq;
	}

	const virtualmodel_t *pVModel = GetVirtualModel();
	Assert( pVModel );

	const virtualgroup_t &pGroup = pVModel->m_group[ pVModel->m_seq[baseseq].group ];

	return pGroup.masterSeq[ relseq ];
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

intp studiohdr_t::GetNumPoseParameters() const
{
	if (numincludemodels == 0)
	{
		return numlocalposeparameters;
	}

	const virtualmodel_t *pVModel = GetVirtualModel();
	Assert( pVModel );

	return pVModel->m_pose.Count();
}



//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

const mstudioposeparamdesc_t &studiohdr_t::pPoseParameter( intp i )
{
	if (numincludemodels == 0)
	{
		return *pLocalPoseParameter( i );
	}

	const virtualmodel_t *pVModel = GetVirtualModel();
	Assert( pVModel );

	const virtualgeneric_t &pose = pVModel->m_pose[i];
	if ( pose.group == 0)
		return *pLocalPoseParameter( pose.index );

	const virtualgroup_t &pGroup = pVModel->m_group[ pose.group ];
	const studiohdr_t *pStudioHdr = pGroup.GetStudioHdr();
	Assert( pStudioHdr );

	return *pStudioHdr->pLocalPoseParameter( pose.index );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int studiohdr_t::GetSharedPoseParameter( intp iSequence, int iLocalPose ) const
{
	if (numincludemodels == 0)
	{
		return iLocalPose;
	}

	if (iLocalPose == -1)
		return iLocalPose;

	const virtualmodel_t *pVModel = GetVirtualModel();
	Assert( pVModel );

	const virtualgroup_t &pGroup = pVModel->m_group[ pVModel->m_seq[iSequence].group ];

	return pGroup.masterPose[iLocalPose];
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int studiohdr_t::EntryNode( intp iSequence ) const
{
	const mstudioseqdesc_t &seqdesc = pSeqdesc( iSequence );

	if (numincludemodels == 0 || seqdesc.localentrynode == 0)
	{
		return seqdesc.localentrynode;
	}

	const virtualmodel_t *pVModel = GetVirtualModel();
	Assert( pVModel );

	const virtualgroup_t &pGroup = pVModel->m_group[ pVModel->m_seq[iSequence].group ];

	return pGroup.masterNode[seqdesc.localentrynode-1]+1;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------


int studiohdr_t::ExitNode( intp iSequence ) const
{
	const mstudioseqdesc_t &seqdesc = pSeqdesc( iSequence );

	if (numincludemodels == 0 || seqdesc.localexitnode == 0)
	{
		return seqdesc.localexitnode;
	}

	const virtualmodel_t *pVModel = GetVirtualModel();
	Assert( pVModel );

	const virtualgroup_t &pGroup = pVModel->m_group[ pVModel->m_seq[iSequence].group ];

	return pGroup.masterNode[seqdesc.localexitnode-1]+1;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

intp studiohdr_t::GetNumAttachments() const
{
	if (numincludemodels == 0)
	{
		return numlocalattachments;
	}

	const virtualmodel_t *pVModel = GetVirtualModel();
	Assert( pVModel );

	return pVModel->m_attachment.Count();
}



//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

const mstudioattachment_t &studiohdr_t::pAttachment( intp i ) const
{
	if (numincludemodels == 0)
	{
		return *pLocalAttachment( i );
	}

	const virtualmodel_t *pVModel = GetVirtualModel();
	Assert( pVModel );

	const virtualgroup_t &pGroup = pVModel->m_group[ pVModel->m_attachment[i].group ];
	const studiohdr_t *pStudioHdr = pGroup.GetStudioHdr();
	Assert( pStudioHdr );

	return *pStudioHdr->pLocalAttachment( pVModel->m_attachment[i].index );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

// dimhotepus: Add non-const version.
mstudioattachment_t &studiohdr_t::pAttachment( intp i )
{
	if (numincludemodels == 0)
	{
		return *pLocalAttachment( i );
	}

	virtualmodel_t *pVModel = GetVirtualModel();
	Assert( pVModel );

	virtualgroup_t &pGroup = pVModel->m_group[ pVModel->m_attachment[i].group ];
	const studiohdr_t *pStudioHdr = pGroup.GetStudioHdr();
	Assert( pStudioHdr );

	return *pStudioHdr->pLocalAttachment( pVModel->m_attachment[i].index );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int	studiohdr_t::GetAttachmentBone( intp i ) const
{
	const mstudioattachment_t &attachment = pAttachment( i );

	// remap bone
	const virtualmodel_t *pVModel = GetVirtualModel();
	if (pVModel)
	{
		const virtualgroup_t &pGroup = pVModel->m_group[ pVModel->m_attachment[i].group ];
		int iBone = pGroup.masterBone[attachment.localbone];

		return iBone != -1 ? iBone : 0;
	}

	return attachment.localbone;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void studiohdr_t::SetAttachmentBone( intp iAttachment, int iBone )
{
	mstudioattachment_t &attachment = pAttachment( iAttachment );

	// remap bone
	const virtualmodel_t *pVModel = GetVirtualModel();
	if (pVModel)
	{
		const virtualgroup_t &pGroup = pVModel->m_group[ pVModel->m_attachment[iAttachment].group ];
		iBone = pGroup.boneMap[iBone];
	}

	attachment.localbone = iBone;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

const char *studiohdr_t::pszNodeName( intp iNode ) const
{
	if (numincludemodels == 0)
	{
		return pszLocalNodeName( iNode );
	}

	const virtualmodel_t *pVModel = GetVirtualModel();
	Assert( pVModel );

	if ( pVModel->m_node.Count() <= iNode-1 )
		return "Invalid node";

	const virtualgeneric_t &node = pVModel->m_node[iNode - 1];

	return pVModel->m_group[ node.group ].GetStudioHdr()->pszLocalNodeName( node.index );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int studiohdr_t::GetTransition( int iFrom, int iTo ) const
{
	if (numincludemodels == 0)
	{
		return *pLocalTransition( (iFrom-1)*numlocalnodes + (iTo - 1) );
	}

	return iTo;
	/*
	FIXME: not connected
	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );

	return pVModel->m_transition.Element( iFrom ).Element( iTo );
	*/
}


int	studiohdr_t::GetActivityListVersion()
{
	if (numincludemodels == 0)
	{
		return activitylistversion;
	}

	const virtualmodel_t *pVModel = GetVirtualModel();
	Assert( pVModel );

	int ActVersion = activitylistversion;

	for (intp i = 1; i < pVModel->m_group.Count(); i++)
	{
		const virtualgroup_t &pGroup = pVModel->m_group[ i ];
		const studiohdr_t *pStudioHdr = pGroup.GetStudioHdr();
		Assert( pStudioHdr );

		ActVersion = min( ActVersion, pStudioHdr->activitylistversion );
	}

	return ActVersion;
}

void studiohdr_t::SetActivityListVersion( int ActVersion ) const
{
	activitylistversion = ActVersion;

	if (numincludemodels == 0)
	{
		return;
	}

	const virtualmodel_t *pVModel = GetVirtualModel();
	Assert( pVModel );

	for (intp i = 1; i < pVModel->m_group.Count(); i++)
	{
		const virtualgroup_t &pGroup = pVModel->m_group[ i ];
		const studiohdr_t *pStudioHdr = pGroup.GetStudioHdr();
		Assert( pStudioHdr );

		pStudioHdr->SetActivityListVersion( ActVersion );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------


intp studiohdr_t::GetNumIKAutoplayLocks() const
{
	if (numincludemodels == 0)
	{
		return numlocalikautoplaylocks;
	}

	const virtualmodel_t *pVModel = GetVirtualModel();
	Assert( pVModel );

	return pVModel->m_iklock.Count();
}

const mstudioiklock_t &studiohdr_t::pIKAutoplayLock( intp i ) const
{
	if (numincludemodels == 0)
	{
		return *pLocalIKAutoplayLock( i );
	}

	const virtualmodel_t *pVModel = GetVirtualModel();
	Assert( pVModel );

	const virtualgroup_t &pGroup = pVModel->m_group[ pVModel->m_iklock[i].group ];
	const studiohdr_t *pStudioHdr = pGroup.GetStudioHdr();
	Assert( pStudioHdr );

	return *pStudioHdr->pLocalIKAutoplayLock( pVModel->m_iklock[i].index );
}

intp studiohdr_t::CountAutoplaySequences() const
{
	intp count = 0;
	for (intp i = 0; i < GetNumSeq(); i++)
	{
		mstudioseqdesc_t &seqdesc = pSeqdesc( i );
		if (seqdesc.flags & STUDIO_AUTOPLAY)
		{
			count++;
		}
	}
	return count;
}

intp studiohdr_t::CopyAutoplaySequences( unsigned short *pOut, intp outCount ) const
{
	intp outIndex = 0;
	for (intp i = 0; i < GetNumSeq() && outIndex < outCount; i++)
	{
		const mstudioseqdesc_t &seqdesc = pSeqdesc( i );
		if (seqdesc.flags & STUDIO_AUTOPLAY)
		{
			Assert(i <= std::numeric_limits<unsigned short>::max());

			pOut[outIndex] = static_cast<unsigned short>(i);
			outIndex++;
		}
	}
	return outIndex;
}

//-----------------------------------------------------------------------------
// Purpose:	maps local sequence bone to global bone
//-----------------------------------------------------------------------------

int studiohdr_t::RemapSeqBone( intp iSequence, int iLocalBone ) const	
{
	// remap bone
	const virtualmodel_t *pVModel = GetVirtualModel();
	if (pVModel)
	{
		const virtualgroup_t *pSeqGroup = pVModel->pSeqGroup( iSequence );
		return pSeqGroup ? pSeqGroup->masterBone[iLocalBone] : iLocalBone;
	}
	return iLocalBone;
}

int	studiohdr_t::RemapAnimBone( intp iAnim, int iLocalBone ) const
{
	// remap bone
	const virtualmodel_t *pVModel = GetVirtualModel();
	if (pVModel)
	{
		const virtualgroup_t *pAnimGroup = pVModel->pAnimGroup( iAnim );
		return pAnimGroup->masterBone[iLocalBone];
	}
	return iLocalBone;
}






//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

CStudioHdr::CStudioHdr() 
{
	// set pointer to bogus value
	m_nFrameUnlockCounter = 0;
	m_pFrameUnlockCounter = &m_nFrameUnlockCounter;

#ifdef STUDIO_ENABLE_PERF_COUNTERS
	m_nPerfAnimatedBones = 0;
	m_nPerfUsedBones = 0;
	m_nPerfAnimationLayers = 0;
#endif

	Init( nullptr );
}

CStudioHdr::CStudioHdr( const studiohdr_t *pStudioHdr, IMDLCache *mdlcache ) 
{
	// preset pointer to bogus value (it may be overwritten with legitimate data later)
	m_nFrameUnlockCounter = 0;
	m_pFrameUnlockCounter = &m_nFrameUnlockCounter;

#ifdef STUDIO_ENABLE_PERF_COUNTERS
	m_nPerfAnimatedBones = 0;
	m_nPerfUsedBones = 0;
	m_nPerfAnimationLayers = 0;
#endif

	Init( pStudioHdr, mdlcache );
}


// extern IDataCache *g_pDataCache;

void CStudioHdr::Init( const studiohdr_t *pStudioHdr, IMDLCache *mdlcache )
{
	m_pStudioHdr = pStudioHdr;

	m_pVModel = nullptr;
	m_pStudioHdrCache.RemoveAll();

	if (m_pStudioHdr == nullptr)
	{
		return;
	}

	if ( mdlcache )
	{
		m_pFrameUnlockCounter = mdlcache->GetFrameUnlockCounterPtr( MDLCACHE_STUDIOHDR );
		m_nFrameUnlockCounter = *m_pFrameUnlockCounter - 1;
	}

	if (m_pStudioHdr->numincludemodels == 0)
	{
#if STUDIO_SEQUENCE_ACTIVITY_LAZY_INITIALIZE
#else
		m_ActivityToSequence.Initialize(this);
#endif
	}
	else
	{
		ResetVModel( m_pStudioHdr->GetVirtualModel() );
#if STUDIO_SEQUENCE_ACTIVITY_LAZY_INITIALIZE
#else
		m_ActivityToSequence.Initialize(this);
#endif
	}

	m_boneFlags.EnsureCount( numbones() );
	m_boneParent.EnsureCount( numbones() );
	for (int i = 0; i < numbones(); i++)
	{
		m_boneFlags[i] = pBone( i )->flags;
		m_boneParent[i] = pBone( i )->parent;
	}
}

void CStudioHdr::Term()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

bool CStudioHdr::SequencesAvailable() const
{
	if (m_pStudioHdr->numincludemodels == 0)
	{
		return true;
	}

	if (m_pVModel == nullptr)
	{
		// repoll m_pVModel
		return ResetVModel( m_pStudioHdr->GetVirtualModel() ) != nullptr;
	}

	return true;
}


const virtualmodel_t * CStudioHdr::ResetVModel( const virtualmodel_t *pVModel ) const
{
	if (pVModel != nullptr)
	{
		m_pVModel = (virtualmodel_t *)pVModel;
		Assert( !pVModel->m_Lock.GetOwnerId() );
		m_pStudioHdrCache.SetCount( m_pVModel->m_group.Count() );

		for (intp i = 0; i < m_pStudioHdrCache.Count(); i++)
		{
			m_pStudioHdrCache[ i ] = nullptr;
		}
		
		return pVModel;
	}

	m_pVModel = nullptr;
	return nullptr;
}

const studiohdr_t *CStudioHdr::GroupStudioHdr( intp i )
{
	if ( m_nFrameUnlockCounter != *m_pFrameUnlockCounter )
	{
		AUTO_LOCK(m_FrameUnlockCounterMutex);
		if ( *m_pFrameUnlockCounter != m_nFrameUnlockCounter ) // i.e., this thread got the mutex
		{
			memset( m_pStudioHdrCache.Base(), 0, m_pStudioHdrCache.Count() * sizeof(studiohdr_t *) );
			m_nFrameUnlockCounter = *m_pFrameUnlockCounter;
		}
	}

	if ( !m_pStudioHdrCache.IsValidIndex( i ) )
	{
		const char *pszName = ( m_pStudioHdr ) ? m_pStudioHdr->pszName() : "<<null>>";
		ExecuteNTimes( 5, Warning( "Invalid index passed to CStudioHdr(%s)::GroupStudioHdr(): %zd, but max is %zd\n", pszName, i, m_pStudioHdrCache.Count() ) );
		DebuggerBreakIfDebugging();
		return m_pStudioHdr; // return something known to probably exist, certainly things will be messed up, but hopefully not crash before the warning is noticed
	}

	const studiohdr_t *pStudioHdr = m_pStudioHdrCache[ i ];
	if (pStudioHdr == nullptr)
	{
		Assert( !m_pVModel->m_Lock.GetOwnerId() );

		virtualgroup_t *pGroup = &m_pVModel->m_group[ i ];
		pStudioHdr = pGroup->GetStudioHdr();
		m_pStudioHdrCache[ i ] = pStudioHdr;
	}

	Assert( pStudioHdr );
	return pStudioHdr;
}


const studiohdr_t *CStudioHdr::pSeqStudioHdr( intp sequence )
{
	if (m_pVModel == nullptr)
	{
		return m_pStudioHdr;
	}

	const studiohdr_t *pStudioHdr = GroupStudioHdr( m_pVModel->m_seq[sequence].group );

	return pStudioHdr;
}


const studiohdr_t *CStudioHdr::pAnimStudioHdr( intp animation )
{
	if (m_pVModel == nullptr)
	{
		return m_pStudioHdr;
	}

	const studiohdr_t *pStudioHdr = GroupStudioHdr( m_pVModel->m_anim[animation].group );

	return pStudioHdr;
}



mstudioanimdesc_t &CStudioHdr::pAnimdesc( intp i )
{ 
	if (m_pVModel == nullptr)
	{
		return *m_pStudioHdr->pLocalAnimdesc( i );
	}

	const studiohdr_t *pStudioHdr = GroupStudioHdr( m_pVModel->m_anim[i].group );

	return *pStudioHdr->pLocalAnimdesc( m_pVModel->m_anim[i].index );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

intp CStudioHdr::GetNumSeq( void ) const
{
	if (m_pVModel == nullptr)
	{
		return m_pStudioHdr->numlocalseq;
	}

	return m_pVModel->m_seq.Count();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

mstudioseqdesc_t &CStudioHdr::pSeqdesc( intp i )
{
	Assert( ( i >= 0 && i < GetNumSeq() ) || ( i == 1 && GetNumSeq() <= 1 ) );
	if ( i < 0 || i >= GetNumSeq() )
	{
		if ( GetNumSeq() <= 0 )
		{
			// Return a zero'd out struct reference if we've got nothing.
			// C_BaseObject::StopAnimGeneratedSounds was crashing due to this function
			//	returning a reference to garbage. It should now see numevents is 0,
			//	and bail.
			static mstudioseqdesc_t s_nil_seq;
			return s_nil_seq;
		}

		// Avoid reading random memory.
		i = 0;
	}
	
	if (m_pVModel == nullptr)
	{
		return *m_pStudioHdr->pLocalSeqdesc( i );
	}

	const studiohdr_t *pStudioHdr = GroupStudioHdr( m_pVModel->m_seq[i].group );

	return *pStudioHdr->pLocalSeqdesc( m_pVModel->m_seq[i].index );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int CStudioHdr::iRelativeAnim( intp baseseq, int relanim ) const
{
	if (m_pVModel == nullptr)
	{
		return relanim;
	}

	const virtualgroup_t *pGroup = &m_pVModel->m_group[ m_pVModel->m_seq[baseseq].group ];

	return pGroup->masterAnim[ relanim ];
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int CStudioHdr::iRelativeSeq( intp baseseq, int relseq ) const
{
	if (m_pVModel == nullptr)
	{
		return relseq;
	}

	Assert( m_pVModel );

	const virtualgroup_t *pGroup = &m_pVModel->m_group[ m_pVModel->m_seq[baseseq].group ];

	return pGroup->masterSeq[ relseq ];
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

intp	CStudioHdr::GetNumPoseParameters() const
{
	if (m_pVModel == nullptr)
	{
		return m_pStudioHdr ? m_pStudioHdr->numlocalposeparameters : 0;
	}

	Assert( m_pVModel );

	return m_pVModel->m_pose.Count();
}



//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

const mstudioposeparamdesc_t &CStudioHdr::pPoseParameter( intp i )
{
	if (m_pVModel == nullptr)
	{
		return *m_pStudioHdr->pLocalPoseParameter( i );
	}

	const virtualgeneric_t &pose = m_pVModel->m_pose[i];
	if ( pose.group == 0)
		return *m_pStudioHdr->pLocalPoseParameter( pose.index );

	const studiohdr_t *pStudioHdr = GroupStudioHdr( pose.group );
	return *pStudioHdr->pLocalPoseParameter( pose.index );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int CStudioHdr::GetSharedPoseParameter( intp iSequence, int iLocalPose ) const
{
	if (m_pVModel == nullptr)
		return iLocalPose;

	if (iLocalPose == -1)
		return iLocalPose;

	Assert( m_pVModel );

	const int group = m_pVModel->m_seq[iSequence].group;

	if (m_pVModel->m_group.IsValidIndex( group ))
	{
		const virtualgroup_t &pGroup = m_pVModel->m_group[ group ];
		return pGroup.masterPose[iLocalPose];
	}

	return iLocalPose;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int CStudioHdr::EntryNode( intp iSequence )
{
	const mstudioseqdesc_t &seqdesc = pSeqdesc( iSequence );

	if (m_pVModel == nullptr || seqdesc.localentrynode == 0)
	{
		return seqdesc.localentrynode;
	}

	Assert( m_pVModel );

	virtualgroup_t *pGroup = &m_pVModel->m_group[ m_pVModel->m_seq[iSequence].group ];

	return pGroup->masterNode[seqdesc.localentrynode-1]+1;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------


int CStudioHdr::ExitNode( intp iSequence )
{
	const mstudioseqdesc_t &seqdesc = pSeqdesc( iSequence );

	if (m_pVModel == nullptr || seqdesc.localexitnode == 0)
	{
		return seqdesc.localexitnode;
	}

	Assert( m_pVModel );

	const virtualgroup_t &pGroup = m_pVModel->m_group[ m_pVModel->m_seq[iSequence].group ];

	return pGroup.masterNode[seqdesc.localexitnode-1]+1;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

intp	CStudioHdr::GetNumAttachments() const
{
	if (m_pVModel == nullptr)
	{
		return m_pStudioHdr->numlocalattachments;
	}

	Assert( m_pVModel );

	return m_pVModel->m_attachment.Count();
}



//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

const mstudioattachment_t &CStudioHdr::pAttachment( intp i )
{
	if (m_pVModel == nullptr)
	{
		return *m_pStudioHdr->pLocalAttachment( i );
	}

	Assert( m_pVModel );

	const studiohdr_t *pStudioHdr = GroupStudioHdr( m_pVModel->m_attachment[i].group );

	return *pStudioHdr->pLocalAttachment( m_pVModel->m_attachment[i].index );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int	CStudioHdr::GetAttachmentBone( intp i )
{
	if (m_pVModel == nullptr)
	{
		return m_pStudioHdr->pLocalAttachment( i )->localbone;
	}

	const virtualgroup_t *pGroup = &m_pVModel->m_group[ m_pVModel->m_attachment[i].group ];
	const mstudioattachment_t &attachment = pAttachment( i );
	int iBone = pGroup->masterBone[attachment.localbone];

	return iBone != -1 ? iBone : 0;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void CStudioHdr::SetAttachmentBone( intp iAttachment, int iBone )
{
	mstudioattachment_t &attachment = const_cast<studiohdr_t *>(m_pStudioHdr)->pAttachment( iAttachment );

	// remap bone
	if (m_pVModel)
	{
		const virtualgroup_t &pGroup = m_pVModel->m_group[ m_pVModel->m_attachment[iAttachment].group ];
		iBone = pGroup.boneMap[iBone];
	}

	attachment.localbone = iBone;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

const char *CStudioHdr::pszNodeName( intp iNode )
{
	if (m_pVModel == nullptr)
	{
		return m_pStudioHdr->pszLocalNodeName( iNode );
	}

	if ( m_pVModel->m_node.Count() <= iNode-1 )
		return "Invalid node";

	const studiohdr_t *pStudioHdr = GroupStudioHdr( m_pVModel->m_node[iNode-1].group );
	
	return pStudioHdr->pszLocalNodeName( m_pVModel->m_node[iNode-1].index );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int CStudioHdr::GetTransition( intp iFrom, int iTo ) const
{
	if (m_pVModel == nullptr)
	{
		return *m_pStudioHdr->pLocalTransition( (iFrom-1)*m_pStudioHdr->numlocalnodes + (iTo - 1) );
	}

	return iTo;
	/*
	FIXME: not connected
	virtualmodel_t *pVModel = (virtualmodel_t *)GetVirtualModel();
	Assert( pVModel );

	return pVModel->m_transition.Element( iFrom ).Element( iTo );
	*/
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int	CStudioHdr::GetActivityListVersion( void )
{
	if (m_pVModel == nullptr)
	{
		return m_pStudioHdr->activitylistversion;
	}

	int version = m_pStudioHdr->activitylistversion;

	for (intp i = 1; i < m_pVModel->m_group.Count(); i++)
	{
		const studiohdr_t *pStudioHdr = GroupStudioHdr( i );
		Assert( pStudioHdr );
		version = min( version, pStudioHdr->activitylistversion );
	}

	return version;
}

void CStudioHdr::SetActivityListVersion( int version )
{
	m_pStudioHdr->activitylistversion = version;

	if (m_pVModel == nullptr)
	{
		return;
	}

	for (intp i = 1; i < m_pVModel->m_group.Count(); i++)
	{
		const studiohdr_t *pStudioHdr = GroupStudioHdr( i );
		Assert( pStudioHdr );
		pStudioHdr->SetActivityListVersion( version );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int	CStudioHdr::GetEventListVersion( void )
{
	if (m_pVModel == nullptr)
	{
		return m_pStudioHdr->eventsindexed;
	}

	int version = m_pStudioHdr->eventsindexed;

	for (intp i = 1; i < m_pVModel->m_group.Count(); i++)
	{
		const studiohdr_t *pStudioHdr = GroupStudioHdr( i );
		Assert( pStudioHdr );
		version = min( version, pStudioHdr->eventsindexed );
	}

	return version;
}

void CStudioHdr::SetEventListVersion( int version )
{
	m_pStudioHdr->eventsindexed = version;

	if (m_pVModel == nullptr)
	{
		return;
	}

	for (intp i = 1; i < m_pVModel->m_group.Count(); i++)
	{
		const studiohdr_t *pStudioHdr = GroupStudioHdr( i );
		Assert( pStudioHdr );
		pStudioHdr->eventsindexed = version;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------


intp CStudioHdr::GetNumIKAutoplayLocks() const
{
	if (m_pVModel == nullptr)
	{
		return m_pStudioHdr->numlocalikautoplaylocks;
	}

	return m_pVModel->m_iklock.Count();
}

const mstudioiklock_t &CStudioHdr::pIKAutoplayLock( intp i )
{
	if (m_pVModel == nullptr)
	{
		return *m_pStudioHdr->pLocalIKAutoplayLock( i );
	}

	const studiohdr_t *pStudioHdr = GroupStudioHdr( m_pVModel->m_iklock[i].group );
	Assert( pStudioHdr );
	return *pStudioHdr->pLocalIKAutoplayLock( m_pVModel->m_iklock[i].index );
}

#if 0
intp	CStudioHdr::CountAutoplaySequences() const
{
	intp count = 0;
	for (intp i = 0; i < GetNumSeq(); i++)
	{
		const mstudioseqdesc_t &seqdesc = pSeqdesc( i );
		if (seqdesc.flags & STUDIO_AUTOPLAY)
		{
			count++;
		}
	}
	return count;
}

intp	CStudioHdr::CopyAutoplaySequences( unsigned short *pOut, intp outCount ) const
{
	intp outIndex = 0;
	for (intp i = 0; i < GetNumSeq() && outIndex < outCount; i++)
	{
		const mstudioseqdesc_t &seqdesc = pSeqdesc( i );
		if (seqdesc.flags & STUDIO_AUTOPLAY)
		{
			pOut[outIndex] = i;
			outIndex++;
		}
	}
	return outIndex;
}

#endif

//-----------------------------------------------------------------------------
// Purpose:	maps local sequence bone to global bone
//-----------------------------------------------------------------------------

int	CStudioHdr::RemapSeqBone( intp iSequence, int iLocalBone ) const	
{
	// remap bone
	if (m_pVModel)
	{
		const virtualgroup_t *pSeqGroup = m_pVModel->pSeqGroup( iSequence );
		return pSeqGroup ? pSeqGroup->masterBone[iLocalBone] : iLocalBone;
	}
	return iLocalBone;
}

int	CStudioHdr::RemapAnimBone( intp iAnim, int iLocalBone ) const
{
	// remap bone
	if (m_pVModel)
	{
		const virtualgroup_t *pAnimGroup = m_pVModel->pAnimGroup( iAnim );
		return pAnimGroup->masterBone[iLocalBone];
	}
	return iLocalBone;
}

//-----------------------------------------------------------------------------
// Purpose: run the interpreted FAC's expressions, converting flex_controller 
//			values into FAC weights
//-----------------------------------------------------------------------------
void CStudioHdr::RunFlexRules( const float *src, float *dest )
{
	// FIXME: this shouldn't be needed, flex without rules should be stripped in studiomdl
	for (int i = 0; i < numflexdesc(); i++)
	{
		dest[i] = 0;
	}

	for (int i = 0; i < numflexrules(); i++)
	{
		float stack[32] = {0};
		int k = 0;
		mstudioflexrule_t *prule = pFlexRule( i );

		mstudioflexop_t *pops = prule->iFlexOp( 0 );

		// debugoverlay->AddTextOverlay( GetAbsOrigin() + Vector( 0, 0, 64 ), i + 1, 0, "%2d:%d\n", i, prule->flex );

		for (int j = 0; j < prule->numops; j++)
		{
			if (k >= ssize(stack)) 
			{
				Error("Out of bounds operations count (%d) >= (%zd)", k, ssize(stack));
			}

			switch (pops->op)
			{
			case STUDIO_ADD: stack[k-2] = stack[k-2] + stack[k-1]; k--; break;
			case STUDIO_SUB: stack[k-2] = stack[k-2] - stack[k-1]; k--; break;
			case STUDIO_MUL: stack[k-2] = stack[k-2] * stack[k-1]; k--; break;
			case STUDIO_DIV:
				if (stack[k-1] > 0.0001)
				{
					stack[k-2] = stack[k-2] / stack[k-1];
				}
				else
				{
					stack[k-2] = 0;
				}
				k--; 
				break;
			case STUDIO_NEG: stack[k-1] = -stack[k-1]; break;
			case STUDIO_MAX: stack[k-2] = max( stack[k-2], stack[k-1] ); k--; break;
			case STUDIO_MIN: stack[k-2] = min( stack[k-2], stack[k-1] ); k--; break;
			case STUDIO_CONST: stack[k] = pops->d.value; k++; break;
			case STUDIO_FETCH1: 
				{ 
				int m = pFlexcontroller( (LocalFlexController_t)pops->d.index)->localToGlobal;
				stack[k] = src[m];
				k++; 
				break;
				}
			case STUDIO_FETCH2:
				{
					stack[k] = dest[pops->d.index]; k++; break;
				}
			case STUDIO_COMBO:
				{
					int m = pops->d.index;
					int km = k - m;
					for ( int iStack = km + 1; iStack < k; ++iStack )
					{
						stack[ km ] *= stack[iStack];
					}
					k = k - m + 1;
				}
				break;
			case STUDIO_DOMINATE:
				{
					int m = pops->d.index;
					int km = k - m;
					float dv = stack[ km ];
					for ( int iStack = km + 1; iStack < k; ++iStack )
					{
						dv *= stack[iStack];
					}
					stack[ km - 1 ] *= 1.0f - dv;
					k -= m;
				}
				break;
			case STUDIO_2WAY_0:
				{ 
					int m = pFlexcontroller( (LocalFlexController_t)pops->d.index )->localToGlobal;
					stack[ k ] = RemapValClamped( src[m], -1.0f, 0.0f, 1.0f, 0.0f );
					k++; 
				}
				break;
			case STUDIO_2WAY_1:
				{ 
					int m = pFlexcontroller( (LocalFlexController_t)pops->d.index )->localToGlobal;
					stack[ k ] = RemapValClamped( src[m], 0.0f, 1.0f, 0.0f, 1.0f );
					k++; 
				}
				break;
			case STUDIO_NWAY:
				{
					LocalFlexController_t valueControllerIndex = static_cast< LocalFlexController_t >( (int)stack[ k - 1 ] );
					int m = pFlexcontroller( valueControllerIndex )->localToGlobal;
					float flValue = src[ m ];
					int v = pFlexcontroller( (LocalFlexController_t)pops->d.index )->localToGlobal;

					const Vector4D filterRamp( stack[ k - 5 ], stack[ k - 4 ], stack[ k - 3 ], stack[ k - 2 ] );

					// Apply multicontrol remapping
					if ( flValue <= filterRamp.x || flValue >= filterRamp.w )
					{
						flValue = 0.0f;
					}
					else if ( flValue < filterRamp.y )
					{
						flValue = RemapValClamped( flValue, filterRamp.x, filterRamp.y, 0.0f, 1.0f );
					}
					else if ( flValue > filterRamp.z )
					{
						flValue = RemapValClamped( flValue, filterRamp.z, filterRamp.w, 1.0f, 0.0f );
					}
					else
					{
						flValue = 1.0f;
					}

					stack[ k - 5 ] = flValue * src[ v ];

					k -= 4; 
				}
				break;
			case STUDIO_DME_LOWER_EYELID:
				{ 
					const mstudioflexcontroller_t *const pCloseLidV = pFlexcontroller( (LocalFlexController_t)pops->d.index );
					const float flCloseLidV = RemapValClamped( src[ pCloseLidV->localToGlobal ], pCloseLidV->min, pCloseLidV->max, 0.0f, 1.0f );

					const mstudioflexcontroller_t *const pCloseLid = pFlexcontroller( static_cast< LocalFlexController_t >( (int)stack[ k - 1 ] ) );
					const float flCloseLid = RemapValClamped( src[ pCloseLid->localToGlobal ], pCloseLid->min, pCloseLid->max, 0.0f, 1.0f );

					int nBlinkIndex = static_cast< int >( stack[ k - 2 ] );
					float flBlink = 0.0f;
					if ( nBlinkIndex >= 0 )
					{
						const mstudioflexcontroller_t *const pBlink = pFlexcontroller( static_cast< LocalFlexController_t >( (int)stack[ k - 2 ] ) );
						flBlink = RemapValClamped( src[ pBlink->localToGlobal ], pBlink->min, pBlink->max, 0.0f, 1.0f );
					}

					int nEyeUpDownIndex = static_cast< int >( stack[ k - 3 ] );
					float flEyeUpDown = 0.0f;
					if ( nEyeUpDownIndex >= 0 )
					{
						const mstudioflexcontroller_t *const pEyeUpDown = pFlexcontroller( static_cast< LocalFlexController_t >( (int)stack[ k - 3 ] ) );
						flEyeUpDown = RemapValClamped( src[ pEyeUpDown->localToGlobal ], pEyeUpDown->min, pEyeUpDown->max, -1.0f, 1.0f );
					}

					if ( flEyeUpDown > 0.0 )
					{
						stack [ k - 3 ] = ( 1.0f - flEyeUpDown ) * ( 1.0f - flCloseLidV ) * flCloseLid;
					}
					else
					{
						stack [ k - 3 ] = ( 1.0f - flCloseLidV ) * flCloseLid;
					}
					k -= 2;
				}
				break;
			case STUDIO_DME_UPPER_EYELID:
				{ 
					const mstudioflexcontroller_t *const pCloseLidV = pFlexcontroller( (LocalFlexController_t)pops->d.index );
					const float flCloseLidV = RemapValClamped( src[ pCloseLidV->localToGlobal ], pCloseLidV->min, pCloseLidV->max, 0.0f, 1.0f );

					const mstudioflexcontroller_t *const pCloseLid = pFlexcontroller( static_cast< LocalFlexController_t >( (int)stack[ k - 1 ] ) );
					const float flCloseLid = RemapValClamped( src[ pCloseLid->localToGlobal ], pCloseLid->min, pCloseLid->max, 0.0f, 1.0f );

					int nBlinkIndex = static_cast< int >( stack[ k - 2 ] );
					float flBlink = 0.0f;
					if ( nBlinkIndex >= 0 )
					{
						const mstudioflexcontroller_t *const pBlink = pFlexcontroller( static_cast< LocalFlexController_t >( (int)stack[ k - 2 ] ) );
						flBlink = RemapValClamped( src[ pBlink->localToGlobal ], pBlink->min, pBlink->max, 0.0f, 1.0f );
					}

					int nEyeUpDownIndex = static_cast< int >( stack[ k - 3 ] );
					float flEyeUpDown = 0.0f;
					if ( nEyeUpDownIndex >= 0 )
					{
						const mstudioflexcontroller_t *const pEyeUpDown = pFlexcontroller( static_cast< LocalFlexController_t >( (int)stack[ k - 3 ] ) );
						flEyeUpDown = RemapValClamped( src[ pEyeUpDown->localToGlobal ], pEyeUpDown->min, pEyeUpDown->max, -1.0f, 1.0f );
					}

					if ( flEyeUpDown < 0.0f )
					{
						stack [ k - 3 ] = ( 1.0f + flEyeUpDown ) * flCloseLidV * flCloseLid;
					}
					else
					{
						stack [ k - 3 ] = flCloseLidV * flCloseLid;
					}
					k -= 2;
				}
				break;
			}

			pops++;
		}

		dest[prule->flex] = stack[0];
	}
}



//-----------------------------------------------------------------------------
//	CODE PERTAINING TO ACTIVITY->SEQUENCE MAPPING SUBCLASS
//-----------------------------------------------------------------------------
CUtlSymbolTable g_ActivityModifiersTable;

extern void SetActivityForSequence( CStudioHdr *pstudiohdr, intp i );

void CStudioHdr::CActivityToSequenceMapping::Initialize( CStudioHdr * __restrict pstudiohdr )
{
	// Algorithm: walk through every sequence in the model, determine to which activity
	// it corresponds, and keep a count of sequences per activity. Once the total count
	// is available, allocate an array large enough to contain them all, update the 
	// starting indices for every activity's section in the array, and go back through,
	// populating the array with its data.

	AssertMsg1( m_pSequenceTuples == nullptr, "Tried to double-initialize sequence mapping for %s", pstudiohdr->pszName() );
	if ( m_pSequenceTuples != nullptr )
		return; // don't double initialize.

	SetValidationPair(pstudiohdr);

	if ( ! pstudiohdr->SequencesAvailable() )
		return; // nothing to do.

#if STUDIO_SEQUENCE_ACTIVITY_LAZY_INITIALIZE
	m_bIsInitialized = true;
#endif
	
	// Some studio headers have no activities at all. In those
	// cases we can avoid a lot of this effort.
	bool bFoundOne = false;	

	// for each sequence in the header...
	const intp NumSeq = pstudiohdr->GetNumSeq();
	for ( intp i = 0 ; i < NumSeq ; ++i )
	{
		const mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( i );
#if defined(SERVER_DLL) || defined(CLIENT_DLL) || defined(GAME_DLL)
		if (!(seqdesc.flags & STUDIO_ACTIVITY))
		{
			// AssertMsg2( false, "Sequence %d on studiohdr %s didn't have its activity initialized!", i, pstudiohdr->pszName() );
			SetActivityForSequence( pstudiohdr, i );
		}
#endif

		// is there an activity associated with this sequence?
		if (seqdesc.activity >= 0)
		{
			bFoundOne = true;

			// look up if we already have an entry. First we need to make a speculative one --
			HashValueType entry(seqdesc.activity, 0, 1, abs(seqdesc.actweight));
			UtlHashHandle_t handle = m_ActToSeqHash.Find(entry);
			if ( m_ActToSeqHash.IsValidHandle(handle) )
			{	
				// we already have an entry and must update it by incrementing count
				HashValueType * __restrict toUpdate = &m_ActToSeqHash.Element(handle);
				toUpdate->count += 1;
				toUpdate->totalWeight += abs(seqdesc.actweight);
				if ( !HushAsserts() )
				{
					AssertMsg( toUpdate->totalWeight > 0, "toUpdate->totalWeight: %d", toUpdate->totalWeight );
				}
			}
			else
			{
				// we do not have an entry yet; create one.
				m_ActToSeqHash.Insert(entry);
			}
		}
	}

	// if we found nothing, don't bother with any other initialization!
	if (!bFoundOne)
		return;

	// Now, create starting indices for each activity. For an activity n, 
	// the starting index is of course the sum of counts [0..n-1]. 
	unsigned sequenceCount = 0;
	int topActivity = 0; // this will store the highest seen activity number (used later to make an ad hoc map on the stack)
	for ( UtlHashHandle_t handle = m_ActToSeqHash.GetFirstHandle() ; 
		  m_ActToSeqHash.IsValidHandle(handle) ;
		  handle = m_ActToSeqHash.GetNextHandle(handle) )
	{
		HashValueType &element = m_ActToSeqHash[handle];
		element.startingIdx = sequenceCount;
		sequenceCount += element.count;
		topActivity = max(topActivity, element.activityIdx);
	}
	

	// Allocate the actual array of sequence information. Note the use of restrict;
	// this is an important optimization, but means that you must never refer to this
	// array through m_pSequenceTuples in the scope of this function.
	SequenceTuple * __restrict tupleList = new SequenceTuple[sequenceCount];
	m_pSequenceTuples = tupleList; // save it off -- NEVER USE m_pSequenceTuples in this function!
	m_iSequenceTuplesCount = sequenceCount;



	// Now we're going to actually populate that list with the relevant data. 
	// First, create an array on the stack to store how many sequences we've written
	// so far for each activity. (This is basically a very simple way of doing a map.)
	// This stack may potentially grow very large; so if you have problems with it, 
	// go to a utlmap or similar structure.
	const unsigned int allocsize = AlignValue((topActivity + 1) * static_cast<int>(sizeof(int)), 16);
	int * __restrict seqsPerAct = stackallocT( int, topActivity + 1 );
	memset(seqsPerAct, 0, allocsize);

	// okay, walk through all the sequences again, and write the relevant data into 
	// our little table.
	for ( intp i = 0 ; i < NumSeq ; ++i )
	{
		const mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( i );
		if (seqdesc.activity >= 0)
		{
			const HashValueType &element = m_ActToSeqHash[m_ActToSeqHash.Find(HashValueType(seqdesc.activity, 0, 0, 0))];
			
			// If this assert trips, we've written more sequences per activity than we allocated 
			// (therefore there must have been a miscount in the first for loop above).
			int tupleOffset = seqsPerAct[seqdesc.activity];
			Assert( tupleOffset < element.count );

			// dimhotepus: Cache frequently accessed things.
			auto *it = tupleList + element.startingIdx + tupleOffset;

			if ( seqdesc.numactivitymodifiers > 0 )
			{
				const int iNumActivityModifiers = seqdesc.numactivitymodifiers;
				// add entries for this model's activity modifiers
				it->pActivityModifiers = new CUtlSymbol[ iNumActivityModifiers ];
				it->iNumActivityModifiers = iNumActivityModifiers;

				for ( int k = 0; k < iNumActivityModifiers; k++ )
				{
					it->pActivityModifiers[ k ] = g_ActivityModifiersTable.AddString( seqdesc.pActivityModifier( k )->pszName() );
				}
			}
			else
			{
				it->pActivityModifiers = nullptr;
				it->iNumActivityModifiers = 0;
			}

			// You might be tempted to collapse this pointer math into a single pointer --
			// don't! the tuple list is marked __restrict above.
			it->seqnum = i; // store sequence number
			it->weight = abs(seqdesc.actweight);

			// We can't have weights of 0
			// Assert( it->weight > 0 );
			if ( it->weight == 0 )
			{
				it->weight = 1;
			}

			seqsPerAct[seqdesc.activity] += 1;
		}
	}

#ifdef DBGFLAG_ASSERT
	// double check that we wrote exactly the right number of sequences.
	unsigned int chkSequenceCount = 0;
	for (int j = 0 ; j <= topActivity ; ++j)
	{
		chkSequenceCount += seqsPerAct[j];
	}
	Assert(chkSequenceCount == m_iSequenceTuplesCount);
#endif

}

/// Force Initialize() to occur again, even if it has already occured.
void CStudioHdr::CActivityToSequenceMapping::Reinitialize( CStudioHdr *pstudiohdr )
{
	m_bIsInitialized = false;

	delete m_pSequenceTuples;
	m_pSequenceTuples = nullptr;

	m_ActToSeqHash.RemoveAll();

	Initialize(pstudiohdr);
}

// Look up relevant data for an activity's sequences. This isn't terribly efficient, due to the
// load-hit-store on the output parameters, so the most common case -- SelectWeightedSequence --
// is specially implemented.
const CStudioHdr::CActivityToSequenceMapping::SequenceTuple *CStudioHdr::CActivityToSequenceMapping::GetSequences( int forActivity, int * __restrict outSequenceCount, int * __restrict outTotalWeight )
{
	// Construct a dummy entry so we can do a hash lookup (the UtlHash does not divorce keys from values)

	HashValueType entry(forActivity, 0, 0, 0);
	UtlHashHandle_t handle = m_ActToSeqHash.Find(entry);
	
	if (m_ActToSeqHash.IsValidHandle(handle))
	{
		const HashValueType &element = m_ActToSeqHash[handle];
		const SequenceTuple *retval = m_pSequenceTuples + element.startingIdx;
		*outSequenceCount = element.count;
		*outTotalWeight = element.totalWeight;

		return retval;
	}

	// invalid handle; return NULL.
	// this is actually a legit use case, so no need to assert.
	return nullptr;
}

int CStudioHdr::CActivityToSequenceMapping::NumSequencesForActivity( int forActivity )
{
	// If this trips, you've called this function on something that doesn't 
	// have activities.
	//Assert(m_pSequenceTuples != NULL);
	if ( m_pSequenceTuples == nullptr )
		return 0;

	HashValueType entry(forActivity, 0, 0, 0);
	UtlHashHandle_t handle = m_ActToSeqHash.Find(entry);
	if (m_ActToSeqHash.IsValidHandle(handle))
	{
		return m_ActToSeqHash[handle].count;
	}

	return 0;
}

// double-check that the data I point to hasn't changed
bool CStudioHdr::CActivityToSequenceMapping::ValidateAgainst( const CStudioHdr * RESTRICT pstudiohdr ) RESTRICT
{
	if (m_bIsInitialized)
	{
		return m_expectedPStudioHdr == pstudiohdr->GetRenderHdr() &&
			   m_expectedVModel == pstudiohdr->GetVirtualModel();
	}

	return true; // Allow an ordinary initialization to take place without printing a panicky assert.
}

void CStudioHdr::CActivityToSequenceMapping::SetValidationPair( const CStudioHdr *RESTRICT pstudiohdr ) RESTRICT
{
	m_expectedPStudioHdr = pstudiohdr->GetRenderHdr();
	m_expectedVModel = pstudiohdr->GetVirtualModel();
}
