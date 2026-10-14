#include "shaderlib/cshader.h"
class teeth_bump_ps30_Static_Index
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
public:
	teeth_bump_ps30_Static_Index( )
	{
#ifndef NDEBUG
		m_bCONVERT_TO_SRGB = true;
#endif  // !NDEBUG
		m_nCONVERT_TO_SRGB =  g_pHardwareConfig->NeedsShaderSRGBConversion();
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bCONVERT_TO_SRGB;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 40 * m_nCONVERT_TO_SRGB ) + 0;
	}
};
#define shaderStaticTest_teeth_bump_ps30 0
class teeth_bump_ps30_Dynamic_Index
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
	int m_nNUM_LIGHTS;
#ifndef NDEBUG
	bool m_bNUM_LIGHTS;
#endif
public:
	void SetNUM_LIGHTS( int i )
	{
		Assert( i >= 0 && i <= 4 );
		m_nNUM_LIGHTS = i;
#ifndef NDEBUG
		m_bNUM_LIGHTS = true;
#endif
	}
	void SetNUM_LIGHTS( bool i )
	{
		m_nNUM_LIGHTS = i ? 1 : 0;
#ifndef NDEBUG
		m_bNUM_LIGHTS = true;
#endif
	}
private:
	int m_nAMBIENT_LIGHT;
#ifndef NDEBUG
	bool m_bAMBIENT_LIGHT;
#endif
public:
	void SetAMBIENT_LIGHT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nAMBIENT_LIGHT = i;
#ifndef NDEBUG
		m_bAMBIENT_LIGHT = true;
#endif
	}
	void SetAMBIENT_LIGHT( bool i )
	{
		m_nAMBIENT_LIGHT = i ? 1 : 0;
#ifndef NDEBUG
		m_bAMBIENT_LIGHT = true;
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
	teeth_bump_ps30_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bPIXELFOGTYPE = false;
#endif  // !NDEBUG
		m_nPIXELFOGTYPE = 0;
#ifndef NDEBUG
		m_bNUM_LIGHTS = false;
#endif  // !NDEBUG
		m_nNUM_LIGHTS = 0;
#ifndef NDEBUG
		m_bAMBIENT_LIGHT = false;
#endif  // !NDEBUG
		m_nAMBIENT_LIGHT = 0;
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
		bool bAllDynamicVarsDefined = m_bPIXELFOGTYPE && m_bNUM_LIGHTS && m_bAMBIENT_LIGHT && m_bWRITE_DEPTH_TO_DESTALPHA;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nPIXELFOGTYPE ) + ( 2 * m_nNUM_LIGHTS ) + ( 10 * m_nAMBIENT_LIGHT ) + ( 20 * m_nWRITE_DEPTH_TO_DESTALPHA ) + 0;
	}
};
#define shaderDynamicTest_teeth_bump_ps30 psh_forgot_to_set_dynamic_PIXELFOGTYPE + psh_forgot_to_set_dynamic_NUM_LIGHTS + psh_forgot_to_set_dynamic_AMBIENT_LIGHT + psh_forgot_to_set_dynamic_WRITE_DEPTH_TO_DESTALPHA + 0
