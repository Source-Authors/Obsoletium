//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include <limits.h>
#include "studio.h"
#include "tier1/utlmap.h"
#include "tier1/utldict.h"
#include "tier1/utlbuffer.h"
#include "filesystem.h"
#include "tier0/icommandline.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IFileSystem *		g_pFileSystem;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

// a string table to speed up searching for sequences in the current virtual model
struct modellookup_t
{
	// dimhotepus: short -> intp
	CUtlDict<intp,intp> seqTable;
	// dimhotepus: short -> intp
	CUtlDict<intp,intp> animTable;
};

static CUtlVector<modellookup_t> g_ModelLookup;
static intp g_ModelLookupIndex = -1;

static inline bool HasLookupTable()
{
	return g_ModelLookupIndex >= 0 ? true : false;
}

static CThreadFastMutex g_pSeqTableLock;
// dimhotepus: short -> intp
static inline CUtlDict<intp,intp> *GetSeqTable()
{
	return &g_ModelLookup[g_ModelLookupIndex].seqTable;
}

static CThreadFastMutex g_pAnimTableLock;
// dimhotepus: short -> intp
static inline CUtlDict<intp,intp> *GetAnimTable()
{
	return &g_ModelLookup[g_ModelLookupIndex].animTable;
}

class CModelLookupContext final
{
public:
	CModelLookupContext(int group, const studiohdr_t *pStudioHdr);
	~CModelLookupContext();

private:
	intp		m_lookupIndex;
};

CModelLookupContext::CModelLookupContext(int group, const studiohdr_t *pStudioHdr)
{
	m_lookupIndex = -1;
	if ( group == 0 && pStudioHdr->numincludemodels )
	{
		m_lookupIndex = g_ModelLookup.AddToTail();
		g_ModelLookupIndex = g_ModelLookup.Count()-1;
	}
}

CModelLookupContext::~CModelLookupContext()
{
	if ( m_lookupIndex >= 0 )
	{
		Assert(m_lookupIndex == (g_ModelLookup.Count()-1));
		g_ModelLookup.FastRemove(m_lookupIndex);
		g_ModelLookupIndex = g_ModelLookup.Count()-1;
	}
}

void virtualmodel_t::AppendModels( intp group, const studiohdr_t *pStudioHdr )
{
	AUTO_LOCK( m_Lock );

	// build a search table if necesary
	CModelLookupContext ctx(group, pStudioHdr);

	AppendSequences( group, pStudioHdr );
	AppendAnimations( group, pStudioHdr );
	AppendBonemap( group, pStudioHdr );
	AppendAttachments( group, pStudioHdr );
	AppendPoseParameters( group, pStudioHdr );
	AppendNodes( group, pStudioHdr );
	AppendIKLocks( group, pStudioHdr );

	struct HandleAndHeader_t
	{
		void				*handle;
		const studiohdr_t	*pHdr;
	};
	HandleAndHeader_t list[64];

	// determine quantity of valid include models in one pass only
	// temporarily cache results off, otherwise FindModel() causes ref counting problems
	intp nValidIncludes = 0;
	for (int j = 0; j < pStudioHdr->numincludemodels; j++)
	{
		// find model (increases ref count)
		void *tmp = nullptr;
		const studiohdr_t *pTmpHdr = pStudioHdr->FindModel( &tmp, pStudioHdr->pModelGroup( j )->pszName() );
		if ( pTmpHdr )
		{
			if ( nValidIncludes >= ssize( list ) )
			{
				// would cause stack overflow
				Assert( 0 );
				break;
			}

			list[nValidIncludes].handle = tmp;
			list[nValidIncludes].pHdr = pTmpHdr;
			nValidIncludes++;
		}
	}

	if ( nValidIncludes )
	{
		m_group.EnsureCapacity( m_group.Count() + nValidIncludes );
		for (intp j = 0; j < nValidIncludes; j++)
		{
			MEM_ALLOC_CREDIT();
			intp groupi = m_group.AddToTail();
			m_group[groupi].cache = list[j].handle;
			AppendModels( groupi, list[j].pHdr );
		}
	}

	UpdateAutoplaySequences( pStudioHdr );
}

void virtualmodel_t::AppendSequences( intp group, const studiohdr_t *pStudioHdr )
{
	AUTO_LOCK( m_Lock );
	AUTO_LOCK( g_pSeqTableLock ); // RaphaelIT7: m_Lock is for THIS virtual model - so multiple threads still can party on here
	const intp numCheck = m_seq.Count();

	MEM_ALLOC_CREDIT();

	CUtlVector< virtualsequence_t > seq;
	seq = m_seq;
	
	auto &vgroup = m_group[group];
	vgroup.masterSeq.SetCount( pStudioHdr->numlocalseq );

	auto *seqTable = GetSeqTable();

	for (int j = 0; j < pStudioHdr->numlocalseq; j++)
	{
		const mstudioseqdesc_t *seqdesc = pStudioHdr->pLocalSeqdesc( j );
		const char *s1 = seqdesc->pszLabel();
		
		intp k;
		if ( HasLookupTable() )
		{
			k = numCheck;
			const intp index = seqTable->Find( s1 );
			if ( index != seqTable->InvalidIndex() )
			{
				k = seqTable->Element(index);
			}
		}
		else
		{
			for (k = 0; k < numCheck; k++)
			{
				const auto &vseq = seq[k];
				const studiohdr_t *hdr = m_group[ vseq.group ].GetStudioHdr();
				const char *s2 = hdr->pLocalSeqdesc( vseq.index )->pszLabel();
				if ( !stricmp( s1, s2 ) )
				{
					break;
				}
			}
		}

		// no duplication
		if (k == numCheck)
		{
			virtualsequence_t tmp;
			tmp.group = group;
			tmp.index = j;
			tmp.flags = seqdesc->flags;
			tmp.activity = seqdesc->activity;
			k = seq.AddToTail( tmp );
		}
		else if (m_group[ seq[k].group ].GetStudioHdr()->pLocalSeqdesc( seq[k].index )->flags & STUDIO_OVERRIDE)
		{
			// the one in memory is a forward declared sequence, override it
			virtualsequence_t tmp;
			tmp.group = group;
			tmp.index = j;
			tmp.flags = seqdesc->flags;
			tmp.activity = seqdesc->activity;
			seq[k] = tmp;
		}

		vgroup.masterSeq[ j ] = k;
	}

	if ( HasLookupTable() )
	{
		for ( intp j = numCheck; j < seq.Count(); j++ )
		{
			const auto &vseq = seq[j];
			const studiohdr_t *hdr = m_group[ vseq.group ].GetStudioHdr();
			const char *s1 = hdr->pLocalSeqdesc( vseq.index )->pszLabel();
			seqTable->Insert( s1, j );
		}
	}

	m_seq.Swap(seq);
}


void virtualmodel_t::UpdateAutoplaySequences( const studiohdr_t *pStudioHdr )
{
	AUTO_LOCK( m_Lock );
	const intp autoplayCount = pStudioHdr->CountAutoplaySequences();
	m_autoplaySequences.SetCount( autoplayCount );
	pStudioHdr->CopyAutoplaySequences( m_autoplaySequences.Base(), autoplayCount );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void virtualmodel_t::AppendAnimations( intp group, const studiohdr_t *pStudioHdr )
{
	AUTO_LOCK( m_Lock );
	AUTO_LOCK( g_pAnimTableLock ); // RaphaelIT7: m_Lock is for THIS virtual model - so multiple threads still can party on here
	const intp numCheck = m_anim.Count();

	CUtlVector< virtualgeneric_t > anim;
	anim = m_anim;

	MEM_ALLOC_CREDIT();

	auto &vgroup = m_group[group];
	vgroup.masterAnim.SetCount( pStudioHdr->numlocalanim );

	auto *animTable = GetAnimTable();

	for (int j = 0; j < pStudioHdr->numlocalanim; j++)
	{
		const char *s1 = pStudioHdr->pLocalAnimdesc( j )->pszName();
		intp k;
		if ( HasLookupTable() )
		{
			k = numCheck;
			const intp index = animTable->Find( s1 );
			if ( index != animTable->InvalidIndex() )
			{
				k = animTable->Element(index);
			}
		}
		else
		{
			for (k = 0; k < numCheck; k++)
			{
				const auto &vanim = anim[k];
				const char *s2 = m_group[ vanim.group ].GetStudioHdr()->pLocalAnimdesc( vanim.index )->pszName();
				if (stricmp( s1, s2 ) == 0)
				{
					break;
				}
			}
		}

		// no duplication
		if (k == numCheck)
		{
			virtualgeneric_t tmp;
			tmp.group = group;
			tmp.index = j;
			k = anim.AddToTail( tmp );
		}

		vgroup.masterAnim[j] = k;
	}
	
	if ( HasLookupTable() )
	{
		for ( intp j = numCheck; j < anim.Count(); j++ )
		{
			const auto &vanim = anim[j];
			const char *s1 = m_group[ vanim.group ].GetStudioHdr()->pLocalAnimdesc( vanim.index )->pszName();
			animTable->Insert( s1, j );
		}
	}

	m_anim.Swap(anim);
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void virtualmodel_t::AppendBonemap( intp group, const studiohdr_t *pStudioHdr )
{
	AUTO_LOCK( m_Lock );
	MEM_ALLOC_CREDIT();

	const studiohdr_t *pBaseStudioHdr = m_group[ 0 ].GetStudioHdr( );

	auto &vgroup = m_group[ group ];
	vgroup.boneMap.SetCount( pBaseStudioHdr->numbones );
	vgroup.masterBone.SetCount( pStudioHdr->numbones );

	if (group == 0)
	{
		for (int j = 0; j < pStudioHdr->numbones; j++)
		{
			vgroup.boneMap[ j ] = j;
			vgroup.masterBone[ j ] = j;
		}
	}
	else
	{
		for (int j = 0; j < pBaseStudioHdr->numbones; j++)
		{
			vgroup.boneMap[ j ] = -1;
		}

		for (int j = 0; j < pStudioHdr->numbones; j++)
		{
			const auto *studioBone = pStudioHdr->pBone( j );

			int k;
			// NOTE: studiohdr has a bone table - using the table is ~5% faster than this for alyx.mdl on a P4/3.2GHz
			for (k = 0; k < pBaseStudioHdr->numbones; k++)
			{
				if (stricmp( studioBone->pszName(), pBaseStudioHdr->pBone( k )->pszName() ) == 0)
				{
					break;
				}
			}

			if (k < pBaseStudioHdr->numbones)
			{
				const auto *baseBone = pBaseStudioHdr->pBone( k );

				vgroup.masterBone[ j ] = k;
				vgroup.boneMap[ k ] = j;

				// FIXME: these runtime messages don't display in hlmv
				if (studioBone->parent == -1 || baseBone->parent == -1)
				{
					if (studioBone->parent != -1 || baseBone->parent != -1)
					{
						Warning( "%s/%s : missmatched parent bones on \"%s\"\n", pBaseStudioHdr->pszName(), pStudioHdr->pszName(), studioBone->pszName() );
					}
				}
				else if (vgroup.masterBone[ studioBone->parent ] != m_group[ 0 ].masterBone[ baseBone->parent ])
				{
					Warning( "%s/%s : missmatched parent bones on \"%s\"\n", pBaseStudioHdr->pszName(), pStudioHdr->pszName(), studioBone->pszName() );
				}
			}
			else
			{
				vgroup.masterBone[ j ] = -1;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void virtualmodel_t::AppendAttachments( intp group, const studiohdr_t *pStudioHdr )
{
	AUTO_LOCK( m_Lock );
	const intp numCheck = m_attachment.Count();

	CUtlVector< virtualgeneric_t > attachment;
	attachment = m_attachment;

	MEM_ALLOC_CREDIT();

	auto &vgroup = m_group[ group ];
	vgroup.masterAttachment.SetCount( pStudioHdr->numlocalattachments );

	for (int j = 0; j < pStudioHdr->numlocalattachments; j++)
	{
		auto n = vgroup.masterBone[ pStudioHdr->pLocalAttachment( j )->localbone ];
		// skip if the attachments bone doesn't exist in the root model
		if (n == -1)
		{
			continue;
		}
		
		const char *s1 = pStudioHdr->pLocalAttachment( j )->pszName();
		intp k;
		for (k = 0; k < numCheck; k++)
		{
			const auto &vattachment = attachment[k];
			const char *s2 = m_group[ vattachment.group ].GetStudioHdr()->pLocalAttachment( vattachment.index )->pszName();

			if (stricmp( s1, s2 ) == 0)
			{
				break;
			}
		}

		// no duplication
		if (k == numCheck)
		{
			virtualgeneric_t tmp;
			tmp.group = group;
			tmp.index = j;
			k = attachment.AddToTail( tmp );

			const studiohdr_t *firstHdr = m_group[ 0 ].GetStudioHdr();
			// make sure bone flags are set so attachment calculates
			if ((firstHdr->pBone( n )->flags & BONE_USED_BY_ATTACHMENT) == 0)
			{
				while (n != -1)
				{
					firstHdr->pBone( n )->flags |= BONE_USED_BY_ATTACHMENT;

					if (firstHdr->pLinearBones())
					{
						*firstHdr->pLinearBones()->pflags( n ) |= BONE_USED_BY_ATTACHMENT;
					}

					n = firstHdr->pBone( n )->parent;
				}
				continue;
			}
		}

		vgroup.masterAttachment[ j ] = k;
	}

	m_attachment.Swap(attachment);
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void virtualmodel_t::AppendPoseParameters( intp group, const studiohdr_t *pStudioHdr )
{
	AUTO_LOCK( m_Lock );
	const intp numCheck = m_pose.Count();

	CUtlVector< virtualgeneric_t > pose;
	pose = m_pose;

	MEM_ALLOC_CREDIT();

	auto &vgroup = m_group[group];
	vgroup.masterPose.SetCount( pStudioHdr->numlocalposeparameters );

	for (int j = 0; j < pStudioHdr->numlocalposeparameters; j++)
	{
		const char *s1 = pStudioHdr->pLocalPoseParameter( j )->pszName();
		intp k;
		for (k = 0; k < numCheck; k++)
		{
			const auto &vpose = pose[k];
			const char *s2 = m_group[ vpose.group ].GetStudioHdr()->pLocalPoseParameter( vpose.index )->pszName();

			if (stricmp( s1, s2 ) == 0)
			{
				break;
			}
		}

		if (k == numCheck)
		{
			// no duplication
			virtualgeneric_t tmp;
			tmp.group = group;
			tmp.index = j;
			k = pose.AddToTail( tmp );
		}
		else
		{
			// duplicate, reset start and end to fit full dynamic range
			mstudioposeparamdesc_t *pPose1 = pStudioHdr->pLocalPoseParameter( j );
			const auto &vpose = pose[k];
			mstudioposeparamdesc_t *pPose2 = m_group[ vpose.group ].GetStudioHdr()->pLocalPoseParameter( vpose.index );
			const float start =  min( pPose2->end, min( pPose1->end, min( pPose2->start, pPose1->start ) ) );
			const float end =  max( pPose2->end, max( pPose1->end, max( pPose2->start, pPose1->start ) ) );
			pPose2->start = start;
			pPose2->end = end;
		}

		vgroup.masterPose[j] = k;
	}

	m_pose.Swap(pose);
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void virtualmodel_t::AppendNodes( intp group, const studiohdr_t *pStudioHdr )
{
	AUTO_LOCK( m_Lock );
	const intp numCheck = m_node.Count();

	CUtlVector< virtualgeneric_t > node;
	node = m_node;

	MEM_ALLOC_CREDIT();

	auto &vgroup = m_group[ group ];
	vgroup.masterNode.SetCount( pStudioHdr->numlocalnodes );

	for (int j = 0; j < pStudioHdr->numlocalnodes; j++)
	{
		const char *s1 = pStudioHdr->pszLocalNodeName( j );
		intp k;
		for (k = 0; k < numCheck; k++)
		{
			const auto &vnode = node[k];
			const char *s2 = m_group[ vnode.group ].GetStudioHdr()->pszLocalNodeName( vnode.index );

			if (stricmp( s1, s2 ) == 0)
			{
				break;
			}
		}

		// no duplication
		if (k == numCheck)
		{
			virtualgeneric_t tmp;
			tmp.group = group;
			tmp.index = j;
			k = node.AddToTail( tmp );
		}

		vgroup.masterNode[ j ] = k;
	}

	m_node.Swap(node);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------


void virtualmodel_t::AppendIKLocks( intp group, const studiohdr_t *pStudioHdr )
{
	AUTO_LOCK( m_Lock );
	const intp numCheck = m_iklock.Count();

	CUtlVector< virtualgeneric_t > iklock;
	iklock = m_iklock;

	for (int j = 0; j < pStudioHdr->numlocalikautoplaylocks; j++)
	{
		const int chain1 = pStudioHdr->pLocalIKAutoplayLock( j )->chain;
		intp k;
		for (k = 0; k < numCheck; k++)
		{
			const auto &vlock = iklock[k];
			const int chain2 = m_group[ vlock.group ].GetStudioHdr()->pLocalIKAutoplayLock( vlock.index )->chain;

			if (chain1 == chain2)
			{
				break;
			}
		}

		// no duplication
		if (k == numCheck)
		{
			MEM_ALLOC_CREDIT();

			virtualgeneric_t tmp;
			tmp.group = group;
			tmp.index = j;
			k = iklock.AddToTail( tmp );
		}
	}

	m_iklock.Swap(iklock);

	// copy knee directions for uninitialized knees
	if ( group != 0 )
	{
		studiohdr_t *pBaseHdr = m_group[ 0 ].GetStudioHdr();
		if ( pStudioHdr->numikchains == pBaseHdr->numikchains )
		{
			for (int j = 0; j < pStudioHdr->numikchains; j++)
			{
				Vector &baseKneeDir = pBaseHdr->pIKChain( j )->pLink(0)->kneeDir;
				if ( baseKneeDir.LengthSqr() == 0.0f )
				{
					const Vector &studioKneeDir = pStudioHdr->pIKChain( j )->pLink(0)->kneeDir;
					if ( studioKneeDir.LengthSqr() > 0.0f )
					{
						baseKneeDir = studioKneeDir;
					}
				}
			}
		}
	}
}
