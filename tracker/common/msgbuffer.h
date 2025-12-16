// Copyright Valve Corporation, All rights reserved.

#ifndef MSGBUFFER_H
#define MSGBUFFER_H

#include "tier1/netadr.h"

//-----------------------------------------------------------------------------
// Purpose: Generic byte level message buffer with read/write support
//-----------------------------------------------------------------------------
class CMsgBuffer  
{
public:
	enum 
	{
		NET_MAXMESSAGE = 8192
	};

	// Buffers must be named
	explicit CMsgBuffer( const char *buffername = "unnamed", void (*ef)( PRINTF_FORMAT_STRING const char *fmt, ... ) = 0 );
	virtual			~CMsgBuffer();

	// Reset the buffer for writing
	void			Clear();
	// Get current # of bytes 
	intp			GetCurSize();
	// Get max # of bytes
	intp			GetMaxSize();
	// Get pointer to raw data
	void			*GetData();
	// Set/unset the allow overflow flag
	void			SetOverflow( bool allowed );
	// Start reading from buffer
	void			BeginReading();
	// Get current read byte
	intp			GetReadCount();

	// Push read count ( to peek at data )
	void			Push();
	void			Pop();

	// Writing functions
	void			WriteByte(unsigned char c);
	void			WriteShort(short c);
	void			WriteLong(int32 c);
	void			WriteFloat(float f);
	void			WriteString(const char *s);
	void			WriteBuf(intp iSize, IN_Z_CAP(iSize) const void *buf);

	// Reading functions
	int				ReadByte();
	int				ReadShort();
	int				ReadLong();
	float			ReadFloat();
	char			*ReadString();
	intp			ReadBuf( intp iSize, OUT_CAP(iSize) void *pbuf );

	// setting and storing time received
	void			SetTime(float time);
	float			GetTime();

	// net address received from
	void			SetNetAddress(const netadr_t &adr);
	netadr_t		&GetNetAddress();

private:
	// Ensures sufficient space to append an object of length
	void			*GetSpace( intp length );
	// Copy buffer in at current writing point
	void			Write( IN_Z_CAP(length) const void *data, intp length );

private:
	// Name of buffer in case of debugging/errors
	const char		*m_pszBufferName;
	// Optional error callback
	void			( *m_pfnErrorFunc )( PRINTF_FORMAT_STRING const char *fmt, ... );

	// Current read pointer
	intp			m_nReadCount;
	// Push/pop read state
	intp			m_nPushedCount;
	bool			m_bPushed;
	// Did we hit the end of the read buffer?
	bool			m_bBadRead;
	// Max size of buffer
	intp			m_nMaxSize;
	// Current bytes written
	intp			m_nCurSize;
	// if false, call m_pfnErrorFunc
	bool			m_bAllowOverflow;
	// set to true when buffer hits end
	bool			m_bOverFlowed;
	// Internal storage
	unsigned char	m_rgData[ NET_MAXMESSAGE ];
	// time received
	float			m_fRecvTime;
	// address received from
	netadr_t		m_NetAddr;
};

#endif // !MSGBUFFER_H