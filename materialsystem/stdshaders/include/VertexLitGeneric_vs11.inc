class vertexlitgeneric_vs11_Static_Index
{
private:
	int m_nHALF_LAMBERT;
#ifndef NDEBUG
	bool m_bHALF_LAMBERT;
#endif
public:
	void SetHALF_LAMBERT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nHALF_LAMBERT = i;
#ifndef NDEBUG
		m_bHALF_LAMBERT = true;
#endif
	}
	void SetHALF_LAMBERT( bool i )
	{
		m_nHALF_LAMBERT = i ? 1 : 0;
#ifndef NDEBUG
		m_bHALF_LAMBERT = true;
#endif
	}
private:
	int m_nENVMAP;
#ifndef NDEBUG
	bool m_bENVMAP;
#endif
public:
	void SetENVMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nENVMAP = i;
#ifndef NDEBUG
		m_bENVMAP = true;
#endif
	}
	void SetENVMAP( bool i )
	{
		m_nENVMAP = i ? 1 : 0;
#ifndef NDEBUG
		m_bENVMAP = true;
#endif
	}
private:
	int m_nENVMAPCAMERASPACE;
#ifndef NDEBUG
	bool m_bENVMAPCAMERASPACE;
#endif
public:
	void SetENVMAPCAMERASPACE( int i )
	{
	}
	void SetENVMAPCAMERASPACE( bool i )
	{
	}
private:
	int m_nENVMAPSPHERE;
#ifndef NDEBUG
	bool m_bENVMAPSPHERE;
#endif
public:
	void SetENVMAPSPHERE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nENVMAPSPHERE = i;
#ifndef NDEBUG
		m_bENVMAPSPHERE = true;
#endif
	}
	void SetENVMAPSPHERE( bool i )
	{
		m_nENVMAPSPHERE = i ? 1 : 0;
#ifndef NDEBUG
		m_bENVMAPSPHERE = true;
#endif
	}
public:
	vertexlitgeneric_vs11_Static_Index()
	{
#ifndef NDEBUG
		m_bHALF_LAMBERT = false;
#endif // _DEBUG
		m_nHALF_LAMBERT = 0;
#ifndef NDEBUG
		m_bENVMAP = false;
#endif // _DEBUG
		m_nENVMAP = 0;
#ifndef NDEBUG
		m_bENVMAPCAMERASPACE = true;
#endif // _DEBUG
		m_nENVMAPCAMERASPACE = 0;
#ifndef NDEBUG
		m_bENVMAPSPHERE = false;
#endif // _DEBUG
		m_nENVMAPSPHERE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bHALF_LAMBERT && m_bENVMAP && m_bENVMAPCAMERASPACE && m_bENVMAPSPHERE;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 88 * m_nHALF_LAMBERT ) + ( 176 * m_nENVMAP ) + ( 352 * m_nENVMAPCAMERASPACE ) + ( 352 * m_nENVMAPSPHERE ) + 0;
	}
};
class vertexlitgeneric_vs11_Dynamic_Index
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
private:
	int m_nLIGHT_COMBO;
#ifndef NDEBUG
	bool m_bLIGHT_COMBO;
#endif
public:
	void SetLIGHT_COMBO( int i )
	{
		Assert( i >= 0 && i <= 21 );
		m_nLIGHT_COMBO = i;
#ifndef NDEBUG
		m_bLIGHT_COMBO = true;
#endif
	}
	void SetLIGHT_COMBO( bool i )
	{
		m_nLIGHT_COMBO = i ? 1 : 0;
#ifndef NDEBUG
		m_bLIGHT_COMBO = true;
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
	vertexlitgeneric_vs11_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bDOWATERFOG = false;
#endif // _DEBUG
		m_nDOWATERFOG = 0;
#ifndef NDEBUG
		m_bLIGHT_COMBO = false;
#endif // _DEBUG
		m_nLIGHT_COMBO = 0;
#ifndef NDEBUG
		m_bSKINNING = false;
#endif // _DEBUG
		m_nSKINNING = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bDOWATERFOG && m_bLIGHT_COMBO && m_bSKINNING;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nDOWATERFOG ) + ( 2 * m_nLIGHT_COMBO ) + ( 44 * m_nSKINNING ) + 0;
	}
};
