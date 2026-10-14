class particlelit_generic_vs30_Static_Index
{
private:
	int m_nHALFLAMBERT;
#ifndef NDEBUG
	bool m_bHALFLAMBERT;
#endif
public:
	void SetHALFLAMBERT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nHALFLAMBERT = i;
#ifndef NDEBUG
		m_bHALFLAMBERT = true;
#endif
	}
	void SetHALFLAMBERT( bool i )
	{
		m_nHALFLAMBERT = i ? 1 : 0;
#ifndef NDEBUG
		m_bHALFLAMBERT = true;
#endif
	}
public:
	particlelit_generic_vs30_Static_Index()
	{
#ifndef NDEBUG
		m_bHALFLAMBERT = false;
#endif // _DEBUG
		m_nHALFLAMBERT = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bHALFLAMBERT;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nHALFLAMBERT ) + 0;
	}
};
#define shaderStaticTest_particlelit_generic_vs30 vsh_forgot_to_set_static_HALFLAMBERT + 0
class particlelit_generic_vs30_Dynamic_Index
{
public:
	particlelit_generic_vs30_Dynamic_Index()
	{
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
#endif // _DEBUG
		return 0;
	}
};
#define shaderDynamicTest_particlelit_generic_vs30 0
