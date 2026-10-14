class unlitgeneric_lightingonly_vs11_Static_Index
{
public:
	unlitgeneric_lightingonly_vs11_Static_Index()
	{
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
#endif  // !NDEBUG
		return 0;
	}
};
#define shaderStaticTest_unlitgeneric_lightingonly_vs11 0
class unlitgeneric_lightingonly_vs11_Dynamic_Index
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
	int m_nNUM_BONES;
#ifndef NDEBUG
	bool m_bNUM_BONES;
#endif
public:
	void SetNUM_BONES( int i )
	{
		Assert( i >= 0 && i <= 3 );
		m_nNUM_BONES = i;
#ifndef NDEBUG
		m_bNUM_BONES = true;
#endif
	}
	void SetNUM_BONES( bool i )
	{
		m_nNUM_BONES = i ? 1 : 0;
#ifndef NDEBUG
		m_bNUM_BONES = true;
#endif
	}
public:
	unlitgeneric_lightingonly_vs11_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bDOWATERFOG = false;
#endif  // !NDEBUG
		m_nDOWATERFOG = 0;
#ifndef NDEBUG
		m_bNUM_BONES = false;
#endif  // !NDEBUG
		m_nNUM_BONES = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bDOWATERFOG && m_bNUM_BONES;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nDOWATERFOG ) + ( 2 * m_nNUM_BONES ) + 0;
	}
};
#define shaderDynamicTest_unlitgeneric_lightingonly_vs11 vsh_forgot_to_set_dynamic_DOWATERFOG + vsh_forgot_to_set_dynamic_NUM_BONES + 0
