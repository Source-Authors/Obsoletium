#include "shaderlib/cshader.h"
class depthwrite_vs20_Static_Index
{
private:
	int m_nONLY_PROJECT_POSITION;
#ifndef NDEBUG
	bool m_bONLY_PROJECT_POSITION;
#endif
public:
	void SetONLY_PROJECT_POSITION( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nONLY_PROJECT_POSITION = i;
#ifndef NDEBUG
		m_bONLY_PROJECT_POSITION = true;
#endif
	}
	void SetONLY_PROJECT_POSITION( bool i )
	{
		m_nONLY_PROJECT_POSITION = i ? 1 : 0;
#ifndef NDEBUG
		m_bONLY_PROJECT_POSITION = true;
#endif
	}
private:
	int m_nCOLOR_DEPTH;
#ifndef NDEBUG
	bool m_bCOLOR_DEPTH;
#endif
public:
	void SetCOLOR_DEPTH( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCOLOR_DEPTH = i;
#ifndef NDEBUG
		m_bCOLOR_DEPTH = true;
#endif
	}
	void SetCOLOR_DEPTH( bool i )
	{
		m_nCOLOR_DEPTH = i ? 1 : 0;
#ifndef NDEBUG
		m_bCOLOR_DEPTH = true;
#endif
	}
public:
	depthwrite_vs20_Static_Index( )
	{
#ifndef NDEBUG
		m_bONLY_PROJECT_POSITION = false;
#endif  // !NDEBUG
		m_nONLY_PROJECT_POSITION = 0;
#ifndef NDEBUG
		m_bCOLOR_DEPTH = false;
#endif  // !NDEBUG
		m_nCOLOR_DEPTH = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bONLY_PROJECT_POSITION && m_bCOLOR_DEPTH;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 4 * m_nONLY_PROJECT_POSITION ) + ( 4 * m_nCOLOR_DEPTH ) + 0;
	}
};
#define shaderStaticTest_depthwrite_vs20 vsh_forgot_to_set_static_ONLY_PROJECT_POSITION + vsh_forgot_to_set_static_COLOR_DEPTH + 0
class depthwrite_vs20_Dynamic_Index
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
public:
	depthwrite_vs20_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bCOMPRESSED_VERTS = false;
#endif  // !NDEBUG
		m_nCOMPRESSED_VERTS = 0;
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
		bool bAllDynamicVarsDefined = m_bCOMPRESSED_VERTS && m_bSKINNING;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nCOMPRESSED_VERTS ) + ( 2 * m_nSKINNING ) + 0;
	}
};
#define shaderDynamicTest_depthwrite_vs20 vsh_forgot_to_set_dynamic_COMPRESSED_VERTS + vsh_forgot_to_set_dynamic_SKINNING + 0
