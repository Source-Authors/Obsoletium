//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "bugreporter/bugreporter.h"

#define PROTECTED_THINGS_DISABLE
#undef PROTECT_FILEIO_FUNCTIONS
#undef fopen
#include "winlite.h"
#undef GetUserName

#include "tier0/basetypes.h"

#include "tier1/utlvector.h"
#include "tier1/utlsymbol.h"
#include "tier1/utldict.h"
#include "tier1/utlbuffer.h"
#include "steamcommon.h"

#include "filesystem_tools.h"
#include "tier1/KeyValues.h"
#include "tier1/netadr.h"
#include "steam/steamclientpublic.h"

bool UploadBugReport(
	const netadr_t& cserIP,
	const CSteamID& userid,
	int build,
	char const *title,
	char const *body,
	char const *exename,
	char const *gamedir,
	char const *mapname,
	char const *reporttype,
	char const *email,
	char const *accountname,
	int ram,
	int cpu,
	char const *processor,
	unsigned int high, 
	unsigned int low, 
	unsigned int vendor, 
	unsigned int device,
	char const *osversion,
	char const *attachedfile,
	unsigned int attachedfilesize
);

IBaseFileSystem *g_pFileSystem = nullptr;

class CBug
{
public:
	CBug()
	{
		Clear();
	}

	void Clear()
	{
		Q_memset( title, 0, sizeof( title ) );
		Q_memset( desc, 0, sizeof( desc ) );
		Q_memset( submitter, 0, sizeof( submitter ) );
		Q_memset( owner, 0, sizeof( owner ) );
		Q_memset( severity, 0, sizeof( severity ) );
		Q_memset( priority, 0, sizeof( priority ) );
		Q_memset( area, 0, sizeof( area ) );
		Q_memset( mapnumber, 0, sizeof( mapnumber) );
		Q_memset( reporttype, 0, sizeof( reporttype ) );
		Q_memset( level, 0, sizeof( level ) );
		Q_memset( build, 0, sizeof( build ) );
		Q_memset( position, 0, sizeof( position ) );
		Q_memset( orientation, 0, sizeof( orientation ) );
		Q_memset( screenshot_unc, 0, sizeof( screenshot_unc ) );
		Q_memset( savegame_unc, 0, sizeof( savegame_unc ) );
		Q_memset( bsp_unc, 0, sizeof( bsp_unc ) );
		Q_memset( vmf_unc, 0, sizeof( vmf_unc ) );
		Q_memset( driverinfo, 0, sizeof( driverinfo ) );
		Q_memset( misc, 0, sizeof( misc ) );
		Q_memset( zip, 0, sizeof( zip ) );

		Q_memset( exename, 0, sizeof( exename ) );
		Q_memset( gamedir, 0, sizeof( gamedir ) );
		ram = 0;
		cpu = 0;
		Q_memset( processor, 0, sizeof( processor ) );
		dxversionhigh = 0;
		dxversionlow = 0;
		dxvendor = 0;
		dxdevice = 0;
		Q_memset( osversion, 0, sizeof( osversion ) );

		includedfiles.Purge();
	}

	char			title[ 256 ];
	char			desc[ 8192 ];
	char			owner[ 256 ];
	char			submitter[ 256 ];
	char			severity[ 256 ];
	char			priority[ 256 ];
	char			area[ 256 ];
	char			mapnumber[ 256 ];
	char			reporttype[ 256 ];
	char			level[ 256 ];
	char			build[ 256 ];
	char			position[ 256 ];
	char			orientation[ 256 ];
	char			screenshot_unc[ 256 ];
	char			savegame_unc[ 256 ];
	char			bsp_unc[ 256 ];
	char			vmf_unc[ 256 ];
	char			driverinfo[ 2048 ];
	char			misc[ 1024 ];

	char			exename[ 256 ];
	char			gamedir[ 256 ];
	unsigned int	ram;
	unsigned int	cpu;
	char			processor[ 256 ];
	unsigned int	dxversionhigh;
	unsigned int	dxversionlow;
	unsigned int	dxvendor;
	unsigned int	dxdevice;
	char			osversion[ 256 ];

	char			zip[ 256 ];

	struct incfile
	{
		char name[ 256 ];
	};

	CUtlVector< incfile >	includedfiles;
};

class CBugReporter : public IBugReporter
{
public:

	CBugReporter();
	~CBugReporter() override;

	// Initialize and login with default username/password for this computer (from resource/bugreporter.res)
	bool		Init( CreateInterfaceFn engineFactory ) override;
	void		Shutdown() override;

	bool		IsPublicUI() override { return true; }

	char const	*GetUserName() override;
	char const	*GetUserName_Display() override;

	intp			GetNameCount() override;
	char const	*GetName( intp index ) override;

	intp			GetDisplayNameCount() override;
	char const  *GetDisplayName( intp index ) override;

	char const	*GetDisplayNameForUserName( char const *username ) override;
	char const  *GetUserNameForDisplayName( char const *display ) override;

	intp			GetSeverityCount() override;
	char const	*GetSeverity( intp index ) override;

	intp			GetPriorityCount() override;
	char const	*GetPriority( intp index ) override;

	intp			GetAreaCount() override;
	char const	*GetArea( intp index ) override;

	intp			GetAreaMapCount() override;
	char const	*GetAreaMap( intp ) override;

	intp			GetMapNumberCount() override;
	char const	*GetMapNumber( intp ) override;

	intp			GetReportTypeCount() override;
	char const	*GetReportType( intp ) override;

	char const *GetRepositoryURL() override { return nullptr; }
	char const *GetSubmissionURL() override { return nullptr; }

	intp		GetLevelCount(intp) override { return 0; }
	char const	*GetLevel(intp, intp ) override { return ""; }

// Submission API
	void		StartNewBugReport() override;
	void		CancelNewBugReport() override;
	bool		CommitBugReport( int& bugSubmissionId ) override;

	void		SetTitle( char const *title ) override;
	void		SetDescription( char const *description ) override;

	// NULL for current user
	void		SetSubmitter( char const *username = nullptr ) override;
	void		SetOwner( char const *username ) override;
	void		SetSeverity( char const *severity ) override;
	void		SetPriority( char const *priority ) override;
	void		SetArea( char const *area ) override;
	void		SetMapNumber ( char const *mapnumber ) override;
	void		SetReportType( char const *reporttype ) override;

	void		SetLevel( char const *levelnamne ) override;
	void		SetPosition( char const *position ) override;
	void		SetOrientation( char const *pitch_yaw_roll ) override;
	void		SetBuildNumber( char const *build_num ) override;

	void		SetScreenShot( char const *screenshot_unc_address ) override;
	void		SetSaveGame( char const *savegame_unc_address ) override;

	void		SetBSPName( char const *bsp_unc_address ) override;
	void		SetVMFName( char const *vmf_unc_address ) override;

	void		AddIncludedFile( char const *filename ) override;
	void		ResetIncludedFiles() override;

	void		SetDriverInfo( char const *info ) override;

	void		SetZipAttachmentName( char const *zipfilename ) override;

	void		SetMiscInfo( char const *info ) override;

	void		SetCSERAddress( const struct netadr_s& adr ) override;
	void		SetExeName( char const *exename ) override;
	void		SetGameDirectory( char const *gamedir ) override;
	void		SetRAM( int ram ) override;
	void		SetCPU( int cpu ) override;
	void		SetProcessor( char const *processor ) override;
	void		SetDXVersion( unsigned int high, unsigned int low, unsigned int vendor, unsigned int device ) override;
	void		SetOSVersion( char const *osversion ) override;
	void		SetSteamUserID( void *steamid, int idsize ) override;

private:

	void				SubstituteBugId( int bugid, char *out, int outlen, CUtlBuffer& src );

	CUtlSymbolTable				m_BugStrings;

	CUtlVector< CUtlSymbol >	m_Severity;
	CUtlVector< CUtlSymbol >	m_Area;
	CUtlVector< CUtlSymbol >	m_MapNumber;
	CUtlVector< CUtlSymbol >	m_ReportType;

	CUtlSymbol					m_UserName;

	CBug						*m_pBug;
	netadr_t					m_cserIP;
	CSteamID			m_SteamID;
};

CBugReporter::CBugReporter()
{
	Q_memset( &m_cserIP, 0, sizeof( m_cserIP ) );
	m_pBug = nullptr;

	m_Severity.AddToTail( m_BugStrings.AddString( "Zero" ) );
	m_Severity.AddToTail( m_BugStrings.AddString( "Low" ) );
	m_Severity.AddToTail( m_BugStrings.AddString( "Medium" ) );
	m_Severity.AddToTail( m_BugStrings.AddString( "High" ) );
	m_Severity.AddToTail( m_BugStrings.AddString( "Showstopper" ) );

	
	m_ReportType.AddToTail( m_BugStrings.AddString( "<<Choose Item>>" ) );
	m_ReportType.AddToTail( m_BugStrings.AddString( "Video / Display Problems" ) );
	m_ReportType.AddToTail( m_BugStrings.AddString( "Network / Connectivity Problems" ) );
	m_ReportType.AddToTail( m_BugStrings.AddString( "Download / Installation Problems" ) );
	m_ReportType.AddToTail( m_BugStrings.AddString( "In-game Crash" ) );
	m_ReportType.AddToTail( m_BugStrings.AddString( "Game play / Strategy Problems" ) );
	m_ReportType.AddToTail( m_BugStrings.AddString( "Steam Problems" ) );
	m_ReportType.AddToTail( m_BugStrings.AddString( "Unlisted Bug" ) );
	m_ReportType.AddToTail( m_BugStrings.AddString( "Feature Request / Suggestion" ) );
}

CBugReporter::~CBugReporter()
{
	m_BugStrings.RemoveAll();
	m_Severity.Purge();
	m_Area.Purge();
	m_MapNumber.Purge();
	m_ReportType.Purge();

	delete m_pBug;
}
//-----------------------------------------------------------------------------
// Purpose: Initialize and login with default username/password for this computer (from resource/bugreporter.res)
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBugReporter::Init( CreateInterfaceFn engineFactory )
{
	g_pFileSystem = (IFileSystem *)engineFactory( FILESYSTEM_INTERFACE_VERSION, NULL );
	if ( !g_pFileSystem )
	{
		AssertMsg( 0, "Failed to create/get IFileSystem" );
		return false;
	}
	return true;
}

void CBugReporter::Shutdown()
{
	// dimhotepus: Cleanup filesystem pointer.
	g_pFileSystem = nullptr;
}

char const *CBugReporter::GetUserName()
{
	return m_UserName.String();
}

char const *CBugReporter::GetUserName_Display()
{
	return GetUserName();
}

intp CBugReporter::GetNameCount()
{
	return 1;
}

char const *CBugReporter::GetName( intp index )
{
	if ( index < 0 || index >= 1 )
		return "<<Invalid>>";

	return GetUserName();
}

intp CBugReporter::GetDisplayNameCount()
{
	return 1;
}

char const *CBugReporter::GetDisplayName( intp index ) //-V524
{
	if ( index < 0 || index >= 1 )
		return "<<Invalid>>";

	return GetUserName(); 
}

char const *CBugReporter::GetDisplayNameForUserName( char const *username )
{
	return username;
}

char const *CBugReporter::GetUserNameForDisplayName( char const *display )
{
	return display;
}

intp CBugReporter::GetSeverityCount()
{
	return m_Severity.Count();
}

char const *CBugReporter::GetSeverity( intp index )
{
	if ( index < 0 || index >= m_Severity.Count() )
		return "<<Invalid>>";

	return m_BugStrings.String( m_Severity[ index ] );
}

intp CBugReporter::GetPriorityCount()
{
	return 0;
}

char const *CBugReporter::GetPriority( intp )
{
	return "<<Invalid>>";
}

intp CBugReporter::GetAreaCount()
{
	return 0;
}

char const *CBugReporter::GetArea( intp )
{
	return "<<Invalid>>";
}

intp CBugReporter::GetAreaMapCount()
{
	return 0;
}

char const *CBugReporter::GetAreaMap( intp )
{
	return "<<Invalid>>";
}

intp CBugReporter::GetMapNumberCount()
{
	return 0;
}

char const *CBugReporter::GetMapNumber( intp )
{
	return "<<Invalid>>";
}

intp CBugReporter::GetReportTypeCount()
{
	return m_ReportType.Count();
}

char const *CBugReporter::GetReportType( intp index )
{
	if ( index < 0 || index >= m_ReportType.Count() )
		return "<<Invalid>>";

	return m_BugStrings.String( m_ReportType[ index ] );
}

void CBugReporter::StartNewBugReport()
{
	if ( !m_pBug )
	{
		m_pBug = new CBug();
	}
	else
	{
		m_pBug->Clear();
	}
}

void CBugReporter::CancelNewBugReport()
{
	if ( !m_pBug )
		return;

	m_pBug->Clear();
}

void CBugReporter::SubstituteBugId( int bugid, char *out, int outlen, CUtlBuffer& src )
{
	out[ 0 ] = 0;

	char *dest = out;

	src.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );

	char const *replace = "\\BugId\\";
	intp replace_len = Q_strlen( replace );

	for ( intp pos = 0; pos <= src.TellPut() && ( ( dest - out ) < outlen ); )
	{
		char const *str = ( char const * )src.PeekGet( pos );
		if ( !Q_strnicmp( str, replace, replace_len ) )
		{
			*dest++ = '\\';

			char num[ 32 ];
			V_to_chars( num, bugid );
			char *pnum = num;
			while ( *pnum )
			{
				*dest++ = *pnum++;
			}

			*dest++ = '\\';
			pos += replace_len;
			continue;
		}

		*dest++ = *str;
		++pos;
	}
	*dest = 0;
}

bool CBugReporter::CommitBugReport( int& bugSubmissionId )
{
	bugSubmissionId = -1;

	if ( !m_pBug )
		return false;

	CUtlBuffer buf( (intp)0, 0, CUtlBuffer::TEXT_BUFFER );

	buf.Printf( "%s\n\n", m_pBug->desc );

	buf.Printf( "level:  %s\nbuild:  %s\nposition:  setpos %s; setang %s\n",
		m_pBug->level,
		m_pBug->build,
		m_pBug->position,
		m_pBug->orientation );

	if ( m_pBug->screenshot_unc[ 0 ] )
	{
		buf.Printf( "screenshot:  %s\n", m_pBug->screenshot_unc );
	}
	if ( m_pBug->savegame_unc[ 0 ] )
	{
		buf.Printf( "savegame:  %s\n", m_pBug->savegame_unc );
	}
	/*
	if ( m_pBug->bsp_unc[ 0 ] )
	{
		buf.Printf( "bsp:  %s\n", m_pBug->bsp_unc );
	}
	if ( m_pBug->vmf_unc[ 0 ] )
	{
		buf.Printf( "vmf:  %s\n", m_pBug->vmf_unc );
	}
	if ( m_pBug->includedfiles.Count() > 0 )
	{
		int c = m_pBug->includedfiles.Count();
		for ( int i = 0 ; i < c; ++i )
		{
			buf.Printf( "include:  %s\n", m_pBug->includedfiles[ i ].name );
		}
	}
	*/
	if ( m_pBug->driverinfo[ 0 ] )
	{
		buf.Printf( "%s\n", m_pBug->driverinfo );
	}
	if ( m_pBug->misc[ 0 ] )
	{
		buf.Printf( "%s\n", m_pBug->misc );
	}

	buf.PutChar( 0 );

	// Store desc

	int bugId = 0;

	bugSubmissionId = (int)bugId;

	int attachedfilesize = ( m_pBug->zip[ 0 ] == 0 ) ? 0 : g_pFileSystem->Size( m_pBug->zip );

	if ( !UploadBugReport( 
			m_cserIP,
			m_SteamID,
			atoi( m_pBug->build ),
			m_pBug->title,
			buf.Base<const char>(),
			m_pBug->exename,
			m_pBug->gamedir,
			m_pBug->level,
			m_pBug->reporttype,
			m_pBug->owner,
			m_pBug->submitter,
			m_pBug->ram,
			m_pBug->cpu,
			m_pBug->processor,
			m_pBug->dxversionhigh, 
			m_pBug->dxversionlow, 
			m_pBug->dxvendor, 
			m_pBug->dxdevice,
			m_pBug->osversion,
			m_pBug->zip,
			attachedfilesize
		) )
	{
		Warning( "Unable to upload bug...\n" );
		m_pBug->Clear();

		// dimhotepus: Signal bug upload failure.
		return false;
	}

	m_pBug->Clear();
	return true;
}

void CBugReporter::SetTitle( char const *title )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->title, title, sizeof( m_pBug->title ) );
}

void CBugReporter::SetDescription( char const *description )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->desc, description, sizeof( m_pBug->desc ) );
}

void CBugReporter::SetSubmitter( char const *username /*= 0*/ )
{
	m_UserName = username;
	/*
	if ( !username )
	{
		username = GetUserName();
	}
	*/

	Assert( m_pBug );
	Q_strncpy( m_pBug->submitter, username ? username : "", sizeof( m_pBug->submitter ) );
}

void CBugReporter::SetOwner( char const *username )
{
	Q_strncpy( m_pBug->owner, username, sizeof( m_pBug->owner ) );
}

void CBugReporter::SetSeverity( char const * )
{
}

void CBugReporter::SetPriority( char const * )
{
}

void CBugReporter::SetArea( char const * )
{
}

void CBugReporter::SetMapNumber( char const * )
{
}

void CBugReporter::SetReportType( char const *reporttype )
{
	Q_strncpy( m_pBug->reporttype, reporttype, sizeof( m_pBug->reporttype ) );
}

void CBugReporter::SetLevel( char const *levelnamne )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->level, levelnamne, sizeof( m_pBug->level ) );
}

void CBugReporter::SetDriverInfo( char const *info )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->driverinfo, info, sizeof( m_pBug->driverinfo ) );
}

void CBugReporter::SetZipAttachmentName( char const *zipfilename )
{
	Assert( m_pBug );

	Q_strncpy( m_pBug->zip, zipfilename, sizeof( m_pBug->zip ) );
}

void CBugReporter::SetMiscInfo( char const *info )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->misc, info, sizeof( m_pBug->misc ) );
}

void CBugReporter::SetPosition( char const *position )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->position, position, sizeof( m_pBug->position ) );
}

void CBugReporter::SetOrientation( char const *pitch_yaw_roll )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->orientation, pitch_yaw_roll, sizeof( m_pBug->orientation ) );
}

void CBugReporter::SetBuildNumber( char const *build_num )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->build, build_num, sizeof( m_pBug->build ) );
}

void CBugReporter::SetScreenShot( char const *screenshot_unc_address )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->screenshot_unc, screenshot_unc_address, sizeof( m_pBug->screenshot_unc ) );
}

void CBugReporter::SetSaveGame( char const *savegame_unc_address )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->savegame_unc, savegame_unc_address, sizeof( m_pBug->savegame_unc ) );
}

void CBugReporter::SetBSPName( char const * )
{
}

void CBugReporter::SetVMFName( char const * )
{
}

void CBugReporter::AddIncludedFile( char const * )
{
}

void CBugReporter::ResetIncludedFiles()
{
}

void CBugReporter::SetExeName( char const *exename )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->exename, exename, sizeof( m_pBug->exename ) );
}

void CBugReporter::SetGameDirectory( char const *pchGamedir )
{
	Assert( m_pBug );

	V_FileBase( pchGamedir, m_pBug->gamedir );
}

void CBugReporter::SetRAM( int ram )
{
	Assert( m_pBug );
	m_pBug->ram = ram;
}

void CBugReporter::SetCPU( int cpu )
{
	Assert( m_pBug );
	m_pBug->cpu = cpu;
}

void CBugReporter::SetProcessor( char const *processor )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->processor, processor, sizeof( m_pBug->processor ) );
}

void CBugReporter::SetDXVersion( unsigned int high, unsigned int low, unsigned int vendor, unsigned int device )
{
	Assert( m_pBug );
	m_pBug->dxversionhigh = high;
	m_pBug->dxversionlow = low;
	m_pBug->dxvendor = vendor;
	m_pBug->dxdevice = device;
}

void CBugReporter::SetOSVersion( char const *osversion )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->osversion, osversion, sizeof( m_pBug->osversion ) );
}

void CBugReporter::SetCSERAddress( const struct netadr_s& adr )
{
	m_cserIP = adr;
}

void CBugReporter::SetSteamUserID( void *steamid, int idsize )
{
	Assert( idsize == sizeof( uint64 ) );
	m_SteamID.SetFromUint64( *((uint64*)steamid) );
}

EXPOSE_SINGLE_INTERFACE( CBugReporter, IBugReporter, INTERFACEVERSION_BUGREPORTER );
