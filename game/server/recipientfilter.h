//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef RECIPIENTFILTER_H
#define RECIPIENTFILTER_H
#ifdef _WIN32
#pragma once
#endif

#include "irecipientfilter.h"
#include "const.h"
#include "player.h"
#include "bitvec.h"

//-----------------------------------------------------------------------------
// Purpose: A generic filter for determining whom to send message/sounds etc. to and
//  providing a bit of additional state information
//-----------------------------------------------------------------------------
class CRecipientFilter : public IRecipientFilter
{
public:
					CRecipientFilter();
	virtual 		~CRecipientFilter();

	bool	IsReliable( void ) const override;
	bool	IsInitMessage( void ) const override;

	intp	GetRecipientCount( void ) const override;
	intp	GetRecipientIndex( intp slot ) const override;

public:

	void			CopyFrom( const CRecipientFilter& src );

	void			Reset( void );

	void			MakeInitMessage( void );

	void			MakeReliable( void );
	
	void			AddAllPlayers( void );
	void			AddRecipientsByPVS( const Vector& origin );
	void			RemoveRecipientsByPVS( const Vector& origin );
	void			AddRecipientsByPAS( const Vector& origin );
	void			AddRecipient( const CBasePlayer *player );
	void			RemoveAllRecipients( void );
	void			RemoveRecipient( CBasePlayer *player );
	void			RemoveRecipientByPlayerIndex( intp playerindex );
	void			AddRecipientsByTeam( CTeam *team );
	void			RemoveRecipientsByTeam( CTeam *team );
	void			RemoveRecipientsNotOnTeam( CTeam *team );

	void			UsePredictionRules( void );
	bool			IsUsingPredictionRules( void ) const;

	bool			IgnorePredictionCull( void ) const;
	void			SetIgnorePredictionCull( bool ignore );

	void			AddPlayersFromBitMask( CBitVec< ABSOLUTE_PLAYER_LIMIT >& playerbits );
	void			RemovePlayersFromBitMask( CBitVec< ABSOLUTE_PLAYER_LIMIT >& playerbits );

private:

	bool				m_bReliable;
	bool				m_bInitMessage;
	CUtlVector< int >	m_Recipients;
	
	// If using prediction rules, the filter itself suppresses local player
	bool				m_bUsingPredictionRules;
	// If ignoring prediction cull, then external systems can determine
	//  whether this is a special case where culling should not occur
	bool				m_bIgnorePredictionCull;
};

//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for a single player ( unreliable )
//-----------------------------------------------------------------------------
class CSingleUserRecipientFilter : public CRecipientFilter
{
public:
	CSingleUserRecipientFilter( const CBasePlayer *player )
	{
		AddRecipient( player );
	}
};

//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for all players on a given team 
//-----------------------------------------------------------------------------
class CTeamRecipientFilter : public CRecipientFilter
{
public:
	CTeamRecipientFilter( int team, bool isReliable = false );
};

//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for all players ( unreliable )
//-----------------------------------------------------------------------------
class CBroadcastRecipientFilter : public CRecipientFilter
{
public:
	CBroadcastRecipientFilter( void )
	{
		AddAllPlayers();
	}
};

//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for all players ( reliable )
//-----------------------------------------------------------------------------
class CReliableBroadcastRecipientFilter : public CBroadcastRecipientFilter
{
public:
	CReliableBroadcastRecipientFilter( void )
	{
		MakeReliable();
	}
};

//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for all players except for one ( unreliable )
//-----------------------------------------------------------------------------
class CBroadcastNonOwnerRecipientFilter : public CRecipientFilter
{
public:
	CBroadcastNonOwnerRecipientFilter( CBasePlayer *player )
	{
		AddAllPlayers();
		RemoveRecipient( player );
	}
};

//-----------------------------------------------------------------------------
// Purpose: Add players in PAS to recipient list (unreliable)
//-----------------------------------------------------------------------------
class CPASFilter : public CRecipientFilter
{
public:
	CPASFilter( void )
	{
	}

	CPASFilter( const Vector& origin )
	{
		AddRecipientsByPAS( origin );
	}
};

//-----------------------------------------------------------------------------
// Purpose: Add players in PAS to list and if not in single player, use attenuation
//  to remove those that are too far away from source origin
// Source origin can be stated as an entity or just a passed in origin
// (unreliable)
//-----------------------------------------------------------------------------
class CPASAttenuationFilter : public CPASFilter
{
public:
	CPASAttenuationFilter( void )
	{
	}

	CPASAttenuationFilter( CBaseEntity *entity, soundlevel_t soundlevel ) :
		CPASAttenuationFilter( entity, SNDLVL_TO_ATTN( soundlevel ) )
	{
	}

	CPASAttenuationFilter( CBaseEntity *entity, float attenuation = ATTN_NORM ) :
		CPASAttenuationFilter( static_cast<const Vector&>(entity->GetSoundEmissionOrigin()), attenuation )
	{
	}

	CPASAttenuationFilter( const Vector& origin, soundlevel_t soundlevel ) :
		CPASAttenuationFilter( origin, SNDLVL_TO_ATTN( soundlevel ) )
	{
	}

	CPASAttenuationFilter( const Vector& origin, float attenuation = ATTN_NORM ) :
		CPASFilter( origin )
	{
		Filter( origin, attenuation );
	}

	CPASAttenuationFilter( CBaseEntity *entity, const char *lookupSound ) :
		CPASAttenuationFilter( entity, SNDLVL_TO_ATTN( CBaseEntity::LookupSoundLevel( lookupSound ) ) )
	{
	}

	CPASAttenuationFilter( const Vector& origin, const char *lookupSound ) :
		CPASAttenuationFilter( origin, SNDLVL_TO_ATTN( CBaseEntity::LookupSoundLevel( lookupSound ) ) )
	{
	}

	CPASAttenuationFilter( CBaseEntity *entity, const char *lookupSound, HSOUNDSCRIPTHANDLE& handle ) :
		CPASAttenuationFilter( static_cast<const Vector&>(entity->GetSoundEmissionOrigin()), lookupSound, handle )
	{
	}

	CPASAttenuationFilter( const Vector& origin, const char *lookupSound, HSOUNDSCRIPTHANDLE& handle ) :
		CPASAttenuationFilter( origin, SNDLVL_TO_ATTN( CBaseEntity::LookupSoundLevel( lookupSound, handle ) ) )
	{
	}

public:
	void Filter( const Vector& origin, float attenuation = ATTN_NORM );
};

//-----------------------------------------------------------------------------
// Purpose: Simple PVS based filter ( unreliable )
//-----------------------------------------------------------------------------
class CPVSFilter : public CRecipientFilter
{
public:
	CPVSFilter( const Vector& origin )
	{
		AddRecipientsByPVS( origin );
	}
};

#endif // RECIPIENTFILTER_H
