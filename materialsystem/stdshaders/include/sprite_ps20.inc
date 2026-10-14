#include "shaderlib/cshader.h"
class sprite_ps20_Static_Index
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
	int m_nCONSTANTCOLOR;
#ifndef NDEBUG
	bool m_bCONSTANTCOLOR;
#endif
public:
	void SetCONSTANTCOLOR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCONSTANTCOLOR = i;
#ifndef NDEBUG
		m_bCONSTANTCOLOR = true;
#endif
	}
	void SetCONSTANTCOLOR( bool i )
	{
		m_nCONSTANTCOLOR = i ? 1 : 0;
#ifndef NDEBUG
		m_bCONSTANTCOLOR = true;
#endif
	}
private:
	int m_nHDRTYPE;
#ifndef NDEBUG
	bool m_bHDRTYPE;
#endif
public:
	void SetHDRTYPE( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nHDRTYPE = i;
#ifndef NDEBUG
		m_bHDRTYPE = true;
#endif
	}
	void SetHDRTYPE( bool i )
	{
		m_nHDRTYPE = i ? 1 : 0;
#ifndef NDEBUG
		m_bHDRTYPE = true;
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
	sprite_ps20_Static_Index( )
	{
#ifndef NDEBUG
		m_bVERTEXCOLOR = false;
#endif  // !NDEBUG
		m_nVERTEXCOLOR = 0;
#ifndef NDEBUG
		m_bCONSTANTCOLOR = false;
#endif  // !NDEBUG
		m_nCONSTANTCOLOR = 0;
#ifndef NDEBUG
		m_bHDRTYPE = false;
#endif  // !NDEBUG
		m_nHDRTYPE = 0;
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
		bool bAllStaticVarsDefined = m_bVERTEXCOLOR && m_bCONSTANTCOLOR && m_bHDRTYPE && m_bSRGB;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 4 * m_nVERTEXCOLOR ) + ( 8 * m_nCONSTANTCOLOR ) + ( 16 * m_nHDRTYPE ) + ( 48 * m_nSRGB ) + 0;
	}
};
#define shaderStaticTest_sprite_ps20 psh_forgot_to_set_static_VERTEXCOLOR + psh_forgot_to_set_static_CONSTANTCOLOR + psh_forgot_to_set_static_HDRTYPE + psh_forgot_to_set_static_SRGB + 0
class sprite_ps20_Dynamic_Index
{
private:
	int m_nHDRENABLED;
#ifndef NDEBUG
	bool m_bHDRENABLED;
#endif
public:
	void SetHDRENABLED( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nHDRENABLED = i;
#ifndef NDEBUG
		m_bHDRENABLED = true;
#endif
	}
	void SetHDRENABLED( bool i )
	{
		m_nHDRENABLED = i ? 1 : 0;
#ifndef NDEBUG
		m_bHDRENABLED = true;
#endif
	}
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
public:
	sprite_ps20_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bHDRENABLED = false;
#endif  // !NDEBUG
		m_nHDRENABLED = 0;
#ifndef NDEBUG
		m_bPIXELFOGTYPE = false;
#endif  // !NDEBUG
		m_nPIXELFOGTYPE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bHDRENABLED && m_bPIXELFOGTYPE;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nHDRENABLED ) + ( 2 * m_nPIXELFOGTYPE ) + 0;
	}
};
#define shaderDynamicTest_sprite_ps20 psh_forgot_to_set_dynamic_HDRENABLED + psh_forgot_to_set_dynamic_PIXELFOGTYPE + 0
