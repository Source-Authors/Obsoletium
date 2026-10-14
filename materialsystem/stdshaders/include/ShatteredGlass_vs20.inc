#include "shaderlib/cshader.h"
class shatteredglass_vs20_Static_Index
{
private:
	int m_nENVMAP_MASK;
#ifndef NDEBUG
	bool m_bENVMAP_MASK;
#endif
public:
	void SetENVMAP_MASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nENVMAP_MASK = i;
#ifndef NDEBUG
		m_bENVMAP_MASK = true;
#endif
	}
	void SetENVMAP_MASK( bool i )
	{
		m_nENVMAP_MASK = i ? 1 : 0;
#ifndef NDEBUG
		m_bENVMAP_MASK = true;
#endif
	}
public:
	shatteredglass_vs20_Static_Index( )
	{
#ifndef NDEBUG
		m_bENVMAP_MASK = false;
#endif  // !NDEBUG
		m_nENVMAP_MASK = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bENVMAP_MASK;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 2 * m_nENVMAP_MASK ) + 0;
	}
};
#define shaderStaticTest_shatteredglass_vs20 vsh_forgot_to_set_static_ENVMAP_MASK + 0
class shatteredglass_vs20_Dynamic_Index
{
private:
	int m_nDOWATERFOG;
#ifndef NDEBUG
	bool m_bDOWATERFOG;
#endif
public:
	void SetDOWATERFOG( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDOWATERFOG = i;
#ifndef NDEBUG
		m_bDOWATERFOG = true;
#endif
	}
	void SetDOWATERFOG( bool i )
	{
		m_nDOWATERFOG = i ? 1 : 0;
#ifndef NDEBUG
		m_bDOWATERFOG = true;
#endif
	}
public:
	shatteredglass_vs20_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bDOWATERFOG = false;
#endif  // !NDEBUG
		m_nDOWATERFOG = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bDOWATERFOG;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nDOWATERFOG ) + 0;
	}
};
#define shaderDynamicTest_shatteredglass_vs20 vsh_forgot_to_set_dynamic_DOWATERFOG + 0
