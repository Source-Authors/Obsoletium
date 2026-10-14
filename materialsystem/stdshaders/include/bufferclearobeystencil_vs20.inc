#include "shaderlib/cshader.h"
class bufferclearobeystencil_vs20_Static_Index
{
private:
	int m_nUSESCOLOR;
#ifndef NDEBUG
	bool m_bUSESCOLOR;
#endif
public:
	void SetUSESCOLOR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nUSESCOLOR = i;
#ifndef NDEBUG
		m_bUSESCOLOR = true;
#endif
	}
	void SetUSESCOLOR( bool i )
	{
		m_nUSESCOLOR = i ? 1 : 0;
#ifndef NDEBUG
		m_bUSESCOLOR = true;
#endif
	}
public:
	bufferclearobeystencil_vs20_Static_Index( )
	{
#ifndef NDEBUG
		m_bUSESCOLOR = false;
#endif  // !NDEBUG
		m_nUSESCOLOR = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bUSESCOLOR;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nUSESCOLOR ) + 0;
	}
};
#define shaderStaticTest_bufferclearobeystencil_vs20 vsh_forgot_to_set_static_USESCOLOR + 0
class bufferclearobeystencil_vs20_Dynamic_Index
{
public:
	bufferclearobeystencil_vs20_Dynamic_Index()
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
#define shaderDynamicTest_bufferclearobeystencil_vs20 0
