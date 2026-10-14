#include "shaderlib/cshader.h"
class unlittwotexture_ps20_Static_Index
{
public:
	unlittwotexture_ps20_Static_Index( )
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
#define shaderStaticTest_unlittwotexture_ps20 0
class unlittwotexture_ps20_Dynamic_Index
{
private:
	int m_nPIXELFOGTYPE;
#ifndef NDEBUG
	bool m_bPIXELFOGTYPE;
#endif
public:
	void SetPIXELFOGTYPE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPIXELFOGTYPE = i;
#ifndef NDEBUG
		m_bPIXELFOGTYPE = true;
#endif
	}
	void SetPIXELFOGTYPE( bool i )
	{
		m_nPIXELFOGTYPE = i ? 1 : 0;
#ifndef NDEBUG
		m_bPIXELFOGTYPE = true;
#endif
	}
private:
	int m_nLIGHTING_PREVIEW;
#ifndef NDEBUG
	bool m_bLIGHTING_PREVIEW;
#endif
public:
	void SetLIGHTING_PREVIEW( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nLIGHTING_PREVIEW = i;
#ifndef NDEBUG
		m_bLIGHTING_PREVIEW = true;
#endif
	}
	void SetLIGHTING_PREVIEW( bool i )
	{
		m_nLIGHTING_PREVIEW = i ? 1 : 0;
#ifndef NDEBUG
		m_bLIGHTING_PREVIEW = true;
#endif
	}
public:
	unlittwotexture_ps20_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bPIXELFOGTYPE = false;
#endif  // !NDEBUG
		m_nPIXELFOGTYPE = 0;
#ifndef NDEBUG
		m_bLIGHTING_PREVIEW = false;
#endif  // !NDEBUG
		m_nLIGHTING_PREVIEW = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bPIXELFOGTYPE && m_bLIGHTING_PREVIEW;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nPIXELFOGTYPE ) + ( 2 * m_nLIGHTING_PREVIEW ) + 0;
	}
};
#define shaderDynamicTest_unlittwotexture_ps20 psh_forgot_to_set_dynamic_PIXELFOGTYPE + psh_forgot_to_set_dynamic_LIGHTING_PREVIEW + 0
