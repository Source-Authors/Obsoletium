#include "shaderlib/cshader.h"
class portal_ps20_Static_Index
{
private:
	int m_nHASALPHAMASK;
#ifndef NDEBUG
	bool m_bHASALPHAMASK;
#endif
public:
	void SetHASALPHAMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nHASALPHAMASK = i;
#ifndef NDEBUG
		m_bHASALPHAMASK = true;
#endif
	}
	void SetHASALPHAMASK( bool i )
	{
		m_nHASALPHAMASK = i ? 1 : 0;
#ifndef NDEBUG
		m_bHASALPHAMASK = true;
#endif
	}
private:
	int m_nHASSTATICTEXTURE;
#ifndef NDEBUG
	bool m_bHASSTATICTEXTURE;
#endif
public:
	void SetHASSTATICTEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nHASSTATICTEXTURE = i;
#ifndef NDEBUG
		m_bHASSTATICTEXTURE = true;
#endif
	}
	void SetHASSTATICTEXTURE( bool i )
	{
		m_nHASSTATICTEXTURE = i ? 1 : 0;
#ifndef NDEBUG
		m_bHASSTATICTEXTURE = true;
#endif
	}
public:
	portal_ps20_Static_Index( )
	{
#ifndef NDEBUG
		m_bHASALPHAMASK = false;
#endif  // !NDEBUG
		m_nHASALPHAMASK = 0;
#ifndef NDEBUG
		m_bHASSTATICTEXTURE = false;
#endif  // !NDEBUG
		m_nHASSTATICTEXTURE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bHASALPHAMASK && m_bHASSTATICTEXTURE;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 8 * m_nHASALPHAMASK ) + ( 16 * m_nHASSTATICTEXTURE ) + 0;
	}
};
#define shaderStaticTest_portal_ps20 psh_forgot_to_set_static_HASALPHAMASK + psh_forgot_to_set_static_HASSTATICTEXTURE + 0
class portal_ps20_Dynamic_Index
{
private:
	int m_nADDSTATIC;
#ifndef NDEBUG
	bool m_bADDSTATIC;
#endif
public:
	void SetADDSTATIC( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nADDSTATIC = i;
#ifndef NDEBUG
		m_bADDSTATIC = true;
#endif
	}
	void SetADDSTATIC( bool i )
	{
		m_nADDSTATIC = i ? 1 : 0;
#ifndef NDEBUG
		m_bADDSTATIC = true;
#endif
	}
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
	portal_ps20_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bADDSTATIC = false;
#endif  // !NDEBUG
		m_nADDSTATIC = 0;
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
		bool bAllDynamicVarsDefined = m_bADDSTATIC && m_bHDRENABLED && m_bPIXELFOGTYPE;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nADDSTATIC ) + ( 2 * m_nHDRENABLED ) + ( 4 * m_nPIXELFOGTYPE ) + 0;
	}
};
#define shaderDynamicTest_portal_ps20 psh_forgot_to_set_dynamic_ADDSTATIC + psh_forgot_to_set_dynamic_HDRENABLED + psh_forgot_to_set_dynamic_PIXELFOGTYPE + 0
