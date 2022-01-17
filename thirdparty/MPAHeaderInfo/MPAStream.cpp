#include "stdafx.h"

#include "mpastream.h"
#include "mpaexception.h"

#include <windows.h>	// for CreateFile, CloseHandle, ...

// constructor
CMPAStream::CMPAStream(LPCTSTR szFilename)
{
	// save filename
	m_szFile = _tcsdup(szFilename);
}

CMPAStream::~CMPAStream(void)
{
	free(m_szFile);
}

DWORD CMPAStream::ReadLEValue(DWORD dwNumBytes, DWORD& dwOffset, bool bMoveOffset) const
{
	_ASSERTE(dwNumBytes > 0);
	_ASSERTE(dwNumBytes <= 4);	// max 4 byte

	BYTE* pBuffer = ReadBytes(dwNumBytes, dwOffset, bMoveOffset);

	DWORD dwResult = 0;

	// little endian extract (least significant byte first) (will work on little and big-endian computers)
	DWORD dwNumByteShifts = 0;

	for (DWORD n=0; n < dwNumBytes; n++)
	{
		dwResult |= pBuffer[n] << 8 * dwNumByteShifts++;                                                          
	}
	
	return dwResult;
}

// convert from big endian to native format (Intel=little endian) and return as DWORD (32bit)
DWORD CMPAStream::ReadBEValue(DWORD dwNumBytes, DWORD& dwOffset,  bool bMoveOffset) const
{	
	_ASSERTE(dwNumBytes > 0);
	_ASSERTE(dwNumBytes <= 4);	// max 4 byte

	BYTE* pBuffer = ReadBytes(dwNumBytes, dwOffset, bMoveOffset);

	DWORD dwResult = 0;

	// big endian extract (most significant byte first) (will work on little and big-endian computers)
	DWORD dwNumByteShifts = dwNumBytes - 1;

	for (DWORD n=0; n < dwNumBytes; n++)
	{
		dwResult |= pBuffer[n] << 8*dwNumByteShifts--; // the bit shift will do the correct byte order for you                                                           
	}
	
	return dwResult;
}