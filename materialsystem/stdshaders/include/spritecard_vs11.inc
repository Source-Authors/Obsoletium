#include "shaderlib/cshader.h"
class spritecard_vs11_Static_Index
{
private:
	int m_nZOOM_ANIMATE_SEQ2;
#ifndef NDEBUG
	bool m_bZOOM_ANIMATE_SEQ2;
#endif
public:
	void SetZOOM_ANIMATE_SEQ2( int i )
	{
		Assert( i >= 0 && i <= 0 );
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
		Assert( i >= 0 && i <= 0 );
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
		Assert( i >= 0 && i <= 0 );
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
public:
	spritecard_vs11_Static_Index( )
	{
#ifndef NDEBUG
		m_bZOOM_ANIMATE_SEQ2 = false;
#endif // _DEBUG
		m_nZOOM_ANIMATE_SEQ2 = 0;
#ifndef NDEBUG
		m_bDUALSEQUENCE = false;
#endif // _DEBUG
		m_nDUALSEQUENCE = 0;
#ifndef NDEBUG
		m_bEXTRACTGREENALPHA = false;
#endif // _DEBUG
		m_nEXTRACTGREENALPHA = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bZOOM_ANIMATE_SEQ2 && m_bDUALSEQUENCE && m_bEXTRACTGREENALPHA;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 3 * m_nZOOM_ANIMATE_SEQ2 ) + ( 3 * m_nDUALSEQUENCE ) + ( 3 * m_nEXTRACTGREENALPHA ) + 0;
	}
};
#define shaderStaticTest_spritecard_vs11 vsh_forgot_to_set_static_ZOOM_ANIMATE_SEQ2 + vsh_forgot_to_set_static_DUALSEQUENCE + vsh_forgot_to_set_static_EXTRACTGREENALPHA + 0
class spritecard_vs11_Dynamic_Index
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
	spritecard_vs11_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bORIENTATION = false;
#endif // _DEBUG
		m_nORIENTATION = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bORIENTATION;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nORIENTATION ) + 0;
	}
};
#define shaderDynamicTest_spritecard_vs11 vsh_forgot_to_set_dynamic_ORIENTATION + 0
