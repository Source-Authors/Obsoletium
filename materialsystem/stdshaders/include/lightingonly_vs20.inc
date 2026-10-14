class lightingonly_vs20_Static_Index
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
	lightingonly_vs20_Static_Index()
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
		return ( 32 * m_nHALFLAMBERT ) + 0;
	}
};
#define shaderStaticTest_lightingonly_vs20 vsh_forgot_to_set_static_HALFLAMBERT + 0
class lightingonly_vs20_Dynamic_Index
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
private:
	int m_nAMBIENT_LIGHT;
#ifndef NDEBUG
	bool m_bAMBIENT_LIGHT;
#endif
public:
	void SetAMBIENT_LIGHT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nAMBIENT_LIGHT = i;
#ifndef NDEBUG
		m_bAMBIENT_LIGHT = true;
#endif
	}
	void SetAMBIENT_LIGHT( bool i )
	{
		m_nAMBIENT_LIGHT = i ? 1 : 0;
#ifndef NDEBUG
		m_bAMBIENT_LIGHT = true;
#endif
	}
private:
	int m_nSTATIC_LIGHT;
#ifndef NDEBUG
	bool m_bSTATIC_LIGHT;
#endif
public:
	void SetSTATIC_LIGHT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSTATIC_LIGHT = i;
#ifndef NDEBUG
		m_bSTATIC_LIGHT = true;
#endif
	}
	void SetSTATIC_LIGHT( bool i )
	{
		m_nSTATIC_LIGHT = i ? 1 : 0;
#ifndef NDEBUG
		m_bSTATIC_LIGHT = true;
#endif
	}
public:
	lightingonly_vs20_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bDOWATERFOG = false;
#endif // _DEBUG
		m_nDOWATERFOG = 0;
#ifndef NDEBUG
		m_bNUM_BONES = false;
#endif // _DEBUG
		m_nNUM_BONES = 0;
#ifndef NDEBUG
		m_bAMBIENT_LIGHT = false;
#endif // _DEBUG
		m_nAMBIENT_LIGHT = 0;
#ifndef NDEBUG
		m_bSTATIC_LIGHT = false;
#endif // _DEBUG
		m_nSTATIC_LIGHT = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bDOWATERFOG && m_bNUM_BONES && m_bAMBIENT_LIGHT && m_bSTATIC_LIGHT;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nDOWATERFOG ) + ( 2 * m_nNUM_BONES ) + ( 8 * m_nAMBIENT_LIGHT ) + ( 16 * m_nSTATIC_LIGHT ) + 0;
	}
};
#define shaderDynamicTest_lightingonly_vs20 vsh_forgot_to_set_dynamic_DOWATERFOG + vsh_forgot_to_set_dynamic_NUM_BONES + vsh_forgot_to_set_dynamic_AMBIENT_LIGHT + vsh_forgot_to_set_dynamic_STATIC_LIGHT + 0
