//===== Copyright c 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: Simple encoder/decoder
//
//===========================================================================//

#ifndef COMMON_VALVE_FONT
#define COMMON_VALVE_FONT

#include "simplecodec.h"
#include <ctime>
#include <array>

namespace ValveFont
{

//
// Encodes the font buffer, buffer size is increased automatically
//
inline void EncodeFont( CUtlBuffer &buffer )
{
	srand( (unsigned) time( nullptr ) );

	intp numBytes = buffer.TellPut();
	int numSultBytes = 0x10 + ( rand() % 0x10 );
	
	constexpr char szTag[]{"VFONT1"};
	constexpr intp numTagBytes = std::size( szTag ) - 1;

	for ( int k = 0; k < numSultBytes; ++ k )
		buffer.PutUnsignedChar( k );
	for ( intp k = 0; k < numTagBytes; ++ k )
		buffer.PutUnsignedChar( szTag[ k ] );

	SimpleCodec::EncodeBuffer( ( unsigned char * ) buffer.Base(), numBytes, numSultBytes );
}

//
// Decodes font buffer, returns true if successful, buffer's put pointer is positioned
// at the end of the font data.
// Returns false on failure.
//
inline bool DecodeFont( CUtlBuffer &buffer )
{
	intp numTotalBytes = buffer.TellPut();

	constexpr char szTag[]{"VFONT1"};
	constexpr intp numTagBytes = std::size( szTag ) - 1;
	
	if ( numTotalBytes <= numTagBytes )
		return false;

	if ( memcmp( (( unsigned char * ) buffer.Base() ) + numTotalBytes - numTagBytes,
		szTag, numTagBytes ) )
		return false;

	numTotalBytes -= numTagBytes;
	SimpleCodec::DecodeBuffer( ( unsigned char * ) buffer.Base(), numTotalBytes );

	buffer.SeekPut( CUtlBuffer::SEEK_HEAD, numTotalBytes );
	return true;
}

}; // namespace ValveFont

#endif // COMMON_VALVE_FONT

