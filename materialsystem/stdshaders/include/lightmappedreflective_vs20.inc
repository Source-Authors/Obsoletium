#include "shaderlib/cshader.h"
class lightmappedreflective_vs20_Static_Index
{
private:
	int m_nBASETEXTURE;
#ifndef NDEBUG
	bool m_bBASETEXTURE;
#endif
public:
	void SetBASETEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBASETEXTURE = i;
#ifndef NDEBUG
		m_bBASETEXTURE = true;
#endif
	}
	void SetBASETEXTURE( bool i )
	{
		m_nBASETEXTURE = i ? 1 : 0;
#ifndef NDEBUG
		m_bBASETEXTURE = true;
#endif
	}
public:
	lightmappedreflective_vs20_Static_Index( )
	{
#ifndef NDEBUG
		m_bBASETEXTURE = false;
#endif  // !NDEBUG
		m_nBASETEXTURE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bBASETEXTURE;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nBASETEXTURE ) + 0;
	}
};
#define shaderStaticTest_lightmappedreflective_vs20 vsh_forgot_to_set_static_BASETEXTURE + 0
class lightmappedreflective_vs20_Dynamic_Index
{
public:
	lightmappedreflective_vs20_Dynamic_Index()
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
#define shaderDynamicTest_lightmappedreflective_vs20 0
