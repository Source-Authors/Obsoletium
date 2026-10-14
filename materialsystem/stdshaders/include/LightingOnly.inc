class lightingonly_Static_Index
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
public:
	lightingonly_Static_Index()
	{
#ifndef NDEBUG
		m_bHALF_LAMBERT = false;
#endif  // !NDEBUG
		m_nHALF_LAMBERT = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bHALF_LAMBERT;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 88 * m_nHALF_LAMBERT ) + 0;
	}
};
class lightingonly_Dynamic_Index
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
	lightingonly_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bDOWATERFOG = false;
#endif  // !NDEBUG
		m_nDOWATERFOG = 0;
#ifndef NDEBUG
		m_bLIGHT_COMBO = false;
#endif  // !NDEBUG
		m_nLIGHT_COMBO = 0;
#ifndef NDEBUG
		m_bSKINNING = false;
#endif  // !NDEBUG
		m_nSKINNING = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bDOWATERFOG && m_bLIGHT_COMBO && m_bSKINNING;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nDOWATERFOG ) + ( 2 * m_nLIGHT_COMBO ) + ( 44 * m_nSKINNING ) + 0;
	}
};
