//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "isaverestore.h"
#include "saverestoretypes.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: iterates through a typedescript data block, so it can insert key/value data into the block
// Input  : *pObject - pointer to the struct or class the data is to be insterted into
//			*pFields - description of the data
//			iNumFields - number of fields contained in pFields
//			char *szKeyName - name of the variable to look for
//			char *szValue - value to set the variable to
// Output : Returns true if the variable is found and set, false if the key is not found.
//-----------------------------------------------------------------------------
bool ParseKeyvalue( void *pObject, typedescription_t *pFields, int iNumFields, const char *szKeyName, const char *szValue )
{
	int i;
	typedescription_t 	*pField;

	for ( i = 0; i < iNumFields; i++ )
	{
		pField = &pFields[i];

		int fieldOffset = pField->fieldOffset[ TD_OFFSET_NORMAL ];

		// Check the nested classes, but only if they aren't in array form.
		if ((pField->fieldType == FIELD_EMBEDDED) && (pField->fieldSize == 1))
		{
			for ( datamap_t *dmap = pField->td; dmap != NULL; dmap = dmap->baseMap )
			{
				void *pEmbeddedObject = (void*)((char*)pObject + fieldOffset);
				if ( ParseKeyvalue( pEmbeddedObject, dmap->dataDesc, dmap->dataNumFields, szKeyName, szValue) )
					return true;
			}
		}

		if ( (pField->flags & FTYPEDESC_KEY) && !stricmp(pField->externalName, szKeyName) )
		{
			switch( pField->fieldType )
			{
			case FIELD_MODELNAME:
			case FIELD_SOUNDNAME:
			case FIELD_STRING:
			{
				// dimhotepus: Fix UB on cast to different type pointer & write to.
				const string_t string = AllocPooledString( szValue );
				V_memcpy( (byte *)pObject + fieldOffset, &string, sizeof(string) );
				return true;
			}

			case FIELD_TIME:
			case FIELD_FLOAT:
			{
				// dimhotepus: Fix UB on cast to different type pointer & write to.
				const float val = strtof( szValue, nullptr );
				V_memcpy( (byte *)pObject + fieldOffset, &val, sizeof(float) );
				return true;
			}

			case FIELD_BOOLEAN:
			{
				// dimhotepus: Fix UB on cast to different type pointer & write to.
				const bool b = atoi( szValue ) != 0;
				V_memcpy( (byte *)pObject + fieldOffset, &b, sizeof(b) );
				return true;
			}

			case FIELD_CHARACTER:
			{
				// dimhotepus: Fix UB on cast to different type pointer & write to.
				const char c = (char)atoi( szValue );
				V_memcpy( (byte *)pObject + fieldOffset, &c, sizeof(c) );
				return true;
			}

			case FIELD_SHORT:
			{
				// dimhotepus: Fix UB on cast to different type pointer & write to.
				const short s = (short)atoi( szValue );
				V_memcpy( (byte *)pObject + fieldOffset, &s, sizeof(s) );
				return true;
			}

			case FIELD_INTEGER:
			case FIELD_TICK:
			{
				// dimhotepus: Fix UB on cast to different type pointer & write to.
				const int tick = atoi( szValue );
				V_memcpy( (byte *)pObject + fieldOffset, &tick, sizeof(tick) );
				return true;
			}

			case FIELD_POSITION_VECTOR:
			case FIELD_VECTOR:
			{
				// dimhotepus: Fix UB on cast to different type pointer & write to.
				Vector vec;
				UTIL_StringToVector( vec, szValue );
				V_memcpy( (byte *)pObject + fieldOffset, &vec, sizeof(vec) );
				return true;
			}

			case FIELD_VMATRIX:
			case FIELD_VMATRIX_WORLDSPACE:
				UTIL_StringToFloatArray( (float *)((char *)pObject + fieldOffset), 16, szValue );
				return true;

			case FIELD_MATRIX3X4_WORLDSPACE:
				UTIL_StringToFloatArray( (float *)((char *)pObject + fieldOffset), 12, szValue );
				return true;

			case FIELD_COLOR32:
				UTIL_StringToColor32( (color32 *) ((char *)pObject + fieldOffset), szValue );
				return true;

			case FIELD_CUSTOM:
			{
				SaveRestoreFieldInfo_t fieldInfo =
				{
					(byte *)pObject + fieldOffset,
					pObject,
					pField
				};
				pField->pSaveRestoreOps->Parse( fieldInfo, szValue );
				return true;
			}

			default:
			case FIELD_INTERVAL: // Fixme, could write this if needed
			case FIELD_CLASSPTR:
			case FIELD_MODELINDEX:
			case FIELD_MATERIALINDEX:
			case FIELD_EDICT:
				Warning( "Bad field in entity!!\n" );
				Assert(0);
				break;
			}
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: iterates through a typedescript data block, so it can insert key/value data into the block
// Input  : *pObject - pointer to the struct or class the data is to be insterted into
//			*pFields - description of the data
//			iNumFields - number of fields contained in pFields
//			char *szKeyName - name of the variable to look for
//			char *szValue - value to set the variable to
// Output : Returns true if the variable is found and set, false if the key is not found.
//-----------------------------------------------------------------------------
bool ExtractKeyvalue( void *pObject, typedescription_t *pFields, int iNumFields, const char *szKeyName, char *szValue, int iMaxLen )
{
	int i;
	typedescription_t 	*pField;

	for ( i = 0; i < iNumFields; i++ )
	{
		pField = &pFields[i];

		int fieldOffset = pField->fieldOffset[ TD_OFFSET_NORMAL ];

		// Check the nested classes, but only if they aren't in array form.
		if ((pField->fieldType == FIELD_EMBEDDED) && (pField->fieldSize == 1))
		{
			for ( datamap_t *dmap = pField->td; dmap != NULL; dmap = dmap->baseMap )
			{
				void *pEmbeddedObject = (void*)((char*)pObject + fieldOffset);
				if ( ExtractKeyvalue( pEmbeddedObject, dmap->dataDesc, dmap->dataNumFields, szKeyName, szValue, iMaxLen ) )
					return true;
			}
		}

		if ( (pField->flags & FTYPEDESC_KEY) && !stricmp(pField->externalName, szKeyName) )
		{
			switch( pField->fieldType )
			{
			case FIELD_MODELNAME:
			case FIELD_SOUNDNAME:
			case FIELD_STRING:
				Q_strncpy( szValue, ((char *)pObject + fieldOffset), iMaxLen );
				return true;

			case FIELD_TIME:
			case FIELD_FLOAT:
				Q_snprintf( szValue, iMaxLen, "%f", (*(float *)((char *)pObject + fieldOffset)) );
				return true;

			case FIELD_BOOLEAN:
				Q_snprintf( szValue, iMaxLen, "%d", (*(bool *)((char *)pObject + fieldOffset)) != 0);
				return true;

			case FIELD_CHARACTER:
				Q_snprintf( szValue, iMaxLen, "%d", (*(char *)((char *)pObject + fieldOffset)) );
				return true;

			case FIELD_SHORT:
				Q_snprintf( szValue, iMaxLen, "%d", (*(short *)((char *)pObject + fieldOffset)) );
				return true;

			case FIELD_INTEGER:
			case FIELD_TICK:
				Q_snprintf( szValue, iMaxLen, "%d", (*(int *)((char *)pObject + fieldOffset)) );
				return true;

			case FIELD_POSITION_VECTOR:
			case FIELD_VECTOR:
				Q_snprintf( szValue, iMaxLen, "%f %f %f", 
					((float *)((char *)pObject + fieldOffset))[0],
					((float *)((char *)pObject + fieldOffset))[1],
					((float *)((char *)pObject + fieldOffset))[2] );
				return true;

			case FIELD_VMATRIX:
			case FIELD_VMATRIX_WORLDSPACE:
				//UTIL_StringToFloatArray( (float *)((char *)pObject + fieldOffset), 16, szValue );
				return false;

			case FIELD_MATRIX3X4_WORLDSPACE:
				//UTIL_StringToFloatArray( (float *)((char *)pObject + fieldOffset), 12, szValue );
				return false;

			case FIELD_COLOR32:
				Q_snprintf( szValue, iMaxLen, "%d %d %d %d", 
					((int *)((char *)pObject + fieldOffset))[0],
					((int *)((char *)pObject + fieldOffset))[1],
					((int *)((char *)pObject + fieldOffset))[2],
					((int *)((char *)pObject + fieldOffset))[3] );
				return true;

			case FIELD_CUSTOM:
			{
				/*
				SaveRestoreFieldInfo_t fieldInfo =
				{
					(char *)pObject + fieldOffset,
					pObject,
					pField
				};
				pField->pSaveRestoreOps->Parse( fieldInfo, szValue );
				*/
				return false;
			}

			default:
			case FIELD_INTERVAL: // Fixme, could write this if needed
			case FIELD_CLASSPTR:
			case FIELD_MODELINDEX:
			case FIELD_MATERIALINDEX:
			case FIELD_EDICT:
				Warning( "Bad field in entity!!\n" );
				Assert(0);
				break;
			}
		}
	}

	return false;
}
