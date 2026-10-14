#include "shaderlib/cshader.h"
class introscreenspaceeffect_ps11_Static_Index
{
public:
	introscreenspaceeffect_ps11_Static_Index( )
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
#define shaderStaticTest_introscreenspaceeffect_ps11 0
class introscreenspaceeffect_ps11_Dynamic_Index
{
private:
	int m_nMODE;
#ifndef NDEBUG
	bool m_bMODE;
#endif
public:
	void SetMODE( int i )
	{
		Assert( i >= 0 && i <= 9 );
		m_nMODE = i;
#ifndef NDEBUG
		m_bMODE = true;
#endif
	}
	void SetMODE( bool i )
	{
		m_nMODE = i ? 1 : 0;
#ifndef NDEBUG
		m_bMODE = true;
#endif
	}
public:
	introscreenspaceeffect_ps11_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bMODE = false;
#endif // _DEBUG
		m_nMODE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bMODE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nMODE ) + 0;
	}
};
#define shaderDynamicTest_introscreenspaceeffect_ps11 psh_forgot_to_set_dynamic_MODE + 0
