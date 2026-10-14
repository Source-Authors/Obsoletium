#include "shaderlib/cshader.h"
class pyro_vision_vs30_Static_Index
{
private:
	int m_nEFFECT;
#ifndef NDEBUG
	bool m_bEFFECT;
#endif
public:
	void SetEFFECT( int i )
	{
		Assert( i >= 0 && i <= 3 );
		m_nEFFECT = i;
#ifndef NDEBUG
		m_bEFFECT = true;
#endif
	}
	void SetEFFECT( bool i )
	{
		m_nEFFECT = i ? 1 : 0;
#ifndef NDEBUG
		m_bEFFECT = true;
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
private:
	int m_nVERTEX_LIT;
#ifndef NDEBUG
	bool m_bVERTEX_LIT;
#endif
public:
	void SetVERTEX_LIT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nVERTEX_LIT = i;
#ifndef NDEBUG
		m_bVERTEX_LIT = true;
#endif
	}
	void SetVERTEX_LIT( bool i )
	{
		m_nVERTEX_LIT = i ? 1 : 0;
#ifndef NDEBUG
		m_bVERTEX_LIT = true;
#endif
	}
private:
	int m_nFULLBRIGHT;
#ifndef NDEBUG
	bool m_bFULLBRIGHT;
#endif
public:
	void SetFULLBRIGHT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFULLBRIGHT = i;
#ifndef NDEBUG
		m_bFULLBRIGHT = true;
#endif
	}
	void SetFULLBRIGHT( bool i )
	{
		m_nFULLBRIGHT = i ? 1 : 0;
#ifndef NDEBUG
		m_bFULLBRIGHT = true;
#endif
	}
private:
	int m_nBASETEXTURE2;
#ifndef NDEBUG
	bool m_bBASETEXTURE2;
#endif
public:
	void SetBASETEXTURE2( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBASETEXTURE2 = i;
#ifndef NDEBUG
		m_bBASETEXTURE2 = true;
#endif
	}
	void SetBASETEXTURE2( bool i )
	{
		m_nBASETEXTURE2 = i ? 1 : 0;
#ifndef NDEBUG
		m_bBASETEXTURE2 = true;
#endif
	}
private:
	int m_nSTRIPES;
#ifndef NDEBUG
	bool m_bSTRIPES;
#endif
public:
	void SetSTRIPES( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSTRIPES = i;
#ifndef NDEBUG
		m_bSTRIPES = true;
#endif
	}
	void SetSTRIPES( bool i )
	{
		m_nSTRIPES = i ? 1 : 0;
#ifndef NDEBUG
		m_bSTRIPES = true;
#endif
	}
private:
	int m_nSTRIPES_USE_NORMAL2;
#ifndef NDEBUG
	bool m_bSTRIPES_USE_NORMAL2;
#endif
public:
	void SetSTRIPES_USE_NORMAL2( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSTRIPES_USE_NORMAL2 = i;
#ifndef NDEBUG
		m_bSTRIPES_USE_NORMAL2 = true;
#endif
	}
	void SetSTRIPES_USE_NORMAL2( bool i )
	{
		m_nSTRIPES_USE_NORMAL2 = i ? 1 : 0;
#ifndef NDEBUG
		m_bSTRIPES_USE_NORMAL2 = true;
#endif
	}
public:
	pyro_vision_vs30_Static_Index( )
	{
#ifndef NDEBUG
		m_bEFFECT = false;
#endif  // !NDEBUG
		m_nEFFECT = 0;
#ifndef NDEBUG
		m_bVERTEXCOLOR = false;
#endif  // !NDEBUG
		m_nVERTEXCOLOR = 0;
#ifndef NDEBUG
		m_bHALFLAMBERT = false;
#endif  // !NDEBUG
		m_nHALFLAMBERT = 0;
#ifndef NDEBUG
		m_bVERTEX_LIT = false;
#endif  // !NDEBUG
		m_nVERTEX_LIT = 0;
#ifndef NDEBUG
		m_bFULLBRIGHT = false;
#endif  // !NDEBUG
		m_nFULLBRIGHT = 0;
#ifndef NDEBUG
		m_bBASETEXTURE2 = false;
#endif  // !NDEBUG
		m_nBASETEXTURE2 = 0;
#ifndef NDEBUG
		m_bSTRIPES = false;
#endif  // !NDEBUG
		m_nSTRIPES = 0;
#ifndef NDEBUG
		m_bSTRIPES_USE_NORMAL2 = false;
#endif  // !NDEBUG
		m_nSTRIPES_USE_NORMAL2 = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bEFFECT && m_bVERTEXCOLOR && m_bHALFLAMBERT && m_bVERTEX_LIT && m_bFULLBRIGHT && m_bBASETEXTURE2 && m_bSTRIPES && m_bSTRIPES_USE_NORMAL2;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 16 * m_nEFFECT ) + ( 64 * m_nVERTEXCOLOR ) + ( 128 * m_nHALFLAMBERT ) + ( 256 * m_nVERTEX_LIT ) + ( 512 * m_nFULLBRIGHT ) + ( 1024 * m_nBASETEXTURE2 ) + ( 2048 * m_nSTRIPES ) + ( 4096 * m_nSTRIPES_USE_NORMAL2 ) + 0;
	}
};
#define shaderStaticTest_pyro_vision_vs30 vsh_forgot_to_set_static_EFFECT + vsh_forgot_to_set_static_VERTEXCOLOR + vsh_forgot_to_set_static_HALFLAMBERT + vsh_forgot_to_set_static_VERTEX_LIT + vsh_forgot_to_set_static_FULLBRIGHT + vsh_forgot_to_set_static_BASETEXTURE2 + vsh_forgot_to_set_static_STRIPES + vsh_forgot_to_set_static_STRIPES_USE_NORMAL2 + 0
class pyro_vision_vs30_Dynamic_Index
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
private:
	int m_nDYNAMIC_LIGHT;
#ifndef NDEBUG
	bool m_bDYNAMIC_LIGHT;
#endif
public:
	void SetDYNAMIC_LIGHT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDYNAMIC_LIGHT = i;
#ifndef NDEBUG
		m_bDYNAMIC_LIGHT = true;
#endif
	}
	void SetDYNAMIC_LIGHT( bool i )
	{
		m_nDYNAMIC_LIGHT = i ? 1 : 0;
#ifndef NDEBUG
		m_bDYNAMIC_LIGHT = true;
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
	pyro_vision_vs30_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bCOMPRESSED_VERTS = false;
#endif  // !NDEBUG
		m_nCOMPRESSED_VERTS = 0;
#ifndef NDEBUG
		m_bSKINNING = false;
#endif  // !NDEBUG
		m_nSKINNING = 0;
#ifndef NDEBUG
		m_bDYNAMIC_LIGHT = false;
#endif  // !NDEBUG
		m_nDYNAMIC_LIGHT = 0;
#ifndef NDEBUG
		m_bSTATIC_LIGHT = false;
#endif  // !NDEBUG
		m_nSTATIC_LIGHT = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bCOMPRESSED_VERTS && m_bSKINNING && m_bDYNAMIC_LIGHT && m_bSTATIC_LIGHT;
		Assert( bAllDynamicVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nCOMPRESSED_VERTS ) + ( 2 * m_nSKINNING ) + ( 4 * m_nDYNAMIC_LIGHT ) + ( 8 * m_nSTATIC_LIGHT ) + 0;
	}
};
#define shaderDynamicTest_pyro_vision_vs30 vsh_forgot_to_set_dynamic_COMPRESSED_VERTS + vsh_forgot_to_set_dynamic_SKINNING + vsh_forgot_to_set_dynamic_DYNAMIC_LIGHT + vsh_forgot_to_set_dynamic_STATIC_LIGHT + 0
