#include "shaderlib/cshader.h"
class spritecard_ps20b_Static_Index
{
private:
	int m_nCONVERT_TO_SRGB;
#ifndef NDEBUG
	bool m_bCONVERT_TO_SRGB;
#endif
public:
	void SetCONVERT_TO_SRGB( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCONVERT_TO_SRGB = i;
#ifndef NDEBUG
		m_bCONVERT_TO_SRGB = true;
#endif
	}
	void SetCONVERT_TO_SRGB( bool i )
	{
		m_nCONVERT_TO_SRGB = i ? 1 : 0;
#ifndef NDEBUG
		m_bCONVERT_TO_SRGB = true;
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
	int m_nSEQUENCE_BLEND_MODE;
#ifndef NDEBUG
	bool m_bSEQUENCE_BLEND_MODE;
#endif
public:
	void SetSEQUENCE_BLEND_MODE( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nSEQUENCE_BLEND_MODE = i;
#ifndef NDEBUG
		m_bSEQUENCE_BLEND_MODE = true;
#endif
	}
	void SetSEQUENCE_BLEND_MODE( bool i )
	{
		m_nSEQUENCE_BLEND_MODE = i ? 1 : 0;
#ifndef NDEBUG
		m_bSEQUENCE_BLEND_MODE = true;
#endif
	}
private:
	int m_nADDBASETEXTURE2;
#ifndef NDEBUG
	bool m_bADDBASETEXTURE2;
#endif
public:
	void SetADDBASETEXTURE2( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nADDBASETEXTURE2 = i;
#ifndef NDEBUG
		m_bADDBASETEXTURE2 = true;
#endif
	}
	void SetADDBASETEXTURE2( bool i )
	{
		m_nADDBASETEXTURE2 = i ? 1 : 0;
#ifndef NDEBUG
		m_bADDBASETEXTURE2 = true;
#endif
	}
private:
	int m_nMAXLUMFRAMEBLEND1;
#ifndef NDEBUG
	bool m_bMAXLUMFRAMEBLEND1;
#endif
public:
	void SetMAXLUMFRAMEBLEND1( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMAXLUMFRAMEBLEND1 = i;
#ifndef NDEBUG
		m_bMAXLUMFRAMEBLEND1 = true;
#endif
	}
	void SetMAXLUMFRAMEBLEND1( bool i )
	{
		m_nMAXLUMFRAMEBLEND1 = i ? 1 : 0;
#ifndef NDEBUG
		m_bMAXLUMFRAMEBLEND1 = true;
#endif
	}
private:
	int m_nMAXLUMFRAMEBLEND2;
#ifndef NDEBUG
	bool m_bMAXLUMFRAMEBLEND2;
#endif
public:
	void SetMAXLUMFRAMEBLEND2( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMAXLUMFRAMEBLEND2 = i;
#ifndef NDEBUG
		m_bMAXLUMFRAMEBLEND2 = true;
#endif
	}
	void SetMAXLUMFRAMEBLEND2( bool i )
	{
		m_nMAXLUMFRAMEBLEND2 = i ? 1 : 0;
#ifndef NDEBUG
		m_bMAXLUMFRAMEBLEND2 = true;
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
	int m_nCOLORRAMP;
#ifndef NDEBUG
	bool m_bCOLORRAMP;
#endif
public:
	void SetCOLORRAMP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCOLORRAMP = i;
#ifndef NDEBUG
		m_bCOLORRAMP = true;
#endif
	}
	void SetCOLORRAMP( bool i )
	{
		m_nCOLORRAMP = i ? 1 : 0;
#ifndef NDEBUG
		m_bCOLORRAMP = true;
#endif
	}
private:
	int m_nANIMBLEND;
#ifndef NDEBUG
	bool m_bANIMBLEND;
#endif
public:
	void SetANIMBLEND( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nANIMBLEND = i;
#ifndef NDEBUG
		m_bANIMBLEND = true;
#endif
	}
	void SetANIMBLEND( bool i )
	{
		m_nANIMBLEND = i ? 1 : 0;
#ifndef NDEBUG
		m_bANIMBLEND = true;
#endif
	}
private:
	int m_nADDSELF;
#ifndef NDEBUG
	bool m_bADDSELF;
#endif
public:
	void SetADDSELF( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nADDSELF = i;
#ifndef NDEBUG
		m_bADDSELF = true;
#endif
	}
	void SetADDSELF( bool i )
	{
		m_nADDSELF = i ? 1 : 0;
#ifndef NDEBUG
		m_bADDSELF = true;
#endif
	}
private:
	int m_nDEPTHBLEND;
#ifndef NDEBUG
	bool m_bDEPTHBLEND;
#endif
public:
	void SetDEPTHBLEND( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDEPTHBLEND = i;
#ifndef NDEBUG
		m_bDEPTHBLEND = true;
#endif
	}
	void SetDEPTHBLEND( bool i )
	{
		m_nDEPTHBLEND = i ? 1 : 0;
#ifndef NDEBUG
		m_bDEPTHBLEND = true;
#endif
	}
public:
	spritecard_ps20b_Static_Index( )
	{
#ifndef NDEBUG
		m_bCONVERT_TO_SRGB = true;
#endif  // !NDEBUG
		m_nCONVERT_TO_SRGB =  g_pHardwareConfig->NeedsShaderSRGBConversion();
#ifndef NDEBUG
		m_bDUALSEQUENCE = false;
#endif  // !NDEBUG
		m_nDUALSEQUENCE = 0;
#ifndef NDEBUG
		m_bSEQUENCE_BLEND_MODE = false;
#endif  // !NDEBUG
		m_nSEQUENCE_BLEND_MODE = 0;
#ifndef NDEBUG
		m_bADDBASETEXTURE2 = false;
#endif  // !NDEBUG
		m_nADDBASETEXTURE2 = 0;
#ifndef NDEBUG
		m_bMAXLUMFRAMEBLEND1 = false;
#endif  // !NDEBUG
		m_nMAXLUMFRAMEBLEND1 = 0;
#ifndef NDEBUG
		m_bMAXLUMFRAMEBLEND2 = false;
#endif  // !NDEBUG
		m_nMAXLUMFRAMEBLEND2 = 0;
#ifndef NDEBUG
		m_bEXTRACTGREENALPHA = false;
#endif  // !NDEBUG
		m_nEXTRACTGREENALPHA = 0;
#ifndef NDEBUG
		m_bCOLORRAMP = false;
#endif  // !NDEBUG
		m_nCOLORRAMP = 0;
#ifndef NDEBUG
		m_bANIMBLEND = false;
#endif  // !NDEBUG
		m_nANIMBLEND = 0;
#ifndef NDEBUG
		m_bADDSELF = false;
#endif  // !NDEBUG
		m_nADDSELF = 0;
#ifndef NDEBUG
		m_bDEPTHBLEND = false;
#endif  // !NDEBUG
		m_nDEPTHBLEND = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifndef NDEBUG
		bool bAllStaticVarsDefined = m_bCONVERT_TO_SRGB && m_bDUALSEQUENCE && m_bSEQUENCE_BLEND_MODE && m_bADDBASETEXTURE2 && m_bMAXLUMFRAMEBLEND1 && m_bMAXLUMFRAMEBLEND2 && m_bEXTRACTGREENALPHA && m_bCOLORRAMP && m_bANIMBLEND && m_bADDSELF && m_bDEPTHBLEND;
		Assert( bAllStaticVarsDefined );
#endif  // !NDEBUG
		return ( 1 * m_nCONVERT_TO_SRGB ) + ( 2 * m_nDUALSEQUENCE ) + ( 4 * m_nSEQUENCE_BLEND_MODE ) + ( 12 * m_nADDBASETEXTURE2 ) + ( 24 * m_nMAXLUMFRAMEBLEND1 ) + ( 48 * m_nMAXLUMFRAMEBLEND2 ) + ( 96 * m_nEXTRACTGREENALPHA ) + ( 192 * m_nCOLORRAMP ) + ( 384 * m_nANIMBLEND ) + ( 768 * m_nADDSELF ) + ( 1536 * m_nDEPTHBLEND ) + 0;
	}
};
#define shaderStaticTest_spritecard_ps20b psh_forgot_to_set_static_DUALSEQUENCE + psh_forgot_to_set_static_SEQUENCE_BLEND_MODE + psh_forgot_to_set_static_ADDBASETEXTURE2 + psh_forgot_to_set_static_MAXLUMFRAMEBLEND1 + psh_forgot_to_set_static_MAXLUMFRAMEBLEND2 + psh_forgot_to_set_static_EXTRACTGREENALPHA + psh_forgot_to_set_static_COLORRAMP + psh_forgot_to_set_static_ANIMBLEND + psh_forgot_to_set_static_ADDSELF + psh_forgot_to_set_static_DEPTHBLEND + 0
class spritecard_ps20b_Dynamic_Index
{
public:
	spritecard_ps20b_Dynamic_Index()
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
#define shaderDynamicTest_spritecard_ps20b 0
