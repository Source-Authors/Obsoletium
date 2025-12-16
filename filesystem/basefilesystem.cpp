//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "basefilesystem.h"
#include "tier0/vprof.h"
#include "tier0/icommandline.h"
#include "tier1/characterset.h"
#include "tier1/utlbuffer.h"
#include "tier1/convar.h"
#include "tier1/KeyValues.h"
#include "tier1/generichash.h"
#include "tier1/utllinkedlist.h"
#include "filesystem/IQueuedLoader.h"
#include "tier2/tier2.h"
#include "zip_utils.h"
#include "packfile.h"

#include "posix_file_stream.h"

#ifndef DEDICATED
#include "keyvaluescompiler.h"
#endif

#include "ifilelist.h"

#ifdef IS_WINDOWS_PC
// Needed for getting file type string
#define WIN32_LEAN_AND_MEAN
#include <shellapi.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar fs_report_sync_opens( "fs_report_sync_opens", "0", 0, "0:Off, 1:Blocking only, 2:All" );
ConVar fs_warning_mode( "fs_warning_mode", "0", 0, "0:Off, 1:Warn main thread, 2:Warn other threads"  );

constexpr inline int BSPOUTPUT{0};	// bsp output flag -- determines type of fs_log output to generate

static void AddSeperatorAndFixPath( char *str );

// Case-insensitive symbol table for path IDs.
CUtlSymbolTableMT g_PathIDTable( 0, 32, true );

int g_iNextSearchPathID = 1;

// This can be used to easily fix a filename on the stack.
#ifdef	DBGFLAG_ASSERT
// Look for cases like materials\\blah.vmt.
static bool V_CheckDoubleSlashes( const char *pStr )
{
	intp len = V_strlen( pStr );

	for ( intp i=1; i < len-1; i++ )
	{
		if ( (pStr[i] == '/' || pStr[i] == '\\') && (pStr[i+1] == '/' || pStr[i+1] == '\\') )
		{
			Msg( "Double slashes found in path '%s'.\n", pStr );
			return true;
		}
	}
	return false;
}

#define CHECK_DOUBLE_SLASHES( x ) V_CheckDoubleSlashes(x)
#else
#define CHECK_DOUBLE_SLASHES( x ) 
#endif

static void LogFileOpen( const char *vpk, const char *pFilename, const char *pAbsPath )
{
	static const char *mode = nullptr;

	// Figure out if we should be doing this at all, the first time we are acalled
	if ( mode == nullptr )
	{
		if ( CommandLine()->FindParm( "-log_opened_files" ) )
			mode = "wt";
		else
			mode = "";
	}

	if ( *mode == '\0' )
		return;

	// Open file for write or append
	auto [f, rc] = se::posix::posix_file_stream_factory::open( "opened_files.txt", mode );
	Assert( !rc );
	if ( !rc )
	{
		std::tie(std::ignore, rc) = f.print( "%s, %s, %s\n", vpk, pFilename, pAbsPath );
		Assert( !rc );

		// If this was the first time, switch from write to append for further writes
		mode = "at";
	}
}


static CBaseFileSystem *g_pBaseFileSystem;
CBaseFileSystem *BaseFileSystem()
{
	return g_pBaseFileSystem;
}

ConVar filesystem_buffer_size( "filesystem_buffer_size", "0", 0, "Size of per file buffers. 0 for none" );

#if defined( TRACK_BLOCKING_IO )

// If we hit more than 100 items in a frame, we're probably doing a level load...
#define MAX_ITEMS	100

class CBlockingFileItemList : public IBlockingFileItemList
{
public:
	CBlockingFileItemList( CBaseFileSystem *fs ) 
		: 
		m_pFS( fs ),
		m_bLocked( false )
	{
	}

	// You can't call any of the below calls without calling these methods!!!!
	virtual void	LockMutex()
	{
		Assert( !m_bLocked );
		if ( m_bLocked )
			return;
		m_bLocked = true;
		m_pFS->BlockingFileAccess_EnterCriticalSection();
	}

	virtual void	UnlockMutex()
	{
		Assert( m_bLocked );
		if ( !m_bLocked )
			return;

		m_pFS->BlockingFileAccess_LeaveCriticalSection();
		m_bLocked = false;
	}

	virtual int First() const
	{
		if ( !m_bLocked )
		{
			Error( "CBlockingFileItemList::First() w/o calling EnterCriticalSectionFirst!" );
		}
		return m_Items.Head();
	}

	virtual int Next( int i ) const
	{
		if ( !m_bLocked )
		{
			Error( "CBlockingFileItemList::Next() w/o calling EnterCriticalSectionFirst!" );
		}
		return m_Items.Next( i ); 
	}

	virtual int InvalidIndex() const
	{
		return m_Items.InvalidIndex();
	}

	virtual const FileBlockingItem& Get( int index ) const
	{
		if ( !m_bLocked )
		{
			Error( "CBlockingFileItemList::Get( %d ) w/o calling EnterCriticalSectionFirst!", index );
		}
		return m_Items[ index ];
	}

	virtual void Reset()
	{
		if ( !m_bLocked )
		{
			Error( "CBlockingFileItemList::Reset() w/o calling EnterCriticalSectionFirst!" );
		}
		m_Items.RemoveAll();
	}

	void Add( const FileBlockingItem& item )
	{
		// Ack, should use a linked list probably...
		while ( m_Items.Count() > MAX_ITEMS )
		{
			m_Items.Remove( m_Items.Head() );
		}
		m_Items.AddToTail( item );
	}


private:
	CUtlLinkedList< FileBlockingItem, unsigned short >	m_Items;
	CBaseFileSystem						*m_pFS;
	bool								m_bLocked;
};
#endif

CUtlSymbol CBaseFileSystem::m_GamePathID;
CUtlSymbol CBaseFileSystem::m_BSPPathID;
DVDMode_t CBaseFileSystem::m_DVDMode;
CUtlVector< FileNameHandle_t > CBaseFileSystem::m_ExcludePaths;


class CStoreIDEntry
{
public:
	CStoreIDEntry() = default;
	CStoreIDEntry( const char *pPathIDStr, int storeID )
		:  m_PathIDString{pPathIDStr}, m_StoreID{storeID}
	{
	}

public:
	CUtlSymbol	m_PathIDString;
	int			m_StoreID;
};


//-----------------------------------------------------------------------------

class CBaseFileSystem::CFileCacheObject
{
public:
	CFileCacheObject( CBaseFileSystem* pFS );
	~CFileCacheObject();
	void AddFiles( const char **ppFileNames, int nFileNames );
	bool IsReady() const { return m_nPending == 0; }

	static void IOCallback( const FileAsyncRequest_t &request, int nBytesRead, FSAsyncStatus_t err );

	struct Info_t
	{
		const char* pFileName;
		FSAsyncControl_t hIOAsync;
		CMemoryFileBacking* pBacking;
		CFileCacheObject* pOwner;
	};

	CBaseFileSystem* m_pFS;
	CInterlockedInt m_nPending;
	CThreadFastMutex m_InfosMutex;
	CUtlVector< Info_t* > m_Infos;

private:
	void ProcessNewEntries( intp start );

	CFileCacheObject(const CFileCacheObject &) = delete;
	CFileCacheObject & operator=(const CFileCacheObject& ) = delete;
};

//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------

CBaseFileSystem::CBaseFileSystem()
	: m_FileTracker2( this )
{
	g_pBaseFileSystem = this;
	g_pFullFileSystem = this;

	m_WhitelistFileTrackingEnabled = -1;

	// If this changes then FileNameHandleInternal_t/FileNameHandle_t needs to be fixed!!!
	Assert( sizeof( CUtlSymbol ) == sizeof( short ) );

	// Clear out statistics
	memset( &m_Stats, 0, sizeof(m_Stats) );

	m_fwLevel    = FILESYSTEM_WARNING_REPORTUNCLOSED;
	m_pfnWarning = nullptr;
	m_pLogFile   = nullptr;
	m_bOutputDebugString = false;
	m_WhitelistSpewFlags = 0;
	m_DirtyDiskReportFunc = nullptr;
	m_pPureServerWhitelist = nullptr;

	m_pThreadPool = nullptr;

#if defined( TRACK_BLOCKING_IO )
	m_pBlockingItems = new CBlockingFileItemList( this );
	m_bBlockingFileAccessReportingEnabled = false;
	m_bAllowSynchronousLogging = true;
#endif

	m_iMapLoad = 0;

	V_memset( m_PreloadData, 0, sizeof( m_PreloadData ) );

	// allows very specifc constrained behavior
	m_DVDMode = DVDMODE_OFF;

	m_pBaseDir[0] = '\0';
	m_iBaseLength = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseFileSystem::~CBaseFileSystem()
{
	m_PathIDInfos.PurgeAndDeleteElements();

#if defined( TRACK_BLOCKING_IO )
	delete m_pBlockingItems;
#endif

	// Free the whitelist.
	RegisterFileWhitelist( nullptr, nullptr );
}


//-----------------------------------------------------------------------------
// Methods of IAppSystem
//-----------------------------------------------------------------------------
void *CBaseFileSystem::QueryInterface( const char *pInterfaceName )
{
	// We also implement the IMatSystemSurface interface
	if (!Q_strncmp(	pInterfaceName, BASEFILESYSTEM_INTERFACE_VERSION, Q_strlen(BASEFILESYSTEM_INTERFACE_VERSION) + 1))
		return (IBaseFileSystem*)this;

	return nullptr;
}

InitReturnVal_t CBaseFileSystem::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	// This is a special tag to allow iterating just the BSP file, it doesn't show up in the list per se, but gets converted to "GAME" in the filter function
	m_BSPPathID = g_PathIDTable.AddString( "BSP" );
	m_GamePathID = g_PathIDTable.AddString( "GAME" );

	if ( getenv( "fs_debug" ) )
	{
		m_bOutputDebugString = true;
	}

	const char *logFileName = CommandLine()->ParmValue( "-fs_log" );
	if( logFileName )
	{
		m_pLogFile = fopen( logFileName, "w" ); // STEAM OK
		if ( !m_pLogFile )
			return INIT_FAILED;
		fprintf( m_pLogFile, "@echo off\n" );
		fprintf( m_pLogFile, "setlocal\n" );
		const char *fs_target = CommandLine()->ParmValue( "-fs_target" );
		if( fs_target )
		{
			fprintf( m_pLogFile, "set fs_target=\"%s\"\n", fs_target );
		}
		fprintf( m_pLogFile, "if \"%%fs_target%%\" == \"\" goto error\n" );
		fprintf( m_pLogFile, "@echo on\n" );
	}

	InitAsync();

	if ( IsX360() && m_DVDMode == DVDMODE_DEV )
	{
		// exclude paths are valid ony in dvddev mode
		char szExcludeFile[MAX_PATH];
		const char *pRemotePath = CommandLine()->ParmValue( "-remote" );
		const char *pBasePath = CommandLine()->ParmValue( "-basedir" );
		if ( pRemotePath && pBasePath )
		{
			// the optional exclude path file only exists at the remote path
			V_ComposeFileName( pRemotePath, "xbox_exclude_paths.txt", szExcludeFile );

			// populate the exclusion list
			CUtlBuffer buf( (intp)0, 0, CUtlBuffer::TEXT_BUFFER );
			if ( ReadFile( szExcludeFile, nullptr, buf, 0, 0 ) )
			{
				constexpr characterset_t breakSet{""};
				char szPath[MAX_PATH];
				char szToken[MAX_PATH];
				for ( ;; )
				{
					intp nTokenSize = buf.ParseToken( &breakSet, szToken );
					if ( nTokenSize <= 0 )
					{
						break;
					}

					char *pToken = szToken;
					if ( pToken[0] == '\\' )
					{
						// skip past possible initial seperator
						pToken++;
					}

					V_ComposeFileName( pBasePath, pToken, szPath );
					V_AppendSlash( szPath );
					
					FileNameHandle_t hFileName = FindOrAddFileName( szPath );
					if ( m_ExcludePaths.Find( hFileName ) == -1 )
					{
						m_ExcludePaths.AddToTail( hFileName );
					}
				}
			}
		}
	}

	return INIT_OK;
}

void CBaseFileSystem::Shutdown()
{
	ShutdownAsync();
	m_FileTracker2.ShutdownAsync();

#ifndef _X360
	if( m_pLogFile )
	{
		if( CommandLine()->FindParm( "-fs_logbins" ) >= 0 )
		{
			char cwd[MAX_FILEPATH];
			if( !getcwd( cwd, MAX_FILEPATH-1 ) ) 
			{
				// dimhotepus: Warn if getcwd failed.
				Warning(FileWarningLevel_t::FILESYSTEM_WARNING, "Enable to get cd: %s.\n", strerror(errno));
			}
			fprintf( m_pLogFile, "set binsrc=\"%s\"\n", cwd );
			fprintf( m_pLogFile, "mkdir \"%%fs_target%%\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\hl2.exe\" \"%%fs_target%%\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\hl2.dat\" \"%%fs_target%%\"\n" );
			fprintf( m_pLogFile, "mkdir \"%%fs_target%%\\" PLATFORM_BIN_DIR "\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\" PLATFORM_BIN_DIR "\\*.asi\" \"%%fs_target%%\\" PLATFORM_BIN_DIR "\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\" PLATFORM_BIN_DIR "\\materialsystem.dll\" \"%%fs_target%%\\" PLATFORM_BIN_DIR "\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\" PLATFORM_BIN_DIR "\\shaderapidx9.dll\" \"%%fs_target%%\\" PLATFORM_BIN_DIR "\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\" PLATFORM_BIN_DIR "\\filesystem_stdio.dll\" \"%%fs_target%%\\" PLATFORM_BIN_DIR "\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\" PLATFORM_BIN_DIR "\\soundemittersystem.dll\" \"%%fs_target%%\\" PLATFORM_BIN_DIR "\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\" PLATFORM_BIN_DIR "\\stdshader*.dll\" \"%%fs_target%%\\" PLATFORM_BIN_DIR "\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\" PLATFORM_BIN_DIR "\\launcher.dll\" \"%%fs_target%%\\" PLATFORM_BIN_DIR "\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\" PLATFORM_BIN_DIR "\\engine.dll\" \"%%fs_target%%\\" PLATFORM_BIN_DIR "\"\n" );

#ifdef _WIN64
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\" PLATFORM_BIN_DIR "\\mss64.dll\" \"%%fs_target%%\\" PLATFORM_BIN_DIR "\"\n" );
#else
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\" PLATFORM_BIN_DIR "\\mss32.dll\" \"%%fs_target%%\\" PLATFORM_BIN_DIR "\"\n" );
#endif

			fprintf( m_pLogFile, "copy \"%%binsrc%%\\" PLATFORM_BIN_DIR "\\tier0.dll\" \"%%fs_target%%\\" PLATFORM_BIN_DIR "\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\" PLATFORM_BIN_DIR "\\vgui2.dll\" \"%%fs_target%%\\" PLATFORM_BIN_DIR "\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\" PLATFORM_BIN_DIR "\\vguimatsurface.dll\" \"%%fs_target%%\\" PLATFORM_BIN_DIR "\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\" PLATFORM_BIN_DIR "\\voice_miles.dll\" \"%%fs_target%%\\" PLATFORM_BIN_DIR "\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\" PLATFORM_BIN_DIR "\\vphysics.dll\" \"%%fs_target%%\\" PLATFORM_BIN_DIR "\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\" PLATFORM_BIN_DIR "\\vstdlib.dll\" \"%%fs_target%%\\" PLATFORM_BIN_DIR "\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\" PLATFORM_BIN_DIR "\\studiorender.dll\" \"%%fs_target%%\\" PLATFORM_BIN_DIR "\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\" PLATFORM_BIN_DIR "\\vaudio_miles.dll\" \"%%fs_target%%\\" PLATFORM_BIN_DIR "\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\hl2\\resource\\*.ttf\" \"%%fs_target%%\\hl2\\resource\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\hl2\\" PLATFORM_BIN_DIR "\\gameui.dll\" \"%%fs_target%%\\hl2\\" PLATFORM_BIN_DIR "\"\n" );
		}
		fprintf( m_pLogFile, "goto done\n" );
		fprintf( m_pLogFile, ":error\n" );
		fprintf( m_pLogFile, "echo !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\"\n" );
		fprintf( m_pLogFile, "echo ERROR: must set fs_target=targetpath (ie. \"set fs_target=u:\\destdir\")!\n" );
		fprintf( m_pLogFile, "echo !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\"\n" );
		fprintf( m_pLogFile, ":done\n" );
		fclose( m_pLogFile ); // STEAM OK
	}
#endif

	UnloadCompiledKeyValues();

	RemoveAllSearchPaths();
	Trace_DumpUnclosedFiles();
	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Computes a full write path
//-----------------------------------------------------------------------------
inline void CBaseFileSystem::ComputeFullWritePath( char* pDest, int maxlen, const char *pRelativePath, const char *pWritePathID )
{
	Q_strncpy( pDest, GetWritePath( pRelativePath, pWritePathID ), maxlen );
	Q_strncat( pDest, pRelativePath, maxlen, COPY_ALL_CHARACTERS );
	Q_FixSlashes( pDest );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : src1 - 
//			src2 - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::OpenedFileLessFunc( COpenedFile const& src1, COpenedFile const& src2 )
{
	return src1.m_pFile < src2.m_pFile;
}


void CBaseFileSystem::InstallDirtyDiskReportFunc( FSDirtyDiskReportFunc_t func )
{
	m_DirtyDiskReportFunc = func;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fullpath - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::LogAccessToFile( char const *accesstype, char const *fullpath, char const *options )
{
	LOCAL_THREAD_LOCK();

	if ( m_fwLevel >= FILESYSTEM_WARNING_REPORTALLACCESSES && !V_stristr( fullpath, "console.log" ) )
	{
		Warning( FILESYSTEM_WARNING_REPORTALLACCESSES, "---FS%s:  %s %s (%.3f)\n", ThreadInMainThread() ? "" : "[a]", accesstype, fullpath, Plat_FloatTime() );
	}

	intp c = m_LogFuncs.Count();
	if ( !c )
		return;

	for ( intp i = 0; i < c; ++i )
	{
		( m_LogFuncs[ i ] )( fullpath, options );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//			*options - 
// Output : FILE
//-----------------------------------------------------------------------------
FILE *CBaseFileSystem::Trace_FOpen( const char *filenameT, const char *options, unsigned flags, int64 *size )
{
	AUTOBLOCKREPORTER_FN( Trace_FOpen, this, true, filenameT, FILESYSTEM_BLOCKING_SYNCHRONOUS, FileBlockingItem::FB_ACCESS_OPEN );

	char filename[MAX_PATH];
	FixUpPath ( filenameT, filename );

	FILE *fp = FS_fopen( filename, options, flags, size );
	if ( fp )
	{
		if ( options[0] == 'r' )
		{
			FS_setbufsize(fp, filesystem_buffer_size.GetInt() );
		}
		else
		{
			FS_setbufsize(fp, 32*1024 );
		}

		AUTO_LOCK( m_OpenedFilesMutex );
		COpenedFile file;

		file.SetName( filename );
		file.m_pFile = fp;

		m_OpenedFiles.AddToTail( file );

		LogAccessToFile( "open", filename, options );
	}

	return fp;
}

void CBaseFileSystem::GetFileNameForHandle( FileHandle_t handle, char *buf, size_t buflen )
{
	V_strncpy( buf, "Unknown", buflen );
	/*
	CFileHandle *fh = ( CFileHandle *)handle;
	if ( !fh )
	{
		buf[ 0 ] = 0;
		return;
	}

#if !defined( _RETAIL )
	// Pack file filehandles store the underlying name for convenience
	if ( fh->IsPack() )
	{
		Q_strncpy( buf, fh->Name(), buflen );
		return;
	}
#endif

	AUTO_LOCK( m_OpenedFilesMutex );

	COpenedFile file;
	file.m_pFile = fh->GetFileHandle();

	intp result = m_OpenedFiles.Find( file );
	if ( result != -1 )
	{
		COpenedFile found = m_OpenedFiles[ result ];
		Q_strncpy( buf, found.GetName(), buflen );
	}
	else
	{
		buf[ 0 ] = 0;
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fp - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::Trace_FClose( FILE *fp )
{
	if ( fp )
	{
		COpenedFile file;
		file.m_pFile = fp;

		intp result;
		{
			AUTO_LOCK( m_OpenedFilesMutex );

			result = m_OpenedFiles.Find( file );
			if ( result != -1 )
			{
				const COpenedFile &found = m_OpenedFiles[ result ];
				if ( m_fwLevel >= FILESYSTEM_WARNING_REPORTALLACCESSES && !V_stristr( found.GetName(), "console.log" ) )
				{
					Warning( FILESYSTEM_WARNING_REPORTALLACCESSES, "---FS%s:  close %s %p %zd (%.3f)\n", ThreadInMainThread() ? "" : "[a]", found.GetName(), fp, m_OpenedFiles.Count(), Plat_FloatTime() );
				}
				m_OpenedFiles.Remove( result );
			}
		}

		if (result == -1)
		{
			Assert( 0 );

			if ( m_fwLevel >= FILESYSTEM_WARNING_REPORTALLACCESSES )
			{
				Warning( FILESYSTEM_WARNING_REPORTALLACCESSES, "Tried to close unknown file pointer %p\n", fp );
			}
		}

		FS_fclose( fp );
	}
}


void CBaseFileSystem::Trace_FRead( int size, FILE* fp )
{
	if ( !fp || m_fwLevel < FILESYSTEM_WARNING_REPORTALLACCESSES_READ )
		return;

	AUTO_LOCK( m_OpenedFilesMutex );

	COpenedFile file;
	file.m_pFile = fp;

	intp result = m_OpenedFiles.Find( file );

	if( result != -1 )
	{
		COpenedFile found = m_OpenedFiles[ result ];
		
		Warning( FILESYSTEM_WARNING_REPORTALLACCESSES_READ, "---FS%s:  read %s %i %p (%.3f)\n", ThreadInMainThread() ? "" : "[a]", found.GetName(), size, fp, Plat_FloatTime()  );
	} 
	else
	{
		Warning( FILESYSTEM_WARNING_REPORTALLACCESSES_READ, "Tried to read %i bytes from unknown file pointer %p\n", size, fp );
		
	}
}

void CBaseFileSystem::Trace_FWrite( int size, FILE* fp )
{
	if ( !fp || m_fwLevel < FILESYSTEM_WARNING_REPORTALLACCESSES_READWRITE )
		return;

	COpenedFile file;
	file.m_pFile = fp;

	AUTO_LOCK( m_OpenedFilesMutex );

	intp result = m_OpenedFiles.Find( file );

	if( result != -1 )
	{
		COpenedFile found = m_OpenedFiles[ result ];

		Warning( FILESYSTEM_WARNING_REPORTALLACCESSES_READWRITE, "---FS%s:  write %s %i %p\n", ThreadInMainThread() ? "" : "[a]", found.GetName(), size, fp  );
	} 
	else
	{
		Warning( FILESYSTEM_WARNING_REPORTALLACCESSES_READWRITE, "Tried to write %i bytes from unknown file pointer %p\n", size, fp );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::Trace_DumpUnclosedFiles( void )
{
	/*
	AUTO_LOCK( m_OpenedFilesMutex );
	for ( intp i = 0 ; i < m_OpenedFiles.Count(); i++ )
	{
		COpenedFile *found = &m_OpenedFiles[ i ];

		if ( m_fwLevel >= FILESYSTEM_WARNING_REPORTUNCLOSED )
		{
			// This warning can legitimately trigger because the log file is, by design, not
			// closed. It is not closed at shutdown in order to avoid race conditions. It is
			// not closed at each call to log information because the performance costs,
			// especially if Microsoft Security Essentials is running, are extreme.
			// In addition, when this warning triggers it can, due to the state of the process,
			// actually cause a crash.
			Warning( FILESYSTEM_WARNING_REPORTUNCLOSED, "File %s was never closed\n", found->GetName() );
		}
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::PrintOpenedFiles( void )
{
  FileWarningLevel_t saveLevel = std::exchange(m_fwLevel, FILESYSTEM_WARNING_REPORTUNCLOSED);
	Trace_DumpUnclosedFiles();
	m_fwLevel = saveLevel;
}

#if defined( SUPPORT_PACKED_STORE )

CPackedStoreRefCount::CPackedStoreRefCount( char const *pFileBasename, char *pszFName, intp fnameSize, IBaseFileSystem *pFS )
: CPackedStore( pFileBasename, pszFName, fnameSize, pFS, false )
{
	// If the VPK is signed, check the signature
	//
	// !FIXME! This code is simple a linchpin that a hacker could detour to bypass sv_pure
	#ifdef VPK_ENABLE_SIGNING
		CPackedStore::ESignatureCheckResult eSigResult = CPackedStore::CheckSignature( 0, nullptr );
		m_bSignatureValid = ( eSigResult == CPackedStore::eSignatureCheckResult_ValidSignature );
	#else
		m_bSignatureValid = false;
	#endif
}

#endif

void CBaseFileSystem::AddVPKFile( char const *pPath, const char *pPathID, SearchPathAdd_t addType )
{
#if defined( SUPPORT_PACKED_STORE )
	char nameBuf[MAX_PATH];

	V_MakeAbsolutePath( nameBuf, pPath );
	V_FixSlashes( nameBuf );

	CUtlSymbol pathIDSym = g_PathIDTable.AddString( pPathID );

	// See if we already have this vpk file as a search path
	CPackedStoreRefCount *pVPK = nullptr;
	for ( auto &s : m_SearchPaths )
	{
		CPackedStoreRefCount *p = s.GetPackedStore();
		if ( p && V_stricmp( p->FullPathName(), nameBuf ) == 0 )
		{
			// Already present
			if ( s.GetPath() == pathIDSym )
				return;

			// Present, but for a different path
			pVPK = p;
		}
	}

	// Create a new VPK if we didn't don't already have it opened
	if ( pVPK == nullptr )
	{
		char pszFName[MAX_PATH];
		pVPK = new CPackedStoreRefCount( nameBuf, pszFName, ssize(pszFName), this ); 
		if ( pVPK->IsEmpty() )
		{
			delete pVPK;
			return;
		}
		pVPK->RegisterFileTracker( (IThreadedFileMD5Processor *)&m_FileTracker2 );

		pVPK->m_PackFileID = m_FileTracker2.NotePackFileOpened( pVPK->FullPathName(), pPathID, 0 );
	}
	else
	{
		pVPK->AddRef();
	}

	// Crete a search path for this
	CSearchPath *sp = &m_SearchPaths[ ( addType == PATH_ADD_TO_TAIL ) ? m_SearchPaths.AddToTail() : m_SearchPaths.AddToHead() ];
	sp->SetPackedStore( pVPK );
	sp->m_storeId = g_iNextSearchPathID++;
	sp->SetPath( pathIDSym );
	sp->m_pPathIDInfo = FindOrAddPathIDInfo( g_PathIDTable.AddString( pPathID ), -1 );

	// Check if we're trusted or not
	SetSearchPathIsTrustedSource( sp );
#endif // SUPPORT_PACKED_STORE
}


bool CBaseFileSystem::RemoveVPKFile( const char *pPath, const char *pPathID )
{
#if defined( SUPPORT_PACKED_STORE )
	char nameBuf[MAX_PATH];

	V_MakeAbsolutePath( nameBuf, pPath );
	V_FixSlashes( nameBuf );

	CUtlSymbol pathIDSym = g_PathIDTable.AddString( pPathID );

	// See if we already have this vpk file as a search path
	for ( intp i = 0; i < m_SearchPaths.Count(); i++ )
	{
		CPackedStoreRefCount *p = m_SearchPaths[i].GetPackedStore();
		if ( p && V_stricmp( p->FullPathName(), nameBuf ) == 0 )
		{
			// remove if we find one
			if ( m_SearchPaths[i].GetPath() == pathIDSym )
			{
				m_SearchPaths.Remove( i );
				return true;
			}
		}
	}
#endif // SUPPORT_PACKED_STORE

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Adds the specified pack file to the list
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::AddPackFile( const char *pFileName, const char *pathID )
{
	CHECK_DOUBLE_SLASHES( pFileName );

	AsyncFinishAll();
	return AddPackFileFromPath( "", pFileName, true, pathID );
}

//-----------------------------------------------------------------------------
// Purpose: Adds a pack file from the specified path
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::AddPackFileFromPath( const char *pPath, const char *pakfile, bool bCheckForAppendedPack, const char *pathID )
{
	char fullpath[ MAX_PATH ];
	V_sprintf_safe( fullpath, "%s%s", pPath, pakfile );
	Q_FixSlashes( fullpath );

	struct	_stat buf;
	if ( FS_stat( fullpath, &buf ) == -1 )
		return false;

	CPackFile *pf = new CZipPackFile( this );
	pf->m_hPackFileHandleFS = Trace_FOpen( fullpath, "rb", 0, nullptr );
	if ( !pf->m_hPackFileHandleFS )
	{
		delete pf;
		return false;
	}

	// NOTE: Opening .bsp fields inside of VPK files is not supported

	// Get the length of the pack file:
	// dimhotepus: FS_fseek now returns offset.
	int64 len = FS_fseek( pf->m_hPackFileHandleFS, 0, FILESYSTEM_SEEK_TAIL );
	FS_fseek( pf->m_hPackFileHandleFS, 0, FILESYSTEM_SEEK_HEAD );

	if ( !pf->Prepare( len ) )
	{
		// Failed for some reason, ignore it
		Trace_FClose( pf->m_hPackFileHandleFS );
		pf->m_hPackFileHandleFS = nullptr;
		delete pf;

		return false;
	}

	// Add this pack file to the search path:
	CSearchPath *sp = &m_SearchPaths[ m_SearchPaths.AddToTail() ];
	pf->SetPath( sp->GetPath() );
	pf->m_lPackFileTime = GetFileTime( pakfile );

	sp->SetPath( pPath );
	sp->m_pPathIDInfo->SetPathID( pathID );
	sp->SetPackFile( pf );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Search pPath for pak?.pak files and add to search path if found
// Input  : *pPath - 
//-----------------------------------------------------------------------------
#if !defined( _X360 )
#define PACK_NAME_FORMAT "zip%i.zip"
#else
#define PACK_NAME_FORMAT "zip%i.360.zip"
#define PACK_LOCALIZED_NAME_FORMAT "zip%i_%s.360.zip"
#endif

void CBaseFileSystem::AddPackFiles( const char *pPath, const CUtlSymbol &pathID, SearchPathAdd_t addType )
{
	Assert( ThreadInMainThread() );
	DISK_INTENSIVE();

	CUtlVector< CUtlString > pakNames;
	CUtlVector< int64 > pakSizes;

	// determine pak files, [zip0..zipN]
	for ( int i = 0; ; i++ )
	{
		char pakfile[MAX_PATH];
		char fullpath[MAX_PATH];
		V_snprintf( pakfile, sizeof( pakfile ), PACK_NAME_FORMAT, i );
		V_ComposeFileName( pPath, pakfile, fullpath );

		struct _stat buf;
		if ( FS_stat( fullpath, &buf ) == -1 )
			break;

		MEM_ALLOC_CREDIT();

		pakNames.AddToTail( pakfile );
		pakSizes.AddToTail( (int64)((unsigned int)buf.st_size) );
	}

#if defined( _X360 )
	// localized zips are added last to ensure they appear first in the search path construction
	// localized zips can only appear in the game or mod directories
	bool bUseEnglishAudio = XboxLaunch()->GetForceEnglish();

	if ( XBX_IsLocalized() && ( bUseEnglishAudio == false ) && 
		 ( V_stricmp( g_PathIDTable.String( pathID ), "game" ) == 0 || V_stricmp( g_PathIDTable.String( pathID ), "mod" ) == 0 ) )
	{
		// determine localized pak files, [zip0_language..zipN_language]
		for ( int i = 0; ; i++ )
		{
			char pakfile[MAX_PATH];
			char fullpath[MAX_PATH];
			V_snprintf( pakfile, sizeof( pakfile ), PACK_LOCALIZED_NAME_FORMAT, i, XBX_GetLanguageString() );
			V_ComposeFileName( pPath, pakfile, fullpath );

			struct _stat buf;
			if ( FS_stat( fullpath, &buf ) == -1 )
				break;

			pakNames.AddToTail( pakfile );
			pakSizes.AddToTail( (int64)((unsigned int)buf.st_size) );
		}
	}
#endif

	// Add any zip files in the format zip1.zip ... zip0.zip
	// Add them backwards so zip(N) is higher priority than zip(N-1), etc.
	intp pakcount = pakSizes.Count();
	intp nCount = 0;
	for ( intp i = pakcount-1; i >= 0; i-- )
	{
		char fullpath[MAX_PATH];
		V_ComposeFileName( pPath, pakNames[i].Get(), fullpath );

		intp nIndex;
		if ( addType == PATH_ADD_TO_TAIL )
		{
			nIndex = m_SearchPaths.AddToTail();
		}
		else
		{
			nIndex = m_SearchPaths.InsertBefore( nCount );
			++nCount;
		}

		CSearchPath *sp = &m_SearchPaths[ nIndex ];
		
		sp->m_pPathIDInfo = FindOrAddPathIDInfo( pathID, -1 );
		sp->m_storeId = g_iNextSearchPathID++;
		sp->SetPath( g_PathIDTable.AddString( pPath ) );

		CPackFile *pf = nullptr;
		for ( auto *p : m_ZipFiles )
		{
			if ( !Q_stricmp( p->m_ZipName.Get(), fullpath ) )
			{
				pf = p;
				sp->SetPackFile( pf );
				pf->AddRef();
			}
		}

		if ( !pf )
		{
			MEM_ALLOC_CREDIT();

			pf = new CZipPackFile( this );

			pf->SetPath( sp->GetPath() );
			pf->m_ZipName = fullpath;

			m_ZipFiles.AddToTail( pf );
			sp->SetPackFile( pf );
			pf->m_lPackFileTime = GetFileTime( fullpath );

			pf->m_hPackFileHandleFS = Trace_FOpen( fullpath, "rb", 0, nullptr );

			if ( pf->m_hPackFileHandleFS )
			{
				FS_setbufsize( pf->m_hPackFileHandleFS, 32*1024 );	// 32k buffer.

				if ( pf->Prepare( pakSizes[i] ) )
				{
					FS_setbufsize( pf->m_hPackFileHandleFS, filesystem_buffer_size.GetInt() );
				}
				else
				{
					// Failed for some reason, ignore it
					if ( pf->m_hPackFileHandleFS )
					{
						Trace_FClose( pf->m_hPackFileHandleFS );
						pf->m_hPackFileHandleFS = nullptr;
					}
					m_SearchPaths.Remove( nIndex );
				}
			}
			else
			{
				// !NOTE! Pack files inside of VPK not supported
				//pf->m_hPackFileHandleVPK = FindFileInVPKs( fullpath );
				//if ( !pf->m_hPackFileHandleVPK || !pf->Prepare( pakSizes[i] ) )
				{
					m_SearchPaths.Remove( nIndex );
				} 
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Wipe all map (.bsp) pak file search paths
//-----------------------------------------------------------------------------
void CBaseFileSystem::RemoveAllMapSearchPaths( void )
{
	AsyncFinishAll();

	intp c = m_SearchPaths.Count();
	for ( intp i = c - 1; i >= 0; i-- )
	{
		if ( !( m_SearchPaths[i].GetPackFile() && m_SearchPaths[i].GetPackFile()->m_bIsMapPath ) )
		{
			continue;
		}
		
		m_SearchPaths.Remove( i );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::AddMapPackFile( const char *pPath, const char *pPathID, SearchPathAdd_t addType )
{
	char tempPathID[MAX_PATH];
	ParsePathID( pPath, pPathID, tempPathID );

	// Security nightmares already, should not let things explicitly loading from e.g. "MOD" get surprise untrusted
	// files unless you really really know what you're doing.
	AssertMsg( V_strcasecmp( pPathID, "GAME" ) == 0,
	           "Mounting map files anywhere besides GAME is asking for pain" );

	char newPath[ MAX_FILEPATH ];
	// +2 for '\0' and potential slash added at end.
	V_strcpy_safe( newPath, pPath );
#ifdef _WIN32 // don't do this on linux!
	V_strlower( newPath );
#endif
	V_FixSlashes( newPath );

	// Open the .bsp and find the map lump
	char fullpath[ MAX_FILEPATH ];
	if ( V_IsAbsolutePath( newPath ) ) // If it's an absolute path, just use that.
	{
		V_strcpy_safe( fullpath, newPath );
	}
	else
	{
		if ( !GetLocalPath_safe( newPath, fullpath ) )
		{
			// Couldn't find that .bsp file!!!
			return;
		}
	}

	for ( auto &sp : m_SearchPaths )
	{
		if ( !( sp.GetPackFile() && sp.GetPackFile()->m_bIsMapPath ) )
			continue;
		
		if ( V_stricmp( sp.GetPackFile()->m_ZipName.Get(), fullpath ) == 0 )
		{
			// Already set as map path
			return;
		}
	}

	RemoveAllMapSearchPaths();

	CUtlSymbol pathSymbol = g_PathIDTable.AddString( newPath );

	int iStoreId = CRC32_ProcessSingleBuffer( fullpath, V_strlen(fullpath) ) | 0x80000000; // set store ID based on .bsp filename, so if we remount the same map file, it will have the same storeid

	// Look through already-open packfiles in case we intentionally
	// preserved this ZIP across a map reload via refcount holding
	for ( auto *pf : m_ZipFiles )
	{
		if ( pf && pf->m_bIsMapPath && pf->GetPath() == pathSymbol && V_stricmp( pf->m_ZipName.Get(), fullpath ) == 0 )
		{
			CSearchPath *sp = &m_SearchPaths[ ( addType == PATH_ADD_TO_TAIL ) ? m_SearchPaths.AddToTail() : m_SearchPaths.AddToHead() ];
			pf->AddRef();
			sp->SetPackFile( pf );
			sp->m_storeId = iStoreId;
			sp->SetPath( pathSymbol );
			sp->m_pPathIDInfo = FindOrAddPathIDInfo( g_PathIDTable.AddString( pPathID ), -1 );
			SetSearchPathIsTrustedSource( sp );
			return;
		}
	}

	{
		FILE *fp = Trace_FOpen( fullpath, "rb", 0, nullptr );
		if ( !fp )
		{
			// Couldn't open it
			Warning( FILESYSTEM_WARNING, "Couldn't open .bsp %s for embedded pack file check.\n", fullpath );
			return;
		}
	
		// Get the .bsp file header
		dheader_t header;
		memset( &header, 0, sizeof(dheader_t) );
		m_Stats.nBytesRead += FS_fread( &header, sizeof( header ), fp );
		++m_Stats.nReads;
	
		if ( header.ident != IDBSPHEADER || header.version < MINBSPVERSION || header.version > BSPVERSION )
		{
			Trace_FClose( fp );
			return;
		}
	
		// Find the LUMP_PAKFILE offset
		lump_t *packfile = &header.lumps[ LUMP_PAKFILE ];
		if ( packfile->filelen <= static_cast<int>(sizeof( lump_t )) )
		{
			// It's empty or only contains a file header ( so there are no entries ), so don't add to search paths
			Trace_FClose( fp );
			return;
		}
	
		// Seek to correct position
		FS_fseek( fp, packfile->fileofs, FILESYSTEM_SEEK_HEAD );
	
		CPackFile *pf = new CZipPackFile( this );
	
		pf->m_bIsMapPath = true;
		pf->m_hPackFileHandleFS = fp;

		MEM_ALLOC_CREDIT();
		pf->m_ZipName = fullpath;
	
		if ( pf->Prepare( packfile->filelen, packfile->fileofs ) )
		{
			intp nIndex;
			if ( addType == PATH_ADD_TO_TAIL )
			{
				nIndex = m_SearchPaths.AddToTail();	
			}
			else
			{
				nIndex = m_SearchPaths.AddToHead();	
			}
	
			CSearchPath *sp = &m_SearchPaths[ nIndex ];
	
			sp->SetPackFile( pf );
			sp->m_storeId = iStoreId;
			sp->SetPath( pathSymbol );
			sp->m_pPathIDInfo = FindOrAddPathIDInfo( g_PathIDTable.AddString( pPathID ), -1 );
	
			pf->SetPath( pathSymbol );
			pf->m_lPackFileTime = GetFileTime( newPath );
	
			Trace_FClose( pf->m_hPackFileHandleFS );
			pf->m_hPackFileHandleFS = nullptr;

			m_ZipFiles.AddToTail( pf );

			SetSearchPathIsTrustedSource( sp );
		}
		else
		{
			delete pf;
		}
	}
}


void CBaseFileSystem::BeginMapAccess() 
{
	if ( m_iMapLoad++ == 0 )
	{
		for( auto &sp : m_SearchPaths )
		{
			CPackFile *pPackFile = sp.GetPackFile();

			if ( pPackFile && pPackFile->m_bIsMapPath )
			{
				pPackFile->AddRef();
				AUTO_LOCK(pPackFile->m_mutex);

#if defined( SUPPORT_PACKED_STORE )
				if ( pPackFile->m_nOpenFiles == 0 && pPackFile->m_hPackFileHandleFS == nullptr && !pPackFile->m_hPackFileHandleVPK )
#else
				if ( pPackFile->m_nOpenFiles == 0 && pPackFile->m_hPackFileHandleFS == nullptr )
#endif
				{
					// Try opening the file as a regular file 
					pPackFile->m_hPackFileHandleFS = Trace_FOpen( pPackFile->m_ZipName, "rb", 0, nullptr );

// !NOTE! Pack files inside of VPK not supported
//#if defined( SUPPORT_PACKED_STORE )
//					if ( !pPackFile->m_hPackFileHandleFS )
//					{
//						pPackFile->m_hPackFileHandleVPK = FindFileInVPKs( pPackFile->m_ZipName );
//					}
//#endif
				}
				pPackFile->m_nOpenFiles++;
			}
		}
	}
}


void CBaseFileSystem::EndMapAccess() 
{
	if ( m_iMapLoad-- == 1 )
	{
		for( auto &sp : m_SearchPaths )
		{
			CPackFile *pPackFile = sp.GetPackFile();

			if ( pPackFile && pPackFile->m_bIsMapPath )
			{
				AUTO_LOCK(pPackFile->m_mutex);
				if ( --pPackFile->m_nOpenFiles == 0  )
				{
					if ( pPackFile->m_hPackFileHandleFS )
					{
						Trace_FClose( pPackFile->m_hPackFileHandleFS );
						pPackFile->m_hPackFileHandleFS = nullptr;
					}
				}
				pPackFile->Release();
			}
		}
	}
}


void CBaseFileSystem::PrintSearchPaths( void )
{
	Msg( "---------------\n" );
	Msg( "Paths:\n" );

	for( auto &sp : m_SearchPaths )
	{
		const char *pszPack = "";
		const char *pszType = "";
		if ( sp.GetPackFile() && sp.GetPackFile()->m_bIsMapPath )
		{
			pszType = "(map)";
		}
		else if ( sp.GetPackFile()  )
		{
			pszType = "(pack) ";
			pszPack = sp.GetPackFile()->m_ZipName;
		}
		#ifdef SUPPORT_PACKED_STORE
			else if ( sp.GetPackedStore()  )
			{
				pszType = "(VPK)";
				pszPack = sp.GetPackedStore()->FullPathName();
			}
		#endif

		Msg( "\"%s\" \"%s\" %s%s\n", sp.GetPathString(), sp.GetPathIDString(), pszType, pszPack );
	}
}


//-----------------------------------------------------------------------------
// Purpose: This is where search paths are created.  map files are created at head of list (they occur after
//  file system paths have already been set ) so they get highest priority.  Otherwise, we add the disk (non-packfile)
//  path and then the paks if they exist for the path
// Input  : *pPath - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::AddSearchPathInternal( const char *pPath, const char *pathID, SearchPathAdd_t addType, bool bAddPackFiles )
{
	AsyncFinishAll();

	Assert( ThreadInMainThread() );

	// Map pak files have their own handler
	if ( V_stristr( pPath, ".bsp" ) )
	{
		AddMapPackFile( pPath, pathID, addType );
		return;
	}

	// So do VPK files
	if ( V_stristr( pPath, ".vpk" ) )
	{
		AddVPKFile( pPath, pathID, addType );
		return;
	}

	// Clean up the name
	char newPath[ MAX_FILEPATH ];
	if ( Q_isempty( pPath ) )
	{
		newPath[0] = newPath[1] = '\0';
	}
	else
	{
		if ( IsX360() || Q_IsAbsolutePath( pPath ) )
		{
			V_strcpy_safe( newPath, pPath );
		}
		else
		{
			V_MakeAbsolutePath( newPath, pPath );
		}
#ifdef _WIN32
		Q_strlower( newPath );
#endif
		AddSeperatorAndFixPath( newPath );
	}

	// dimhotepus: As Raphael suggested, cache base path here.
	if ( V_strcmp( pathID, "BASE_PATH" ) == 0 )
	{
		V_strcpy_safe( m_pBaseDir, newPath );
		m_iBaseLength = V_strlen( m_pBaseDir );
	}
	
	// Make sure that it doesn't already exist
	CUtlSymbol pathSym = g_PathIDTable.AddString( newPath );
	CUtlSymbol pathIDSym = g_PathIDTable.AddString( pathID );
	intp c = m_SearchPaths.Count();
	int id = 0;
	for ( intp i = 0; i < c; i++ )
	{
		CSearchPath *pSearchPath = &m_SearchPaths[i];
		if ( pSearchPath->GetPath() == pathSym && pSearchPath->GetPathID() == pathIDSym )
		{
			if ( ( addType == PATH_ADD_TO_HEAD && i == 0 ) || ( addType == PATH_ADD_TO_TAIL ) )
			{
				return; // this entry is already at the head
			}
			else
			{
				m_SearchPaths.Remove(i); // remove it from its current position so we can add it back to the head
				i--;
				c--;
			}
		}
		if ( !id && pSearchPath->GetPath() == pathSym )
		{
			// get first found - all reference the same store
			id = pSearchPath->m_storeId;
		}
	}

	if (!id)
	{
		id = g_iNextSearchPathID++;
	}

	// Add to list
	bool bAdded = false;
	intp nIndex = m_SearchPaths.Count();
	if ( bAddPackFiles )
	{
		// Add pack files for this path next
		AddPackFiles( newPath, pathIDSym, addType );
		bAdded = m_SearchPaths.Count() != nIndex;
	}
	if ( addType == PATH_ADD_TO_HEAD )
	{
		nIndex = m_SearchPaths.Count() - nIndex;
		Assert( nIndex >= 0 );
	}

	// Grab last entry and set the path
	m_SearchPaths.InsertBefore( nIndex );

	CSearchPath *sp = &m_SearchPaths[ nIndex ];
	
	sp->SetPath( pathSym );
	sp->m_pPathIDInfo = FindOrAddPathIDInfo( pathIDSym, -1 );

	// all matching paths have a reference to the same store
	sp->m_storeId = id;
}

//-----------------------------------------------------------------------------
CBaseFileSystem::CSearchPath *CBaseFileSystem::FindSearchPathByStoreId( int storeId )
{
	for ( auto &s : m_SearchPaths )
	{
		if ( s.m_storeId == storeId )
			return &s;
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Create the search path.
//-----------------------------------------------------------------------------
void CBaseFileSystem::AddSearchPath( const char *pPath, const char *pathID, SearchPathAdd_t addType )
{
	[[maybe_unused]] const intp oldPathsCount = m_SearchPaths.Count();

	AddSearchPathInternal( pPath, pathID, addType, true );

#ifdef _DEBUG
	if ( oldPathsCount != m_SearchPaths.Count() )
	{
#if !defined( DEDICATED )
		// spew updated search paths
		// PrintSearchPaths();
#endif
	}
#endif
}

//-----------------------------------------------------------------------------
// Returns the search path, each path is separated by ;s. Returns the length of the string returned
// Pack search paths include the pack name, so that callers can still form absolute paths
// and that absolute path can be sent to the filesystem, and mounted as a file inside a pack.
//-----------------------------------------------------------------------------
int CBaseFileSystem::GetSearchPath( const char *pathID, bool bGetPackFiles, OUT_Z_CAP(maxLenInChars) char *pDest, intp maxLenInChars )
{
	AUTO_LOCK( m_SearchPathsMutex );

	// Build up result into string object
	// dimhotepus: Use std::string and reserve capacity. 
	std::string sResult;
	sResult.reserve(maxLenInChars);

	CSearchPathsIterator iter( this, pathID, bGetPackFiles ? FILTER_NONE : FILTER_CULLPACK );
	for ( CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != nullptr; pSearchPath = iter.GetNext() )
	{
		if ( !sResult.empty() )
			sResult += ";";
		if ( pSearchPath->GetPackFile() )
		{
			sResult += pSearchPath->GetPackFile()->m_ZipName.String();
			sResult += CORRECT_PATH_SEPARATOR_S;
		}
#ifdef SUPPORT_PACKED_STORE
		else if ( pSearchPath->GetPackedStore() )
		{
			sResult += pSearchPath->GetPackedStore()->FullPathName();
		}
#endif
		else
		{
			sResult += pSearchPath->GetPathString();
		}
	}

	// Copy into user's buffer, possibly truncating
	if ( maxLenInChars )
	{
		V_strncpy( pDest, sResult.c_str(), maxLenInChars );
	}

	// Return 1 extra for the nullptr terminator
	return sResult.size()+1;
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPath - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::RemoveSearchPath( const char *pPath, const char *pathID )
{
	AsyncFinishAll();

	char newPath[ MAX_FILEPATH ];
	newPath[ 0 ] = 0;

	if ( pPath )
	{
		// +2 for '\0' and potential slash added at end.
		V_strcpy_safe( newPath, pPath );
#ifdef _WIN32 // don't do this on linux!
		Q_strlower( newPath );
#endif

		if ( V_stristr( newPath, ".bsp" ) )
		{
			Q_FixSlashes( newPath );
		}
		else if ( V_stristr( newPath, ".vpk" ) )
		{
			return RemoveVPKFile( newPath, pathID );
		}
		else
		{
			AddSeperatorAndFixPath( newPath );
		}
	}

	CUtlSymbol lookup = g_PathIDTable.AddString( newPath );
	CUtlSymbol id = g_PathIDTable.AddString( pathID );

	bool bret = false;

	// Count backward since we're possibly deleting one or more pack files, too
	intp i;
	intp c = m_SearchPaths.Count();
	for( i = c - 1; i >= 0; i-- )
	{
		if ( newPath[0] && m_SearchPaths[i].GetPath() != lookup )
			continue;

		if ( FilterByPathID( &m_SearchPaths[i], id ) )
			continue;

		m_SearchPaths.Remove( i );
		bret = true;
	}
	return bret;
}


//-----------------------------------------------------------------------------
// Purpose: Removes all search paths for a given pathID, such as all "GAME" paths.
// Input  : pathID - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::RemoveSearchPaths( const char *pathID )
{
	AsyncFinishAll();

	intp nCount = m_SearchPaths.Count();
	for (intp i = nCount - 1; i >= 0; i--)
	{
		if (!Q_stricmp(m_SearchPaths.Element(i).GetPathIDString(), pathID))
		{
			m_SearchPaths.FastRemove(i);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseFileSystem::CSearchPath *CBaseFileSystem::FindWritePath( const char *pFilename, const char *pathID )
{
	CUtlSymbol lookup = g_PathIDTable.AddString( pathID );

	AUTO_LOCK( m_SearchPathsMutex );

	// a pathID has been specified, find the first match in the path list
	intp c = m_SearchPaths.Count();
	for ( intp i = 0; i < c; i++ )
	{
		// pak files are not allowed to be written to...
		CSearchPath *pSearchPath = &m_SearchPaths[i];
		if ( pSearchPath->GetPackFile() || pSearchPath->GetPackedStore() )
		{
			continue;
		}

		if ( !pathID || ( pSearchPath->GetPathID() == lookup ) )
		{
			return pSearchPath;
		}
	}

	return nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Finds a search path that should be used for writing to, given a pathID.
//-----------------------------------------------------------------------------
const char *CBaseFileSystem::GetWritePath( const char *pFilename, const char *pathID )
{
	CSearchPath *pSearchPath = nullptr;
	if ( pathID && pathID[ 0 ] != '\0' )
	{

		// Check for "game_write" and "mod_write"
		if ( V_stricmp( pathID, "game" ) == 0 )
			pSearchPath = FindWritePath( pFilename, "game_write" );
		else if ( V_stricmp( pathID, "mod" ) == 0 )
			pSearchPath = FindWritePath( pFilename, "mod_write" );

		if ( pSearchPath == nullptr )
			pSearchPath = FindWritePath( pFilename, pathID );

		if ( pSearchPath )
		{
			return pSearchPath->GetPathString();
		}

		Warning( FILESYSTEM_WARNING, "Requested non-existent write path %s!\n", pathID );
	}

	pSearchPath = FindWritePath( pFilename, "DEFAULT_WRITE_PATH" );
	if ( pSearchPath )
	{
		return pSearchPath->GetPathString();
	}

	pSearchPath = FindWritePath( pFilename, nullptr ); // okay, just return the first search path added!
	if ( pSearchPath )
	{
		return pSearchPath->GetPathString();
	}

	// Hope this is reasonable!!
	return ".\\";
}

//-----------------------------------------------------------------------------
// Reads/writes files to utlbuffers.  Attempts alignment fixups for optimal read
//-----------------------------------------------------------------------------
CThreadLocal<const char *> g_pszReadFilename;
bool CBaseFileSystem::ReadToBuffer( FileHandle_t fp, CUtlBuffer &buf, int nMaxBytes, FSAllocFunc_t pfnAlloc )
{
	SetBufferSize( fp, 0 );  // TODO: what if it's a pack file? restore buffer size?

	int nBytesToRead = Size( fp );
	if ( nBytesToRead == 0 )
	{
		// no data in file
		return true;
	}

	if ( nMaxBytes > 0 )
	{
		// can't read more than file has
		nBytesToRead = min( nMaxBytes, nBytesToRead );
	}

	int nBytesRead = 0;
	int nBytesOffset = 0;

	int iStartPos = Tell( fp );

	if ( nBytesToRead != 0 )
	{
		int nBytesDestBuffer = nBytesToRead;
		unsigned nSizeAlign = 0, nBufferAlign = 0, nOffsetAlign = 0;

		bool bBinary = !( buf.IsText() && !buf.ContainsCRLF() );

		if ( bBinary && !IsLinux() && !buf.IsExternallyAllocated() && !pfnAlloc && 
			( buf.TellPut() == 0 ) && ( buf.TellGet() == 0 ) && ( iStartPos % 4 == 0 ) &&
			GetOptimalIOConstraints( fp, &nOffsetAlign, &nSizeAlign, &nBufferAlign ) )
		{
			// correct conditions to allow an optimal read
			if ( iStartPos % nOffsetAlign != 0 )
			{
				// move starting position back to nearest alignment
				nBytesOffset = ( iStartPos % nOffsetAlign );
				Assert ( ( iStartPos - nBytesOffset ) % nOffsetAlign == 0 );
				Seek( fp, -nBytesOffset, FILESYSTEM_SEEK_CURRENT );

				// going to read from aligned start, increase target buffer size by offset alignment
				nBytesDestBuffer += nBytesOffset;
			}

			// snap target buffer size to its size alignment
			// add additional alignment slop for target pointer adjustment
			nBytesDestBuffer = AlignValue( nBytesDestBuffer, nSizeAlign ) + nBufferAlign;
		}

		if ( !pfnAlloc )
		{
			buf.EnsureCapacity( nBytesDestBuffer + buf.TellPut() );
		}
		else
		{
			// caller provided allocator
			void *pMemory = (*pfnAlloc)( g_pszReadFilename.Get(), nBytesDestBuffer );
			buf.SetExternalBuffer( pMemory, nBytesDestBuffer, 0, buf.GetFlags() & ~CUtlBuffer::EXTERNAL_GROWABLE );
		}

		int seekGet = -1;
		if ( nBytesDestBuffer != nBytesToRead )
		{
			// doing optimal read, align target pointer
			intp nAlignedBase = AlignValue( buf.Base<byte>(), nBufferAlign ) - buf.Base<byte>();
			buf.SeekPut( CUtlBuffer::SEEK_HEAD, nAlignedBase );
	
			// the buffer read position is slid forward to ignore the addtional
			// starting offset alignment
			seekGet = nAlignedBase + nBytesOffset;
		}

		nBytesRead = ReadEx( buf.PeekPut(), buf.Size() - buf.TellPut(), nBytesToRead + nBytesOffset, fp );
		buf.SeekPut( CUtlBuffer::SEEK_CURRENT, nBytesRead );

		if ( seekGet != -1 )
		{
			// can only seek the get after data has been put, otherwise buffer sets overflow error
			buf.SeekGet( CUtlBuffer::SEEK_HEAD, seekGet );
		}

		Seek( fp, iStartPos + ( nBytesRead - nBytesOffset ), FILESYSTEM_SEEK_HEAD );
	}

	return (nBytesRead != 0);
}

//-----------------------------------------------------------------------------
// Reads/writes files to utlbuffers
// NOTE NOTE!! 
// If you change this implementation, copy it into CBaseVMPIFileSystem::ReadFile
// in vmpi_filesystem.cpp
//-----------------------------------------------------------------------------
bool CBaseFileSystem::ReadFile( const char *pFileName, const char *pPath, CUtlBuffer &buf, int nMaxBytes, int nStartingByte, FSAllocFunc_t pfnAlloc )
{
	CHECK_DOUBLE_SLASHES( pFileName );

	bool bBinary = !( buf.IsText() && !buf.ContainsCRLF() );

	FileHandle_t fp = Open( pFileName, ( bBinary ) ? "rb" : "rt", pPath );
	if ( !fp )
		return false;

	if ( nStartingByte != 0 )
	{
		Seek( fp, nStartingByte, FILESYSTEM_SEEK_HEAD );
	}

	if ( pfnAlloc )
	{
		g_pszReadFilename.Set( pFileName );
	}

	bool bSuccess = ReadToBuffer( fp, buf, nMaxBytes, pfnAlloc );

	Close( fp );

	return bSuccess;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CBaseFileSystem::ReadFileEx( const char *pFileName, const char *pPath, void **ppBuf, bool bNullTerminate, bool bOptimalAlloc, int nMaxBytes, int nStartingByte, FSAllocFunc_t pfnAlloc )
{
	FileHandle_t fp = Open( pFileName, "rb", pPath );
	if ( !fp )
	{
		return 0;
	}

	if ( IsX360() )
	{
		// callers are sloppy, always want optimal
		bOptimalAlloc = true;
	}

	SetBufferSize( fp, 0 );  // TODO: what if it's a pack file? restore buffer size?

	int nBytesToRead = Size( fp );
	int nBytesRead = 0;
	if ( nMaxBytes > 0 )
	{
		nBytesToRead = min( nMaxBytes, nBytesToRead );
		if ( bNullTerminate )
		{
			nBytesToRead--;
		}
	}

	if ( nBytesToRead != 0 )
	{
		int nBytesBuf;
		if ( !*ppBuf )
		{
			nBytesBuf = nBytesToRead + ( ( bNullTerminate ) ? 1 : 0 );

			if ( !pfnAlloc && !bOptimalAlloc )
			{
				// AllocOptimalReadBuffer does malloc...
				*ppBuf = malloc( nBytesBuf );
			}
			else if ( !pfnAlloc )
			{
				*ppBuf = AllocOptimalReadBuffer( fp, nBytesBuf, 0 );
				nBytesBuf = GetOptimalReadSize( fp, nBytesBuf );
			}
			else
			{
				*ppBuf = (*pfnAlloc)( pFileName, nBytesBuf );
			}
		}
		else
		{
			nBytesBuf = nMaxBytes;
		}

		if ( nStartingByte != 0 )
		{
			Seek( fp, nStartingByte, FILESYSTEM_SEEK_HEAD );
		}

		nBytesRead = ReadEx( *ppBuf, nBytesBuf, nBytesToRead, fp );

		if ( bNullTerminate )
		{
			((byte *)(*ppBuf))[nBytesToRead] = 0;
		}
	}

	Close( fp );
	return nBytesRead;
}


//-----------------------------------------------------------------------------
// NOTE NOTE!! 
// If you change this implementation, copy it into CBaseVMPIFileSystem::WriteFile
// in vmpi_filesystem.cpp
//-----------------------------------------------------------------------------
bool CBaseFileSystem::WriteFile( const char *pFileName, const char *pPath, CUtlBuffer &buf )
{
	CHECK_DOUBLE_SLASHES( pFileName );

	const char *pWriteFlags = "wb";
	if ( buf.IsText() && !buf.ContainsCRLF() )
	{
		pWriteFlags = "wt";
	}

	FileHandle_t fp = Open( pFileName, pWriteFlags, pPath );
	if ( !fp )
		return false;

	int nBytesWritten = Write( buf.Base(), buf.TellPut(), fp );

	Close( fp );
	return (nBytesWritten == buf.TellPut());
}


bool CBaseFileSystem::UnzipFile( const char *pFileName, const char *pPath, const char *pDestination )
{
	IZip *pZip = IZip::CreateZip( nullptr, true );

	HANDLE hZipFile = pZip->ParseFromDisk( pFileName );
	if ( !hZipFile )
	{
		Msg( "Bad or missing zip file, failed to open '%s'\n", pFileName );
		return false;
	}

	int iZipIndex = -1;
	int iFileSize;
	char szFileName[MAX_PATH];

	// Create Directories
	CreateDirHierarchy( pDestination, pPath );

	while ( 1 )
	{
		// Get the next file in the zip
		szFileName[0] = '\0';
		iFileSize = 0;
		iZipIndex = pZip->GetNextFilename( iZipIndex, szFileName, sizeof( szFileName ), iFileSize );

		// If there aren't any more files then break out of this while
		if ( iZipIndex == -1 )
			break;

		intp iFileNameLength = Q_strlen( szFileName );
		if ( szFileName[ iFileNameLength - 1 ] == '/' )
		{
			// Its a directory, so create it
			szFileName[ iFileNameLength - 1 ] = '\0';

			char szFinalName[ MAX_PATH ];
			Q_snprintf( szFinalName, sizeof( szFinalName ), "%s%c%s", pDestination, CORRECT_PATH_SEPARATOR, szFileName );
			CreateDirHierarchy( szFinalName, pPath );
		}
	}

	// Write Files
	while ( 1 )
	{
		szFileName[0] = '\0';
		iFileSize = 0;
		iZipIndex = pZip->GetNextFilename( iZipIndex, szFileName, sizeof( szFileName ), iFileSize );

		// If there aren't any more files then break out of this while
		if ( iZipIndex == -1 )
			break;

		intp iFileNameLength = Q_strlen( szFileName );
		if ( szFileName[ iFileNameLength - 1 ] != '/' )
		{
			// It's not a directory, so write the file
			CUtlBuffer fileBuffer;
			fileBuffer.Purge();

			if ( pZip->ReadFileFromZip( hZipFile, szFileName, false, fileBuffer ) )
			{
				char szFinalName[ MAX_PATH ];
				Q_snprintf( szFinalName, sizeof( szFinalName ), "%s%c%s", pDestination, CORRECT_PATH_SEPARATOR, szFileName );

				// Make sure the directory actually exists in case the ZIP doesn't list it (our zip utils create zips like this)
				char szFilePath[ MAX_PATH ];
				Q_strncpy( szFilePath, szFinalName, sizeof(szFilePath) );
				V_StripFilename( szFilePath );
				CreateDirHierarchy( szFilePath, pPath );

				WriteFile( szFinalName, pPath, fileBuffer );
			}
		}
	}

#ifdef WIN32
	::CloseHandle( hZipFile );
#else
	fclose( (FILE *)hZipFile );
#endif

	IZip::ReleaseZip( pZip );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::RemoveAllSearchPaths( void )
{
	AUTO_LOCK( m_SearchPathsMutex );
	m_SearchPaths.Purge();
}


void CBaseFileSystem::LogFileAccess( const char *pFullFileName )
{
	if( !m_pLogFile )
	{
		return;
	}

	char buf[1024];
#if BSPOUTPUT
	V_sprintf_safe( buf, "%s\n%s\n", pShortFileName, pFullFileName);
	fprintf( m_pLogFile, "%s", buf ); // STEAM OK
#else
	char cwd[MAX_FILEPATH];
	if( !getcwd( cwd, MAX_FILEPATH-1 ) ) 
	{
		// dimhotepus: Warn if getcwd failed.
		Warning(FileWarningLevel_t::FILESYSTEM_WARNING, "Enable to get cd: %s.\n", strerror(errno));
		return;
	}
	V_strcat_safe( cwd, "\\" );
	const size_t cwdLen = strlen( cwd );
	if( Q_strnicmp( cwd, pFullFileName, cwdLen ) == 0 )
	{
		const char *pFileNameWithoutExeDir = pFullFileName + cwdLen;

		char targetPath[ MAX_FILEPATH ];
		V_strcpy_safe( targetPath, "%fs_target%\\" );
		V_strcat_safe( targetPath, pFileNameWithoutExeDir );
		V_sprintf_safe( buf, "copy \"%s\" \"%s\"\n", pFullFileName, targetPath );

		char tmp[ MAX_FILEPATH ];
		V_strcpy_safe( tmp, targetPath );
		V_StripFilename( tmp );

		fprintf( m_pLogFile, "mkdir \"%s\"\n", tmp ); // STEAM OK
		fprintf( m_pLogFile, "%s", buf ); // STEAM OK
	}
	else
	{
		Assert( 0 );
	}
#endif
}

class CPackedStore;

class CFileOpenInfo
{
public:
	CFileOpenInfo( CBaseFileSystem *pFileSystem, const char *pFileName, const CBaseFileSystem::CSearchPath *path, const char *pOptions, int flags, char **ppszResolvedFilename ) : 
		m_pFileSystem( pFileSystem ), m_ppszResolvedFilename( ppszResolvedFilename ), m_pFileName( pFileName ), m_pSearchPath( path ), m_pOptions( pOptions ), m_Flags( flags )
	{
		m_pFileHandle = nullptr;
		m_ePureFileClass = ePureServerFileClass_Any;
		if ( m_pFileSystem->m_pPureServerWhitelist )
		{
			m_ePureFileClass = m_pFileSystem->m_pPureServerWhitelist->GetFileClass( pFileName );
		}

		if ( m_ppszResolvedFilename )
			*m_ppszResolvedFilename = nullptr;

		m_pPackFile = nullptr;
		m_pVPKFile = nullptr;
		m_AbsolutePath[0] = '\0';
	}
	
	~CFileOpenInfo()
	{
	}
	
	void SetAbsolutePath( const char *pFormat, ... )
	{
		va_list marker;
		va_start( marker, pFormat ); //-V2018 //-V2019
		V_vsprintf_safe( m_AbsolutePath, pFormat, marker );
		va_end( marker );

		V_FixSlashes( m_AbsolutePath );
	}
	
	void SetResolvedFilename( const char *pStr )
	{
		if ( m_ppszResolvedFilename )
		{
			Assert( !( *m_ppszResolvedFilename ) );
			*m_ppszResolvedFilename = strdup( pStr );
		}
	}

	// Handles telling CFileTracker about the file we just opened so it can remember
	// where the file came from, and possibly calculate a CRC if necessary.
	void HandleFileCRCTracking( const char *pRelativeFileName )
	{
		if ( m_pFileSystem->m_WhitelistFileTrackingEnabled == 0 )
			return;

		if ( m_pFileHandle && m_pSearchPath )
		{
			FILE *fp = m_pFileHandle->m_pFile;
			int64 nLength = m_pFileHandle->m_nLength;
#ifdef SUPPORT_PACKED_STORE
			if ( m_pVPKFile )
			{
				m_pFileSystem->m_FileTracker2.NotePackFileAccess( pRelativeFileName, m_pSearchPath->GetPathIDString(), m_pSearchPath->m_storeId, m_pFileHandle->m_VPKHandle );
				return;
			}
#endif
			// we always record hashes of everything we load. we may filter later.
			m_pFileSystem->m_FileTracker2.NoteFileLoadedFromDisk( pRelativeFileName, m_pSearchPath->GetPathIDString(), m_pSearchPath->m_storeId, fp, nLength );
		}
	}

#ifdef SUPPORT_PACKED_STORE
	void SetFromPackedStoredFileHandle( const CPackedStoreFileHandle &fHandle, CBaseFileSystem *pFileSystem )
	{
		Assert( fHandle );
		Assert( m_pFileHandle == nullptr );
		m_pFileHandle = new CFileHandle(pFileSystem);
		m_pFileHandle->m_VPKHandle = fHandle;
		m_pFileHandle->m_type = FT_NORMAL;
		m_pFileHandle->m_nLength = fHandle.m_nFileSize;
	}
#endif

public:
	CBaseFileSystem *m_pFileSystem;

	// These are output parameters.
	CFileHandle *m_pFileHandle;
	char **m_ppszResolvedFilename;

	CPackFile *m_pPackFile;
	CPackedStore *m_pVPKFile;

	const char *m_pFileName;
	const CBaseFileSystem::CSearchPath *m_pSearchPath;
	const char *m_pOptions;
	int m_Flags;

	EPureServerFileClass m_ePureFileClass;

	char m_AbsolutePath[MAX_FILEPATH];	// This is set 
};


void CBaseFileSystem::HandleOpenRegularFile( CFileOpenInfo &openInfo, bool bIsAbsolutePath )
{
	openInfo.m_pFileHandle = nullptr;

	int64 size;
	FILE *fp = Trace_FOpen( openInfo.m_AbsolutePath, openInfo.m_pOptions, openInfo.m_Flags, &size );
	if ( fp )
	{
		if ( m_pLogFile )
		{
			LogFileAccess( openInfo.m_AbsolutePath );
		}

		if ( m_bOutputDebugString )
		{
#ifdef _WIN32
			Plat_DebugString( "fs_debug: " );
			Plat_DebugString( openInfo.m_AbsolutePath );
			Plat_DebugString( "\n" );
#elif POSIX
			fprintf(stderr, "fs_debug: %s\n", openInfo.m_AbsolutePath );
#endif
		}

		openInfo.m_pFileHandle = new CFileHandle(this);
		openInfo.m_pFileHandle->m_pFile = fp;
		openInfo.m_pFileHandle->m_type = FT_NORMAL;
		openInfo.m_pFileHandle->m_nLength = size;

		openInfo.SetResolvedFilename( openInfo.m_AbsolutePath );
		
		LogFileOpen( "Loose", openInfo.m_pFileName, openInfo.m_AbsolutePath );
	}
}


//-----------------------------------------------------------------------------
// Purpose: The base file search goes through here
// Input  : *path - 
//			*pFileName - 
//			*pOptions - 
//			packfile - 
//			*filetime - 
// Output : FileHandle_t
//-----------------------------------------------------------------------------
FileHandle_t CBaseFileSystem::FindFileInSearchPath( CFileOpenInfo &openInfo )
{
	VPROF( "CBaseFileSystem::FindFile" );
	
	Assert( openInfo.m_pSearchPath );
	openInfo.m_pFileHandle = nullptr;

	// Loading from pack file?
	CPackFile *pPackFile = openInfo.m_pSearchPath->GetPackFile();
	if ( pPackFile )
	{
		openInfo.m_pFileHandle = pPackFile->OpenFile( openInfo.m_pFileName, openInfo.m_pOptions );
		openInfo.m_pPackFile = pPackFile;
		if ( openInfo.m_pFileHandle )
		{
			char tempStr[MAX_PATH*2+2];
			V_sprintf_safe( tempStr, "%s%c%s", pPackFile->m_ZipName.String(), CORRECT_PATH_SEPARATOR, openInfo.m_pFileName );
			openInfo.SetResolvedFilename( tempStr );
		}

		// If it's a BSP file, then the BSP file got CRC'd elsewhere so no need to verify stuff in there.
		return (FileHandle_t)openInfo.m_pFileHandle;
	}

	// Loading from VPK?
	#ifdef SUPPORT_PACKED_STORE
		CPackedStore *pVPK = openInfo.m_pSearchPath->GetPackedStore();
		if ( pVPK )
		{
			CPackedStoreFileHandle fHandle = pVPK->OpenFile( openInfo.m_pFileName );
			if ( fHandle )
			{
				openInfo.SetFromPackedStoredFileHandle( fHandle, this );
				openInfo.SetResolvedFilename( openInfo.m_pFileName );

				openInfo.m_pVPKFile = pVPK;
				LogFileOpen( "VPK", openInfo.m_pFileName, pVPK->BaseName() );
				openInfo.HandleFileCRCTracking( openInfo.m_pFileName );
				return ( FileHandle_t ) openInfo.m_pFileHandle;
			}
			return nullptr;
		}
	#endif

	// Load loose file specified as relative filename
	// Convert filename to lowercase.  All files in the
	// game logical filesystem must be accessed by lowercase name
	char szLowercaseFilename[ MAX_PATH ];
	V_strcpy_safe( szLowercaseFilename, openInfo.m_pFileName );
	V_strlower( szLowercaseFilename );

	openInfo.SetAbsolutePath( "%s%s", openInfo.m_pSearchPath->GetPathString(), szLowercaseFilename );

	// now have an absolute name
	HandleOpenRegularFile( openInfo, false );
	return (FileHandle_t)openInfo.m_pFileHandle;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
FileHandle_t CBaseFileSystem::OpenForRead( const char *pFileNameT, const char *pOptions, unsigned flags, const char *pathID, char **ppszResolvedFilename )
{
	VPROF( "CBaseFileSystem::OpenForRead" );

	char pFileNameBuff[MAX_PATH];
	const char *pFileName = pFileNameBuff;

	FixUpPath ( pFileNameT, pFileNameBuff );

	// Try the memory cache for un-restricted searches or "GAME" items.
	if ( !pathID || Q_stricmp( pathID, "GAME" ) == 0 )
	{
		CMemoryFileBacking* pBacking = nullptr;
		{
			AUTO_LOCK( m_MemoryFileMutex );
			CUtlHashtable< const char*, CMemoryFileBacking* >& table = m_MemoryFileHash;
			UtlHashHandle_t idx = table.Find( pFileName );
			if ( idx != table.InvalidHandle() )
			{
				pBacking = table[idx];
				pBacking->AddRef();
			}
		}
		if ( pBacking )
		{
			if ( pBacking->m_nLength != -1 )
			{
				CFileHandle* pFile = new CMemoryFileHandle( this, pBacking );
				pFile->m_type = strchr( pOptions, 'b' ) ? FT_MEMORY_BINARY : FT_MEMORY_TEXT;
				return ( FileHandle_t )pFile;
			}
			else
			{
				// length -1 == cached failure to read
				return ( FileHandle_t )nullptr;
			}
		}
		else if ( ThreadInMainThread() && fs_report_sync_opens.GetInt() > 0 )
		{
			DevWarning("blocking load %s\n", pFileName);
		}
	}

	CFileOpenInfo openInfo( this, pFileName, nullptr, pOptions, flags, ppszResolvedFilename );

	// Already have an absolute path?
	// If so, don't bother iterating search paths.
	if ( V_IsAbsolutePath( pFileName ) )
	{
		openInfo.SetAbsolutePath( "%s", pFileName );

		// Check if it's of the form C:/a/b/c/blah.zip/materials/blah.vtf
		// an absolute path can encode a zip pack file (i.e. caller wants to open the file from within the pack file)
		// format must strictly be ????.zip\????
		// assuming a reasonable restriction that the zip must be a pre-existing search path zip
		char *pZipExt = V_stristr( openInfo.m_AbsolutePath, ".zip" CORRECT_PATH_SEPARATOR_S );
		if ( !pZipExt )
			pZipExt = V_stristr( openInfo.m_AbsolutePath, ".bsp" CORRECT_PATH_SEPARATOR_S );
		#if defined( SUPPORT_PACKED_STORE )
			if ( !pZipExt )
				pZipExt = V_stristr( openInfo.m_AbsolutePath, ".vpk" CORRECT_PATH_SEPARATOR_S );
		#endif
	
		if ( pZipExt && pZipExt[5] )
		{
			// Cut string at the slash
			char *pSlash = pZipExt + 4;
			Assert( *pSlash == CORRECT_PATH_SEPARATOR );
			*pSlash = '\0';

			// want relative portion only, everything after the slash
			char *pRelativeFileName = pSlash + 1;

			// Find the zip or VPK in the search paths
			for ( intp i = 0; i < m_SearchPaths.Count(); i++ )
			{

				// In VPK?
				#if defined( SUPPORT_PACKED_STORE )
					CPackedStore *pVPK = m_SearchPaths[i].GetPackedStore();
					if ( pVPK )
					{
						if ( V_stricmp( pVPK->FullPathName(), openInfo.m_AbsolutePath ) == 0 )
						{
							CPackedStoreFileHandle fHandle = pVPK->OpenFile( pRelativeFileName );
							if ( fHandle )
							{
								openInfo.m_pSearchPath = &m_SearchPaths[i];
								openInfo.SetFromPackedStoredFileHandle( fHandle, this );
							}
							break;
						}
						continue;
					}
				#endif

				// In .zip?
				CPackFile *pPackFile = m_SearchPaths[i].GetPackFile();
				if ( pPackFile )
				{
					if ( Q_stricmp( pPackFile->m_ZipName.Get(), openInfo.m_AbsolutePath ) == 0 )
					{
						openInfo.m_pSearchPath = &m_SearchPaths[i];
						openInfo.m_pFileHandle = pPackFile->OpenFile( pRelativeFileName, openInfo.m_pOptions );
						openInfo.m_pPackFile = pPackFile;
						break;
					}
				}
			}

			if ( openInfo.m_pFileHandle )
			{
				openInfo.SetResolvedFilename( openInfo.m_pFileName );
				openInfo.HandleFileCRCTracking( pRelativeFileName );
			}
			return (FileHandle_t)openInfo.m_pFileHandle;
		}

		// Otherwise, it must be a regular file, specified by absolute filename
		HandleOpenRegularFile( openInfo, true );

		// !FIXME! We probably need to deal with CRC tracking, right?

		return (FileHandle_t)openInfo.m_pFileHandle;
	}

	// Run through all the search paths.
	PathTypeFilter_t pathFilter = FILTER_NONE;

	CSearchPathsIterator iter( this, &pFileName, pathID, pathFilter );
	for ( openInfo.m_pSearchPath = iter.GetFirst(); openInfo.m_pSearchPath != nullptr; openInfo.m_pSearchPath = iter.GetNext() )
	{
		FileHandle_t filehandle = FindFileInSearchPath( openInfo );
		if ( filehandle )
		{
			// Check if search path is excluded due to pure server white list,
			// then we should make a note of this fact, and keep searching
			if ( !openInfo.m_pSearchPath->m_bIsTrustedForPureServer && openInfo.m_ePureFileClass == ePureServerFileClass_AnyTrusted )
			{
				#ifdef PURE_SERVER_DEBUG_SPEW
					Msg( "Ignoring %s from %s for pure server operation\n", openInfo.m_pFileName, openInfo.m_pSearchPath->GetDebugString() );
				#endif

				m_FileTracker2.NoteFileIgnoredForPureServer( openInfo.m_pFileName, pathID, openInfo.m_pSearchPath->m_storeId );
				Close( filehandle );
				openInfo.m_pFileHandle = nullptr;
				if ( ppszResolvedFilename && *ppszResolvedFilename )
				{
					free( *ppszResolvedFilename );
					*ppszResolvedFilename = nullptr;
				}
				continue;
			}

			// 
			openInfo.HandleFileCRCTracking( openInfo.m_pFileName );
			return filehandle;
		}
	}

	LogFileOpen( "[Failed]", pFileName, "" );
	return nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
FileHandle_t CBaseFileSystem::OpenForWrite( const char *pFileName, const char *pOptions, const char *pathID )
{
	char tempPathID[MAX_PATH];
	ParsePathID( pFileName, pathID, tempPathID );

	if ( ThreadInMainThread() && fs_report_sync_opens.GetInt() )
	{
		DevWarning("blocking write %s\n", pFileName);
	}

	// Opening for write or append uses the write path
	// Unless an absolute path is specified...
	const char *pTmpFileName;
	char szScratchFileName[MAX_PATH];
	if ( V_IsAbsolutePath( pFileName ) )
	{
		pTmpFileName = pFileName;
	}
	else
	{
		ComputeFullWritePath( szScratchFileName, sizeof( szScratchFileName ), pFileName, pathID );
		pTmpFileName = szScratchFileName; 
	}

	int64 size;
	FILE *fp = Trace_FOpen( pTmpFileName, pOptions, 0, &size );
	if ( !fp )
	{
		return nullptr;
	}

	auto *fh = new CFileHandle( this );
	fh->m_nLength = size;
	fh->m_type = FT_NORMAL;
	fh->m_pFile = fp;

	return ( FileHandle_t )fh;
}


// This looks for UNC-type filename specifiers, which should be used instead of 
// passing in path ID. So if it finds //mod/cfg/config.cfg, it translates
// pFilename to "cfg/config.cfg" and pPathID to "mod" (mod is placed in tempPathID).
void CBaseFileSystem::ParsePathID( const char* &pFilename, const char* &pPathID, char tempPathID[MAX_PATH] )
{
	tempPathID[0] = 0;
	
	if ( !pFilename || pFilename[0] == 0 )
		return;

	// FIXME: Pain! Backslashes are used to denote network drives, forward to denote path ids
	// HOORAY! We call FixSlashes everywhere. That will definitely not work
	// I'm not changing it yet though because I don't know how painful the bugs would be that would be generated
	bool bIsForwardSlash = ( pFilename[0] == '/' && pFilename[1] == '/' );
//	bool bIsBackwardSlash = ( pFilename[0] == '\\' && pFilename[1] == '\\' );
	if ( !bIsForwardSlash ) //&& !bIsBackwardSlash ) 
		return;

	// They're specifying two path IDs. Ignore the one passed-in.
	if ( pPathID )
	{
		Warning( FILESYSTEM_WARNING, "FS: Specified two path IDs (%s, %s).\n", pFilename, pPathID );
	}

	// Parse out the path ID.
	const char *pIn = &pFilename[2];
	char *pOut = tempPathID;
	while ( *pIn && !PATHSEPARATOR( *pIn ) && (pOut - tempPathID) < (MAX_PATH-1) )
	{
		*pOut++ = *pIn++;
	}

	*pOut = 0;

	if ( tempPathID[0] == '*' )
	{
		// * means nullptr.
		pPathID = nullptr;
	}
	else
	{
		pPathID = tempPathID;
	}

	// Move pFilename up past the part with the path ID.
	if ( *pIn == 0 )
		pFilename = pIn;
	else
		pFilename = pIn + 1;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
FileHandle_t CBaseFileSystem::Open( const char *pFileName, const char *pOptions, const char *pathID )
{
	return OpenEx( pFileName, pOptions, 0, pathID );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
FileHandle_t CBaseFileSystem::OpenEx( const char *pFileName, const char *pOptions, unsigned flags, const char *pathID, char **ppszResolvedFilename )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s(%s, %s, %u %s )", __FUNCTION__, tmDynamicString( TELEMETRY_LEVEL0, pFileName ), tmDynamicString( TELEMETRY_LEVEL0, pOptions ), flags, tmDynamicString( TELEMETRY_LEVEL0, pathID ) );

	VPROF_BUDGET( "CBaseFileSystem::Open", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

	if ( !pFileName )
		return (FileHandle_t)0;
		
	CHECK_DOUBLE_SLASHES( pFileName );

	if ( ThreadInMainThread() && fs_report_sync_opens.GetInt() > 1 )
	{
		::Warning( "Open( %s )\n", pFileName );
	}

	// Allow for UNC-type syntax to specify the path ID.
	char tempPathID[MAX_PATH];
	ParsePathID( pFileName, pathID, tempPathID );

	// Try each of the search paths in succession
	// FIXME: call createdirhierarchy upon opening for write.
	if ( strchr( pOptions, 'r' ) && !strchr( pOptions, '+' ) )
	{
		return OpenForRead( pFileName, pOptions, flags, pathID, ppszResolvedFilename );
	}

	return OpenForWrite( pFileName, pOptions, pathID );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::Close( FileHandle_t file )
{
	VPROF_BUDGET( "CBaseFileSystem::Close", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

	if ( !file )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to Close nullptr file handle!\n" );
		return;
	}
	
	delete (CFileHandle*)file;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::Seek( FileHandle_t file, int pos, FileSystemSeek_t whence )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s (pos=%d, whence=%d)", __FUNCTION__, pos, whence );

	VPROF_BUDGET( "CBaseFileSystem::Seek", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

	auto *fh = ( CFileHandle *)file;
	if ( !fh )
	{
		Warning( FILESYSTEM_WARNING, "Tried to Seek nullptr file handle!\n" );
		return;
	}
	
	fh->Seek( pos, to_underlying( whence ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
// Output : unsigned int
//-----------------------------------------------------------------------------
unsigned int CBaseFileSystem::Tell( FileHandle_t file )
{
	VPROF_BUDGET( "CBaseFileSystem::Tell", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

	if ( !file )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to Tell nullptr file handle!\n" );
		return 0;
	}

	// Pack files are relative
	return (( CFileHandle *)file)->Tell();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
// Output : unsigned int
//-----------------------------------------------------------------------------
unsigned int CBaseFileSystem::Size( FileHandle_t file )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	VPROF_BUDGET( "CBaseFileSystem::Size", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );
	if ( !file )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to Size nullptr file handle!\n" );
		return 0;
	}

	return ((CFileHandle *)file)->Size();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
// Output : unsigned int
//-----------------------------------------------------------------------------
unsigned int CBaseFileSystem::Size( const char* pFileName, const char *pPathID )
{
	VPROF_BUDGET( "CBaseFileSystem::Size", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );
	// handle the case where no name passed...
	if ( !pFileName || !pFileName[0] )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to Size nullptr filename!\n" );
		return 0;
	}

	CHECK_DOUBLE_SLASHES( pFileName );
	
	// Ok, fall through to the fast path.
	unsigned result = 0;
	FileHandle_t h = Open( pFileName, "rb", pPathID );
	if ( h )
	{
		result = Size( h );
		Close(h);
	}

	return result;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *path - 
//			*pFileName - 
// Output : long
//-----------------------------------------------------------------------------
time_t CBaseFileSystem::FastFileTime( const CSearchPath *path, const char *pFileName )
{
	struct _stat buf;

	if ( path->GetPackFile() )
	{
		// If we found the file:
		if ( path->GetPackFile()->ContainsFile( pFileName ) )
		{
			return path->GetPackFile()->m_lPackFileTime;
		}
	}
#ifdef SUPPORT_PACKED_STORE
	else if ( path->GetPackedStore() )
	{
		// Hm, should we support this in some way?
		return 0L;
	}
#endif
	else
	{
		// Is it an absolute path?
		char pTmpFileName[ MAX_FILEPATH ]; 
		
		if ( V_IsAbsolutePath( pFileName ) )
		{
			V_strcpy_safe( pTmpFileName, pFileName );
		}
		else
		{
			V_sprintf_safe( pTmpFileName, "%s%s", path->GetPathString(), pFileName );
		}

		V_FixSlashes( pTmpFileName );

		if( FS_stat( pTmpFileName, &buf ) != -1 )
		{
			return buf.st_mtime;
		}
#ifdef LINUX
		char caseFixedName[ MAX_PATH ];
		bool found = findFileInDirCaseInsensitive_safe( pTmpFileName, caseFixedName );
		if ( found && FS_stat( caseFixedName, &buf ) != -1 )
		{
			return buf.st_mtime;
		}
#endif
	}

	return ( 0L );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseFileSystem::EndOfFile( FileHandle_t file )
{
	if ( !file )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to EndOfFile nullptr file handle!\n" );
		return true;
	}

	return ((CFileHandle *)file)->EndOfFile();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseFileSystem::Read( OUT_BYTECAP(size) void *pOutput, int size, FileHandle_t file )
{
	return ReadEx( pOutput, size, size, file );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseFileSystem::ReadEx( OUT_BYTECAP(destSize) void *pOutput, int destSize, int size, FileHandle_t file )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s (%d bytes)", __FUNCTION__, size );

	VPROF_BUDGET( "CBaseFileSystem::Read", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );
	if ( !file )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to Read nullptr file handle!\n" );
		return 0;
	}
	if ( size < 0 )
	{
		return 0;
	}
	return ((CFileHandle*)file)->Read(pOutput, destSize, size );
}

//-----------------------------------------------------------------------------
// Purpose: Blow away current readers
// Input  :  - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::UnloadCompiledKeyValues()
{
#ifndef DEDICATED
	for ( auto &preload : m_PreloadData )
	{
		delete preload.m_pReader;
		preload.m_pReader = nullptr;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Put data file into list of at specific slot, will be loaded when ::SetupPreloadData() gets called
// Input  : type - 
//			*archiveFile - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::LoadCompiledKeyValues( KeyValuesPreloadType_t type, char const *archiveFile )
{
	// Just add to list for appropriate loader
	Assert( type >= 0 && type < IFileSystem::NUM_PRELOAD_TYPES );
	CompiledKeyValuesPreloaders_t& loader = m_PreloadData[ type ];
	Assert( loader.m_CacheFile == 0 );
	loader.m_CacheFile = FindOrAddFileName( archiveFile );
}

//-----------------------------------------------------------------------------
// Purpose: Takes a passed in KeyValues& head and fills in the precompiled keyvalues data into it.
// Input  : head - 
//			type - 
//			*filename - 
//			*pPathID - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::LoadKeyValues( KeyValues& head, KeyValuesPreloadType_t type, char const *filename, char const *pPathID /*= 0*/ )
{
#ifndef DEDICATED
	char tempPathID[MAX_PATH];
	ParsePathID( filename, pPathID, tempPathID );
	
	bool bret = true;
	// FIXME:  THIS STUFF DOESN'T TRACK pPathID AT ALL RIGHT NOW!!!!!
	if ( !m_PreloadData[ type ].m_pReader || !m_PreloadData[ type ].m_pReader->InstanceInPlace( head, filename ) )
	{
		bret = head.LoadFromFile( this, filename, pPathID );
	}
	return bret;
#else
	return head.LoadFromFile( this, filename, pPathID );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: If the "PreloadedData" hasn't been purged, then this'll try and instance the KeyValues using the fast path of 
/// compiled keyvalues loaded during startup.
// Otherwise, it'll just fall through to the regular KeyValues loading routines
// Input  : type - 
//			*filename - 
//			*pPathID - 
// Output : KeyValues
//-----------------------------------------------------------------------------
KeyValues *CBaseFileSystem::LoadKeyValues( KeyValuesPreloadType_t type, char const *filename, char const *pPathID /*= 0*/ )
{
	KeyValues *kv = nullptr;

	if ( !m_PreloadData[ type ].m_pReader )
	{
		kv = new KeyValues( filename );
		if ( kv )
		{
			if ( !kv->LoadFromFile( this, filename, pPathID ) )
			{
				// dimhotepus: Do not return anything if load failed.
				kv->deleteThis();
				return nullptr;
			}
		}
	}
	else
	{
#ifndef DEDICATED
		// FIXME:  THIS STUFF DOESN'T TRACK pPathID AT ALL RIGHT NOW!!!!!
		kv = m_PreloadData[ type ].m_pReader->Instance( filename );
		if ( !kv )
		{
			kv = new KeyValues( filename );
			if ( kv )
			{
				if ( !kv->LoadFromFile( this, filename, pPathID ) )
				{
					// dimhotepus: Do not return anything if load failed.
					kv->deleteThis();
					return nullptr;
				}
			}
		}
#endif
	}
	return kv;
}

//-----------------------------------------------------------------------------
// Purpose: This is the fallback method of reading the name of the first key in the file
// Input  : *filename - 
//			*pPathID - 
//			*rootName - 
//			bufsize - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::LookupKeyValuesRootKeyName( char const *filename, char const *pPathID, OUT_Z_CAP(bufsize) char *rootName, size_t bufsize )
{
	// open file and get shader name
	FileHandle_t hFile = Open( filename, "r", pPathID );
	if ( hFile == FILESYSTEM_INVALID_HANDLE )
	{
		// dimhotepus: Always zero-terminate.
		if (bufsize) rootName[0] = '\0';
		return false;
	}

	char buf[ 128 ];
	static_cast<IFileSystem *>(this)->ReadLine( buf, hFile );
	Close( hFile );

	// The name will possibly come in as "foo"\n

	// So we need to strip the starting " character
	char *pStart = buf;
	if ( *pStart == '\"' )
	{
		++pStart;
	}
	// Then copy the rest of the string
	V_strncpy( rootName, pStart, bufsize );

	// And then strip off the \n and the " character at the end, in that order
	intp len = V_strlen( pStart );
	while ( len > 0 && rootName[ len - 1 ] == '\n' )
	{
		rootName[ len - 1 ] = 0;
		--len;
	}
	while ( len > 0 && rootName[ len - 1 ] == '\"' )
	{
		rootName[ len - 1 ] = 0;
		--len;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Tries to look up the name of the first key in the file from the compiled data
// Input  : type - 
//			*outbuf - 
//			bufsize - 
//			*filename - 
//			*pPathID - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::ExtractRootKeyName( KeyValuesPreloadType_t type, OUT_Z_CAP(bufsize) char *outbuf, size_t bufsize, char const *filename, char const *pPathID /*= 0*/ )
{
	char tempPathID[MAX_PATH];
	ParsePathID( filename, pPathID, tempPathID );

	bool bret = true;

	if ( !m_PreloadData[ type ].m_pReader )
	{
		// Use fallback
		bret = LookupKeyValuesRootKeyName( filename, pPathID, outbuf, bufsize );
	}
	else
	{
#ifndef DEDICATED
		// Try to use cache
		bret = m_PreloadData[ type ].m_pReader->LookupKeyValuesRootKeyName( filename, outbuf, bufsize );
		if ( !bret )
		{
			// Not in cache, use fallback
			bret = LookupKeyValuesRootKeyName( filename, pPathID, outbuf, bufsize );
		}
#endif
	}
	return bret;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::SetupPreloadData()
{
	for ( auto &path : m_SearchPaths )
	{
		CPackFile* pf = path.GetPackFile();
		if ( pf ) 
		{
			pf->SetupPreloadData();
		}
	}

#ifndef DEDICATED
	if ( !CommandLine()->FindParm( "-fs_nopreloaddata" ) )
	{
		char fn[MAX_PATH];
		// Loads in the precompiled keyvalues data for each type
		for ( auto &preloader : m_PreloadData )
		{
			Assert( !preloader.m_pReader );

			if ( preloader.m_CacheFile != 0 && 
				String( preloader.m_CacheFile, fn, sizeof( fn ) ) )
			{
				preloader.m_pReader = new CCompiledKeyValuesReader();
				preloader.m_pReader->LoadFile( fn );
			}
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::DiscardPreloadData()
{
	for( auto &path : m_SearchPaths )
	{
		CPackFile* pf = path.GetPackFile();
		if ( pf )
		{
			pf->DiscardPreloadData();
		}
	}

	UnloadCompiledKeyValues();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseFileSystem::Write( IN_BYTECAP(size) void const* pInput, int size, FileHandle_t file )
{
	VPROF_BUDGET( "CBaseFileSystem::Write", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

	AUTOBLOCKREPORTER_FH( Write, this, true, file, FILESYSTEM_BLOCKING_SYNCHRONOUS, FileBlockingItem::FB_ACCESS_WRITE );

	auto *fh = ( CFileHandle *)file;
	if ( !fh )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to Write nullptr file handle!\n" );
		return 0;
	}
	return fh->Write( pInput, size );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseFileSystem::FPrintf( FileHandle_t file, PRINTF_FORMAT_STRING const char *pFormat, ... )
{
	va_list args;
	va_start( args, pFormat ); //-V2018 //-V2019

	VPROF_BUDGET( "CBaseFileSystem::FPrintf", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

	if ( !file )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to FPrintf nullptr file handle!\n" );
		return 0;
	}

	char buffer[65535];
	const int chars_count{V_vsprintf_safe( buffer, pFormat, args )};
	va_end( args );
	AssertMsg(chars_count < ssize(buffer),
		"FPrintf output is truncated from %d to %zd.", chars_count, ssize(buffer));

	auto *fh = static_cast<CFileHandle *>(file);
	const int bytes_written{ fh->Write( buffer, chars_count ) };
	AssertMsg(chars_count == bytes_written,
		"Should FPrintf %d characters, but written %d.", chars_count, bytes_written);
	
	return bytes_written;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::SetBufferSize( FileHandle_t file, unsigned nBytes )
{
	auto *fh = ( CFileHandle *)file;
	if ( !fh )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to SetBufferSize nullptr file handle!\n" );
		return;
	}
	fh->SetBufferSize( nBytes );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseFileSystem::IsOk( FileHandle_t file )
{
	auto *fh = ( CFileHandle *)file;
	if ( !fh )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to IsOk nullptr file handle!\n" );
		return false;
	}

	return fh->IsOK();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::Flush( FileHandle_t file )
{
	VPROF_BUDGET( "CBaseFileSystem::Flush", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );
	auto *fh = ( CFileHandle *)file;
	if ( !fh )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to Flush nullptr file handle!\n" );
		return;
	}

	fh->Flush();
}

bool CBaseFileSystem::Precache( const char *pFileName, const char *pPathID)
{
	CHECK_DOUBLE_SLASHES( pFileName );

	// Allow for UNC-type syntax to specify the path ID.
	char tempPathID[MAX_PATH];
	ParsePathID( pFileName, pPathID, tempPathID );
	Assert( pPathID );

	// Really simple, just open, the file, read it all in and close it. 
	// We probably want to use file mapping to do this eventually.
	FileHandle_t f = Open( pFileName, "rb", pPathID );
	if ( !f )
		return false;

	// dimhotepus: Double buffer size.
	char buffer[65535];
	while( sizeof(buffer) == Read(buffer,sizeof(buffer),f) );

	Close( f );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
char *CBaseFileSystem::ReadLine( OUT_Z_CAP(maxChars) char *pOutput, int maxChars, FileHandle_t file )
{
	VPROF_BUDGET( "CBaseFileSystem::ReadLine", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

	CFileHandle *fh = ( CFileHandle *)file;
	if ( !fh )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to ReadLine nullptr file handle!\n" );
		// dimhotepus: Always zero-terminate.
		if (maxChars > 0) pOutput[0] = '\0';
		return nullptr;
	}
	++m_Stats.nReads;

	int nRead = 0;

	// Read up to maxchars:
	while( nRead < ( maxChars - 1 ) )
	{
		// Are we at the end of the file?
		if( 1 != fh->Read( pOutput + nRead, 1 ) )
			break;

		// Translate for text mode files:
		if( ( fh->m_type == FT_PACK_TEXT || fh->m_type == FT_MEMORY_TEXT ) && pOutput[nRead] == '\r' )
		{
			// Ignore \r
			continue;
		}

		// We're done when we hit a '\n'
		if( pOutput[nRead] == '\n' )
		{
			nRead++;
			break;
		}

		// Get outta here if we find a nullptr.
		if( pOutput[nRead] == '\0' )
		{
			pOutput[nRead] = '\n';
			nRead++;
			break;
		}

		nRead++;
	}

	// dimhotepus: Always zero-terminate.
	Assert( nRead < maxChars );
	pOutput[nRead] = '\0';

	m_Stats.nBytesRead += nRead;
	return ( nRead ) ? pOutput : nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFileName - 
// Output : long
//-----------------------------------------------------------------------------
time_t CBaseFileSystem::GetFileTime( const char *pFileName, const char *pPathID )
{
	VPROF_BUDGET( "CBaseFileSystem::GetFileTime", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

	CHECK_DOUBLE_SLASHES( pFileName );

	CSearchPathsIterator iter( this, &pFileName, pPathID );

	char tempFileName[MAX_PATH];
	V_strcpy_safe( tempFileName, pFileName );
	V_FixSlashes( tempFileName );
#ifdef _WIN32
	V_strlower( tempFileName );
#endif

	for ( CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != nullptr; pSearchPath = iter.GetNext() )
	{
		time_t ft = FastFileTime( pSearchPath, tempFileName );
		if ( ft != 0L )
		{
			if ( !pSearchPath->GetPackFile() && m_LogFuncs.Count() )
			{
				char pTmpFileName[ MAX_FILEPATH ]; 
				if ( strchr( tempFileName, ':' ) )
				{
					V_strcpy_safe( pTmpFileName, tempFileName );
				}
				else
				{
					V_sprintf_safe( pTmpFileName, "%s%s", pSearchPath->GetPathString(), tempFileName );
				}

				V_FixSlashes( tempFileName );
				LogAccessToFile( "filetime", pTmpFileName, "" );
			}

			return ft;
		}
	}
	return 0L;
}

time_t CBaseFileSystem::GetPathTime( const char *pFileName, const char *pPathID )
{
	VPROF_BUDGET( "CBaseFileSystem::GetPathTime", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

	CSearchPathsIterator iter( this, &pFileName, pPathID );

	char tempFileName[MAX_PATH];
	V_strcpy_safe( tempFileName, pFileName );
	V_FixSlashes( tempFileName );
#ifdef _WIN32
	V_strlower( tempFileName );
#endif

	time_t pathTime = 0L;
	for ( CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != nullptr; pSearchPath = iter.GetNext() )
	{
		time_t ft = FastFileTime( pSearchPath, tempFileName );
		if ( ft > pathTime )
			pathTime = ft;
		if ( ft != 0L )
		{
			if ( !pSearchPath->GetPackFile() && m_LogFuncs.Count() )
			{
				char pTmpFileName[ MAX_FILEPATH ]; 
				if ( strchr( tempFileName, ':' ) )
				{
					V_strcpy_safe( pTmpFileName, tempFileName );
				}
				else
				{
					V_sprintf_safe( pTmpFileName, "%s%s", pSearchPath->GetPathString(), tempFileName );
				}

				V_FixSlashes( tempFileName );

				LogAccessToFile( "filetime", pTmpFileName, "" );
			}
		}
	}
	return pathTime;
}

void CBaseFileSystem::MarkAllCRCsUnverified()
{
	m_FileTracker2.MarkAllCRCsUnverified();
}


void CBaseFileSystem::CacheFileCRCs( const char *pPathname, ECacheCRCType eType, IFileList *pFilter )
{
}

EFileCRCStatus CBaseFileSystem::CheckCachedFileHash( const char *pPathID, const char *pRelativeFilename, int nFileFraction, FileHash_t *pFileHash )
{
	return m_FileTracker2.CheckCachedFileHash( pPathID, pRelativeFilename, nFileFraction, pFileHash );
}


void CBaseFileSystem::EnableWhitelistFileTracking( bool bEnable, bool bCacheAllVPKHashes, bool bRecalculateAndCheckHashes )
{
	if ( m_WhitelistFileTrackingEnabled != -1 )
	{
		Error( "CBaseFileSystem::EnableWhitelistFileTracking called more than once." );
	}
	
	m_WhitelistFileTrackingEnabled = bEnable;
	if ( m_WhitelistFileTrackingEnabled && bCacheAllVPKHashes )
	{
		CacheAllVPKFileHashes( bCacheAllVPKHashes, bRecalculateAndCheckHashes );
	}
}


void CBaseFileSystem::CacheAllVPKFileHashes( bool bCacheAllVPKHashes, bool bRecalculateAndCheckHashes )
{
#ifdef SUPPORT_PACKED_STORE
	for ( intp i = 0; i < m_SearchPaths.Count(); i++ )
	{
		CPackedStore *pVPK = m_SearchPaths[i].GetPackedStore();
		if ( pVPK == nullptr )
			continue;
		if ( !pVPK->BTestDirectoryHash() )
		{
			Msg( "VPK dir file hash does not match. File corrupted or modified.\n" );
		}
		if ( !pVPK->BTestMasterChunkHash() )
		{
			Msg( "VPK chunk hash hash does not match. File corrupted or modified.\n" );
		}

		CUtlVector<ChunkHashFraction_t> &vecChunkHash = pVPK->AccessPackFileHashes();
		CPackedStoreFileHandle fhandle = pVPK->GetHandleForHashingFiles();
		CUtlVector<ChunkHashFraction_t> vecChunkHashFractionCopy;
		if ( bRecalculateAndCheckHashes )
		{
			CUtlString sPackFileErrors;
			pVPK->GetPackFileLoadErrorSummary( sPackFileErrors );

			if ( sPackFileErrors.Length() )
			{
				Msg( "Errors occured loading files.\n" );
				Msg( "%s", sPackFileErrors.String() );
				Msg( "Verify integrity of your game files, perform memory and disk diagnostics on your system.\n" );
			}
			else
				Msg( "No VPK Errors occured loading files.\n" );

			Msg( "Recomputing all VPK file hashes.\n" );
			vecChunkHashFractionCopy.Swap( vecChunkHash );
		}
		intp cFailures = 0;
		if ( vecChunkHash.Count() == 0 )
		{
			if ( vecChunkHashFractionCopy.Count() == 0 )
				Msg( "File hash information not found: Hashing all VPK files for pure server operation.\n" );
			pVPK->HashAllChunkFiles();
			if ( vecChunkHashFractionCopy.Count() != 0 )
			{
				if ( vecChunkHash.Count() != vecChunkHashFractionCopy.Count() )
				{
					Msg( "VPK hash count does not match. VPK content may be corrupt.\n" );
				}
				else if  ( Q_memcmp( vecChunkHash.Base(), vecChunkHashFractionCopy.Base(), vecChunkHash.Count()*sizeof(vecChunkHash[0])) != 0 )
				{
					Msg( "VPK hashes do not match. VPK content may be corrupt.\n" );
					// find the actual mismatch
					FOR_EACH_VEC( vecChunkHashFractionCopy, iHash )
					{
						if ( Q_memcmp( vecChunkHashFractionCopy[iHash].m_md5contents.bits, vecChunkHash[iHash].m_md5contents.bits, sizeof( vecChunkHashFractionCopy[iHash].m_md5contents.bits ) ) != 0 )
						{
							Msg( "VPK hash for file %d failure at offset %x.\n", vecChunkHashFractionCopy[iHash].m_nPackFileNumber, vecChunkHashFractionCopy[iHash].m_nFileFraction );
							cFailures++;
						}
					}
				}
			}
		}
		if ( bCacheAllVPKHashes )
		{
			Msg( "Loaded %zd VPK file hashes from %s for pure server operation.\n", vecChunkHash.Count(), pVPK->FullPathName() );
			for ( auto &f : vecChunkHash )
			{
				m_FileTracker2.AddFileHashForVPKFile( f.m_nPackFileNumber, f.m_nFileFraction, f.m_cbChunkLen, f.m_md5contents, fhandle );
			}
		}
		else
		{
			if ( cFailures == 0 && vecChunkHash.Count() == vecChunkHashFractionCopy.Count() )
				Msg( "File hashes checked. %zd matches. no failures.\n", vecChunkHash.Count() );
			else
				Msg( "File hashes checked. %zd matches. %zd failures.\n", vecChunkHash.Count(), cFailures );
		}
	}
#endif
}


bool CBaseFileSystem::CheckVPKFileHash( int PackFileID, int nPackFileNumber, int nFileFraction, MD5Value_t &md5Value )
{
#ifdef SUPPORT_PACKED_STORE
	for ( auto &sp : m_SearchPaths )
	{
		CPackedStore *pVPK = sp.GetPackedStore();
		if ( pVPK == nullptr || pVPK->m_PackFileID != PackFileID )
			continue;

		ChunkHashFraction_t fileHashFraction;
		if ( pVPK->FindFileHashFraction( nPackFileNumber, nFileFraction, fileHashFraction ) )
		{
			CPackedStoreFileHandle fhandle = pVPK->GetHandleForHashingFiles();
			fhandle.m_nFileNumber = nPackFileNumber;

			char szFileName[MAX_PATH];
			pVPK->GetPackFileName( fhandle, szFileName );

			char hex[ 34 ];
			Q_memset( hex, 0, sizeof( hex ) );
			V_binarytohex( md5Value.bits, hex );

			char hex2[ 34 ];
			Q_memset( hex2, 0, sizeof( hex2 ) );
			V_binarytohex( fileHashFraction.m_md5contents.bits, hex2 );

			if ( Q_memcmp( fileHashFraction.m_md5contents.bits, md5Value.bits, sizeof(md5Value.bits) ) != 0 )
			{
				Msg( "File %s offset %x hash %s does not match ( should be %s ) \n", szFileName, nFileFraction, hex, hex2 );
				return false;
			}
			else
			{
				return true;
			}
		}
	}
#else
	Error("CBaseFileSystem::CheckVPKFileHash should not be called, SUPPORT_PACKED_STORE not defined" );
#endif
	return false;
}

void CBaseFileSystem::RegisterFileWhitelist( IPureServerWhitelist *pWhiteList, IFileList **pFilesToReload )
{
	if ( pFilesToReload )
		*pFilesToReload = nullptr;

	if ( m_pPureServerWhitelist )
	{
		m_pPureServerWhitelist->Release();
		m_pPureServerWhitelist = nullptr;
	}
	if ( pWhiteList )
	{
		pWhiteList->AddRef();
		m_pPureServerWhitelist = pWhiteList;
	}

	// update which search paths are considered trusted
	for ( auto &path : m_SearchPaths )
	{
		SetSearchPathIsTrustedSource( &path );
	}

	// See if we need to reload any files
	if ( pFilesToReload )
		*pFilesToReload = m_FileTracker2.GetFilesToUnloadForWhitelistChange( pWhiteList );
}

void CBaseFileSystem::NotifyFileUnloaded( const char *pszFilename, const char *pPathId )
{
	m_FileTracker2.NoteFileUnloaded( pszFilename, pPathId );
}

void CBaseFileSystem::SetSearchPathIsTrustedSource( CSearchPath *pSearchPath )
{
	// Most paths are not considered trusted
	pSearchPath->m_bIsTrustedForPureServer = false;

	// If we don't have a pure server whitelist, we cannot say that any
	// particular files are trusted.  (But then again, all files will be
	// accepted, so it won't really matter.)
	if ( m_pPureServerWhitelist == nullptr )
		return;

	// Treat map packs as trusted, because we will send the CRC of the map pack to the server
	if ( pSearchPath->GetPackFile() && pSearchPath->GetPackFile()->m_bIsMapPath )
	{
#ifdef PURE_SERVER_DEBUG_SPEW
		Msg( "Setting map pack search path %s as trusted\n", pSearchPath->GetDebugString() );
#endif
		pSearchPath->m_bIsTrustedForPureServer = true;
		return;
	}

#ifdef SUPPORT_PACKED_STORE
		// Only signed VPK's can be trusted
		CPackedStoreRefCount *pVPK = pSearchPath->GetPackedStore();
		if ( pVPK == nullptr )
		{
#ifdef PURE_SERVER_DEBUG_SPEW
			Msg( "Setting %s as untrusted (loose files)\n", pSearchPath->GetDebugString() );
#endif
			return;
		}
		if ( !pVPK->m_bSignatureValid )
		{
#ifdef PURE_SERVER_DEBUG_SPEW
			Msg( "Setting %s as untrusted (unsigned VPK)\n", pSearchPath->GetDebugString() );
#endif
			return;
		}
		const CUtlVector<uint8> &key = pVPK->GetSignaturePublicKey();
		for ( int iKeyIndex = 0 ; iKeyIndex < m_pPureServerWhitelist->GetTrustedKeyCount() ; ++iKeyIndex )
		{
			int nKeySz = 0;
			const byte *pbKey = m_pPureServerWhitelist->GetTrustedKey( iKeyIndex, &nKeySz );
			Assert( pbKey != nullptr && nKeySz > 0 );
			if ( key.Count() == nKeySz && V_memcmp( pbKey, key.Base(), nKeySz ) == 0 )
			{
#ifdef PURE_SERVER_DEBUG_SPEW
				Msg( "Setting %s as untrusted\n", pSearchPath->GetDebugString() );
#endif
				pSearchPath->m_bIsTrustedForPureServer = true;
				return;
			}
		}

#ifdef PURE_SERVER_DEBUG_SPEW
		Msg( "Setting %s as untrusted.  (Key not in trusted key list)\n", pSearchPath->GetDebugString() );
#endif
#endif
}


int CBaseFileSystem::GetUnverifiedFileHashes( CUnverifiedFileHash *pFiles, int nMaxFiles )
{
	return m_FileTracker2.GetUnverifiedFileHashes( pFiles, nMaxFiles );
}



int CBaseFileSystem::GetWhitelistSpewFlags()
{
	return m_WhitelistSpewFlags;
}


void CBaseFileSystem::SetWhitelistSpewFlags( int flags )
{
	m_WhitelistSpewFlags = flags;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pString - 
//			maxChars - 
//			fileTime - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::FileTimeToString( OUT_Z_CAP(maxChars) char *pString, intp maxChars, time_t fileTime )
{
	time_t time = fileTime;
	V_strncpy( pString, ctime( &time ), maxChars );

	// We see a linefeed at the end of these strings...if there is one, gobble it up
	intp len = V_strlen( pString );
	if ( len > 0 && pString[ len - 1 ] == '\n' )
	{
		pString[ len - 1 ] = '\0';
	}

	pString[maxChars - 1] = '\0';
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFileName - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::FileExists( const char *pFileName, const char *pPathID )
{
	VPROF_BUDGET( "CBaseFileSystem::FileExists", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

	CHECK_DOUBLE_SLASHES( pFileName );

	FileHandle_t h = Open( pFileName, "rb", pPathID );
	if ( h )
	{
		Close(h);
		return true;
	}

	return false;
}

bool CBaseFileSystem::IsFileWritable( char const *pFileName, char const *pPathID /*=0*/ )
{
	CHECK_DOUBLE_SLASHES( pFileName );

	struct _stat buf;

	char tempPathID[MAX_PATH];
	ParsePathID( pFileName, pPathID, tempPathID );

	if ( V_IsAbsolutePath( pFileName ) )
	{
		if ( FS_stat( pFileName, &buf ) != -1 )
		{
#ifdef WIN32
			if( buf.st_mode & _S_IWRITE )
#elif POSIX
			if( buf.st_mode & S_IWRITE )
#endif
			{
				return true;
			}
		}
		return false;
	}

	CSearchPathsIterator iter( this, &pFileName, pPathID, FILTER_CULLPACK );
	for ( CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != nullptr; pSearchPath = iter.GetNext() )
	{
		char pTmpFileName[ MAX_FILEPATH ];
		V_sprintf_safe( pTmpFileName, "%s%s", pSearchPath->GetPathString(), pFileName );
		V_FixSlashes( pTmpFileName );

		if ( FS_stat( pTmpFileName, &buf ) != -1 )
		{
#ifdef WIN32
			if ( buf.st_mode & _S_IWRITE )
#elif LINUX
			if ( buf.st_mode & S_IWRITE )
#else
			if ( buf.st_mode & S_IWRITE )
#endif
			{
				return true;
			}
		}
	}
	return false;
}


bool CBaseFileSystem::SetFileWritable( char const *pFileName, bool writable, const char *pPathID /*= 0*/ )
{
	CHECK_DOUBLE_SLASHES( pFileName );

#ifdef _WIN32
	const int pmode = writable ? ( _S_IWRITE | _S_IREAD ) : ( _S_IREAD );
#else
	const int pmode = writable ? ( S_IWRITE | S_IREAD ) : ( S_IREAD );
#endif

	char tempPathID[MAX_PATH];
	ParsePathID( pFileName, pPathID, tempPathID );

	if ( V_IsAbsolutePath( pFileName ) )
	{
		return ( FS_chmod( pFileName, pmode ) == 0 );
	}

	CSearchPathsIterator iter( this, &pFileName, pPathID, FILTER_CULLPACK );
	for ( CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != nullptr; pSearchPath = iter.GetNext() )
	{
		char pTmpFileName[ MAX_FILEPATH ];
		V_sprintf_safe( pTmpFileName, "%s%s", pSearchPath->GetPathString(), pFileName );
		V_FixSlashes( pTmpFileName );

		if ( FS_chmod( pTmpFileName, pmode ) == 0 )
		{
			return true;
		}
	}

	// Failure
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFileName - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::IsDirectory( const char *pFileName, const char *pathID )
{
	CHECK_DOUBLE_SLASHES( pFileName );

	// Allow for UNC-type syntax to specify the path ID.
	struct	_stat buf;

	char pTempBuf[MAX_PATH];
	V_strcpy_safe( pTempBuf, pFileName );
	V_StripTrailingSlash( pTempBuf );
	pFileName = pTempBuf;

	char tempPathID[MAX_PATH];
	ParsePathID( pFileName, pathID, tempPathID );
	if ( V_IsAbsolutePath( pFileName ) )
	{
		if ( FS_stat( pFileName, &buf ) != -1 )
		{
			if ( buf.st_mode & _S_IFDIR )
				return true;
		}
		return false;
	}

	CSearchPathsIterator iter( this, &pFileName, pathID, FILTER_CULLPACK );
	for ( CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != nullptr; pSearchPath = iter.GetNext() )
	{
#ifdef SUPPORT_PACKED_STORE
		if ( pSearchPath->GetPackedStore() )
		{
			CUtlStringList outDir, outFile;
			pSearchPath->GetPackedStore()->GetFileAndDirLists( outDir, outFile, false );

			for ( auto *dir : outDir )
			{
				if ( !V_stricmp( dir, pFileName ) )
					return true;
			}
		}
		else
#endif // SUPPORT_PACKED_STORE
		{
			char pTmpFileName[ MAX_FILEPATH ];
			V_sprintf_safe( pTmpFileName, "%s%s", pSearchPath->GetPathString(), pFileName );
			V_FixSlashes( pTmpFileName );

			if ( FS_stat( pTmpFileName, &buf ) != -1 )
			{
				if ( buf.st_mode & _S_IFDIR )
					return true;
			}
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *path - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::CreateDirHierarchy( const char *pRelativePathT, const char *pathID )
{	
	// Allow for UNC-type syntax to specify the path ID.
	char tempPathID[MAX_PATH];
	ParsePathID(pRelativePathT, pathID, tempPathID); // use the original path param to preserve "//"

	char pRelativePathBuff[ MAX_PATH ];
	const char *pRelativePath = pRelativePathBuff;

	FixUpPath( pRelativePathT, pRelativePathBuff );

	CHECK_DOUBLE_SLASHES( pRelativePath );

	char szScratchFileName[MAX_PATH];
	if ( !V_IsAbsolutePath( pRelativePath ) )
	{
		Assert( pathID );

		ComputeFullWritePath( szScratchFileName, sizeof( szScratchFileName ), pRelativePath, pathID );
	}
	else
	{
		V_strcpy_safe( szScratchFileName, pRelativePath );
	}

	intp len = V_strlen( szScratchFileName ) + 1;
	char *end = szScratchFileName + len;
	char *s = szScratchFileName;
	while ( s < end )
	{
		if ( *s == CORRECT_PATH_SEPARATOR && s != szScratchFileName && ( IsLinux() || *( s - 1 ) != ':' ) )
		{
			*s = '\0';

#if defined( _WIN32 )
			if ( _mkdir( szScratchFileName ) && errno != EEXIST )
#elif defined( POSIX )
			if ( mkdir( szScratchFileName, S_IRWXU |  S_IRGRP |  S_IROTH ) && errno != EEXIST )// owner has rwx, rest have r
#endif
			{
				::Warning( "Unable to create file or directory '%s' in hierarchy '%s': %s.\n",
					szScratchFileName,
					pRelativePathT,
					std::generic_category().message(errno).c_str() );
			}

			*s = CORRECT_PATH_SEPARATOR;
		}

		s++;
	}

#if defined( _WIN32 )
	if ( _mkdir( szScratchFileName ) && errno != EEXIST )
#elif defined( POSIX )
	if ( mkdir( szScratchFileName, S_IRWXU |  S_IRGRP |  S_IROTH ) && errno != EEXIST )
#endif
	{
		::Warning( "Unable to create file '%s' in hierarchy '%s': %s.\n",
			szScratchFileName,
			pRelativePathT,
			std::generic_category().message(errno).c_str() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pWildCard - 
//			*pHandle - 
// Output : const char
//-----------------------------------------------------------------------------
const char *CBaseFileSystem::FindFirstEx( const char *pWildCard, const char *pPathID, FileFindHandle_t *pHandle )
{
	CHECK_DOUBLE_SLASHES( pWildCard );

	return FindFirstHelper( pWildCard, pPathID, pHandle, nullptr );
}


const char *CBaseFileSystem::FindFirstHelper( const char *pWildCardT, const char *pPathID, FileFindHandle_t *pHandle, int *pFoundStoreID )
{
	VPROF_BUDGET( "CBaseFileSystem::FindFirst", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

 	Assert(pWildCardT);
 	Assert(pHandle);

	auto hTmpHandle = m_FindData.AddToTail();
	FindData_t *pFindData = &m_FindData[hTmpHandle];
	Assert( pFindData );
	if ( pPathID )
	{
		pFindData->m_FilterPathID = g_PathIDTable.AddString( pPathID );
	}

	char pWildCard[ MAX_PATH ];
	FixUpPath ( pWildCardT, pWildCard );

	intp maxlen = V_strlen( pWildCard ) + 1;
	pFindData->wildCardString.AddMultipleToTail( maxlen );
	V_strncpy( pFindData->wildCardString.Base(), pWildCard, maxlen );
	pFindData->findHandle = INVALID_HANDLE_VALUE;

	if ( V_IsAbsolutePath( pWildCard ) )
	{
		// Absolute path, cannot be VPK or Pak
		pFindData->findHandle = FS_FindFirstFile( pWildCard, &pFindData->findData );
		pFindData->currentSearchPathID = -1;
	}
	else
	{
		intp c = m_SearchPaths.Count();
		for(	pFindData->currentSearchPathID = 0;
				pFindData->currentSearchPathID < c;
				pFindData->currentSearchPathID++ )
		{
			CSearchPath *pSearchPath = &m_SearchPaths[pFindData->currentSearchPathID];

			if ( FilterByPathID( pSearchPath, pFindData->m_FilterPathID ) )
				continue;

			// already visited this path?
			if ( pFindData->m_VisitedSearchPaths.MarkVisit( *pSearchPath ) )
				continue;

			// If this is a VPK or Pak file, build list of matches now and use helper
			bool bIsVPKOrPak = false;
			if ( pSearchPath->GetPackFile() )
			{
				// XXX(johns) This support didn't exist for a long time, and I'm now worried about various things
				//            looking for misc files suddenly finding them in the (untrusted) BSP and causing security
				//            nightmares. For now, restricting FindFirst() support to BSPs only when the BSP search path
				//            is explicitly requested, but this would otherwise work fine.
				if ( !pPathID || V_strcmp( pPathID, "BSP" ) != 0 )
				{
					continue;
				}

				Assert( pFindData->m_dirMatchesFromVPKOrPak.Count() == 0 );
				Assert( pFindData->m_fileMatchesFromVPKOrPak.Count() == 0 );
				pSearchPath->GetPackFile()->GetFileAndDirLists( pWildCard, pFindData->m_dirMatchesFromVPKOrPak, pFindData->m_fileMatchesFromVPKOrPak, true );
				bIsVPKOrPak = true;
			}

#ifdef SUPPORT_PACKED_STORE
			if ( pSearchPath->GetPackedStore() )
			{
				Assert( pFindData->m_dirMatchesFromVPKOrPak.Count() == 0 );
				Assert( pFindData->m_fileMatchesFromVPKOrPak.Count() == 0 );
				pSearchPath->GetPackedStore()->GetFileAndDirLists( pWildCard, pFindData->m_dirMatchesFromVPKOrPak, pFindData->m_fileMatchesFromVPKOrPak, true );
				bIsVPKOrPak = true;
			}
#endif

			if ( bIsVPKOrPak )
			{
				if ( FindNextFileInVPKOrPakHelper( pFindData ) )
				{
					// Remember that we visited this file already.
					pFindData->m_VisitedFiles.Insert( pFindData->findData.cFileName, 0 );
					*pHandle = hTmpHandle;
					return pFindData->findData.cFileName;
				}
				continue;
			}

			// Otherwise, raw FS find for relative path
			char pTmpFileName[ MAX_FILEPATH ];
			V_sprintf_safe( pTmpFileName, "%s%s", pSearchPath->GetPathString(), pFindData->wildCardString.Base() );
			V_FixSlashes( pTmpFileName );
			pFindData->findHandle = FS_FindFirstFile( pTmpFileName, &pFindData->findData );
			pFindData->m_CurrentStoreID = pSearchPath->m_storeId;

			if( pFindData->findHandle != INVALID_HANDLE_VALUE )
				break;
		}
	}

	// We have a result from the filesystem
 	if( pFindData->findHandle != INVALID_HANDLE_VALUE )
	{
		// Remember that we visited this file already.
		pFindData->m_VisitedFiles.Insert( pFindData->findData.cFileName, 0 );

		if ( pFoundStoreID )
			*pFoundStoreID = pFindData->m_CurrentStoreID;

		*pHandle = hTmpHandle;
		return pFindData->findData.cFileName;
	}

	// Handle failure here
	pFindData = 0;
	m_FindData.Remove(hTmpHandle);
	// dimhotepus: FileFindHandle_t: int -> unsigned short
	*pHandle = std::numeric_limits<FileFindHandle_t>::max();

	return nullptr;
}

const char *CBaseFileSystem::FindFirst( const char *pWildCard, FileFindHandle_t *pHandle )
{
	return FindFirstEx( pWildCard, nullptr, pHandle );
}


// Get the next file, trucking through the path. . don't check for duplicates.
bool CBaseFileSystem::FindNextFileHelper( FindData_t *pFindData, int *pFoundStoreID )
{
	// Try the same search path that we were already searching on.
	if( FS_FindNextFile( pFindData->findHandle, &pFindData->findData ) )
	{
		if ( pFoundStoreID )
			*pFoundStoreID = pFindData->m_CurrentStoreID;

		return true;
	}

	if ( FindNextFileInVPKOrPakHelper( pFindData ) )
		return true;

	// This happens when we searched a full path
	if ( pFindData->currentSearchPathID < 0 )
		return false;

	pFindData->currentSearchPathID++;

	if ( pFindData->findHandle != INVALID_HANDLE_VALUE )
	{
		FS_FindClose( pFindData->findHandle );
	}
	pFindData->findHandle = INVALID_HANDLE_VALUE;

	intp c = m_SearchPaths.Count();
	for( ; pFindData->currentSearchPathID < c; ++pFindData->currentSearchPathID ) 
	{
		CSearchPath *pSearchPath = &m_SearchPaths[pFindData->currentSearchPathID];

		if ( FilterByPathID( pSearchPath, pFindData->m_FilterPathID ) )
			continue;

		// already visited this path
		if ( pFindData->m_VisitedSearchPaths.MarkVisit( *pSearchPath ) )
			continue;

		if ( pSearchPath->GetPackFile() )
		{
			// XXX(johns) This support didn't exist for a long time, and I'm now worried about various things
			//            looking for misc files suddenly finding them in the (untrusted) BSP and causing security
			//            nightmares. For now, restricting FindFirst() support to BSPs only when the BSP search path
			//            is explicitly requested, but this would otherwise work fine.
			if ( !pFindData->m_FilterPathID || V_strcmp( g_PathIDTable.String( pFindData->m_FilterPathID ), "BSP" ) != 0 )
			{
				continue;
			}
			Assert( pFindData->m_dirMatchesFromVPKOrPak.Count() == 0 );
			Assert( pFindData->m_fileMatchesFromVPKOrPak.Count() == 0 );
			pSearchPath->GetPackFile()->GetFileAndDirLists( pFindData->wildCardString.Base(), pFindData->m_dirMatchesFromVPKOrPak, pFindData->m_fileMatchesFromVPKOrPak, true );
			if ( FindNextFileInVPKOrPakHelper( pFindData ) )
				return true;
			continue;
		}

#ifdef SUPPORT_PACKED_STORE
			if ( pSearchPath->GetPackedStore() )
			{
				Assert( pFindData->m_dirMatchesFromVPKOrPak.Count() == 0 );
				Assert( pFindData->m_fileMatchesFromVPKOrPak.Count() == 0 );
				pSearchPath->GetPackedStore()->GetFileAndDirLists( pFindData->wildCardString.Base(), pFindData->m_dirMatchesFromVPKOrPak, pFindData->m_fileMatchesFromVPKOrPak, true );
				if ( FindNextFileInVPKOrPakHelper( pFindData ) )
					return true;
				continue;
			}
#endif

		char pTmpFileName[ MAX_FILEPATH ];
		V_sprintf_safe( pTmpFileName, "%s%s", pSearchPath->GetPathString(), pFindData->wildCardString.Base() );
		V_FixSlashes( pTmpFileName );
		pFindData->findHandle = FS_FindFirstFile( pTmpFileName, &pFindData->findData );
		pFindData->m_CurrentStoreID = pSearchPath->m_storeId;
		if( pFindData->findHandle != INVALID_HANDLE_VALUE )
		{
			if ( pFoundStoreID )
				*pFoundStoreID = pFindData->m_CurrentStoreID;

			return true;
		}
	}

	return false;
}

bool CBaseFileSystem::FindNextFileInVPKOrPakHelper( FindData_t *pFindData )
{
	// Return the next one from the list of VPK matches if there is one
	if ( pFindData->m_fileMatchesFromVPKOrPak.Count() > 0 )
	{
		V_strcpy_safe( pFindData->findData.cFileName, V_UnqualifiedFileName( pFindData->m_fileMatchesFromVPKOrPak[0] ) );
		pFindData->findData.dwFileAttributes = 0;
		delete pFindData->m_fileMatchesFromVPKOrPak.Head();
		pFindData->m_fileMatchesFromVPKOrPak.RemoveMultipleFromHead( 1 );

		return true;
	}

	// Return the next one from the list of VPK matches if there is one
	if ( pFindData->m_dirMatchesFromVPKOrPak.Count() > 0 )
	{
		V_strcpy_safe( pFindData->findData.cFileName, V_UnqualifiedFileName( pFindData->m_dirMatchesFromVPKOrPak[0] ) );
		pFindData->findData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		delete pFindData->m_dirMatchesFromVPKOrPak.Head();
		pFindData->m_dirMatchesFromVPKOrPak.RemoveMultipleFromHead( 1 );

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
// Output : const char
//-----------------------------------------------------------------------------
const char *CBaseFileSystem::FindNext( FileFindHandle_t handle )
{
	VPROF_BUDGET( "CBaseFileSystem::FindNext", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );
	FindData_t *pFindData = &m_FindData[handle];

	while( 1 )
	{
		if( FindNextFileHelper( pFindData, nullptr ) )
		{
			if ( pFindData->m_VisitedFiles.Find( pFindData->findData.cFileName ) == -1 )
			{
				pFindData->m_VisitedFiles.Insert( pFindData->findData.cFileName, 0 );
				return pFindData->findData.cFileName;
			}
		}
		else
		{
			return nullptr;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::FindIsDirectory( FileFindHandle_t handle )
{
	FindData_t *pFindData = &m_FindData[handle];
	return !!( pFindData->findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::FindClose( FileFindHandle_t handle )
{
	if ( ( handle < 0 ) || ( !m_FindData.IsInList( handle ) ) )
		return;

	FindData_t *pFindData = &m_FindData[handle];
	Assert(pFindData);

	if ( pFindData->findHandle != INVALID_HANDLE_VALUE)
	{
		FS_FindClose( pFindData->findHandle );
	}
	pFindData->findHandle = INVALID_HANDLE_VALUE;

	pFindData->wildCardString.Purge();
	pFindData->m_fileMatchesFromVPKOrPak.PurgeAndDeleteElements();
	pFindData->m_dirMatchesFromVPKOrPak.PurgeAndDeleteElements();
	m_FindData.Remove( handle );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFileName - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::GetLocalCopy( const char *pFileName )
{
	// do nothing. . everything is local.
}

//-----------------------------------------------------------------------------
// Purpose: Fixes up Path names.  Will fix up platform specific slashes, remove
//          any ../ or ./, fix //s, and lowercase anything under the directory
//          that the game is installed to.  We expect all files to be lower cased
//          there - especially on Linux (where case sensitivity is the norm). 
//
// Input  : *pFileName - Original name to convert
//          *pFixedUpFileName - a buffer to put the converted filename into
//          sizeFixedUpFileName - the size of the above buffer in chars 
//-----------------------------------------------------------------------------
bool CBaseFileSystem::FixUpPath( const char *pFileName, OUT_Z_CAP(sizeFixedUpFileName) char *pFixedUpFileName, int sizeFixedUpFileName )
{
	//  If appropriate fixes up the filename to ensure that it's handled properly by the system.
	//
	V_strncpy( pFixedUpFileName, pFileName, sizeFixedUpFileName );
	V_FixSlashes( pFixedUpFileName, CORRECT_PATH_SEPARATOR );
	V_FixDoubleSlashes( pFixedUpFileName );

	if ( !V_IsAbsolutePath( pFixedUpFileName ) )
	{
		V_strlower( pFixedUpFileName );
	}
	else 
	{
		// raphael: BASE_PATH already in m_pBaseDir as quering it here wastes performance.
		// If the first part of the pFixedUpFilename is pBaseDir
		// then lowercase the part after that.
		if ( !Q_isempty(m_pBaseDir) && m_iBaseLength > 0 &&
			(m_iBaseLength+1 < V_strlen( pFixedUpFileName ) ) &&
			(0 != V_strncmp( m_pBaseDir, pFixedUpFileName, m_iBaseLength ) )  )
		{
			V_strlower( &pFixedUpFileName[m_iBaseLength-1] );
		}
	}

//	Msg("CBaseFileSystem::FixUpPath: Converted %s to %s\n", pFileName, pFixedUpFileName);  // too noisy

#ifdef NEVER // Useful if you're trying to see why your file may not be found (if you have a mixed case file)
	if (strncmp(pFixedUpFileName, pFileName, 256))
	{
	    printf("FixUpPath->Converting %s to %s\n",pFileName, pFixedUpFileName);
	}
#endif // NEVER
	return true;
}


//-----------------------------------------------------------------------------
// Converts a partial path into a full path
// Relative paths that are pack based are returned as an absolute path .../zip?.zip/foo
// A pack absolute path can be sent back in for opening, and the file will be properly
// detected as pack based and mounted inside the pack.
//-----------------------------------------------------------------------------
const char *CBaseFileSystem::RelativePathToFullPath( const char *pFileName, const char *pPathID, OUT_Z_CAP(maxLenInChars) char *pDest, int maxLenInChars, PathTypeFilter_t pathFilter, PathTypeQuery_t *pPathType )
{
	CHECK_DOUBLE_SLASHES( pFileName );

	struct _stat buf;

	if ( pPathType )
	{
		*pPathType = PATH_IS_NORMAL;
	}

	// Convert filename to lowercase.  All files in the
	// game logical filesystem must be accessed by lowercase name
	char szLowercaseFilename[ MAX_PATH ];
	FixUpPath( pFileName, szLowercaseFilename );
	pFileName = szLowercaseFilename;

	// Fill in the default in case it's not found...
	V_strncpy( pDest, pFileName, maxLenInChars );

// @FD This is arbitrary and seems broken.  If the caller needs this filter, they should
//     request it with the flag themselves.  As it is, I cannot search all the file paths
//     for a file using this function because there is no option that says, "No, really, I
//     mean ALL SEARCH PATHS."  The current problem I'm trying to fix is that sounds are not
//     working if they are in the BSP.  I wrote code that assumed that I could just ask for
//     the absolute path of a file, since we are able to open files with these absolute
//     filenames, and that each particular filesystem call wouldn't have its own individual
//     quirks.
//	if ( IsPC() && pathFilter == FILTER_NONE )
//	{
//		// X360TBD: PC legacy behavior never returned pack paths
//		// do legacy behavior to ensure naive callers don't break
//		pathFilter = FILTER_CULLPACK;
//	}


	CSearchPathsIterator iter( this, &pFileName, pPathID, pathFilter );
	for ( CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != nullptr; pSearchPath = iter.GetNext() )
	{
		CPackFile *pPack = pSearchPath->GetPackFile();
		if ( pPack )
		{
			if ( pPack->ContainsFile( pFileName ) )
			{
				if ( pPathType )
				{
					if ( pPack->m_bIsMapPath )
					{
						*pPathType |= PATH_IS_MAPPACKFILE;
					}
					else
					{
						*pPathType |= PATH_IS_PACKFILE;
					}
					if ( pSearchPath->m_bIsRemotePath )
					{
						*pPathType |= PATH_IS_REMOTE;
					}
				}

				// form an encoded absolute path that can be decoded by our FS as pak based
				const char *pszPackName = pPack->m_ZipName.String();
				intp len = V_strlen( pszPackName );
				intp nTotalLen = len + 1 + V_strlen( pFileName );
				if ( nTotalLen >= maxLenInChars )
				{
					::Warning( "File %s was found in %s, but resulting abs filename won't fit in callers buffer of %d bytes\n",
						pFileName, pszPackName, maxLenInChars );
					Assert( false );
					return nullptr;
				}

				V_strncpy( pDest, pszPackName, maxLenInChars );
				V_AppendSlash( pDest, maxLenInChars );
				V_strncat( pDest, pFileName, maxLenInChars ); 
				Assert( V_strlen( pDest ) == nTotalLen );
				return pDest;
			}

			continue;
		}

		// Found in VPK?
#ifdef SUPPORT_PACKED_STORE
			CPackedStore *pVPK = pSearchPath->GetPackedStore();
			if ( pVPK )
			{
				CPackedStoreFileHandle vpkHandle = pVPK->OpenFile( pFileName );
				if ( vpkHandle )
				{
					const char *pszVpkName = vpkHandle.m_pOwner->FullPathName();
					Assert( V_GetFileExtension( pszVpkName ) != nullptr );

					intp len = V_strlen( pszVpkName );
					intp nTotalLen = len + 1 + V_strlen( pFileName );
					if ( nTotalLen >= maxLenInChars )
					{
						::Warning( "File %s was found in %s, but resulting abs filename won't fit in callers buffer of %d bytes\n",
							pFileName, pszVpkName, maxLenInChars );
						Assert( false );
						return nullptr;
					}

					V_strncpy( pDest, pszVpkName, maxLenInChars );
					V_AppendSlash( pDest, maxLenInChars );
					V_strncat( pDest, pFileName, maxLenInChars );
					V_FixSlashes( pDest );
					return pDest;
				}
				continue;
			}
#endif

		char pTmpFileName[ MAX_FILEPATH ];
		V_sprintf_safe( pTmpFileName, "%s%s", pSearchPath->GetPathString(), pFileName );
		V_FixSlashes( pTmpFileName );
		if ( FS_stat( pTmpFileName, &buf ) != -1 )
		{
			V_strncpy( pDest, pTmpFileName, maxLenInChars );
			if ( pPathType && pSearchPath->m_bIsRemotePath )
			{
				*pPathType |= PATH_IS_REMOTE;
			}
			return pDest;
		}
	}

	// not found
	return nullptr;
}

const char *CBaseFileSystem::GetLocalPath( const char *pFileName, OUT_Z_CAP(maxLenInChars) char *pDest, int maxLenInChars )
{
	CHECK_DOUBLE_SLASHES( pFileName );

	return RelativePathToFullPath( pFileName, nullptr, pDest, maxLenInChars );
}


//-----------------------------------------------------------------------------
// Returns true on success, otherwise false if it can't be resolved
//-----------------------------------------------------------------------------
bool CBaseFileSystem::FullPathToRelativePathEx( const char *pFullPath, const char *pPathId, OUT_Z_CAP(maxLenInChars) char *pDest, int maxLenInChars )
{
	CHECK_DOUBLE_SLASHES( pFullPath );

	intp nInlen = V_strlen( pFullPath );
	if ( nInlen <= 0 )
	{
		pDest[ 0 ] = 0;
		return false;
	}

	V_strncpy( pDest, pFullPath, maxLenInChars );

	char pInPath[ MAX_FILEPATH ];
	V_strcpy_safe( pInPath, pFullPath );
#ifdef _WIN32
	V_strlower( pInPath );
#endif
	V_FixSlashes( pInPath );

	CUtlSymbol lookup;
	if ( pPathId )
	{
		lookup = g_PathIDTable.AddString( pPathId );
	}

	for( auto &sp : m_SearchPaths )
	{
		// FIXME: Should this work with embedded pak files?
		if ( sp.GetPackFile() && sp.GetPackFile()->m_bIsMapPath )
			continue;

		// Skip paths that are not on the specified search path
		if ( FilterByPathID( &sp, lookup ) )
			continue;

		char pSearchBase[ MAX_FILEPATH ];
		V_strcpy_safe( pSearchBase, sp.GetPathString() );
#ifdef _WIN32
		V_strlower( pSearchBase );
#endif
		V_FixSlashes( pSearchBase );

		intp nSearchLen = V_strlen( pSearchBase );
		if ( V_strnicmp( pSearchBase, pInPath, nSearchLen ) )
			continue;

		V_strncpy( pDest, &pInPath[ nSearchLen ], maxLenInChars );
		return true;
	}

	return false;
}

	
//-----------------------------------------------------------------------------
// Obsolete version
//-----------------------------------------------------------------------------
bool CBaseFileSystem::FullPathToRelativePath( const char *pFullPath, OUT_Z_CAP(maxLenInChars) char *pDest, int maxLenInChars )
{
	return FullPathToRelativePathEx( pFullPath, nullptr, pDest, maxLenInChars );
}


//-----------------------------------------------------------------------------
// Returns true on successfully retrieve case-sensitive full path, otherwise false
//-----------------------------------------------------------------------------
bool CBaseFileSystem::GetCaseCorrectFullPath_Ptr( const char *pFullPath, OUT_Z_CAP(maxLenInChars) char *pDest, int maxLenInChars )
{
	CHECK_DOUBLE_SLASHES( pFullPath );
	
#ifndef _WIN32
	V_strncpy( pDest, pFullPath, maxLenInChars );
	return true;
#else

	char szCurrentDir[MAX_PATH];
	V_strcpy_safe( szCurrentDir, pFullPath );
	V_StripLastDir( szCurrentDir );

	CUtlString strSearchName = pFullPath;
	strSearchName.StripTrailingSlash();
	strSearchName = strSearchName.Slice( V_strlen( szCurrentDir ), strSearchName.Length() );

	CUtlString strSearchPath = szCurrentDir;
	strSearchPath += "*";

	CUtlString strFoundCaseCorrectName;
	FileFindHandle_t findHandle = FILESYSTEM_INVALID_FIND_HANDLE;
	const char *pszCaseCorrectName = FindFirst( strSearchPath.Get(), &findHandle );
	while ( pszCaseCorrectName )
	{
		if ( V_stricmp( strSearchName.String(), pszCaseCorrectName ) == 0 )
		{
			strFoundCaseCorrectName = pszCaseCorrectName;
			break;
		}
		pszCaseCorrectName = FindNext( findHandle );
	}
	FindClose( findHandle );

	// Not found
	if ( strFoundCaseCorrectName.IsEmpty() )
	{
		V_strncpy( pDest, pFullPath, maxLenInChars );
		return false;
	}

	// If drive path, no need to recurse anymore.
	bool bResult = false;
	char szDir[MAX_PATH];
	if ( !IsDirectory( szCurrentDir, nullptr ) )
	{
		V_strupr( szCurrentDir );
		V_strcpy_safe( szDir, szCurrentDir );
		bResult = true;
	}
	else
	{
		bResult = GetCaseCorrectFullPath( szCurrentDir, szDir );
	}

	// connect the current path with the case-correct dir/file name
	V_MakeAbsolutePath( pDest, maxLenInChars, strFoundCaseCorrectName.String(), szDir );

	return bResult;
#endif // _WIN32
}



//-----------------------------------------------------------------------------
// Deletes a file
//-----------------------------------------------------------------------------
void CBaseFileSystem::RemoveFile( char const* pRelativePath, const char *pathID )
{
	CHECK_DOUBLE_SLASHES( pRelativePath );

	// Allow for UNC-type syntax to specify the path ID.
	char tempPathID[MAX_PATH];
	ParsePathID( pRelativePath, pathID, tempPathID );

	Assert( pathID || !IsX360() );

	// Opening for write or append uses Write Path
	char szScratchFileName[MAX_PATH];
	if ( V_IsAbsolutePath( pRelativePath ) )
	{
		V_strcpy_safe( szScratchFileName, pRelativePath );
	}
	else
	{
		ComputeFullWritePath( szScratchFileName, sizeof( szScratchFileName ), pRelativePath, pathID );
	}

	if ( unlink( szScratchFileName ) )
	{
		Warning( FILESYSTEM_WARNING, "Unable to remove file '%s': %s.\n",
			szScratchFileName,
			std::generic_category().message(errno).c_str() );
	}
}


//-----------------------------------------------------------------------------
// Renames a file
//-----------------------------------------------------------------------------
bool CBaseFileSystem::RenameFile( char const *pOldPath, char const *pNewPath, const char *pathID )
{
	Assert( pOldPath && pNewPath );

	CHECK_DOUBLE_SLASHES( pOldPath );
	CHECK_DOUBLE_SLASHES( pNewPath );

	// Allow for UNC-type syntax to specify the path ID.
	char pPathIdCopy[MAX_PATH];
	const char *pOldPathId = pathID;
	if ( pathID )
	{
		V_strcpy_safe( pPathIdCopy, pathID );
		pOldPathId = pPathIdCopy;
	}

	char tempOldPathID[MAX_PATH];
	ParsePathID( pOldPath, pOldPathId, tempOldPathID );
	Assert( pOldPathId );

	// Allow for UNC-type syntax to specify the path ID.
	char tempNewPathID[MAX_PATH];
	ParsePathID( pNewPath, pathID, tempNewPathID );
	Assert( pathID );

	char pNewFileName[ MAX_PATH ];
	char szScratchFileName[MAX_PATH];

	// The source file may be in a fallback directory, so just resolve the actual path, don't assume pathid...
	RelativePathToFullPath_safe( pOldPath, pOldPathId, szScratchFileName );

	// Figure out the dest path
	if ( !V_IsAbsolutePath( pNewPath ) )
	{
		ComputeFullWritePath( pNewFileName, sizeof( pNewFileName ), pNewPath, pathID );
	}
	else
	{
		V_strcpy_safe( pNewFileName, pNewPath );
	}

	// Make sure the directory exitsts, too
	char pPathOnly[ MAX_PATH ];
	V_strcpy_safe( pPathOnly, pNewFileName );
	V_StripFilename( pPathOnly );
	CreateDirHierarchy( pPathOnly, pathID );

	// Now copy the file over.
	if ( rename( szScratchFileName, pNewFileName ) )
	{
		::Warning( "Unable to rename file '%s' to '%s': %s.\n",
			szScratchFileName,
			pNewFileName,
			std::generic_category().message(errno).c_str() );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : **ppdir - 
//-----------------------------------------------------------------------------
bool CBaseFileSystem::GetCurrentDirectory( char* pDirectory, int maxlen )
{
#if defined( _WIN32 )
	if ( !_getcwd( pDirectory, maxlen ) )
#elif defined( POSIX )
	if ( !getcwd( pDirectory, maxlen ) )
#endif
		return false;

	V_FixSlashes( pDirectory );

	// Strip the last slash
	const intp len = strlen( pDirectory );
	if ( len > 0 && pDirectory[ len - 1 ] == CORRECT_PATH_SEPARATOR )
	{
		pDirectory[ len - 1 ] = '\0';
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pfnWarning - warning function callback
//-----------------------------------------------------------------------------
void CBaseFileSystem::SetWarningFunc( void (*pfnWarning)( const char *fmt, ... ) )
{
	m_pfnWarning = pfnWarning;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : level - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::SetWarningLevel( FileWarningLevel_t level )
{
	m_fwLevel = level;
}

const FileSystemStatistics *CBaseFileSystem::GetFilesystemStatistics()
{
	return &m_Stats;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : level - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::Warning( FileWarningLevel_t level, PRINTF_FORMAT_STRING const char *fmt, ... )
{
	if ( level > m_fwLevel )
		return;

	if ( ( fs_warning_mode.GetInt() == 1 && !ThreadInMainThread() ) || ( fs_warning_mode.GetInt() == 2 && ThreadInMainThread() ) )
		return;

	va_list argptr; 
	char warningtext[ 4096 ];
 
	va_start( argptr, fmt ); //-V2018 //-V2019
	V_vsprintf_safe( warningtext, fmt, argptr );
	va_end( argptr );

	// Dump to stdio
	fprintf( stderr, "%s", warningtext );
	if ( m_pfnWarning )
	{
		(*m_pfnWarning)( warningtext );
	}
	else
	{
#ifdef _WIN32
		Plat_DebugString( warningtext );
#endif
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseFileSystem::COpenedFile::~COpenedFile( void )
{
	delete[] m_pName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : src - 
//-----------------------------------------------------------------------------
CBaseFileSystem::COpenedFile::COpenedFile( const COpenedFile& src ) : m_pName{nullptr} {
	m_pFile = src.m_pFile;
	if ( src.m_pName )
	{
		SetName( src.m_pName );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : src - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::COpenedFile::operator==( const CBaseFileSystem::COpenedFile& src ) const
{
	return src.m_pFile == m_pFile;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::COpenedFile::SetName( char const *name )
{
	delete[] m_pName;
	m_pName = V_strdup( name );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char
//-----------------------------------------------------------------------------
char const *CBaseFileSystem::COpenedFile::GetName( void ) const
{
	return m_pName ? m_pName : "???";
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseFileSystem::CSearchPath::CSearchPath( void )
{
	m_Path = g_PathIDTable.AddString( "" );
	m_pDebugPath = "";

	m_storeId = 0;
	m_pPackFile = nullptr;
	m_pPathIDInfo = nullptr;
	m_bIsRemotePath = false;
	m_pPackedStore = nullptr;
	m_bIsTrustedForPureServer = false;
}

const char *CBaseFileSystem::CSearchPath::GetDebugString() const
{
	if ( GetPackFile()  )
	{
		return GetPackFile()->m_ZipName;
	}
#ifdef SUPPORT_PACKED_STORE
	if ( GetPackedStore()  )
	{
		return GetPackedStore()->FullPathName();
	}
#endif
	return GetPathString();
}

bool CBaseFileSystem::CSearchPath::IsMapPath() const
{
	return GetPackFile()->m_bIsMapPath;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseFileSystem::CSearchPath::~CSearchPath()
{
	if ( m_pPackFile )
	{	
		m_pPackFile->Release();
	}
	if ( m_pPackedStore )
	{
		m_pPackedStore->Release();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseFileSystem::CSearchPath *CBaseFileSystem::CSearchPathsIterator::GetFirst()
{
	if ( m_SearchPaths.Count() )
	{
		m_visits.Reset();
		m_iCurrent = -1;
		return GetNext();
	}
	return &m_EmptySearchPath;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseFileSystem::CSearchPath *CBaseFileSystem::CSearchPathsIterator::GetNext()
{
	CSearchPath *pSearchPath = nullptr;

	for ( m_iCurrent++; m_iCurrent < m_SearchPaths.Count(); m_iCurrent++ )
	{
		pSearchPath = &m_SearchPaths[m_iCurrent];

		if ( m_PathTypeFilter == FILTER_CULLPACK && pSearchPath->GetPackFile() )
			continue;

		if ( m_PathTypeFilter == FILTER_CULLNONPACK && !pSearchPath->GetPackFile() )
			continue;

		if ( CBaseFileSystem::FilterByPathID( pSearchPath, m_pathID ) )
			continue;

		if ( !m_visits.MarkVisit( *pSearchPath ) )
			break;
	}

	if ( m_iCurrent < m_SearchPaths.Count() )
	{
		return pSearchPath;
	}

	return nullptr;
}

void CBaseFileSystem::CSearchPathsIterator::CopySearchPaths( const CUtlVector<CSearchPath>	&searchPaths )
{
	m_SearchPaths = searchPaths;
	for ( auto &sp : m_SearchPaths )
	{
		if ( sp.GetPackFile() )
		{
			sp.GetPackFile()->AddRef();
		}
		else if ( sp.GetPackedStore() )
		{
			sp.GetPackedStore()->AddRef();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Load/unload a DLL
//-----------------------------------------------------------------------------
CSysModule *CBaseFileSystem::LoadModule( const char *pFileName, const char *pPathID, bool bValidatedDllOnly )
{
	CHECK_DOUBLE_SLASHES( pFileName );

	// dimhotepus: Load directly by system if absolute path.
	if ( V_IsAbsolutePath( pFileName ) )
	{
		return Sys_LoadModule( pFileName );
	}

	LogFileAccess( pFileName );
	if ( !pPathID )
	{
		pPathID = "EXECUTABLE_PATH"; // default to the bin dir
	}

	char tempPathID[ MAX_PATH ];
	ParsePathID( pFileName, pPathID, tempPathID );
	
	CUtlSymbol lookup = g_PathIDTable.AddString( pPathID );

	// a pathID has been specified, find the first match in the path list
	for ( auto &sp : m_SearchPaths )
	{
		// pak files don't have modules
		if ( sp.GetPackFile() )
			continue;

		if ( FilterByPathID( &sp, lookup ) )
			continue;

		V_sprintf_safe( tempPathID, "%s%s", sp.GetPathString(), pFileName ); // append the path to this dir.
		CSysModule *pModule = Sys_LoadModule( tempPathID );
		if ( pModule ) 
		{
			// we found the binary in one of our search paths
			return pModule;
		}
	}

	// couldn't load it from any of the search paths, let LoadLibrary try
	// dimhotepus: Prevent loading something placed into current directory!
	// return Sys_LoadModule( pFileName );
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::UnloadModule( CSysModule *pModule )
{
	Sys_UnloadModule( pModule );
}

//-----------------------------------------------------------------------------
// Purpose: Adds a filesystem logging function
//-----------------------------------------------------------------------------
void CBaseFileSystem::AddLoggingFunc( FileSystemLoggingFunc_t logFunc )
{
	Assert(!m_LogFuncs.IsValidIndex(m_LogFuncs.Find(logFunc)));
	m_LogFuncs.AddToTail(logFunc);
}

//-----------------------------------------------------------------------------
// Purpose: Removes a filesystem logging function
//-----------------------------------------------------------------------------
void CBaseFileSystem::RemoveLoggingFunc( FileSystemLoggingFunc_t logFunc )
{
	m_LogFuncs.FindAndRemove(logFunc);
}

//-----------------------------------------------------------------------------
// Make sure that slashes are of the right kind and that there is a slash at the 
// end of the filename.
// WARNING!!: assumes that you have an extra byte allocated in the case that you need
// a slash at the end.
//-----------------------------------------------------------------------------
static void AddSeperatorAndFixPath( char *str )
{
	char *lastChar = &str[strlen( str ) - 1];
	if( *lastChar != CORRECT_PATH_SEPARATOR && *lastChar != INCORRECT_PATH_SEPARATOR )
	{
		lastChar[1] = CORRECT_PATH_SEPARATOR;
		lastChar[2] = '\0';
	}
	V_FixSlashes( str );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFileName - 
// Output : FileNameHandle_t
//-----------------------------------------------------------------------------
FileNameHandle_t CBaseFileSystem::FindOrAddFileName( char const *pFileName )
{
	return m_FileNames.FindOrAddFileName( pFileName );
}

FileNameHandle_t CBaseFileSystem::FindFileName( char const *pFileName )
{
	return m_FileNames.FindFileName( pFileName );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
// Output : char const
//-----------------------------------------------------------------------------
bool CBaseFileSystem::String( const FileNameHandle_t& handle, OUT_Z_CAP(buflen) char *buf, intp buflen )
{
	return m_FileNames.String( handle, buf, buflen );
}

int	CBaseFileSystem::GetPathIndex( const FileNameHandle_t &handle )
{
	return m_FileNames.PathIndex(handle);
}

CBaseFileSystem::CPathIDInfo* CBaseFileSystem::FindOrAddPathIDInfo( const CUtlSymbol &id, int bByRequestOnly )
{
	for ( auto *info : m_PathIDInfos )
	{
		if ( info->GetPathID() == id )
		{
			if ( bByRequestOnly != -1 )
			{
				info->m_bByRequestOnly = bByRequestOnly != 0;
			}
			return info;
		}
	}

	// Add a new one.
	auto *pInfo = new CBaseFileSystem::CPathIDInfo;
	m_PathIDInfos.AddToTail( pInfo );
	pInfo->SetPathID( id );
	pInfo->m_bByRequestOnly = (bByRequestOnly == 1);
	return pInfo;
}

void CBaseFileSystem::MarkPathIDByRequestOnly( const char *pPathID, bool bRequestOnly )
{
	FindOrAddPathIDInfo( g_PathIDTable.AddString( pPathID ), bRequestOnly );
}

#if defined( TRACK_BLOCKING_IO )

void CBaseFileSystem::EnableBlockingFileAccessTracking( bool state )
{
	m_bBlockingFileAccessReportingEnabled = state;
}

bool CBaseFileSystem::IsBlockingFileAccessEnabled() const
{
	return m_bBlockingFileAccessReportingEnabled;
}

IBlockingFileItemList *CBaseFileSystem::RetrieveBlockingFileAccessInfo()
{
	Assert( m_pBlockingItems );
	return m_pBlockingItems;
}

void CBaseFileSystem::RecordBlockingFileAccess( bool synchronous, const FileBlockingItem& item )
{
	AUTO_LOCK( m_BlockingFileMutex );

	// Not tracking anything
	if ( !m_bBlockingFileAccessReportingEnabled )
		return;

	if ( synchronous && !m_bAllowSynchronousLogging && ( item.m_ItemType == FILESYSTEM_BLOCKING_SYNCHRONOUS ) )
		return;

	// Track it
	m_pBlockingItems->Add( item );
}

bool CBaseFileSystem::SetAllowSynchronousLogging( bool state )
{
	bool oldState = m_bAllowSynchronousLogging;
	m_bAllowSynchronousLogging = state;
	return oldState;
}

void CBaseFileSystem::BlockingFileAccess_EnterCriticalSection()
{
	m_BlockingFileMutex.Lock();
}

void CBaseFileSystem::BlockingFileAccess_LeaveCriticalSection()
{
	m_BlockingFileMutex.Unlock();
}

#endif // TRACK_BLOCKING_IO

bool CBaseFileSystem::GetFileTypeForFullPath( char const *pFullPath, wchar_t *buf, size_t bufSizeInBytes )
{
#if !defined( POSIX )
	wchar_t wcharpath[512];
	V_UTF8ToUnicode( pFullPath, wcharpath, sizeof(wcharpath) );

	SHFILEINFOW info = {};
	DWORD_PTR dwResult = SHGetFileInfoW( 
		wcharpath,
		0,
		&info,
		sizeof( info ),
		SHGFI_TYPENAME 
	);
	if ( dwResult )
	{
		V_wcsncpy( buf, info.szTypeName, bufSizeInBytes );
		return true;
	}

#endif
	char ext[32];
	V_ExtractFileExtension( pFullPath, ext );
#ifdef POSIX		
	swprintf( buf, ( bufSizeInBytes / sizeof( wchar_t ) ) - 1, L"%s", V_strupr( ext ) ); // Matches what Windows does
#else
	_snwprintf( buf, ( bufSizeInBytes / sizeof( wchar_t ) ) - 1, L".%S", ext );
#endif
	buf[( bufSizeInBytes / sizeof( wchar_t ) ) - 1] = L'\0';

	return false;
}


bool CBaseFileSystem::GetOptimalIOConstraints( FileHandle_t hFile, unsigned *pOffsetAlign, unsigned *pSizeAlign, unsigned *pBufferAlign )
{
	if ( pOffsetAlign )
		*pOffsetAlign = 1;
	if ( pSizeAlign )
		*pSizeAlign = 1;
	if ( pBufferAlign )
		*pBufferAlign = 1;
	return false;
}

//-----------------------------------------------------------------------------

FileCacheHandle_t CBaseFileSystem::CreateFileCache( )
{
	return new CFileCacheObject( this );
}

//-----------------------------------------------------------------------------

void CBaseFileSystem::AddFilesToFileCache( FileCacheHandle_t cacheId, const char **ppFileNames, int nFileNames, const char *pPathID )
{
	// For now, assuming that we're only used with GAME.
	Assert( pPathID && V_strcasecmp( pPathID, "GAME" ) == 0 );
	return static_cast< CFileCacheObject * >( cacheId )->AddFiles( ppFileNames, nFileNames );
}

//-----------------------------------------------------------------------------

bool CBaseFileSystem::IsFileCacheLoaded( FileCacheHandle_t cacheId )
{
	return static_cast< CFileCacheObject * >( cacheId )->IsReady();
}

//-----------------------------------------------------------------------------

void CBaseFileSystem::DestroyFileCache( FileCacheHandle_t cacheId )
{
	delete static_cast< CFileCacheObject * >( cacheId );
}

//-----------------------------------------------------------------------------

bool CBaseFileSystem::IsFileCacheFileLoaded( FileCacheHandle_t cacheId, const char* pFileName )
{
#ifdef _DEBUG
	auto* pFileCache = static_cast< CFileCacheObject * >( cacheId );
	bool bFileIsHeldByCache = false;
	{
		AUTO_LOCK( pFileCache->m_InfosMutex );
		for ( auto *info : pFileCache->m_Infos )
		{
			if ( info->pFileName && Q_strcmp( info->pFileName, pFileName ) )
			{
				bFileIsHeldByCache = true;
				break;
			}
		}
	}
	Assert( bFileIsHeldByCache );
#endif

	AUTO_LOCK( m_MemoryFileMutex );
	return m_MemoryFileHash.Find( pFileName ) != m_MemoryFileHash.InvalidHandle();
}

//-----------------------------------------------------------------------------

bool CBaseFileSystem::RegisterMemoryFile( CMemoryFileBacking *pFile, CMemoryFileBacking **ppExistingFileWithRef )
{
	Assert( pFile->m_pFS == static_cast< IFileSystem* >( this ) );
	Assert( pFile->m_pFileName && pFile->m_pFileName[0] );
	AUTO_LOCK( m_MemoryFileMutex );
	
	CMemoryFileBacking *pInTable = m_MemoryFileHash[ m_MemoryFileHash.Insert( pFile->m_pFileName, pFile ) ];
	pInTable->m_nRegistered++;
	pInTable->AddRef(); // either for table or for ppExistingFileWithRef return

	if ( pFile == pInTable )
	{
		Assert( pInTable->m_nRegistered == 1 );
		*ppExistingFileWithRef = nullptr;
		return true;
	}

	Assert( pInTable->m_nRegistered > 1 );
	*ppExistingFileWithRef = pInTable;
	return false;
}

//-----------------------------------------------------------------------------

void CBaseFileSystem::UnregisterMemoryFile( CMemoryFileBacking *pFile )
{
	Assert( pFile->m_pFS == static_cast< IFileSystem* >( this ) && pFile->m_nRegistered > 0 );
	bool bRelease = false;
	{
		AUTO_LOCK( m_MemoryFileMutex );
		pFile->m_nRegistered--;
		if ( pFile->m_nRegistered == 0 )
		{
			m_MemoryFileHash.Remove( pFile->m_pFileName );
			bRelease = true;
		}
	}

	// Release potentially a complex op, prefer to perform it outside of mutex.
	if (bRelease)
	{
		pFile->Release();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Constructs a file handle
// Input  : base file system handle
// Output : 
//-----------------------------------------------------------------------------
CFileHandle::CFileHandle( CBaseFileSystem* fs )
{
	Init( fs );
}

CFileHandle::~CFileHandle()
{
	Assert( IsValid() );
#if !defined( _RETAIL )
	delete[] m_pszTrueFileName;
#endif

	if ( m_pPackFileHandle )
	{
		delete m_pPackFileHandle;
		m_pPackFileHandle = nullptr;
	}

	if ( m_pFile )
	{
		m_fs->Trace_FClose( m_pFile );
		m_pFile = nullptr;
	}

	m_nMagic = FREE_MAGIC;
}

void CFileHandle::Init( CBaseFileSystem *fs )
{
	m_nMagic = MAGIC;
	m_pFile = nullptr;
	m_nLength = 0;
	m_type = FT_NORMAL;		
	m_pPackFileHandle = nullptr;

	m_fs = fs;

#if !defined( _RETAIL )
	m_pszTrueFileName = 0;
#endif
}

bool CFileHandle::IsValid() const
{
	return m_nMagic == MAGIC;
}

int CFileHandle::GetSectorSize()
{
	Assert( IsValid() );

	if ( m_pFile )
	{
		return m_fs->FS_GetSectorSize( m_pFile );
	}

	if ( m_pPackFileHandle )
	{
		return m_pPackFileHandle->GetSectorSize();
	}

	return  m_type == FT_MEMORY_BINARY || m_type == FT_MEMORY_TEXT ? 1 : -1;
}

bool CFileHandle::IsOK()
{
#if defined( SUPPORT_PACKED_STORE )
	if ( m_VPKHandle )
	{
		return true;
	}
#endif

	if ( m_pFile )
	{
		return IsValid() && m_fs->FS_ferror( m_pFile ) == 0;
	}

	if ( m_pPackFileHandle || m_type == FT_MEMORY_BINARY || m_type == FT_MEMORY_TEXT )
	{
		return IsValid();
	}

	m_fs->Warning( FILESYSTEM_WARNING, "FS:  Tried to IsOk nullptr file pointer inside valid file handle!\n" );
	return false;
}

void CFileHandle::Flush()
{
	Assert( IsValid() );

	if ( m_pFile )
	{
		m_fs->FS_fflush( m_pFile );
	}
}

void CFileHandle::SetBufferSize( int nBytes )
{
	Assert( IsValid() );

	if ( m_pFile )
	{
		m_fs->FS_setbufsize( m_pFile, nBytes );
	}
	else if ( m_pPackFileHandle )
	{
		m_pPackFileHandle->SetBufferSize( nBytes );
	}
}

int CFileHandle::Read( OUT_BYTECAP(nLength) void* pBuffer, int nLength )
{
	Assert( IsValid() );
	return Read( pBuffer, -1, nLength );
}

int CFileHandle::Read( void* pBuffer, int nDestSize, int nLength )
{
	Assert( IsValid() );

#if defined( SUPPORT_PACKED_STORE )
	if ( m_VPKHandle )
	{
		if ( nDestSize >= 0 )
			nLength = MIN( nLength, nDestSize );
		return m_VPKHandle.Read( pBuffer, nLength );
	}
#endif

	// Is this a regular file or a pack file?  
	if ( m_pFile )
	{
		return m_fs->FS_fread( pBuffer, nDestSize, nLength, m_pFile );
	}

	if ( m_pPackFileHandle )
	{
		// Pack file handle handles clamping all the reads:
		return m_pPackFileHandle->Read( pBuffer, nDestSize, nLength );
	}

	if ( m_type == FT_MEMORY_BINARY || m_type == FT_MEMORY_TEXT )
	{
		return static_cast< CMemoryFileHandle* >( this )->Read( pBuffer, nDestSize, nLength );
	}

	return 0;
}

int CFileHandle::Write( IN_BYTECAP(nLength) const void* pBuffer, int nLength )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );
	if( ThreadInMainThread() )
	{
		tmPlotI32( TELEMETRY_LEVEL0, TMPT_MEMORY, 0, nLength, "FileBytesWrite" );
	}

	Assert( IsValid() );

	if ( !m_pFile )
	{
		m_fs->Warning( FILESYSTEM_WARNING, "FS:  Tried to Write nullptr file pointer inside valid file handle!\n" );
		return 0;
	}

	size_t nBytesWritten = m_fs->FS_fwrite( pBuffer, nLength, m_pFile  );

	m_fs->Trace_FWrite(nBytesWritten,m_pFile);

	return nBytesWritten;
}

int CFileHandle::Seek( int64 nOffset, int nWhence )
{
	Assert( IsValid() );

#if defined( SUPPORT_PACKED_STORE )
	if ( m_VPKHandle )
	{
		return m_VPKHandle.Seek( nOffset, nWhence );
	}
#endif

	if ( m_pFile )
	{
		// dimhotepus: Return seek offset like other paths do.
		return m_fs->FS_fseek( m_pFile, nOffset, nWhence );
	}

	if ( m_pPackFileHandle )
	{
		return m_pPackFileHandle->Seek( nOffset, nWhence );
	}

	if ( m_type == FT_MEMORY_BINARY || m_type == FT_MEMORY_TEXT )
	{
		return static_cast< CMemoryFileHandle* >( this )->Seek( nOffset, nWhence );
	}

	return -1;
}

int CFileHandle::Tell()
{
	Assert( IsValid() );

#if defined( SUPPORT_PACKED_STORE )
	if ( m_VPKHandle )
	{
		return m_VPKHandle.Tell();
	}
#endif

	if ( m_pFile )
	{
		return m_fs->FS_ftell( m_pFile );
	}

	if ( m_pPackFileHandle )
	{
		return m_pPackFileHandle->Tell();
	}

	if ( m_type == FT_MEMORY_BINARY || m_type == FT_MEMORY_TEXT )
	{
		return static_cast< CMemoryFileHandle* >( this )->Tell();
	}

	return -1;
}

unsigned CFileHandle::Size()
{
	Assert( IsValid() );

	unsigned nReturnedSize = std::numeric_limits<unsigned>::max();

#if defined( SUPPORT_PACKED_STORE )
	if ( m_VPKHandle )
	{
		return m_VPKHandle.m_nFileSize;
	}
#endif

	if ( m_pFile  )
	{
		nReturnedSize = m_nLength;
	}
	else if ( m_pPackFileHandle )
	{
		nReturnedSize = m_pPackFileHandle->Size();
	}
	else if ( m_type == FT_MEMORY_BINARY || m_type == FT_MEMORY_TEXT )
	{
		return static_cast< CMemoryFileHandle* >( this )->Size();
	}

	return nReturnedSize;
}

int64 CFileHandle::AbsoluteBaseOffset()
{
	Assert( IsValid() );

	if ( !m_pFile && m_pPackFileHandle )
	{
		return m_pPackFileHandle->AbsoluteBaseOffset();
	}

	return 0LL;
}

bool CFileHandle::EndOfFile()
{
	Assert( IsValid() );

	return ( Tell() >= Size() );
}

//-----------------------------------------------------------------------------

int CMemoryFileHandle::Read( void* pBuffer, int nDestSize, int nLength )
{
	nLength = min( nLength, (int) m_nLength - m_nPosition );
	if ( nLength > 0 )
	{
		Assert( m_nPosition >= 0 );
		memcpy( pBuffer, m_pBacking->m_pData + m_nPosition, nLength );
		m_nPosition += nLength;
		return nLength;
	}
	if ( m_nPosition >= (int) m_nLength )
	{
		return -1;
	}
	return 0;
}

int CMemoryFileHandle::Seek( int64 nOffset64, int nWhence )
{
	if ( nWhence == SEEK_SET )
	{
		m_nPosition = (int) clamp( nOffset64, 0ll, m_nLength );
	}
	else if ( nWhence == SEEK_CUR )
	{
		m_nPosition += (int) clamp( nOffset64, -(int64)m_nPosition, m_nLength - m_nPosition );
	}
	else if ( nWhence == SEEK_END )
	{
		m_nPosition = (int) m_nLength + (int) clamp( nOffset64, -m_nLength, 0LL );
	}
	return m_nPosition;
}

//-----------------------------------------------------------------------------

CBaseFileSystem::CFileCacheObject::CFileCacheObject( CBaseFileSystem* pFS )
:	m_pFS( pFS )
{
}

void CBaseFileSystem::CFileCacheObject::AddFiles( const char **ppFileNames, int nFileNames )
{
	CUtlVector< Info_t* > infos;
	infos.SetCount( nFileNames );
	for ( int i = 0; i < nFileNames; ++i )
	{
		infos[i] = new Info_t;
		Info_t &info = *infos[i];
		char *pFileName = strdup( ppFileNames[i] );
		V_FixSlashes( pFileName );
#ifdef _WIN32
		V_strlower( pFileName );
#endif
		info.pFileName = pFileName;
		info.hIOAsync = nullptr;
		info.pBacking = nullptr;
		info.pOwner = nullptr;
	}

	AUTO_LOCK( m_InfosMutex );
	const intp offset = m_Infos.AddMultipleToTail( nFileNames, infos.Base() );

	m_nPending += nFileNames;
	ProcessNewEntries(offset);
}

void CBaseFileSystem::CFileCacheObject::ProcessNewEntries( intp start )
{
	for ( auto *info : m_Infos )
	{
		if ( !info->pOwner )
		{
			info->pOwner = this;

			// NOTE: currently only caching files with GAME pathID
			FileAsyncRequest_t request;
			request.pszFilename = info->pFileName;
			request.pszPathID = "GAME";
			request.flags = FSASYNC_FLAGS_ALLOCNOFREE;
			request.pfnCallback = &IOCallback;
			request.pContext = &info;
			if ( m_pFS->AsyncRead( request, &info->hIOAsync ) != FSASYNC_OK )
			{
				--m_nPending;
			}
		}
	}
}

void CBaseFileSystem::CFileCacheObject::IOCallback( const FileAsyncRequest_t &request, int nBytesRead, FSAsyncStatus_t err )
{
	Assert( request.pContext );
	Info_t &info = *static_cast<Info_t *>(request.pContext);

	auto *pBacking = new CMemoryFileBacking( info.pOwner->m_pFS );
	pBacking->m_pData = nullptr;
	pBacking->m_pFileName = info.pFileName;
	pBacking->m_nLength = ( err == FSASYNC_OK && nBytesRead == 0 ) ? 0 : -1;

	if ( request.pData )
	{
		if ( err == FSASYNC_OK && nBytesRead > 0 )
		{
			pBacking->m_pData = (const char*) request.pData; // transfer data ownership
			pBacking->m_nLength = nBytesRead;
		}
		else
		{
			info.pOwner->m_pFS->FreeOptimalReadBuffer( request.pData );
		}
	}

	CMemoryFileBacking *pExistingBacking = nullptr;
	if ( !info.pOwner->m_pFS->RegisterMemoryFile( pBacking, &pExistingBacking ) )
	{
		// Someone already registered this file
		info.pFileName = pExistingBacking->m_pFileName;
		pBacking->Release();
		pBacking = pExistingBacking;
	}

	//DevWarning("preload %s  %d\n", request.pszFilename, pBacking->m_nLength);

	info.pBacking = pBacking;
	--info.pOwner->m_nPending;
}

CBaseFileSystem::CFileCacheObject::~CFileCacheObject()
{
	AUTO_LOCK( m_InfosMutex );
	for ( auto *info : m_Infos )
	{
		if ( info->hIOAsync )
		{
			// job is guaranteed to not be running after abort
			m_pFS->AsyncAbort( info->hIOAsync );
			m_pFS->AsyncRelease( info->hIOAsync );
		}

		if ( info->pBacking )
		{
			m_pFS->UnregisterMemoryFile( info->pBacking );
			info->pBacking->Release();
		}
		else
		{
			free( (char*) info->pFileName );
		}
		
		delete info;
	}
	Assert( m_nPending == 0 );
}
