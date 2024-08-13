//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "winlite.h"
#include "shader_dll_verify.h"


static unsigned char *g_pLastInputData = 0;
static HANDLE g_hDLLInst = 0;


extern "C"
{
	void __declspec( dllexport ) _ftol3( char *pData );
};


extern "C" BOOL WINAPI DllMain (HANDLE hInst, ULONG ulInit, LPVOID)
{
	if (ulInit == DLL_PROCESS_ATTACH)
	{
		g_hDLLInst = hInst;
	}
	else if (ulInit == DLL_PROCESS_DETACH)
	{
		g_hDLLInst = nullptr;
	}

	return true;
}


class CShaderDLLVerification : public IShaderDLLVerification
{
public:

	virtual CRC32_t Function1( unsigned char *pData );
	virtual void Function2( int a, int b, int c );
	virtual void Function3( int a, int b, int c );
	virtual void Function4( int a, int b, int c );
	virtual CRC32_t Function5();
};

static CShaderDLLVerification g_Blah;



// The main exported function.. return a pointer to g_Blah.
void __declspec( dllexport ) _ftol3( char *pData )
{
	pData += SHADER_DLL_VERIFY_DATA_PTR_OFFSET;
	IShaderDLLVerification *pToFillIn = &g_Blah;
	memcpy( pData, &pToFillIn, sizeof(IShaderDLLVerification*) );
}


CRC32_t CShaderDLLVerification::Function1( unsigned char *pData )
{
	pData += SHADER_DLL_VERIFY_DATA_PTR_OFFSET;
	g_pLastInputData = (unsigned char*)pData;

	void *pVerifyPtr1 = &g_Blah;
	
	CRC32_t testCRC;
	CRC32_Init( &testCRC );
	CRC32_ProcessBuffer( &testCRC, pData, SHADER_DLL_VERIFY_DATA_LEN1 );
	CRC32_ProcessBuffer( &testCRC, &g_hDLLInst, sizeof(g_hDLLInst) );
	CRC32_ProcessBuffer( &testCRC, &pVerifyPtr1, sizeof(pVerifyPtr1) );
	CRC32_Final( &testCRC );

	return testCRC;
}

void CShaderDLLVerification::Function2( int, int, int )
{
	MD5Context_t md5Context;
	MD5Init( &md5Context );
	MD5Update( &md5Context, g_pLastInputData + SHADER_DLL_VERIFY_DATA_PTR_OFFSET, SHADER_DLL_VERIFY_DATA_LEN1 - SHADER_DLL_VERIFY_DATA_PTR_OFFSET );
	MD5Final( g_pLastInputData, &md5Context );
}

void CShaderDLLVerification::Function3( int, int, int )
{
}

void CShaderDLLVerification::Function4( int, int, int )
{
}


CRC32_t CShaderDLLVerification::Function5()
{
	return 32423;
}
