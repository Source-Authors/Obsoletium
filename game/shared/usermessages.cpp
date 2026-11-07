//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "usermessages.h"
#include "tier1/bitbuf.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void RegisterUserMessages( void );

//-----------------------------------------------------------------------------
// Purpose: Force registration on .dll load
// FIXME:  Should this be a client/server system?
//-----------------------------------------------------------------------------
CUserMessages::CUserMessages()
{
	// Game specific registration function;
	RegisterUserMessages();
}

CUserMessages::~CUserMessages()
{
	const int c{m_UserMessages.Count()};
	for ( int i = 0; i < c; ++i )
	{
		delete m_UserMessages[ i ];
	}
	m_UserMessages.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : int
//-----------------------------------------------------------------------------
int CUserMessages::LookupUserMessage( const char *name )
{
	const int idx{m_UserMessages.Find( name )};
	if ( idx == m_UserMessages.InvalidIndex() )
	{
		return -1;
	}

	return idx;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : int
//-----------------------------------------------------------------------------
int CUserMessages::GetUserMessageSize( int index )
{
	if ( index < 0 || index >= m_UserMessages.Count() )
	{
		Error( "%s: index %i out of range!!!\n", __FUNCTION__, index );
	}

	CUserMessage *e = m_UserMessages[ index ];
	return e->size;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : char const
//-----------------------------------------------------------------------------
const char *CUserMessages::GetUserMessageName( int index )
{
	if ( index < 0 || index >= m_UserMessages.Count() )
	{
		Error( "%s: index %i out of range!!!\n", __FUNCTION__, index );
	}

	return m_UserMessages.GetElementName( index );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CUserMessages::IsValidIndex( int index )
{
	return m_UserMessages.IsValidIndex( index );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			size - -1 for variable size
//-----------------------------------------------------------------------------
void CUserMessages::Register( const char *name, int size )
{
	Assert( name );
	const int idx{m_UserMessages.Find( name )};
	if ( idx != m_UserMessages.InvalidIndex() )
	{
		Error( "%s: message '%s' already registered.\n", __FUNCTION__, name );
	}

	auto *entry = new CUserMessage{size, name, CUtlVector<pfnUserMsgHook>{}};
	m_UserMessages.Insert( name, entry );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			hook - 
//-----------------------------------------------------------------------------
void CUserMessages::HookMessage( const char *name, pfnUserMsgHook hook )
{
#if defined( CLIENT_DLL )
	Assert( name );
	Assert( hook );

	const int idx{m_UserMessages.Find( name )};
	if ( idx == m_UserMessages.InvalidIndex() )
	{
		DevMsg( "%s: no such message %s.\n", __FUNCTION__, name );
		AssertMsg( false, "Message %s not found.", name );
		return;
	}

	const intp i = m_UserMessages[ idx ]->clienthooks.AddToTail();
	m_UserMessages[ idx ]->clienthooks[i] = hook;

#else
	Error( "%s: called from server code!!!\n", __FUNCTION__ );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszName - 
//			iSize - 
//			*pbuf - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CUserMessages::DispatchUserMessage( int msg_type, bf_read &msg_data )
{
#if defined( CLIENT_DLL )
	if ( msg_type < 0 || msg_type >= m_UserMessages.Count() )
	{
		DevWarning( "%s: bogus msg type %i (max == %d).\n", __FUNCTION__, msg_type, m_UserMessages.Count() );
		Assert( 0 );
		return false;
	}

	CUserMessage *entry = m_UserMessages[ msg_type ];

	if ( !entry )
	{
		DevWarning( "%s: missing client entry for msg type %i.\n", __FUNCTION__, msg_type );
		Assert( 0 );
		return false;
	}

	if ( entry->clienthooks.Count() == 0 )
	{
		DevWarning( "%s: missing client hook for %s.\n", __FUNCTION__, GetUserMessageName(msg_type) );
		Assert( 0 );
		return false;
	}

	for (pfnUserMsgHook hook : entry->clienthooks)
	{
		bf_read msg_copy = msg_data;

		(*hook)( msg_copy );
	}

	return true;
#else
	Error( "%s: called from server code!!!\n", __FUNCTION__ );
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			hook - 
//-----------------------------------------------------------------------------
// dimhotepus: Unhook message for cleanup.
void CUserMessages::UnhookMessage( const char *name, pfnUserMsgHook hook )
{
#if defined( CLIENT_DLL )
	Assert( name );
	Assert( hook );

	int idx = m_UserMessages.Find( name );
	if ( idx == m_UserMessages.InvalidIndex() )
	{
		DevMsg( "%s: no such message %s.\n", __FUNCTION__, name );
		AssertMsg( false, "Message %s not found.", name );
		return;
	}

	const bool ok = m_UserMessages[ idx ]->clienthooks.FindAndFastRemove( hook );
	if ( !ok )
	{
		DevWarning( "%s: message %s hook 0x%p not found.", __FUNCTION__, name, hook );
		AssertMsg( false, "Message %s hook 0x%p not found.", name, hook );
		return;
	}

#else
	Error( "%s: called from server code!!!\n", __FUNCTION__ );
#endif
}

// Singleton
static CUserMessages g_UserMessages;
// Expose to rest of .dll
CUserMessages *usermessages = &g_UserMessages;
