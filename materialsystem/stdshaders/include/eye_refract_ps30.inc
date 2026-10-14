#include "shaderlib/cshader.h"
class eye_refract_ps30_Static_Index
{
private:
	int m_nFLASHLIGHT;
#ifndef NDEBUG
	bool m_bFLASHLIGHT;
#endif
public:
	void SetFLASHLIGHT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFLASHLIGHT = i;
#ifndef NDEBUG
		m_bFLASHLIGHT = true;
#endif
	}
	void SetFLASHLIGHT( bool i )
	{
		m_nFLASHLIGHT = i ? 1 : 0;
#ifndef NDEBUG
		m_bFLASHLIGHT = true;
#endif
	}
private:
	int m_nLIGHTWARPTEXTURE;
#ifndef NDEBUG
	bool m_bLIGHTWARPTEXTURE;
#endif
public:
	void SetLIGHTWARPTEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nLIGHTWARPTEXTURE = i;
#ifndef NDEBUG
		m_bLIGHTWARPTEXTURE = true;
#endif
	}
	void SetLIGHTWARPTEXTURE( bool i )
	{
		m_nLIGHTWARPTEXTURE = i ? 1 : 0;
#ifndef NDEBUG
		m_bLIGHTWARPTEXTURE = true;
#endif
	}
private:
	int m_nSPHERETEXKILLCOMBO;
#ifndef NDEBUG
	bool m_bSPHERETEXKILLCOMBO;
#endif
public:
	void SetSPHERETEXKILLCOMBO( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSPHERETEXKILLCOMBO = i;
#ifndef NDEBUG
		m_bSPHERETEXKILLCOMBO = true;
#endif
	}
	void SetSPHERETEXKILLCOMBO( bool i )
	{
		m_nSPHERETEXKILLCOMBO = i ? 1 : 0;
#ifndef NDEBUG
		m_bSPHERETEXKILLCOMBO = true;
#endif
	}
private:
	int m_nRAYTRACESPHERE;
#ifndef NDEBUG
	bool m_bRAYTRACESPHERE;
#endif
public:
	void SetRAYTRACESPHERE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nRAYTRACESPHERE = i;
#ifndef NDEBUG
		m_bRAYTRACESPHERE = true;
#endif
	}
	void SetRAYTRACESPHERE( bool i )
	{
		m_nRAYTRACESPHERE = i ? 1 : 0;
#ifndef NDEBUG
		m_bRAYTRACESPHERE = true;
#endif
	}
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
	eye_refract_ps30_Static_Index( )
	{
#ifndef NDEBUG
		m_bFLASHLIGHT = false;
#endif  // !NDEBUG
		m_nFLASHLIGHT = 0;
#ifndef NDEBUG
		m_bLIGHTWARPTEXTURE = false;
#endif  // !NDEBUG
		m_nLIGHTWARPTEXTURE = 0;
#ifndef NDEBUG
		m_bSPHERETEXKILLCOMBO = false;
#endif  // !NDEBUG
		m_nSPHERETEXKILLCOMBO = 0;
#ifndef NDEBUG
		m_bRAYTRACESPHERE = false;
#endif  // !NDEBUG
		m_nRAYTRACESPHERE = 0;
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
		bool bAllStaticVarsDefined = m_bFLASHLIGHT && m_bLIGHTWARPTEXTURE && m_bSPHERETEXKILLCOMBO && m_bRAYTRACESPHERE && m_bFLASHLIGHTDEPTHFILTERMODE;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 10 * m_nFLASHLIGHT ) + ( 20 * m_nLIGHTWARPTEXTURE ) + ( 40 * m_nSPHERETEXKILLCOMBO ) + ( 80 * m_nRAYTRACESPHERE ) + ( 160 * m_nFLASHLIGHTDEPTHFILTERMODE ) + 0;
	}
};
#define shaderStaticTest_eye_refract_ps30 psh_forgot_to_set_static_FLASHLIGHT + psh_forgot_to_set_static_LIGHTWARPTEXTURE + psh_forgot_to_set_static_SPHERETEXKILLCOMBO + psh_forgot_to_set_static_RAYTRACESPHERE + psh_forgot_to_set_static_FLASHLIGHTDEPTHFILTERMODE + 0
class eye_refract_ps30_Dynamic_Index
{
private:
	int m_nNUM_LIGHTS;
#ifndef NDEBUG
	bool m_bNUM_LIGHTS;
#endif
public:
	void SetNUM_LIGHTS( int i )
	{
		Assert( i >= 0 && i <= 4 );
		m_nNUM_LIGHTS = i;
#ifndef NDEBUG
		m_bNUM_LIGHTS = true;
#endif
	}
	void SetNUM_LIGHTS( bool i )
	{
		m_nNUM_LIGHTS = i ? 1 : 0;
#ifndef NDEBUG
		m_bNUM_LIGHTS = true;
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
	eye_refract_ps30_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bNUM_LIGHTS = false;
#endif  // !NDEBUG
		m_nNUM_LIGHTS = 0;
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
		bool bAllDynamicVarsDefined = m_bNUM_LIGHTS && m_bFLASHLIGHTSHADOWS;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nNUM_LIGHTS ) + ( 5 * m_nFLASHLIGHTSHADOWS ) + 0;
	}
};
#define shaderDynamicTest_eye_refract_ps30 psh_forgot_to_set_dynamic_NUM_LIGHTS + psh_forgot_to_set_dynamic_FLASHLIGHTSHADOWS + 0
