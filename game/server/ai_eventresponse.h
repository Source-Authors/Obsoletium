//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: AI system that makes NPCs verbally respond to game events
//
//=============================================================================//

#ifndef	AI_EVENTRESPONSE_H
#define	AI_EVENTRESPONSE_H

#include "utldict.h"

constexpr inline float NPCEVENTRESPONSE_DISTANCE_SQR{768 * 768};	// Maximum distance for responding to NPCs
constexpr inline float NPCEVENTRESPONSE_REFIRE_TIME{15.0f};		// Time after giving a response before giving any more
constexpr inline float NPCEVENTRESPONSE_GIVEUP_TIME{4.0f};		// Time after a response trigger was fired before discarding it without responding

//-----------------------------------------------------------------------------
// Purpose: AI system that makes NPCs verbally respond to game events
//-----------------------------------------------------------------------------
class CNPCEventResponseSystem : public CAutoGameSystemPerFrame
{
public:
	CNPCEventResponseSystem( char const *name ) : CAutoGameSystemPerFrame( name )
	{
	}

	void	LevelInitPreEntity();
	void	FrameUpdatePreEntityThink();
	void	TriggerEvent( const char *pResponse, bool bForce, bool bCancelScript );

private:
	float	m_flNextEventPoll;

	struct storedevent_t
	{
		float	flEventTime;
		float	flNextResponseTime;
		bool	bForce;
		bool	bCancelScript;
		bool	bPreventExpiration;
	};

	typedef CUtlDict< storedevent_t, int > EventMap;
	EventMap m_ActiveEvents;
};

CNPCEventResponseSystem	*NPCEventResponse();

#endif // AI_EVENTRESPONSE_H