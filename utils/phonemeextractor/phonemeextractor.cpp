//========= Copyright Valve Corporation, All rights reserved. ============//

#include "phonemeextractor/PhonemeExtractor.h"

// Face poser and util includes
#include "tier0/wchartypes.h"
#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "tier1/utlvector.h"

#include "PhonemeConverter.h"
#include "sentence.h"
#include "filesystem.h"

#include <cstdio>

#include "winlite.h"
MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()
MSVC_DISABLE_WARNING(4805)  // Mix BOOL and bool.
MSVC_DISABLE_WARNING(4996)  // GetVersionExA deprecated.
#include <sphelper.h>
#include <spddkhlp.h>
MSVC_END_WARNING_OVERRIDE_SCOPE()

// ATL Header Files
#include <atlbase.h>

// Extract phoneme grammar id
#define EP_GRAM_ID			101
// First rule of dynamic sentence rule set
#define DYN_SENTENCERULE	102
// # of milliseconds to allow for processing before timeout
#define SR_WAVTIMEOUT		4000
// Weight tag for rule to rule word/rule transitions
#define CONFIDENCE_WEIGHT	0.0f

//#define LOGGING		1

void LogReset()
{
#ifdef LOGGING
	// dimhotepus: Unique file name in current dir instead of c:\\fp.log
	static const char *fileName = _mktemp("pe_log_XXXXXX");
	FILE *fp = fopen( fileName, "w" );
	if ( fp )
		fclose( fp );
#endif
}

char *va( const char *fmt, ... );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *words - 
//-----------------------------------------------------------------------------
void LogWords( CSentence& sentence )
{
	Log( "Wordcount == %zd\n", sentence.m_Words.Count() );

	for ( const auto *w : sentence.m_Words )
	{
		Log( "Word %s %u to %u\n", w->GetWord(), w->m_uiStartByte, w->m_uiEndByte );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *phonemes - 
//-----------------------------------------------------------------------------
void LogPhonemes( CSentence& sentence )
{
	// dimhotepus: Reenable logging in LOGGING mode. It was always disabled.
#ifdef LOGGING
	Log( "Phonemecount == %zd\n", sentence.CountPhonemes() );

	for ( const auto *w : sentence.m_Words )
	{
		for ( const auto *p : w->m_Phonemes )
		{
			Log( "Phoneme %s %u to %u\n", p->GetTag(), p->m_uiStartByte, p->m_uiEndByte );
		}
	}
#endif
}

#define NANO_CONVERT 10000000.0f;

//-----------------------------------------------------------------------------
// Purpose: Walk list of words and phonemes and create phoneme tags in CSentence object
//  FIXME:  Right now, phonemes are assumed to evenly space out across a word.
// Input  : *converter - 
//			result - 
//			sentence - 
//-----------------------------------------------------------------------------
void EnumeratePhonemes( ISpPhoneConverter *converter, const ISpRecoResult* result, CSentence& sentence )
{
	USES_CONVERSION;

	// Grab access to element container
	ISpPhrase *phrase = ( ISpPhrase * )result;
	if ( !phrase )
		return;

	SPPHRASE *pElements;
	if ( FAILED( phrase->GetPhrase( &pElements ) ) )
		return;

	// Only use it if it's better/same size as what we already had on-hand
	if ( pElements->Rule.ulCountOfElements > 0 )
	{
		sentence.ResetToBase();

		// Walk list of words
		for ( ULONG i = 0; i < pElements->Rule.ulCountOfElements; i++ )
		{
			// Get start/end sample index
			const unsigned wordstart = (unsigned)(pElements->pElements[i].ulAudioStreamOffset + pElements->ullAudioStreamPosition);
			const unsigned wordend	 = wordstart + pElements->pElements[i].ulAudioSizeBytes;

			// Create word tag
			auto *w = new CWordTag( W2T( pElements->pElements[i].pszDisplayText ) );
			Assert( w );
			w->m_uiStartByte = wordstart;
			w->m_uiEndByte   = wordend;

			sentence.AddWordTag( w );

			// Count # of phonemes in this word
			SPPHONEID pstr[ 2 ];
			BitwiseClear( pstr );
			WCHAR wszPhoneme[ SP_MAX_PRON_LENGTH ];

			const SPPHONEID *current = pElements->pElements[i].pszPronunciation;
			float total_weight = 0.0f;
			while ( 1 )
			{
				SPPHONEID phoneme = *current++;
				if ( !phoneme )
					break;

				pstr[ 0 ] = phoneme;
				wszPhoneme[ 0 ] = L'\0';

				converter->IdToPhone( pstr, wszPhoneme );

				total_weight += WeightForPhoneme( W2A( wszPhoneme ) );
			}

			current = pElements->pElements[i].pszPronunciation;

			// Decide # of bytes/phoneme weight
			float psize = 0;
			if ( total_weight )
			{
				psize = ( wordend - wordstart ) / total_weight;
			}

			// Re-walk the phoneme list and create true phoneme tags
			float startWeight = 0.0f;
			while ( 1 )
			{
				SPPHONEID phoneme = *current++;
				if ( !phoneme )
					break;

				pstr[ 0 ] = phoneme;
				wszPhoneme[ 0 ] = L'\0';

				converter->IdToPhone( pstr, wszPhoneme );
 
				auto *p = new CPhonemeTag( W2A( wszPhoneme ) );
				Assert( p );
				
				float weight = WeightForPhoneme( W2A( wszPhoneme ) );

				p->m_uiStartByte = wordstart + (unsigned)( startWeight * psize );
				p->m_uiEndByte	 = p->m_uiStartByte + (unsigned)( psize * weight );

				startWeight += weight;

				// Convert to IPA phoneme code
				p->SetPhonemeCode( TextToPhoneme( p->GetTag() ) );

				sentence.AddPhonemeTag( w, p );
			}
		}
	}

	// Free memory
    ::CoTaskMemFree(pElements);
}

//-----------------------------------------------------------------------------
// Purpose: Create rules for each word in the reference sentence
//-----------------------------------------------------------------------------
struct WORDRULETYPE
{
	int					ruleId;
	SPSTATEHANDLE		hRule;
	CSpDynamicString	word;
	char				plaintext[ 256 ];
};

//-----------------------------------------------------------------------------
// Purpose: Creates start for word of sentence
// Input  : cpRecoGrammar - 
//			*root - 
//			*rules - 
//			word - 
//-----------------------------------------------------------------------------
void AddWordRule( ISpRecoGrammar* cpRecoGrammar, SPSTATEHANDLE *root, CUtlVector< WORDRULETYPE > *rules, CSpDynamicString& word )
{
	USES_CONVERSION;

	const intp idx = (*rules).AddToTail();

	WORDRULETYPE *newrule = &(*rules)[idx];
	newrule->ruleId = DYN_SENTENCERULE + idx + 1;
	newrule->word = word;

	V_strcpy_safe( newrule->plaintext, W2T( word ) );

	// Create empty rule
	HRESULT hr = cpRecoGrammar->CreateNewState( *root, &newrule->hRule );
	Assert( SUCCEEDED( hr ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : cpRecoGrammar - 
//			*from - 
//			*to - 
//-----------------------------------------------------------------------------
void AddWordTransitionRule( ISpRecoGrammar* cpRecoGrammar, WORDRULETYPE *from, WORDRULETYPE *to )
{
	USES_CONVERSION;

	Assert( from );

	if ( from && !to )
	{
		Plat_DebugString( va( "Transition from %s to TERM\r\n", from->plaintext ) );
	}
	else
	{
		Plat_DebugString( va( "Transition from %s to %s\r\n", from->plaintext, to->plaintext ) );
	}

	HRESULT hr = cpRecoGrammar->AddWordTransition( from->hRule,
		to ? to->hRule : nullptr,
		(WCHAR *)from->word,
		nullptr,
		SPWT_LEXICAL,
		CONFIDENCE_WEIGHT,
		nullptr );
	Assert( SUCCEEDED( hr ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : cpRecoGrammar - 
//			*from - 
//			*to - 
//-----------------------------------------------------------------------------
void AddOptionalTransitionRule( ISpRecoGrammar* cpRecoGrammar, WORDRULETYPE *from, WORDRULETYPE *to )
{
	USES_CONVERSION;

	Assert( from );

	if ( from && !to )
	{
		Plat_DebugString( va( "Opt transition from %s to TERM\r\n", from->plaintext ) );
	}
	else
	{
		Plat_DebugString( va( "Opt transition from %s to %s\r\n", from->plaintext, to->plaintext ) );
	}

	HRESULT hr = cpRecoGrammar->AddWordTransition( from->hRule,
		to ? to->hRule : nullptr,
		nullptr,
		nullptr,
		SPWT_LEXICAL,
		CONFIDENCE_WEIGHT,
		nullptr );
	Assert( SUCCEEDED( hr ) );
}

#define MAX_WORD_SKIP 1
//-----------------------------------------------------------------------------
// Purpose: Links together all word rule states into a sentence rule CFG
// Input  : singleword - 
//			cpRecoGrammar - 
//			*root - 
//			*rules - 
//-----------------------------------------------------------------------------
bool BuildRules( ISpRecoGrammar* cpRecoGrammar, SPSTATEHANDLE *root, CUtlVector< WORDRULETYPE > *rules )
{
	intp numrules = (*rules).Count();
	WORDRULETYPE *rule = &(*rules)[0];

	// Add transition
	HRESULT hr = cpRecoGrammar->AddWordTransition( *root, rule->hRule, nullptr, nullptr, SPWT_LEXICAL, CONFIDENCE_WEIGHT, nullptr );
	Assert( !FAILED( hr ) );

	for ( intp i = 0; i < numrules; i++ )
	{
		rule = &(*rules)[ i ];
		
		WORDRULETYPE *next;
		if ( i < numrules - 1 )
		{
			next = &(*rules)[ i + 1 ];
		}
		else
		{
			next = nullptr;
		}

		AddWordTransitionRule( cpRecoGrammar, rule, next );
	}

	if ( numrules > 1 )
	{
		for ( intp skip = 1; skip <= min( (intp)MAX_WORD_SKIP, numrules ); skip++ )
		{
			Plat_DebugString( va( "Opt transition from Root to %s\r\n", (*rules)[ 0 ].plaintext ) );

			hr = cpRecoGrammar->AddWordTransition( *root, (*rules)[ 0 ].hRule, nullptr, nullptr, SPWT_LEXICAL, CONFIDENCE_WEIGHT, nullptr );

			// Now build rules where you can skip 1 to N intervening words
			for ( intp i = 1; i < numrules; i++ )
			{
				// Start at the beginning?
				rule = &(*rules)[ i ];

				WORDRULETYPE *next;
				if ( i < numrules - skip )
				{
					next = &(*rules)[ i + skip ];
				}
				else
				{
					continue;
				}

				// Add transition
				AddOptionalTransitionRule( cpRecoGrammar, rule, next );
			}

			// Go from final rule to end point
			AddOptionalTransitionRule( cpRecoGrammar, rule, nullptr );
		}
	}

	// Store it
	hr = cpRecoGrammar->Commit(0);
	return SUCCEEDED( hr );
}

//-----------------------------------------------------------------------------
// Purpose: Debugging, prints alternate list if one is created
// Input  : cpResult - 
//			(*pfnPrint - 
//-----------------------------------------------------------------------------
void PrintAlternates( ISpRecoResult* cpResult, void (*pfnPrint)( const char *fmt, ... ) )
{
	ISpPhraseAlt *rgPhraseAlt[ 32 ];
	BitwiseClear( rgPhraseAlt );

	ULONG ulCount;
	
	ISpPhrase *phrase = ( ISpPhrase * )cpResult;
	if ( phrase )
	{
		SPPHRASE *pElements;
		if ( SUCCEEDED( phrase->GetPhrase( &pElements ) ) )
		{
			if ( pElements->Rule.ulCountOfElements > 0 )
			{
				HRESULT hr = cpResult->GetAlternates(
					pElements->Rule.ulFirstElement,
					pElements->Rule.ulCountOfElements, 
					32,
					rgPhraseAlt,
					&ulCount);
				Assert( SUCCEEDED( hr ) );
				
				for ( ULONG r = 0 ; r < ulCount; r++ )
				{
					CSpDynamicString dstrText;
					hr = rgPhraseAlt[ r ]->GetText( (ULONG)SP_GETWHOLEPHRASE, (ULONG)SP_GETWHOLEPHRASE, TRUE, &dstrText, nullptr);
					Assert( !FAILED( hr ) );

					pfnPrint( "[ ALT ]" );
					pfnPrint( dstrText.CopyToChar() );
					pfnPrint( "\r\n" );
				}
			}
		}
		
		::CoTaskMemFree( pElements );
	}

	for ( auto *p : rgPhraseAlt )
	{
		p->Release();
		p = nullptr;
	}
}

void PrintWordsAndPhonemes( CSentence& sentence, void (*pfnPrint)( const char *fmt, ... ) )
{
	char sz[ 256 ];

	pfnPrint( "WORDS\r\n\r\n" );

	for ( auto *word : sentence.m_Words )
	{
		if ( !word )
			continue;

		V_sprintf_safe( sz, "<%u - %u> %s\r\n", 
			word->m_uiStartByte, word->m_uiEndByte, word->GetWord() );

		pfnPrint( sz );

		for ( auto *phoneme : word->m_Phonemes )
		{
			if ( !phoneme )
				continue;

			V_sprintf_safe( sz, "  <%u - %u> %s\r\n", 
				phoneme->m_uiStartByte, phoneme->m_uiEndByte, phoneme->GetTag() );

			pfnPrint( sz );
		}
	}

	pfnPrint( "\r\n" );
}

//-----------------------------------------------------------------------------
// Purpose: Given a wave file and a string of words "text", creates a CFG from the
//  sentence and stores the resulting words/phonemes in CSentence
// Input  : *wavname - 
//			text - 
//			sentence - 
//			(*pfnPrint - 
// Output : SR_RESULT
//-----------------------------------------------------------------------------
SR_RESULT ExtractPhonemes( const char *wavname, CSpDynamicString& text, CSentence& sentence, void (*pfnPrint)( const char *fmt, ...) )
{
	// Assume failure
	SR_RESULT result = SR_RESULT_ERROR;

	if ( text.Length() <= 0 )
	{
		pfnPrint( "Error:  no rule / text specified\n" );
		return result;
	}

	USES_CONVERSION;
	
	CUtlVector < WORDRULETYPE > wordRules;

	CComPtr<ISpStream> cpInputStream;
	CComPtr<ISpRecognizer> cpRecognizer;
	CComPtr<ISpRecoContext> cpRecoContext;
	CComPtr<ISpRecoGrammar> cpRecoGrammar;
	CComPtr<ISpPhoneConverter>  cpPhoneConv;
 
	// Create basic SAPI stream object
	// NOTE: The helper SpBindToFile can be used to perform the following operations
	HRESULT hr = cpInputStream.CoCreateInstance(CLSID_SpStream);
	if ( FAILED( hr ) )
	{
		pfnPrint( "Error:  SAPI 0x%x Stream object not installed?\n", _SAPI_VER );
		return result;
	}

	CSpStreamFormat sInputFormat;
	
	// setup stream object with wav file MY_WAVE_AUDIO_FILENAME
	//   for read-only access, since it will only be access by the SR engine
	hr = cpInputStream->BindToFile(
		T2W(wavname),
		SPFM_OPEN_READONLY,
		nullptr,
		sInputFormat.WaveFormatExPtr(),
		SPFEI_ALL_EVENTS );

	if ( FAILED( hr ) )
	{
		pfnPrint( "Error: couldn't open wav file %s\n", wavname );
		return result;
	}
	
	// Create in-process speech recognition engine
	hr = cpRecognizer.CoCreateInstance(CLSID_SpInprocRecognizer);
	if ( FAILED( hr ) )
	{
		pfnPrint( "Error:  SAPI 0x%x In process recognizer object not installed?\n", _SAPI_VER );
		return result;
	}

	// Create recognition context to receive events
	hr = cpRecognizer->CreateRecoContext(&cpRecoContext);
	if ( FAILED( hr ) )
	{
		pfnPrint( "Error:  SAPI 0x%x Unable to create recognizer context\n", _SAPI_VER );
		return result;
	}
	
	// Create a grammar
	hr = cpRecoContext->CreateGrammar( EP_GRAM_ID, &cpRecoGrammar );
	if ( FAILED( hr ) )
	{
		pfnPrint( "Error:  SAPI 0x%x Unable to create recognizer grammar\n", _SAPI_VER );
		return result;
	}

	constexpr LANGID englishID = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);

	bool userSpecified = false;
	LANGID langID = SpGetUserDefaultUILanguage();

	// Allow commandline override
	if ( CommandLine()->FindParm( "-languageid" ) != 0 )
	{
		userSpecified = true;
		langID = static_cast<LANGID>( CommandLine()->ParmValue( "-languageid", langID ) );
	}

	// Create a phoneme converter ( so we can convert to IPA codes )
	hr = SpCreatePhoneConverter( langID, nullptr, nullptr, &cpPhoneConv );
	if ( FAILED( hr ) )
	{
		if ( langID != englishID )
		{
			if ( userSpecified )
			{
				pfnPrint( "Warning:  SAPI 0x%x Unable to create phoneme converter for command line override -languageid %hu\n", langID, _SAPI_VER );
			}
			else
			{
				pfnPrint( "Warning:  SAPI 0x%x Unable to create phoneme converter for default UI language %hu\n", langID, _SAPI_VER );
			}

			// Try english!!!
			langID = englishID;
			hr = SpCreatePhoneConverter( langID, nullptr, nullptr, &cpPhoneConv );
		}

		if ( FAILED( hr ) )
		{
			pfnPrint( "Error:  SAPI 0x%x Unable to create phoneme converter for English (USA) language id %hu\n", langID, _SAPI_VER );
			return result;
		}
		else
		{
			pfnPrint( "Note:  SAPI 0x%x Falling back to use English USA) -languageid %hu\n", langID, _SAPI_VER );
		}
	}
	else if ( userSpecified )
	{
		pfnPrint( "Note:  SAPI 0x%x Using user specified -languageid %hu\n", langID, _SAPI_VER );
	}

	SPSTATEHANDLE hStateRoot;
	// create/re-create Root level rule of grammar
	hr = cpRecoGrammar->GetRule(L"Root", 0, SPRAF_TopLevel | SPRAF_Active, TRUE, &hStateRoot);
	if ( FAILED( hr ) )
	{
		pfnPrint( "Error:  SAPI 0x%x Unable to create root rule\n", _SAPI_VER );
		return result;
	}

	// Inactivate it so we can alter it
	hr = cpRecoGrammar->SetRuleState( nullptr, nullptr, SPRS_INACTIVE );
	if ( FAILED( hr ) )
	{
		pfnPrint( "Error:  SAPI 0x%x Unable to deactivate grammar rules\n", _SAPI_VER );
		return result;
	}

	// Create the rule set from the words in text
	{
		CSpDynamicString currentWord;
		WCHAR *pos = ( WCHAR * )text;
		WCHAR str[ 2 ];
		str[1]= 0;

		while ( *pos )
		{
			if ( *pos == L' ' /*|| *pos == L'.' || *pos == L'-'*/ )
			{
				// Add word to rule set
				if ( currentWord.Length() > 0 )
				{
					AddWordRule( cpRecoGrammar, &hStateRoot, &wordRules, currentWord );
					currentWord.Clear();
				}
				pos++;
				continue;
			}

			// Skip anything that's inside a [ xxx ] pair.
			if ( *pos == L'[' )
			{
				while ( *pos && *pos != L']' )
				{
					pos++;
				}

				if ( *pos )
				{
					pos++;
				}
				continue;
			}

			str[ 0 ] = *pos;

			currentWord.Append( str );
			pos++;
		}

		if ( currentWord.Length() > 0 )
		{
			AddWordRule( cpRecoGrammar, &hStateRoot, &wordRules, currentWord );
		}

		if ( wordRules.Count() <= 0 )
		{
			pfnPrint( "Error:  Text %S contained no usable words\n", static_cast<wchar_t*>( text ) );
			return result;
		}

		// Build all word to word transitions in the grammar
		if ( !BuildRules( cpRecoGrammar, &hStateRoot, &wordRules ) )
		{
			pfnPrint( "Error:  Rule set for %S could not be generated\n", static_cast<wchar_t*>( text ) );
			return result;
		}
	}

	// check for recognitions and end of stream event
	const ULONGLONG ullInterest = 
		SPFEI(SPEI_RECOGNITION) | SPFEI(SPEI_END_SR_STREAM) | SPFEI(SPEI_FALSE_RECOGNITION) | 
		SPFEI(SPEI_PHRASE_START ) | SPFEI(SPEI_HYPOTHESIS ) | SPFEI(SPEI_INTERFERENCE) ;
	hr = cpRecoContext->SetInterest( ullInterest, ullInterest );
	if ( FAILED( hr ) )
	{
		pfnPrint( "Error:  SAPI 0x%x Unable to set interest level\n", _SAPI_VER );
		return result;
	}	
	// use Win32 events for command-line style application
	hr = cpRecoContext->SetNotifyWin32Event();
	if ( FAILED( hr ) )
	{
		pfnPrint( "Error:  SAPI 0x%x Unable to set win32 notify event\n", _SAPI_VER );
		return result;
	}
	// connect wav input to recognizer
	// SAPI will negotiate mismatched engine/input audio formats using system audio codecs, so second parameter is not important - use default of TRUE
	hr = cpRecognizer->SetInput(cpInputStream, TRUE);
	if ( FAILED( hr ) )
	{
		pfnPrint( "Error:  SAPI 0x%x Unable to associate input stream\n", _SAPI_VER );
		return result;
	}	

	// Activate the CFG ( rather than using dictation )
	hr = cpRecoGrammar->SetRuleState( nullptr, nullptr, SPRS_ACTIVE );
	if ( FAILED( hr ) )
	{
		switch ( hr )
		{
		case E_INVALIDARG:
			pfnPrint( "pszName is invalid or bad. Alternatively, pReserved is non-nullptr\n" );
			break;
		case SP_STREAM_UNINITIALIZED:
			pfnPrint( "ISpRecognizer::SetInput has not been called with the InProc recognizer\n" );
			break;
		case SPERR_UNINITIALIZED:
			pfnPrint( "The object has not been properly initialized.\n");
			break;
		case SPERR_UNSUPPORTED_FORMAT:
			pfnPrint( "Audio format is bad or is not recognized. Alternatively, the device driver may be busy by another application and cannot be accessed.\n" );
			break;
		case SPERR_NOT_TOPLEVEL_RULE:
			pfnPrint( "The rule pszName exists, but is not a top-level rule.\n" );
			break;
		default:
			pfnPrint( "Unknown error\n" );
			break;
		}
		pfnPrint( "Error:  SAPI 0x%x Unable to activate rule set\n", _SAPI_VER );
		return result;
	}

	// while events occur, continue processing
	// timeout should be greater than the audio stream length, or a reasonable amount of time expected to pass before no more recognitions are expected in an audio stream
	BOOL fEndStreamReached = FALSE;
	while (!fEndStreamReached && S_OK == cpRecoContext->WaitForNotifyEvent( SR_WAVTIMEOUT ))
	{
		CSpEvent spEvent;
		// pull all queued events from the reco context's event queue
		
		while (!fEndStreamReached && S_OK == spEvent.GetFrom(cpRecoContext))
		{
			// Check event type
			switch (spEvent.eEventId)
			{
			case SPEI_INTERFERENCE:
				{
					SPINTERFERENCE interference = spEvent.Interference();

					switch ( interference )
					{
					case SPINTERFERENCE_NONE:
						pfnPrint( "[ I None ]\r\n" );
						break;
					case SPINTERFERENCE_NOISE:
						pfnPrint( "[ I Noise ]\r\n" );
						break;
					case SPINTERFERENCE_NOSIGNAL:
						pfnPrint( "[ I No Signal ]\r\n" );
						break;
					case SPINTERFERENCE_TOOLOUD:
						pfnPrint( "[ I Too Loud ]\r\n" );
						break;
					case SPINTERFERENCE_TOOQUIET:
						pfnPrint( "[ I Too Quiet ]\r\n" );
						break;
					case SPINTERFERENCE_TOOFAST:
						pfnPrint( "[ I Too Fast ]\r\n" );
						break;
					case SPINTERFERENCE_TOOSLOW:
						pfnPrint( "[ I Too Slow ]\r\n" );
						break;
					default:
						break;
					}
				}
				break;
			case SPEI_PHRASE_START:
				pfnPrint( "Phrase Start\r\n" );
				sentence.MarkNewPhraseBase();
				break;

			case SPEI_HYPOTHESIS:
			case SPEI_RECOGNITION:
			case SPEI_FALSE_RECOGNITION:
				{
                    CComPtr<ISpRecoResult> cpResult;
                    cpResult = spEvent.RecoResult();

                    CSpDynamicString dstrText;
                    if (spEvent.eEventId == SPEI_FALSE_RECOGNITION)
                    {
                        dstrText = L"(Unrecognized)";

						result = SR_RESULT_FAILED;

						// It's possible that the failed recog might have more words, so see if that's the case
						EnumeratePhonemes( cpPhoneConv, cpResult, sentence );
					}
                    else
                    {
						// Hypothesis or recognition success
                        cpResult->GetText( (ULONG)SP_GETWHOLEPHRASE, (ULONG)SP_GETWHOLEPHRASE, TRUE, &dstrText, nullptr);

						EnumeratePhonemes( cpPhoneConv, cpResult, sentence );

						if ( spEvent.eEventId == SPEI_RECOGNITION )
						{
							result = SR_RESULT_SUCCESS;
						}

						pfnPrint( va( "%s%s\r\n", spEvent.eEventId == SPEI_HYPOTHESIS ? "[ Hypothesis ] " : "", dstrText.CopyToChar() ) );
					}
                    
                    cpResult.Release();
				}
				break;
				// end of the wav file was reached by the speech recognition engine
            case SPEI_END_SR_STREAM:
				fEndStreamReached = TRUE;
				break;
			}
			
			// clear any event data/object references
			spEvent.Clear();
		}// END event pulling loop - break on empty event queue OR end stream
	}// END event polling loop - break on event timeout OR end stream
	
	// Deactivate rule
	hr = cpRecoGrammar->SetRuleState( nullptr, nullptr, SPRS_INACTIVE );
	if ( FAILED( hr ) )
	{
		pfnPrint( "Error:  SAPI 0x%x Unable to deactivate rule set\n", _SAPI_VER );
		return result;
	}

	// close the input stream, since we're done with it
	// NOTE: smart pointer will call SpStream's destructor, and consequently ::Close, but code may want to check for errors on ::Close operation
	hr = cpInputStream->Close();
	if ( FAILED( hr ) )
	{
		pfnPrint( "Error:  SAPI 0x%x Unable to close input stream\n", _SAPI_VER );
		return result;
	}

	return result;
}

//-----------------------------------------------------------------------------
// Purpose: HACK HACK:  We have to delete the RecoContext key or sapi starts to train
//  itself on each iteration which was causing some problems.
// Input  : hKey - 
//-----------------------------------------------------------------------------
void RecursiveRegDelKey(HKEY hKey)
{
	char keyname[256]={0};
	DWORD namesize=256;

	//base case: no subkeys when RegEnumKeyEx returns error on index 0
	LONG lResult=RegEnumKeyEx(hKey,0,keyname,&namesize,nullptr,nullptr,nullptr,nullptr);
	if (lResult!=ERROR_SUCCESS)
	{
		return;
	}

	do
	{
		HKEY subkey;
		LONG lResult2=RegOpenKeyEx(hKey,keyname,0,KEY_ALL_ACCESS,&subkey);
		
		if (lResult2==ERROR_SUCCESS)
		{
			RecursiveRegDelKey(subkey);
			RegCloseKey(subkey);

			LONG lDelResult=RegDeleteKey(hKey,keyname);
			Assert( lDelResult == ERROR_SUCCESS );

			namesize=256;
			//use 0 in the next function call because when you delete one, the rest shift down!
			lResult=RegEnumKeyEx(hKey,0,keyname,&namesize,nullptr,nullptr,nullptr,nullptr);
		}
		else 
		{
			break;
		}
	} while (lResult!=ERROR_NO_MORE_ITEMS);
}

bool IsUseable( CWordTag *word )
{
	if ( word->m_uiStartByte || word->m_uiEndByte )
		return true;

	return false;
}

intp FindLastUsableWord( CSentence& outwords )
{
	intp numwords = outwords.m_Words.Count();
	if ( numwords < 1 )
	{
		Assert( 0 );
		return -1;
	}

	for ( intp i = numwords - 1; i >= 0; i-- )
	{
		CWordTag *check = outwords.m_Words[ i ];
		if ( IsUseable( check ) )
		{
			return i;
		}
	}

	return -1;
}


intp FindFirstUsableWord( CSentence& outwords )
{
	intp numwords = outwords.m_Words.Count();
	if ( numwords < 1 )
	{
		Assert( 0 );
		return -1;
	}

	intp i = 0;
	for ( auto *word : outwords.m_Words )
	{
		if ( IsUseable( word ) )
		{
			return i;
		}

		++i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Counts words which have either a valid start or end byte
// Input  : *outwords - 
// Output : int
//-----------------------------------------------------------------------------
intp CountUsableWords( CSentence& outwords )
{
	intp count = 0;

	for ( auto *word : outwords.m_Words )
	{
		if ( !IsUseable( word ) )
			continue;

		count++;
	}

	return count;
}


//-----------------------------------------------------------------------------
// Purpose: Counts words which have either a valid start or end byte
// Input  : *outwords - 
// Output : int
//-----------------------------------------------------------------------------
intp CountUnuseableWords( CSentence& outwords )
{
	intp count = 0;

	for ( auto *word : outwords.m_Words )
	{
		if ( IsUseable( word ) )
			continue;

		count++;
	}

	return count;
}

// Keeps same relative spacing, but rebases list
void RepartitionPhonemes( CWordTag *word, unsigned oldStart, unsigned oldEnd )
{
	// Repartition phonemes based on old range
	float oldRange = ( float )( oldEnd - oldStart );
	float newRange = ( float )( word->m_uiEndByte - word->m_uiStartByte );

	for ( auto *tag : word->m_Phonemes )
	{
		Assert( tag );

		float frac1 = 0.0f, frac2 = 0.0f;
		
		float delta1 = ( float ) ( tag->m_uiStartByte - oldStart );
		float delta2 = ( float ) ( tag->m_uiEndByte - oldStart );
		if ( oldRange > 0.0f )
		{
			frac1 = delta1 / oldRange;
			frac2 = delta2 / oldRange;
		}

		tag->m_uiStartByte = word->m_uiStartByte + ( unsigned ) ( frac1 * newRange );
		tag->m_uiEndByte = word->m_uiStartByte +  ( unsigned ) ( frac2 * newRange );
	}
}

void PartitionWords( CSentence& outwords, intp start, intp end, int sampleStart, int sampleEnd )
{
	intp wordCount = end - start + 1;
	Assert( wordCount >= 1 );
	int stepSize  = ( sampleEnd - sampleStart ) / wordCount;

	int currentStart = sampleStart;

	for ( intp i = start; i <= end; i++ )
	{
		CWordTag *word = outwords.m_Words[ i ];
		Assert( word );

		unsigned oldStart = word->m_uiStartByte;
		unsigned oldEnd = word->m_uiEndByte;

		word->m_uiStartByte = currentStart;
		word->m_uiEndByte = currentStart + stepSize;

		RepartitionPhonemes( word, oldStart, oldEnd );

		currentStart += stepSize;
	}
}

void MergeWords( CWordTag *w1, CWordTag *w2 )
{
	const unsigned start = min( w1->m_uiStartByte, w2->m_uiStartByte );
	const unsigned end = max( w1->m_uiEndByte, w2->m_uiEndByte );

	const unsigned mid = ( start + end ) / 2;

	const unsigned oldw1start = w1->m_uiStartByte;
	const unsigned oldw2start = w2->m_uiStartByte;
	const unsigned oldw1end = w1->m_uiEndByte;
	const unsigned oldw2end = w2->m_uiEndByte;

	w1->m_uiStartByte = start;
	w1->m_uiEndByte = mid;
	w2->m_uiStartByte = mid;
	w2->m_uiEndByte = end;

	RepartitionPhonemes( w1, oldw1start, oldw1end );
	RepartitionPhonemes( w2, oldw2start, oldw2end );
}

void FixupZeroLengthWords( CSentence& outwords )
{
	while ( 1 )
	{
		intp i;
		for ( i = 0 ; i < outwords.m_Words.Count() - 1; i++ )
		{
			CWordTag *current = outwords.m_Words[ i ];
			CWordTag *next = outwords.m_Words[ i + 1 ];

			if ( current->m_uiEndByte - current->m_uiStartByte <= 0 )
			{
				MergeWords( current, next );
				break;
			}

			if ( next->m_uiEndByte - next->m_uiStartByte <= 0 )
			{
				MergeWords( current, next );
				break;
			}
		}

		if ( i >= outwords.m_Words.Count() - 1 )
		{
			break;
		}
	}
}

void ComputeMissingByteSpans( int numsamples, CSentence& outwords )
{
	intp numwords = outwords.m_Words.Count();
	// Nothing to do
	if ( numwords <= 0 )
		return;

	intp interationcount = 1;

	while( 1 )
	{
		Log( "\nCompute %zd\n", interationcount++ );
		LogWords( outwords );

		// Done!
		if ( !CountUnuseableWords( outwords ) )
		{
			FixupZeroLengthWords( outwords );
			break;
		}

		if ( !CountUsableWords( outwords ) )
		{
			// Evenly space words across full sample time
			PartitionWords( outwords, 0, numwords - 1, 0, numsamples );
			break;
		}

		intp wordNumber = FindFirstUsableWord( outwords );
		// Not the first word
		if ( wordNumber > 0 )
		{
			// Repartition all of the unusables and the first one starting at zero over the range
			CWordTag *firstUsable = outwords.m_Words[ wordNumber ];
			Assert( firstUsable );

			if ( firstUsable->m_uiStartByte != 0 )
			{
				PartitionWords( outwords, 0, wordNumber - 1, 0, firstUsable->m_uiStartByte );
			}
			else
			{
				PartitionWords( outwords, 0, wordNumber, 0, firstUsable->m_uiEndByte );
			}

			// Start over
			continue;
		}

		wordNumber = FindLastUsableWord( outwords );
		// Not the last word
		if ( wordNumber >= 0 && wordNumber < numwords - 1 )
		{
			// Repartition all of the unusables and the first one starting at zero over the range
			CWordTag *lastUsable = outwords.m_Words[ wordNumber ];
			Assert( lastUsable );

			if ( lastUsable->m_uiEndByte != (unsigned)numsamples )
			{
				PartitionWords( outwords, wordNumber + 1, numwords-1, lastUsable->m_uiEndByte, numsamples );
			}
			else
			{
				PartitionWords( outwords, wordNumber, numwords-1, lastUsable->m_uiStartByte, numsamples );
			}

			// Start over
			continue;
		}

		// If we get here it means that the start and end of the list are okay and we just have to
		//  iterate across the list and fix things in the middle
		unsigned startByte = 0;
		unsigned endByte = 0;
		for ( intp i = 0; i < numwords ; i++ )
		{
			CWordTag *word = outwords.m_Words[ i ];
			if ( IsUseable( word ) )
			{
				startByte = word->m_uiEndByte;
				continue;
			}

			// Found the start of a chain of 1 or more unusable words
			// Find the startbyte of the next usable word and count how many words we check
			intp wordCount = 1;
			for ( intp j = i + 1; j < numwords; j++ )
			{
				CWordTag *next = outwords.m_Words[ j ];
				if ( IsUseable( next ) )
				{
					endByte = next->m_uiStartByte;
					break;
				}

				wordCount++;
			}

			// Now partition words across the gap and go to start again
			PartitionWords( outwords, i, i + wordCount - 1, startByte, endByte );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Given a wavfile and a list of inwords, determines the word/phonene 
//  sample counts for the sentce
// Input  : *wavfile - 
//			*inwords - 
//			*outphonemes{	text.Clear( - 
// Output : SR_RESULT
//-----------------------------------------------------------------------------
static SR_RESULT SAPI_ExtractPhonemes( 
	const char *wavfile,
	int numsamples,
	void (*pfnPrint)( PRINTF_FORMAT_STRING const char *fmt, ... ),
	CSentence& inwords,
	CSentence& outwords )
{
	LogReset();

	USES_CONVERSION;

	CSpDynamicString text;
	text.Clear();

	HKEY hkwipe;
	LONG lResult = RegOpenKeyEx( HKEY_CURRENT_USER,
		"Software\\Microsoft\\Speech\\RecoProfiles",
		0,
		KEY_ALL_ACCESS,
		&hkwipe );
	if ( lResult == ERROR_SUCCESS )
	{
		RecursiveRegDelKey( hkwipe );
		RegCloseKey( hkwipe );
	}

	if ( Q_isempty( inwords.GetText() ) )
	{
		inwords.SetTextFromWords();
	}

	// Construct a string from the inwords array
	text.Append( T2W( inwords.GetText() ) );

	// Assume failure
	SR_RESULT result = SR_RESULT_ERROR;

	if ( text.Length() > 0 )
	{
		CSentence sentence;

		pfnPrint( "Processing...\r\n" );

		// Give it a try
		result = ExtractPhonemes( wavfile, text, sentence, pfnPrint );

		pfnPrint( "Finished.\r\n" );
		// PrintWordsAndPhonemes( sentence, pfnPrint );

		// Copy results to outputs
		outwords.Reset();
		outwords.SetText( inwords.GetText() );
		
		Log( "Starting\n" );
		LogWords( inwords );

		if ( SR_RESULT_ERROR != result )
		{
			Log( "Hypothesized\n" );
			LogWords( sentence );

			for( auto *tag : sentence.m_Words )
			{
				if ( tag )
				{
					// Skip '...' tag
					if ( stricmp( tag->GetWord(), "..." ) )
					{
						outwords.m_Words.AddToTail( new CWordTag( *tag ) );
					}
				}
			}

			// Now insert unrecognized/skipped words from original list
			//
			intp frompos = 0, topos = 0;

			while( 1 )
			{
				// End of source list
				if ( frompos >= inwords.m_Words.Count() )
					break;

				const CWordTag *fromTag = inwords.m_Words[ frompos ];

				// Reached end of destination list, just copy words over from from source list until
				//  we run out of source words
				if ( topos >= outwords.m_Words.Count() )
				{
					// Just copy words over
					auto *newWord = new CWordTag( *fromTag );

					// Remove phonemes
					while ( newWord->m_Phonemes.Count() > 0 )
					{
						CPhonemeTag *kill = newWord->m_Phonemes[ 0 ];
						newWord->m_Phonemes.Remove( 0 );
						delete kill;
					}

					outwords.m_Words.AddToTail( newWord );
					frompos++;
					topos++;
					continue;
				}

				// Destination word
				const CWordTag *toTag = outwords.m_Words[ topos ];

				// Words match, just skip ahead
				if ( !stricmp( fromTag->GetWord(), toTag->GetWord() ) )
				{
					frompos++;
					topos++;
					continue;
				}

				// The only case we handle is that something in the source wasn't in the destination

				// Find the next source word that appears in the destination
				intp skipAhead = frompos + 1;
				bool found = false;
				while ( skipAhead < inwords.m_Words.Count() )
				{
					const CWordTag *sourceWord = inwords.m_Words[ skipAhead ];
					if ( !stricmp( sourceWord->GetWord(), toTag->GetWord() ) )
					{
						found = true;
						break;
					}

					skipAhead++;
				}

				// Uh oh destination has words that are not in source, just skip to next destination word?
				if ( !found )
				{
					topos++;
				}
				else
				{
					// Copy words from from source list into destination
					// 
					intp skipCount = skipAhead - frompos;

					while ( --skipCount>= 0 )
					{
						const CWordTag *sourceWord = inwords.m_Words[ frompos++ ];
						auto *newWord = new CWordTag( *sourceWord );

						// Remove phonemes
						while ( newWord->m_Phonemes.Count() > 0 )
						{
							CPhonemeTag *kill = newWord->m_Phonemes[ 0 ];
							newWord->m_Phonemes.Remove( 0 );
							delete kill;
						}

						outwords.m_Words.InsertBefore( topos, newWord );
						topos++;
					}

					frompos++;
					topos++;
				}
			}

			Log( "\nDone simple check\n" );

			LogWords( outwords );
			LogPhonemes( outwords );

			ComputeMissingByteSpans( numsamples, outwords );

			Log( "\nFinal check\n" );

			LogWords( outwords );
			LogPhonemes( outwords );
		}
	}
	else
	{
		pfnPrint( "Input sentence is empty!\n" );
	}

	// Return results
	return result;
}


//-----------------------------------------------------------------------------
// Purpose: Expose the interface
//-----------------------------------------------------------------------------
class CPhonemeExtractorSAPI final : public IPhonemeExtractor
{
public:
	PE_APITYPE GetAPIType() const override
	{
		return SPEECH_API_SAPI;
	}

	// Used for menus, etc
	char const *GetName() const override
	{
		static char version[16] = { '\0' };
		if ( Q_isempty( version ) )
		{
			V_sprintf_safe( version, "MS SAPI %x.%x", _SAPI_VER / 0x10, _SAPI_VER % 0x10 );
		}

		return version;
	}

	SR_RESULT Extract( 
		const char *wavfile,
		int numsamples,
		void (*pfnPrint)( PRINTF_FORMAT_STRING const char *fmt, ... ),
		CSentence& inwords,
		CSentence& outwords ) override
	{
		return SAPI_ExtractPhonemes( wavfile, numsamples, pfnPrint, inwords, outwords );
	}
};

EXPOSE_SINGLE_INTERFACE( CPhonemeExtractorSAPI, IPhonemeExtractor, VPHONEME_EXTRACTOR_INTERFACE );