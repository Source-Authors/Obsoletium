#include "shaderlib/cshader.h"
class water_ps20b_Static_Index
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
	int m_nBASETEXTURE;
#ifndef NDEBUG
	bool m_bBASETEXTURE;
#endif
public:
	void SetBASETEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBASETEXTURE = i;
#ifndef NDEBUG
		m_bBASETEXTURE = true;
#endif
	}
	void SetBASETEXTURE( bool i )
	{
		m_nBASETEXTURE = i ? 1 : 0;
#ifndef NDEBUG
		m_bBASETEXTURE = true;
#endif
	}
private:
	int m_nMULTITEXTURE;
#ifndef NDEBUG
	bool m_bMULTITEXTURE;
#endif
public:
	void SetMULTITEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMULTITEXTURE = i;
#ifndef NDEBUG
		m_bMULTITEXTURE = true;
#endif
	}
	void SetMULTITEXTURE( bool i )
	{
		m_nMULTITEXTURE = i ? 1 : 0;
#ifndef NDEBUG
		m_bMULTITEXTURE = true;
#endif
	}
private:
	int m_nREFLECT;
#ifndef NDEBUG
	bool m_bREFLECT;
#endif
public:
	void SetREFLECT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nREFLECT = i;
#ifndef NDEBUG
		m_bREFLECT = true;
#endif
	}
	void SetREFLECT( bool i )
	{
		m_nREFLECT = i ? 1 : 0;
#ifndef NDEBUG
		m_bREFLECT = true;
#endif
	}
private:
	int m_nREFRACT;
#ifndef NDEBUG
	bool m_bREFRACT;
#endif
public:
	void SetREFRACT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nREFRACT = i;
#ifndef NDEBUG
		m_bREFRACT = true;
#endif
	}
	void SetREFRACT( bool i )
	{
		m_nREFRACT = i ? 1 : 0;
#ifndef NDEBUG
		m_bREFRACT = true;
#endif
	}
private:
	int m_nABOVEWATER;
#ifndef NDEBUG
	bool m_bABOVEWATER;
#endif
public:
	void SetABOVEWATER( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nABOVEWATER = i;
#ifndef NDEBUG
		m_bABOVEWATER = true;
#endif
	}
	void SetABOVEWATER( bool i )
	{
		m_nABOVEWATER = i ? 1 : 0;
#ifndef NDEBUG
		m_bABOVEWATER = true;
#endif
	}
private:
	int m_nBLURRY_REFRACT;
#ifndef NDEBUG
	bool m_bBLURRY_REFRACT;
#endif
public:
	void SetBLURRY_REFRACT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBLURRY_REFRACT = i;
#ifndef NDEBUG
		m_bBLURRY_REFRACT = true;
#endif
	}
	void SetBLURRY_REFRACT( bool i )
	{
		m_nBLURRY_REFRACT = i ? 1 : 0;
#ifndef NDEBUG
		m_bBLURRY_REFRACT = true;
#endif
	}
private:
	int m_nNORMAL_DECODE_MODE;
#ifndef NDEBUG
	bool m_bNORMAL_DECODE_MODE;
#endif
public:
	void SetNORMAL_DECODE_MODE( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nNORMAL_DECODE_MODE = i;
#ifndef NDEBUG
		m_bNORMAL_DECODE_MODE = true;
#endif
	}
	void SetNORMAL_DECODE_MODE( bool i )
	{
		m_nNORMAL_DECODE_MODE = i ? 1 : 0;
#ifndef NDEBUG
		m_bNORMAL_DECODE_MODE = true;
#endif
	}
public:
	water_ps20b_Static_Index( )
	{
#ifndef NDEBUG
		m_bCONVERT_TO_SRGB = true;
#endif  // !NDEBUG
		m_nCONVERT_TO_SRGB =  g_pHardwareConfig->NeedsShaderSRGBConversion();
#ifndef NDEBUG
		m_bBASETEXTURE = false;
#endif  // !NDEBUG
		m_nBASETEXTURE = 0;
#ifndef NDEBUG
		m_bMULTITEXTURE = false;
#endif  // !NDEBUG
		m_nMULTITEXTURE = 0;
#ifndef NDEBUG
		m_bREFLECT = false;
#endif  // !NDEBUG
		m_nREFLECT = 0;
#ifndef NDEBUG
		m_bREFRACT = false;
#endif  // !NDEBUG
		m_nREFRACT = 0;
#ifndef NDEBUG
		m_bABOVEWATER = false;
#endif  // !NDEBUG
		m_nABOVEWATER = 0;
#ifndef NDEBUG
		m_bBLURRY_REFRACT = false;
#endif  // !NDEBUG
		m_nBLURRY_REFRACT = 0;
#ifndef NDEBUG
		m_bNORMAL_DECODE_MODE = false;
#endif  // !NDEBUG
		m_nNORMAL_DECODE_MODE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bCONVERT_TO_SRGB && m_bBASETEXTURE && m_bMULTITEXTURE && m_bREFLECT && m_bREFRACT && m_bABOVEWATER && m_bBLURRY_REFRACT && m_bNORMAL_DECODE_MODE;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 4 * m_nCONVERT_TO_SRGB ) + ( 8 * m_nBASETEXTURE ) + ( 16 * m_nMULTITEXTURE ) + ( 32 * m_nREFLECT ) + ( 64 * m_nREFRACT ) + ( 128 * m_nABOVEWATER ) + ( 256 * m_nBLURRY_REFRACT ) + ( 512 * m_nNORMAL_DECODE_MODE ) + 0;
	}
};
#define shaderStaticTest_water_ps20b psh_forgot_to_set_static_BASETEXTURE + psh_forgot_to_set_static_MULTITEXTURE + psh_forgot_to_set_static_REFLECT + psh_forgot_to_set_static_REFRACT + psh_forgot_to_set_static_ABOVEWATER + psh_forgot_to_set_static_BLURRY_REFRACT + psh_forgot_to_set_static_NORMAL_DECODE_MODE + 0
class water_ps20b_Dynamic_Index
{
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
private:
	int m_nWRITE_DEPTH_TO_DESTALPHA;
#ifndef NDEBUG
	bool m_bWRITE_DEPTH_TO_DESTALPHA;
#endif
public:
	void SetWRITE_DEPTH_TO_DESTALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nWRITE_DEPTH_TO_DESTALPHA = i;
#ifndef NDEBUG
		m_bWRITE_DEPTH_TO_DESTALPHA = true;
#endif
	}
	void SetWRITE_DEPTH_TO_DESTALPHA( bool i )
	{
		m_nWRITE_DEPTH_TO_DESTALPHA = i ? 1 : 0;
#ifndef NDEBUG
		m_bWRITE_DEPTH_TO_DESTALPHA = true;
#endif
	}
public:
	water_ps20b_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bPIXELFOGTYPE = false;
#endif  // !NDEBUG
		m_nPIXELFOGTYPE = 0;
#ifndef NDEBUG
		m_bWRITE_DEPTH_TO_DESTALPHA = false;
#endif  // !NDEBUG
		m_nWRITE_DEPTH_TO_DESTALPHA = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bPIXELFOGTYPE && m_bWRITE_DEPTH_TO_DESTALPHA;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nPIXELFOGTYPE ) + ( 2 * m_nWRITE_DEPTH_TO_DESTALPHA ) + 0;
	}
};
#define shaderDynamicTest_water_ps20b psh_forgot_to_set_dynamic_PIXELFOGTYPE + psh_forgot_to_set_dynamic_WRITE_DEPTH_TO_DESTALPHA + 0
