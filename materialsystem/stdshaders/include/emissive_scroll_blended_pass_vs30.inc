#include "shaderlib/cshader.h"
class emissive_scroll_blended_pass_vs30_Static_Index
{
public:
	emissive_scroll_blended_pass_vs30_Static_Index( )
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
#define shaderStaticTest_emissive_scroll_blended_pass_vs30 0
class emissive_scroll_blended_pass_vs30_Dynamic_Index
{
private:
	int m_nCOMPRESSED_VERTS;
#ifndef NDEBUG
	bool m_bCOMPRESSED_VERTS;
#endif
public:
	void SetCOMPRESSED_VERTS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCOMPRESSED_VERTS = i;
#ifndef NDEBUG
		m_bCOMPRESSED_VERTS = true;
#endif
	}
	void SetCOMPRESSED_VERTS( bool i )
	{
		m_nCOMPRESSED_VERTS = i ? 1 : 0;
#ifndef NDEBUG
		m_bCOMPRESSED_VERTS = true;
#endif
	}
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
private:
	int m_nMORPHING;
#ifndef NDEBUG
	bool m_bMORPHING;
#endif
public:
	void SetMORPHING( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMORPHING = i;
#ifndef NDEBUG
		m_bMORPHING = true;
#endif
	}
	void SetMORPHING( bool i )
	{
		m_nMORPHING = i ? 1 : 0;
#ifndef NDEBUG
		m_bMORPHING = true;
#endif
	}
public:
	emissive_scroll_blended_pass_vs30_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bCOMPRESSED_VERTS = false;
#endif // _DEBUG
		m_nCOMPRESSED_VERTS = 0;
#ifndef NDEBUG
		m_bSKINNING = false;
#endif // _DEBUG
		m_nSKINNING = 0;
#ifndef NDEBUG
		m_bMORPHING = false;
#endif // _DEBUG
		m_nMORPHING = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bCOMPRESSED_VERTS && m_bSKINNING && m_bMORPHING;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nCOMPRESSED_VERTS ) + ( 2 * m_nSKINNING ) + ( 4 * m_nMORPHING ) + 0;
	}
};
#define shaderDynamicTest_emissive_scroll_blended_pass_vs30 vsh_forgot_to_set_dynamic_COMPRESSED_VERTS + vsh_forgot_to_set_dynamic_SKINNING + vsh_forgot_to_set_dynamic_MORPHING + 0
