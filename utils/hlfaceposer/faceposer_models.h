//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef FACEPOSER_MODELS_H
#define FACEPOSER_MODELS_H
#ifdef _WIN32
#pragma once
#endif

class StudioModel;
class CStudioHdr;

#include "mxbitmaptools.h"
#include "tier0/platform.h"
#include "tier1/utlrbtree.h"
#include "tier1/utlvector.h"

typedef unsigned int CRC32_t;

class IFaceposerModels
{
public:
						IFaceposerModels();
	virtual				~IFaceposerModels();

	virtual intp		Count( void ) const;
	virtual char const	*GetModelName( intp index );
	virtual char const	*GetModelFileName( intp index );
	virtual intp		GetActiveModelIndex( void ) const;
	virtual char const	*GetActiveModelName( void );
	virtual StudioModel *GetActiveStudioModel( void );
	virtual void		ForceActiveModelIndex( intp index );
	virtual void		UnForceActiveModelIndex();
	virtual intp		FindModelByFilename( char const *filename );

	virtual intp		LoadModel( char const *filename );
	virtual void		FreeModel( intp index );

	virtual void		CloseAllModels( void );

	virtual StudioModel *GetStudioModel( intp index );
	virtual CStudioHdr	*GetStudioHeader( intp index );
	virtual intp		GetIndexForStudioModel( StudioModel *model );

	virtual intp		GetModelIndexForActor( char const *actorname );
	virtual StudioModel *GetModelForActor( char const *actorname );

	virtual char const *GetActorNameForModel( intp modelindex );
	virtual void		SetActorNameForModel( intp modelindex, char const *actorname );

	virtual intp		CountVisibleModels( void );
	virtual void		ShowModelIn3DView( intp modelindex, bool show );
	virtual bool		IsModelShownIn3DView( intp modelindex );

	virtual void		SaveModelList( void );
	virtual void		LoadModelList( void );

	virtual void		ReleaseModels( void );
	virtual void		RestoreModels( void );
	
	//virtual void		RefreshModels( void );

	virtual void		CheckResetFlexes( void );
	virtual void		ClearOverlaysSequences( void );

	virtual mxbitmapdata_t *GetBitmapForSequence( intp modelindex, intp sequence );

	virtual void		RecreateAllAnimationBitmaps( intp modelindex );
	virtual void		RecreateAnimationBitmap( intp modelindex, intp sequence );

	virtual void		CreateNewBitmap( intp modelindex, char const *pchBitmapFilename, intp sequence, int nSnapShotSize, bool bZoomInOnFace, class CExpression *pExpression, mxbitmapdata_t *bitmap );

	virtual intp		CountActiveSources();

	virtual void		SetSolveHeadTurn( int solve );

	virtual void		ClearModelTargets( bool force = false );

private:
	class CFacePoserModel
	{
	public:
		CFacePoserModel( char const *modelfile, StudioModel *model );
		~CFacePoserModel();

		void LoadBitmaps();
		void FreeBitmaps();
		mxbitmapdata_t *GetBitmapForSequence( intp sequence );

		const char *GetBitmapChecksum( intp sequence );
		CRC32_t GetBitmapCRC( intp sequence );
		const char *GetBitmapFilename( intp sequence );
		void		RecreateAllAnimationBitmaps();
		void		RecreateAnimationBitmap( intp sequence, bool reconcile );


		void SetActorName( char const *actorname )
		{
			V_strcpy_safe( m_szActorName, actorname );
		}

		char const *GetActorName( void ) const
		{
			return m_szActorName;
		}

		StudioModel *GetModel( void ) const
		{
			return m_pModel;
		}

		char const *GetModelFileName( void ) const
		{
			return m_szModelFileName;
		}

		char const	*GetShortModelName( void ) const
		{
			return m_szShortName;
		}

		void SetVisibleIn3DView( bool visible )
		{
			m_bVisibileIn3DView = visible;
		}

		bool GetVisibleIn3DView( void ) const
		{
			return m_bVisibileIn3DView;
		}

		// For material system purposes
		void Release( void );
		void Restore( void );

		void Refresh( void )
		{
			// Forces a reload from disk
			Release();
			Restore();
		}

		void		CreateNewBitmap( char const *pchBitmapFilename, intp sequence, int nSnapShotSize, bool bZoomInOnFace, class CExpression *pExpression, mxbitmapdata_t *bitmap );

	private:

		void		LoadBitmapForSequence( mxbitmapdata_t *bitmap, intp sequence );

		void		ReconcileAnimationBitmaps();
		void		BuildValidChecksums( CUtlRBTree< CRC32_t > &tree );

		enum
		{
			MAX_ACTOR_NAME = 64,
			MAX_MODEL_FILE = 128,
			MAX_SHORT_NAME = 32,
		};

		char			m_szActorName[ MAX_ACTOR_NAME ];
		char			m_szModelFileName[ MAX_MODEL_FILE ];
		char			m_szShortName[ MAX_SHORT_NAME ];
		StudioModel		*m_pModel;
		bool			m_bVisibileIn3DView;

		struct AnimBitmap
		{
			AnimBitmap()
			{
				needsload = false;
				bitmap = 0;
			}
			bool			needsload;
			mxbitmapdata_t *bitmap;
		};

		CUtlVector< AnimBitmap * >	m_AnimationBitmaps;
		bool			m_bFirstBitmapLoad;
	};

	CFacePoserModel *GetEntry( intp index );

	CUtlVector< CFacePoserModel * > m_Models;

	int					m_nLastRenderFrame;
	intp				m_nForceModelIndex;
};

extern IFaceposerModels *models;

void EnableStickySnapshotMode( void );
void DisableStickySnapshotMode( void );

#endif // FACEPOSER_MODELS_H
