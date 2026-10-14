#include "shaderlib/cshader.h"
class watercheap_vs20_Static_Index
{
private:
	int m_nBLEND;
#ifndef NDEBUG
	bool m_bBLEND;
#endif
public:
	void SetBLEND( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBLEND = i;
#ifndef NDEBUG
		m_bBLEND = true;
#endif
	}
	void SetBLEND( bool i )
	{
		m_nBLEND = i ? 1 : 0;
#ifndef NDEBUG
		m_bBLEND = true;
#endif
	}
public:
	watercheap_vs20_Static_Index( )
	{
#ifndef NDEBUG
		m_bBLEND = false;
#endif // _DEBUG
		m_nBLEND = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bBLEND;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nBLEND ) + 0;
	}
};
#define shaderStaticTest_watercheap_vs20 vsh_forgot_to_set_static_BLEND + 0
class watercheap_vs20_Dynamic_Index
{
public:
	watercheap_vs20_Dynamic_Index()
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
#define shaderDynamicTest_watercheap_vs20 0
