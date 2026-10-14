#include "shaderlib/cshader.h"
class portal_vs11_Static_Index
{
private:
	int m_nMAXTEXTURESTAGES;
#ifndef NDEBUG
	bool m_bMAXTEXTURESTAGES;
#endif
public:
	void SetMAXTEXTURESTAGES( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nMAXTEXTURESTAGES = i;
#ifndef NDEBUG
		m_bMAXTEXTURESTAGES = true;
#endif
	}
	void SetMAXTEXTURESTAGES( bool i )
	{
		m_nMAXTEXTURESTAGES = i ? 1 : 0;
#ifndef NDEBUG
		m_bMAXTEXTURESTAGES = true;
#endif
	}
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
private:
	int m_nUSEALTERNATEVIEW;
#ifndef NDEBUG
	bool m_bUSEALTERNATEVIEW;
#endif
public:
	void SetUSEALTERNATEVIEW( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nUSEALTERNATEVIEW = i;
#ifndef NDEBUG
		m_bUSEALTERNATEVIEW = true;
#endif
	}
	void SetUSEALTERNATEVIEW( bool i )
	{
		m_nUSEALTERNATEVIEW = i ? 1 : 0;
#ifndef NDEBUG
		m_bUSEALTERNATEVIEW = true;
#endif
	}
public:
	portal_vs11_Static_Index( )
	{
#ifndef NDEBUG
		m_bMAXTEXTURESTAGES = false;
#endif  // !NDEBUG
		m_nMAXTEXTURESTAGES = 0;
#ifndef NDEBUG
		m_bHASALPHAMASK = false;
#endif  // !NDEBUG
		m_nHASALPHAMASK = 0;
#ifndef NDEBUG
		m_bHASSTATICTEXTURE = false;
#endif  // !NDEBUG
		m_nHASSTATICTEXTURE = 0;
#ifndef NDEBUG
		m_bUSEALTERNATEVIEW = false;
#endif  // !NDEBUG
		m_nUSEALTERNATEVIEW = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bMAXTEXTURESTAGES && m_bHASALPHAMASK && m_bHASSTATICTEXTURE && m_bUSEALTERNATEVIEW;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 4 * m_nMAXTEXTURESTAGES ) + ( 12 * m_nHASALPHAMASK ) + ( 24 * m_nHASSTATICTEXTURE ) + ( 48 * m_nUSEALTERNATEVIEW ) + 0;
	}
};
#define shaderStaticTest_portal_vs11 vsh_forgot_to_set_static_MAXTEXTURESTAGES + vsh_forgot_to_set_static_HASALPHAMASK + vsh_forgot_to_set_static_HASSTATICTEXTURE + vsh_forgot_to_set_static_USEALTERNATEVIEW + 0
class portal_vs11_Dynamic_Index
{
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
public:
	portal_vs11_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bSKINNING = false;
#endif  // !NDEBUG
		m_nSKINNING = 0;
#ifndef NDEBUG
		m_bADDSTATIC = false;
#endif  // !NDEBUG
		m_nADDSTATIC = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bSKINNING && m_bADDSTATIC;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nSKINNING ) + ( 2 * m_nADDSTATIC ) + 0;
	}
};
#define shaderDynamicTest_portal_vs11 vsh_forgot_to_set_dynamic_SKINNING + vsh_forgot_to_set_dynamic_ADDSTATIC + 0
