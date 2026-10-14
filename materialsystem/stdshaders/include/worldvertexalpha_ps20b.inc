#include "shaderlib/cshader.h"
class worldvertexalpha_ps20b_Static_Index
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
	int m_nPASS;
#ifndef NDEBUG
	bool m_bPASS;
#endif
public:
	void SetPASS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPASS = i;
#ifndef NDEBUG
		m_bPASS = true;
#endif
	}
	void SetPASS( bool i )
	{
		m_nPASS = i ? 1 : 0;
#ifndef NDEBUG
		m_bPASS = true;
#endif
	}
public:
	worldvertexalpha_ps20b_Static_Index( )
	{
#ifndef NDEBUG
		m_bCONVERT_TO_SRGB = true;
#endif  // !NDEBUG
		m_nCONVERT_TO_SRGB =  g_pHardwareConfig->NeedsShaderSRGBConversion();
#ifndef NDEBUG
		m_bPASS = false;
#endif  // !NDEBUG
		m_nPASS = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bCONVERT_TO_SRGB && m_bPASS;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nCONVERT_TO_SRGB ) + ( 2 * m_nPASS ) + 0;
	}
};
#define shaderStaticTest_worldvertexalpha_ps20b psh_forgot_to_set_static_PASS + 0
class worldvertexalpha_ps20b_Dynamic_Index
{
public:
	worldvertexalpha_ps20b_Dynamic_Index()
	{
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
#endif  // !NDEBUG
		return 0;
	}
};
#define shaderDynamicTest_worldvertexalpha_ps20b 0
