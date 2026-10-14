#include "shaderlib/cshader.h"
class depthwrite_ps20_Static_Index
{
private:
	int m_nCOLOR_DEPTH;
#ifndef NDEBUG
	bool m_bCOLOR_DEPTH;
#endif
public:
	void SetCOLOR_DEPTH( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCOLOR_DEPTH = i;
#ifndef NDEBUG
		m_bCOLOR_DEPTH = true;
#endif
	}
	void SetCOLOR_DEPTH( bool i )
	{
		m_nCOLOR_DEPTH = i ? 1 : 0;
#ifndef NDEBUG
		m_bCOLOR_DEPTH = true;
#endif
	}
public:
	depthwrite_ps20_Static_Index( )
	{
#ifndef NDEBUG
		m_bCOLOR_DEPTH = false;
#endif  // !NDEBUG
		m_nCOLOR_DEPTH = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bCOLOR_DEPTH;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 2 * m_nCOLOR_DEPTH ) + 0;
	}
};
#define shaderStaticTest_depthwrite_ps20 psh_forgot_to_set_static_COLOR_DEPTH + 0
class depthwrite_ps20_Dynamic_Index
{
private:
	int m_nALPHACLIP;
#ifndef NDEBUG
	bool m_bALPHACLIP;
#endif
public:
	void SetALPHACLIP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nALPHACLIP = i;
#ifndef NDEBUG
		m_bALPHACLIP = true;
#endif
	}
	void SetALPHACLIP( bool i )
	{
		m_nALPHACLIP = i ? 1 : 0;
#ifndef NDEBUG
		m_bALPHACLIP = true;
#endif
	}
public:
	depthwrite_ps20_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bALPHACLIP = false;
#endif  // !NDEBUG
		m_nALPHACLIP = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bALPHACLIP;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nALPHACLIP ) + 0;
	}
};
#define shaderDynamicTest_depthwrite_ps20 psh_forgot_to_set_dynamic_ALPHACLIP + 0
