//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ENGINESINGLEUSERFILTER_H
#define ENGINESINGLEUSERFILTER_H
#ifdef _WIN32
#pragma once
#endif

#include "irecipientfilter.h"
#include "bitvec.h"
#include "utlvector.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEngineRecipientFilter : public IRecipientFilter
{
public:	// IRecipientFilter interface:
	
					CEngineRecipientFilter();
	virtual intp	GetRecipientCount( void ) const;
	virtual intp	GetRecipientIndex( intp slot ) const;
	virtual bool	IsReliable( void ) const { return m_bReliable; };
	virtual bool	IsInitMessage( void )  const { return m_bInit; };

public:

	void			Reset( void );

	void			MakeInitMessage( void );
	void			MakeReliable( void );

	void			AddAllPlayers( void );
	void			AddRecipientsByPVS( const Vector& origin );
	void			AddRecipientsByPAS( const Vector& origin );
	void			AddPlayersFromBitMask( CBitVec< ABSOLUTE_PLAYER_LIMIT >& playerbits );
	void			AddPlayersFromFilter( const IRecipientFilter *filter );
	void			AddRecipient( intp playerindex );
	void			RemoveRecipient( intp playerindex );
	bool			IncludesPlayer(intp playerindex);
	
private:

	bool				m_bInit;
	bool				m_bReliable;
	CUtlVector< intp >	m_Recipients;
};

//-----------------------------------------------------------------------------
// Purpose: Simple filter for doing MSG_ONE type stuff directly in engine
//-----------------------------------------------------------------------------
class CEngineSingleUserFilter : public IRecipientFilter
{
public:
	CEngineSingleUserFilter( intp clientindex, bool bReliable = false )
	{
		m_nClientIndex = clientindex;
		m_bReliable = bReliable;
	}

	virtual bool	IsReliable( void ) const
	{
		return m_bReliable;
	}

	virtual intp	GetRecipientCount( void ) const
	{
		return 1;
	}

	virtual intp	GetRecipientIndex( intp ) const
	{
		return m_nClientIndex;
	}

	virtual bool	IsBroadcastMessage( void ) const
	{
		return false;
	}

	virtual bool	IsInitMessage( void ) const
	{
		return false;
	}

private:
	intp			m_nClientIndex;
	bool			m_bReliable;
};

#endif // ENGINESINGLEUSERFILTER_H
