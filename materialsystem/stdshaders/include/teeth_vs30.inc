#include "shaderlib/cshader.h"
class teeth_vs30_Static_Index
{
private:
	int m_nINTRO;
#ifndef NDEBUG
	bool m_bINTRO;
#endif
public:
	void SetINTRO( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nINTRO = i;
#ifndef NDEBUG
		m_bINTRO = true;
#endif
	}
	void SetINTRO( bool i )
	{
		m_nINTRO = i ? 1 : 0;
#ifndef NDEBUG
		m_bINTRO = true;
#endif
	}
public:
	teeth_vs30_Static_Index( )
	{
#ifndef NDEBUG
		m_bINTRO = false;
#endif  // !NDEBUG
		m_nINTRO = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bINTRO;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 64 * m_nINTRO ) + 0;
	}
};
#define shaderStaticTest_teeth_vs30 vsh_forgot_to_set_static_INTRO + 0
class teeth_vs30_Dynamic_Index
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
	int m_nDYNAMIC_LIGHT;
#ifndef NDEBUG
	bool m_bDYNAMIC_LIGHT;
#endif
public:
	void SetDYNAMIC_LIGHT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDYNAMIC_LIGHT = i;
#ifndef NDEBUG
		m_bDYNAMIC_LIGHT = true;
#endif
	}
	void SetDYNAMIC_LIGHT( bool i )
	{
		m_nDYNAMIC_LIGHT = i ? 1 : 0;
#ifndef NDEBUG
		m_bDYNAMIC_LIGHT = true;
#endif
	}
private:
	int m_nSTATIC_LIGHT;
#ifndef NDEBUG
	bool m_bSTATIC_LIGHT;
#endif
public:
	void SetSTATIC_LIGHT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSTATIC_LIGHT = i;
#ifndef NDEBUG
		m_bSTATIC_LIGHT = true;
#endif
	}
	void SetSTATIC_LIGHT( bool i )
	{
		m_nSTATIC_LIGHT = i ? 1 : 0;
#ifndef NDEBUG
		m_bSTATIC_LIGHT = true;
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
	teeth_vs30_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bCOMPRESSED_VERTS = false;
#endif  // !NDEBUG
		m_nCOMPRESSED_VERTS = 0;
#ifndef NDEBUG
		m_bDOWATERFOG = false;
#endif  // !NDEBUG
		m_nDOWATERFOG = 0;
#ifndef NDEBUG
		m_bSKINNING = false;
#endif  // !NDEBUG
		m_nSKINNING = 0;
#ifndef NDEBUG
		m_bDYNAMIC_LIGHT = false;
#endif  // !NDEBUG
		m_nDYNAMIC_LIGHT = 0;
#ifndef NDEBUG
		m_bSTATIC_LIGHT = false;
#endif  // !NDEBUG
		m_nSTATIC_LIGHT = 0;
#ifndef NDEBUG
		m_bMORPHING = false;
#endif  // !NDEBUG
		m_nMORPHING = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bCOMPRESSED_VERTS && m_bDOWATERFOG && m_bSKINNING && m_bDYNAMIC_LIGHT && m_bSTATIC_LIGHT && m_bMORPHING;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nCOMPRESSED_VERTS ) + ( 2 * m_nDOWATERFOG ) + ( 4 * m_nSKINNING ) + ( 8 * m_nDYNAMIC_LIGHT ) + ( 16 * m_nSTATIC_LIGHT ) + ( 32 * m_nMORPHING ) + 0;
	}
};
#define shaderDynamicTest_teeth_vs30 vsh_forgot_to_set_dynamic_COMPRESSED_VERTS + vsh_forgot_to_set_dynamic_DOWATERFOG + vsh_forgot_to_set_dynamic_SKINNING + vsh_forgot_to_set_dynamic_DYNAMIC_LIGHT + vsh_forgot_to_set_dynamic_STATIC_LIGHT + vsh_forgot_to_set_dynamic_MORPHING + 0
