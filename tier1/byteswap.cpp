//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Low level byte swapping routines.
//
// $NoKeywords: $
//=============================================================================

#include "tier1/byteswap.h"

#ifdef CByteswap
#undef CByteswap
#endif

//-----------------------------------------------------------------------------
// Copy a single field from the input buffer to the output buffer, swapping the bytes if necessary
//-----------------------------------------------------------------------------
template<bool isBigEndian>
void CByteswap<isBigEndian>::SwapFieldToTargetEndian( void* pOutputBuffer, const void *pData, typedescription_t *pField )
{
	switch ( pField->fieldType )
	{
	case FIELD_CHARACTER:
		SwapBufferToTargetEndian<char>( (char*)pOutputBuffer, (const char*)pData, pField->fieldSize );
		break;

	case FIELD_BOOLEAN:
		SwapBufferToTargetEndian<bool>( (bool*)pOutputBuffer, (const bool*)pData, pField->fieldSize );
		break;

	case FIELD_SHORT:
		SwapBufferToTargetEndian<short>( (short*)pOutputBuffer, (const short*)pData, pField->fieldSize );
		break;

	case FIELD_FLOAT:
		SwapBufferToTargetEndian<uint>( (uint*)pOutputBuffer, (const uint*)pData, pField->fieldSize );
		break;

	case FIELD_INTEGER:
		SwapBufferToTargetEndian<int>( (int*)pOutputBuffer, (const int*)pData, pField->fieldSize );
		break;

	case FIELD_VECTOR:
		SwapBufferToTargetEndian<uint>( (uint*)pOutputBuffer, (const uint*)pData, pField->fieldSize * 3 );
		break;

	case FIELD_VECTOR2D:
		SwapBufferToTargetEndian<uint>( (uint*)pOutputBuffer, (const uint*)pData, pField->fieldSize * 2 );
		break;

	case FIELD_QUATERNION:
		SwapBufferToTargetEndian<uint>( (uint*)pOutputBuffer, (const uint*)pData, pField->fieldSize * 4 );
		break;

	case FIELD_EMBEDDED:
		{
			typedescription_t *pEmbed = pField->td->dataDesc;
			for ( int i = 0; i < pField->fieldSize; ++i )
			{
				SwapFieldsToTargetEndian( (byte*)pOutputBuffer + pEmbed->fieldOffset[ TD_OFFSET_NORMAL ], 
										(byte*)pData + pEmbed->fieldOffset[ TD_OFFSET_NORMAL ],  
										pField->td );

				pOutputBuffer = (byte*)pOutputBuffer + pField->fieldSizeInBytes;
				pData = (byte*)pData + pField->fieldSizeInBytes;
			}
		}
		break;
		
	default:
		Assert(0); 
	}
}

//-----------------------------------------------------------------------------
// Write a block of fields. Works a bit like the saverestore code.  
//-----------------------------------------------------------------------------
template<bool isBigEndian>
void CByteswap<isBigEndian>::SwapFieldsToTargetEndian( void *pOutputBuffer, const void *pBaseData, datamap_t *pDataMap)
{
	// deal with base class first
	if ( pDataMap->baseMap )
	{
		SwapFieldsToTargetEndian( pOutputBuffer, pBaseData, pDataMap->baseMap );
	}

	typedescription_t *pFields = pDataMap->dataDesc;
	int fieldCount = pDataMap->dataNumFields;
	for ( int i = 0; i < fieldCount; ++i )
	{
		typedescription_t *pField = &pFields[i];
		SwapFieldToTargetEndian( (byte*)pOutputBuffer + pField->fieldOffset[ TD_OFFSET_NORMAL ],  
								 (byte*)pBaseData + pField->fieldOffset[ TD_OFFSET_NORMAL ], 
								  pField );
	}
}

void InstantiateTemplates()
{
	[[maybe_unused]] CByteswap<false> falseObj;
	falseObj.SwapFieldToTargetEndian(nullptr, nullptr, static_cast<typedescription_t*>(nullptr));
	falseObj.SwapFieldsToTargetEndian(nullptr, nullptr, static_cast<datamap_t*>(nullptr));

	[[maybe_unused]] CByteswap<true> trueObj;
	falseObj.SwapFieldToTargetEndian(nullptr, nullptr, static_cast<typedescription_t*>(nullptr));
	falseObj.SwapFieldsToTargetEndian(nullptr, nullptr, static_cast<datamap_t*>(nullptr));
}
