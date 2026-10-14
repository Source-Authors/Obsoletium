#include "shaderlib/cshader.h"
class debugtextureview_vs20_Static_Index
{
public:
	debugtextureview_vs20_Static_Index( )
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
#define shaderStaticTest_debugtextureview_vs20 0
class debugtextureview_vs20_Dynamic_Index
{
private:
	int m_nCOMPRESSED_VERTS;
#ifndef NDEBUG
	bool m_bCOMPRESSED_VERTS;
#endif
public:
	void SetCOMPRESSED_VERTS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCOMPRESSED_VERTS = i;
#ifndef NDEBUG
		m_bCOMPRESSED_VERTS = true;
#endif
	}
	void SetCOMPRESSED_VERTS( bool i )
	{
		m_nCOMPRESSED_VERTS = i ? 1 : 0;
#ifndef NDEBUG
		m_bCOMPRESSED_VERTS = true;
#endif
	}
public:
	debugtextureview_vs20_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bCOMPRESSED_VERTS = false;
#endif  // !NDEBUG
		m_nCOMPRESSED_VERTS = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bCOMPRESSED_VERTS;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nCOMPRESSED_VERTS ) + 0;
	}
};
#define shaderDynamicTest_debugtextureview_vs20 vsh_forgot_to_set_dynamic_COMPRESSED_VERTS + 0
