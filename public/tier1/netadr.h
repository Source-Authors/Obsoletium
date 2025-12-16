//========= Copyright Valve Corporation, All rights reserved. ============//
//
//

#ifndef SE_TIER1_NETADR_H_
#define SE_TIER1_NETADR_H_

#include "tier0/platform.h"
#include "tier0/vcrmode.h"

#undef SetPort

class bf_read;
class bf_write;

enum netadrtype_t
{ 
	NA_NULL = 0,
	NA_LOOPBACK,
	NA_BROADCAST,
	NA_IP,
};

using netadr_t = struct netadr_s
{
public:
	netadr_s() { SetIP( 0 ); SetPort( 0 ); SetType( NA_IP ); }
	netadr_s( uint unIP, uint16 usPort ) { SetIP( unIP ); SetPort( usPort ); SetType( NA_IP ); }
	[[deprecated("Use SetFromString and check return value.")]] netadr_s( const char *pch ) { (void)SetFromString( pch ); }
	void	Clear();	// invalids Address

	void	SetType( netadrtype_t type );
	void	SetPort( unsigned short port );
	[[nodiscard]] bool	SetFromSockadr(const struct sockaddr *s);
	void	SetIP(uint8 b1, uint8 b2, uint8 b3, uint8 b4);
	void	SetIP(uint unIP);									// Sets IP.  unIP is in host order (little-endian)
	void    SetIPAndPort( uint unIP, unsigned short usPort ) { SetIP( unIP ); SetPort( usPort ); }
	[[nodiscard]] bool	SetFromString(const char *pch, bool bUseDNS = false ); // if bUseDNS is true then do a DNS lookup if needed
	
	[[nodiscard]] bool	CompareAdr (const netadr_s &a, bool onlyBase = false) const;
	[[nodiscard]] bool	CompareClassBAdr (const netadr_s &a) const;
	[[nodiscard]] bool	CompareClassCAdr (const netadr_s &a) const;

	[[nodiscard]] netadrtype_t	GetType() const;
	[[nodiscard]] unsigned short	GetPort() const;

	// DON'T CALL THIS
	[[nodiscard]] const char*		ToString( bool onlyBase = false ) const; // returns xxx.xxx.xxx.xxx:ppppp

	void	ToString( OUT_Z_CAP(unBufferSize) char *pchBuffer, size_t unBufferSize, bool onlyBase = false ) const; // returns xxx.xxx.xxx.xxx:ppppp
	template< size_t maxLenInChars >
	void	ToString_safe( OUT_Z_ARRAY char (&pDest)[maxLenInChars], bool onlyBase = false ) const
	{
		ToString( &pDest[0], maxLenInChars, onlyBase );
	}

	void			ToSockadr(struct sockaddr *s) const;

	// Returns 0xAABBCCDD for AA.BB.CC.DD on all platforms, which is the same format used by SetIP().
	// (So why isn't it just named GetIP()?  Because previously there was a fucntion named GetIP(), and
	// it did NOT return back what you put into SetIP().  So we nuked that guy.)
	[[nodiscard]] unsigned int	GetIPHostByteOrder() const;

	// Returns a number that depends on the platform.  In most cases, this probably should not be used.
	[[nodiscard]] unsigned int	GetIPNetworkByteOrder() const;

	[[nodiscard]] bool	IsLocalhost() const; // true, if this is the localhost IP 
	[[nodiscard]] bool	IsLoopback() const;	// true if engine loopback buffers are used
	[[nodiscard]] bool	IsReservedAdr() const; // true, if this is a private LAN IP
	[[nodiscard]] bool	IsValid() const;	// ip & port != 0
	[[nodiscard]] bool	IsBaseAdrValid() const;	// ip != 0

	[[nodiscard]] bool SetFromSocket(socket_handle hSocket);

	[[nodiscard]] bool	Unserialize( bf_read &readBuf );
	[[nodiscard]] bool	Serialize( bf_write &writeBuf );

	[[nodiscard]] bool operator==(const netadr_s &netadr) const {return ( CompareAdr( netadr ) );}
	[[nodiscard]] bool operator!=(const netadr_s &netadr) const {return !( CompareAdr( netadr ) );}
	[[nodiscard]] bool operator<(const netadr_s &netadr) const;

public:	// members are public to avoid to much changes

	netadrtype_t	type;
	alignas(unsigned) unsigned char	ip[4];
	unsigned short	port;
};


/// Helper class to render a netadr_t.  Use this when formatting a net address
/// in a printf.  Don't use adr.ToString()!
class CUtlNetAdrRender
{
public:
	CUtlNetAdrRender( const netadr_t &obj, bool bBaseOnly = false )
	{
		obj.ToString_safe( m_rgchString, bBaseOnly );
	}

	CUtlNetAdrRender( uint32 unIP )
	{
		netadr_t addr( unIP, 0 );
		addr.ToString_safe( m_rgchString, true );
	}

	CUtlNetAdrRender( uint32 unIP, uint16 unPort )
	{
		netadr_t addr( unIP, unPort );
		addr.ToString_safe( m_rgchString, false );
	}

	CUtlNetAdrRender( const struct sockaddr &s )
	{
		m_rgchString[0] = '\0';

		netadr_t addr;
		if ( addr.SetFromSockadr( &s ) )
			addr.ToString_safe( m_rgchString, false );
	}

	[[nodiscard]] const char * String() const
	{ 
		return m_rgchString;
	}

private:

	char m_rgchString[32];
};

#endif  // !SE_TIER1_NETADR_H_
