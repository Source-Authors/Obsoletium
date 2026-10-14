#include "shaderlib/cshader.h"
class vertexlit_lighting_only_ps20_Static_Index
{
private:
	int m_nDIFFUSELIGHTING;
#ifndef NDEBUG
	bool m_bDIFFUSELIGHTING;
#endif
public:
	void SetDIFFUSELIGHTING( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDIFFUSELIGHTING = i;
#ifndef NDEBUG
		m_bDIFFUSELIGHTING = true;
#endif
	}
	void SetDIFFUSELIGHTING( bool i )
	{
		m_nDIFFUSELIGHTING = i ? 1 : 0;
#ifndef NDEBUG
		m_bDIFFUSELIGHTING = true;
#endif
	}
private:
	int m_nHALFLAMBERT;
#ifndef NDEBUG
	bool m_bHALFLAMBERT;
#endif
public:
	void SetHALFLAMBERT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nHALFLAMBERT = i;
#ifndef NDEBUG
		m_bHALFLAMBERT = true;
#endif
	}
	void SetHALFLAMBERT( bool i )
	{
		m_nHALFLAMBERT = i ? 1 : 0;
#ifndef NDEBUG
		m_bHALFLAMBERT = true;
#endif
	}
public:
	vertexlit_lighting_only_ps20_Static_Index( )
	{
#ifndef NDEBUG
		m_bDIFFUSELIGHTING = false;
#endif  // !NDEBUG
		m_nDIFFUSELIGHTING = 0;
#ifndef NDEBUG
		m_bHALFLAMBERT = false;
#endif  // !NDEBUG
		m_nHALFLAMBERT = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bDIFFUSELIGHTING && m_bHALFLAMBERT;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 6 * m_nDIFFUSELIGHTING ) + ( 12 * m_nHALFLAMBERT ) + 0;
	}
};
#define shaderStaticTest_vertexlit_lighting_only_ps20 psh_forgot_to_set_static_DIFFUSELIGHTING + psh_forgot_to_set_static_HALFLAMBERT + 0
class vertexlit_lighting_only_ps20_Dynamic_Index
{
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
	int m_nNUM_LIGHTS;
#ifndef NDEBUG
	bool m_bNUM_LIGHTS;
#endif
public:
	void SetNUM_LIGHTS( int i )
	{
		Assert( i >= 0 && i <= 2 );
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
public:
	vertexlit_lighting_only_ps20_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bAMBIENT_LIGHT = false;
#endif  // !NDEBUG
		m_nAMBIENT_LIGHT = 0;
#ifndef NDEBUG
		m_bNUM_LIGHTS = false;
#endif  // !NDEBUG
		m_nNUM_LIGHTS = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bAMBIENT_LIGHT && m_bNUM_LIGHTS;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nAMBIENT_LIGHT ) + ( 2 * m_nNUM_LIGHTS ) + 0;
	}
};
#define shaderDynamicTest_vertexlit_lighting_only_ps20 psh_forgot_to_set_dynamic_AMBIENT_LIGHT + psh_forgot_to_set_dynamic_NUM_LIGHTS + 0
