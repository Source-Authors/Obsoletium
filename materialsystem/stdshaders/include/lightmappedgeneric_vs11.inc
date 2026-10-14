class lightmappedgeneric_vs11_Static_Index
{
private:
	int m_nDETAIL;
#ifndef NDEBUG
	bool m_bDETAIL;
#endif
public:
	void SetDETAIL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDETAIL = i;
#ifndef NDEBUG
		m_bDETAIL = true;
#endif
	}
	void SetDETAIL( bool i )
	{
		m_nDETAIL = i ? 1 : 0;
#ifndef NDEBUG
		m_bDETAIL = true;
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
	lightmappedgeneric_vs11_Static_Index()
	{
#ifndef NDEBUG
		m_bDETAIL = false;
#endif // _DEBUG
		m_nDETAIL = 0;
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
		bool bAllStaticVarsDefined = m_bDETAIL && m_bENVMAP && m_bENVMAPCAMERASPACE && m_bENVMAPSPHERE && m_bVERTEXCOLOR;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 2 * m_nDETAIL ) + ( 4 * m_nENVMAP ) + ( 8 * m_nENVMAPCAMERASPACE ) + ( 8 * m_nENVMAPSPHERE ) + ( 16 * m_nVERTEXCOLOR ) + 0;
	}
};
class lightmappedgeneric_vs11_Dynamic_Index
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
	lightmappedgeneric_vs11_Dynamic_Index()
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
