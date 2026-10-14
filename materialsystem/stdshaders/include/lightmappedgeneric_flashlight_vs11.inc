#include "shaderlib/cshader.h"
class lightmappedgeneric_flashlight_vs11_Static_Index
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
	int m_nWORLDVERTEXTRANSITION;
#ifndef NDEBUG
	bool m_bWORLDVERTEXTRANSITION;
#endif
public:
	void SetWORLDVERTEXTRANSITION( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nWORLDVERTEXTRANSITION = i;
#ifndef NDEBUG
		m_bWORLDVERTEXTRANSITION = true;
#endif
	}
	void SetWORLDVERTEXTRANSITION( bool i )
	{
		m_nWORLDVERTEXTRANSITION = i ? 1 : 0;
#ifndef NDEBUG
		m_bWORLDVERTEXTRANSITION = true;
#endif
	}
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
public:
	lightmappedgeneric_flashlight_vs11_Static_Index( )
	{
#ifndef NDEBUG
		m_bNORMALMAP = false;
#endif // _DEBUG
		m_nNORMALMAP = 0;
#ifndef NDEBUG
		m_bWORLDVERTEXTRANSITION = false;
#endif // _DEBUG
		m_nWORLDVERTEXTRANSITION = 0;
#ifndef NDEBUG
		m_bVERTEXCOLOR = false;
#endif // _DEBUG
		m_nVERTEXCOLOR = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bNORMALMAP && m_bWORLDVERTEXTRANSITION && m_bVERTEXCOLOR;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 2 * m_nNORMALMAP ) + ( 4 * m_nWORLDVERTEXTRANSITION ) + ( 8 * m_nVERTEXCOLOR ) + 0;
	}
};
#define shaderStaticTest_lightmappedgeneric_flashlight_vs11 vsh_forgot_to_set_static_NORMALMAP + vsh_forgot_to_set_static_WORLDVERTEXTRANSITION + vsh_forgot_to_set_static_VERTEXCOLOR + 0
class lightmappedgeneric_flashlight_vs11_Dynamic_Index
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
	lightmappedgeneric_flashlight_vs11_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bDOWATERFOG = false;
#endif // _DEBUG
		m_nDOWATERFOG = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bDOWATERFOG;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nDOWATERFOG ) + 0;
	}
};
#define shaderDynamicTest_lightmappedgeneric_flashlight_vs11 vsh_forgot_to_set_dynamic_DOWATERFOG + 0
