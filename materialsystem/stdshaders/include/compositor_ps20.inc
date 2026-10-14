#include "shaderlib/cshader.h"
class compositor_ps20_Static_Index
{
private:
	int m_nCOMBINE_MODE;
#ifndef NDEBUG
	bool m_bCOMBINE_MODE;
#endif
public:
	void SetCOMBINE_MODE( int i )
	{
		Assert( i >= 0 && i <= 6 );
		m_nCOMBINE_MODE = i;
#ifndef NDEBUG
		m_bCOMBINE_MODE = true;
#endif
	}
	void SetCOMBINE_MODE( bool i )
	{
		m_nCOMBINE_MODE = i ? 1 : 0;
#ifndef NDEBUG
		m_bCOMBINE_MODE = true;
#endif
	}
public:
	compositor_ps20_Static_Index( )
	{
#ifndef NDEBUG
		m_bCOMBINE_MODE = false;
#endif // _DEBUG
		m_nCOMBINE_MODE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bCOMBINE_MODE;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nCOMBINE_MODE ) + 0;
	}
};
#define shaderStaticTest_compositor_ps20 psh_forgot_to_set_static_COMBINE_MODE + 0
class compositor_ps20_Dynamic_Index
{
private:
	int m_nDEBUG_MODE;
#ifndef NDEBUG
	bool m_bDEBUG_MODE;
#endif
public:
	void SetDEBUG_MODE( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nDEBUG_MODE = i;
#ifndef NDEBUG
		m_bDEBUG_MODE = true;
#endif
	}
	void SetDEBUG_MODE( bool i )
	{
		m_nDEBUG_MODE = i ? 1 : 0;
#ifndef NDEBUG
		m_bDEBUG_MODE = true;
#endif
	}
public:
	compositor_ps20_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bDEBUG_MODE = false;
#endif // _DEBUG
		m_nDEBUG_MODE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bDEBUG_MODE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nDEBUG_MODE ) + 0;
	}
};
#define shaderDynamicTest_compositor_ps20 psh_forgot_to_set_dynamic_DEBUG_MODE + 0
