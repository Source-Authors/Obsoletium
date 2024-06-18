//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
// Serialization/unserialization buffer
//=============================================================================//

#ifndef UTLSTREAMBUFFER_H
#define UTLSTREAMBUFFER_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlbuffer.h"
#include "filesystem.h"


//-----------------------------------------------------------------------------
// Command parsing..
//-----------------------------------------------------------------------------
class CUtlStreamBuffer : public CUtlBuffer
{
	typedef CUtlBuffer BaseClass;

public:
	// See CUtlBuffer::BufferFlags_t for flags
	CUtlStreamBuffer( );
	CUtlStreamBuffer( const char *pFileName, const char *pPath, unsigned char nFlags = 0, bool bDelayOpen = false );
	~CUtlStreamBuffer();

	// Open the file. normally done in constructor
	void Open( const char *pFileName, const char *pPath, unsigned char nFlags );

	// close the file. normally done in destructor
	void Close();

	// Is the file open?
	bool IsOpen() const;

private:
	// error flags
	enum
	{
		FILE_OPEN_ERROR = MAX_ERROR_FLAG << 1,
		FILE_WRITE_ERROR = MAX_ERROR_FLAG << 2,
	};

	// Overflow functions
	bool StreamPutOverflow( intp nSize );
	bool StreamGetOverflow( intp nSize );

	// Grow allocation size to fit requested size
	void GrowAllocatedSize( intp nSize );

	// Reads bytes from the file; fixes up maxput if necessary and null terminates
	intp ReadBytesFromFile( intp nBytesToRead, intp nReadOffset );

	FileHandle_t OpenFile( const char *pFileName, const char *pPath );

	FileHandle_t m_hFileHandle;

	char *m_pFileName;
	char *m_pPath;
};


#endif // UTLSTREAMBUFFER_H

