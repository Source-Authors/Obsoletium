//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "msgbuffer.h"

#include "tier0/dbg.h"
#include "tier1/strtools.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//-----------------------------------------------------------------------------
// Purpose: Allocate message buffer
// Input  : *buffername - 
//			*ef - 
//-----------------------------------------------------------------------------
CMsgBuffer::CMsgBuffer( const char *buffername, void (*ef)( PRINTF_FORMAT_STRING const char *fmt, ... ) /*= NULL*/ )
{
	m_pszBufferName		= buffername;
	m_pfnErrorFunc		= ef;
	m_bAllowOverflow	= false;	// if false, Error
	m_fRecvTime			= -1.f;
	m_bOverFlowed		= false;	// set to true if the buffer size failed
	m_nMaxSize			= NET_MAXMESSAGE;
	m_nPushedCount		= 0;
	m_bPushed			= false;
	m_nReadCount		= 0;
	m_bBadRead			= false;

	Clear();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMsgBuffer::~CMsgBuffer( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Temporarily remember the read position so we can reset it
//-----------------------------------------------------------------------------
void CMsgBuffer::Push( void )
{
	// ??? Allow multiple pushes without matching pops ???
	Assert( !m_bPushed );

	m_nPushedCount	= m_nReadCount;
	m_bPushed		= true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMsgBuffer::Pop( void )
{
	Assert( m_bPushed );

	m_nReadCount	= m_nPushedCount;
	m_bPushed		= false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : allowed - 
//-----------------------------------------------------------------------------
void CMsgBuffer::SetOverflow( bool allowed )
{
	m_bAllowOverflow = allowed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : intp
//-----------------------------------------------------------------------------
intp CMsgBuffer::GetMaxSize( void )
{
	return m_nMaxSize;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void *
//-----------------------------------------------------------------------------
void * CMsgBuffer::GetData( void )
{
	return m_rgData;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : intp
//-----------------------------------------------------------------------------
intp CMsgBuffer::GetCurSize( void )
{
	return m_nCurSize;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : intp
//-----------------------------------------------------------------------------
intp CMsgBuffer::GetReadCount( void )
{
	return m_nReadCount;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CMsgBuffer::SetTime(float time)
{
	m_fRecvTime = time;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
float CMsgBuffer::GetTime()
{
	return m_fRecvTime;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CMsgBuffer::SetNetAddress(const netadr_t &adr)
{
	m_NetAddr = adr;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
netadr_t &CMsgBuffer::GetNetAddress()
{
	return m_NetAddr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMsgBuffer::WriteByte( unsigned char c )
{
	unsigned char    *buf = (unsigned char *)GetSpace( 1 );
	buf[0] = c;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : c - 
//-----------------------------------------------------------------------------
void CMsgBuffer::WriteShort ( short c )
{
	unsigned char    *buf;
	buf = (unsigned char *)GetSpace( 2 );
	buf[0] = c&0xff;
	buf[1] = c>>8;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : c - 
//-----------------------------------------------------------------------------
void CMsgBuffer::WriteLong (int32 c)
{
	unsigned char    *buf;
	
	buf = (unsigned char *)GetSpace( sizeof(c) );
	buf[0] = c&0xff;
	buf[1] = (c>>8)&0xff;
	buf[2] = (c>>16)&0xff;
	buf[3] = c>>24;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : f - 
//-----------------------------------------------------------------------------
void CMsgBuffer::WriteFloat (float f)
{
	union
	{
		float   f;
		int     l;
	} dat;
	
	dat.f = f;
	Write( &dat.l, sizeof(dat.l) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *s - 
//-----------------------------------------------------------------------------
void CMsgBuffer::WriteString (const char *s)
{
	if ( !s )
	{
		Write ("", 1);
	}
	else
	{
		Write ( s, V_strlen( s ) + 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iSize - 
//			*buf - 
//-----------------------------------------------------------------------------
void CMsgBuffer::WriteBuf( intp iSize, IN_Z_CAP(iSize) const void *buf )
{
	if ( !buf )
	{
		return;
	}

	Write( buf, iSize );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMsgBuffer::BeginReading (void)
{
	m_nReadCount		= 0;
	m_bBadRead		= false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int CMsgBuffer::ReadByte
//-----------------------------------------------------------------------------
int CMsgBuffer::ReadByte (void)
{
	int     c;
	
	if ( m_nReadCount + 1 > m_nCurSize )
	{
		m_bBadRead = true;
		return -1;
	}
		
	c = ( unsigned char )m_rgData[ m_nReadCount ];
	m_nReadCount++;
	return c;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int CMsgBuffer::ReadShort
//-----------------------------------------------------------------------------
int CMsgBuffer::ReadShort (void)
{
	int     c;
	
	if ( m_nReadCount + 2 > m_nCurSize )
	{
		m_bBadRead = true;
		return -1;
	}
		
	c = (short)(m_rgData[m_nReadCount] + (m_rgData[m_nReadCount+1]<<8));
	m_nReadCount += 2;
	
	return c;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int CMsgBuffer::ReadLong
//-----------------------------------------------------------------------------
int CMsgBuffer::ReadLong (void)
{
	int32     c;
	
	if (m_nReadCount+static_cast<intp>(sizeof(c)) > m_nCurSize)
	{
		m_bBadRead = true;
		return -1;
	}
		
	c = m_rgData[m_nReadCount]
	+ (m_rgData[m_nReadCount+1]<<8)
	+ (m_rgData[m_nReadCount+2]<<16)
	+ (m_rgData[m_nReadCount+3]<<24);
	
	m_nReadCount += sizeof(c);
	
	return c;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float CMsgBuffer::ReadFloat
//-----------------------------------------------------------------------------
float CMsgBuffer::ReadFloat (void)
{
	union
	{
		unsigned char    b[4]; //-V112
		float   f;
	} dat;
	
	dat.b[0] =      m_rgData[m_nReadCount];
	dat.b[1] =      m_rgData[m_nReadCount+1];
	dat.b[2] =      m_rgData[m_nReadCount+2];
	dat.b[3] =      m_rgData[m_nReadCount+3];
	m_nReadCount += sizeof(dat);
	return dat.f;   
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iSize - 
//			*pbuf - 
// Output : int
//-----------------------------------------------------------------------------
intp CMsgBuffer::ReadBuf( intp iSize, OUT_CAP(iSize) void *pbuf )
{
	if (m_nReadCount + iSize > m_nCurSize)
	{
		m_bBadRead = true;
		return -1;
	}

	memcpy( pbuf, &m_rgData[m_nReadCount], iSize );
	m_nReadCount += iSize;
	
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char *
//-----------------------------------------------------------------------------
char *CMsgBuffer::ReadString (void)
{
	static char     string[ NET_MAXMESSAGE ];
	
	intp l = 0;
	do
	{
		char c = (char)ReadByte();
		if ( c == -1 || c == 0 )
			break;
		string[l] = c;
		l++;
	} while ( l < ssize(string)-1 );
	
	string[ l ] = '\0';
	
	return string;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMsgBuffer::Clear( void )
{
	m_nCurSize		= 0;
	m_bOverFlowed	= false;
	m_nReadCount	= 0;
	m_bBadRead		= false;
	BitwiseClear( m_rgData );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : length - 
//-----------------------------------------------------------------------------
void *CMsgBuffer::GetSpace( intp length )
{	
	if (m_nCurSize + length > m_nMaxSize)
	{
		if ( !m_bAllowOverflow )
		{
			if ( m_pfnErrorFunc )
			{
				( *m_pfnErrorFunc )( "CMsgBuffer(%s), no room for %i bytes, %i / %i already in use\n",
					m_pszBufferName, length, m_nCurSize, m_nMaxSize );
			}
			return NULL;
		}
		
		if (length > m_nMaxSize)
		{
			if ( m_pfnErrorFunc )
			{
				( *m_pfnErrorFunc )( "CMsgBuffer(%s), no room for %i bytes, %i is max\n",
					m_pszBufferName, length, m_nMaxSize );
			}
			return NULL;
		}
			
		m_bOverFlowed = true;
		Clear(); 
	}

	void *d = m_rgData + m_nCurSize;
	m_nCurSize += length;
	return d;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *m_rgData - 
//			length - 
//-----------------------------------------------------------------------------
void CMsgBuffer::Write(IN_Z_CAP(length) const void *data, intp length)
{
	memcpy( GetSpace(length), data, length );
}