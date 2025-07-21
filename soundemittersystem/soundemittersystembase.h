//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SOUNDEMITTERSYSTEMBASE_H
#define SOUNDEMITTERSYSTEMBASE_H
#ifdef _WIN32
#pragma once
#endif

#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "soundflags.h"
#include "interval.h"
#include "tier1/KeyValues.h"
#include "tier1/UtlSortVector.h"
#include "tier1/utlstring.h"
#include "tier1/utlhashtable.h"

soundlevel_t TextToSoundLevel( const char *key );

struct CSoundEntry
{
	CUtlConstString				m_Name;
	CSoundParametersInternal	m_SoundParams;
	uint16						m_nScriptFileIndex;
	bool						m_bRemoved : 1;
	bool						m_bIsOverride : 1;

	bool						IsOverride() const
	{
		return m_bIsOverride;
	}
};

struct CSoundEntryHashFunctor : CaselessStringHashFunctor
{
	using CaselessStringHashFunctor::operator();
	unsigned int operator()( CSoundEntry *e ) const
	{
		return CaselessStringHashFunctor::operator()( e->m_Name.Get() );
	}
};

struct CSoundEntryEqualFunctor : CaselessStringEqualFunctor
{
	using CaselessStringEqualFunctor::operator();
	bool operator()( CSoundEntry *lhs, CSoundEntry *rhs ) const
	{
		return CaselessStringEqualFunctor::operator()( lhs->m_Name.Get(), rhs->m_Name.Get() );
	}
	bool operator()( CSoundEntry *lhs, const char *rhs ) const
	{
		return CaselessStringEqualFunctor::operator()( lhs->m_Name.Get(), rhs );
	}
};



//-----------------------------------------------------------------------------
// Purpose: Base class for sound emitter system handling (can be used by tools)
//-----------------------------------------------------------------------------
class CSoundEmitterSystemBase : public ISoundEmitterSystemBase
{
public:
	CSoundEmitterSystemBase();
	virtual ~CSoundEmitterSystemBase() { }

	// Methods of IAppSystem
	bool Connect( CreateInterfaceFn factory ) override;
	void Disconnect() override;
	void *QueryInterface( const char *pInterfaceName ) override;
	InitReturnVal_t Init() override;
	void Shutdown() override;

public:

	bool ModInit() override;
	void ModShutdown() override;

	UtlHashHandle_t	GetSoundIndex( const char *pName ) const override;
	bool IsValidIndex( UtlHashHandle_t index ) override;
	intp GetSoundCount( void ) override;

	const char *GetSoundName( UtlHashHandle_t index ) override;
	bool GetParametersForSound( const char *soundname, CSoundParameters& params, gender_t gender, bool isbeingemitted = false ) override;
	const char *GetWaveName( CUtlSymbol& sym ) override;

	CUtlSymbol AddWaveName( const char *name ) override;

	soundlevel_t LookupSoundLevel( const char *soundname ) override;
	const char *GetWavFileForSound( const char *soundname, char const *actormodel ) override;
	const char *GetWavFileForSound( const char *soundname, gender_t gender ) override;

	int		CheckForMissingWavFiles( bool verbose ) override;
	const char *GetSourceFileForSound( intp index ) const override;
	// Iteration methods
	UtlHashHandle_t		First() const override;
	UtlHashHandle_t		Next( UtlHashHandle_t i ) const override;
	UtlHashHandle_t		InvalidIndex() const override;

	CSoundParametersInternal *InternalGetParametersForSound( UtlHashHandle_t index ) override;


	// The host application is responsible for dealing with dirty sound scripts, etc.
	bool		AddSound( const char *soundname, const char *scriptfile, const CSoundParametersInternal& params ) override;
	void		RemoveSound( const char *soundname ) override;

	void		MoveSound( const char *soundname, const char *newscript ) override;

	void		RenameSound( const char *soundname, const char *newname ) override;
	void		UpdateSoundParameters( const char *soundname, const CSoundParametersInternal& params ) override;

	int			GetNumSoundScripts() const override;
	char const	*GetSoundScriptName( intp index ) const override;
	bool		IsSoundScriptDirty( intp index ) const override;
	int			FindSoundScript( const char *name ) const override;

	void		SaveChangesToSoundScript( intp scriptindex ) override;

	void		ExpandSoundNameMacros( CSoundParametersInternal& params, char const *wavename ) override;
	gender_t	GetActorGender( char const *actormodel ) override;
	void		GenderExpandString( char const *actormodel, char const *in, char *out, int maxlen ) override;
	void		GenderExpandString( gender_t gender, char const *in, char *out, int maxlen ) override;
	bool		IsUsingGenderToken( char const *soundname ) override;
	unsigned int GetManifestFileTimeChecksum() override;

	bool			GetParametersForSoundEx( const char *soundname, HSOUNDSCRIPTHANDLE& handle, CSoundParameters& params, gender_t gender, bool isbeingemitted = false ) override;
	soundlevel_t	LookupSoundLevelByHandle( char const *soundname, HSOUNDSCRIPTHANDLE& handle ) override;


	// Called from both client and server (single player) or just one (server only in dedicated server and client only if connected to a remote server)
	// Called by LevelInitPreEntity to override sound scripts for the mod with level specific overrides based on custom mapnames, etc.
	void			AddSoundOverrides( char const *scriptfile, bool bPreload = false ) override;

	// Called by either client or server in LevelShutdown to clear out custom overrides
	void			ClearSoundOverrides() override;

	void			ReloadSoundEntriesInList( IFileList *pFilesToReload ) override;

	// Called by either client or server to force ModShutdown and ModInit
	void			Flush() override;

private:

	bool InternalModInit();
	void InternalModShutdown();

	void AddSoundsFromFile( const char *filename, bool bPreload, bool bIsOverride = false, bool bRefresh = false );

	bool		InitSoundInternalParameters( const char *soundname, KeyValues *kv, CSoundParametersInternal& params );

	void LoadGlobalActors();

	float	TranslateAttenuation( const char *key );
	soundlevel_t	TranslateSoundLevel( const char *key );
	int TranslateChannel( const char *name );

	int		FindBestSoundForGender( SoundFile *pSoundnames, int c, gender_t gender );
	void	EnsureAvailableSlotsForGender( SoundFile *pSoundnames, int c, gender_t gender );
	void	AddSoundName( CSoundParametersInternal& params, char const *wavename, gender_t gender );

	CUtlHashtable< CUtlConstString, gender_t, CaselessStringHashFunctor, UTLConstStringCaselessStringEqualFunctor<char> > m_ActorGenders;
	CUtlStableHashtable< CSoundEntry*, empty_t, CSoundEntryHashFunctor, CSoundEntryEqualFunctor, uint16, const char* > m_Sounds;

    CUtlVector< CSoundEntry * >			m_SavedOverrides; 
	CUtlVector< FileNameHandle_t >				m_OverrideFiles;

	struct CSoundScriptFile
	{
		FileNameHandle_t hFilename;
		bool		dirty;
	};

	CUtlVector< CSoundScriptFile >	m_SoundKeyValues;
	int					m_nInitCount;
	unsigned int		m_uManifestPlusScriptChecksum;

	CUtlSymbolTable		m_Waves;
};

#endif // SOUNDEMITTERSYSTEMBASE_H
