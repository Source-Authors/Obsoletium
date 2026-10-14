#include "shaderlib/cshader.h"
class vertexlit_and_unlit_generic_ps20_Static_Index
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
	int m_nCUBEMAP;
#ifndef NDEBUG
	bool m_bCUBEMAP;
#endif
public:
	void SetCUBEMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCUBEMAP = i;
#ifndef NDEBUG
		m_bCUBEMAP = true;
#endif
	}
	void SetCUBEMAP( bool i )
	{
		m_nCUBEMAP = i ? 1 : 0;
#ifndef NDEBUG
		m_bCUBEMAP = true;
#endif
	}
private:
	int m_nDIFFUSELIGHTING;
#ifndef NDEBUG
	bool m_bDIFFUSELIGHTING;
#endif
public:
	void SetDIFFUSELIGHTING( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDIFFUSELIGHTING = i;
#ifndef NDEBUG
		m_bDIFFUSELIGHTING = true;
#endif
	}
	void SetDIFFUSELIGHTING( bool i )
	{
		m_nDIFFUSELIGHTING = i ? 1 : 0;
#ifndef NDEBUG
		m_bDIFFUSELIGHTING = true;
#endif
	}
private:
	int m_nENVMAPMASK;
#ifndef NDEBUG
	bool m_bENVMAPMASK;
#endif
public:
	void SetENVMAPMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nENVMAPMASK = i;
#ifndef NDEBUG
		m_bENVMAPMASK = true;
#endif
	}
	void SetENVMAPMASK( bool i )
	{
		m_nENVMAPMASK = i ? 1 : 0;
#ifndef NDEBUG
		m_bENVMAPMASK = true;
#endif
	}
private:
	int m_nBASEALPHAENVMAPMASK;
#ifndef NDEBUG
	bool m_bBASEALPHAENVMAPMASK;
#endif
public:
	void SetBASEALPHAENVMAPMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBASEALPHAENVMAPMASK = i;
#ifndef NDEBUG
		m_bBASEALPHAENVMAPMASK = true;
#endif
	}
	void SetBASEALPHAENVMAPMASK( bool i )
	{
		m_nBASEALPHAENVMAPMASK = i ? 1 : 0;
#ifndef NDEBUG
		m_bBASEALPHAENVMAPMASK = true;
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
	int m_nSELFILLUM_ENVMAPMASK_ALPHA;
#ifndef NDEBUG
	bool m_bSELFILLUM_ENVMAPMASK_ALPHA;
#endif
public:
	void SetSELFILLUM_ENVMAPMASK_ALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSELFILLUM_ENVMAPMASK_ALPHA = i;
#ifndef NDEBUG
		m_bSELFILLUM_ENVMAPMASK_ALPHA = true;
#endif
	}
	void SetSELFILLUM_ENVMAPMASK_ALPHA( bool i )
	{
		m_nSELFILLUM_ENVMAPMASK_ALPHA = i ? 1 : 0;
#ifndef NDEBUG
		m_bSELFILLUM_ENVMAPMASK_ALPHA = true;
#endif
	}
private:
	int m_nDETAIL_BLEND_MODE;
#ifndef NDEBUG
	bool m_bDETAIL_BLEND_MODE;
#endif
public:
	void SetDETAIL_BLEND_MODE( int i )
	{
		Assert( i >= 0 && i <= 9 );
		m_nDETAIL_BLEND_MODE = i;
#ifndef NDEBUG
		m_bDETAIL_BLEND_MODE = true;
#endif
	}
	void SetDETAIL_BLEND_MODE( bool i )
	{
		m_nDETAIL_BLEND_MODE = i ? 1 : 0;
#ifndef NDEBUG
		m_bDETAIL_BLEND_MODE = true;
#endif
	}
private:
	int m_nSEAMLESS_BASE;
#ifndef NDEBUG
	bool m_bSEAMLESS_BASE;
#endif
public:
	void SetSEAMLESS_BASE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSEAMLESS_BASE = i;
#ifndef NDEBUG
		m_bSEAMLESS_BASE = true;
#endif
	}
	void SetSEAMLESS_BASE( bool i )
	{
		m_nSEAMLESS_BASE = i ? 1 : 0;
#ifndef NDEBUG
		m_bSEAMLESS_BASE = true;
#endif
	}
private:
	int m_nSEAMLESS_DETAIL;
#ifndef NDEBUG
	bool m_bSEAMLESS_DETAIL;
#endif
public:
	void SetSEAMLESS_DETAIL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSEAMLESS_DETAIL = i;
#ifndef NDEBUG
		m_bSEAMLESS_DETAIL = true;
#endif
	}
	void SetSEAMLESS_DETAIL( bool i )
	{
		m_nSEAMLESS_DETAIL = i ? 1 : 0;
#ifndef NDEBUG
		m_bSEAMLESS_DETAIL = true;
#endif
	}
private:
	int m_nDISTANCEALPHA;
#ifndef NDEBUG
	bool m_bDISTANCEALPHA;
#endif
public:
	void SetDISTANCEALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDISTANCEALPHA = i;
#ifndef NDEBUG
		m_bDISTANCEALPHA = true;
#endif
	}
	void SetDISTANCEALPHA( bool i )
	{
		m_nDISTANCEALPHA = i ? 1 : 0;
#ifndef NDEBUG
		m_bDISTANCEALPHA = true;
#endif
	}
private:
	int m_nDISTANCEALPHAFROMDETAIL;
#ifndef NDEBUG
	bool m_bDISTANCEALPHAFROMDETAIL;
#endif
public:
	void SetDISTANCEALPHAFROMDETAIL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDISTANCEALPHAFROMDETAIL = i;
#ifndef NDEBUG
		m_bDISTANCEALPHAFROMDETAIL = true;
#endif
	}
	void SetDISTANCEALPHAFROMDETAIL( bool i )
	{
		m_nDISTANCEALPHAFROMDETAIL = i ? 1 : 0;
#ifndef NDEBUG
		m_bDISTANCEALPHAFROMDETAIL = true;
#endif
	}
private:
	int m_nSOFT_MASK;
#ifndef NDEBUG
	bool m_bSOFT_MASK;
#endif
public:
	void SetSOFT_MASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSOFT_MASK = i;
#ifndef NDEBUG
		m_bSOFT_MASK = true;
#endif
	}
	void SetSOFT_MASK( bool i )
	{
		m_nSOFT_MASK = i ? 1 : 0;
#ifndef NDEBUG
		m_bSOFT_MASK = true;
#endif
	}
private:
	int m_nOUTLINE;
#ifndef NDEBUG
	bool m_bOUTLINE;
#endif
public:
	void SetOUTLINE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nOUTLINE = i;
#ifndef NDEBUG
		m_bOUTLINE = true;
#endif
	}
	void SetOUTLINE( bool i )
	{
		m_nOUTLINE = i ? 1 : 0;
#ifndef NDEBUG
		m_bOUTLINE = true;
#endif
	}
private:
	int m_nOUTER_GLOW;
#ifndef NDEBUG
	bool m_bOUTER_GLOW;
#endif
public:
	void SetOUTER_GLOW( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nOUTER_GLOW = i;
#ifndef NDEBUG
		m_bOUTER_GLOW = true;
#endif
	}
	void SetOUTER_GLOW( bool i )
	{
		m_nOUTER_GLOW = i ? 1 : 0;
#ifndef NDEBUG
		m_bOUTER_GLOW = true;
#endif
	}
private:
	int m_nBLENDTINTBYBASEALPHA;
#ifndef NDEBUG
	bool m_bBLENDTINTBYBASEALPHA;
#endif
public:
	void SetBLENDTINTBYBASEALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBLENDTINTBYBASEALPHA = i;
#ifndef NDEBUG
		m_bBLENDTINTBYBASEALPHA = true;
#endif
	}
	void SetBLENDTINTBYBASEALPHA( bool i )
	{
		m_nBLENDTINTBYBASEALPHA = i ? 1 : 0;
#ifndef NDEBUG
		m_bBLENDTINTBYBASEALPHA = true;
#endif
	}
private:
	int m_nCUBEMAP_SPHERE_LEGACY;
#ifndef NDEBUG
	bool m_bCUBEMAP_SPHERE_LEGACY;
#endif
public:
	void SetCUBEMAP_SPHERE_LEGACY( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCUBEMAP_SPHERE_LEGACY = i;
#ifndef NDEBUG
		m_bCUBEMAP_SPHERE_LEGACY = true;
#endif
	}
	void SetCUBEMAP_SPHERE_LEGACY( bool i )
	{
		m_nCUBEMAP_SPHERE_LEGACY = i ? 1 : 0;
#ifndef NDEBUG
		m_bCUBEMAP_SPHERE_LEGACY = true;
#endif
	}
public:
	vertexlit_and_unlit_generic_ps20_Static_Index( )
	{
#ifndef NDEBUG
		m_bDETAILTEXTURE = false;
#endif // _DEBUG
		m_nDETAILTEXTURE = 0;
#ifndef NDEBUG
		m_bCUBEMAP = false;
#endif // _DEBUG
		m_nCUBEMAP = 0;
#ifndef NDEBUG
		m_bDIFFUSELIGHTING = false;
#endif // _DEBUG
		m_nDIFFUSELIGHTING = 0;
#ifndef NDEBUG
		m_bENVMAPMASK = false;
#endif // _DEBUG
		m_nENVMAPMASK = 0;
#ifndef NDEBUG
		m_bBASEALPHAENVMAPMASK = false;
#endif // _DEBUG
		m_nBASEALPHAENVMAPMASK = 0;
#ifndef NDEBUG
		m_bSELFILLUM = false;
#endif // _DEBUG
		m_nSELFILLUM = 0;
#ifndef NDEBUG
		m_bVERTEXCOLOR = false;
#endif // _DEBUG
		m_nVERTEXCOLOR = 0;
#ifndef NDEBUG
		m_bFLASHLIGHT = false;
#endif // _DEBUG
		m_nFLASHLIGHT = 0;
#ifndef NDEBUG
		m_bSELFILLUM_ENVMAPMASK_ALPHA = false;
#endif // _DEBUG
		m_nSELFILLUM_ENVMAPMASK_ALPHA = 0;
#ifndef NDEBUG
		m_bDETAIL_BLEND_MODE = false;
#endif // _DEBUG
		m_nDETAIL_BLEND_MODE = 0;
#ifndef NDEBUG
		m_bSEAMLESS_BASE = false;
#endif // _DEBUG
		m_nSEAMLESS_BASE = 0;
#ifndef NDEBUG
		m_bSEAMLESS_DETAIL = false;
#endif // _DEBUG
		m_nSEAMLESS_DETAIL = 0;
#ifndef NDEBUG
		m_bDISTANCEALPHA = false;
#endif // _DEBUG
		m_nDISTANCEALPHA = 0;
#ifndef NDEBUG
		m_bDISTANCEALPHAFROMDETAIL = false;
#endif // _DEBUG
		m_nDISTANCEALPHAFROMDETAIL = 0;
#ifndef NDEBUG
		m_bSOFT_MASK = false;
#endif // _DEBUG
		m_nSOFT_MASK = 0;
#ifndef NDEBUG
		m_bOUTLINE = false;
#endif // _DEBUG
		m_nOUTLINE = 0;
#ifndef NDEBUG
		m_bOUTER_GLOW = false;
#endif // _DEBUG
		m_nOUTER_GLOW = 0;
#ifndef NDEBUG
		m_bBLENDTINTBYBASEALPHA = false;
#endif // _DEBUG
		m_nBLENDTINTBYBASEALPHA = 0;
#ifndef NDEBUG
		m_bCUBEMAP_SPHERE_LEGACY = false;
#endif // _DEBUG
		m_nCUBEMAP_SPHERE_LEGACY = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bDETAILTEXTURE && m_bCUBEMAP && m_bDIFFUSELIGHTING && m_bENVMAPMASK && m_bBASEALPHAENVMAPMASK && m_bSELFILLUM && m_bVERTEXCOLOR && m_bFLASHLIGHT && m_bSELFILLUM_ENVMAPMASK_ALPHA && m_bDETAIL_BLEND_MODE && m_bSEAMLESS_BASE && m_bSEAMLESS_DETAIL && m_bDISTANCEALPHA && m_bDISTANCEALPHAFROMDETAIL && m_bSOFT_MASK && m_bOUTLINE && m_bOUTER_GLOW && m_bBLENDTINTBYBASEALPHA && m_bCUBEMAP_SPHERE_LEGACY;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 6 * m_nDETAILTEXTURE ) + ( 12 * m_nCUBEMAP ) + ( 24 * m_nDIFFUSELIGHTING ) + ( 48 * m_nENVMAPMASK ) + ( 96 * m_nBASEALPHAENVMAPMASK ) + ( 192 * m_nSELFILLUM ) + ( 384 * m_nVERTEXCOLOR ) + ( 768 * m_nFLASHLIGHT ) + ( 1536 * m_nSELFILLUM_ENVMAPMASK_ALPHA ) + ( 3072 * m_nDETAIL_BLEND_MODE ) + ( 30720 * m_nSEAMLESS_BASE ) + ( 61440 * m_nSEAMLESS_DETAIL ) + ( 122880 * m_nDISTANCEALPHA ) + ( 245760 * m_nDISTANCEALPHAFROMDETAIL ) + ( 491520 * m_nSOFT_MASK ) + ( 983040 * m_nOUTLINE ) + ( 1966080 * m_nOUTER_GLOW ) + ( 3932160 * m_nBLENDTINTBYBASEALPHA ) + ( 7864320 * m_nCUBEMAP_SPHERE_LEGACY ) + 0;
	}
};
#define shaderStaticTest_vertexlit_and_unlit_generic_ps20 psh_forgot_to_set_static_DETAILTEXTURE + psh_forgot_to_set_static_CUBEMAP + psh_forgot_to_set_static_DIFFUSELIGHTING + psh_forgot_to_set_static_ENVMAPMASK + psh_forgot_to_set_static_BASEALPHAENVMAPMASK + psh_forgot_to_set_static_SELFILLUM + psh_forgot_to_set_static_VERTEXCOLOR + psh_forgot_to_set_static_FLASHLIGHT + psh_forgot_to_set_static_SELFILLUM_ENVMAPMASK_ALPHA + psh_forgot_to_set_static_DETAIL_BLEND_MODE + psh_forgot_to_set_static_SEAMLESS_BASE + psh_forgot_to_set_static_SEAMLESS_DETAIL + psh_forgot_to_set_static_DISTANCEALPHA + psh_forgot_to_set_static_DISTANCEALPHAFROMDETAIL + psh_forgot_to_set_static_SOFT_MASK + psh_forgot_to_set_static_OUTLINE + psh_forgot_to_set_static_OUTER_GLOW + psh_forgot_to_set_static_BLENDTINTBYBASEALPHA + psh_forgot_to_set_static_CUBEMAP_SPHERE_LEGACY + 0
class vertexlit_and_unlit_generic_ps20_Dynamic_Index
{
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
private:
	int m_nLIGHTING_PREVIEW;
#ifndef NDEBUG
	bool m_bLIGHTING_PREVIEW;
#endif
public:
	void SetLIGHTING_PREVIEW( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nLIGHTING_PREVIEW = i;
#ifndef NDEBUG
		m_bLIGHTING_PREVIEW = true;
#endif
	}
	void SetLIGHTING_PREVIEW( bool i )
	{
		m_nLIGHTING_PREVIEW = i ? 1 : 0;
#ifndef NDEBUG
		m_bLIGHTING_PREVIEW = true;
#endif
	}
public:
	vertexlit_and_unlit_generic_ps20_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bPIXELFOGTYPE = false;
#endif // _DEBUG
		m_nPIXELFOGTYPE = 0;
#ifndef NDEBUG
		m_bLIGHTING_PREVIEW = false;
#endif // _DEBUG
		m_nLIGHTING_PREVIEW = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bPIXELFOGTYPE && m_bLIGHTING_PREVIEW; // && m_bSTATIC_LIGHT_LIGHTMAP
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nPIXELFOGTYPE ) + ( 2 * m_nLIGHTING_PREVIEW ) + 0;
	}
};
#define shaderDynamicTest_vertexlit_and_unlit_generic_ps20 psh_forgot_to_set_dynamic_PIXELFOGTYPE + psh_forgot_to_set_dynamic_LIGHTING_PREVIEW + 0
