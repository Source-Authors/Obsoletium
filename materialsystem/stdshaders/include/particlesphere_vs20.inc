#include "shaderlib/cshader.h"
class particlesphere_vs20_Static_Index
{
public:
	particlesphere_vs20_Static_Index( )
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
#define shaderStaticTest_particlesphere_vs20 0
class particlesphere_vs20_Dynamic_Index
{
private:
	int m_nFOGTYPE;
#ifndef NDEBUG
	bool m_bFOGTYPE;
#endif
public:
	void SetFOGTYPE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFOGTYPE = i;
#ifndef NDEBUG
		m_bFOGTYPE = true;
#endif
	}
	void SetFOGTYPE( bool i )
	{
		m_nFOGTYPE = i ? 1 : 0;
#ifndef NDEBUG
		m_bFOGTYPE = true;
#endif
	}
public:
	particlesphere_vs20_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bFOGTYPE = false;
#endif  // !NDEBUG
		m_nFOGTYPE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bFOGTYPE;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nFOGTYPE ) + 0;
	}
};
#define shaderDynamicTest_particlesphere_vs20 vsh_forgot_to_set_dynamic_FOGTYPE + 0
