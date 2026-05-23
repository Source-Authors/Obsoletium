//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "tier0/platform.h"
#include "mpaheaderinfo/stdafx.h"
#include "mpaheaderinfo/MPAFile.h"
#include "filesystem.h"
#include "soundchars.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlrbtree.h"

#include "memdbgon.h"

extern IFileSystem* g_pFullFileSystem;

namespace
{

[[nodiscard]] FileSystemSeek_t MapMPAFileSeekType(MPAFileSeekType seek_type)
{
	switch (seek_type)
	{
		case MPAFileSeekType::kCurrent:
			return FileSystemSeek_t::FILESYSTEM_SEEK_CURRENT;
		case MPAFileSeekType::kEnd:
			return FileSystemSeek_t::FILESYSTEM_SEEK_TAIL;
		case MPAFileSeekType::kSet:
			return FileSystemSeek_t::FILESYSTEM_SEEK_HEAD;
		default:
			return FileSystemSeek_t::FILESYSTEM_SEEK_CURRENT;
	}
}
	
class SourceMPAFile : public IMPAFile
{
public:
	SourceMPAFile(const char* path,	const char* mode, IFileSystem* file_system)
		: m_file{file_system->Open(path, mode)},
		m_file_system{file_system},
		m_size(file_system->Size(m_file))
	{
	}
	~SourceMPAFile()
	{
		if (m_file)
		{
			m_file_system->Close(m_file);
		}
	}

	[[nodiscard]] size_t Read(void* buffer, size_t size) override
	{
		return m_file_system->Read(buffer, size, m_file);
	}

	[[nodiscard]] int Seek(long offset, MPAFileSeekType seek_type) override
	{
		m_file_system->Seek(m_file, offset, MapMPAFileSeekType(seek_type));
		return 0;
	}
	[[nodiscard]] long Tell() override
	{
		return m_file_system->Tell(m_file);
	}
	
	[[nodiscard]] int Error() override
	{
		return m_file_system->IsOk(m_file) ? 0 : 1;
	}

private:
	FileHandle_t m_file;
	IFileSystem *m_file_system;
	const unsigned m_size;
};

struct SourceMPAFileSystem : public IMPAFileSystem
{
	[[nodiscard]] std::unique_ptr<IMPAFile> Open(const char* path,
		const char* mode) override
	{
		return std::make_unique<SourceMPAFile>(path, mode, g_pFullFileSystem);
	}
};

struct MP3Duration_t
{
    FileNameHandle_t h;
    float           duration;

    static bool LessFunc( const MP3Duration_t& lhs, const MP3Duration_t& rhs )
    {
        return lhs.h < rhs.h;
    }
};

CUtlRBTree< MP3Duration_t, int >    g_MP3Durations( 0, 0, MP3Duration_t::LessFunc );

}

float GetMP3Duration_Helper( char const *filename )
{
	// See if it's in the RB tree already...
	char fn[ MAX_PATH ];
	V_sprintf_safe( fn, "sound/%s", PSkipSoundChars( filename ) );
	V_FixSlashes(fn);

	MP3Duration_t search = {};
	search.h = g_pFullFileSystem->FindOrAddFileName( fn );
	
	auto idx = g_MP3Durations.Find( search );
	if ( idx != g_MP3Durations.InvalidIndex() )
	{
		return g_MP3Durations[ idx ].duration;
	}

	char fullPath[MAX_PATH];
	fullPath[0] = '\0';
	g_pFullFileSystem->RelativePathToFullPath_safe( fn, "MOD", fullPath );
	
	float duration = 60.0f;
	SourceMPAFileSystem source_file_system;

	try
	{
		CMPAFile MPAFile( !Q_isempty( fullPath ) ? fullPath : fn, &source_file_system );
		duration = (float)MPAFile.GetLengthSec();
	}
	catch ( std::exception &e )
	{
		// dimhotepus: Dump exception info.
		Warning( "Unable to get %s file length: %s.", filename, e.what() );
	}

	search.duration = duration;
	g_MP3Durations.Insert( search );

	return duration;
}
