//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef RIFF_H
#define RIFF_H
#pragma once

#include "tier0/basetypes.h"
#include "tier0/commonmacros.h"


//-----------------------------------------------------------------------------
// Purpose: This is a simple abstraction that the RIFF classes use to read from
//			files/memory
//-----------------------------------------------------------------------------
class IFileReadBinary
{
public:
	[[nodiscard]] virtual intp open( const char *pFileName ) = 0;
	virtual int read( void *pOutput, int size, intp file ) = 0;
	virtual void close( intp file ) = 0;
	virtual void seek( intp file, int pos ) = 0;
	[[nodiscard]] virtual unsigned int tell( intp file ) = 0;
	[[nodiscard]] virtual unsigned int size( intp file ) = 0;
};


//-----------------------------------------------------------------------------
// Purpose: Used to read/parse a RIFF format file
//-----------------------------------------------------------------------------
class InFileRIFF
{
public:
	InFileRIFF( const char *pFileName, IFileReadBinary &io );
	~InFileRIFF( void );

	[[nodiscard]] unsigned int RIFFName() const { return m_riffName; }
	[[nodiscard]] unsigned int RIFFSize() const { return m_riffSize; }

	[[nodiscard]] int		ReadInt();
	int		ReadData( void *pOutput, int dataSize );
	[[nodiscard]] int		PositionGet();
	void	PositionSet( int position );
	[[nodiscard]] bool	IsValid() const { return m_file != 0; }

private:
	const InFileRIFF & operator=( const InFileRIFF & ) = delete;

	IFileReadBinary		&m_io;
	intp				m_file;
	unsigned int		m_riffName;
	unsigned int		m_riffSize;
};


//-----------------------------------------------------------------------------
// Purpose: Used to iterate over an InFileRIFF
//-----------------------------------------------------------------------------
class IterateRIFF
{
public:
	IterateRIFF( InFileRIFF &riff, int size );
	IterateRIFF( IterateRIFF &parent );

	[[nodiscard]] bool			ChunkAvailable();
	bool			ChunkNext();

	[[nodiscard]] unsigned int	ChunkName();
	[[nodiscard]] unsigned int	ChunkSize();
	int				ChunkRead( void *pOutput );
	int				ChunkReadPartial( void *pOutput, int dataSize );
	[[nodiscard]] int				ChunkReadInt();
	[[nodiscard]] int				ChunkFilePosition() const { return m_chunkPosition; }

private:
	const IterateRIFF & operator=( const IterateRIFF & ) = delete;

	void	ChunkSetup( void );
	void	ChunkClear( void );

	InFileRIFF			&m_riff;
	int					m_start;
	int					m_size;

	unsigned int		m_chunkName;
	int					m_chunkSize;
	int					m_chunkPosition;
};

class IFileWriteBinary
{
public:
	[[nodiscard]] virtual intp create( const char *pFileName ) = 0;
	virtual int write( void *pData, int size, intp file ) = 0;
	virtual void close( intp file ) = 0;
	virtual void seek( intp file, int pos ) = 0;
	[[nodiscard]] virtual unsigned int tell( intp file ) = 0;
};
//-----------------------------------------------------------------------------
// Purpose: Used to write a RIFF format file
//-----------------------------------------------------------------------------
class OutFileRIFF
{
public:
	OutFileRIFF( const char *pFileName, IFileWriteBinary &io );
	~OutFileRIFF( void );

	bool	WriteInt( int number );
	bool 	WriteData( void *pOutput, int dataSize );
	[[nodiscard]] int		PositionGet();
	void	PositionSet( int position );
	[[nodiscard]] bool	IsValid() const { return m_file != 0; }
	
	void    HasLISETData( int position );

private:
	const OutFileRIFF & operator=( const OutFileRIFF & ) = delete;

	IFileWriteBinary	&m_io;
	intp				m_file;
	unsigned int		m_riffName;
	unsigned int		m_riffSize;
	unsigned int		m_nNamePos;

	// hack to make liset work correctly
	bool				m_bUseIncorrectLISETLength;
	int					m_nLISETSize;


};

//-----------------------------------------------------------------------------
// Purpose: Used to iterate over an InFileRIFF
//-----------------------------------------------------------------------------
class IterateOutputRIFF
{
public:
	IterateOutputRIFF( OutFileRIFF &riff );
	IterateOutputRIFF( IterateOutputRIFF &parent );

	void			ChunkStart( unsigned int chunkname );
	void			ChunkFinish( void );

	void			ChunkWrite( unsigned int chunkname, void *pOutput, int size );
	void			ChunkWriteInt( int number );
	void			ChunkWriteData( void *pOutput, int size );

	[[nodiscard]] int	ChunkFilePosition() const { return m_chunkPosition; }

	[[nodiscard]] unsigned int	ChunkGetPosition();
	void			ChunkSetPosition( int position );

	void			CopyChunkData( IterateRIFF& input );

	void			SetLISETData( int position );

private:

	const IterateOutputRIFF & operator=( const IterateOutputRIFF & ) = delete;

	OutFileRIFF			&m_riff;
	int					m_start;
	int					m_size;

	unsigned int		m_chunkName;
	int					m_chunkSize;
	int					m_chunkPosition;
	int					m_chunkStart;
};

constexpr inline int RIFF_ID{MAKEID('R', 'I', 'F', 'F')};
constexpr inline unsigned RIFF_WAVE{MAKEUID('W', 'A', 'V', 'E')};
constexpr inline int WAVE_FMT{MAKEID('f', 'm', 't', ' ')};
constexpr inline int WAVE_DATA{MAKEID('d','a','t','a')};
constexpr inline int WAVE_FACT{MAKEID('f','a','c','t')};
constexpr inline int WAVE_CUE{MAKEID('c','u','e',' ')};
constexpr inline int WAVE_SAMPLER{MAKEID('s','m','p','l')};
constexpr inline int WAVE_VALVEDATA{MAKEID('V','D','A','T')};
constexpr inline int WAVE_PADD{MAKEID('P','A','D','D')};
constexpr inline int WAVE_LIST{MAKEID('L','I','S','T')}; 

#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM			0x0001
#endif
#ifndef WAVE_FORMAT_ADPCM
#define WAVE_FORMAT_ADPCM		0x0002
#endif
#define WAVE_FORMAT_XBOX_ADPCM	0x0069
#ifndef WAVE_FORMAT_XMA
#define WAVE_FORMAT_XMA			0x0165
#endif

#endif // RIFF_H
