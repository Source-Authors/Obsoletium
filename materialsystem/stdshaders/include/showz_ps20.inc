#include "shaderlib/cshader.h"
class showz_ps20_Static_Index
{
private:
	int m_nDEPTH_IN_ALPHA;
#ifndef NDEBUG
	bool m_bDEPTH_IN_ALPHA;
#endif
public:
	void SetDEPTH_IN_ALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDEPTH_IN_ALPHA = i;
#ifndef NDEBUG
		m_bDEPTH_IN_ALPHA = true;
#endif
	}
	void SetDEPTH_IN_ALPHA( bool i )
	{
		m_nDEPTH_IN_ALPHA = i ? 1 : 0;
#ifndef NDEBUG
		m_bDEPTH_IN_ALPHA = true;
#endif
	}
public:
	showz_ps20_Static_Index( )
	{
#ifndef NDEBUG
		m_bDEPTH_IN_ALPHA = false;
#endif  // !NDEBUG
		m_nDEPTH_IN_ALPHA = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bDEPTH_IN_ALPHA;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nDEPTH_IN_ALPHA ) + 0;
	}
};
#define shaderStaticTest_showz_ps20 psh_forgot_to_set_static_DEPTH_IN_ALPHA + 0
class showz_ps20_Dynamic_Index
{
public:
	showz_ps20_Dynamic_Index()
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
#define shaderDynamicTest_showz_ps20 0
