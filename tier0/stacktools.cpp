// Copyright 1996-2005, Valve Corporation, All rights reserved.

#include "pch_tier0.h"
#include "tier0/stacktools.h"
#include "tier0/threadtools.h"
#include "tier0/icommandline.h"

#include "tier0/valve_off.h"

#if defined( PLATFORM_WINDOWS_PC )
#include "winlite.h"

#include <DbgHelp.h>
#endif

#if defined( LINUX )
#include <execinfo.h>
#endif

#include "tier0/valve_on.h"

#include "tier0/memdbgon.h"


#if !defined( ENABLE_RUNTIME_STACK_TRANSLATION ) //disable the whole toolset

#if defined( LINUX )

int GetCallStack( void **pReturnAddressesOut, int iArrayCount, int iSkipCount )
{
	return backtrace( pReturnAddressesOut, iArrayCount );
}

int GetCallStack_Fast( void **pReturnAddressesOut, int iArrayCount, int iSkipCount )
{
	return backtrace( pReturnAddressesOut, iArrayCount );
}

#else

int GetCallStack( void **pReturnAddressesOut, int iArrayCount, int iSkipCount )
{
	return 0;
}

int GetCallStack_Fast( void **pReturnAddressesOut, int iArrayCount, int iSkipCount )
{
	return 0;
}

#endif

//where we'll find our PDB's for win32. Translation will not work until this has been called once (even if with NULL)
void SetStackTranslationSymbolSearchPath( const char *szSemicolonSeparatedList )
{
}

void StackToolsNotify_LoadedLibrary( const char *szLibName )
{
}

int TranslateStackInfo( const void * const *pCallStack, int iCallStackCount, tchar *szOutput, int iOutBufferSize, const tchar *szEntrySeparator, TranslateStackInfo_StyleFlags_t style )
{
	if( iOutBufferSize > 0 )
		*szOutput = '\0';

	return 0;
}

void PreloadStackInformation( const void **pAddresses, int iAddressCount )
{
}

bool GetFileAndLineFromAddress( const void *pAddress, tchar *pFileNameOut, int iMaxFileNameLength, uint32 &iLineNumberOut, uint32 *pDisplacementOut )
{
	if( iMaxFileNameLength > 0 )
		*pFileNameOut = '\0';

	return false;
}

bool GetSymbolNameFromAddress( const void *pAddress, tchar *pSymbolNameOut, int iMaxSymbolNameLength, uint64 *pDisplacementOut )
{
	if( iMaxSymbolNameLength > 0 )
		*pSymbolNameOut = '\0';

	return false;
}

bool GetModuleNameFromAddress( const void *pAddress, tchar *pModuleNameOut, int iMaxModuleNameLength )
{
	if( iMaxModuleNameLength > 0 )
		*pModuleNameOut = '\0';

	return false;
}

#else //#if !defined( ENABLE_RUNTIME_STACK_TRANSLATION )

//===============================================================================================================
// Shared Windows/X360 code
//===============================================================================================================

CTHREADLOCALPTR( CStackTop_Base ) g_StackTop;

class CStackTop_FriendFuncs : public CStackTop_Base
{
public:
	friend int AppendParentStackTrace( void **pReturnAddressesOut, int iArrayCount, int iAlreadyFilled );
	friend int GetCallStack_Fast( void **pReturnAddressesOut, int iArrayCount, int iSkipCount );
};


inline int AppendParentStackTrace( void **pReturnAddressesOut, int iArrayCount, int iAlreadyFilled )
{
	CStackTop_FriendFuncs *pTop = (CStackTop_FriendFuncs *)(CStackTop_Base *)g_StackTop;
	if( pTop != NULL )
	{
		if( pTop->m_pReplaceAddress != NULL )
		{
			for( int i = iAlreadyFilled; --i >= 0; )
			{
				if( pReturnAddressesOut[i] == pTop->m_pReplaceAddress )
				{
					iAlreadyFilled = i;
					break;
				}
			}
		}

		if( pTop->m_iParentStackTraceLength != 0 )
		{
			int iCopy = MIN( iArrayCount - iAlreadyFilled, pTop->m_iParentStackTraceLength );
			memcpy( pReturnAddressesOut + iAlreadyFilled, pTop->m_pParentStackTrace, iCopy * sizeof( void * ) );
			iAlreadyFilled += iCopy;
		}
	}

	return iAlreadyFilled;
}

inline bool ValidStackAddress( void *pAddress, const void *pNoLessThan, const void *pNoGreaterThan )
{
	if( (uintp)pAddress & 3 )
		return false;
	if( pAddress < pNoLessThan ) //frame pointer traversal should always increase the pointer
		return false;
	if( pAddress > pNoGreaterThan ) //never traverse outside the stack (Oh 0xCCCCCCCC, how I hate you)
		return false;

#if defined( WIN32 ) && !defined( _X360 ) && 1
	if( IsBadReadPtr( pAddress, (sizeof( void * ) * 2) ) ) //safety net, but also throws an exception (handled internally) to stop bad access
		return false;
#endif

	return true;
}

#pragma auto_inline( off )
int GetCallStack_Fast( void **pReturnAddressesOut, int iArrayCount, int iSkipCount )
{
	//Only tested in windows. This function won't work with frame pointer omission enabled. "vpc /nofpo" all projects
#if (defined( TIER0_FPO_DISABLED ) || defined( _DEBUG )) &&\
	(defined( WIN32 ) && !defined( _X360 ) && !defined(_M_X64))
	void *pStackCrawlEBP;
	__asm
	{
		mov [pStackCrawlEBP], ebp;
	}

	/*
	With frame pointer omission disabled, this should be the pattern all the way up the stack
	[ebp+00]   Old ebp value
	[ebp+04]   Return address
	*/

	void *pNoLessThan = pStackCrawlEBP; //impossible for a valid stack to traverse before this address
	int i;

	CStackTop_FriendFuncs *pTop = (CStackTop_FriendFuncs *)(CStackTop_Base *)g_StackTop;
	if( pTop != NULL ) //we can do fewer error checks if we have a valid reference point for the top of the stack
	{		
		void *pNoGreaterThan = pTop->m_pStackBase;

		//skips
		for( i = 0; i != iSkipCount; ++i )
		{
			if( (pStackCrawlEBP < pNoLessThan) || (pStackCrawlEBP > pNoGreaterThan) )
				return AppendParentStackTrace( pReturnAddressesOut, iArrayCount, 0 );

			pNoLessThan = pStackCrawlEBP;
			pStackCrawlEBP = *(void **)pStackCrawlEBP; //should be pointing at old ebp value
		}

		//store
		for( i = 0; i != iArrayCount; ++i )
		{
			if( (pStackCrawlEBP < pNoLessThan) || (pStackCrawlEBP > pNoGreaterThan) )
				break;

			pReturnAddressesOut[i] = *((void **)pStackCrawlEBP + 1);

			pNoLessThan = pStackCrawlEBP;
			pStackCrawlEBP = *(void **)pStackCrawlEBP; //should be pointing at old ebp value
		}

		return AppendParentStackTrace( pReturnAddressesOut, iArrayCount, i );
	}
	else
	{
		void *pNoGreaterThan = ((unsigned char *)pNoLessThan) + (1024 * 1024); //standard stack is 1MB. TODO: Get actual stack end address if available since this check isn't foolproof	

		//skips
		for( i = 0; i != iSkipCount; ++i )
		{
			if( !ValidStackAddress( pStackCrawlEBP, pNoLessThan, pNoGreaterThan ) )
				return AppendParentStackTrace( pReturnAddressesOut, iArrayCount, 0 );

			pNoLessThan = pStackCrawlEBP;
			pStackCrawlEBP = *(void **)pStackCrawlEBP; //should be pointing at old ebp value
		}

		//store
		for( i = 0; i != iArrayCount; ++i )
		{
			if( !ValidStackAddress( pStackCrawlEBP, pNoLessThan, pNoGreaterThan ) )
				break;

			pReturnAddressesOut[i] = *((void **)pStackCrawlEBP + 1);

			pNoLessThan = pStackCrawlEBP;
			pStackCrawlEBP = *(void **)pStackCrawlEBP; //should be pointing at old ebp value
		}

		return AppendParentStackTrace( pReturnAddressesOut, iArrayCount, i );
	}
#else

	return 0;

#endif
}
#pragma auto_inline( on )






#if defined( WIN32 ) && !defined( _X360 )
//===============================================================================================================
// Windows version of the toolset
//===============================================================================================================


#if defined( TIER0_FPO_DISABLED )
//#	define USE_CAPTURESTACKBACKTRACE //faster than StackWalk64, but only works on XP or newer and only with Frame Pointer Omission optimization disabled(/Oy-) for every function it traces through
#endif


#if defined(_M_IX86) || defined(_M_X64)
#	define USE_STACKWALK64
#	if defined(_M_IX86)
#		define STACKWALK64_MACHINETYPE IMAGE_FILE_MACHINE_I386
#	else
#		define STACKWALK64_MACHINETYPE IMAGE_FILE_MACHINE_AMD64
#	endif
#endif
using PFN_SymGetOptions = decltype(&SymGetOptions);
using PFN_SymSetOptions = decltype(&SymSetOptions);
using PFN_SymSetSearchPath = decltype(&SymSetSearchPath);
using PFN_SymInitialize = decltype(&SymInitialize);
using PFN_SymCleanup = decltype(&SymCleanup);
using PFN_SymEnumerateModules64 = decltype(&SymEnumerateModules64);
using PFN_EnumerateLoadedModules64 = decltype(&EnumerateLoadedModules64);
using PFN_SymLoadModule64 = decltype(&SymLoadModule64);
using PFN_SymUnloadModule64 = decltype(&SymUnloadModule64);
using PFN_SymFromAddr = decltype(&SymFromAddr);
using PFN_SymGetLineFromAddr64 = decltype(&SymGetLineFromAddr64);
using PFN_SymGetModuleInfo64 = decltype(&SymGetModuleInfo64);
using PFN_StackWalk64 = decltype(&StackWalk64);
using PFN_CaptureStackBackTrace = decltype(&RtlCaptureStackBackTrace);


DWORD WINAPI SymGetOptions_DummyFn( VOID )
{
	return 0;
}

DWORD WINAPI SymSetOptions_DummyFn( IN DWORD )
{
	return 0;
}

BOOL WINAPI SymSetSearchPath_DummyFn( IN HANDLE, IN PCSTR )
{
	return FALSE;
}

BOOL WINAPI SymInitialize_DummyFn( IN HANDLE, IN PCSTR, IN BOOL )
{
	return FALSE;
}

BOOL WINAPI SymCleanup_DummyFn( IN HANDLE )
{
	return TRUE;
}

BOOL WINAPI SymEnumerateModules64_DummyFn( IN HANDLE, IN PSYM_ENUMMODULES_CALLBACK64, IN PVOID )
{
	return FALSE;
}

BOOL WINAPI EnumerateLoadedModules64_DummyFn( IN HANDLE, IN PENUMLOADED_MODULES_CALLBACK64, IN PVOID )
{
	return FALSE;
}

DWORD64 WINAPI SymLoadModule64_DummyFn( IN HANDLE, IN HANDLE, IN PCSTR, IN PCSTR, IN DWORD64, IN DWORD )
{
	return 0;
}

BOOL WINAPI SymUnloadModule64_DummyFn( IN HANDLE, IN DWORD64 )
{
	return FALSE;
}

BOOL WINAPI SymFromAddr_DummyFn( IN HANDLE, IN DWORD64, OUT PDWORD64, IN OUT PSYMBOL_INFO )
{
	return FALSE;
}

BOOL WINAPI SymGetLineFromAddr64_DummyFn( IN HANDLE, IN DWORD64, OUT PDWORD, OUT PIMAGEHLP_LINE64 )
{
	return FALSE;
}

BOOL WINAPI SymGetModuleInfo64_DummyFn( IN HANDLE, IN DWORD64, OUT PIMAGEHLP_MODULE64 )
{
	return FALSE;
}



BOOL WINAPI StackWalk64_DummyFn( DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID, PREAD_PROCESS_MEMORY_ROUTINE64, PFUNCTION_TABLE_ACCESS_ROUTINE64, PGET_MODULE_BASE_ROUTINE64, PTRANSLATE_ADDRESS_ROUTINE64 )
{
	return FALSE;
}

USHORT WINAPI CaptureStackBackTrace_DummyFn( IN ULONG, IN ULONG, OUT PVOID *, OUT OPTIONAL PULONG )
{
	return 0;
}

class CHelperFunctionsLoader
{
public:
	CHelperFunctionsLoader( void )
	{
		m_bIsInitialized = false;
		m_bShouldReloadSymbols = false;
		m_hProcess = INVALID_HANDLE_VALUE;
		m_hDbgHelpDll = NULL;
		m_szPDBSearchPath = NULL;
		m_pSymInitialize = SymInitialize_DummyFn;
		m_pSymCleanup = SymCleanup_DummyFn;
		m_pSymSetOptions = SymSetOptions_DummyFn;
		m_pSymGetOptions = SymGetOptions_DummyFn;
		m_pSymSetSearchPath = SymSetSearchPath_DummyFn;
		m_pSymEnumerateModules64 = SymEnumerateModules64_DummyFn;
		m_pEnumerateLoadedModules64 = EnumerateLoadedModules64_DummyFn;
		m_pSymLoadModule64 = SymLoadModule64_DummyFn;
		m_pSymUnloadModule64 = SymUnloadModule64_DummyFn;
		m_pSymFromAddr = SymFromAddr_DummyFn;
		m_pSymGetLineFromAddr64 = SymGetLineFromAddr64_DummyFn;
		m_pSymGetModuleInfo64 = SymGetModuleInfo64_DummyFn;

#if defined( USE_STACKWALK64 )
		m_pStackWalk64 = StackWalk64_DummyFn;
#endif

#if defined( USE_CAPTURESTACKBACKTRACE )
		m_pCaptureStackBackTrace = CaptureStackBackTrace_DummyFn;
		m_hNTDllDll = NULL;
#endif
	}

	~CHelperFunctionsLoader( void )
	{
		m_pSymCleanup( m_hProcess );

		if( m_hDbgHelpDll != NULL )
			::FreeLibrary( m_hDbgHelpDll );

#if defined( USE_CAPTURESTACKBACKTRACE )
		if( m_hNTDllDll != NULL )
			::FreeLibrary( m_hNTDllDll );
#endif

		delete []m_szPDBSearchPath;
	}

	static BOOL CALLBACK UnloadSymbolsCallback( [[maybe_unused]] PSTR ModuleName, DWORD64 BaseOfDll, PVOID UserContext )
	{
		const CHelperFunctionsLoader *pThis = ((CHelperFunctionsLoader *)UserContext);
		pThis->m_pSymUnloadModule64( pThis->m_hProcess, BaseOfDll );
		return TRUE;
	}

	static BOOL CALLBACK LoadSymbolsCallback( PCSTR ModuleName, DWORD64 ModuleBase, ULONG ModuleSize, PVOID UserContext )
	{
		const CHelperFunctionsLoader *pThis = ((CHelperFunctionsLoader *)UserContext);
		//SymLoadModule64( IN HANDLE hProcess, IN HANDLE hFile, IN PSTR ImageName, IN PSTR ModuleName, IN DWORD64 BaseOfDll, IN DWORD SizeOfDll );
		pThis->m_pSymLoadModule64( pThis->m_hProcess, NULL, ModuleName, ModuleName, ModuleBase, ModuleSize );
		return TRUE;
	}

	void TryLoadingNewSymbols( void )
	{
		AUTO_LOCK( m_Mutex );

		if( m_bIsInitialized )
		{
			//m_pSymEnumerateModules64( m_hProcess, UnloadSymbolsCallback, this ); //unloaded modules we've already loaded
			m_pEnumerateLoadedModules64( m_hProcess, LoadSymbolsCallback, this ); //load everything
			m_bShouldReloadSymbols = false;
		}
	}

	void SetStackTranslationSymbolSearchPath( const char *szSemicolonSeparatedList )
	{
		AUTO_LOCK( m_Mutex );

		delete []m_szPDBSearchPath;

		if( szSemicolonSeparatedList == NULL )
		{
			m_szPDBSearchPath = NULL;
			return;
		}

		intp iLength = (intp)strlen( szSemicolonSeparatedList ) + 1;
		char *pNewPath = new char [iLength];
		memcpy( pNewPath, szSemicolonSeparatedList, iLength );
		m_szPDBSearchPath = pNewPath;

		//re-init search paths. Or if we haven't yet loaded dbghelp.dll, this will go to the dummy function and do nothing
		m_pSymSetSearchPath( m_hProcess, m_szPDBSearchPath );
		//TryLoadingNewSymbols();
		m_bShouldReloadSymbols = true;
	}

	bool GetSymbolNameFromAddress( const void *pAddress, tchar *pSymbolNameOut, int iMaxSymbolNameLength, uint64 *pDisplacementOut )
	{
		if( pAddress == NULL )
			return false;

		AUTO_LOCK( m_Mutex );

		alignas(SYMBOL_INFO) unsigned char genericbuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME*sizeof(TCHAR)];

		((PSYMBOL_INFO)genericbuffer)->SizeOfStruct = sizeof(SYMBOL_INFO);
		((PSYMBOL_INFO)genericbuffer)->MaxNameLen = MAX_SYM_NAME;

		DWORD64 dwDisplacement;
		if( m_pSymFromAddr( m_hProcess, (DWORD64)pAddress, &dwDisplacement, (PSYMBOL_INFO)genericbuffer) )
		{
			strncpy( pSymbolNameOut, ((PSYMBOL_INFO)genericbuffer)->Name, iMaxSymbolNameLength );
			if( pDisplacementOut != NULL )
				*pDisplacementOut = dwDisplacement;
			return true;
		}
		
		return false;
	}

	bool GetFileAndLineFromAddress( const void *pAddress, tchar *pFileNameOut, int iMaxFileNameLength, uint32 &iLineNumberOut, uint32 *pDisplacementOut )
	{
		if( pAddress == NULL )
			return false;

		AUTO_LOCK( m_Mutex );

		tchar szBuffer[1024];
		szBuffer[0] = _T('\0');
		
		IMAGEHLP_LINE64 imageHelpLine64;		
		imageHelpLine64.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
		imageHelpLine64.FileName = szBuffer;

		DWORD dwDisplacement;
		if( m_pSymGetLineFromAddr64( m_hProcess, (DWORD64)pAddress, &dwDisplacement, &imageHelpLine64 ) )
		{
			strncpy( pFileNameOut, imageHelpLine64.FileName, iMaxFileNameLength );
			iLineNumberOut = imageHelpLine64.LineNumber;
			
			if( pDisplacementOut != NULL )
				*pDisplacementOut = dwDisplacement;

			return true;
		}
		
		return false;
	}

	bool GetModuleNameFromAddress( const void *pAddress, tchar *pModuleNameOut, int iMaxModuleNameLength )
	{
		AUTO_LOCK( m_Mutex );
		IMAGEHLP_MODULE64 moduleInfo;

		moduleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);

		if ( m_pSymGetModuleInfo64( m_hProcess, (DWORD64)pAddress, &moduleInfo ) )
		{
			strncpy( pModuleNameOut, moduleInfo.ModuleName, iMaxModuleNameLength );
			return true;
		}

		return false;
	}

	//only returns false if we ran out of buffer space.
	bool TranslatePointer( const void * const pAddress, tchar *pTranslationOut, int iTranslationBufferLength, TranslateStackInfo_StyleFlags_t style )
	{
		//AUTO_LOCK( m_Mutex );

		if( pTranslationOut == NULL )
			return false;

		if( iTranslationBufferLength <= 0 )
			return false;

		//sample desired output
		// valid translation -		"tier0.dll!CHelperFunctionsLoader::TranslatePointer - u:\Dev\L4D\src\tier0\stacktools.cpp(162) + 4 bytes"
		// fallback translation -	"tier0.dll!0x01234567"

		tchar *pWrite = pTranslationOut;
		*pWrite = '\0';
		intp iLength;

		if( style & TSISTYLEFLAG_MODULENAME )
		{
			if( !this->GetModuleNameFromAddress( pAddress, pWrite, iTranslationBufferLength ) )
				strncpy( pWrite, "unknown_module", iTranslationBufferLength );

			iLength = (intp)strlen( pWrite );
			pWrite += iLength;
			iTranslationBufferLength -= iLength;

			if( iTranslationBufferLength < 2 )
				return false; //need more buffer

			if( style & TSISTYLEFLAG_SYMBOLNAME )
			{
				*pWrite = '!';
				++pWrite;
				--iTranslationBufferLength;
				*pWrite = '\0';
			}			
		}

		//use symbol name to test if the rest is going to work. So grab it whether they want it or not
		if( !this->GetSymbolNameFromAddress( pAddress, pWrite, iTranslationBufferLength, NULL ) )
		{
			int nBytesWritten = _snprintf( pWrite, iTranslationBufferLength, "0x%p", pAddress );
			if ( nBytesWritten < 0 )
			{
				*pWrite = '\0'; // if we can't write all of the line/lineandoffset, don't write any at all
				return false;
			}
			return true;
		}
		else if( style & TSISTYLEFLAG_SYMBOLNAME )
		{
			iLength = (intp)strlen( pWrite );
			pWrite += iLength;
			iTranslationBufferLength -= iLength;
		}
		else
		{
			*pWrite = '\0'; //symbol name lookup worked, but unwanted, discard
		}

		if( style & (TSISTYLEFLAG_FULLPATH | TSISTYLEFLAG_SHORTPATH | TSISTYLEFLAG_LINE | TSISTYLEFLAG_LINEANDOFFSET) )
		{
			if( pWrite != pTranslationOut ) //if we've written anything yet, separate the printed data from the file name and line
			{
				if( iTranslationBufferLength < 6 )
					return false; //need more buffer

				pWrite[0] = ' '; //append " - "
				pWrite[1] = '-';
				pWrite[2] = ' ';
				pWrite[3] = '\0';
				pWrite += 3;
				iTranslationBufferLength -= 3;
			}

			uint32 iLine;
			uint32 iDisplacement;
			char szFileName[MAX_PATH];
			if( this->GetFileAndLineFromAddress( pAddress, szFileName, MAX_PATH, iLine, &iDisplacement ) )
			{
				if( style & TSISTYLEFLAG_FULLPATH )
				{
					iLength = (intp)strlen( szFileName );
					if ( iTranslationBufferLength < iLength + 1 )
						return false;

					memcpy( pWrite, szFileName, iLength + 1 );
					pWrite += iLength;
					iTranslationBufferLength -= iLength;
				}
				else if( style & TSISTYLEFLAG_SHORTPATH )
				{
					//shorten the path and copy
					iLength = (intp)strlen( szFileName );
					char *pShortened = szFileName + iLength;
					int iSlashesAllowed = 3;
					while( pShortened > szFileName )
					{
						if( (*pShortened == '\\') || (*pShortened == '/') )
						{
							--iSlashesAllowed;
							if( iSlashesAllowed == 0 )
								break;
						}

						--pShortened;
					}
					
					iLength = (intp)strlen( pShortened );
					if( iTranslationBufferLength < iLength + 1 )
					{
						//Remove the " - " that we can't append to
						pWrite -= 3;
						iTranslationBufferLength += 3;
						*pWrite = '\0';
						return false;
					}

					memcpy( pWrite, szFileName, iLength + 1 );
					pWrite += iLength;
					iTranslationBufferLength -= iLength;
				}

				if( style & (TSISTYLEFLAG_LINE | TSISTYLEFLAG_LINEANDOFFSET) )
				{
					int nBytesWritten = _snprintf( pWrite, iTranslationBufferLength, ((style & TSISTYLEFLAG_LINEANDOFFSET) && (iDisplacement != 0)) ? "(%d) + %d bytes" : "(%d)", iLine, iDisplacement );
					if ( nBytesWritten < 0 )
					{
						*pWrite = '\0'; // if we can't write all of the line/lineandoffset, don't write any at all
						return false;
					}

					pWrite += nBytesWritten;
					iTranslationBufferLength -= nBytesWritten;
				}
			}
			else
			{
				//Remove the " - " that we didn't append to
				pWrite -= 3;
				iTranslationBufferLength += 3;
				*pWrite = '\0';
			}
		}

		return true;
	}


	//about to actually use the functions, load if necessary
	void EnsureReady( void )
	{
		if( m_bIsInitialized )
		{
			if( m_bShouldReloadSymbols )
				TryLoadingNewSymbols();

			return;
		}

		AUTO_LOCK( m_Mutex );

		//Only enabled for P4 and Steam Beta builds
		if( (CommandLine()->FindParm( "-steam" ) != 0) && //is steam
			(CommandLine()->FindParm( "-internalbuild" ) == 0) ) //is not steam beta
		{
			//disable the toolset by falsifying initialized state
			m_bIsInitialized = true;
			return;
		}

		m_hProcess = GetCurrentProcess();
		if( m_hProcess == NULL )
			return;

		m_bIsInitialized = true;

		// get the function pointer directly so that we don't have to include the .lib, and that
		// we can easily change it to using our own dll when this code is used on win98/ME/2K machines
		m_hDbgHelpDll = ::LoadLibraryExA( "DbgHelp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 );
		if ( !m_hDbgHelpDll )
		{
			//it's possible it's just way too early to initialize (as shown with attempts at using these tools in the memory allocator)
			if( m_szPDBSearchPath == NULL ) //not a rock solid check, but pretty good compromise between endless failing initialization and general failure due to trying too early
				m_bIsInitialized = false; 

			return;
		}

		m_pSymInitialize = (PFN_SymInitialize) ::GetProcAddress( m_hDbgHelpDll, "SymInitialize" );
		if( m_pSymInitialize == NULL )
		{
			//very bad
			::FreeLibrary( m_hDbgHelpDll );
			m_hDbgHelpDll = NULL;
			m_pSymInitialize = SymInitialize_DummyFn;
			return;
		}

		m_pSymCleanup = (PFN_SymCleanup) ::GetProcAddress( m_hDbgHelpDll, "SymCleanup" );
		if( m_pSymCleanup == NULL )
			m_pSymCleanup = SymCleanup_DummyFn;

		m_pSymGetOptions = (PFN_SymGetOptions) ::GetProcAddress( m_hDbgHelpDll, "SymGetOptions" );
		if( m_pSymGetOptions == NULL )
			m_pSymGetOptions = SymGetOptions_DummyFn;

		m_pSymSetOptions = (PFN_SymSetOptions) ::GetProcAddress( m_hDbgHelpDll, "SymSetOptions" );
		if( m_pSymSetOptions == NULL )
			m_pSymSetOptions = SymSetOptions_DummyFn;

		m_pSymSetSearchPath = (PFN_SymSetSearchPath) ::GetProcAddress( m_hDbgHelpDll, "SymSetSearchPath" );
		if( m_pSymSetSearchPath == NULL )
			m_pSymSetSearchPath = SymSetSearchPath_DummyFn;

		m_pSymEnumerateModules64 = (PFN_SymEnumerateModules64) ::GetProcAddress( m_hDbgHelpDll, "SymEnumerateModules64" );
		if( m_pSymEnumerateModules64 == NULL )
			m_pSymEnumerateModules64 = SymEnumerateModules64_DummyFn;

		m_pEnumerateLoadedModules64 = (PFN_EnumerateLoadedModules64) ::GetProcAddress( m_hDbgHelpDll, "EnumerateLoadedModules64" );
		if( m_pEnumerateLoadedModules64 == NULL )
			m_pEnumerateLoadedModules64 = EnumerateLoadedModules64_DummyFn;

		m_pSymLoadModule64 = (PFN_SymLoadModule64) ::GetProcAddress( m_hDbgHelpDll, "SymLoadModule64" );
		if( m_pSymLoadModule64 == NULL )
			m_pSymLoadModule64 = SymLoadModule64_DummyFn;

		m_pSymUnloadModule64 = (PFN_SymUnloadModule64) ::GetProcAddress( m_hDbgHelpDll, "SymUnloadModule64" );
		if( m_pSymUnloadModule64 == NULL )
			m_pSymUnloadModule64 = SymUnloadModule64_DummyFn;

		m_pSymFromAddr = (PFN_SymFromAddr) ::GetProcAddress( m_hDbgHelpDll, "SymFromAddr" );
		if( m_pSymFromAddr == NULL )
			m_pSymFromAddr = SymFromAddr_DummyFn;

		m_pSymGetLineFromAddr64 = (PFN_SymGetLineFromAddr64) ::GetProcAddress( m_hDbgHelpDll, "SymGetLineFromAddr64" );
		if( m_pSymGetLineFromAddr64 == NULL )
			m_pSymGetLineFromAddr64 = SymGetLineFromAddr64_DummyFn;

		m_pSymGetModuleInfo64 = (PFN_SymGetModuleInfo64) ::GetProcAddress( m_hDbgHelpDll, "SymGetModuleInfo64" );
		if( m_pSymGetModuleInfo64 == NULL )
			m_pSymGetModuleInfo64 = SymGetModuleInfo64_DummyFn;

#if defined( USE_STACKWALK64 )
		m_pStackWalk64 = (PFN_StackWalk64) ::GetProcAddress( m_hDbgHelpDll, "StackWalk64" );
		if( m_pStackWalk64 == NULL )
			m_pStackWalk64 = StackWalk64_DummyFn;
#endif


#if defined( USE_CAPTURESTACKBACKTRACE )
		m_hNTDllDll = ::LoadLibrary( "ntdll.dll" );

		m_pCaptureStackBackTrace = (PFN_CaptureStackBackTrace) ::GetProcAddress( m_hNTDllDll, "RtlCaptureStackBackTrace" );
		if( m_pCaptureStackBackTrace == NULL )		m_pCaptureStackBackTrace = CaptureStackBackTrace_DummyFn;
#endif


		m_pSymSetOptions( m_pSymGetOptions() |
			SYMOPT_DEFERRED_LOADS | //load on demand
			SYMOPT_EXACT_SYMBOLS | //don't load the wrong file
			SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS | //don't prompt ever
			SYMOPT_LOAD_LINES ); //load line info

		m_pSymInitialize( m_hProcess, m_szPDBSearchPath, FALSE );
		TryLoadingNewSymbols();
	}

	bool m_bIsInitialized;
	bool m_bShouldReloadSymbols;
	HANDLE m_hProcess;
	HMODULE m_hDbgHelpDll;
	char *m_szPDBSearchPath;
	CThreadFastMutex m_Mutex; //DbgHelp functions are all single threaded.

	PFN_SymInitialize m_pSymInitialize;
	PFN_SymCleanup m_pSymCleanup;
	PFN_SymGetOptions m_pSymGetOptions;
	PFN_SymSetOptions m_pSymSetOptions;
	PFN_SymSetSearchPath m_pSymSetSearchPath;
	PFN_SymEnumerateModules64 m_pSymEnumerateModules64;
	PFN_EnumerateLoadedModules64 m_pEnumerateLoadedModules64;
	PFN_SymLoadModule64 m_pSymLoadModule64;
	PFN_SymUnloadModule64 m_pSymUnloadModule64;

	PFN_SymFromAddr m_pSymFromAddr;
	PFN_SymGetLineFromAddr64 m_pSymGetLineFromAddr64;
	PFN_SymGetModuleInfo64 m_pSymGetModuleInfo64;

#if defined( USE_STACKWALK64 )
	PFN_StackWalk64 m_pStackWalk64;
#endif

#if defined( USE_CAPTURESTACKBACKTRACE )
	HMODULE m_hNTDllDll;
	PFN_CaptureStackBackTrace m_pCaptureStackBackTrace;
#endif
};
static CHelperFunctionsLoader s_HelperFunctions;



#if defined( USE_STACKWALK64 ) //most reliable method thanks to boatloads of windows helper functions. Also the slowest.
int CrawlStack_StackWalk64( CONTEXT *pExceptionContext, void **pReturnAddressesOut, int iArrayCount, int iSkipCount )
{
	s_HelperFunctions.EnsureReady();

	AUTO_LOCK( s_HelperFunctions.m_Mutex );

	CONTEXT currentContext;
	memcpy( &currentContext, pExceptionContext, sizeof( CONTEXT ) );

	STACKFRAME64 sfFrame = {}; //memset(&sfFrame, 0x0, sizeof(sfFrame));
	sfFrame.AddrPC.Mode = sfFrame.AddrFrame.Mode = AddrModeFlat;
#ifdef _M_X64
	sfFrame.AddrPC.Offset = currentContext.Rip;	
	sfFrame.AddrFrame.Offset = currentContext.Rbp; // ????
#else
	sfFrame.AddrPC.Offset = currentContext.Eip;	
	sfFrame.AddrFrame.Offset = currentContext.Ebp;
#endif

	HANDLE hThread = GetCurrentThread();

	int i;
	for( i = 0; i != iSkipCount; ++i ) //skip entries that the requesting function thinks are uninformative
	{
		if(!s_HelperFunctions.m_pStackWalk64( STACKWALK64_MACHINETYPE, s_HelperFunctions.m_hProcess, hThread, &sfFrame, &currentContext, NULL, NULL, NULL, NULL ) ||
			(sfFrame.AddrFrame.Offset == 0) )
		{
			return 0;
		}
	}

	for( i = 0; i != iArrayCount; ++i )
	{
		if(!s_HelperFunctions.m_pStackWalk64( STACKWALK64_MACHINETYPE, s_HelperFunctions.m_hProcess, hThread, &sfFrame, &currentContext, NULL, NULL, NULL, NULL ) ||
			(sfFrame.AddrFrame.Offset == 0) )
		{
			break;
		}
		pReturnAddressesOut[i] = (void *)sfFrame.AddrPC.Offset;	
	}

	return i;
}

void GetCallStackReturnAddresses_Exception( void **CallStackReturnAddresses, int *pRetCount, int iSkipCount, _EXCEPTION_POINTERS * pExceptionInfo )
{
	int iCount = CrawlStack_StackWalk64( pExceptionInfo->ContextRecord, CallStackReturnAddresses, *pRetCount, iSkipCount + 1 ); //skipping RaiseException()
	*pRetCount = iCount;
}
#endif //#if defined( USE_STACKWALK64 )



int GetCallStack( void **pReturnAddressesOut, int iArrayCount, int iSkipCount )
{
	s_HelperFunctions.EnsureReady();

	++iSkipCount; //skip this function

#if defined( USE_CAPTURESTACKBACKTRACE )
	if( s_HelperFunctions.m_pCaptureStackBackTrace != CaptureStackBackTrace_DummyFn )
	{
		//docs state a total limit of 63 back traces between skipped and stored
		int iRetVal = s_HelperFunctions.m_pCaptureStackBackTrace( iSkipCount, MIN( iArrayCount, 63 - iSkipCount ), pReturnAddressesOut, NULL );
		return AppendParentStackTrace( pReturnAddressesOut, iArrayCount, iRetVal );
	}
#endif
#if defined( USE_STACKWALK64 )
	if( s_HelperFunctions.m_pStackWalk64 != StackWalk64_DummyFn )
	{
		int iInOutArrayCount = iArrayCount; //array count becomes both input and output with exception handler version
		__try
		{
			::RaiseException( 0, EXCEPTION_NONCONTINUABLE, 0, NULL );
		}
		__except ( GetCallStackReturnAddresses_Exception( pReturnAddressesOut, &iInOutArrayCount, iSkipCount, GetExceptionInformation() ), EXCEPTION_EXECUTE_HANDLER )
		{
			return AppendParentStackTrace( pReturnAddressesOut, iArrayCount, iInOutArrayCount );
		}
	}
#endif

	return GetCallStack_Fast( pReturnAddressesOut, iArrayCount, iSkipCount );
}

void SetStackTranslationSymbolSearchPath( const char *szSemicolonSeparatedList )
{
	s_HelperFunctions.SetStackTranslationSymbolSearchPath( szSemicolonSeparatedList );
}

void StackToolsNotify_LoadedLibrary( [[maybe_unused]] const char *szLibName )
{
	s_HelperFunctions.m_bShouldReloadSymbols = true;
}

int TranslateStackInfo( const void * const *pCallStack, int iCallStackCount, tchar *szOutput, int iOutBufferSize, const tchar *szEntrySeparator, TranslateStackInfo_StyleFlags_t style )
{
	s_HelperFunctions.EnsureReady();
	tchar *szStartOutput = szOutput;

	if( szEntrySeparator == NULL )
		szEntrySeparator = _T("");

	intp iSeparatorSize = (intp)strlen( szEntrySeparator );

	for( int i = 0; i < iCallStackCount; ++i )
	{
		if( !s_HelperFunctions.TranslatePointer( pCallStack[i], szOutput, iOutBufferSize, style ) )
		{
			return i;
		}

		intp iLength = (intp)strlen( szOutput );
		szOutput += iLength;
		iOutBufferSize -= iLength;

		if( iOutBufferSize > iSeparatorSize )
		{
			memcpy( szOutput, szEntrySeparator, iSeparatorSize * sizeof( tchar ) );
			szOutput += iSeparatorSize;
			iOutBufferSize -= iSeparatorSize;
		}
		*szOutput = '\0';
	}

	szOutput -= iSeparatorSize;
	if( szOutput >= szStartOutput )
		*szOutput = '\0';

	return iCallStackCount;
}

void PreloadStackInformation( [[maybe_unused]] void * const *pAddresses, [[maybe_unused]] int iAddressCount )
{
	//nop on anything but 360
}

bool GetFileAndLineFromAddress( const void *pAddress, tchar *pFileNameOut, int iMaxFileNameLength, uint32 &iLineNumberOut, uint32 *pDisplacementOut )
{
	s_HelperFunctions.EnsureReady();
	return s_HelperFunctions.GetFileAndLineFromAddress( pAddress, pFileNameOut, iMaxFileNameLength, iLineNumberOut, pDisplacementOut );
}

bool GetSymbolNameFromAddress( const void *pAddress, tchar *pSymbolNameOut, int iMaxSymbolNameLength, uint64 *pDisplacementOut )
{
	s_HelperFunctions.EnsureReady();
	return s_HelperFunctions.GetSymbolNameFromAddress( pAddress, pSymbolNameOut, iMaxSymbolNameLength, pDisplacementOut );
}

bool GetModuleNameFromAddress( const void *pAddress, tchar *pModuleNameOut, int iMaxModuleNameLength )
{
	s_HelperFunctions.EnsureReady();
	return s_HelperFunctions.GetModuleNameFromAddress( pAddress, pModuleNameOut, iMaxModuleNameLength );
}

#endif //#else //#if defined( WIN32 ) && !defined( _X360 )

#endif //#if !defined( ENABLE_RUNTIME_STACK_TRANSLATION )




CCallStackStorage::CCallStackStorage( FN_GetCallStack GetStackFunction, uint32 iSkipCalls )
{
	iValidEntries = GetStackFunction( pStack, std::size( pStack ), iSkipCalls + 1 );
}



CStackTop_CopyParentStack::CStackTop_CopyParentStack( void * const *pParentStackTrace, int iParentStackTraceLength )
{
#if defined( ENABLE_RUNTIME_STACK_TRANSLATION )
	//miniature version of GetCallStack_Fast()
#if (defined( TIER0_FPO_DISABLED ) || defined( _DEBUG )) &&\
	(defined( WIN32 ) && !defined( _X360 ) && !defined(_M_X64))
	void *pStackCrawlEBP;
	__asm
	{
		mov [pStackCrawlEBP], ebp;
	}
	pStackCrawlEBP = *(void **)pStackCrawlEBP;
	m_pReplaceAddress = *((void **)pStackCrawlEBP + 1);
	m_pStackBase = (void *)((void **)pStackCrawlEBP + 1);
#else
	m_pReplaceAddress = NULL;
	m_pStackBase = this;
#endif

	m_pParentStackTrace = NULL;

	if( (pParentStackTrace != NULL) && (iParentStackTraceLength > 0) )
	{
		while( (iParentStackTraceLength > 0) && (pParentStackTrace[iParentStackTraceLength - 1] == NULL) )
		{
			--iParentStackTraceLength;
		}

		if( iParentStackTraceLength > 0 )
		{
			m_pParentStackTrace = new void * [iParentStackTraceLength];
			memcpy( (void **)m_pParentStackTrace, pParentStackTrace, sizeof( void * ) * iParentStackTraceLength );
		}
	}	

	m_iParentStackTraceLength = iParentStackTraceLength;

	m_pPrevTop = g_StackTop;
	g_StackTop = this;
	Assert( (CStackTop_Base *)g_StackTop == this );
#endif //#if defined( ENABLE_RUNTIME_STACK_TRANSLATION )
}

CStackTop_CopyParentStack::~CStackTop_CopyParentStack( void )
{
#if defined( ENABLE_RUNTIME_STACK_TRANSLATION )
	Assert( (CStackTop_Base *)g_StackTop == this );
	g_StackTop = m_pPrevTop;

	delete []m_pParentStackTrace;
#endif
}



CStackTop_ReferenceParentStack::CStackTop_ReferenceParentStack( void * const *pParentStackTrace, int iParentStackTraceLength )
{
#if defined( ENABLE_RUNTIME_STACK_TRANSLATION )
	//miniature version of GetCallStack_Fast()
#if (defined( TIER0_FPO_DISABLED ) || defined( _DEBUG )) &&\
	(defined( WIN32 ) && !defined( _X360 ) && !defined(_M_X64))
	void *pStackCrawlEBP;
	__asm
	{
		mov [pStackCrawlEBP], ebp;
	}
	pStackCrawlEBP = *(void **)pStackCrawlEBP;
	m_pReplaceAddress = *((void **)pStackCrawlEBP + 1);
	m_pStackBase = (void *)((void **)pStackCrawlEBP + 1);
#else
	m_pReplaceAddress = NULL;
	m_pStackBase = this;
#endif

	m_pParentStackTrace = pParentStackTrace;

	if( (pParentStackTrace != NULL) && (iParentStackTraceLength > 0) )
	{
		while( (iParentStackTraceLength > 0) && (pParentStackTrace[iParentStackTraceLength - 1] == NULL) )
		{
			--iParentStackTraceLength;
		}
	}	

	m_iParentStackTraceLength = iParentStackTraceLength;

	m_pPrevTop = g_StackTop;
	g_StackTop = this;
	Assert( (CStackTop_Base *)g_StackTop == this );
#endif //#if defined( ENABLE_RUNTIME_STACK_TRANSLATION )
}

CStackTop_ReferenceParentStack::~CStackTop_ReferenceParentStack( void )
{
#if defined( ENABLE_RUNTIME_STACK_TRANSLATION )
	Assert( (CStackTop_Base *)g_StackTop == this );
	g_StackTop = m_pPrevTop;

	ReleaseParentStackReferences();
#endif
}

void CStackTop_ReferenceParentStack::ReleaseParentStackReferences( void )
{
#if defined( ENABLE_RUNTIME_STACK_TRANSLATION )
	m_pParentStackTrace = NULL;
	m_iParentStackTraceLength = 0;
#endif
}




//Encodes data so that every byte's most significant bit is a 1. Ensuring no null terminators.
//This puts the encoded data in the 128-255 value range. Leaving all standard ascii characters for control.
//Returns string length (not including the written null terminator as is standard). 
//Or if the buffer is too small. Returns negative of necessary buffer size (including room needed for null terminator)
int EncodeBinaryToString( const void *pToEncode, int iDataLength, char *pEncodeOut, int iEncodeBufferSize )
{
	int iEncodedSize = iDataLength;
	iEncodedSize += (iEncodedSize + 6) / 7; //Have 1 control byte for every 7 actual bytes
	iEncodedSize += sizeof( uint32 ) + 1; //data size at the beginning of the blob and null terminator at the end

	if( (iEncodedSize > iEncodeBufferSize) || (pEncodeOut == NULL) || (pToEncode == NULL) )
		return -iEncodedSize; //not enough room

	uint8 *pEncodeWrite = (uint8 *)pEncodeOut;	

	//first encode the data size. Encodes lowest 28 bits and discards the high 4
	pEncodeWrite[0] = ((iDataLength >> 21) & 0xFF) | 0x80;
	pEncodeWrite[1] = ((iDataLength >> 14) & 0xFF) | 0x80;
	pEncodeWrite[2] = ((iDataLength >> 7) & 0xFF) | 0x80;
	pEncodeWrite[3] = ((iDataLength >> 0) & 0xFF) | 0x80;
	pEncodeWrite += 4;

	const uint8 *pEncodeRead = (const uint8 *)pToEncode;
	const uint8 *pEncodeStop = pEncodeRead + iDataLength;
	uint8 *pEncodeWriteLastControlByte = pEncodeWrite;
	int iControl = 0;

	//Encode the data
	while( pEncodeRead < pEncodeStop )
	{
		if( iControl == 0 )
		{
			pEncodeWriteLastControlByte = pEncodeWrite;
			*pEncodeWriteLastControlByte = 0x80;
		}
		else
		{
			*pEncodeWrite = *pEncodeRead | 0x80; //encoded data always has the MSB bit set (cheap avoidance of null terminators)
			*pEncodeWriteLastControlByte |= (((*pEncodeRead) & 0x80) ^ 0x80) >> iControl; //We use the control byte to XOR the MSB back to original values on decode
			++pEncodeRead;
		}

		++pEncodeWrite;
		++iControl;
		iControl &= 7; //8->0
	}
	*pEncodeWrite = '\0';

	return iEncodedSize - 1;
}

//Decodes a string produced by EncodeBinaryToString(). Safe to decode in place if you don't mind trashing your string, binary byte count always less than string byte count.
//Returns:
//	>= 0 is the decoded data size
//	INT_MIN (most negative value possible) indicates an improperly formatted string (not our data)
//	all other negative values are the negative of how much dest buffer size is necessary.
int DecodeBinaryFromString( const char *pString, void *pDestBuffer, int iDestBufferSize, char **ppParseFinishOut )
{
	const uint8 *pDecodeRead = (const uint8 *)pString;

	if( (pDecodeRead[0] < 0x80) || (pDecodeRead[1] < 0x80) || (pDecodeRead[2] < 0x80) || (pDecodeRead[3] < 0x80) )
	{
		if( ppParseFinishOut != NULL )
			*ppParseFinishOut = (char *)pString;

		return INT_MIN; //Don't know what the string is, but it's not our format
	}

	int iDecodedSize = 0;
	iDecodedSize |= (pDecodeRead[0] & 0x7F) << 21;
	iDecodedSize |= (pDecodeRead[1] & 0x7F) << 14;
	iDecodedSize |= (pDecodeRead[2] & 0x7F) << 7;
	iDecodedSize |= (pDecodeRead[3] & 0x7F) << 0;
	pDecodeRead += 4;

	int iTextLength = iDecodedSize;
	iTextLength += (iTextLength + 6) / 7; //Have 1 control byte for every 7 actual bytes

	//make sure it's formatted properly
	for( int i = 0; i != iTextLength; ++i )
	{
		if( pDecodeRead[i] < 0x80 ) //encoded data always has MSB set
		{
			if( ppParseFinishOut != NULL )
				*ppParseFinishOut = (char *)pString;

			return INT_MIN; //either not our data, or part of the string is missing
		}
	}

	if( iDestBufferSize < iDecodedSize )
	{
		if( ppParseFinishOut != NULL )
			*ppParseFinishOut = (char *)pDecodeRead;

		return -iDecodedSize; //dest buffer not big enough to hold the data
	}

	const uint8 *pStopDecoding = pDecodeRead + iTextLength;		
	uint8 *pDecodeWrite = (uint8 *)pDestBuffer;
	int iControl = 0;
	int iLSBXOR = 0;

	while( pDecodeRead < pStopDecoding )
	{
		if( iControl == 0 )
		{
			iLSBXOR = *pDecodeRead;
		}
		else
		{
			*pDecodeWrite = *pDecodeRead ^ ((iLSBXOR << iControl) & 0x80);
			++pDecodeWrite;
		}

		++pDecodeRead;
		++iControl;
		iControl &= 7; //8->0
	}

	if( ppParseFinishOut != NULL )
		*ppParseFinishOut = (char *)pDecodeRead;

	return iDecodedSize;	
}


