#include "shaderlib/cshader.h"
class introscreenspaceeffect_ps20b_Static_Index
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
	int m_nLINEAR_TO_SRGB;
#ifndef NDEBUG
	bool m_bLINEAR_TO_SRGB;
#endif
public:
	void SetLINEAR_TO_SRGB( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nLINEAR_TO_SRGB = i;
#ifndef NDEBUG
		m_bLINEAR_TO_SRGB = true;
#endif
	}
	void SetLINEAR_TO_SRGB( bool i )
	{
		m_nLINEAR_TO_SRGB = i ? 1 : 0;
#ifndef NDEBUG
		m_bLINEAR_TO_SRGB = true;
#endif
	}
public:
	introscreenspaceeffect_ps20b_Static_Index( )
	{
#ifndef NDEBUG
		m_bCONVERT_TO_SRGB = true;
#endif  // !NDEBUG
		m_nCONVERT_TO_SRGB =  g_pHardwareConfig->NeedsShaderSRGBConversion();
#ifndef NDEBUG
		m_bLINEAR_TO_SRGB = false;
#endif  // !NDEBUG
		m_nLINEAR_TO_SRGB = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bCONVERT_TO_SRGB && m_bLINEAR_TO_SRGB;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 10 * m_nCONVERT_TO_SRGB ) + ( 20 * m_nLINEAR_TO_SRGB ) + 0;
	}
};
#define shaderStaticTest_introscreenspaceeffect_ps20b psh_forgot_to_set_static_LINEAR_TO_SRGB + 0
class introscreenspaceeffect_ps20b_Dynamic_Index
{
private:
	int m_nMODE;
#ifndef NDEBUG
	bool m_bMODE;
#endif
public:
	void SetMODE( int i )
	{
		Assert( i >= 0 && i <= 9 );
		m_nMODE = i;
#ifndef NDEBUG
		m_bMODE = true;
#endif
	}
	void SetMODE( bool i )
	{
		m_nMODE = i ? 1 : 0;
#ifndef NDEBUG
		m_bMODE = true;
#endif
	}
public:
	introscreenspaceeffect_ps20b_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bMODE = false;
#endif  // !NDEBUG
		m_nMODE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bMODE;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nMODE ) + 0;
	}
};
#define shaderDynamicTest_introscreenspaceeffect_ps20b psh_forgot_to_set_dynamic_MODE + 0
