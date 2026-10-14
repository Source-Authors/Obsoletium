#include "shaderlib/cshader.h"
class treeleaf_vs20_Static_Index
{
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
private:
	int m_nUSE_STATIC_CONTROL_FLOW;
#ifndef NDEBUG
	bool m_bUSE_STATIC_CONTROL_FLOW;
#endif
public:
	void SetUSE_STATIC_CONTROL_FLOW( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nUSE_STATIC_CONTROL_FLOW = i;
#ifndef NDEBUG
		m_bUSE_STATIC_CONTROL_FLOW = true;
#endif
	}
	void SetUSE_STATIC_CONTROL_FLOW( bool i )
	{
		m_nUSE_STATIC_CONTROL_FLOW = i ? 1 : 0;
#ifndef NDEBUG
		m_bUSE_STATIC_CONTROL_FLOW = true;
#endif
	}
public:
	treeleaf_vs20_Static_Index( )
	{
#ifndef NDEBUG
		m_bHALFLAMBERT = false;
#endif  // !NDEBUG
		m_nHALFLAMBERT = 0;
#ifndef NDEBUG
		m_bUSE_STATIC_CONTROL_FLOW = false;
#endif  // !NDEBUG
		m_nUSE_STATIC_CONTROL_FLOW = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bHALFLAMBERT && m_bUSE_STATIC_CONTROL_FLOW;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 12 * m_nHALFLAMBERT ) + ( 24 * m_nUSE_STATIC_CONTROL_FLOW ) + 0;
	}
};
#define shaderStaticTest_treeleaf_vs20 vsh_forgot_to_set_static_HALFLAMBERT + vsh_forgot_to_set_static_USE_STATIC_CONTROL_FLOW + 0
class treeleaf_vs20_Dynamic_Index
{
private:
	int m_nDYNAMIC_LIGHT;
#ifndef NDEBUG
	bool m_bDYNAMIC_LIGHT;
#endif
public:
	void SetDYNAMIC_LIGHT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDYNAMIC_LIGHT = i;
#ifndef NDEBUG
		m_bDYNAMIC_LIGHT = true;
#endif
	}
	void SetDYNAMIC_LIGHT( bool i )
	{
		m_nDYNAMIC_LIGHT = i ? 1 : 0;
#ifndef NDEBUG
		m_bDYNAMIC_LIGHT = true;
#endif
	}
private:
	int m_nSTATIC_LIGHT;
#ifndef NDEBUG
	bool m_bSTATIC_LIGHT;
#endif
public:
	void SetSTATIC_LIGHT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSTATIC_LIGHT = i;
#ifndef NDEBUG
		m_bSTATIC_LIGHT = true;
#endif
	}
	void SetSTATIC_LIGHT( bool i )
	{
		m_nSTATIC_LIGHT = i ? 1 : 0;
#ifndef NDEBUG
		m_bSTATIC_LIGHT = true;
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
	treeleaf_vs20_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bDYNAMIC_LIGHT = false;
#endif  // !NDEBUG
		m_nDYNAMIC_LIGHT = 0;
#ifndef NDEBUG
		m_bSTATIC_LIGHT = false;
#endif  // !NDEBUG
		m_nSTATIC_LIGHT = 0;
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
		bool bAllDynamicVarsDefined = m_bDYNAMIC_LIGHT && m_bSTATIC_LIGHT && m_bNUM_LIGHTS;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nDYNAMIC_LIGHT ) + ( 2 * m_nSTATIC_LIGHT ) + ( 4 * m_nNUM_LIGHTS ) + 0;
	}
};
#define shaderDynamicTest_treeleaf_vs20 vsh_forgot_to_set_dynamic_DYNAMIC_LIGHT + vsh_forgot_to_set_dynamic_STATIC_LIGHT + vsh_forgot_to_set_dynamic_NUM_LIGHTS + 0
