#include "shaderlib/cshader.h"
class debugdrawenvmapmask_ps20b_Static_Index
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
	debugdrawenvmapmask_ps20b_Static_Index( )
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
		return ( 2 * m_nCONVERT_TO_SRGB ) + 0;
	}
};
#define shaderStaticTest_debugdrawenvmapmask_ps20b 0
class debugdrawenvmapmask_ps20b_Dynamic_Index
{
private:
	int m_nSHOWALPHA;
#ifndef NDEBUG
	bool m_bSHOWALPHA;
#endif
public:
	void SetSHOWALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSHOWALPHA = i;
#ifndef NDEBUG
		m_bSHOWALPHA = true;
#endif
	}
	void SetSHOWALPHA( bool i )
	{
		m_nSHOWALPHA = i ? 1 : 0;
#ifndef NDEBUG
		m_bSHOWALPHA = true;
#endif
	}
public:
	debugdrawenvmapmask_ps20b_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bSHOWALPHA = false;
#endif  // !NDEBUG
		m_nSHOWALPHA = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bSHOWALPHA;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nSHOWALPHA ) + 0;
	}
};
#define shaderDynamicTest_debugdrawenvmapmask_ps20b psh_forgot_to_set_dynamic_SHOWALPHA + 0
