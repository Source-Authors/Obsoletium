#include "shaderlib/cshader.h"
class vertexlit_and_unlit_generic_bump_ps20_Static_Index
{
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
	int m_nLIGHTWARPTEXTURE;
#ifndef NDEBUG
	bool m_bLIGHTWARPTEXTURE;
#endif
public:
	void SetLIGHTWARPTEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nLIGHTWARPTEXTURE = i;
#ifndef NDEBUG
		m_bLIGHTWARPTEXTURE = true;
#endif
	}
	void SetLIGHTWARPTEXTURE( bool i )
	{
		m_nLIGHTWARPTEXTURE = i ? 1 : 0;
#ifndef NDEBUG
		m_bLIGHTWARPTEXTURE = true;
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
	int m_nSELFILLUMFRESNEL;
#ifndef NDEBUG
	bool m_bSELFILLUMFRESNEL;
#endif
public:
	void SetSELFILLUMFRESNEL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSELFILLUMFRESNEL = i;
#ifndef NDEBUG
		m_bSELFILLUMFRESNEL = true;
#endif
	}
	void SetSELFILLUMFRESNEL( bool i )
	{
		m_nSELFILLUMFRESNEL = i ? 1 : 0;
#ifndef NDEBUG
		m_bSELFILLUMFRESNEL = true;
#endif
	}
private:
	int m_nNORMALMAPALPHAENVMAPMASK;
#ifndef NDEBUG
	bool m_bNORMALMAPALPHAENVMAPMASK;
#endif
public:
	void SetNORMALMAPALPHAENVMAPMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nNORMALMAPALPHAENVMAPMASK = i;
#ifndef NDEBUG
		m_bNORMALMAPALPHAENVMAPMASK = true;
#endif
	}
	void SetNORMALMAPALPHAENVMAPMASK( bool i )
	{
		m_nNORMALMAPALPHAENVMAPMASK = i ? 1 : 0;
#ifndef NDEBUG
		m_bNORMALMAPALPHAENVMAPMASK = true;
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
	int m_nDETAIL_BLEND_MODE;
#ifndef NDEBUG
	bool m_bDETAIL_BLEND_MODE;
#endif
public:
	void SetDETAIL_BLEND_MODE( int i )
	{
		Assert( i >= 0 && i <= 6 );
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
public:
	vertexlit_and_unlit_generic_bump_ps20_Static_Index( )
	{
#ifndef NDEBUG
		m_bCUBEMAP = false;
#endif // _DEBUG
		m_nCUBEMAP = 0;
#ifndef NDEBUG
		m_bDIFFUSELIGHTING = false;
#endif // _DEBUG
		m_nDIFFUSELIGHTING = 0;
#ifndef NDEBUG
		m_bLIGHTWARPTEXTURE = false;
#endif // _DEBUG
		m_nLIGHTWARPTEXTURE = 0;
#ifndef NDEBUG
		m_bSELFILLUM = false;
#endif // _DEBUG
		m_nSELFILLUM = 0;
#ifndef NDEBUG
		m_bSELFILLUMFRESNEL = false;
#endif // _DEBUG
		m_nSELFILLUMFRESNEL = 0;
#ifndef NDEBUG
		m_bNORMALMAPALPHAENVMAPMASK = false;
#endif // _DEBUG
		m_nNORMALMAPALPHAENVMAPMASK = 0;
#ifndef NDEBUG
		m_bHALFLAMBERT = false;
#endif // _DEBUG
		m_nHALFLAMBERT = 0;
#ifndef NDEBUG
		m_bFLASHLIGHT = false;
#endif // _DEBUG
		m_nFLASHLIGHT = 0;
#ifndef NDEBUG
		m_bDETAILTEXTURE = false;
#endif // _DEBUG
		m_nDETAILTEXTURE = 0;
#ifndef NDEBUG
		m_bDETAIL_BLEND_MODE = false;
#endif // _DEBUG
		m_nDETAIL_BLEND_MODE = 0;
#ifndef NDEBUG
		m_bBLENDTINTBYBASEALPHA = false;
#endif // _DEBUG
		m_nBLENDTINTBYBASEALPHA = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bCUBEMAP && m_bDIFFUSELIGHTING && m_bLIGHTWARPTEXTURE && m_bSELFILLUM && m_bSELFILLUMFRESNEL && m_bNORMALMAPALPHAENVMAPMASK && m_bHALFLAMBERT && m_bFLASHLIGHT && m_bDETAILTEXTURE && m_bDETAIL_BLEND_MODE && m_bBLENDTINTBYBASEALPHA;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 24 * m_nCUBEMAP ) + ( 48 * m_nDIFFUSELIGHTING ) + ( 96 * m_nLIGHTWARPTEXTURE ) + ( 192 * m_nSELFILLUM ) + ( 384 * m_nSELFILLUMFRESNEL ) + ( 768 * m_nNORMALMAPALPHAENVMAPMASK ) + ( 1536 * m_nHALFLAMBERT ) + ( 3072 * m_nFLASHLIGHT ) + ( 6144 * m_nDETAILTEXTURE ) + ( 12288 * m_nDETAIL_BLEND_MODE ) + ( 86016 * m_nBLENDTINTBYBASEALPHA ) + 0;
	}
};
#define shaderStaticTest_vertexlit_and_unlit_generic_bump_ps20 psh_forgot_to_set_static_CUBEMAP + psh_forgot_to_set_static_DIFFUSELIGHTING + psh_forgot_to_set_static_LIGHTWARPTEXTURE + psh_forgot_to_set_static_SELFILLUM + psh_forgot_to_set_static_SELFILLUMFRESNEL + psh_forgot_to_set_static_NORMALMAPALPHAENVMAPMASK + psh_forgot_to_set_static_HALFLAMBERT + psh_forgot_to_set_static_FLASHLIGHT + psh_forgot_to_set_static_DETAILTEXTURE + psh_forgot_to_set_static_DETAIL_BLEND_MODE + psh_forgot_to_set_static_BLENDTINTBYBASEALPHA + 0
class vertexlit_and_unlit_generic_bump_ps20_Dynamic_Index
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
	int m_nNUM_LIGHTS;
#ifndef NDEBUG
	bool m_bNUM_LIGHTS;
#endif
public:
	void SetNUM_LIGHTS( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nNUM_LIGHTS = i;
#ifndef NDEBUG
		m_bNUM_LIGHTS = true;
#endif
	}
	void SetNUM_LIGHTS( bool i )
	{
		m_nNUM_LIGHTS = i ? 1 : 0;
#ifndef NDEBUG
		m_bNUM_LIGHTS = true;
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
public:
	vertexlit_and_unlit_generic_bump_ps20_Dynamic_Index()
	{
#ifndef NDEBUG
		m_bPIXELFOGTYPE = false;
#endif // _DEBUG
		m_nPIXELFOGTYPE = 0;
#ifndef NDEBUG
		m_bWRITEWATERFOGTODESTALPHA = false;
#endif // _DEBUG
		m_nWRITEWATERFOGTODESTALPHA = 0;
#ifndef NDEBUG
		m_bNUM_LIGHTS = false;
#endif // _DEBUG
		m_nNUM_LIGHTS = 0;
#ifndef NDEBUG
		m_bAMBIENT_LIGHT = false;
#endif // _DEBUG
		m_nAMBIENT_LIGHT = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllDynamicVarsDefined = m_bPIXELFOGTYPE && m_bWRITEWATERFOGTODESTALPHA && m_bNUM_LIGHTS && m_bAMBIENT_LIGHT;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nPIXELFOGTYPE ) + ( 2 * m_nWRITEWATERFOGTODESTALPHA ) + ( 4 * m_nNUM_LIGHTS ) + ( 12 * m_nAMBIENT_LIGHT ) + 0;
	}
};
#define shaderDynamicTest_vertexlit_and_unlit_generic_bump_ps20 psh_forgot_to_set_dynamic_PIXELFOGTYPE + psh_forgot_to_set_dynamic_WRITEWATERFOGTODESTALPHA + psh_forgot_to_set_dynamic_NUM_LIGHTS + psh_forgot_to_set_dynamic_AMBIENT_LIGHT + 0
