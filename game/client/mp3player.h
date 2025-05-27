// Copyright Valve Corporation, All rights reserved.
//
// Simple MP3 player.

#ifndef SE_GAME_CLIENT_MP3PLAYER_H_
#define SE_GAME_CLIENT_MP3PLAYER_H_

// The MP3 player has a menu button for setting options, opening files, etc.
// It has a tree control to show the high level categories.
// It has a property sheet to switch between the file view and the current
// playlist.
// It has a list view to show the actual files in either view.

#include "vgui_controls/Frame.h"
#include "filesystem.h"
#include "tier1/utlsymbol.h"

// Forward declarations
namespace vgui
{
	class MenuButton;
	class Button;
	class Slider;
	class IScheme;
	class FileOpenDialog;
	class DirectorySelectDialog;
};

class CMP3FileSheet;
class CMP3TreeControl;
class CMP3SongProgress;

// This is the core MP3 file element
struct MP3File_t
{
	enum
	{
		FLAG_UNKNOWN = 0,
		// File came from steam cache/game data, rather than the user's "My Music" folder...
		FLAG_FROMGAME,
		// File came from directory outside of game data directory
		FLAG_FROMFS
	};

	MP3File_t()
	{
		filename = 0;
		playbackfilename = 0;
		flags = FLAG_UNKNOWN;
		dirnum = -1;
		shortname = UTL_INVAL_SYMBOL;
	}

	int					flags;
	FileNameHandle_t	filename;
	FileNameHandle_t	playbackfilename;  // in case we had to make a local copy somewhere else...
	CUtlSymbol			shortname;
	intp				dirnum;
};

// Directory has a name, 0 or more subdirectories and 0 or more mp3 files in it.
struct MP3Dir_t
{
	MP3Dir_t() :
		m_DirName( UTL_INVAL_SYMBOL ),
		m_FullDirPath( UTL_INVAL_SYMBOL )
	{
	}

	~MP3Dir_t()
	{
		DeleteSubdirectories();
	}

	void DeleteSubdirectories()
	{
		intp c = m_Subdirectories.Count();
		for ( intp i = c - 1; i >= 0 ; --i )
		{
			delete m_Subdirectories[ i ];
		}
		m_Subdirectories.RemoveAll();
	}

	MP3Dir_t( const MP3Dir_t& src )
	{
		m_DirName = src.m_DirName;
		m_FullDirPath = src.m_FullDirPath;

		intp c = src.m_Subdirectories.Count();
		for ( intp i = 0; i < c; ++i )
		{
			m_Subdirectories.AddToTail( new MP3Dir_t( *src.m_Subdirectories[ i ] ) );
		}

		c = src.m_FilesInDirectory.Count();
		for ( intp i = 0; i < c; ++i )
		{
			m_FilesInDirectory.AddToTail( src.m_FilesInDirectory[ i ] );
		}
	}

	MP3Dir_t &operator =( const MP3Dir_t& src )
	{
		if ( this == &src )
		{
			return *this;
		}

		m_DirName = src.m_DirName;
		m_FullDirPath = src.m_FullDirPath;

		DeleteSubdirectories();

		m_FilesInDirectory.RemoveAll();

		intp c = src.m_Subdirectories.Count();
		for ( intp i = 0; i < c; ++i )
		{
			// make a copy
			m_Subdirectories.AddToTail( new MP3Dir_t( *src.m_Subdirectories[ i ] ) );
		}

		c = src.m_FilesInDirectory.Count();
		for ( intp i = 0; i < c; ++i )
		{
			m_FilesInDirectory.AddToTail( src.m_FilesInDirectory[ i ] );
		}

		return *this;
	}

	void AddSubDirectory( MP3Dir_t *sub )
	{
		m_Subdirectories.AddToTail( sub );
	}

	CUtlSymbol					m_DirName;	// "artist"
	CUtlSymbol					m_FullDirPath;	// "artist/album
	CUtlVector< MP3Dir_t * >	m_Subdirectories;
	CUtlVector< intp >			m_FilesInDirectory;
};

// Sound directory is a root directory which is recursed looking for .mp3.
//
// We assume that the folders under the sound directory are configured as
// artist/album/filename.mp3...
struct SoundDirectory_t
{
	explicit SoundDirectory_t( intp index ) :
		m_nIndex( index ),
		m_Root( UTL_INVAL_SYMBOL ),
		m_pTree( 0 ),
		m_bGameSound( false )
	{
	}

	~SoundDirectory_t()
	{
		delete m_pTree;
	}

	void SetTree( MP3Dir_t *tree )
	{
		if ( m_pTree )
		{
			delete m_pTree;
		}
		m_pTree = tree;
	}

	intp		GetIndex() const { return m_nIndex; }

	intp		m_nIndex;
	CUtlSymbol	m_Root;
	MP3Dir_t	*m_pTree;
	bool		m_bGameSound;
};

// VGui based .mp3 player
class CMP3Player : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CMP3Player, vgui::Frame );

public:
	CMP3Player( vgui::VPANEL parent, char const *panelName );
	~CMP3Player();

	virtual void			SetVisible( bool );

	// Lookup data
	MP3File_t				*GetSongInfo( intp songIndex );

	void					AddToPlayList( intp songIndex, bool playNow );
	void					RemoveFromPlayList( intp songIndex );

	void					ClearPlayList();
	void					OnLoadPlayList();
	void					OnSavePlayList();
	void					OnSavePlayListAs();

	void					SetPlayListSong( intp listIndex );

	enum SongListSource_t
	{
		SONG_FROM_UNKNOWN = 0,
		SONG_FROM_TREE,
		SONG_FROM_FILELIST,
		SONG_FROM_PLAYLIST
	};

	void					SelectedSongs( SongListSource_t from, CUtlVector< int >& songIndexList );

	void					EnableAutoAdvance( bool state );

protected:
	virtual void			OnCommand( char const *cmd );
	virtual void			ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void			OnTick();

	MESSAGE_FUNC( OnTreeViewItemSelected, "TreeViewItemSelected" );
	MESSAGE_FUNC( OnSliderMoved, "SliderMoved" );

	void					PopulateTree();
	void					PopulateLists();
	void					RecursiveAddToTree( MP3Dir_t *current, intp parentIndex );
	void					DeleteSoundDirectories();
	// Leave root objects, clear all subdirs
	void					WipeSoundDirectories();

	// Remove the _mp3/a45ef65a.mp3 style temp sounds
	void					RemoveTempSounds();

	void					SplitFile( CUtlVector< CUtlSymbol >& splitList, char const *relative );
	intp					AddSplitFileToDirectoryTree_R( intp songIndex, MP3Dir_t *parent, CUtlVector< CUtlSymbol >& splitList, int level );

	MP3Dir_t				*FindOrAddSubdirectory( MP3Dir_t *parent, char const *dirname );

	intp					AddFileToDirectoryTree( SoundDirectory_t *dir, char const *relative );
	void					RecursiveFindMP3Files( SoundDirectory_t *root, char const *current, char const *pathID );

	intp					AddSong( char const *relative, intp dirnum );
	void					RemoveFSSongs(); // Remove all non-built-in .mp3s
	intp					FindSong( char const *relative );

	void					PlaySong( intp songIndex, float skipTime = 0.0f );
	void					GetLocalCopyOfSong( const MP3File_t &mp3, char *outsong, size_t outlen );
	float					GetMP3Duration( char const *songname );
	void					OnNextTrack();
	void					OnPrevTrack();

	void					OnPlay();
	void					OnStop();
	void					OnChangeVolume( float newVol );

	void					AddGameSounds( bool recurse );
	SoundDirectory_t		*AddSoundDirectory( char const *fullpath, bool recurse );
	intp					FindSoundDirectory( char const *fullpath );

	bool					RestoreDb( char const *filename );
	void					SaveDb( char const *filename );

	void					SaveDbFile( int level, CUtlBuffer& buf, MP3File_t *file, intp filenumber );
	void					FlattenDirectoryFileList_R( MP3Dir_t *dir, CUtlVector< intp >& list );
	void					SaveDbDirectory( int level, CUtlBuffer& buf, SoundDirectory_t *sd );

	void					RestoreSongs( KeyValues *songs );
	void					RestoreDirectories( KeyValues *dirs );
	void					RestoreDirectory( KeyValues *dir, SoundDirectory_t *sd );	

	void					LoadPlayList( char const *filename );
	void					SavePlayList( char const *filename );

	void					SetMostRecentPlayList( char const *filename );

	void					SaveSettings();
	void					LoadSettings();

	// Refresh all directories, built-in sounds
	void					OnRefresh();
	void					OnSave();

	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );
	void					ShowFileOpenDialog( bool saving );
	MESSAGE_FUNC_PARAMS( OnDirectorySelected, "DirectorySelected", params );
	void					ShowDirectorySelectDialog();

	void					GoToNextSong( int skip );

// Data
private:

// UI elements
	vgui::MenuButton		*m_pOptions;
	CMP3TreeControl			*m_pTree;
	CMP3FileSheet			*m_pFileSheet;
	vgui::Label				*m_pCurrentSong;
	vgui::Label				*m_pDuration;
	CMP3SongProgress		*m_pSongProgress;
	vgui::Button			*m_pPlay;
	vgui::Button			*m_pStop;
	vgui::Button			*m_pNext, *m_pPrev; // moving between tracks
	vgui::CheckButton		*m_pMute;
	vgui::CheckButton		*m_pShuffle;
	vgui::Slider			*m_pVolume;

// Raw list of all known files
	CUtlVector< MP3File_t >	m_Files;
	intp					m_nFilesAdded;
// Indices into m_Files for currently playing songs
	CUtlVector< intp >		m_PlayList;
// Where in the list we are...
	intp					m_nCurrentPlaylistSong;
	CUtlSymbol				m_PlayListFileName;

// Flag for one-time init
	bool					m_bFirstTime;
	intp					m_nCurrentSong;
	FileNameHandle_t		m_LastSong;
	float					m_flCurrentVolume;
	bool					m_bMuted;

// Currently playing a song?
	bool					m_bPlaying;
	int						m_nSongGuid;
// Song start  time
	float					m_SongStart;
// Estimated song diration
	float					m_flSongDuration;
// For the UI
	int						m_nSongMinutes;
	int						m_nSongSeconds;

// List of all added directories
	CUtlVector< SoundDirectory_t * >	m_SoundDirectories;

// Selection set
	CUtlVector< intp >		m_SelectedSongs;
	SongListSource_t		m_SelectionFrom;

// Is database dirty?
	bool					m_bDirty;
// Are settings dirty?
	bool					m_bSettingsDirty;

// File dialog
	vgui::DHANDLE< vgui::FileOpenDialog >	m_hSaveLoadPlaylist;
// Type of dialog
	bool					m_bSavingFile;

// Directory selection dialog
	vgui::DHANDLE< vgui::DirectorySelectDialog > m_hDirectorySelect;

	bool					m_bEnableAutoAdvance;
};

#endif  // !SE_GAME_CLIENT_MP3PLAYER_H_