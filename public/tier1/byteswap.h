//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Low level byte swapping routines.
//
// $NoKeywords: $
//=============================================================================
#ifndef BYTESWAP_H
#define BYTESWAP_H

#include "datamap.h"	// Needed for typedescription_t.  Note datamap.h is tier1 as well.
#include "tier0/basetypes.h"
#include "tier0/dbg.h"
#include "tier0/platform.h"

#ifdef CByteswap
#undef CByteswap
#endif

template<bool isBigEndian = endian::native == endian::big>
class CByteswap
{
public:
	constexpr CByteswap() : m_bSwapBytes(false), m_bBigEndian(isBigEndian)
	{
		// Default behavior sets the target endian to match the machine native endian (no swap).
	}

	//-----------------------------------------------------------------------------
	// Write a single field.
	//-----------------------------------------------------------------------------
	void SwapFieldToTargetEndian( void* pOutputBuffer, const void *pData, typedescription_t *pField );

	//-----------------------------------------------------------------------------
	// Write a block of fields.  Works a bit like the saverestore code.  
	//-----------------------------------------------------------------------------
	void SwapFieldsToTargetEndian( void *pOutputBuffer, const void *pBaseData, datamap_t *pDataMap );

	// Swaps fields for the templated type to the output buffer.
	template<typename T> inline void SwapFieldsToTargetEndian( T* pOutputBuffer, const void *pBaseData, unsigned int objectCount = 1 )
	{
		for ( unsigned int i = 0; i < objectCount; ++i, ++pOutputBuffer )
		{
			SwapFieldsToTargetEndian( static_cast<void*>(pOutputBuffer), pBaseData, &T::m_DataMap );
			pBaseData = static_cast<const byte*>(pBaseData) + sizeof(T);
		}
	}

	// Swaps fields for the templated type in place.
	template<typename T> inline void SwapFieldsToTargetEndian( T* pOutputBuffer, unsigned int objectCount = 1 )
	{
		SwapFieldsToTargetEndian<T>( pOutputBuffer, static_cast<void*>(pOutputBuffer), objectCount );
	}

	//-----------------------------------------------------------------------------
	// True if the current machine is detected as big endian. 
	// (Endienness is effectively detected at compile time when optimizations are
	// enabled)
	//-----------------------------------------------------------------------------
	[[nodiscard]] constexpr bool IsMachineBigEndian()
	{
		return endian::native == endian::big;
	}

	//-----------------------------------------------------------------------------
	// Sets the target byte ordering we are swapping to or from.
	//
	// Braindead Endian Reference:
	//		x86 is LITTLE Endian
	//		PowerPC is BIG Endian
	//-----------------------------------------------------------------------------
	constexpr inline void SetTargetBigEndian( bool bigEndian )
	{
		m_bBigEndian = bigEndian;
		m_bSwapBytes = IsMachineBigEndian() != bigEndian;
	}

	// Changes target endian
	constexpr inline void FlipTargetEndian()
	{
		m_bSwapBytes = !m_bSwapBytes;
		m_bBigEndian = !m_bBigEndian;
	}

	// Forces byte swapping state, regardless of endianess
	constexpr inline void ActivateByteSwapping( bool bActivate )	
	{
		SetTargetBigEndian( IsMachineBigEndian() != bActivate );
	}

	//-----------------------------------------------------------------------------
	// Returns true if the target machine is the same as this one in endianness.
	//
	// Used to determine when a byteswap needs to take place.
	//-----------------------------------------------------------------------------
	[[nodiscard]] constexpr inline bool IsSwappingBytes() const	// Are bytes being swapped?
	{
		return m_bSwapBytes;
	}

	[[nodiscard]] constexpr inline bool IsTargetBigEndian() const	// What is the current target endian?
	{
		return m_bBigEndian;
	}

	//-----------------------------------------------------------------------------
	// IsByteSwapped()
	//
	// When supplied with a chunk of input data and a constant or magic number
	// (in native format) determines the endienness of the current machine in
	// relation to the given input data.
	//
	// Returns:
	//		1  if input is the same as nativeConstant.
	//		0  if input is byteswapped relative to nativeConstant.
	//		-1 if input is not the same as nativeConstant and not byteswapped either.
	//
	// ( This is useful for detecting byteswapping in magic numbers in structure 
	// headers for example. )
	//-----------------------------------------------------------------------------
	template<typename T> [[nodiscard]] inline int SourceIsNativeEndian( T input, T nativeConstant )
	{
		// If it's the same, it isn't byteswapped:
		if( input == nativeConstant )
			return 1;

		int output;
		LowLevelByteSwap<T>( output, input );
		if( output == nativeConstant )
			return 0;

		Assert( 0 );		// if we get here, input is neither a swapped nor unswapped version of nativeConstant.
		return -1;
	}

	//-----------------------------------------------------------------------------
	// Swaps an input buffer full of type T into the given output buffer.
	//
	// Swaps [count] items from the inputBuffer to the outputBuffer.
	// If inputBuffer is omitted or nullptr, then it is assumed to be the same as
	// outputBuffer - effectively swapping the contents of the buffer in place.
	//-----------------------------------------------------------------------------
	template<typename T> inline void SwapBuffer( T* outputBuffer, T* inputBuffer = nullptr, intp count = 1 )
	{
		Assert( count >= 0 );
		Assert( outputBuffer );

		// Fail gracefully in release:
		if( count <=0 || !outputBuffer )
			return;

		// Optimization for the case when we are swapping in place.
		if( inputBuffer == nullptr )
		{
			inputBuffer = outputBuffer;
		}

		// Swap everything in the buffer:
		for( intp i = 0; i < count; i++ )
		{
			LowLevelByteSwap<T>( outputBuffer[i], inputBuffer[i] );
		}
	}

	//-----------------------------------------------------------------------------
	// Swaps an input buffer full of type T into the given output buffer.
	//
	// Swaps [count] items from the inputBuffer to the outputBuffer.
	// If inputBuffer is omitted or nullptr, then it is assumed to be the same as
	// outputBuffer - effectively swapping the contents of the buffer in place.
	//-----------------------------------------------------------------------------
	template<typename T> inline void SwapBufferToTargetEndian( T* outputBuffer, const T* inputBuffer = nullptr, intp count = 1 )
	{
		Assert( count >= 0 );
		Assert( outputBuffer );

		// Fail gracefully in release:
		if( count <= 0 || !outputBuffer )
			return;

		// Optimization for the case when we are swapping in place.
		if( inputBuffer == nullptr )
		{
			inputBuffer = outputBuffer;
		}

		// Are we already the correct endienness? ( or are we swapping 1 byte items? )
		if ( !m_bSwapBytes || ( sizeof(T) == 1 ) )
		{
			// Otherwise copy the inputBuffer to the outputBuffer:
			memcpy( outputBuffer, inputBuffer, count * sizeof( T ) );
			return;
		}

		// Swap everything in the buffer:
		for( intp i = 0; i < count; i++ )
		{
			LowLevelByteSwap<T>( outputBuffer[i], inputBuffer[i] );
		}
	}

private:
	//-----------------------------------------------------------------------------
	// The lowest level byte swapping workhorse of doom.  output always contains the 
	// swapped version of input.  ( Doesn't compare machine to target endianness )
	//-----------------------------------------------------------------------------
	template <typename T>
	inline void LowLevelByteSwap( T &output, const T &input )
	{
		if constexpr (sizeof(T) == 1U)
		{
			output = input;
		}
		else if constexpr (sizeof(T) == sizeof(uint16))
		{
			if constexpr (std::is_same_v<T, unsigned short>)
			{
				output = Plat_ByteSwap16(input);
			}
			else
			{
				uint16 v;
				memcpy(&v, &input, sizeof(T));

				v = Plat_ByteSwap16(v);

				memcpy(&output, &v, sizeof(T));
			}
		}
		else if constexpr (sizeof(T) == sizeof(uint32))
		{
			if constexpr (std::is_same_v<T, unsigned int> || std::is_same_v<T, unsigned long>)
			{
				output = Plat_ByteSwap32(input);
			}
			else
			{
				uint32 v;
				memcpy(&v, &input, sizeof(T));

				v = Plat_ByteSwap32(v);

				memcpy(&output, &v, sizeof(T));
			}
		}
		else if constexpr (sizeof(T) == sizeof(uint64))
		{
			if constexpr (std::is_same_v<T, uint64>)
			{
				output = Plat_ByteSwap64( input );
			}
			else
			{
				uint64 v;
				memcpy(&v, &input, sizeof(T));

				v = Plat_ByteSwap64(v);

				memcpy(&output, &v, sizeof(T));
			}
		}
		else
		{
			static_assert(false, "Unknown size " V_STRINGIFY(sizeof(T)) " not in {2,4,8} set.");
		}
	}

	unsigned int m_bSwapBytes : 1;
	unsigned int m_bBigEndian : 1;
};


#define CByteswap CByteswap<>

#endif /* !BYTESWAP_H */
