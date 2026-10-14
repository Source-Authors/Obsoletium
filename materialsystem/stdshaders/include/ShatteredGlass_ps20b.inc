#include "shaderlib/cshader.h"
class shatteredglass_ps20b_Static_Index
{
private:
	int m_nCONVERT_TO_SRGB;
#ifndef NDEBUG
	bool m_bCONVERT_TO_SRGB;
#endif
public:
	void SetCONVERT_TO_SRGB( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCONVERT_TO_SRGB = i;
#ifndef NDEBUG
		m_bCONVERT_TO_SRGB = true;
#endif
	}
	void SetCONVERT_TO_SRGB( bool i )
	{
		m_nCONVERT_TO_SRGB = i ? 1 : 0;
#ifndef NDEBUG
		m_bCONVERT_TO_SRGB = true;
#endif
	}
private:
	int m_nCUBEMAP;
#ifndef NDEBUG
	bool m_bCUBEMAP;
#endif
public:
	void SetCUBEMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCUBEMAP = i;
#ifndef NDEBUG
		m_bCUBEMAP = true;
#endif
	}
	void SetCUBEMAP( bool i )
	{
		m_nCUBEMAP = i ? 1 : 0;
#ifndef NDEBUG
		m_bCUBEMAP = true;
#endif
	}
private:
	int m_nVERTEXCOLOR;
#ifndef NDEBUG
	bool m_bVERTEXCOLOR;
#endif
public:
	void SetVERTEXCOLOR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nVERTEXCOLOR = i;
#ifndef NDEBUG
		m_bVERTEXCOLOR = true;
#endif
	}
	void SetVERTEXCOLOR( bool i )
	{
		m_nVERTEXCOLOR = i ? 1 : 0;
#ifndef NDEBUG
		m_bVERTEXCOLOR = true;
#endif
	}
private:
	int m_nENVMAPMASK;
#ifndef NDEBUG
	bool m_bENVMAPMASK;
#endif
public:
	void SetENVMAPMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nENVMAPMASK = i;
#ifndef NDEBUG
		m_bENVMAPMASK = true;
#endif
	}
	void SetENVMAPMASK( bool i )
	{
		m_nENVMAPMASK = i ? 1 : 0;
#ifndef NDEBUG
		m_bENVMAPMASK = true;
#endif
	}
private:
	int m_nBASEALPHAENVMAPMASK;
#ifndef NDEBUG
	bool m_bBASEALPHAENVMAPMASK;
#endif
public:
	void SetBASEALPHAENVMAPMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBASEALPHAENVMAPMASK = i;
#ifndef NDEBUG
		m_bBASEALPHAENVMAPMASK = true;
#endif
	}
	void SetBASEALPHAENVMAPMASK( bool i )
	{
		m_nBASEALPHAENVMAPMASK = i ? 1 : 0;
#ifndef NDEBUG
		m_bBASEALPHAENVMAPMASK = true;
#endif
	}
private:
	int m_nHDRTYPE;
#ifndef NDEBUG
	bool m_bHDRTYPE;
#endif
public:
	void SetHDRTYPE( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nHDRTYPE = i;
#ifndef NDEBUG
		m_bHDRTYPE = true;
#endif
	}
	void SetHDRTYPE( bool i )
	{
		m_nHDRTYPE = i ? 1 : 0;
#ifndef NDEBUG
		m_bHDRTYPE = true;
#endif
	}
public:
	shatteredglass_ps20b_Static_Index( )
	{
#ifndef NDEBUG
		m_bCONVERT_TO_SRGB = true;
#endif  // !NDEBUG
		m_nCONVERT_TO_SRGB =  g_pHardwareConfig->NeedsShaderSRGBConversion();
#ifndef NDEBUG
		m_bCUBEMAP = false;
#endif  // !NDEBUG
		m_nCUBEMAP = 0;
#ifndef NDEBUG
		m_bVERTEXCOLOR = false;
#endif  // !NDEBUG
		m_nVERTEXCOLOR = 0;
#ifndef NDEBUG
		m_bENVMAPMASK = false;
#endif  // !NDEBUG
		m_nENVMAPMASK = 0;
#ifndef NDEBUG
		m_bBASEALPHAENVMAPMASK = false;
#endif  // !NDEBUG
		m_nBASEALPHAENVMAPMASK = 0;
#ifndef NDEBUG
		m_bHDRTYPE = false;
#endif  // !NDEBUG
		m_nHDRTYPE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bCONVERT_TO_SRGB && m_bCUBEMAP && m_bVERTEXCOLOR && m_bENVMAPMASK && m_bBASEALPHAENVMAPMASK && m_bHDRTYPE;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 4 * m_nCONVERT_TO_SRGB ) + ( 8 * m_nCUBEMAP ) + ( 16 * m_nVERTEXCOLOR ) + ( 32 * m_nENVMAPMASK ) + ( 64 * m_nBASEALPHAENVMAPMASK ) + ( 128 * m_nHDRTYPE ) + 0;
	}
};
#define shaderStaticTest_shatteredglass_ps20b psh_forgot_to_set_static_CUBEMAP + psh_forgot_to_set_static_VERTEXCOLOR + psh_forgot_to_set_static_ENVMAPMASK + psh_forgot_to_set_static_BASEALPHAENVMAPMASK + psh_forgot_to_set_static_HDRTYPE + 0
class shatteredglass_ps20b_Dynamic_Index
{
private:
	int m_nHDRENABLED;
#ifndef NDEBUG
	bool m_bHDRENABLED;
#endif
public:
	void SetHDRENABLED( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nHDRENABLED = i;
#ifndef NDEBUG
		m_bHDRENABLED = true;
#endif
	}
	void SetHDRENABLED( bool i )
	{
		m_nHDRENABLED = i ? 1 : 0;
#ifndef NDEBUG
		m_bHDRENABLED = true;
#endif
	}
private:
	int m_nPIXELFOGTYPE;
#ifndef NDEBUG
	bool m_bPIXELFOGTYPE;
#endif
public:
	void SetPIXELFOGTYPE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPIXELFOGTYPE = i;
#ifndef NDEBUG
		m_bPIXELFOGTYPE = true;
#endif
	}
	void SetPIXELFOGTYPE( bool i )
	{
		m_nPIXELFOGTYPE = i ? 1 : 0;
#ifndef NDEBUG
		m_bPIXELFOGTYPE = true;
#endif
	}
public:
	shatteredglass_ps20b_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bHDRENABLED = false;
#endif  // !NDEBUG
		m_nHDRENABLED = 0;
#ifndef NDEBUG
		m_bPIXELFOGTYPE = false;
#endif  // !NDEBUG
		m_nPIXELFOGTYPE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bHDRENABLED && m_bPIXELFOGTYPE;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nHDRENABLED ) + ( 2 * m_nPIXELFOGTYPE ) + 0;
	}
};
#define shaderDynamicTest_shatteredglass_ps20b psh_forgot_to_set_dynamic_HDRENABLED + psh_forgot_to_set_dynamic_PIXELFOGTYPE + 0
