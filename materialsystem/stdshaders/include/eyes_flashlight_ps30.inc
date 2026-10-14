#include "shaderlib/cshader.h"
class eyes_flashlight_ps30_Static_Index
{
private:
	int m_nFLASHLIGHTDEPTHFILTERMODE;
#ifndef NDEBUG
	bool m_bFLASHLIGHTDEPTHFILTERMODE;
#endif
public:
	void SetFLASHLIGHTDEPTHFILTERMODE( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nFLASHLIGHTDEPTHFILTERMODE = i;
#ifndef NDEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = true;
#endif
	}
	void SetFLASHLIGHTDEPTHFILTERMODE( bool i )
	{
		m_nFLASHLIGHTDEPTHFILTERMODE = i ? 1 : 0;
#ifndef NDEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = true;
#endif
	}
public:
	eyes_flashlight_ps30_Static_Index( )
	{
#ifndef NDEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = false;
#endif  // !NDEBUG
		m_nFLASHLIGHTDEPTHFILTERMODE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bFLASHLIGHTDEPTHFILTERMODE;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 4 * m_nFLASHLIGHTDEPTHFILTERMODE ) + 0;
	}
};
#define shaderStaticTest_eyes_flashlight_ps30 psh_forgot_to_set_static_FLASHLIGHTDEPTHFILTERMODE + 0
class eyes_flashlight_ps30_Dynamic_Index
{
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
private:
	int m_nFLASHLIGHTSHADOWS;
#ifndef NDEBUG
	bool m_bFLASHLIGHTSHADOWS;
#endif
public:
	void SetFLASHLIGHTSHADOWS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFLASHLIGHTSHADOWS = i;
#ifndef NDEBUG
		m_bFLASHLIGHTSHADOWS = true;
#endif
	}
	void SetFLASHLIGHTSHADOWS( bool i )
	{
		m_nFLASHLIGHTSHADOWS = i ? 1 : 0;
#ifndef NDEBUG
		m_bFLASHLIGHTSHADOWS = true;
#endif
	}
public:
	eyes_flashlight_ps30_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bPIXELFOGTYPE = false;
#endif  // !NDEBUG
		m_nPIXELFOGTYPE = 0;
#ifndef NDEBUG
		m_bFLASHLIGHTSHADOWS = false;
#endif  // !NDEBUG
		m_nFLASHLIGHTSHADOWS = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bPIXELFOGTYPE && m_bFLASHLIGHTSHADOWS;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nPIXELFOGTYPE ) + ( 2 * m_nFLASHLIGHTSHADOWS ) + 0;
	}
};
#define shaderDynamicTest_eyes_flashlight_ps30 psh_forgot_to_set_dynamic_PIXELFOGTYPE + psh_forgot_to_set_dynamic_FLASHLIGHTSHADOWS + 0
