//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "choreoactor.h"
#include "choreochannel.h"
#include "choreoscene.h"
#include "tier0/dbg.h"
#include "tier1/utlbuffer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CChoreoActor::CChoreoActor()
{
	m_bMarkedForSave = false;
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CChoreoActor::CChoreoActor( const char *name )
{
	m_bMarkedForSave = false;
	Init();
	SetName( name );
}

CChoreoActor::CChoreoActor( const CChoreoActor &src )
{
	m_bActive = src.m_bActive;

	V_strcpy_safe( m_szName, src.m_szName );
	V_strcpy_safe( m_szFacePoserModelName, src.m_szFacePoserModelName );

	for ( auto *c : src.m_Channels )
	{
		auto *newChannel = new CChoreoChannel();
		newChannel->SetActor( this );
		*newChannel = *c;
		AddChannel( newChannel );
	}
}

//-----------------------------------------------------------------------------
// Purpose: // Assignment
// Input  : src - 
// Output : CChoreoActor&
//-----------------------------------------------------------------------------
CChoreoActor& CChoreoActor::operator=( const CChoreoActor& src )
{
	if ( &src == this ) return *this;

	m_bActive = src.m_bActive;

	V_strcpy_safe( m_szName, src.m_szName );
	V_strcpy_safe( m_szFacePoserModelName, src.m_szFacePoserModelName );

	for ( auto *c : src.m_Channels )
	{
		auto *newChannel = new CChoreoChannel();
		newChannel->SetActor( this );
		*newChannel = *c;
		AddChannel( newChannel );
	}

	return *this;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoActor::Init()
{
	m_szName[ 0 ] = '\0';
	m_szFacePoserModelName[ 0 ] = '\0';
	m_bActive = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void CChoreoActor::SetName( const char *name )
{
	Assert( strlen( name ) < MAX_ACTOR_NAME );
	Q_strncpy( m_szName, name, sizeof( m_szName ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CChoreoActor::GetName() const
{
	return m_szName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
intp CChoreoActor::GetNumChannels() const
{
	return m_Channels.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : channel - 
// Output : CChoreoChannel
//-----------------------------------------------------------------------------
CChoreoChannel *CChoreoActor::GetChannel( intp channel )
{
	if ( channel < 0 || channel >= m_Channels.Count() )
	{
		return NULL;
	}

	return m_Channels[ channel ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *channel - 
//-----------------------------------------------------------------------------
void CChoreoActor::AddChannel( CChoreoChannel *channel )
{
	m_Channels.AddToTail( channel );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *channel - 
//-----------------------------------------------------------------------------
void CChoreoActor::RemoveChannel( CChoreoChannel *channel )
{
	intp idx = FindChannelIndex( channel );
	if ( idx == -1 )
		return;

	m_Channels.Remove( idx );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoActor::RemoveAllChannels()
{
	m_Channels.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : c1 - 
//			c2 - 
//-----------------------------------------------------------------------------
void CChoreoActor::SwapChannels( int c1, int c2 )
{
	std::swap( m_Channels[ c1 ], m_Channels[ c2 ] );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *channel - 
// Output : int
//-----------------------------------------------------------------------------
ptrdiff_t CChoreoActor::FindChannelIndex( CChoreoChannel *channel )
{
	ptrdiff_t i{ 0 };
	for ( auto *ch : m_Channels )
	{
		if ( channel == ch )
		{
			return i;
		}
		++i;
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void CChoreoActor::SetFacePoserModelName( const char *name )
{
	Q_strncpy( m_szFacePoserModelName, name, sizeof( m_szFacePoserModelName ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *CChoreoActor::GetFacePoserModelName( void ) const
{
	return m_szFacePoserModelName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : active - 
//-----------------------------------------------------------------------------
void CChoreoActor::SetActive( bool active )
{
	m_bActive = active;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CChoreoActor::GetActive( void ) const
{
	return m_bActive;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoActor::MarkForSaveAll( bool mark )
{
	SetMarkedForSave( mark );

	for ( auto *channel : m_Channels )
	{
		channel->MarkForSaveAll( mark );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : CChoreoChannel
//-----------------------------------------------------------------------------
CChoreoChannel *CChoreoActor::FindChannel( const char *name )
{
	for ( auto *channel : m_Channels )
	{
		if ( !Q_stricmp( channel->GetName(), name ) )
			return channel;
	}

	return NULL;
}

void CChoreoActor::SaveToBuffer( CUtlBuffer& buf, CChoreoScene *pScene, IChoreoStringPool *pStringPool )
{
	buf.PutShort( pStringPool->FindOrAddString( GetName() ) );

	intp c = GetNumChannels();
	Assert( c <= 255 );
	buf.PutUnsignedChar( static_cast<unsigned char>(c) );

	for ( intp i = 0; i < c; i++ )
	{
		CChoreoChannel *channel = GetChannel( i );
		Assert( channel );
		channel->SaveToBuffer( buf, pScene, pStringPool );
	}

	/*
	if ( !Q_isempty( a->GetFacePoserModelName() ) )
	{
		FilePrintf( buf, level + 1, "faceposermodel \"%s\"\n", a->GetFacePoserModelName() );
	}
	*/
	buf.PutChar( GetActive() ? 1 : 0 );
}

bool CChoreoActor::RestoreFromBuffer( CUtlBuffer& buf, CChoreoScene *pScene, IChoreoStringPool *pStringPool )
{
	char sz[ 256 ];
	pStringPool->GetString( buf.GetShort(), sz, sizeof( sz ) );

	SetName( sz );

	intp c = buf.GetUnsignedChar();
	for ( intp i = 0; i < c; i++ )
	{
		CChoreoChannel *channel = pScene->AllocChannel();
		Assert( channel );
		if ( channel->RestoreFromBuffer( buf, pScene, this, pStringPool ) )
		{
			AddChannel( channel );
			channel->SetActor( this );
			continue;
		}

		return false;
	}

	SetActive( buf.GetChar() == 1 ? true : false );

	return true;
}

