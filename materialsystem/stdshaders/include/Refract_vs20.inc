#include "shaderlib/cshader.h"
class refract_vs20_Static_Index
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
private:
	int m_nCOLORMODULATE;
#ifndef NDEBUG
	bool m_bCOLORMODULATE;
#endif
public:
	void SetCOLORMODULATE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCOLORMODULATE = i;
#ifndef NDEBUG
		m_bCOLORMODULATE = true;
#endif
	}
	void SetCOLORMODULATE( bool i )
	{
		m_nCOLORMODULATE = i ? 1 : 0;
#ifndef NDEBUG
		m_bCOLORMODULATE = true;
#endif
	}
public:
	refract_vs20_Static_Index( )
	{
#ifndef NDEBUG
		m_bMODEL = false;
#endif  // !NDEBUG
		m_nMODEL = 0;
#ifndef NDEBUG
		m_bCOLORMODULATE = false;
#endif  // !NDEBUG
		m_nCOLORMODULATE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bMODEL && m_bCOLORMODULATE;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 4 * m_nMODEL ) + ( 8 * m_nCOLORMODULATE ) + 0;
	}
};
#define shaderStaticTest_refract_vs20 vsh_forgot_to_set_static_MODEL + vsh_forgot_to_set_static_COLORMODULATE + 0
class refract_vs20_Dynamic_Index
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
	refract_vs20_Dynamic_Index()
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
#define shaderDynamicTest_refract_vs20 vsh_forgot_to_set_dynamic_COMPRESSED_VERTS + vsh_forgot_to_set_dynamic_SKINNING + 0
