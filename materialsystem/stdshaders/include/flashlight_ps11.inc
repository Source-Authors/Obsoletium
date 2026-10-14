#include "shaderlib/cshader.h"
class flashlight_ps11_Static_Index
{
private:
	int m_nNORMALMAP;
#ifndef NDEBUG
	bool m_bNORMALMAP;
#endif
public:
	void SetNORMALMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nNORMALMAP = i;
#ifndef NDEBUG
		m_bNORMALMAP = true;
#endif
	}
	void SetNORMALMAP( bool i )
	{
		m_nNORMALMAP = i ? 1 : 0;
#ifndef NDEBUG
		m_bNORMALMAP = true;
#endif
	}
private:
	int m_nNOCULL;
#ifndef NDEBUG
	bool m_bNOCULL;
#endif
public:
	void SetNOCULL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nNOCULL = i;
#ifndef NDEBUG
		m_bNOCULL = true;
#endif
	}
	void SetNOCULL( bool i )
	{
		m_nNOCULL = i ? 1 : 0;
#ifndef NDEBUG
		m_bNOCULL = true;
#endif
	}
public:
	flashlight_ps11_Static_Index( )
	{
#ifndef NDEBUG
		m_bNORMALMAP = false;
#endif  // !NDEBUG
		m_nNORMALMAP = 0;
#ifndef NDEBUG
		m_bNOCULL = false;
#endif  // !NDEBUG
		m_nNOCULL = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bNORMALMAP && m_bNOCULL;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nNORMALMAP ) + ( 2 * m_nNOCULL ) + 0;
	}
};
#define shaderStaticTest_flashlight_ps11 psh_forgot_to_set_static_NORMALMAP + psh_forgot_to_set_static_NOCULL + 0
class flashlight_ps11_Dynamic_Index
{
public:
	flashlight_ps11_Dynamic_Index()
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
#define shaderDynamicTest_flashlight_ps11 0
