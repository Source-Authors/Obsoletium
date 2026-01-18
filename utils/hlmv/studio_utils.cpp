//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// updates:
// 1-4-99	fixed file texture load and file read bug

////////////////////////////////////////////////////////////////////////
#include "StudioModel.h"
#include "vphysics/constraints.h"
#include "physmesh.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterial.h"
#include "ViewerSettings.h"
#include "bone_setup.h"
#include "tier1/utldict.h"
#include "tier1/utlmemory.h"
#include "mxtk/mx.h"
#include "filesystem.h"
#include "IStudioRender.h"
#include "materialsystem/IMaterialSystemHardwareConfig.h"
#include "MDLViewer.h"
#include "optimize.h"

extern char g_appTitle[];
Vector *StudioModel::m_AmbientLightColors;

static StudioModel g_studioModel;
static StudioModel *g_pActiveModel;

// Expose it to the rest of the app
StudioModel *g_pStudioModel = &g_studioModel;
StudioModel *g_pStudioExtraModel[HLMV_MAX_MERGED_MODELS];

StudioModel::StudioModel()
{
	m_MDLHandle = MDLHANDLE_INVALID;
	ClearLookTargets();
}

void StudioModel::Init()
{
	m_AmbientLightColors = new Vector[g_pStudioRender->GetNumAmbientLightSamples()];
	
	// JasonM & garymcthack - should really only do this once a frame and at init time.
	UpdateStudioRenderConfig( g_viewerSettings.renderMode == RM_WIREFRAME, false,
							  g_viewerSettings.showNormals,
							  g_viewerSettings.showTangentFrame );
}

void StudioModel::Shutdown( void )
{
	g_pStudioModel->FreeModel( false );
	delete [] m_AmbientLightColors;
}

void StudioModel::SetCurrentModel()
{
	// track the correct model
	g_pActiveModel = this;
}

void StudioModel::ReleaseStudioModel()
{
	SaveViewerSettings( g_pStudioModel->GetFileName(), g_pStudioModel );
	g_pStudioModel->FreeModel( true ); 
}

void StudioModel::RestoreStudioModel()
{
	// should view settings be loaded before the model is loaded?
	if ( g_pStudioModel->LoadModel( g_pStudioModel->m_pModelName ) )
	{
		g_pStudioModel->PostLoadModel( g_pStudioModel->m_pModelName );
	}
}



//-----------------------------------------------------------------------------
// Purpose: Frees the model data and releases textures from OpenGL.
//-----------------------------------------------------------------------------
void StudioModel::FreeModel( bool bReleasing )
{
		delete m_pStudioHdr;
		m_pStudioHdr = NULL;

	if ( m_MDLHandle != MDLHANDLE_INVALID )
	{
		g_pMDLCache->Release( m_MDLHandle );
		m_MDLHandle = MDLHANDLE_INVALID;
	}

	if ( !bReleasing )
	{
			delete[] m_pModelName;
			m_pModelName = NULL;
		}

	m_SurfaceProps.Purge();

	DestroyPhysics( m_pPhysics );
	m_pPhysics = NULL;
}

void *StudioModel::operator new( size_t stAllocateBlock )
{
	// call into engine to get memory
	Assert( stAllocateBlock != 0 );
	return calloc( 1, stAllocateBlock );
}

// dimhotepus: Ensure compiler doesn't drop memset.
const volatile auto secureMemset = &memset;

void StudioModel::operator delete( void *pMem )
{
#ifdef _DEBUG
	// set the memory to a known value
	size_t size = _msize( pMem );
	secureMemset( pMem, 0xcd, size );
#endif

	// get the engine to free the memory
	free( pMem );
}

void *StudioModel::operator new( size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine )
{
	// call into engine to get memory
	Assert( stAllocateBlock != 0 );
	return calloc( 1, stAllocateBlock );
}

void StudioModel::operator delete( void *pMem, int nBlockUse, const char *pFileName, int nLine )
{
#ifdef _DEBUG
	// set the memory to a known value
	size_t size = _msize( pMem );
	secureMemset( pMem, 0xcd, size );
#endif
	// get the engine to free the memory
	free( pMem );
}

static CUtlVector<CUtlSymbol> g_GlobalFlexControllers;
static CUtlDict<intp, intp> g_GlobalFlexControllerLookup;

//-----------------------------------------------------------------------------
// Purpose: Accumulates throughout runtime session, oh well
// Input  : *szName - 
// Output : int
//-----------------------------------------------------------------------------
static intp AddGlobalFlexController( StudioModel *model, const char *szName )
{
	auto idx = g_GlobalFlexControllerLookup.Find( szName );
	if ( idx != g_GlobalFlexControllerLookup.InvalidIndex() )
	{
		return g_GlobalFlexControllerLookup[ idx ];
	}

	idx = g_GlobalFlexControllers.AddToTail( CUtlSymbol{szName} );
	g_GlobalFlexControllerLookup.Insert( szName, idx );
	// Con_Printf( "Added global flex controller %i %s from %s\n", idx, szName, model->GetStudioHdr()->name );
	return idx;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *model - 
//-----------------------------------------------------------------------------
static void SetupModelFlexcontrollerLinks( StudioModel *model )
{
	if ( !model )
		return;

	CStudioHdr *hdr = model->GetStudioHdr();
	if ( !hdr )
		return;

	if ( hdr->numflexcontrollers() <= 0 )
		return;

	// Already set up!!!
	if ( hdr->pFlexcontroller( LocalFlexController_t(0) )->localToGlobal != -1 )
		return;

	for (auto i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); i++)
	{
		auto j = AddGlobalFlexController( model, hdr->pFlexcontroller( i )->pszName() );
		hdr->pFlexcontroller( i )->localToGlobal = size_cast<int>( j );
		model->SetFlexController( i, 0.0f );
	}
}

bool StudioModel::LoadModel( const char *pModelName )
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	if (!pModelName)
		return 0;

	// In the case of restore, m_pModelName == modelname
	if (m_pModelName != pModelName)
	{
		// Copy over the model name; we'll need it later...
		delete[] m_pModelName;
		m_pModelName = V_strdup( pModelName );
	}

	m_MDLHandle = g_pMDLCache->FindMDL( pModelName );

	// allocate a pool for a studiohdr cache
		delete m_pStudioHdr;
	m_pStudioHdr = new CStudioHdr( g_pMDLCache->GetStudioHdr( m_MDLHandle ), g_pMDLCache );

	// mandatory to access correct verts
	SetCurrentModel();

	m_pPhysics = LoadPhysics( m_MDLHandle );

	// Copy over all of the hitboxes; we may add and remove elements
	m_HitboxSets.RemoveAll();

	CStudioHdr *pStudioHdr = GetStudioHdr();

	for ( int s = 0; s < pStudioHdr->numhitboxsets(); s++ )
	{
		mstudiohitboxset_t *pSrcSet = pStudioHdr->pHitboxSet( s );
		if ( !pSrcSet )
			continue;

		intp j = m_HitboxSets.AddToTail();
		HitboxSet_t &set = m_HitboxSets[j];
		set.m_Name = pSrcSet->pszName();

		for ( int i = 0; i < pSrcSet->numhitboxes; ++i )
		{
			mstudiobbox_t *pHit = pSrcSet->pHitbox(i);
			auto nIndex = set.m_Hitboxes.AddToTail( );
			HitboxInfo_t &hitbox = set.m_Hitboxes[nIndex];

			hitbox.m_Name = pHit->pszHitboxName();
			hitbox.m_BBox = *pHit;

			// Blat out bbox name index so we don't use it by mistake...
			hitbox.m_BBox.szhitboxnameindex = 0;
		}
	}

	// Copy over all of the surface props; we may change them...
	for ( int i = 0; i < pStudioHdr->numbones(); ++i )
	{
		mstudiobone_t* pBone = pStudioHdr->pBone(i);

		CUtlSymbol prop( pBone->pszSurfaceProp() );
		m_SurfaceProps.AddToTail( prop );
	}

	m_physPreviewBone = -1;

	bool forceOpaque = (pStudioHdr->flags() & STUDIOHDR_FLAGS_FORCE_OPAQUE) != 0;
	bool vertexLit = false;
	m_bIsTransparent = false;
	m_bHasProxy = false;

	studiohwdata_t *pHardwareData = g_pMDLCache->GetHardwareData( m_MDLHandle );
	if ( !pHardwareData )
	{
		Assert( 0 );
		return false;
	}

	for( int lodID = pHardwareData->m_RootLOD; lodID < pHardwareData->m_NumLODs; lodID++ )
	{
		studioloddata_t *pLODData = &pHardwareData->m_pLODs[lodID];
		for ( int i = 0; i < pLODData->numMaterials; ++i )
		{
			if (pLODData->ppMaterials[i]->IsVertexLit())
			{
				vertexLit = true;
			}
			if ((!forceOpaque) && pLODData->ppMaterials[i]->IsTranslucent())
			{
				m_bIsTransparent = true;
				//Msg("Translucent material %s for model %s\n", pLODData->ppMaterials[i]->GetName(), pStudioHdr->name );
			}
			if (pLODData->ppMaterials[i]->HasProxy())
			{
				m_bHasProxy = true;
			}
		}
	}

	SetupModelFlexcontrollerLinks(this);

	return true;
}



bool StudioModel::PostLoadModel( const char *modelname )
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (pStudioHdr == NULL)
		return false;

	SetSequence ((intp)0);
	SetController (0, 0.0f);
	SetController (1, 0.0f);
	SetController (2, 0.0f);
	SetController (3, 0.0f);
	SetBlendTime( DEFAULT_BLEND_TIME );
	// SetHeadTurn( 1.0f );  // FIXME:!!!

	for (int n = 0; n < pStudioHdr->numbodyparts(); n++)
	{
		SetBodygroup (n, 0);
	}

	SetSkin (0);

	return true;
}


//------------------------------------------------------------------------------
// Returns true if the model has at least one body part with model data, false if not.
//------------------------------------------------------------------------------
bool StudioModel::HasModel()
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return false;
		
	for ( int i = 0; i < pStudioHdr->numbodyparts(); i++ )
	{
		if ( pStudioHdr->pBodypart(i)->nummodels )
		{
			return true;
		}
	}

	return false;
}


////////////////////////////////////////////////////////////////////////


intp StudioModel::GetSequence( ) const
{
	return m_sequence;
}

intp StudioModel::SetSequence( intp iSequence )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return 0;

	if (iSequence < 0)
		return 0;

	if (iSequence > pStudioHdr->GetNumSeq())
		return m_sequence;

	m_prevsequence = m_sequence;
	m_sequence = iSequence;
	m_cycle = 0;
	m_sequencetime = 0.0f;

	return m_sequence;
}

const char* StudioModel::GetSequenceName( intp iSequence )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return NULL;

	if (iSequence < 0)
		return NULL;

	if (iSequence > pStudioHdr->GetNumSeq())
		return NULL;

	return pStudioHdr->pSeqdesc( iSequence ).pszLabel();
}

void StudioModel::ClearOverlaysSequences( void )
{
	ClearAnimationLayers( );
	BitwiseClear( m_Layer );
}

void StudioModel::ClearAnimationLayers( void )
{
	m_iActiveLayers = 0;
}

int	StudioModel::GetNewAnimationLayer( int iPriority )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return 0;

	if ( m_iActiveLayers >= MAXSTUDIOANIMLAYERS )
	{
		Assert( 0 );
		return MAXSTUDIOANIMLAYERS - 1;
	}

	m_Layer[m_iActiveLayers].m_priority = iPriority;

	return m_iActiveLayers++;
}

int StudioModel::SetOverlaySequence( int iLayer, intp iSequence, float flWeight )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return 0;

	if (iSequence < 0)
		return 0;

	if (iLayer < 0 || iLayer >= MAXSTUDIOANIMLAYERS)
	{
		Assert(0);
		return 0;
	}

	if (iSequence > pStudioHdr->GetNumSeq())
		return m_Layer[iLayer].m_sequence;

	m_Layer[iLayer].m_sequence = iSequence;
	m_Layer[iLayer].m_weight = flWeight;
	m_Layer[iLayer].m_playbackrate = 1.0;

	return iSequence;
}

float StudioModel::SetOverlayRate( int iLayer, float flCycle, float flPlaybackRate )
{
	if (iLayer >= 0 && iLayer < MAXSTUDIOANIMLAYERS)
	{
		m_Layer[iLayer].m_cycle = flCycle;
		m_Layer[iLayer].m_playbackrate = flPlaybackRate;
	}
	return flCycle;
}


int StudioModel::GetOverlaySequence( int iLayer )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return -1;

	if (iLayer < 0 || iLayer >= MAXSTUDIOANIMLAYERS)
	{
		Assert(0);
		return 0;
	}

	return m_Layer[iLayer].m_sequence;
}


float StudioModel::GetOverlaySequenceWeight( int iLayer )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return -1;

	if (iLayer < 0 || iLayer >= MAXSTUDIOANIMLAYERS)
	{
		Assert(0);
		return 0;
	}

	return m_Layer[iLayer].m_weight;
}


intp StudioModel::LookupSequence( const char *szSequence ) const
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return -1;

	for (intp i = 0; i < pStudioHdr->GetNumSeq(); i++)
	{
		if (!stricmp( szSequence, pStudioHdr->pSeqdesc( i ).pszLabel() ))
		{
			return i;
		}
	}
	return -1;
}

intp StudioModel::LookupActivity( const char *szActivity ) const
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return -1;

	for (intp i = 0; i < pStudioHdr->GetNumSeq(); i++)
	{
		if (!stricmp( szActivity, pStudioHdr->pSeqdesc( i ).pszActivityName() ))
		{
			return i;
		}
	}
	return -1;
}

intp StudioModel::SetSequence( const char *szSequence )
{
	return SetSequence( LookupSequence( szSequence ) );
}

void StudioModel::StartBlending( void )
{
	// Switch back to old sequence ( this will oscillate between this one and the last one )
	SetSequence( m_prevsequence );
}

void StudioModel::SetBlendTime( float blendtime )
{
	if ( blendtime > 0.0f )
	{
		m_blendtime = blendtime;
	}
}

float StudioModel::GetTransitionAmount( void )
{
	if ( g_viewerSettings.blendSequenceChanges &&
		m_sequencetime < m_blendtime && m_prevsequence != m_sequence )
	{
		float s = m_sequencetime / m_blendtime;
		return s;
	}

	return 0.0f;
}

LocalFlexController_t StudioModel::LookupFlexController( char *szName )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return LocalFlexController_t(0);

	for (auto iFlex = LocalFlexController_t(0); iFlex < pStudioHdr->numflexcontrollers(); iFlex++)
	{
		if (stricmp( szName, pStudioHdr->pFlexcontroller( iFlex )->pszName() ) == 0)
		{
			return iFlex;
		}
	}
	return LocalFlexController_t(-1);
}


void StudioModel::SetFlexController( char *szName, float flValue )
{
	SetFlexController( LookupFlexController( szName ), flValue );
}

void StudioModel::SetFlexController( LocalFlexController_t iFlex, float flValue )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return;

	if (iFlex >= 0 && iFlex < pStudioHdr->numflexcontrollers())
	{
		mstudioflexcontroller_t *pflex = pStudioHdr->pFlexcontroller(iFlex);

		if (pflex->min != pflex->max)
		{
			flValue = (flValue - pflex->min) / (pflex->max - pflex->min);
		}
		m_flexweight[iFlex] = clamp( flValue, 0.0f, 1.0f );
	}
}


void StudioModel::SetFlexControllerRaw( LocalFlexController_t iFlex, float flValue )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return;

	if (iFlex >= 0 && iFlex < pStudioHdr->numflexcontrollers())
	{
//		mstudioflexcontroller_t *pflex = pStudioHdr->pFlexcontroller(iFlex);
		m_flexweight[iFlex] = clamp( flValue, 0.0f, 1.0f );
	}
}

float StudioModel::GetFlexController( char *szName )
{
	return GetFlexController( LookupFlexController( szName ) );
}

float StudioModel::GetFlexController( LocalFlexController_t iFlex )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return 0.0f;

	if (iFlex >= 0 && iFlex < pStudioHdr->numflexcontrollers())
	{
		mstudioflexcontroller_t *pflex = pStudioHdr->pFlexcontroller(iFlex);

		float flValue = m_flexweight[iFlex];

		if (pflex->min != pflex->max)
		{
			flValue = flValue * (pflex->max - pflex->min) + pflex->min;
		}
		return flValue;
	}
	return 0.0;
}


float StudioModel::GetFlexControllerRaw( LocalFlexController_t iFlex )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return 0.0f;

	if (iFlex >= 0 && iFlex < pStudioHdr->numflexcontrollers())
	{
//		mstudioflexcontroller_t *pflex = pStudioHdr->pFlexcontroller(iFlex);
		return m_flexweight[iFlex];
	}
	return 0.0;
}

int StudioModel::GetNumLODs() const
{
	return g_pStudioRender->GetNumLODs( *GetHardwareData() );
}

float StudioModel::GetLODSwitchValue( int lod ) const
{
	return g_pStudioRender->GetLODSwitchValue( *GetHardwareData(), lod );
}

void StudioModel::SetLODSwitchValue( int lod, float switchValue )
{
	g_pStudioRender->SetLODSwitchValue( *GetHardwareData(), lod, switchValue );
}

void StudioModel::ExtractBbox( Vector &mins, Vector &maxs )
{
	studiohdr_t *pStudioHdr = GetStudioRenderHdr();
	if ( !pStudioHdr )
		return;

	// look for hull
	if ( pStudioHdr->hull_min.Length() != 0 )
	{
		mins = pStudioHdr->hull_min;
		maxs = pStudioHdr->hull_max;
	}
	// look for view clip
	else if (pStudioHdr->view_bbmin.Length() != 0)
	{
		mins = pStudioHdr->view_bbmin;
		maxs = pStudioHdr->view_bbmax;
	}
	else
	{
		mstudioseqdesc_t &pseqdesc = pStudioHdr->pSeqdesc( m_sequence );

		mins = pseqdesc.bbmin;
		maxs = pseqdesc.bbmax;
	}
}



void StudioModel::GetSequenceInfo( intp iSequence, float *pflFrameRate, float *pflGroundSpeed )
{
	float t = GetDuration( iSequence );

	if (t > 0)
	{
		*pflFrameRate = 1.0f / t;
	}
	else
	{
		*pflFrameRate = 1.0f;
	}
	*pflGroundSpeed = GetGroundSpeed( iSequence );
}

void StudioModel::GetSequenceInfo( float *pflFrameRate, float *pflGroundSpeed )
{
	GetSequenceInfo( m_sequence, pflFrameRate, pflGroundSpeed );
}

float StudioModel::GetFPS( intp iSequence )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return 0.0f;

	return Studio_FPS( pStudioHdr, iSequence, m_poseparameter );
}

float StudioModel::GetFPS( void )
{
	return GetFPS( m_sequence );
}

float StudioModel::GetDuration( intp iSequence )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return 0.0f;

	return Studio_Duration( pStudioHdr, iSequence, m_poseparameter );
}


int StudioModel::GetNumFrames( intp iSequence )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr || iSequence < 0 || iSequence >= pStudioHdr->GetNumSeq() )
	{
		return 1;
	}

	return Studio_MaxFrame( pStudioHdr, iSequence, m_poseparameter );
}

static int GetSequenceFlags( CStudioHdr *pstudiohdr, int sequence )
{
	if ( !pstudiohdr || 
		sequence < 0 || 
		sequence >= pstudiohdr->GetNumSeq() )
	{
		return 0;
	}

	const mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( sequence );

	return seqdesc.flags;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iSequence - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool StudioModel::GetSequenceLoops( intp iSequence )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return false;

	int flags = GetSequenceFlags( pStudioHdr, iSequence );
	bool looping = flags & STUDIO_LOOPING ? true : false;
	return looping;
}

float StudioModel::GetDuration( )
{
	return GetDuration( m_sequence );
}


void StudioModel::GetMovement( float prevcycle[5], Vector &vecPos, QAngle &vecAngles )
{
	vecPos.Init();
	vecAngles.Init();

	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return;

  	// assume that changes < -0.5 are loops....
  	if (m_cycle - prevcycle[0] < -0.5f)
  	{
		prevcycle[0] -= 1.0f;
  	}

	Studio_SeqMovement( pStudioHdr, m_sequence, prevcycle[0], m_cycle, m_poseparameter, vecPos, vecAngles );
	prevcycle[0] = m_cycle;

		Vector vecTmp;
		QAngle angTmp;

	for (int i = 0; i < 4; i++)
	{
  		if (m_Layer[i].m_cycle - prevcycle[i+1] < -0.5f)
  		{
			prevcycle[i+1] -= 1.0f;
  		}

		if (m_Layer[i].m_weight > 0.0)
		{
			vecTmp.Init();
			angTmp.Init();
			if (Studio_SeqMovement( pStudioHdr, m_Layer[i].m_sequence, prevcycle[i+1], m_Layer[i].m_cycle, m_poseparameter, vecTmp, angTmp ))
			{
				vecPos = vecPos * ( 1.0f - m_Layer[i].m_weight ) + vecTmp * m_Layer[i].m_weight;
			}
		}
		prevcycle[i+1] = m_Layer[i].m_cycle;
	}

	return;
}


void StudioModel::GetMovement( intp iSequence, float prevCycle, float nextCycle, Vector &vecPos, QAngle &vecAngles )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
	{
		vecPos.Init();
		vecAngles.Init();
		return;
	}

	// FIXME: this doesn't consider layers
	Studio_SeqMovement( pStudioHdr, iSequence, prevCycle, nextCycle, m_poseparameter, vecPos, vecAngles );

	return;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the ground speed of the specifed sequence.
//-----------------------------------------------------------------------------
float StudioModel::GetGroundSpeed( int iSequence )
{
	Vector vecMove;
	QAngle vecAngles;
	GetMovement( iSequence, 0, 1, vecMove, vecAngles );

	float t = GetDuration( iSequence );

	float flGroundSpeed = 0;
	if (t > 0)
	{
		flGroundSpeed = vecMove.Length() / t;
	}

	return flGroundSpeed;
}



//-----------------------------------------------------------------------------
// Purpose: Returns the ground speed of the current sequence.
//-----------------------------------------------------------------------------
float StudioModel::GetGroundSpeed( void )
{
	return GetGroundSpeed( m_sequence );
}


//-----------------------------------------------------------------------------
// Purpose: Returns the ground speed of the current sequence.
//-----------------------------------------------------------------------------
float StudioModel::GetCurrentVelocity( void )
{
	Vector vecVelocity;

	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (pStudioHdr && Studio_SeqVelocity( pStudioHdr, m_sequence, m_cycle, m_poseparameter, vecVelocity ))
	{
		return vecVelocity.Length();
	}
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the the sequence should be hidden or not
//-----------------------------------------------------------------------------
bool StudioModel::IsHidden( int iSequence )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (pStudioHdr->pSeqdesc( iSequence ).flags & STUDIO_HIDDEN)
		return true;

	return false;
}



void StudioModel::GetSeqAnims( intp iSequence, const mstudioanimdesc_t *panim[4], float *weight )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return;

	const mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc( iSequence );
	Studio_SeqAnims( pStudioHdr, seqdesc, iSequence, m_poseparameter, panim, weight );
}

void StudioModel::GetSeqAnims( const mstudioanimdesc_t *panim[4], float *weight )
{
	GetSeqAnims( m_sequence, panim, weight );
}


float StudioModel::SetController( int iController, float flValue )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return 0.0f;

	return Studio_SetController( pStudioHdr, iController, flValue, m_controller[iController] );
}



int	StudioModel::LookupPoseParameter( char const *szName )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return false;

	for (intp iParameter = 0; iParameter < pStudioHdr->GetNumPoseParameters(); iParameter++)
	{
		if (stricmp( szName, pStudioHdr->pPoseParameter( iParameter ).pszName() ) == 0)
		{
			return iParameter;
		}
	}
	return -1;
}

float StudioModel::SetPoseParameter( char const *szName, float flValue )
{
	return SetPoseParameter( LookupPoseParameter( szName ), flValue );
}

float StudioModel::SetPoseParameter( int iParameter, float flValue )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return 0.0f;

	return Studio_SetPoseParameter( pStudioHdr, iParameter, flValue, m_poseparameter[iParameter] );
}

float StudioModel::GetPoseParameter( char const *szName )
{
	return GetPoseParameter( LookupPoseParameter( szName ) );
}

float* StudioModel::GetPoseParameters()
{
	return m_poseparameter;
}

float StudioModel::GetPoseParameter( int iParameter )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return 0.0f;

	return Studio_GetPoseParameter( pStudioHdr, iParameter, m_poseparameter[iParameter] );
}

bool StudioModel::GetPoseParameterRange( int iParameter, float *pflMin, float *pflMax )
{
	*pflMin = 0;
	*pflMax = 0;

	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return false;

	if (iParameter < 0 || iParameter >= pStudioHdr->GetNumPoseParameters())
		return false;

	const mstudioposeparamdesc_t &Pose = pStudioHdr->pPoseParameter( iParameter );

	*pflMin = Pose.start;
	*pflMax = Pose.end;

	return true;
}

int StudioModel::LookupAttachment( char const *szName )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return -1;

	for (int i = 0; i < pStudioHdr->GetNumAttachments(); i++)
	{
		if (stricmp( pStudioHdr->pAttachment( i ).pszName(), szName ) == 0)
		{
			return i;
		}
	}
	return -1;
}



int StudioModel::SetBodygroup( int iGroup, int iValue /*= -1*/ )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return 0;

	if (iGroup > pStudioHdr->numbodyparts())
		return -1;

	mstudiobodyparts_t *pbodypart = pStudioHdr->pBodypart( iGroup );

	int iCurrent = (m_bodynum / pbodypart->base) % pbodypart->nummodels;

	// if the submodel index is not specified or out of range, just use the current value
	if ( iValue < 0 || iValue >= pbodypart->nummodels )
		return iCurrent;

	m_bodynum = (m_bodynum - (iCurrent * pbodypart->base) + (iValue * pbodypart->base));

	return iValue;
}


int StudioModel::SetSkin( int iValue )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return 0;

	if (iValue >= pStudioHdr->numskinfamilies())
	{
		return m_skinnum;
	}

	m_skinnum = iValue;

	return iValue;
}



void StudioModel::scaleMeshes (float scale)
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return;

	// manadatory to access correct verts
	SetCurrentModel();

	// scale verts
	int tmp = m_bodynum;
	for (int i = 0; i < pStudioHdr->numbodyparts(); i++)
	{
		mstudiobodyparts_t *pbodypart = pStudioHdr->pBodypart( i );
		for (int j = 0; j < pbodypart->nummodels; j++)
		{
			SetBodygroup (i, j);
			SetupModel (i);

			const mstudio_modelvertexdata_t *vertData = m_pmodel->GetVertexData();
			Assert( vertData ); // This can only return NULL on X360 for now

			for (int k = 0; k < m_pmodel->numvertices; k++)
			{
				*vertData->Position(k) *= scale;
			}
		}
	}

	m_bodynum = tmp;

	// scale complex hitboxes
	int hitboxset = g_MDLViewer->GetCurrentHitboxSet();

	mstudiobbox_t *pbboxes = pStudioHdr->pHitbox( 0, hitboxset );
	for (int i = 0; i < pStudioHdr->iHitboxCount( hitboxset ); i++)
	{
		VectorScale (pbboxes[i].bbmin, scale, pbboxes[i].bbmin);
		VectorScale (pbboxes[i].bbmax, scale, pbboxes[i].bbmax);
	}

	// scale bounding boxes
	for (intp i = 0; i < pStudioHdr->GetNumSeq(); i++)
	{
		mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc( i );
		Vector tmpmin = seqdesc.bbmin;
		VectorScale( tmpmin, scale, tmpmin );
		seqdesc.bbmin = tmpmin;

		tmpmin = seqdesc.bbmax;
		VectorScale( tmpmin, scale, tmpmin );
		seqdesc.bbmax = tmpmin;
	}

	// maybe scale exposition, pivots, attachments
}



void StudioModel::scaleBones (float scale)
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return;

	mstudiobone_t *pbones = pStudioHdr->pBone( 0 );
	for (int i = 0; i < pStudioHdr->numbones(); i++)
	{
		pbones[i].pos *= scale;
		pbones[i].posscale *= scale;
	}	
}

intp StudioModel::Physics_GetBoneCount( void )
{
	return m_pPhysics->Count();
}


const char *StudioModel::Physics_GetBoneName( intp index ) 
{ 
	CPhysmesh *pmesh = m_pPhysics->GetMesh( index );

	if ( !pmesh )
		return nullptr;

	return pmesh->m_boneName;
}


void StudioModel::Physics_GetData( int boneIndex, hlmvsolid_t *psolid, constraint_ragdollparams_t *pConstraint ) const
{
	CPhysmesh *pMesh = m_pPhysics->GetMesh( boneIndex );
	
	if ( !pMesh )
		return;

	if ( psolid )
	{
		memcpy( psolid, &pMesh->m_solid, sizeof(*psolid) );
	}

	if ( pConstraint )
	{
		*pConstraint = pMesh->m_constraint;
	}
}

void StudioModel::Physics_SetData( int boneIndex, const hlmvsolid_t *psolid, const constraint_ragdollparams_t *pConstraint )
{
	CPhysmesh *pMesh = m_pPhysics->GetMesh( boneIndex );
	
	if ( !pMesh )
		return;

	if ( psolid )
	{
		memcpy( &pMesh->m_solid, psolid, sizeof(*psolid) );
	}

	if ( pConstraint )
	{
		pMesh->m_constraint = *pConstraint;
	}
}


float StudioModel::Physics_GetMass( void )
{
	return m_pPhysics->GetMass();
}

void StudioModel::Physics_SetMass( float mass )
{
	m_physMass = mass;
}


char *StudioModel::Physics_DumpQC( void )
{
	return m_pPhysics->DumpQC();
}

const vertexFileHeader_t * mstudiomodel_t::CacheVertexData( void * pModelData )
{
	Assert( pModelData == NULL );
	Assert( g_pActiveModel );

	return g_pStudioDataCache->CacheVertexData( g_pActiveModel->GetStudioRenderHdr() );
}


//-----------------------------------------------------------------------------
// FIXME: This trashy glue code is really not acceptable. Figure out a way of making it unnecessary.
//-----------------------------------------------------------------------------
const studiohdr_t *studiohdr_t::FindModel( void **cache, char const *pModelName ) const
{
	MDLHandle_t handle = g_pMDLCache->FindMDL( pModelName );
	*cache = MDLHandleToVirtual( handle );
	return g_pMDLCache->GetStudioHdr( handle );
}

virtualmodel_t *studiohdr_t::GetVirtualModel( void ) const
{
	return g_pMDLCache->GetVirtualModel( VoidPtrToMDLHandle( VirtualModel() ) );
}

byte *studiohdr_t::GetAnimBlock( intp i ) const
{
	return g_pMDLCache->GetAnimBlock( VoidPtrToMDLHandle( VirtualModel() ), i );
}

intp studiohdr_t::GetAutoplayList( unsigned short **pOut ) const
{
	return g_pMDLCache->GetAutoplayList( VoidPtrToMDLHandle( VirtualModel() ), pOut );
}

const studiohdr_t *virtualgroup_t::GetStudioHdr( void ) const
{
	Assert((intp)cache <= 0xFFFF);
	return g_pMDLCache->GetStudioHdr( VoidPtrToMDLHandle( cache ) );
}

// dimhotepus: Add const-correct API.
studiohdr_t *virtualgroup_t::GetStudioHdr( void )
{
	Assert((intp)cache <= 0xFFFF);
	return g_pMDLCache->GetStudioHdr( VoidPtrToMDLHandle( cache ) );
}
