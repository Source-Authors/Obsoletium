//========= Copyright Valve Corporation, All rights reserved. ============//

#if defined( POSIX )
	#include <iconv.h>
#endif

#include "tier1/ilocalize.h"

#include "tier1/utlstring.h"

//-----------------------------------------------------------------------------
// Purpose: converts an english string to unicode
//-----------------------------------------------------------------------------
int ILocalize::ConvertANSIToUnicode(const char *ansi, OUT_Z_BYTECAP(unicodeBufferSizeInBytes) wchar_t *unicode, int unicodeBufferSizeInBytes)
{
#ifdef POSIX
	// Q_UTF8ToUnicode returns the number of bytes. This function is expected to return the number of chars.
	return Q_UTF8ToUnicode(ansi, unicode, unicodeBufferSizeInBytes) / sizeof( wchar_t );
#else
  return V_UTF8ToUCS2(ansi, -1, unicode, unicodeBufferSizeInBytes);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: converts an unicode string to an english string
//-----------------------------------------------------------------------------
int ILocalize::ConvertUnicodeToANSI(const wchar_t *unicode, OUT_Z_BYTECAP(ansiBufferSize) char *ansi, int ansiBufferSize)
{
#ifdef POSIX
	return Q_UnicodeToUTF8(unicode, ansi, ansiBufferSize);
#else
  return V_UCS2ToUTF8(unicode, ansi, ansiBufferSize);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: construct string helper
//-----------------------------------------------------------------------------
template < typename T >
void ConstructStringVArgsInternal_Impl(T *unicodeOutput, intp unicodeBufferSizeInBytes, const T *formatString, int numFormatParameters, va_list argList)
{
	constexpr int k_cMaxFormatStringArguments = 9; // We only look one character ahead and start at %s1
	Assert( numFormatParameters <= k_cMaxFormatStringArguments );

	// Safety check
	if ( unicodeOutput == NULL || unicodeBufferSizeInBytes < 1 )
	{
		return;
	}

	if ( !formatString || numFormatParameters > k_cMaxFormatStringArguments )
	{
		unicodeOutput[0] = 0;
		return;
	}

	intp unicodeBufferSize = unicodeBufferSizeInBytes / static_cast<intp>(sizeof(T));
	const T *searchPos = formatString;
	T *outputPos = unicodeOutput;

	T *argParams[k_cMaxFormatStringArguments];
	for ( int i = 0; i < numFormatParameters; i++ )
	{
		argParams[i] = va_arg( argList, T* );
	}

	//assumes we can't have %s10
	//assume both are 0 terminated?
	intp formatLength = StringFuncs<T>::Length( formatString );

	while ( searchPos[0] != '\0' && unicodeBufferSize > 1 )
	{
		if ( formatLength >= 3 && searchPos[0] == '%' && searchPos[1] == 's' )
		{
			//this is an escape sequence - %s1, %s2 etc, up to %s9

			int argindex = ( searchPos[2] ) - '0' - 1; // 0 for %s1, 1 for %s2, etc.

			if ( argindex < 0 || argindex > k_cMaxFormatStringArguments )
			{
				Warning( "Bad format string in CLocalizeStringTable::ConstructString\n" );
				*outputPos = '\0';
				return;
			}

			if ( argindex < numFormatParameters )
			{
				T const *param = argParams[argindex];

				if ( param == NULL )
					param = StringFuncs<T>::NullDebugString();

				intp paramSize = StringFuncs<T>::Length(param);
				if (paramSize >= unicodeBufferSize)
				{
					paramSize = unicodeBufferSize - 1;
				}

				memcpy(outputPos, param, paramSize * sizeof(T));

				unicodeBufferSize -= paramSize;
				outputPos += paramSize;

				searchPos += 3;
				formatLength -= 3;
			}
			else
			{
				AssertMsg( argindex < numFormatParameters, "ConstructStringVArgsInternal_Impl() - Found a %%s# escape sequence whose index was more than the number of args." );

				//copy it over, char by char
				*outputPos = *searchPos;

				outputPos++;
				unicodeBufferSize--;

				searchPos++;
				formatLength--;
			}
		}
		else
		{
			//copy it over, char by char
			*outputPos = *searchPos;

			outputPos++;
			unicodeBufferSize--;

			searchPos++;
			formatLength--;
		}
	}

	// ensure null termination
	Assert( outputPos - unicodeOutput < (intp)(unicodeBufferSizeInBytes/sizeof(T)) );
	*outputPos = L'\0';
}

void ILocalize::ConstructStringVArgsInternal(OUT_Z_BYTECAP(unicodeBufferSizeInBytes) char *unicodeOutput, intp unicodeBufferSizeInBytes, const char *formatString, int numFormatParameters, va_list argList)
{
	ConstructStringVArgsInternal_Impl<char>( unicodeOutput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList );
}

void ILocalize::ConstructStringVArgsInternal(OUT_Z_BYTECAP(unicodeBufferSizeInBytes) wchar_t *unicodeOutput, intp unicodeBufferSizeInBytes, const wchar_t *formatString, int numFormatParameters, va_list argList)
{
	ConstructStringVArgsInternal_Impl<wchar_t>( unicodeOutput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList );
}

//-----------------------------------------------------------------------------
// Purpose: construct string helper
//-----------------------------------------------------------------------------
template < typename T >
const T *GetTypedKeyValuesString( KeyValues *pKeyValues, const char *pKeyName );

template < >
const char *GetTypedKeyValuesString<char>( KeyValues *pKeyValues, const char *pKeyName )
{
	return pKeyValues->GetString( pKeyName, "[unknown]" );
}

template < >
const wchar_t *GetTypedKeyValuesString<wchar_t>( KeyValues *pKeyValues, const char *pKeyName )
{
	return pKeyValues->GetWString( pKeyName, L"[unknown]" );
}

template < typename T >
void ConstructStringKeyValuesInternal_Impl( T *unicodeOutput, intp unicodeBufferSizeInBytes, const T *formatString, KeyValues *localizationVariables )
{
	T *outputPos = unicodeOutput;

	//assumes we can't have %s10
	//assume both are 0 terminated?
	intp unicodeBufferSize = unicodeBufferSizeInBytes / sizeof(T);

	while ( *formatString != '\0' && unicodeBufferSize > 1 )
	{
		bool shouldAdvance = true;

		if ( *formatString == '%' )
		{
			// this is an escape sequence that specifies a variable name
			if ( formatString[1] == 's' && formatString[2] >= '0' && formatString[2] <= '9' )
			{
				// old style escape sequence, ignore
			}
			else if ( formatString[1] == '%' )
			{
				// just a '%' char, just write the second one
				formatString++;
			}
			else if ( localizationVariables )
			{
				// get out the variable name
				const T *varStart = formatString + 1;
				const T *varEnd = StringFuncs<T>::FindChar( varStart, '%' );

				if ( varEnd && *varEnd == '%' )
				{
					shouldAdvance = false;

					// assume variable names must be ascii, do a quick convert
					char variableName[32];
					char *vset = variableName;
					for ( const T *pws = varStart; pws < varEnd && (vset < variableName + sizeof(variableName) - 1); ++pws, ++vset )
					{
						*vset = (char)*pws;
					}
					*vset = 0;

					// look up the variable name
					const T *value = GetTypedKeyValuesString<T>( localizationVariables, variableName );
					
					intp paramSize = StringFuncs<T>::Length( value );
					if (paramSize >= unicodeBufferSize)
					{
						paramSize = MAX( static_cast<intp>(0), unicodeBufferSize - 1 );
					}

					StringFuncs<T>::Copy( outputPos, value, paramSize );

					unicodeBufferSize -= paramSize;
					outputPos += paramSize;
					formatString = varEnd + 1;
				}
			}
		}

		if (shouldAdvance)
		{
			//copy it over, char by char
			*outputPos = *formatString;

			outputPos++;
			unicodeBufferSize--;

			formatString++;
		}		
	}

	// ensure null termination
	*outputPos = '\0';
}

void ILocalize::ConstructStringKeyValuesInternal(OUT_Z_BYTECAP(unicodeBufferSizeInBytes) char *unicodeOutput, intp unicodeBufferSizeInBytes, const char *formatString, KeyValues *localizationVariables)
{
	ConstructStringKeyValuesInternal_Impl<char>( unicodeOutput, unicodeBufferSizeInBytes, formatString, localizationVariables );
}

void ILocalize::ConstructStringKeyValuesInternal(OUT_Z_BYTECAP(unicodeBufferSizeInBytes) wchar_t *unicodeOutput, intp unicodeBufferSizeInBytes, const wchar_t *formatString, KeyValues *localizationVariables)
{
	ConstructStringKeyValuesInternal_Impl<wchar_t>( unicodeOutput, unicodeBufferSizeInBytes, formatString, localizationVariables );
}
