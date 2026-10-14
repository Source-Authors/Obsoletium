#include "shaderlib/cshader.h"
class sprite_vs20_Static_Index
{
private:
	int m_nVERTEXCOLOR;
#ifndef NDEBUG
	bool m_bVERTEXCOLOR;
#endif
public:
	void SetVERTEXCOLOR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nVERTEXCOLOR = i;
#ifndef NDEBUG
		m_bVERTEXCOLOR = true;
#endif
	}
	void SetVERTEXCOLOR( bool i )
	{
		m_nVERTEXCOLOR = i ? 1 : 0;
#ifndef NDEBUG
		m_bVERTEXCOLOR = true;
#endif
	}
private:
	int m_nSRGB;
#ifndef NDEBUG
	bool m_bSRGB;
#endif
public:
	void SetSRGB( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSRGB = i;
#ifndef NDEBUG
		m_bSRGB = true;
#endif
	}
	void SetSRGB( bool i )
	{
		m_nSRGB = i ? 1 : 0;
#ifndef NDEBUG
		m_bSRGB = true;
#endif
	}
public:
	sprite_vs20_Static_Index( )
	{
#ifndef NDEBUG
		m_bVERTEXCOLOR = false;
#endif  // !NDEBUG
		m_nVERTEXCOLOR = 0;
#ifndef NDEBUG
		m_bSRGB = false;
#endif  // !NDEBUG
		m_nSRGB = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bVERTEXCOLOR && m_bSRGB;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 2 * m_nVERTEXCOLOR ) + ( 4 * m_nSRGB ) + 0;
	}
};
#define shaderStaticTest_sprite_vs20 vsh_forgot_to_set_static_VERTEXCOLOR + vsh_forgot_to_set_static_SRGB + 0
class sprite_vs20_Dynamic_Index
{
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
public:
	sprite_vs20_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bDOWATERFOG = false;
#endif  // !NDEBUG
		m_nDOWATERFOG = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bDOWATERFOG;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nDOWATERFOG ) + 0;
	}
};
#define shaderDynamicTest_sprite_vs20 vsh_forgot_to_set_dynamic_DOWATERFOG + 0
