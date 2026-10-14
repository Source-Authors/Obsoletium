#include "shaderlib/cshader.h"
class debugtextureview_ps20b_Static_Index
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
	debugtextureview_ps20b_Static_Index( )
	{
#ifndef NDEBUG
		m_bCONVERT_TO_SRGB = true;
#endif  // !NDEBUG
		m_nCONVERT_TO_SRGB =  g_pHardwareConfig->NeedsShaderSRGBConversion();
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
		bool bAllStaticVarsDefined = m_bCONVERT_TO_SRGB && m_bSHOWALPHA;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 2 * m_nCONVERT_TO_SRGB ) + ( 4 * m_nSHOWALPHA ) + 0;
	}
};
#define shaderStaticTest_debugtextureview_ps20b psh_forgot_to_set_static_SHOWALPHA + 0
class debugtextureview_ps20b_Dynamic_Index
{
private:
	int m_nISCUBEMAP;
#ifndef NDEBUG
	bool m_bISCUBEMAP;
#endif
public:
	void SetISCUBEMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nISCUBEMAP = i;
#ifndef NDEBUG
		m_bISCUBEMAP = true;
#endif
	}
	void SetISCUBEMAP( bool i )
	{
		m_nISCUBEMAP = i ? 1 : 0;
#ifndef NDEBUG
		m_bISCUBEMAP = true;
#endif
	}
public:
	debugtextureview_ps20b_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bISCUBEMAP = false;
#endif  // !NDEBUG
		m_nISCUBEMAP = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bISCUBEMAP;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nISCUBEMAP ) + 0;
	}
};
#define shaderDynamicTest_debugtextureview_ps20b psh_forgot_to_set_dynamic_ISCUBEMAP + 0
