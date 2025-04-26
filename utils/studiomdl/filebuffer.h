//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef FILEBUFFER_H
#define FILEBUFFER_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/smartptr.h"
// dimhotepus: Drop P4.
// #include "tier2/p4helpers.h"

class CFileBuffer
{
public:
	CFileBuffer( int size )
	{
		m_pData = new unsigned char[size];
#ifdef _DEBUG
		m_pUsed = new const char *[size];
		memset( m_pUsed, 0, size * sizeof( const char * ) );
#endif
		m_Size = size;
		m_pCurPos = m_pData;
#ifdef _DEBUG
		memset( m_pData, 0xbaadf00d, size );
#endif
	}
	~CFileBuffer()
	{
		delete [] m_pData;
#ifdef _DEBUG
		delete [] m_pUsed;
#endif
	}

#ifdef _DEBUG
	void TestWritten( intp EndOfFileOffset )
	{
		if ( !g_quiet )
		{
			printf( "testing to make sure that the whole file has been written\n" );
		}

		for( intp i = 0; i < EndOfFileOffset; i++ )
		{
			if( !m_pUsed[i] )
			{
				fprintf( stderr, "offset %zd not written, end of file invalid!\n", i );
				Assert( 0 );
			}
		}
	}
#endif
	
	void WriteToFile( const char *fileName, int size )
	{
		// dimhotepus: Drop P4.
		//CPlainAutoPtr< CP4File > spFile( g_p4factory->AccessFile( fileName ) );
		//spFile->Edit();

		FILE *fp = fopen( fileName, "wb" );
		if( !fp )
		{
			MdlWarning( "Can't open \"%s\" for writing!\n", fileName );
			return;
		}

		fwrite( m_pData, 1, size, fp );
		
		fclose( fp );
		//spFile->Add();
	}
	
	void WriteAt( intp offset, void *data, intp size, const char *name )
	{
		m_pCurPos = m_pData + offset;

#ifdef _DEBUG
		const char **used = m_pUsed + offset;
		bool bitched = false;
		for( intp i = 0; i < size; i++ )
		{
			if( used[i] )
			{
				if( !bitched )
				{
					fprintf( stderr, "overwrite at %zd! (overwriting \"%s\" with \"%s\")\n", i + offset, used[i], name );
					Assert( 0 );
					bitched = true;
				}
			}
			else
			{
				used[i] = name;
			}
		}
#endif // _DEBUG

		Append( data, size );
	}
	int GetOffset( void )
	{
		return m_pCurPos - m_pData;
	}
	void *GetPointer( int offset )
	{
		return m_pData + offset;
	}
private:
	void Append( void *data, int size )
	{
		Assert( m_pCurPos + size - m_pData < m_Size );
		memcpy( m_pCurPos, data, size );
		m_pCurPos += size;
	}
	CFileBuffer(); // undefined
	int m_Size;
	unsigned char *m_pData;
	unsigned char *m_pCurPos;
#ifdef _DEBUG
	const char **m_pUsed;
#endif
};
	

#endif // FILEBUFFER_H
