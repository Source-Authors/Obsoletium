class vortwarp_vs11_Static_Index
{
private:
	int m_nUNLIT;
#ifndef NDEBUG
	bool m_bUNLIT;
#endif
public:
	void SetUNLIT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nUNLIT = i;
#ifndef NDEBUG
		m_bUNLIT = true;
#endif
	}
	void SetUNLIT( bool i )
	{
		m_nUNLIT = i ? 1 : 0;
#ifndef NDEBUG
		m_bUNLIT = true;
#endif
	}
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
	vortwarp_vs11_Static_Index()
	{
#ifndef NDEBUG
		m_bUNLIT = false;
#endif  // !NDEBUG
		m_nUNLIT = 0;
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
		bool bAllStaticVarsDefined = m_bUNLIT && m_bHALF_LAMBERT;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 44 * m_nUNLIT ) + ( 88 * m_nHALF_LAMBERT ) + 0;
	}
};
class vortwarp_vs11_Dynamic_Index
{
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
	vortwarp_vs11_Dynamic_Index()
	{
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
		bool bAllDynamicVarsDefined = m_bLIGHT_COMBO && m_bSKINNING;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nLIGHT_COMBO ) + ( 22 * m_nSKINNING ) + 0;
	}
};
