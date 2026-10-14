#include "shaderlib/cshader.h"
class morphaccumulate_vs30_Static_Index
{
private:
	int m_nCONSTANTBASEDMORPH;
#ifndef NDEBUG
	bool m_bCONSTANTBASEDMORPH;
#endif
public:
	void SetCONSTANTBASEDMORPH( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCONSTANTBASEDMORPH = i;
#ifndef NDEBUG
		m_bCONSTANTBASEDMORPH = true;
#endif
	}
	void SetCONSTANTBASEDMORPH( bool i )
	{
		m_nCONSTANTBASEDMORPH = i ? 1 : 0;
#ifndef NDEBUG
		m_bCONSTANTBASEDMORPH = true;
#endif
	}
public:
	morphaccumulate_vs30_Static_Index( )
	{
#ifndef NDEBUG
		m_bCONSTANTBASEDMORPH = false;
#endif // _DEBUG
		m_nCONSTANTBASEDMORPH = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bCONSTANTBASEDMORPH;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nCONSTANTBASEDMORPH ) + 0;
	}
};
#define shaderStaticTest_morphaccumulate_vs30 vsh_forgot_to_set_static_CONSTANTBASEDMORPH + 0
class morphaccumulate_vs30_Dynamic_Index
{
public:
	morphaccumulate_vs30_Dynamic_Index()
	{
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
#endif // _DEBUG
		return 0;
	}
};
#define shaderDynamicTest_morphaccumulate_vs30 0
