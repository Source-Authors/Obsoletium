#include "shaderlib/cshader.h"
class spritecard_vs20_Static_Index
{
private:
	int m_nZOOM_ANIMATE_SEQ2;
#ifndef NDEBUG
	bool m_bZOOM_ANIMATE_SEQ2;
#endif
public:
	void SetZOOM_ANIMATE_SEQ2( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nZOOM_ANIMATE_SEQ2 = i;
#ifndef NDEBUG
		m_bZOOM_ANIMATE_SEQ2 = true;
#endif
	}
	void SetZOOM_ANIMATE_SEQ2( bool i )
	{
		m_nZOOM_ANIMATE_SEQ2 = i ? 1 : 0;
#ifndef NDEBUG
		m_bZOOM_ANIMATE_SEQ2 = true;
#endif
	}
private:
	int m_nDUALSEQUENCE;
#ifndef NDEBUG
	bool m_bDUALSEQUENCE;
#endif
public:
	void SetDUALSEQUENCE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDUALSEQUENCE = i;
#ifndef NDEBUG
		m_bDUALSEQUENCE = true;
#endif
	}
	void SetDUALSEQUENCE( bool i )
	{
		m_nDUALSEQUENCE = i ? 1 : 0;
#ifndef NDEBUG
		m_bDUALSEQUENCE = true;
#endif
	}
private:
	int m_nEXTRACTGREENALPHA;
#ifndef NDEBUG
	bool m_bEXTRACTGREENALPHA;
#endif
public:
	void SetEXTRACTGREENALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nEXTRACTGREENALPHA = i;
#ifndef NDEBUG
		m_bEXTRACTGREENALPHA = true;
#endif
	}
	void SetEXTRACTGREENALPHA( bool i )
	{
		m_nEXTRACTGREENALPHA = i ? 1 : 0;
#ifndef NDEBUG
		m_bEXTRACTGREENALPHA = true;
#endif
	}
private:
	int m_nUSE_INSTANCING;
#ifndef NDEBUG
	bool m_bUSE_INSTANCING;
#endif
public:
	void SetUSE_INSTANCING( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nUSE_INSTANCING = i;
#ifndef NDEBUG
		m_bUSE_INSTANCING = true;
#endif
	}
	void SetUSE_INSTANCING( bool i )
	{
		m_nUSE_INSTANCING = i ? 1 : 0;
#ifndef NDEBUG
		m_bUSE_INSTANCING = true;
#endif
	}
public:
	spritecard_vs20_Static_Index( )
	{
#ifndef NDEBUG
		m_bZOOM_ANIMATE_SEQ2 = false;
#endif  // !NDEBUG
		m_nZOOM_ANIMATE_SEQ2 = 0;
#ifndef NDEBUG
		m_bDUALSEQUENCE = false;
#endif  // !NDEBUG
		m_nDUALSEQUENCE = 0;
#ifndef NDEBUG
		m_bEXTRACTGREENALPHA = false;
#endif  // !NDEBUG
		m_nEXTRACTGREENALPHA = 0;
#ifndef NDEBUG
		m_bUSE_INSTANCING = false;
#endif  // !NDEBUG
		m_nUSE_INSTANCING = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bZOOM_ANIMATE_SEQ2 && m_bDUALSEQUENCE && m_bEXTRACTGREENALPHA && m_bUSE_INSTANCING;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 3 * m_nZOOM_ANIMATE_SEQ2 ) + ( 6 * m_nDUALSEQUENCE ) + ( 12 * m_nEXTRACTGREENALPHA ) + ( 24 * m_nUSE_INSTANCING ) + 0;
	}
};
#define shaderStaticTest_spritecard_vs20 vsh_forgot_to_set_static_ZOOM_ANIMATE_SEQ2 + vsh_forgot_to_set_static_DUALSEQUENCE + vsh_forgot_to_set_static_EXTRACTGREENALPHA + vsh_forgot_to_set_static_USE_INSTANCING + 0
class spritecard_vs20_Dynamic_Index
{
private:
	int m_nORIENTATION;
#ifndef NDEBUG
	bool m_bORIENTATION;
#endif
public:
	void SetORIENTATION( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nORIENTATION = i;
#ifndef NDEBUG
		m_bORIENTATION = true;
#endif
	}
	void SetORIENTATION( bool i )
	{
		m_nORIENTATION = i ? 1 : 0;
#ifndef NDEBUG
		m_bORIENTATION = true;
#endif
	}
public:
	spritecard_vs20_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bORIENTATION = false;
#endif  // !NDEBUG
		m_nORIENTATION = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bORIENTATION;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nORIENTATION ) + 0;
	}
};
#define shaderDynamicTest_spritecard_vs20 vsh_forgot_to_set_dynamic_ORIENTATION + 0
