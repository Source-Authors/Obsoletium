//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "c_slideshow_display.h"
#include "c_te_legacytempents.h"
#include "tempent.h"
#include "engine/IEngineSound.h"
#include "dlight.h"
#include "iefx.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "filesystem.h"
#include "KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define SLIDESHOW_LIST_BUFFER_MAX 8192u


enum SlideshowCycleTypes
{
	SLIDESHOW_CYCLE_RANDOM,
	SLIDESHOW_CYCLE_FORWARD,
	SLIDESHOW_CYCLE_BACKWARD,

	SLIDESHOW_CYCLE_TOTAL
};


CUtlVector< C_SlideshowDisplay* > g_SlideshowDisplays;


IMPLEMENT_CLIENTCLASS_DT(C_SlideshowDisplay, DT_SlideshowDisplay, CSlideshowDisplay)
	RecvPropBool( RECVINFO(m_bEnabled) ),
	RecvPropString( RECVINFO( m_szDisplayText ) ),
	RecvPropString( RECVINFO( m_szSlideshowDirectory ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_chCurrentSlideLists), RecvPropInt( RECVINFO(m_chCurrentSlideLists[0]) ) ),
	RecvPropFloat( RECVINFO(m_fMinSlideTime) ),
	RecvPropFloat( RECVINFO(m_fMaxSlideTime) ),
	RecvPropInt( RECVINFO(m_iCycleType) ),
	RecvPropBool( RECVINFO(m_bNoListRepeats) ),
END_RECV_TABLE()


C_SlideshowDisplay::C_SlideshowDisplay()
{
	g_SlideshowDisplays.AddToTail( this );
}

C_SlideshowDisplay::~C_SlideshowDisplay()
{
	g_SlideshowDisplays.FindAndRemove( this );
}

void C_SlideshowDisplay::Spawn( void )
{
	BaseClass::Spawn();

	m_NextSlideTime = 0;

	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

void C_SlideshowDisplay::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		BuildSlideShowImagesList();
	}
	else if ( updateType == DATA_UPDATE_DATATABLE_CHANGED )
	{
		m_iCurrentSlideList = 0;
		m_iCurrentSlide = 0;
	}
}

int C_SlideshowDisplay::GetMaterialIndex( intp iSlideIndex )
{
	if ( !m_SlideMaterialLists[ 0 ] )
		return 0;

	return m_SlideMaterialLists[ 0 ]->iSlideMaterials[ iSlideIndex ];
}

intp C_SlideshowDisplay::NumMaterials( void )
{
	if ( !m_SlideMaterialLists[ 0 ] )
		return 0;

	return m_SlideMaterialLists[ 0 ]->iSlideMaterials.Count();
}

void C_SlideshowDisplay::ClientThink( void )
{
	BaseClass::ClientThink();

	if ( !m_bEnabled )
		return;

	// Check if it's time for the next slide
	if ( m_NextSlideTime > gpGlobals->curtime )
		return;

	// Set the time to cycle to the next slide
	m_NextSlideTime = gpGlobals->curtime + RandomFloat( m_fMinSlideTime, m_fMaxSlideTime );

	// Get the amount of items to pick from
	int iNumCurrentSlideLists;
	for ( iNumCurrentSlideLists = 0; iNumCurrentSlideLists < 16; ++iNumCurrentSlideLists )
	{
		if ( m_chCurrentSlideLists[ iNumCurrentSlideLists ] == (unsigned char)-1 )
			break;
	}

	// Bail if no slide lists are selected
	if ( iNumCurrentSlideLists == 0 )
		return;

	// Cycle the list
	switch ( m_iCycleType )
	{
		case SLIDESHOW_CYCLE_RANDOM:
		{
			int iOldSlideList = m_iCurrentSlideList;
			m_iCurrentSlideList = RandomInt( 0, iNumCurrentSlideLists - 1 );

			// Prevent repeats if we don't want them
			if ( m_bNoListRepeats && iNumCurrentSlideLists > 1 && m_iCurrentSlideList == iOldSlideList )
			{
				++m_iCurrentSlideList;

				if ( m_iCurrentSlideList >= iNumCurrentSlideLists )
					m_iCurrentSlideList = 0;
			}

			break;
		}

		case SLIDESHOW_CYCLE_FORWARD:
			if ( m_iCurrentSlideList >= iNumCurrentSlideLists )
				m_iCurrentSlideList = 0;
			break;

		case SLIDESHOW_CYCLE_BACKWARD:
			if ( m_iCurrentSlideList < 0 )
				m_iCurrentSlideList = iNumCurrentSlideLists - 1;
			break;
	}

	SlideMaterialList_t *pSlideMaterialList = m_SlideMaterialLists[ m_chCurrentSlideLists[ m_iCurrentSlideList ] ];

	// Cycle in the list
	switch ( m_iCycleType )
	{
		case SLIDESHOW_CYCLE_RANDOM:
			m_iCurrentSlide = RandomInt( 0, pSlideMaterialList->iSlideMaterials.Count() - 1 );
			break;

		case SLIDESHOW_CYCLE_FORWARD:
			++m_iCurrentSlide;
			if ( m_iCurrentSlide >= pSlideMaterialList->iSlideMaterials.Count() )
			{
				++m_iCurrentSlideList;
				if ( m_iCurrentSlideList >= iNumCurrentSlideLists )
					m_iCurrentSlideList = 0;
				pSlideMaterialList = m_SlideMaterialLists[ m_chCurrentSlideLists[ m_iCurrentSlideList ] ];
				m_iCurrentSlide = 0;
			}
			break;

		case SLIDESHOW_CYCLE_BACKWARD:
			--m_iCurrentSlide;
			if ( m_iCurrentSlide < 0 )
			{
				--m_iCurrentSlideList;
				if ( m_iCurrentSlideList < 0 )
					m_iCurrentSlideList = iNumCurrentSlideLists - 1;
				pSlideMaterialList = m_SlideMaterialLists[ m_chCurrentSlideLists[ m_iCurrentSlideList ] ];
				m_iCurrentSlide = pSlideMaterialList->iSlideMaterials.Count() - 1;
			}
			break;
	}

	// Set the current material to what we've cycled to
	m_iCurrentMaterialIndex = pSlideMaterialList->iSlideMaterials[ m_iCurrentSlide ];
	m_iCurrentSlideIndex = pSlideMaterialList->iSlideIndex[ m_iCurrentSlide ];
}

void C_SlideshowDisplay::BuildSlideShowImagesList( void )
{
	FileFindHandle_t matHandle;
	char szDirectory[MAX_PATH];
	char szMatFileName[MAX_PATH] = {'\0'};

	V_sprintf_safe( szDirectory, "materials/vgui/%s/*.vmt", m_szSlideshowDirectory );
	{
		const char *pMatFileName = g_pFullFileSystem->FindFirst( szDirectory, &matHandle );

		if ( pMatFileName )
			V_strcpy_safe( szMatFileName, pMatFileName );
	}

	int iSlideIndex = 0;

	while ( szMatFileName[ 0 ] )
	{
		char szFileName[MAX_PATH];
		V_sprintf_safe( szFileName, "vgui/%s/%s", m_szSlideshowDirectory, szMatFileName );
		szFileName[ V_strlen( szFileName ) - 4 ] = '\0';

		int iMatIndex = ::GetMaterialIndex( szFileName );

		// Get material keywords
		char szFullFileName[MAX_PATH];
		V_sprintf_safe( szFullFileName, "materials/vgui/%s/%s", m_szSlideshowDirectory, szMatFileName );

		KeyValuesAD pMaterialKeys( "material" );
		bool bLoaded = pMaterialKeys->LoadFromFile( g_pFullFileSystem, szFullFileName, NULL );

		if ( bLoaded )
		{
			char szKeywords[ 256 ] = {0};
			V_strcpy_safe( szKeywords, pMaterialKeys->GetString( "%keywords", "" ) );

			char *pchKeyword = szKeywords;

			while ( pchKeyword[ 0 ] != '\0' )
			{
				char *pNextKeyword = pchKeyword;

				// Skip commas and spaces
				while ( pNextKeyword[ 0 ] != '\0' && pNextKeyword[ 0 ] != ',' )
					++pNextKeyword;

				if ( pNextKeyword[ 0 ] != '\0' )
				{
					pNextKeyword[ 0 ] = '\0';
					++pNextKeyword;

					while ( pNextKeyword[ 0 ] != '\0' && ( pNextKeyword[ 0 ] == ',' || pNextKeyword[ 0 ] == ' ' ) )
						++pNextKeyword;
				}

				// Find the list with the current keyword
				intp iList;
				for ( iList = 0; iList < m_SlideMaterialLists.Count(); ++iList )
				{
					if ( Q_strcmp( m_SlideMaterialLists[ iList ]->szSlideKeyword, pchKeyword ) == 0 )
						break;
				}

				if ( iList >= m_SlideMaterialLists.Count() )
				{
					// Couldn't find the list, so create it
					iList = m_SlideMaterialLists.AddToTail( new SlideMaterialList_t );
					V_strcpy_safe( m_SlideMaterialLists[iList]->szSlideKeyword, pchKeyword );
				}

				// Add material index to this list
				m_SlideMaterialLists[ iList ]->iSlideMaterials.AddToTail( iMatIndex );
				m_SlideMaterialLists[ iList ]->iSlideIndex.AddToTail( iSlideIndex );

				pchKeyword = pNextKeyword;
			}
		}

		// Find the generic list
		intp iList;
		for ( iList = 0; iList < m_SlideMaterialLists.Count(); ++iList )
		{
			if ( Q_strcmp( m_SlideMaterialLists[ iList ]->szSlideKeyword, "" ) == 0 )
				break;
		}

		if ( iList >= m_SlideMaterialLists.Count() )
		{
			// Couldn't find the generic list, so create it
			iList = m_SlideMaterialLists.AddToHead( new SlideMaterialList_t );
			V_strcpy_safe( m_SlideMaterialLists[iList]->szSlideKeyword, "" );
		}

		// Add material index to this list
		m_SlideMaterialLists[ iList ]->iSlideMaterials.AddToTail( iMatIndex );
		m_SlideMaterialLists[ iList ]->iSlideIndex.AddToTail( iSlideIndex );
		
		const char *pMatFileName = g_pFullFileSystem->FindNext( matHandle );

		if ( pMatFileName )
			V_strcpy_safe( szMatFileName, pMatFileName );
		else
			szMatFileName[ 0 ] = '\0';

		++iSlideIndex;
	}

	g_pFullFileSystem->FindClose( matHandle );
}