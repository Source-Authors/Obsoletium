#include "shaderlib/cshader.h"
class worldtwotextureblend_ps20_Static_Index
{
private:
	int m_nDETAILTEXTURE;
#ifndef NDEBUG
	bool m_bDETAILTEXTURE;
#endif
public:
	void SetDETAILTEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDETAILTEXTURE = i;
#ifndef NDEBUG
		m_bDETAILTEXTURE = true;
#endif
	}
	void SetDETAILTEXTURE( bool i )
	{
		m_nDETAILTEXTURE = i ? 1 : 0;
#ifndef NDEBUG
		m_bDETAILTEXTURE = true;
#endif
	}
private:
	int m_nBUMPMAP;
#ifndef NDEBUG
	bool m_bBUMPMAP;
#endif
public:
	void SetBUMPMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBUMPMAP = i;
#ifndef NDEBUG
		m_bBUMPMAP = true;
#endif
	}
	void SetBUMPMAP( bool i )
	{
		m_nBUMPMAP = i ? 1 : 0;
#ifndef NDEBUG
		m_bBUMPMAP = true;
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
	int m_nSELFILLUM;
#ifndef NDEBUG
	bool m_bSELFILLUM;
#endif
public:
	void SetSELFILLUM( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSELFILLUM = i;
#ifndef NDEBUG
		m_bSELFILLUM = true;
#endif
	}
	void SetSELFILLUM( bool i )
	{
		m_nSELFILLUM = i ? 1 : 0;
#ifndef NDEBUG
		m_bSELFILLUM = true;
#endif
	}
private:
	int m_nDIFFUSEBUMPMAP;
#ifndef NDEBUG
	bool m_bDIFFUSEBUMPMAP;
#endif
public:
	void SetDIFFUSEBUMPMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDIFFUSEBUMPMAP = i;
#ifndef NDEBUG
		m_bDIFFUSEBUMPMAP = true;
#endif
	}
	void SetDIFFUSEBUMPMAP( bool i )
	{
		m_nDIFFUSEBUMPMAP = i ? 1 : 0;
#ifndef NDEBUG
		m_bDIFFUSEBUMPMAP = true;
#endif
	}
private:
	int m_nDETAIL_ALPHA_MASK_BASE_TEXTURE;
#ifndef NDEBUG
	bool m_bDETAIL_ALPHA_MASK_BASE_TEXTURE;
#endif
public:
	void SetDETAIL_ALPHA_MASK_BASE_TEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDETAIL_ALPHA_MASK_BASE_TEXTURE = i;
#ifndef NDEBUG
		m_bDETAIL_ALPHA_MASK_BASE_TEXTURE = true;
#endif
	}
	void SetDETAIL_ALPHA_MASK_BASE_TEXTURE( bool i )
	{
		m_nDETAIL_ALPHA_MASK_BASE_TEXTURE = i ? 1 : 0;
#ifndef NDEBUG
		m_bDETAIL_ALPHA_MASK_BASE_TEXTURE = true;
#endif
	}
private:
	int m_nFLASHLIGHT;
#ifndef NDEBUG
	bool m_bFLASHLIGHT;
#endif
public:
	void SetFLASHLIGHT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFLASHLIGHT = i;
#ifndef NDEBUG
		m_bFLASHLIGHT = true;
#endif
	}
	void SetFLASHLIGHT( bool i )
	{
		m_nFLASHLIGHT = i ? 1 : 0;
#ifndef NDEBUG
		m_bFLASHLIGHT = true;
#endif
	}
private:
	int m_nSEAMLESS;
#ifndef NDEBUG
	bool m_bSEAMLESS;
#endif
public:
	void SetSEAMLESS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSEAMLESS = i;
#ifndef NDEBUG
		m_bSEAMLESS = true;
#endif
	}
	void SetSEAMLESS( bool i )
	{
		m_nSEAMLESS = i ? 1 : 0;
#ifndef NDEBUG
		m_bSEAMLESS = true;
#endif
	}
public:
	worldtwotextureblend_ps20_Static_Index( )
	{
#ifndef NDEBUG
		m_bDETAILTEXTURE = false;
#endif // _DEBUG
		m_nDETAILTEXTURE = 0;
#ifndef NDEBUG
		m_bBUMPMAP = false;
#endif // _DEBUG
		m_nBUMPMAP = 0;
#ifndef NDEBUG
		m_bVERTEXCOLOR = false;
#endif // _DEBUG
		m_nVERTEXCOLOR = 0;
#ifndef NDEBUG
		m_bSELFILLUM = false;
#endif // _DEBUG
		m_nSELFILLUM = 0;
#ifndef NDEBUG
		m_bDIFFUSEBUMPMAP = false;
#endif // _DEBUG
		m_nDIFFUSEBUMPMAP = 0;
#ifndef NDEBUG
		m_bDETAIL_ALPHA_MASK_BASE_TEXTURE = false;
#endif // _DEBUG
		m_nDETAIL_ALPHA_MASK_BASE_TEXTURE = 0;
#ifndef NDEBUG
		m_bFLASHLIGHT = false;
#endif // _DEBUG
		m_nFLASHLIGHT = 0;
#ifndef NDEBUG
		m_bSEAMLESS = false;
#endif // _DEBUG
		m_nSEAMLESS = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bDETAILTEXTURE && m_bBUMPMAP && m_bVERTEXCOLOR && m_bSELFILLUM && m_bDIFFUSEBUMPMAP && m_bDETAIL_ALPHA_MASK_BASE_TEXTURE && m_bFLASHLIGHT && m_bSEAMLESS;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 4 * m_nDETAILTEXTURE ) + ( 8 * m_nBUMPMAP ) + ( 16 * m_nVERTEXCOLOR ) + ( 32 * m_nSELFILLUM ) + ( 64 * m_nDIFFUSEBUMPMAP ) + ( 128 * m_nDETAIL_ALPHA_MASK_BASE_TEXTURE ) + ( 256 * m_nFLASHLIGHT ) + ( 512 * m_nSEAMLESS ) + 0;
	}
};
#define shaderStaticTest_worldtwotextureblend_ps20 psh_forgot_to_set_static_DETAILTEXTURE + psh_forgot_to_set_static_BUMPMAP + psh_forgot_to_set_static_VERTEXCOLOR + psh_forgot_to_set_static_SELFILLUM + psh_forgot_to_set_static_DIFFUSEBUMPMAP + psh_forgot_to_set_static_DETAIL_ALPHA_MASK_BASE_TEXTURE + psh_forgot_to_set_static_FLASHLIGHT + psh_forgot_to_set_static_SEAMLESS + 0
class worldtwotextureblend_ps20_Dynamic_Index
{
private:
	int m_nWRITEWATERFOGTODESTALPHA;
#ifndef NDEBUG
	bool m_bWRITEWATERFOGTODESTALPHA;
#endif
public:
	void SetWRITEWATERFOGTODESTALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nWRITEWATERFOGTODESTALPHA = i;
#ifndef NDEBUG
		m_bWRITEWATERFOGTODESTALPHA = true;
#endif
	}
	void SetWRITEWATERFOGTODESTALPHA( bool i )
	{
		m_nWRITEWATERFOGTODESTALPHA = i ? 1 : 0;
#ifndef NDEBUG
		m_bWRITEWATERFOGTODESTALPHA = true;
#endif
	}
private:
	int m_nPIXELFOGTYPE;
#ifndef NDEBUG
	bool m_bPIXELFOGTYPE;
#endif
public:
	void SetPIXELFOGTYPE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPIXELFOGTYPE = i;
#ifndef NDEBUG
		m_bPIXELFOGTYPE = true;
#endif
	}
	void SetPIXELFOGTYPE( bool i )
	{
		m_nPIXELFOGTYPE = i ? 1 : 0;
#ifndef NDEBUG
		m_bPIXELFOGTYPE = true;
#endif
	}
public:
	worldtwotextureblend_ps20_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bWRITEWATERFOGTODESTALPHA = false;
#endif // _DEBUG
		m_nWRITEWATERFOGTODESTALPHA = 0;
#ifndef NDEBUG
		m_bPIXELFOGTYPE = false;
#endif // _DEBUG
		m_nPIXELFOGTYPE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bWRITEWATERFOGTODESTALPHA && m_bPIXELFOGTYPE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nWRITEWATERFOGTODESTALPHA ) + ( 2 * m_nPIXELFOGTYPE ) + 0;
	}
};
#define shaderDynamicTest_worldtwotextureblend_ps20 psh_forgot_to_set_dynamic_WRITEWATERFOGTODESTALPHA + psh_forgot_to_set_dynamic_PIXELFOGTYPE + 0
