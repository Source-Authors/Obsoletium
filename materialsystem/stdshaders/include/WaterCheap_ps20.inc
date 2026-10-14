#include "shaderlib/cshader.h"
class watercheap_ps20_Static_Index
{
private:
	int m_nMULTITEXTURE;
#ifndef NDEBUG
	bool m_bMULTITEXTURE;
#endif
public:
	void SetMULTITEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMULTITEXTURE = i;
#ifndef NDEBUG
		m_bMULTITEXTURE = true;
#endif
	}
	void SetMULTITEXTURE( bool i )
	{
		m_nMULTITEXTURE = i ? 1 : 0;
#ifndef NDEBUG
		m_bMULTITEXTURE = true;
#endif
	}
private:
	int m_nFRESNEL;
#ifndef NDEBUG
	bool m_bFRESNEL;
#endif
public:
	void SetFRESNEL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFRESNEL = i;
#ifndef NDEBUG
		m_bFRESNEL = true;
#endif
	}
	void SetFRESNEL( bool i )
	{
		m_nFRESNEL = i ? 1 : 0;
#ifndef NDEBUG
		m_bFRESNEL = true;
#endif
	}
private:
	int m_nBLEND;
#ifndef NDEBUG
	bool m_bBLEND;
#endif
public:
	void SetBLEND( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBLEND = i;
#ifndef NDEBUG
		m_bBLEND = true;
#endif
	}
	void SetBLEND( bool i )
	{
		m_nBLEND = i ? 1 : 0;
#ifndef NDEBUG
		m_bBLEND = true;
#endif
	}
private:
	int m_nREFRACTALPHA;
#ifndef NDEBUG
	bool m_bREFRACTALPHA;
#endif
public:
	void SetREFRACTALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nREFRACTALPHA = i;
#ifndef NDEBUG
		m_bREFRACTALPHA = true;
#endif
	}
	void SetREFRACTALPHA( bool i )
	{
		m_nREFRACTALPHA = i ? 1 : 0;
#ifndef NDEBUG
		m_bREFRACTALPHA = true;
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
	int m_nNORMAL_DECODE_MODE;
#ifndef NDEBUG
	bool m_bNORMAL_DECODE_MODE;
#endif
public:
	void SetNORMAL_DECODE_MODE( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nNORMAL_DECODE_MODE = i;
#ifndef NDEBUG
		m_bNORMAL_DECODE_MODE = true;
#endif
	}
	void SetNORMAL_DECODE_MODE( bool i )
	{
		m_nNORMAL_DECODE_MODE = i ? 1 : 0;
#ifndef NDEBUG
		m_bNORMAL_DECODE_MODE = true;
#endif
	}
public:
	watercheap_ps20_Static_Index( )
	{
#ifndef NDEBUG
		m_bMULTITEXTURE = false;
#endif // _DEBUG
		m_nMULTITEXTURE = 0;
#ifndef NDEBUG
		m_bFRESNEL = false;
#endif // _DEBUG
		m_nFRESNEL = 0;
#ifndef NDEBUG
		m_bBLEND = false;
#endif // _DEBUG
		m_nBLEND = 0;
#ifndef NDEBUG
		m_bREFRACTALPHA = false;
#endif // _DEBUG
		m_nREFRACTALPHA = 0;
#ifndef NDEBUG
		m_bHDRTYPE = false;
#endif // _DEBUG
		m_nHDRTYPE = 0;
#ifndef NDEBUG
		m_bNORMAL_DECODE_MODE = false;
#endif // _DEBUG
		m_nNORMAL_DECODE_MODE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bMULTITEXTURE && m_bFRESNEL && m_bBLEND && m_bREFRACTALPHA && m_bHDRTYPE && m_bNORMAL_DECODE_MODE;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 4 * m_nMULTITEXTURE ) + ( 8 * m_nFRESNEL ) + ( 16 * m_nBLEND ) + ( 32 * m_nREFRACTALPHA ) + ( 64 * m_nHDRTYPE ) + ( 192 * m_nNORMAL_DECODE_MODE ) + 0;
	}
};
#define shaderStaticTest_watercheap_ps20 psh_forgot_to_set_static_MULTITEXTURE + psh_forgot_to_set_static_FRESNEL + psh_forgot_to_set_static_BLEND + psh_forgot_to_set_static_REFRACTALPHA + psh_forgot_to_set_static_HDRTYPE + psh_forgot_to_set_static_NORMAL_DECODE_MODE + 0
class watercheap_ps20_Dynamic_Index
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
	watercheap_ps20_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bHDRENABLED = false;
#endif // _DEBUG
		m_nHDRENABLED = 0;
#ifndef NDEBUG
		m_bPIXELFOGTYPE = false;
#endif // _DEBUG
		m_nPIXELFOGTYPE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bHDRENABLED && m_bPIXELFOGTYPE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nHDRENABLED ) + ( 2 * m_nPIXELFOGTYPE ) + 0;
	}
};
#define shaderDynamicTest_watercheap_ps20 psh_forgot_to_set_dynamic_HDRENABLED + psh_forgot_to_set_dynamic_PIXELFOGTYPE + 0
