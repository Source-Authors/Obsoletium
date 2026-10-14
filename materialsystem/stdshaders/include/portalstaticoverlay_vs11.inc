#include "shaderlib/cshader.h"
class portalstaticoverlay_vs11_Static_Index
{
private:
	int m_nMODEL;
#ifndef NDEBUG
	bool m_bMODEL;
#endif
public:
	void SetMODEL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMODEL = i;
#ifndef NDEBUG
		m_bMODEL = true;
#endif
	}
	void SetMODEL( bool i )
	{
		m_nMODEL = i ? 1 : 0;
#ifndef NDEBUG
		m_bMODEL = true;
#endif
	}
public:
	portalstaticoverlay_vs11_Static_Index( )
	{
#ifndef NDEBUG
		m_bMODEL = false;
#endif  // !NDEBUG
		m_nMODEL = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bMODEL;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 2 * m_nMODEL ) + 0;
	}
};
#define shaderStaticTest_portalstaticoverlay_vs11 vsh_forgot_to_set_static_MODEL + 0
class portalstaticoverlay_vs11_Dynamic_Index
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
	portalstaticoverlay_vs11_Dynamic_Index()
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
#define shaderDynamicTest_portalstaticoverlay_vs11 vsh_forgot_to_set_dynamic_SKINNING + 0
