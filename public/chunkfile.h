//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef CHUNKFILE_H
#define CHUNKFILE_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "tier1/tokenreader.h"


constexpr inline int MAX_INDENT_DEPTH{80};
constexpr inline int MAX_KEYVALUE_LEN{1024};


class CChunkFile;
class Vector2D;
class Vector;
class Vector4D;


//
// Modes for Open.
//
enum ChunkFileOpenMode_t
{
	ChunkFile_Read = 0,
	ChunkFile_Write,
};


//
// Return codes.
//
enum ChunkFileResult_t
{
	ChunkFile_Ok = 0,
	ChunkFile_Fail,
	ChunkFile_OpenFail,
	ChunkFile_EndOfChunk,
	ChunkFile_EOF,
	ChunkFile_UnexpectedEOF,
	ChunkFile_UnexpectedSymbol,
	ChunkFile_OutOfMemory,
	ChunkFile_StringTooLong,
	ChunkFile_NotHandled
};


enum ChunkType_t
{
	ChunkType_Key = 0,
	ChunkType_Chunk,
};


typedef ChunkFileResult_t (*DefaultChunkHandler_t)(CChunkFile *pFile, void *pData, char const *pChunkName);

typedef ChunkFileResult_t (*ChunkHandler_t)(CChunkFile *pFile, void *pData);
typedef ChunkFileResult_t (*KeyHandler_t)(const char *szKey, const char *szValue, void *pData);
typedef bool (*ChunkErrorHandler_t)(CChunkFile *pFile, const char *szChunkName, void *pData);


struct ChunkHandlerInfo_t
{
	char szChunkName[80];
	ChunkHandler_t pfnHandler;
	void *pData;
};


struct ChunkHandlerInfoNode_t
{
	ChunkHandlerInfo_t Handler;
	struct ChunkHandlerInfoNode_t *pNext;
};


//
// Consider handling chunks with handler objects instead of callbacks.
//
//class CChunkHandler
//{
//	virtual ChunkFileResult_t HandleChunk(const char *szName);
//	virtual ChunkFileResult_t HandleKey(const char *szKey, const char *szValue);
//	virtual bool HandleError(const char *szChunkName, ChunkFileResult_t eError);
//};


class CChunkHandlerMap
{
	public:

		CChunkHandlerMap(void);
		~CChunkHandlerMap(void);

		void AddHandler(const char *pszChunkName, ChunkHandler_t pfnHandler, void *pData);
		ChunkHandler_t GetHandler(const char *pszChunkName, void **pData);

		void SetErrorHandler(ChunkErrorHandler_t pfnHandler, void *pData);
		ChunkErrorHandler_t GetErrorHandler(void **pData);

	protected:

		ChunkHandlerInfoNode_t *m_pHandlers;
		ChunkErrorHandler_t m_pfnErrorHandler;
		void *m_pErrorData;
};


class CChunkFile
{
	public:

		CChunkFile(void);
		~CChunkFile(void);

		[[nodiscard]] ChunkFileResult_t Open(const char *pszFileName, ChunkFileOpenMode_t eMode);
		[[nodiscard]] ChunkFileResult_t Close(void);
		[[nodiscard]] const char *GetErrorText(ChunkFileResult_t eResult);

		//
		// Functions for writing chunk files.
		//
		[[nodiscard]] ChunkFileResult_t BeginChunk(const char *pszChunkName);
		[[nodiscard]] ChunkFileResult_t EndChunk(void);

		[[nodiscard]] ChunkFileResult_t WriteKeyValue(const char *pszKey, const char *pszValue);
		[[nodiscard]] ChunkFileResult_t WriteKeyValueBool(const char *pszKey, bool bValue);
		[[nodiscard]] ChunkFileResult_t WriteKeyValueColor(const char *pszKey, unsigned char r, unsigned char g, unsigned char b);
		[[nodiscard]] ChunkFileResult_t WriteKeyValueFloat(const char *pszKey, float fValue);
		[[nodiscard]] ChunkFileResult_t WriteKeyValueInt(const char *pszKey, int nValue);
		[[nodiscard]] ChunkFileResult_t WriteKeyValuePoint(const char *pszKey, const Vector &Point);
		[[nodiscard]] ChunkFileResult_t WriteKeyValueVector2(const char *pszKey, const Vector2D &vec);
		[[nodiscard]] ChunkFileResult_t WriteKeyValueVector3(const char *pszKey, const Vector &vec);
		[[nodiscard]] ChunkFileResult_t WriteKeyValueVector4( const char *pszKey, const Vector4D &vec);

		[[nodiscard]] ChunkFileResult_t WriteLine(const char *pszLine);

		//
		// Functions for reading chunk files.
		//
		[[nodiscard]] ChunkFileResult_t ReadChunk(KeyHandler_t pfnKeyHandler = NULL, void *pData = NULL);
		[[nodiscard]] ChunkFileResult_t ReadNext(OUT_Z_CAP(nNameSize) char *szName, intp nNameSize,
			OUT_Z_CAP(nValueSize) char *szValue, intp nValueSize, ChunkType_t &eChunkType);
		template<intp nKeySize, intp nValueSize>
		[[nodiscard]] ChunkFileResult_t ReadNext(OUT_Z_ARRAY char (&szKey)[nKeySize],
			OUT_Z_ARRAY char (&szValue)[nValueSize], ChunkType_t &eChunkType)
		{
			return ReadNext(szKey, nKeySize, szValue, nValueSize, eChunkType);
		}
		[[nodiscard]] ChunkFileResult_t HandleChunk(const char *szChunkName);
		void HandleError(const char *szChunkName, ChunkFileResult_t eError);

		// These functions should more really be named Parsexxx and possibly moved elsewhere.
		static bool ReadKeyValueBool(const char *pszValue, bool &bBool);
		static bool ReadKeyValueColor(const char *pszValue, unsigned char &chRed, unsigned char &chGreen, unsigned char &chBlue);
		static bool ReadKeyValueInt(const char *pszValue, int &nInt);
		static bool ReadKeyValueFloat(const char *pszValue, float &flFloat);
		static bool ReadKeyValuePoint(const char *pszValue, Vector &Point);
		static bool ReadKeyValueVector2(const char *pszValue, Vector2D &vec);
		static bool ReadKeyValueVector3(const char *pszValue, Vector &vec);
	    static bool ReadKeyValueVector4( const char *pszValue, Vector4D &vec);

		// The default chunk handler gets called before any other chunk handlers.
		//
		// If the handler returns ChunkFile_Ok, then it goes into the chunk.
		// If the handler returns ChunkFile_NotHandled, then the chunk is
		//   passed to the regular handlers.
		//
		// If you pass NULL in here, then it disables the default chunk handler.
		void SetDefaultChunkHandler( DefaultChunkHandler_t pHandler, void *pData );

		void PushHandlers(CChunkHandlerMap *pHandlerMap);
		void PopHandlers(void);

	protected:

		void BuildIndentString(char *pszDest, intp destLen, intp nDepth);

		TokenReader m_TokenReader;

		FILE *m_hFile;
		char m_szErrorToken[80];
		char m_szIndent[MAX_INDENT_DEPTH];
		intp m_nCurrentDepth;

		// See SetDefaultChunkHandler..
		DefaultChunkHandler_t	m_DefaultChunkHandler;
		void					*m_pDefaultChunkHandlerData;

		CChunkHandlerMap *m_HandlerStack[MAX_INDENT_DEPTH];
		int m_nHandlerStackDepth;
};


#endif // CHUNKFILE_H
