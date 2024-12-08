//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "sv_uploaddata.h"

#include "tier1/KeyValues.h"
#include "tier1/bitbuf.h"
#include "mathlib/IceKey.H"

#include "host.h"
#include "blockingudpsocket.h"
#include "cserserverprotocol_engine.h"
#include "net.h"

#if defined(_WIN32)
#include <winsock.h>
#elif POSIX
#include <sys/socket.h>
#include <netinet/in.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace {

constexpr inline unsigned char ucEncryptionKey[8] = { 54, 175, 165, 5, 76, 251, 29, 113 };

unsigned CountFields( KeyValues *fields )
{
	unsigned c = 0;
	KeyValues *kv = fields->GetFirstSubKey();
	while ( kv )
	{
		c++;
		kv = kv->GetNextKey();
	}
	return c;
}

// Purpose: encrypts an 8-byte sequence
inline void Encrypt8ByteSequence( IceKey& cipher, const unsigned char *plainText, unsigned char *cipherText)
{
	cipher.encrypt(plainText, cipherText);
}

void EncryptBuffer( IceKey& cipher, unsigned char *bufData, uint bufferSize)
{
	unsigned char *cipherText = bufData;
	unsigned char *plainText = bufData;
	uint bytesEncrypted = 0;

	while (bytesEncrypted < bufferSize)
	{
		// encrypt 8 byte section
		Encrypt8ByteSequence( cipher, plainText, cipherText);

		bytesEncrypted += 8;
		cipherText += 8;
		plainText += 8;
	}
}

void BuildUploadDataMessage( bf_write& buf, char const *tablename, KeyValues *fields )
{
	bf_write	encrypted;
	alignas(4) byte encrypted_data[ 2048 ];

	buf.WriteByte( C2M_UPLOADDATA );
	buf.WriteByte( '\n' );
	buf.WriteByte( C2M_UPLOADDATA_PROTOCOL_VERSION );

	// encryption object
	IceKey cipher(1); /* medium encryption level */
	cipher.set( ucEncryptionKey );

	encrypted.StartWriting( encrypted_data, sizeof( encrypted_data ) );

	const byte corruption_identifier = 0x01;
	encrypted.WriteByte( corruption_identifier );

	// Data version protocol
	encrypted.WriteByte( C2M_UPLOADDATA_DATA_VERSION );
	encrypted.WriteString( tablename ); 

	const unsigned fieldCount = CountFields( fields );
	if ( fieldCount > 255 )
	{
		Host_Error( "Too many fields in uploaddata (%u max = 255)\n", fieldCount );
	}

	encrypted.WriteByte( (byte)fieldCount );

	KeyValues *kv = fields->GetFirstSubKey();
	while ( kv )
	{
		encrypted.WriteString( kv->GetName() );
		encrypted.WriteString( kv->GetString() );

		kv = kv->GetNextKey();
	}

	// Round to multiple of 8 for encrypted
	while ( encrypted.GetNumBytesWritten() % 8 )
	{
		encrypted.WriteByte( 0 );
	}

	EncryptBuffer( cipher, encrypted.GetData(), encrypted.GetNumBytesWritten() );

	buf.WriteShort( (int)encrypted.GetNumBytesWritten() );
	buf.WriteBytes( encrypted.GetData(), encrypted.GetNumBytesWritten() );
}

}  // namespace

bool UploadData( char const *cserIP, char const *tablename, KeyValues *fields )
{
	alignas(4) byte data[2048];

	bf_write buf;
	buf.StartWriting( data, sizeof( data ) );

	BuildUploadDataMessage( buf, tablename, fields );

	netadr_t cseradr;
	if ( NET_StringToAdr( cserIP, &cseradr ) )
	{
		CBlockingUDPSocket socket;

		sockaddr_in sa;
		cseradr.ToSockadr( (sockaddr *)&sa );

		// Don't bother waiting for response here
		return socket.SendSocketMessage( sa, buf.GetData(), buf.GetNumBytesWritten() );
	}

	return false;
}
