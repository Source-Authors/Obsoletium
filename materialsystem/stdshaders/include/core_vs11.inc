#include "shaderlib/cshader.h"
class core_vs11_Static_Index
{
public:
	core_vs11_Static_Index( )
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
#define shaderStaticTest_core_vs11 0
class core_vs11_Dynamic_Index
{
private:
	int m_nSKINNING;
#ifndef NDEBUG
	bool m_bSKINNING;
#endif
public:
	void SetSKINNING( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSKINNING = i;
#ifndef NDEBUG
		m_bSKINNING = true;
#endif
	}
	void SetSKINNING( bool i )
	{
		m_nSKINNING = i ? 1 : 0;
#ifndef NDEBUG
		m_bSKINNING = true;
#endif
	}
public:
	core_vs11_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bSKINNING = false;
#endif  // !NDEBUG
		m_nSKINNING = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bSKINNING;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nSKINNING ) + 0;
	}
};
#define shaderDynamicTest_core_vs11 vsh_forgot_to_set_dynamic_SKINNING + 0
