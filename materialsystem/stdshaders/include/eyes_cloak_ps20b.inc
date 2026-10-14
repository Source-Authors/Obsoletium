#include "shaderlib/cshader.h"
class eyes_cloak_ps20b_Static_Index
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
private:
	int m_nBLUR;
#ifndef NDEBUG
	bool m_bBLUR;
#endif
public:
	void SetBLUR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBLUR = i;
#ifndef NDEBUG
		m_bBLUR = true;
#endif
	}
	void SetBLUR( bool i )
	{
		m_nBLUR = i ? 1 : 0;
#ifndef NDEBUG
		m_bBLUR = true;
#endif
	}
private:
	int m_nFADEOUTONSILHOUETTE;
#ifndef NDEBUG
	bool m_bFADEOUTONSILHOUETTE;
#endif
public:
	void SetFADEOUTONSILHOUETTE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFADEOUTONSILHOUETTE = i;
#ifndef NDEBUG
		m_bFADEOUTONSILHOUETTE = true;
#endif
	}
	void SetFADEOUTONSILHOUETTE( bool i )
	{
		m_nFADEOUTONSILHOUETTE = i ? 1 : 0;
#ifndef NDEBUG
		m_bFADEOUTONSILHOUETTE = true;
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
	int m_nREFRACTTINTTEXTURE;
#ifndef NDEBUG
	bool m_bREFRACTTINTTEXTURE;
#endif
public:
	void SetREFRACTTINTTEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nREFRACTTINTTEXTURE = i;
#ifndef NDEBUG
		m_bREFRACTTINTTEXTURE = true;
#endif
	}
	void SetREFRACTTINTTEXTURE( bool i )
	{
		m_nREFRACTTINTTEXTURE = i ? 1 : 0;
#ifndef NDEBUG
		m_bREFRACTTINTTEXTURE = true;
#endif
	}
private:
	int m_nMASKED;
#ifndef NDEBUG
	bool m_bMASKED;
#endif
public:
	void SetMASKED( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMASKED = i;
#ifndef NDEBUG
		m_bMASKED = true;
#endif
	}
	void SetMASKED( bool i )
	{
		m_nMASKED = i ? 1 : 0;
#ifndef NDEBUG
		m_bMASKED = true;
#endif
	}
public:
	eyes_cloak_ps20b_Static_Index( )
	{
#ifndef NDEBUG
		m_bCONVERT_TO_SRGB = true;
#endif  // !NDEBUG
		m_nCONVERT_TO_SRGB =  g_pHardwareConfig->NeedsShaderSRGBConversion();
#ifndef NDEBUG
		m_bHDRTYPE = false;
#endif  // !NDEBUG
		m_nHDRTYPE = 0;
#ifndef NDEBUG
		m_bBLUR = false;
#endif  // !NDEBUG
		m_nBLUR = 0;
#ifndef NDEBUG
		m_bFADEOUTONSILHOUETTE = false;
#endif  // !NDEBUG
		m_nFADEOUTONSILHOUETTE = 0;
#ifndef NDEBUG
		m_bCUBEMAP = false;
#endif  // !NDEBUG
		m_nCUBEMAP = 0;
#ifndef NDEBUG
		m_bREFRACTTINTTEXTURE = false;
#endif  // !NDEBUG
		m_nREFRACTTINTTEXTURE = 0;
#ifndef NDEBUG
		m_bMASKED = false;
#endif  // !NDEBUG
		m_nMASKED = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bCONVERT_TO_SRGB && m_bHDRTYPE && m_bBLUR && m_bFADEOUTONSILHOUETTE && m_bCUBEMAP && m_bREFRACTTINTTEXTURE && m_bMASKED;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 4 * m_nCONVERT_TO_SRGB ) + ( 8 * m_nHDRTYPE ) + ( 24 * m_nBLUR ) + ( 48 * m_nFADEOUTONSILHOUETTE ) + ( 96 * m_nCUBEMAP ) + ( 192 * m_nREFRACTTINTTEXTURE ) + ( 384 * m_nMASKED ) + 0;
	}
};
#define shaderStaticTest_eyes_cloak_ps20b psh_forgot_to_set_static_HDRTYPE + psh_forgot_to_set_static_BLUR + psh_forgot_to_set_static_FADEOUTONSILHOUETTE + psh_forgot_to_set_static_CUBEMAP + psh_forgot_to_set_static_REFRACTTINTTEXTURE + psh_forgot_to_set_static_MASKED + 0
class eyes_cloak_ps20b_Dynamic_Index
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
	eyes_cloak_ps20b_Dynamic_Index()
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
#define shaderDynamicTest_eyes_cloak_ps20b psh_forgot_to_set_dynamic_HDRENABLED + psh_forgot_to_set_dynamic_PIXELFOGTYPE + 0
