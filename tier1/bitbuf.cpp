//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "tier1/bitbuf.h"
#include "coordsize.h"
#include "mathlib/vector.h"
#include "mathlib/mathlib.h"
#include "tier1/strtools.h"
#include "bitvec.h"

#include <atomic>

#include "tier0/memdbgon.h"

#if _WIN32
#define FAST_BIT_SCAN 1

#include <intrin.h>
#pragma intrinsic(_BitScanReverse)
#pragma intrinsic(_BitScanForward)

[[nodiscard]] [[maybe_unused]] static inline unsigned CountLeadingZeros(unsigned x)
{
	unsigned long firstBit;
	if ( _BitScanReverse(&firstBit,x) )
		return 31 - firstBit;
	return CHAR_BIT * sizeof(unsigned);
}
[[nodiscard]] [[maybe_unused]] static inline unsigned CountTrailingZeros(unsigned elem)
{
	unsigned long out;
	if ( _BitScanForward(&out, elem) )
		return out;
	return CHAR_BIT * sizeof(unsigned);
}

#else
#define FAST_BIT_SCAN 0
#endif


static std::atomic<BitBufErrorHandler> g_BitBufErrorHandler{nullptr};

[[nodiscard]] static constexpr inline int BitForBitnum(int bitnum)
{
	return GetBitForBitnum(bitnum);
}

void InternalBitBufErrorHandler( BitBufErrorType errorType, const char *pDebugName )
{
	const auto copy = g_BitBufErrorHandler.load(std::memory_order::memory_order_relaxed);
	if ( copy )
		copy( errorType, pDebugName );
}


BitBufErrorHandler SetBitBufErrorHandler( BitBufErrorHandler fn )
{
  return g_BitBufErrorHandler.exchange(fn);
}


// #define BB_PROFILING

unsigned g_LittleBits[CHAR_BIT * sizeof(uint32)];

// Precalculated bit masks for WriteUBitLong. Using these tables instead of 
// doing the calculations gives a 33% speedup in WriteUBitLong.
static unsigned g_BitWriteMasks[CHAR_BIT * sizeof(uint32)]
                               [CHAR_BIT * sizeof(uint32) + 1];

// (1 << i) - 1
unsigned g_ExtraMasks[CHAR_BIT * sizeof(uint32) + 1];

class CBitWriteMasksInit
{
public:
	CBitWriteMasksInit()
	{
		for ( unsigned startbit=0; startbit < CHAR_BIT * sizeof(uint32); startbit++ )
		{
			for ( unsigned nBitsLeft=0; nBitsLeft < std::size(g_BitWriteMasks[0]); nBitsLeft++ )
			{
				unsigned endbit = startbit + nBitsLeft;
				g_BitWriteMasks[startbit][nBitsLeft] = BitForBitnum(startbit) - 1;

				if (endbit < CHAR_BIT * sizeof(uint32))
					g_BitWriteMasks[startbit][nBitsLeft] |= ~(BitForBitnum(endbit) - 1);
			}
		}

		for ( unsigned maskBit=0; maskBit < CHAR_BIT * sizeof(uint32); maskBit++ )
			g_ExtraMasks[maskBit] = BitForBitnum(maskBit) - 1;

		g_ExtraMasks[CHAR_BIT * sizeof(uint32)] = ~0u;

		for ( size_t littleBit=0; littleBit < std::size(g_LittleBits); littleBit++ )
			StoreLittleDWord( &g_LittleBits[littleBit], 0, 1u<<littleBit );
	}
};
static CBitWriteMasksInit g_BitWriteMasksInit;


// ---------------------------------------------------------------------------------------- //
// bf_write
// ---------------------------------------------------------------------------------------- //

bf_write::bf_write()
{
	m_pData = nullptr;
	m_nDataBytes = 0;
	m_nDataBits = -1; // set to -1 so we generate overflow on any operation
	m_iCurBit = 0;
	m_bOverflow = false;
	m_bAssertOnOverflow = true;
	m_pDebugName = nullptr;
}

bf_write::bf_write( const char *pDebugName, void *pData, intp nBytes, intp nBits )
{
	m_bAssertOnOverflow = true;
	m_pDebugName = pDebugName;
	StartWriting( pData, nBytes, 0, nBits );
}

bf_write::bf_write( void *pData, intp nBytes, intp nBits )
{
	m_bAssertOnOverflow = true;
	m_pDebugName = nullptr;
	StartWriting( pData, nBytes, 0, nBits );
}

void bf_write::StartWriting( void *pData, intp nBytes, intp iStartBit, intp nBits )
{
	// Make sure it's dword aligned and padded.
	Assert( (nBytes % 4) == 0 );
	Assert( ((uintp)pData & 3) == 0);

	// The writing code will overrun the end of the buffer if it isn't dword aligned, so truncate to force alignment
	nBytes &= ~3;

	m_pData = static_cast<unsigned*>(pData);
	m_nDataBytes = nBytes;

	if ( nBits == -1 )
	{
		m_nDataBits = nBytes * CHAR_BIT;
	}
	else
	{
		Assert( nBits <= nBytes * CHAR_BIT );
		m_nDataBits = nBits;
	}

	m_iCurBit = iStartBit;
	m_bOverflow = false;
}

void bf_write::Reset()
{
	m_iCurBit = 0;
	m_bOverflow = false;
}


void bf_write::SetAssertOnOverflow( bool bAssert )
{
	m_bAssertOnOverflow = bAssert;
}


const char* bf_write::GetDebugName() const
{
	return m_pDebugName;
}


void bf_write::SetDebugName( const char *pDebugName )
{
	m_pDebugName = pDebugName;
}


void bf_write::SeekToBit( intp bitPos )
{
	m_iCurBit = bitPos;
}


// Sign bit comes first
void bf_write::WriteSBitLong( int32 data, int numbits )
{
	// Force the sign-extension bit to be correct even in the case of overflow.
	int32 nValue = data;
	int32 nPreserveBits = ( 0x7FFFFFFF >> ( static_cast<int32>(CHAR_BIT * sizeof(int32)) - numbits ) );
	int32 nSignExtension = ( nValue >> 31 ) & ~nPreserveBits;
	nValue &= nPreserveBits;
	nValue |= nSignExtension;
	
	AssertMsg2( nValue == data, "WriteSBitLong: 0x%08x does not fit in %d bits", data, numbits );

	WriteUBitLong( nValue, numbits, false );
}

void bf_write::WriteVarInt32( uint32 data )
{
	// Check if align and we have room, slow path if not
	if ( (m_iCurBit & 7) == 0 && (m_iCurBit + (intp)bitbuf::kMaxVarint32Bytes * CHAR_BIT ) <= m_nDataBits)
	{
		uint8 *target = ((uint8*)m_pData) + (m_iCurBit>>3);

		target[0] = static_cast<uint8>(data | 0x80);
		if ( data >= (1 << 7) )
		{
			target[1] = static_cast<uint8>((data >>  7) | 0x80);
			if ( data >= (1 << 14) )
			{
				target[2] = static_cast<uint8>((data >> 14) | 0x80);
				if ( data >= (1 << 21) )
				{
					target[3] = static_cast<uint8>((data >> 21) | 0x80);
					if ( data >= (1 << 28) )
					{
						target[4] = static_cast<uint8>(data >> 28);
						m_iCurBit += 5 * CHAR_BIT;
						return;
					}
					else
					{
						target[3] &= 0x7F;
						m_iCurBit += 4 * CHAR_BIT;
						return;
					}
				}
				else
				{
					target[2] &= 0x7F;
					m_iCurBit += 3 * CHAR_BIT;
					return;
				}
			}
			else
			{
				target[1] &= 0x7F;
				m_iCurBit += 2 * CHAR_BIT;
				return;
			}
		}
		else
		{
			target[0] &= 0x7F;
			m_iCurBit += 1 * CHAR_BIT;
			return;
		}
	}
	else // Slow path
	{
		while ( data > 0x7F ) 
		{
			WriteUBitLong( (data & 0x7F) | 0x80, CHAR_BIT );
			data >>= 7;
		}
		WriteUBitLong( data & 0x7F, CHAR_BIT );
	}
}

void bf_write::WriteVarInt64( uint64 data )
{
	// Check if align and we have room, slow path if not
	if ( (m_iCurBit & 7) == 0 && (m_iCurBit + bitbuf::kMaxVarintBytes * CHAR_BIT ) <= m_nDataBits )
	{
		uint8 *target = ((uint8*)m_pData) + (m_iCurBit / CHAR_BIT);

		// Splitting into 32-bit pieces gives better performance on 32-bit
		// processors.
		uint32 part0 = static_cast<uint32>(data      );
		uint32 part1 = static_cast<uint32>(data >> 28);
		uint32 part2 = static_cast<uint32>(data >> 56);

		int size;

		// Here we can't really optimize for small numbers, since the data is
		// split into three parts.  Cheking for numbers < 128, for instance,
		// would require three comparisons, since you'd have to make sure part1
		// and part2 are zero.  However, if the caller is using 64-bit integers,
		// it is likely that they expect the numbers to often be very large, so
		// we probably don't want to optimize for small numbers anyway.  Thus,
		// we end up with a hardcoded binary search tree...
		if ( part2 == 0 )
		{
			if ( part1 == 0 )
			{
				if ( part0 < (1 << 14) )
				{
					if ( part0 < (1 << 7) )
					{
						size = 1; goto size1;
					}
					else
					{
						size = 2; goto size2;
					}
				}
				else
				{
					if ( part0 < (1 << 21) )
					{
						size = 3; goto size3;
					}
					else
					{
						size = 4; goto size4;
					}
				}
			}
			else
			{
				if ( part1 < (1 << 14) )
				{
					if ( part1 < (1 << 7) )
					{
						size = 5; goto size5;
					}
					else
					{
						size = 6; goto size6;
					}
				}
				else
				{
					if ( part1 < (1 << 21) )
					{
						size = 7; goto size7;
					}
					else
					{
						size = 8; goto size8;
					}
				}
			}
		}
		else
		{
			if ( part2 < (1 << 7) )
			{
				size = 9; goto size9;
			}
			else
			{
				size = 10; goto size10;
			}
		}

		UNREACHABLE();

		size10: target[9] = static_cast<uint8>((part2 >>  7) | 0x80);
		size9 : target[8] = static_cast<uint8>((part2      ) | 0x80);
		size8 : target[7] = static_cast<uint8>((part1 >> 21) | 0x80);
		size7 : target[6] = static_cast<uint8>((part1 >> 14) | 0x80);
		size6 : target[5] = static_cast<uint8>((part1 >>  7) | 0x80);
		size5 : target[4] = static_cast<uint8>((part1      ) | 0x80);
		size4 : target[3] = static_cast<uint8>((part0 >> 21) | 0x80);
		size3 : target[2] = static_cast<uint8>((part0 >> 14) | 0x80);
		size2 : target[1] = static_cast<uint8>((part0 >>  7) | 0x80);
		size1 : target[0] = static_cast<uint8>((part0      ) | 0x80);

		target[size-1] &= 0x7F;
		m_iCurBit += size * CHAR_BIT;
	}
	else // slow path
	{
		while ( data > 0x7F ) 
		{
			WriteUBitLong( (data & 0x7F) | 0x80, CHAR_BIT );
			data >>= 7;
		}
		WriteUBitLong( data & 0x7F, CHAR_BIT );
	}
}

void bf_write::WriteSignedVarInt32( int32 data )
{
	WriteVarInt32( bitbuf::ZigZagEncode32( data ) );
}

void bf_write::WriteSignedVarInt64( int64 data )
{
	WriteVarInt64( bitbuf::ZigZagEncode64( data ) );
}

int	bf_write::ByteSizeVarInt32( uint32 data )
{
	int size = 1;
	while ( data > 0x7F ) {
		size++;
		data >>= 7;
	}
	return size;
}

int	bf_write::ByteSizeVarInt64( uint64 data )
{
	int size = 1;
	while ( data > 0x7F ) {
		size++;
		data >>= 7;
	}
	return size;
}

int bf_write::ByteSizeSignedVarInt32( int32 data )
{
	return ByteSizeVarInt32( bitbuf::ZigZagEncode32( data ) );
}

int bf_write::ByteSizeSignedVarInt64( int64 data )
{
	return ByteSizeVarInt64( bitbuf::ZigZagEncode64( data ) );
}

void bf_write::WriteBitLong(uint32 data, int numbits, bool bSigned)
{
	if(bSigned)
		WriteSBitLong((int32)data, numbits);
	else
		WriteUBitLong(data, numbits);
}

bool bf_write::WriteBits(const void *pInData, intp nBits)
{
#if defined( BB_PROFILING )
	VPROF( "bf_write::WriteBits" );
#endif

	const auto *pOut = (const unsigned char*)pInData;
	intp nBitsLeft = nBits;

	// Bounds checking..
	if ( (m_iCurBit+nBits) > m_nDataBits )
	{
		SetOverflowFlag();
		CallErrorHandler( BITBUFERROR_BUFFER_OVERRUN, GetDebugName() );
		return false;
	}

	// Align output to dword boundary
	while (((uintp)pOut & 3) != 0 && nBitsLeft >= CHAR_BIT)
	{
		WriteUBitLong( *pOut, CHAR_BIT, false );
		++pOut;
		nBitsLeft -= CHAR_BIT;
	}
	
	if ( (nBitsLeft >= 32) && (m_iCurBit & 7) == 0 )
	{
		// current bit is byte aligned, do block copy
		intp numbytes = nBitsLeft / CHAR_BIT; 
		intp numbits = numbytes * CHAR_BIT;
		
		Q_memcpy( (char*)m_pData+(m_iCurBit>>3), pOut, numbytes );
		pOut += numbytes;
		nBitsLeft -= numbits;
		m_iCurBit += numbits;
	}

	constexpr int kUint32Bits = CHAR_BIT * sizeof(uint32);

	if ( nBitsLeft >= kUint32Bits )
	{
		unsigned iBitsRight = (m_iCurBit & 31);
		unsigned iBitsLeft = kUint32Bits - iBitsRight;
		unsigned bitMaskLeft = g_BitWriteMasks[iBitsRight][kUint32Bits];
		unsigned bitMaskRight = g_BitWriteMasks[0][iBitsRight];

		unsigned *pData = &m_pData[m_iCurBit>>5];

		// Read dwords.
		while ( nBitsLeft >= kUint32Bits )
		{
			uint32 curData = *(uint32 *)pOut;
			pOut += sizeof(uint32);

			*pData &= bitMaskLeft;
			*pData |= curData << iBitsRight;

			pData++; 

			if ( iBitsLeft < kUint32Bits )
			{
				curData >>= iBitsLeft;
				*pData &= bitMaskRight;
				*pData |= curData;
			}

			nBitsLeft -= kUint32Bits;
			m_iCurBit += kUint32Bits;
		}
	}

	// write remaining bytes
	while ( nBitsLeft >= CHAR_BIT )
	{
		WriteUBitLong( *pOut, CHAR_BIT, false );
		++pOut;
		nBitsLeft -= CHAR_BIT;
	}
	
	// write remaining bits
	if ( nBitsLeft )
	{
		WriteUBitLong( *pOut, static_cast<int>(nBitsLeft), false );
	}

	return !IsOverflowed();
}


bool bf_write::WriteBitsFromBuffer( bf_read *pIn, intp nBits )
{
	constexpr int kUint32Bits = CHAR_BIT * sizeof(uint32);

	// This could be optimized a little by
	while ( nBits > kUint32Bits )
	{
		WriteUBitLong( pIn->ReadUBitLong( kUint32Bits ), kUint32Bits );
		nBits -= kUint32Bits;
	}

	WriteUBitLong( pIn->ReadUBitLong( static_cast<int>(nBits) ), static_cast<int>(nBits) );
	return !IsOverflowed() && !pIn->IsOverflowed();
}


void bf_write::WriteBitAngle( float fAngle, int numbits )
{
	unsigned shift = BitForBitnum(numbits);
	unsigned mask = shift - 1;

	int d = (int)( (fAngle / 360.0) * shift );
	d &= mask;

	WriteUBitLong((unsigned int)d, numbits);
}

void bf_write::WriteBitCoordMP( const float f, bool bIntegral, bool bLowPrecision )
{
#if defined( BB_PROFILING )
	VPROF( "bf_write::WriteBitCoordMP" );
#endif
	int		signbit = (f <= -( bLowPrecision ? COORD_RESOLUTION_LOWPRECISION : COORD_RESOLUTION ));
	int		intval = (int)abs(f);
	int		fractval = bLowPrecision ? 
		( abs((int)(f*COORD_DENOMINATOR_LOWPRECISION)) & (COORD_DENOMINATOR_LOWPRECISION-1) ) :
		( abs((int)(f*COORD_DENOMINATOR)) & (COORD_DENOMINATOR-1) );

	bool    bInBounds = intval < (1 << COORD_INTEGER_BITS_MP );

	unsigned int bits, numbits;

	if ( bIntegral )
	{
		// Integer encoding: in-bounds bit, nonzero bit, optional sign bit + integer value bits
		if ( intval )
		{
			// Adjust the integers from [1..MAX_COORD_VALUE] to [0..MAX_COORD_VALUE-1]
			--intval;
			bits = intval * CHAR_BIT + signbit * 4 + 2 + bInBounds;
			numbits = 3 + (bInBounds ? COORD_INTEGER_BITS_MP : COORD_INTEGER_BITS);
		}
		else
		{
			bits = bInBounds;
			numbits = 2;
		}
	}
	else
	{
		// Float encoding: in-bounds bit, integer bit, sign bit, fraction value bits, optional integer value bits
		if ( intval )
		{
			// Adjust the integers from [1..MAX_COORD_VALUE] to [0..MAX_COORD_VALUE-1]
			--intval;
			bits = intval * CHAR_BIT + signbit * 4 + 2 + bInBounds;
			bits += bInBounds ? (fractval << (3+COORD_INTEGER_BITS_MP)) : (fractval << (3+COORD_INTEGER_BITS));
			numbits = 3 + (bInBounds ? COORD_INTEGER_BITS_MP : COORD_INTEGER_BITS)
						+ (bLowPrecision ? COORD_FRACTIONAL_BITS_MP_LOWPRECISION : COORD_FRACTIONAL_BITS);
		}
		else
		{
			bits = fractval * CHAR_BIT + signbit * 4 + 0 + bInBounds;
			numbits = 3 + (bLowPrecision ? COORD_FRACTIONAL_BITS_MP_LOWPRECISION : COORD_FRACTIONAL_BITS);
		}
	}

	WriteUBitLong( bits, numbits );
}

void bf_write::WriteBitCoord (const float f)
{
#if defined( BB_PROFILING )
	VPROF( "bf_write::WriteBitCoord" );
#endif
	int		signbit = (f <= -COORD_RESOLUTION);
	int		intval = (int)abs(f);
	int		fractval = abs((int)(f*COORD_DENOMINATOR)) & (COORD_DENOMINATOR-1);


	// Send the bit flags that indicate whether we have an integer part and/or a fraction part.
	WriteOneBit( intval );
	WriteOneBit( fractval );

	if ( intval || fractval )
	{
		// Send the sign bit
		WriteOneBit( signbit );

		// Send the integer if we have one.
		if ( intval )
		{
			// Adjust the integers from [1..MAX_COORD_VALUE] to [0..MAX_COORD_VALUE-1]
			intval--;
			WriteUBitLong( (unsigned int)intval, COORD_INTEGER_BITS );
		}
		
		// Send the fraction if we have one
		if ( fractval )
		{
			WriteUBitLong( (unsigned int)fractval, COORD_FRACTIONAL_BITS );
		}
	}
}

void bf_write::WriteBitVec3Coord( const Vector& fa )
{
	int xflag = (fa[0] >= COORD_RESOLUTION) || (fa[0] <= -COORD_RESOLUTION);
	int yflag = (fa[1] >= COORD_RESOLUTION) || (fa[1] <= -COORD_RESOLUTION);
	int zflag = (fa[2] >= COORD_RESOLUTION) || (fa[2] <= -COORD_RESOLUTION);

	WriteOneBit( xflag );
	WriteOneBit( yflag );
	WriteOneBit( zflag );

	if ( xflag )
		WriteBitCoord( fa[0] );
	if ( yflag )
		WriteBitCoord( fa[1] );
	if ( zflag )
		WriteBitCoord( fa[2] );
}

void bf_write::WriteBitNormal( float f )
{
	int	signbit = (f <= -NORMAL_RESOLUTION);

	// NOTE: Since +/-1 are valid values for a normal, I'm going to encode that as all ones
	unsigned int fractval = abs( (int)(f*NORMAL_DENOMINATOR) );

	// clamp..
	if (fractval > NORMAL_DENOMINATOR)
		fractval = NORMAL_DENOMINATOR;

	// Send the sign bit
	WriteOneBit( signbit );

	// Send the fractional component
	WriteUBitLong( fractval, NORMAL_FRACTIONAL_BITS );
}

void bf_write::WriteBitVec3Normal( const Vector& fa )
{
	int xflag = (fa[0] >= NORMAL_RESOLUTION) || (fa[0] <= -NORMAL_RESOLUTION);
	int yflag = (fa[1] >= NORMAL_RESOLUTION) || (fa[1] <= -NORMAL_RESOLUTION);

	WriteOneBit( xflag );
	WriteOneBit( yflag );

	if ( xflag )
		WriteBitNormal( fa[0] );
	if ( yflag )
		WriteBitNormal( fa[1] );
	
	// Write z sign bit
	int	signbit = (fa[2] <= -NORMAL_RESOLUTION);
	WriteOneBit( signbit );
}

void bf_write::WriteBitAngles( const QAngle& fa )
{
	// FIXME:
	Vector tmp( fa.x, fa.y, fa.z );
	WriteBitVec3Coord( tmp );
}

void bf_write::WriteChar(char val)
{
	// dimhotepus: Correctly write char as it may be signed or unsigned depend on platform.
	if constexpr (std::is_signed_v<decltype(val)>)
	{
		WriteSBitLong(val, sizeof(val) * CHAR_BIT);
	}
	else
	{
		WriteUBitLong(val, sizeof(val) * CHAR_BIT);
	}
}

void bf_write::WriteByte(uint8 val)
{
	WriteUBitLong(val, sizeof(val) * CHAR_BIT);
}

void bf_write::WriteShort(int16 val)
{
	WriteSBitLong(val, sizeof(val) * CHAR_BIT);
}

void bf_write::WriteWord(uint16 val)
{
	WriteUBitLong(val, sizeof(val) * CHAR_BIT);
}

void bf_write::WriteLong(int32 val)
{
	WriteSBitLong(val, sizeof(int32) * CHAR_BIT);
}

void bf_write::WriteULong(uint32 val)
{
	WriteUBitLong(val, sizeof(uint32) * CHAR_BIT);
}

void bf_write::WriteLongLong(int64 val)
{
	uint32 *pLongs = (uint32*)&val;

	// dimhotepus: Fix writing int64 bits in LP64 model.
	// Insert the two DWORDS according to network endian
	constexpr short endianIndex = 0x0100;
	byte *idx = (byte*)&endianIndex;
	WriteUBitLong(pLongs[*idx++], sizeof(uint32) * CHAR_BIT);
	WriteUBitLong(pLongs[*idx], sizeof(uint32) * CHAR_BIT);
}

void bf_write::WriteULongLong(uint64 val)
{
	uint32 *pLongs = (uint32*)&val;

	// dimhotepus: Fix writing int64 bits in LP64 model.
	// Insert the two DWORDS according to network endian
	constexpr short endianIndex = 0x0100;
	byte *idx = (byte*)&endianIndex;
	WriteUBitLong(pLongs[*idx++], sizeof(uint32) * CHAR_BIT);
	WriteUBitLong(pLongs[*idx], sizeof(uint32) * CHAR_BIT);
}

void bf_write::WriteFloat(float val)
{
	// Pre-swap the float, since WriteBits writes raw data
	LittleFloat( &val, &val );

	WriteBits(&val, sizeof(val) * CHAR_BIT);
}

bool bf_write::WriteBytes( const void *pBuf, intp nBytes )
{
	return WriteBits(pBuf, nBytes * CHAR_BIT);
}

bool bf_write::WriteString(const char *pStr)
{
	if(pStr)
	{
		do
		{
			WriteChar( *pStr );
			++pStr;
		} while( *(pStr-1) != 0 );
	}
	else
	{
		WriteChar( 0 );
	}

	return !IsOverflowed();
}

// ---------------------------------------------------------------------------------------- //
// bf_read
// ---------------------------------------------------------------------------------------- //

bf_read::bf_read()
{
	m_pData = NULL;
	m_nDataBytes = 0;
	m_nDataBits = -1; // set to -1 so we overflow on any operation
	m_iCurBit = 0;
	m_bOverflow = false;
	m_bAssertOnOverflow = true;
	m_pDebugName = NULL;
}

bf_read::bf_read( const void *pData, intp nBytes, intp nBits )
{
	m_bAssertOnOverflow = true;
	StartReading( pData, nBytes, 0, nBits );
}

bf_read::bf_read( const char *pDebugName, const void *pData, intp nBytes, intp nBits )
{
	m_bAssertOnOverflow = true;
	m_pDebugName = pDebugName;
	StartReading( pData, nBytes, 0, nBits );
}

void bf_read::StartReading( const void *pData, intp nBytes, intp iStartBit, intp nBits )
{
	// Make sure we're dword aligned.
	Assert(((uintp)pData & 3) == 0);

	m_pData = (unsigned char*)pData;
	m_nDataBytes = nBytes;

	if ( nBits == -1 )
	{
		m_nDataBits = m_nDataBytes * CHAR_BIT;
	}
	else
	{
		Assert( nBits <= nBytes * CHAR_BIT );
		m_nDataBits = nBits;
	}

	m_iCurBit = iStartBit;
	m_bOverflow = false;
}

void bf_read::Reset()
{
	m_iCurBit = 0;
	m_bOverflow = false;
}

void bf_read::SetAssertOnOverflow( bool bAssert )
{
	m_bAssertOnOverflow = bAssert;
}

void bf_read::SetDebugName( const char *pName )
{
	m_pDebugName = pName;
}

void bf_read::SetOverflowFlag()
{
	if ( m_bAssertOnOverflow )
	{
		Assert( false );
	}
	m_bOverflow = true;
}

unsigned int bf_read::CheckReadUBitLong(int numbits)
{
	// Ok, just read bits out.
	int i, nBitValue;
	unsigned int r = 0;

	for(i=0; i < numbits; i++)
	{
		nBitValue = ReadOneBitNoCheck();
		r |= nBitValue << i;
	}
	m_iCurBit -= numbits;
	
	return r;
}

void bf_read::ReadBits(void *pOutData, intp nBits)
{
#if defined( BB_PROFILING )
	VPROF( "bf_read::ReadBits" );
#endif

	uint8 *pOut = static_cast<uint8 *>(pOutData);
	intp nBitsLeft = nBits;
	
	// align output to dword boundary
	while( ((size_t)pOut & 3) != 0 && nBitsLeft >= CHAR_BIT )
	{
		*pOut = (unsigned char)ReadUBitLong(CHAR_BIT);
		++pOut;
		nBitsLeft -= CHAR_BIT;
	}

	// dimhotepus: Fix reading int32 bits in LP64 model.
	constexpr int kUint32Bits{CHAR_BIT * sizeof(uint32)};

	// read dwords
	while ( nBitsLeft >= kUint32Bits )
	{
		*((uint32*)pOut) = ReadUBitLong(kUint32Bits);
		pOut += sizeof(uint32);
		nBitsLeft -= kUint32Bits;
	}

	// read remaining bytes
	while ( nBitsLeft >= CHAR_BIT )
	{
		*pOut = static_cast<uint8>(ReadUBitLong(CHAR_BIT));
		++pOut;
		nBitsLeft -= CHAR_BIT;
	}
	
	// read remaining bits
	if ( nBitsLeft )
	{
		*pOut = static_cast<uint8>(ReadUBitLong(static_cast<int>(nBitsLeft)));
	}
}

intp bf_read::ReadBitsClamped_ptr(void *pOutData, intp outSizeBytes, intp nBits)
{
	intp outSizeBits = outSizeBytes * CHAR_BIT;
	intp readSizeBits = nBits;
	intp skippedBits = 0;
	if ( readSizeBits > outSizeBits )
	{
		// Should we print a message when we clamp the data being read? Only
		// in debug builds I think.
		AssertMsg( 0, "Oversized network packet received, and clamped." );
		readSizeBits = outSizeBits;
		skippedBits = nBits - outSizeBits;
		// What should we do in this case, which should only happen if nBits
		// is negative for some reason?
		//if ( skippedBits < 0 )
		//	return 0;
	}

	ReadBits( pOutData, readSizeBits );
	SeekRelative( skippedBits );

	// Return the number of bits actually read.
	return readSizeBits;
}

float bf_read::ReadBitAngle( int numbits )
{
	float shift = (float)( BitForBitnum(numbits) );

	int i = ReadUBitLong( numbits );
	float fReturn = (float)i * (360.0f / shift);

	return fReturn;
}

uint32 bf_read::PeekUBitLong( int numbits )
{
#ifdef BIT_VERBOSE
	int nShifts = numbits;
#endif

	bf_read savebf = *this;  // Save current state info

	uint32 r = 0;
	for(int i=0; i < numbits; i++)
	{
		int nBitValue = ReadOneBit();

		// Append to current stream
		if ( nBitValue )
		{
			r |= BitForBitnum(i);
		}
	}
	
	*this = savebf;

#ifdef BIT_VERBOSE
	Con_Printf( "PeekBitLong:  %i %u\n", nShifts, r );
#endif

	return r;
}

uint32 bf_read::ReadUBitLongNoInline( int numbits )
{
	return ReadUBitLong( numbits );
}

unsigned int bf_read::ReadUBitVarInternal( int encodingType )
{
	m_iCurBit -= 4;
	// int bits = { 4, 8, 12, 32 }[ encodingType ];
	int bits = 4 + encodingType*4 + (((2 - encodingType) >> 31) & 16);
	return ReadUBitLong( bits );
}

// Append numbits least significant bits from data to the current bit stream
int32 bf_read::ReadSBitLong( int numbits )
{
	uint32 r = ReadUBitLong(numbits);
	uint32 s = 1 << (numbits-1);
	if (r >= s)
	{
		// sign-extend by removing sign bit and then subtracting sign bit again
		r = r - s - s;
	}
	return r;
}

uint32 bf_read::ReadVarInt32()
{
	uint32 result = 0;
	int count = 0;
	uint32 b;

	do 
	{
		if ( count == bitbuf::kMaxVarint32Bytes ) 
		{
			return result;
		}
		b = ReadUBitLong( CHAR_BIT );
		result |= (b & 0x7F) << (7 * count);
		++count;
	} while (b & 0x80);

	return result;
}

uint64 bf_read::ReadVarInt64()
{
	uint64 result = 0;
	int count = 0;
	uint64 b;

	do 
	{
		if ( count == bitbuf::kMaxVarintBytes ) 
		{
			return result;
		}
		b = ReadUBitLong( CHAR_BIT );
		result |= static_cast<uint64>(b & 0x7F) << (7 * count);
		++count;
	} while (b & 0x80);

	return result;
}

int32 bf_read::ReadSignedVarInt32()
{
	uint32 value = ReadVarInt32();
	return bitbuf::ZigZagDecode32( value );
}

int64 bf_read::ReadSignedVarInt64()
{
	uint64 value = ReadVarInt64();
	return bitbuf::ZigZagDecode64( value );
}

unsigned int bf_read::ReadBitLong(int numbits, bool bSigned)
{
	if(bSigned)
		return (unsigned int)ReadSBitLong(numbits);
	else
		return ReadUBitLong(numbits);
}


// Basic Coordinate Routines (these contain bit-field size AND fixed point scaling constants)
float bf_read::ReadBitCoord (void)
{
#if defined( BB_PROFILING )
	VPROF( "bf_read::ReadBitCoord" );
#endif
	int		intval=0,fractval=0,signbit=0;
	float	value = 0.0;


	// Read the required integer and fraction flags
	intval = ReadOneBit();
	fractval = ReadOneBit();

	// If we got either parse them, otherwise it's a zero.
	if ( intval || fractval )
	{
		// Read the sign bit
		signbit = ReadOneBit();

		// If there's an integer, read it in
		if ( intval )
		{
			// Adjust the integers from [0..MAX_COORD_VALUE-1] to [1..MAX_COORD_VALUE]
			intval = ReadUBitLong( COORD_INTEGER_BITS ) + 1;
		}

		// If there's a fraction, read it in
		if ( fractval )
		{
			fractval = ReadUBitLong( COORD_FRACTIONAL_BITS );
		}

		// Calculate the correct floating point value
		value = intval + ((float)fractval * COORD_RESOLUTION);

		// Fixup the sign if negative.
		if ( signbit )
			value = -value;
	}

	return value;
}

float bf_read::ReadBitCoordMP( bool bIntegral, bool bLowPrecision )
{
#if defined( BB_PROFILING )
	VPROF( "bf_read::ReadBitCoordMP" );
#endif
	// BitCoordMP float encoding: inbounds bit, integer bit, sign bit, optional int bits, float bits
	// BitCoordMP integer encoding: inbounds bit, integer bit, optional sign bit, optional int bits.
	// int bits are always encoded as (value - 1) since zero is handled by the integer bit

	// With integer-only encoding, the presence of the third bit depends on the second
	int flags = ReadUBitLong(3 - bIntegral);
	enum { INBOUNDS=1, INTVAL=2, SIGN=4 };

	if ( bIntegral )
	{
		if ( flags & INTVAL )
		{
			// Read the third bit and the integer portion together at once
			unsigned int bits = ReadUBitLong( (flags & INBOUNDS) ? COORD_INTEGER_BITS_MP+1 : COORD_INTEGER_BITS+1 );
			// Remap from [0,N] to [1,N+1]
			int intval = (bits >> 1) + 1;
			return (bits & 1) ? -intval : intval;
		}
		return 0.f;
	}
	
	static constexpr float mul_table[4] =
	{
		1.f/(1<<COORD_FRACTIONAL_BITS),
		-1.f/(1<<COORD_FRACTIONAL_BITS),
		1.f/(1<<COORD_FRACTIONAL_BITS_MP_LOWPRECISION),
		-1.f/(1<<COORD_FRACTIONAL_BITS_MP_LOWPRECISION)
	};
	//equivalent to: float multiply = mul_table[ ((flags & SIGN) ? 1 : 0) + bLowPrecision*2 ];
	float multiply = *(float*)((uintptr_t)&mul_table[0] + (flags & 4) + bLowPrecision*8);

	static constexpr unsigned char numbits_table[8] =
	{
		COORD_FRACTIONAL_BITS,
		COORD_FRACTIONAL_BITS,
		COORD_FRACTIONAL_BITS + COORD_INTEGER_BITS,
		COORD_FRACTIONAL_BITS + COORD_INTEGER_BITS_MP,
		COORD_FRACTIONAL_BITS_MP_LOWPRECISION,
		COORD_FRACTIONAL_BITS_MP_LOWPRECISION,
		COORD_FRACTIONAL_BITS_MP_LOWPRECISION + COORD_INTEGER_BITS,
		COORD_FRACTIONAL_BITS_MP_LOWPRECISION + COORD_INTEGER_BITS_MP
	};
	unsigned int bits = ReadUBitLong( numbits_table[ (flags & (INBOUNDS|INTVAL)) + bLowPrecision*4 ] );

	if ( flags & INTVAL )
	{
		// Shuffle the bits to remap the integer portion from [0,N] to [1,N+1]
		// and then paste in front of the fractional parts so we only need one
		// int-to-float conversion.
		
		uint fracbitsMP = bits >> COORD_INTEGER_BITS_MP;
		uint fracbits = bits >> COORD_INTEGER_BITS;

		uint intmaskMP = ((1<<COORD_INTEGER_BITS_MP)-1);
		uint intmask = ((1<<COORD_INTEGER_BITS)-1);

		uint selectNotMP = (flags & INBOUNDS) - 1;

		fracbits -= fracbitsMP;
		fracbits &= selectNotMP;
		fracbits += fracbitsMP;

		intmask -= intmaskMP;
		intmask &= selectNotMP;
		intmask += intmaskMP;

		uint intpart = (bits & intmask) + 1;
		uint intbitsLow = intpart << COORD_FRACTIONAL_BITS_MP_LOWPRECISION;
		uint intbits = intpart << COORD_FRACTIONAL_BITS;
		uint selectNotLow = (uint)bLowPrecision - 1;
		
		intbits -= intbitsLow;
		intbits &= selectNotLow;
		intbits += intbitsLow;

		bits = fracbits | intbits;
	}

	return (int)bits * multiply;
}

unsigned int bf_read::ReadBitCoordBits (void)
{
#if defined( BB_PROFILING )
	VPROF( "bf_read::ReadBitCoordBits" );
#endif

	unsigned int flags = ReadUBitLong(2);
	if ( flags == 0 )
		return 0;

	static constexpr int numbits_table[3] =
	{
		COORD_INTEGER_BITS + 1,
		COORD_FRACTIONAL_BITS + 1,
		COORD_INTEGER_BITS + COORD_FRACTIONAL_BITS + 1
	};
	return ReadUBitLong( numbits_table[ flags-1 ] ) * 4 + flags;
}

unsigned int bf_read::ReadBitCoordMPBits( bool bIntegral, bool bLowPrecision )
{
#if defined( BB_PROFILING )
	VPROF( "bf_read::ReadBitCoordMPBits" );
#endif

	unsigned int flags = ReadUBitLong(2);
	enum { INBOUNDS=1, INTVAL=2 };
	int numbits = 0;

	if ( bIntegral )
	{
		if ( flags & INTVAL )
		{
			numbits = (flags & INBOUNDS) ? (1 + COORD_INTEGER_BITS_MP) : (1 + COORD_INTEGER_BITS);
		}
		else
		{
			return flags; // no extra bits
		}
	}
	else
	{
		static constexpr unsigned char numbits_table[8] =
		{
			1 + COORD_FRACTIONAL_BITS,
			1 + COORD_FRACTIONAL_BITS,
			1 + COORD_FRACTIONAL_BITS + COORD_INTEGER_BITS,
			1 + COORD_FRACTIONAL_BITS + COORD_INTEGER_BITS_MP,
			1 + COORD_FRACTIONAL_BITS_MP_LOWPRECISION,
			1 + COORD_FRACTIONAL_BITS_MP_LOWPRECISION,
			1 + COORD_FRACTIONAL_BITS_MP_LOWPRECISION + COORD_INTEGER_BITS,
			1 + COORD_FRACTIONAL_BITS_MP_LOWPRECISION + COORD_INTEGER_BITS_MP
		};
		numbits = numbits_table[ flags + bLowPrecision*4 ];
	}

	return flags + ReadUBitLong(numbits)*4;
}

void bf_read::ReadBitVec3Coord( Vector& fa )
{
	// This vector must be initialized! Otherwise, If any of the flags aren't set, 
	// the corresponding component will not be read and will be stack garbage.
	fa.Init( 0, 0, 0 );

	int xflag = ReadOneBit();
	int yflag = ReadOneBit(); 
	int zflag = ReadOneBit();

	if ( xflag )
		fa[0] = ReadBitCoord();
	if ( yflag )
		fa[1] = ReadBitCoord();
	if ( zflag )
		fa[2] = ReadBitCoord();
}

float bf_read::ReadBitNormal (void)
{
	// Read the sign bit
	int	signbit = ReadOneBit();

	// Read the fractional part
	unsigned int fractval = ReadUBitLong( NORMAL_FRACTIONAL_BITS );

	// Calculate the correct floating point value
	float value = (float)fractval * NORMAL_RESOLUTION;

	// Fixup the sign if negative.
	if ( signbit )
		value = -value;

	return value;
}

void bf_read::ReadBitVec3Normal( Vector& fa )
{
	int xflag = ReadOneBit();
	int yflag = ReadOneBit(); 

	if (xflag)
		fa[0] = ReadBitNormal();
	else
		fa[0] = 0.0f;

	if (yflag)
		fa[1] = ReadBitNormal();
	else
		fa[1] = 0.0f;

	// The first two imply the third (but not its sign)
	int znegative = ReadOneBit();

	float fafafbfb = fa[0] * fa[0] + fa[1] * fa[1];
	if (fafafbfb < 1.0f)
		fa[2] = sqrtf( 1.0f - fafafbfb );
	else
		fa[2] = 0.0f;

	if (znegative)
		fa[2] = -fa[2];
}

void bf_read::ReadBitAngles( QAngle& fa )
{
	Vector tmp;
	ReadBitVec3Coord( tmp );
	fa.Init( tmp.x, tmp.y, tmp.z );
}

int64 bf_read::ReadLongLong()
{
	int64 retval;
	uint32 *pLongs = (uint32*)&retval;
	
	// dimhotepus: Fix reading int64 bits in LP64 model.
	// Read the two DWORDs according to network endian
	constexpr short endianIndex = 0x0100;
	const byte *idx = (const byte*)&endianIndex;
	pLongs[*idx++] = ReadUBitLong(sizeof(uint32) * CHAR_BIT);
	pLongs[*idx] = ReadUBitLong(sizeof(uint32) * CHAR_BIT);

	return retval;
}

uint64 bf_read::ReadULongLong()
{
	uint64 retval;
	uint32 *pLongs = (uint32*)&retval;
	
	// dimhotepus: Fix reading int64 bits in LP64 model.
	// Read the two DWORDs according to network endian
	constexpr short endianIndex = 0x0100;
	const byte *idx = (const byte*)&endianIndex;
	pLongs[*idx++] = ReadUBitLong(sizeof(uint32) * CHAR_BIT);
	pLongs[*idx] = ReadUBitLong(sizeof(uint32) * CHAR_BIT);

	return retval;
}

float bf_read::ReadFloat()
{
	float ret;
	static_assert( sizeof(ret) == 4 );
	ReadBits(&ret, CHAR_BIT * sizeof(float));

	// Swap the float, since ReadBits reads raw data
	LittleFloat( &ret, &ret );
	return ret;
}

bool bf_read::ReadBytes( OUT_CAP(nBytes) void *pOut, intp nBytes)
{
	ReadBits(pOut, nBytes << 3);
	return !IsOverflowed();
}

bool bf_read::ReadString( OUT_Z_CAP(maxLen) char *pStr, intp maxLen, bool bLine, intp *pOutNumChars )
{
	Assert( maxLen != 0 );

	bool bTooSmall = false;
	intp iChar = 0;
	while(1)
	{
		char val = ReadChar();
		if ( val == 0 )
			break;
		else if ( bLine && val == '\n' )
			break;

		if ( iChar < (maxLen-1) )
		{
			pStr[iChar] = val;
			++iChar;
		}
		else
		{
			bTooSmall = true;
		}
	}

	// Make sure it's null-terminated.
	Assert( iChar < maxLen );
	pStr[iChar] = 0;

	if ( pOutNumChars )
		*pOutNumChars = iChar;

	return !IsOverflowed() && !bTooSmall;
}


char* bf_read::ReadAndAllocateString( bool *pOverflow )
{
	char str[2048];
	
	intp nChars;
	bool bOverflow = !ReadString( str, ssize(str), false, &nChars );
	if ( pOverflow )
		*pOverflow = bOverflow;

	// Now copy into the output and return it;
	char *pRet = new char[ nChars + 1 ];
	for ( int i=0; i <= nChars; i++ )
		pRet[i] = str[i];

	return pRet;
}

void bf_read::ExciseBits( intp startbit, intp bitstoremove )
{
	intp endbit = startbit + bitstoremove;
	intp remaining_to_end = m_nDataBits - endbit;

	bf_write temp;
	temp.StartWriting( (void *)m_pData, m_nDataBits * CHAR_BIT, startbit );

	Seek( endbit );

	for ( int i = 0; i < remaining_to_end; i++ )
	{
		temp.WriteOneBit( ReadOneBit() );
	}

	Seek( startbit );
	
	m_nDataBits -= bitstoremove;
	m_nDataBytes = m_nDataBits >> 3;
}

int bf_read::CompareBitsAt( int offset, bf_read * RESTRICT other, int otherOffset, int numbits ) RESTRICT
{
	if ( numbits == 0 )
		return 0;

	int overflow1 = offset + numbits > m_nDataBits;
	int overflow2 = otherOffset + numbits > other->m_nDataBits;

	int x = overflow1 | overflow2;
	if ( x != 0 )
		return x;

	unsigned iStartBit1 = offset & 31u;
	unsigned iStartBit2 = otherOffset & 31u;
	unsigned *pData1 = (unsigned*)m_pData + (offset >> 5);
	unsigned *pData2 = (unsigned*)other->m_pData + (otherOffset >> 5);
	unsigned *pData1End = pData1 + ((offset + numbits - 1) >> 5);
	unsigned *pData2End = pData2 + ((otherOffset + numbits - 1) >> 5);

	constexpr int kUint32Bits = CHAR_BIT * sizeof(uint32);

	while ( numbits > kUint32Bits )
	{
		x  = LoadLittleDWord( pData1, 0 ) >> iStartBit1;
		x ^= LoadLittleDWord( pData1, 1 ) << (kUint32Bits - iStartBit1);
		x ^= LoadLittleDWord( pData2, 0 ) >> iStartBit2;
		x ^= LoadLittleDWord( pData2, 1 ) << (kUint32Bits - iStartBit2);
		if ( x != 0 )
		{
			return x; 
		}
		++pData1;
		++pData2;
		numbits -= kUint32Bits;
	}

	x  = LoadLittleDWord( pData1, 0 ) >> iStartBit1;
	x ^= LoadLittleDWord( pData1End, 0 ) << (kUint32Bits - iStartBit1);
	x ^= LoadLittleDWord( pData2, 0 ) >> iStartBit2;
	x ^= LoadLittleDWord( pData2End, 0 ) << (kUint32Bits - iStartBit2);
	return x & g_ExtraMasks[ numbits ];
}
