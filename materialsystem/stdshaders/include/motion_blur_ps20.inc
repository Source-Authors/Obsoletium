#include "shaderlib/cshader.h"
class motion_blur_ps20_Static_Index
{
public:
	motion_blur_ps20_Static_Index( )
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
#define shaderStaticTest_motion_blur_ps20 0
class motion_blur_ps20_Dynamic_Index
{
private:
	int m_nQUALITY;
#ifndef NDEBUG
	bool m_bQUALITY;
#endif
public:
	void SetQUALITY( int i )
	{
		Assert( i >= 0 && i <= 3 );
		m_nQUALITY = i;
#ifndef NDEBUG
		m_bQUALITY = true;
#endif
	}
	void SetQUALITY( bool i )
	{
		m_nQUALITY = i ? 1 : 0;
#ifndef NDEBUG
		m_bQUALITY = true;
#endif
	}
public:
	motion_blur_ps20_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bQUALITY = false;
#endif  // !NDEBUG
		m_nQUALITY = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bQUALITY;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nQUALITY ) + 0;
	}
};
#define shaderDynamicTest_motion_blur_ps20 psh_forgot_to_set_dynamic_QUALITY + 0
