//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: This module is for wrapping Crypto++ functions, including crypto++
//          directly has nasty consequences polluting the global namespace, and
//          conflicting with xdebug and locale stuff, so we only include it here
//          and use this wrapper in the rest of our code.
//
// $NoKeywords: $
//=============================================================================

#ifndef CRYPTO_H
#define CRYPTO_H

#include "tier0/dbg.h"		// for Assert & AssertMsg
#include "tier1/passwordhash.h"
#include "tier1/utlmemory.h"

#include <steam/steamtypes.h> // for Salt_t

extern void FreeListRNG();

constexpr inline unsigned int k_cubSHA256Hash = 32;
using SHA256Digest_t = unsigned char [ k_cubSHA256Hash ];

constexpr inline int k_nSymmetricBlockSize = 16;					// AES block size (128 bits)
constexpr inline int k_nSymmetricKeyLen = 32;						// length in bytes of keys used for symmetric encryption

constexpr inline int k_nRSAKeyLenMax = 1024;						// max length in bytes of keys used for RSA encryption (includes DER encoding)
constexpr inline int k_nRSAKeyLenMaxEncoded = k_nRSAKeyLenMax*2;	// max length in bytes of hex-encoded key (hex encoding exactly doubles size)
constexpr inline int k_nRSAKeyBits = 1024;							// length in bits of keys used for RSA encryption
constexpr inline int k_cubRSAEncryptedBlockSize	= 128;
constexpr inline int k_cubRSAPlaintextBlockSize	= 86 + 1;		// assume plaintext is text, so add a byte for the trailing \0
constexpr inline uint32 k_cubRSASignature = k_cubRSAEncryptedBlockSize;

// Simple buffer class to encapsulate output from crypto functions with unknown output size
class CCryptoOutBuffer
{
public:

	CCryptoOutBuffer() : m_pubData{nullptr}, m_cubData{0}
	{
	}

	~CCryptoOutBuffer()
	{
		delete[] m_pubData;
		m_pubData = nullptr;
		m_cubData = 0;
	}

	void Set( const uint8 *pubData, size_t cubData )
	{
		if ( !pubData || !cubData )
			return;

		delete[] m_pubData;

		m_pubData = new uint8[ cubData ];
		memcpy( m_pubData, pubData, cubData );
		m_cubData = cubData;
	}

	void Allocate( size_t cubData )
	{
		delete[] m_pubData;

		m_pubData = new uint8[ cubData ];
		m_cubData = cubData;
	}

	void Trim( size_t cubTrim )
	{
		Assert( cubTrim <= m_cubData );
		m_cubData = cubTrim;
	}

	[[nodiscard]] uint8 *PubData() { return m_pubData; }
	[[nodiscard]] size_t CubData() const { return m_cubData; }

private:
	uint8 *m_pubData;
	size_t m_cubData;
};

class CCrypto
{
public:
	[[nodiscard]] static size_t GetSymmetricEncryptedSize( size_t cubPlaintextData );

	// this method writes the encrypted IV, then the ciphertext
	[[nodiscard]] static bool SymmetricEncryptWithIV( const uint8 * pubPlaintextData, size_t cubPlaintextData, 
										const uint8 *pIV, size_t cubIV,
										uint8 * pubEncryptedData, size_t * pcubEncryptedData,
										const uint8 * pubKey, size_t cubKey );
	[[nodiscard]] static bool SymmetricEncrypt( const uint8 * pubPlaintextData, size_t cubPlaintextData, 
								  uint8 * pubEncryptedData, size_t * pcubEncryptedData,
								  const uint8 * pubKey, size_t cubKey );
	// this method assumes there is no IV before the payload - dissimilar to SymmetricEncryptWithIV
	[[nodiscard]] static bool SymmetricDecryptWithIV( const uint8 * pubEncryptedData, size_t cubEncryptedData, 
										const uint8 * pIV, size_t cubIV,
										uint8 * pubPlaintextData, size_t * pcubPlaintextData,
										const uint8 * pubKey, size_t cubKey );
	[[nodiscard]] static bool SymmetricDecrypt( const uint8 * pubEncryptedData, size_t cubEncryptedData, 
								  uint8 * pubPlaintextData, size_t * pcubPlaintextData,
								  const uint8 * pubKey, size_t cubKey );

	// symmetrically encrypt data with a text password. A SHA256 hash of the password
	// is used as an AES encryption key (calls SymmetricEncrypt, above).
	// An HMAC of the ciphertext is appended, for authentication.
	[[nodiscard]] static bool EncryptWithPasswordAndHMAC( const uint8 *pubPlaintextData, size_t cubPlaintextData,
									 uint8 * pubEncryptedData, size_t * pcubEncryptedData,
									 const char *pchPassword );

	// Same as above but uses an explicit IV. The format of the ciphertext is the same.
	// Be sure you know what you're doing if you use this - a random IV is much more secure in general!
	[[nodiscard]] static bool EncryptWithPasswordAndHMACWithIV( const uint8 *pubPlaintextData, size_t cubPlaintextData,
											const uint8 * pIV, size_t cubIV,
											uint8 * pubEncryptedData, size_t * pcubEncryptedData,
											const char *pchPassword );

	// Symmetrically decrypt data with the given password (see above).
	// If the HMAC does not match what we expect, then we know that either the password is
	// incorrect or the message is corrupted.
	[[nodiscard]] static bool DecryptWithPasswordAndAuthenticate( const uint8 * pubEncryptedData, size_t cubEncryptedData, 
									 uint8 * pubPlaintextData, size_t * pcubPlaintextData,
									 const char *pchPassword );

	[[nodiscard]] static bool RSAGenerateKeys( uint8 *pubPublicKey, size_t *pcubPublicKey, uint8 *pubPrivateKey, size_t *pcubPrivateKey );

	[[nodiscard]] static bool RSAEncrypt( const uint8 *pubPlaintextPlaintextData, const size_t cubData, uint8 *pubEncryptedData, 
							size_t *pcubEncryptedData, const uint8 *pubPublicKey, const size_t cubPublicKey );
	[[nodiscard]] static bool RSADecrypt( const uint8 *pubEncryptedData, size_t cubEncryptedData, 
							uint8 *pubPlaintextData, size_t *pcubPlaintextData, const uint8 *pubPrivateKey, const size_t cubPrivateKey );

	// decrypt using a public key, and no padding
	[[nodiscard]] static bool RSAPublicDecrypt_NoPadding( const uint8 *pubEncryptedData, size_t cubEncryptedData, 
							uint8 *pubPlaintextData, size_t *pcubPlaintextData, const uint8 *pubPublicKey, const size_t cubPublicKey );

	[[nodiscard]] static bool RSASign( const uint8 *pubData, const size_t cubData, 
						 uint8 *pubSignature, size_t *pcubSignature, const uint8 * pubPrivateKey, const size_t cubPrivateKey );
	[[nodiscard]] static bool RSAVerifySignature( const uint8 *pubData, const size_t cubData, 
									const uint8 *pubSignature, const size_t cubSignature, 
									const uint8 *pubPublicKey, const size_t cubPublicKey );

	[[nodiscard]] static bool RSASignSHA256( const uint8 *pubData, const size_t cubData, 
						 uint8 *pubSignature, size_t *pcubSignature, const uint8 * pubPrivateKey, const size_t cubPrivateKey );
	[[nodiscard]] static bool RSAVerifySignatureSHA256( const uint8 *pubData, const size_t cubData, 
									const uint8 *pubSignature, const size_t cubSignature, 
									const uint8 *pubPublicKey, const size_t cubPublicKey );

	[[nodiscard]] static bool HexEncode( const uint8 *pubData, const size_t cubData, char *pchEncodedData, size_t cchEncodedData );
	[[nodiscard]] static bool HexDecode( const char *pchData, uint8 *pubDecodedData, size_t *pcubDecodedData );

	[[nodiscard]] static size_t Base64EncodeMaxOutput( size_t cubData, const char *pszLineBreakOrNull );
	[[nodiscard]] static bool Base64Encode( const uint8 *pubData, size_t cubData, char *pchEncodedData, size_t cchEncodedData, bool bInsertLineBreaks = true ); // legacy, deprecated
	[[nodiscard]] static bool Base64Encode( const uint8 *pubData, size_t cubData, char *pchEncodedData, size_t *pcchEncodedData, const char *pszLineBreak = "\n" );

	[[nodiscard]] static constexpr size_t Base64DecodeMaxOutput( size_t cubData ) { return ( (cubData + 3 ) / 4) * 3 + 1; }
	[[nodiscard]] static bool Base64Decode( const char *pchEncodedData, uint8 *pubDecodedData, size_t *pcubDecodedData, bool bIgnoreInvalidCharacters = true ); // legacy, deprecated
	[[nodiscard]] static bool Base64Decode( const char *pchEncodedData, size_t cchEncodedData, uint8 *pubDecodedData, size_t *pcubDecodedData, bool bIgnoreInvalidCharacters = true );

	[[nodiscard]] static bool GenerateSalt( Salt_t *pSalt );
	[[nodiscard]] static bool GenerateSHA1Digest( const uint8 *pubInput, const size_t cubInput, SHADigest_t *pOutDigest );
	[[nodiscard]] static bool GenerateSaltedSHA1Digest( const char *pchInput, const Salt_t *pSalt, SHADigest_t *pOutDigest );
	[[nodiscard]] static bool GenerateRandomBlock( uint8 *pubDest, size_t cubDest );

	[[nodiscard]] static bool GenerateHMAC( const uint8 *pubData, size_t cubData, const uint8 *pubKey, size_t cubKey, SHADigest_t *pOutputDigest );
	[[nodiscard]] static bool GenerateHMAC256( const uint8 *pubData, size_t cubData, const uint8 *pubKey, size_t cubKey, SHA256Digest_t *pOutputDigest );

	[[nodiscard]] static bool BGeneratePasswordHash( const char *pchInput, EPasswordHashAlg hashType, const Salt_t &Salt, PasswordHash_t &OutPasswordHash );
	[[nodiscard]] static bool BValidatePasswordHash( const char *pchInput, EPasswordHashAlg hashType, const PasswordHash_t &DigestStored, const Salt_t &Salt, PasswordHash_t *pDigestComputed );
	[[nodiscard]] static bool BGeneratePBKDF2Hash( const char *pchInput, const Salt_t &Salt, unsigned rounds, PasswordHash_t &OutPasswordHash );
	[[nodiscard]] static bool BGenerateWrappedSHA1PasswordHash( const char *pchInput, const Salt_t &Salt, unsigned rounds, PasswordHash_t &OutPasswordHash );
	[[nodiscard]] static bool BUpgradeOrWrapPasswordHash( PasswordHash_t &InPasswordHash, EPasswordHashAlg hashTypeIn, const Salt_t &Salt, PasswordHash_t &OutPasswordHash, EPasswordHashAlg &hashTypeOut, unsigned rounds = 10000 );

	[[nodiscard]] static bool BGzipBuffer( const uint8 *pubData, size_t cubData, CCryptoOutBuffer &bufOutput );
	[[nodiscard]] static bool BGunzipBuffer( const uint8 *pubData, size_t cubData, CCryptoOutBuffer &bufOutput );

#ifdef DBGFLAG_VALIDATE
	static void ValidateStatics( CValidator &validator, const char *pchName );
#endif
};

class CSimpleBitString;

//-----------------------------------------------------------------------------
// Purpose: Implement hex encoding / decoding using a custom lookup table.
//			This is a class because the decoding is done via a generated 
//			reverse-lookup table, and to save time it's best to just create
//			that table once.
//-----------------------------------------------------------------------------
class CCustomHexEncoder
{
public:
	CCustomHexEncoder( const char *pchEncodingTable );
	~CCustomHexEncoder() = default;

	bool Encode( const uint8 *pubData, const size_t cubData, char *pchEncodedData, size_t cchEncodedData );
	bool Decode( const char *pchData, uint8 *pubDecodedData, size_t *pcubDecodedData );

private:
	bool m_bValidEncoding;
	uint8 m_rgubEncodingTable[16];
	int m_rgnDecodingTable[256];
};

//-----------------------------------------------------------------------------
// Purpose: Implement base32 encoding / decoding using a custom lookup table.
//			This is a class because the decoding is done via a generated 
//			reverse-lookup table, and to save time it's best to just create
//			that table once.
//-----------------------------------------------------------------------------
class CCustomBase32Encoder
{
public:
	CCustomBase32Encoder( const char *pchEncodingTable );
	~CCustomBase32Encoder() = default;

	bool Encode( const uint8 *pubData, const size_t cubData, char *pchEncodedData, size_t cchEncodedData );
	bool Decode( const char *pchData, uint8 *pubDecodedData, size_t *pcubDecodedData );

	bool Encode( CSimpleBitString *pBitStringData, char *pchEncodedData, size_t cchEncodedData );
	bool Decode( const char *pchData, CSimpleBitString *pBitStringDecodedData );

private:
	bool m_bValidEncoding;
	uint8 m_rgubEncodingTable[32];
	int m_rgnDecodingTable[256];
};

#endif // CRYPTO_H
