//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef CHOREOACTOR_H
#define CHOREOACTOR_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"

class CChoreoChannel;
class CChoreoScene;
class CUtlBuffer;
class IChoreoStringPool;

//-----------------------------------------------------------------------------
// Purpose: The actor is the atomic element of a scene
//  A scene can have one or more actors, who have multiple events on one or 
//  more channels
//-----------------------------------------------------------------------------
class CChoreoActor
{
public:

	// Construction
					CChoreoActor();
					CChoreoActor( const char *name );
					CChoreoActor( const CChoreoActor& src );
	// Assignment
	CChoreoActor&	operator = ( const CChoreoActor& src );

	// Serialization
	void			SaveToBuffer( CUtlBuffer& buf, CChoreoScene *pScene, IChoreoStringPool *pStringPool );
	bool			RestoreFromBuffer( CUtlBuffer& buf, CChoreoScene *pScene, IChoreoStringPool *pStringPool );

	// Accessors
	void			SetName( const char *name );
	const char		*GetName() const;

	// Iteration
	intp			GetNumChannels() const;
	CChoreoChannel	*GetChannel( intp channel );

	CChoreoChannel	*FindChannel( const char *name );

	// Manipulate children
	void			AddChannel( CChoreoChannel *channel );
	void			RemoveChannel( CChoreoChannel *channel );
	intp			FindChannelIndex( CChoreoChannel *channel );
	void			SwapChannels( intp c1, intp c2 );
	void			RemoveAllChannels();

	void			SetFacePoserModelName( const char *name );
	char const		*GetFacePoserModelName() const;

	void						SetActive( bool active );
	bool						GetActive() const;

	bool			IsMarkedForSave() const { return m_bMarkedForSave; }
	void			SetMarkedForSave( bool mark ) { m_bMarkedForSave = mark; }

	void			MarkForSaveAll( bool mark );

private:
	// Clear structure out
	void			Init();

	enum
	{
		MAX_ACTOR_NAME = 128,
		MAX_FACEPOSER_MODEL_NAME = 128
	};

	char			m_szName[ MAX_ACTOR_NAME ];
	char			m_szFacePoserModelName[ MAX_FACEPOSER_MODEL_NAME ];

	// Children
	CUtlVector < CChoreoChannel * >	m_Channels;

	bool			m_bActive;

	// Purely for save/load
	bool			m_bMarkedForSave;
};

#endif // CHOREOACTOR_H
