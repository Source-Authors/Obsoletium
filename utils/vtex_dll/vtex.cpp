// Copyright Valve Corporation, All rights reserved.
//
// Command-line tool used to convert targa (.tga), portable floatmap (.pfm), or
// Photoshop (.psd) (Orange box SDK) image files into Valve Texture Files
// (.vtf) for use in the  Source engine.
//
// See https://developer.valvesoftware.com/wiki/VTEX_(Valve_Texture_Tool)

#include "tier1/strtools.h"
#include "bitmap/bitmap.h"
#include "bitmap/tgaloader.h"
#include "bitmap/psd.h"
#include "bitmap/float_bm.h"
#include "bitmap/imageformat.h"
#include "mathlib/mathlib.h"

#ifdef POSIX
#include <sys/stat.h>
#define _stat stat
#endif

#ifdef WIN32
#include "conio.h"
#include <direct.h>
#include <io.h>
#endif

#include "vtf/vtf.h"
#include "utlbuffer.h"
#include "tier0/dbg.h"
#include "cmdlib.h"
#include "tier0/icommandline.h"
#ifdef WIN32
#include "winlite.h"
#endif
#include "ilaunchabledll.h"
#include "ivtex.h"
#include "appframework/IAppSystemGroup.h"

#include "tier2/tier2.h"
#include "tier2/p4helpers.h"
// dimhotepus: Drop Perforce support.
// #include "p4lib/ip4.h"
#include "tools_minidump.h"
#include "posix_file_stream.h"

#include "tier1/checksum_crc.h"
#include <system_error>

#include "scoped_app_locale.h"

#define FF_TRYAGAIN 1
#define FF_DONTPROCESS 2

#define LOWRESIMAGE_DIM 16

#ifdef POSIX
#define LOWRES_IMAGE_FORMAT IMAGE_FORMAT_RGBA8888
#else
#define LOWRES_IMAGE_FORMAT IMAGE_FORMAT_DXT1
#endif

//#define DEBUG_NO_COMPRESSION
static bool g_NoPause = false;
static bool g_Quiet = false;
static const char *g_ShaderName = NULL;
static bool g_CreateDir = true;
static bool g_UseGameDir = true;

static bool g_bUseStandardError = false;
static bool g_bWarningsAsErrors = false;

static bool g_bUsedAsLaunchableDLL = false;

static bool g_bNoTga = false;
static bool g_bNoPsd = false;

static char g_ForcedOutputDir[MAX_PATH];


#define MAX_VMT_PARAMS	16

struct VTexVMTParam_t
{
	const char *m_szParam;
	const char *m_szValue;
};

class SmartIVtfTexture
{
public:
	explicit SmartIVtfTexture( IVTFTexture *pVtf ) : m_p( pVtf ) {}
	~SmartIVtfTexture() { if ( m_p ) DestroyVTFTexture( m_p ); }

	SmartIVtfTexture( SmartIVtfTexture const &x ) = delete;
	SmartIVtfTexture & operator = ( SmartIVtfTexture const &x ) = delete;

	IVTFTexture * Assign( IVTFTexture *pVtfNew ) { IVTFTexture *pOld = m_p; m_p = pVtfNew; return pOld; }
	IVTFTexture * Get() const { return m_p; }
	IVTFTexture * operator->() const { return m_p; }

private:
	IVTFTexture *m_p;

	SmartIVtfTexture & operator = ( IVTFTexture *pVtf ) { m_p = pVtf; return *this; }
	operator IVTFTexture * () const { return m_p; }
};

static VTexVMTParam_t g_VMTParams[MAX_VMT_PARAMS];

static int g_NumVMTParams = 0;
// dimhotepus: Make scoped enum.
static enum class Mode { eModePSD, eModeTGA, eModePFM } g_eMode = Mode::eModePSD;

// NOTE: these must stay in the same order as CubeMapFaceIndex_t.
static const char *g_CubemapFacingNames[7] = { "rt", "lf", "bk", "ft", "up", "dn", "sph" };

static void Pause()
{
	if( !g_NoPause )
	{
		printf( "Hit a key to continue\n" );
#ifdef WIN32
		getch();
#endif	
	}
}

static bool VTexErrorAborts()
{

	// dimhotepus: Use simple HasParm API.
	return !CommandLine()->HasParm( "-crcvalidate" );
}


static void VTexError( const char *pFormat, ... )
{
	char str[4096];
	va_list marker;
	va_start( marker, pFormat );
	V_vsprintf_safe( str, pFormat, marker );
	va_end( marker );

	if ( !VTexErrorAborts() )
	{
		fprintf( stderr, "ERROR: %s", str );
		return;
	}

	if ( g_bUseStandardError )
	{
		Error( "ERROR: %s", str );
	}
	else
	{
		fprintf( stderr, "ERROR: %s", str );
		Pause();
		// dimhotepus: 1 -> EOTHER
		exit(EOTHER);
	}
}


static void VTexWarning( const char *pFormat, ... )
{
	char str[4096];
	va_list marker;
	va_start( marker, pFormat );
	V_vsprintf_safe( str, pFormat, marker );
	va_end( marker );

	if ( g_bWarningsAsErrors )
	{
		VTexError( "%s", str );
	}
	else
	{
		fprintf( stderr, "WARN: %s", str );
		Pause();
	}	
}



struct VTexConfigInfo_t
{
	int m_nStartFrame;
	int m_nEndFrame;
	unsigned int m_nFlags;
	float m_flBumpScale;
	LookDir_t m_LookDir;
	bool m_bNormalToDuDv;
	bool m_bAlphaToLuminance;
	bool m_bDuDv;
	float m_flAlphaThreshhold;
	float m_flAlphaHiFreqThreshhold;
	bool m_bSkyBox;
	int m_nVolumeTextureDepth;
	float m_pfmscale;
	bool m_bStripAlphaChannel;
	bool m_bStripColorChannel;
	bool m_bIsCubeMap;


	// scaling parameters
	int m_nReduceX;
	int m_nReduceY;

	int m_nMaxDimensionX, m_nMaxDimensionX_360;
	int m_nMaxDimensionY, m_nMaxDimensionY_360;

	// may restrict the texture to reading only 3 channels
	int m_numChannelsMax;

	bool m_bAlphaToDistance;
	float m_flDistanceSpread;					 // how far to stretch out distance range in pixels

	CRC32_t m_uiInputHash;	// Sources hash

	TextureSettingsEx_t m_exSettings0;
	VtfProcessingOptions m_vtfProcOptions;

	enum
	{
		// CRC of input files:
		//  txt + tga/pfm
		// or
		//  psd
		VTF_INPUTSRC_CRC = MK_VTF_RSRC_ID( 'C','R','C' )
	};

	char m_SrcName[MAX_PATH];

	VTexConfigInfo_t( void )
	{
		m_nStartFrame = -1;
		m_nEndFrame = -1;
		m_nFlags = 0;
		m_flBumpScale = 1.0f;
		m_LookDir = LOOK_DOWN_X;
		m_bNormalToDuDv = false;
		m_bAlphaToLuminance = false;
		m_bDuDv = false;
		m_flAlphaThreshhold = -1.0f;
		m_flAlphaHiFreqThreshhold = -1.0f;
		m_bSkyBox = false;
		m_nVolumeTextureDepth = 1;
		m_pfmscale=1.0;
		m_bStripAlphaChannel = false;
		m_bStripColorChannel = false;
		m_bIsCubeMap = false;
		m_nReduceX = 1;
		m_nReduceY = 1;
		m_SrcName[0]=0;
		m_numChannelsMax = 4;
		m_bAlphaToDistance = 0;
		m_flDistanceSpread = 1.0;
		m_nMaxDimensionX = -1;
		m_nMaxDimensionX_360 = -1;
		m_nMaxDimensionY = -1;
		m_nMaxDimensionY_360 = -1;

		memset( &m_exSettings0, 0, sizeof( m_exSettings0 ) );

		memset( &m_vtfProcOptions, 0, sizeof( m_vtfProcOptions ) );
		m_vtfProcOptions.cbSize = sizeof( m_vtfProcOptions );
		
		m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_FILTER_NICE;

		CRC32_Init( &m_uiInputHash );
	}

	bool IsSettings0Valid( void ) const
	{
		TextureSettingsEx_t exSettingsEmpty = {};
		Assert( sizeof( m_exSettings0 ) == sizeof( exSettingsEmpty ) );
		return !!memcmp( &m_exSettings0, &exSettingsEmpty, sizeof( m_exSettings0 ) );
	}

	// returns false if unrecognized option
	void ParseOptionKey( const char *pKeyName,  const char *pKeyValue );

};

template < typename T >
static inline T& SetFlagValueT( T &field, T const &flag, int bSetFlag )
{
	if ( bSetFlag )
		field |= flag;
	else
		field &= ~flag;

	return field;
}

static inline uint32& SetFlagValue( uint32 &field, uint32 const &flag, int bSetFlag )
{
	return SetFlagValueT<uint32>( field, flag, bSetFlag );
}

void VTexConfigInfo_t::ParseOptionKey( const char *pKeyName,  const char *pKeyValue )
{
	int iValue = atoi( pKeyValue ); // To properly have "clamps 0" and not enable the clamping

	if ( !stricmp( pKeyName, "skybox" ) )
	{
		// We're going to treat it like a cubemap until the very end, so it'll load the other skybox faces and
		// match their edges with the texture compression and mipmapping.
		m_bSkyBox = iValue ? true : false;
		m_bIsCubeMap = iValue ? true : false;

		if ( !g_Quiet && iValue )
			Msg( "'skybox' detected. Treating skybox like a cubemap for edge-matching purposes.\n" );
	}
	else if( !stricmp( pKeyName, "startframe" ) )
	{
		m_nStartFrame = atoi( pKeyValue );
	}
	else if( !stricmp( pKeyName, "endframe" ) )
	{
		m_nEndFrame = atoi( pKeyValue );
	}
	else if( !stricmp( pKeyName, "volumetexture" ) )
	{
		m_nVolumeTextureDepth = atoi( pKeyValue );

		// FIXME: Volume textures don't currently support DXT compression
		m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_NOCOMPRESS;

		// FIXME: Volume textures don't currently support NICE filtering
		m_vtfProcOptions.flags0 &= ~VtfProcessingOptions::OPT_FILTER_NICE;
	}
	else if( !stricmp( pKeyName, "spheremap_x" ) )
	{
		if ( iValue )
			m_LookDir = LOOK_DOWN_X;
	}
	else if( !stricmp( pKeyName, "spheremap_negx" ) )
	{
		if ( iValue )
			m_LookDir = LOOK_DOWN_NEGX;
	}
	else if( !stricmp( pKeyName, "spheremap_y" ) )
	{
		if ( iValue )
			m_LookDir = LOOK_DOWN_Y;
	}
	else if( !stricmp( pKeyName, "spheremap_negy" ) )
	{
		if ( iValue )
			m_LookDir = LOOK_DOWN_NEGY;
	}
	else if( !stricmp( pKeyName, "spheremap_z" ) )
	{
		if ( iValue )
			m_LookDir = LOOK_DOWN_Z;
	}
	else if( !stricmp( pKeyName, "spheremap_negz" ) )
	{
		if ( iValue )
			m_LookDir = LOOK_DOWN_NEGZ;
	}
	else if( !stricmp( pKeyName, "bumpscale" ) )
	{
		// dimhotepus: atof -> strtof.
		m_flBumpScale = strtof( pKeyValue, nullptr );
	}
	else if( !stricmp( pKeyName, "pointsample" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_POINTSAMPLE, iValue );
	}
	else if( !stricmp( pKeyName, "trilinear" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_TRILINEAR, iValue );
	}
	else if( !stricmp( pKeyName, "clamps" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_CLAMPS, iValue );
	}
	else if( !stricmp( pKeyName, "clampt" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_CLAMPT, iValue );
	}
	else if( !stricmp( pKeyName, "clampu" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_CLAMPU, iValue );
	}
	else if( !stricmp( pKeyName, "border" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_BORDER, iValue );
		// Gets applied to s, t and u   We currently assume black border color
	}
	else if( !stricmp( pKeyName, "anisotropic" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_ANISOTROPIC, iValue );
	}
	else if( !stricmp( pKeyName, "dxt5" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_HINT_DXT5, iValue );
	}
	else if( !stricmp( pKeyName, "nocompress" ) )
	{
		SetFlagValue( m_vtfProcOptions.flags0, VtfProcessingOptions::OPT_NOCOMPRESS, iValue );
	}
	else if( !stricmp( pKeyName, "normal" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_NORMAL, iValue );
	}
	else if( !stricmp( pKeyName, "ssbump" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_SSBUMP, iValue );
	}
	else if( !stricmp( pKeyName, "nomip" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_NOMIP, iValue );
	}
	else if( !stricmp( pKeyName, "allmips" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_ALL_MIPS, iValue );
	}
	else if( !stricmp( pKeyName, "nonice" ) )
	{
		SetFlagValue( m_vtfProcOptions.flags0, VtfProcessingOptions::OPT_FILTER_NICE, !iValue );
	}
	else if( !stricmp( pKeyName, "nolod" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_NOLOD, iValue );
	}
	else if( !stricmp( pKeyName, "procedural" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_PROCEDURAL, iValue );
	}
	else if( !stricmp( pKeyName, "alphatest" ) )
	{
		SetFlagValue( m_vtfProcOptions.flags0, VtfProcessingOptions::OPT_MIP_ALPHATEST, iValue );
	}
	else if( !stricmp( pKeyName, "alphatest_threshhold" ) )
	{
		// dimhotepus: atof -> strtof.
		m_flAlphaThreshhold = strtof( pKeyValue, nullptr );
	}
	else if( !stricmp( pKeyName, "alphatest_hifreq_threshhold" ) )
	{
		// dimhotepus: atof -> strtof.
		m_flAlphaHiFreqThreshhold = strtof( pKeyValue, nullptr );
	}
	else if( !stricmp( pKeyName, "rendertarget" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_RENDERTARGET, iValue );
	}
	else if ( !stricmp( pKeyName, "numchannels" ) )
	{
		m_numChannelsMax = atoi( pKeyValue );
	}
	else if ( !stricmp( pKeyName, "nodebug" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_NODEBUGOVERRIDE, iValue );
	}
	else if ( !stricmp( pKeyName, "singlecopy" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_SINGLECOPY, iValue );
	}
	else if( !stricmp( pKeyName, "oneovermiplevelinalpha" ) )
	{
		SetFlagValue( m_vtfProcOptions.flags0, VtfProcessingOptions::OPT_SET_ALPHA_ONEOVERMIP, iValue );
	}
	else if( !stricmp( pKeyName, "premultcolorbyoneovermiplevel" ) )
	{
		SetFlagValue( m_vtfProcOptions.flags0, VtfProcessingOptions::OPT_PREMULT_COLOR_ONEOVERMIP, iValue );
	}
	else if ( !stricmp( pKeyName, "normaltodudv" ) )
	{
		m_bNormalToDuDv = iValue ? true : false;
		SetFlagValue( m_vtfProcOptions.flags0, VtfProcessingOptions::OPT_NORMAL_DUDV, iValue );
	}
	else if ( !stricmp( pKeyName, "stripalphachannel" ) )
	{
		m_bStripAlphaChannel = iValue ? true : false;
	}
	else if ( !stricmp( pKeyName, "stripcolorchannel" ) )
	{
		m_bStripColorChannel = iValue ? true : false;
	}
	else if ( !stricmp( pKeyName, "normalalphatodudvluminance" ) )
	{
		m_bAlphaToLuminance = iValue ? true : false;
	}
	else if ( !stricmp( pKeyName, "dudv" ) )
	{
		m_bDuDv = iValue ? true : false;
	}
	else if( !stricmp( pKeyName, "reduce" ) )
	{
		m_nReduceX = atoi(pKeyValue);
		m_nReduceY = m_nReduceX;
	}
	else if( !stricmp( pKeyName, "reducex" ) )
	{
		m_nReduceX = atoi(pKeyValue);
	}
	else if( !stricmp( pKeyName, "reducey" ) )
	{
		m_nReduceY = atoi(pKeyValue);
	}
	else if( !stricmp( pKeyName, "maxwidth" ) )
	{
		m_nMaxDimensionX = atoi(pKeyValue);
	}
	else if( !stricmp( pKeyName, "maxwidth_360" ) )
	{
		m_nMaxDimensionX_360 = atoi(pKeyValue);
	}
	else if( !stricmp( pKeyName, "maxheight" ) )
	{
		m_nMaxDimensionY = atoi(pKeyValue);
	}
	else if( !stricmp( pKeyName, "maxheight_360" ) )
	{
		m_nMaxDimensionY_360 = atoi(pKeyValue);
	}
	else if( !stricmp( pKeyName, "alphatodistance" ) )
	{
		m_bAlphaToDistance = iValue ? true : false;
	}
	else if( !stricmp( pKeyName, "distancespread" ) )
	{
		// dimhotepus: atof -> strtof.
		m_flDistanceSpread = strtof(pKeyValue, nullptr);
	}
	else if( !stricmp( pKeyName, "pfmscale" ) )
	{
		// dimhotepus: atof -> strtof.
		m_pfmscale = strtof(pKeyValue, nullptr);
		printf("******pfm scale=%f\n",m_pfmscale);
	}
	else if ( !stricmp( pKeyName, "pfm" ) )
	{
		if ( iValue )
			g_eMode = Mode::eModePFM;
	}
	else if ( !stricmp( pKeyName, "specvar" ) )
	{
		int iDecayChannel = -1;

		if ( !stricmp( pKeyValue, "red" ) || !stricmp( pKeyValue, "r" ) )
			iDecayChannel = 0;
		if ( !stricmp( pKeyValue, "green" ) || !stricmp( pKeyValue, "g" ) )
			iDecayChannel = 1;
		if ( !stricmp( pKeyValue, "blue" ) || !stricmp( pKeyValue, "b" ) )
			iDecayChannel = 2;
		if ( !stricmp( pKeyValue, "alpha" ) || !stricmp( pKeyValue, "a" ) )
			iDecayChannel = 3;

		if ( iDecayChannel >= 0 )
		{
			m_vtfProcOptions.flags0 |= ( VtfProcessingOptions::OPT_DECAY_R | VtfProcessingOptions::OPT_DECAY_EXP_R ) << iDecayChannel;
			m_vtfProcOptions.numNotDecayMips[iDecayChannel] = 0;
			m_vtfProcOptions.clrDecayGoal[iDecayChannel] = 0;
			m_vtfProcOptions.fDecayExponentBase[iDecayChannel] = 0.75;
			SetFlagValue( m_nFlags, TEXTUREFLAGS_ALL_MIPS, 1 );
		}
	}
	else if ( !stricmp( pKeyName, "mipblend" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_ALL_MIPS, 1 );

		// Possible values
		if ( !stricmp( pKeyValue, "detail" ) ) // Skip 2 mips and fade to gray -> (128, 128, 128, -)
		{
			for( int ch = 0; ch < 3; ++ ch )
			{
				m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_DECAY_R << ch;
				// m_vtfProcOptions.flags0 &= ~(VtfProcessingOptions::OPT_DECAY_EXP_R << ch);
				m_vtfProcOptions.numNotDecayMips[ch] = 2;
				m_vtfProcOptions.clrDecayGoal[ch] = 128;
			}
		}
		/*
		else if ( !stricmp( pKeyValue, "additive" ) ) // Skip 2 mips and fade to black -> (0, 0, 0, -)
		{
			for( int ch = 0; ch < 3; ++ ch )
			{
				m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_DECAY_R << ch;
				m_vtfProcOptions.flags0 &= ~(VtfProcessingOptions::OPT_DECAY_EXP_R << ch);
				m_vtfProcOptions.numDecayMips[ch] = 2;
				m_vtfProcOptions.clrDecayGoal[ch] = 0;
			}
		}
		else if ( !stricmp( pKeyValue, "alphablended" ) ) // Skip 2 mips and fade out alpha to 0
		{
			for( int ch = 3; ch < 4; ++ ch )
			{
				m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_DECAY_R << ch;
				m_vtfProcOptions.flags0 &= ~(VtfProcessingOptions::OPT_DECAY_EXP_R << ch);
				m_vtfProcOptions.numDecayMips[ch] = 2;
				m_vtfProcOptions.clrDecayGoal[ch] = 0;
			}
		}
		*/
		else
		{
			// Parse the given value:
			// skip=3:r=255:g=255:b=255:a=255  - linear decay
			// r=0e.75 - exponential decay targeting 0 with exponent base 0.75

			int nSteps = 0; // default
			
			for ( char const *szParse = pKeyValue; szParse; szParse = strchr( szParse, ':' ), szParse ? ++ szParse : 0 )
			{
				if ( char const *sz = StringAfterPrefix( szParse, "skip=" ) )
				{
					szParse = sz;
					nSteps = atoi(sz);
				}
				else if ( StringHasPrefix( szParse, "r=" ) ||
					      StringHasPrefix( szParse, "g=" ) ||
						  StringHasPrefix( szParse, "b=" ) ||
						  StringHasPrefix( szParse, "a=" ) )
				{
					int ch = 0;
					switch ( *szParse )
					{
					case 'g': case 'G': ch = 1; break;
					case 'b': case 'B': ch = 2; break;
					case 'a': case 'A': ch = 3; break;
					}
					
					szParse += 2;
					m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_DECAY_R << ch;
					m_vtfProcOptions.flags0 &= ~(VtfProcessingOptions::OPT_DECAY_EXP_R << ch);
					m_vtfProcOptions.numNotDecayMips[ch] = nSteps;
					m_vtfProcOptions.clrDecayGoal[ch] = atoi( szParse );
					
					while ( isdigit( *szParse ) )
						++ szParse;

					// Exponential decay
					if ( ( *szParse == 'e' || *szParse == 'E' ) && ( szParse[1] == '.' ) )
					{
						m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_DECAY_EXP_R << ch;
						// dimhotepus: atof -> V_atof
						m_vtfProcOptions.fDecayExponentBase[ch] = V_atof( szParse + 1 );
					}
				}
				else
				{
					VTexWarning( "Warning: invalid mipblend setting '%s'.\n", pKeyValue );
				}
			}
		}
	}
	else if( !stricmp( pKeyName, "srgb" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_SRGB, iValue );
	}
	else
	{
		VTexError("unrecognized option in text file - %s\n", pKeyName );
	}
}




static const char *GetSourceExtension( void )
{
	switch ( g_eMode )
	{
    case Mode::eModePSD:
		return ".psd";
    case Mode::eModeTGA:
		return ".tga";
    case Mode::eModePFM:
		return ".pfm";
	default:
		return ".tga";
	}
}


//-----------------------------------------------------------------------------
// Computes the desired texture format based on flags
//-----------------------------------------------------------------------------
static ImageFormat ComputeDesiredImageFormat( IVTFTexture *pTexture, VTexConfigInfo_t &info )
{
	bool bDUDVTarget = info.m_bNormalToDuDv || info.m_bDuDv;
	bool bCopyAlphaToLuminance = info.m_bNormalToDuDv && info.m_bAlphaToLuminance;

	ImageFormat targetFormat;

	int nFlags = pTexture->Flags();
	if ( info.m_bStripAlphaChannel )
	{
		nFlags &= ~( TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA );
	}

	// HDRFIXME: Need to figure out what format to use here.
	if ( pTexture->Format() == IMAGE_FORMAT_RGB323232F )
	{
#ifndef DEBUG_NO_COMPRESSION
		if ( g_bUsedAsLaunchableDLL && !( info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_NOCOMPRESS ) )
		{
			return IMAGE_FORMAT_BGRA8888;
		}
		else
#endif // #ifndef DEBUG_NO_COMPRESSION
		{
			return IMAGE_FORMAT_RGBA16161616F;
		}
	}

	if ( bDUDVTarget )
	{
		if ( bCopyAlphaToLuminance && ( nFlags & ( TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA ) ) )
			return IMAGE_FORMAT_UVLX8888;
		return IMAGE_FORMAT_UV88;
	}

	if ( info.m_bStripColorChannel )
	{
		return IMAGE_FORMAT_A8;
	}

	// can't compress textures that are smaller than 4x4
	if( (nFlags & TEXTUREFLAGS_PROCEDURAL) ||
		(info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_NOCOMPRESS) ||
		(pTexture->Width() < 4) || (pTexture->Height() < 4) )
	{
		if ( nFlags & ( TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA ) )
		{
			targetFormat = IMAGE_FORMAT_BGRA8888;
		}
		else
		{
			targetFormat = IMAGE_FORMAT_BGR888;
		}
	}
	else if( nFlags & TEXTUREFLAGS_HINT_DXT5 )
	{
#ifdef DEBUG_NO_COMPRESSION
		targetFormat = IMAGE_FORMAT_BGRA8888;
#else
		targetFormat = IsPosix() ? IMAGE_FORMAT_BGRA8888 : IMAGE_FORMAT_DXT5; // No DXT compressor on Posix
#endif
	}
	else if( nFlags & TEXTUREFLAGS_EIGHTBITALPHA )
	{
		// compressed with alpha blending
#ifdef DEBUG_NO_COMPRESSION
		targetFormat = IMAGE_FORMAT_BGRA8888;
#else
		targetFormat = IsPosix() ? IMAGE_FORMAT_BGRA8888 : IMAGE_FORMAT_DXT5; // No DXT compressor on Posix
#endif
	}
	else if ( nFlags & TEXTUREFLAGS_ONEBITALPHA )
	{
		// garymcthack - fixme IMAGE_FORMAT_DXT1_ONEBITALPHA doesn't work yet.
#ifdef DEBUG_NO_COMPRESSION
		targetFormat = IMAGE_FORMAT_BGRA8888;
#else
		//		targetFormat = IMAGE_FORMAT_DXT1_ONEBITALPHA;
		targetFormat = IsPosix() ? IMAGE_FORMAT_BGRA8888 : IMAGE_FORMAT_DXT5; // No DXT compressor on Posix
#endif
	}
	else
	{
#ifdef DEBUG_NO_COMPRESSION
		targetFormat = IMAGE_FORMAT_BGR888;
#else
		targetFormat = IsPosix() ? IMAGE_FORMAT_BGR888 : IMAGE_FORMAT_DXT1; // No DXT compressor on Posix
#endif
	}
	return targetFormat;
} 


//-----------------------------------------------------------------------------
// Computes the low res image size
//-----------------------------------------------------------------------------
static void VTFGetLowResImageInfo( int cacheWidth, int cacheHeight, int *lowResImageWidth, int *lowResImageHeight,
						   ImageFormat *imageFormat )
{
	if (cacheWidth > cacheHeight)
	{
		int factor = cacheWidth / LOWRESIMAGE_DIM;
		if (factor > 0)
		{
			*lowResImageWidth = LOWRESIMAGE_DIM;
			*lowResImageHeight = cacheHeight / factor;
		}
		else
		{
			*lowResImageWidth = cacheWidth;
			*lowResImageHeight = cacheHeight;
		}
	}
	else
	{
		int factor = cacheHeight / LOWRESIMAGE_DIM;
		if (factor > 0)
		{
			*lowResImageHeight = LOWRESIMAGE_DIM;
			*lowResImageWidth = cacheWidth / factor;
		}
		else
		{
			*lowResImageWidth = cacheWidth;
			*lowResImageHeight = cacheHeight;
		}
	}

	// Can end up with a dimension of zero for high aspect ration images.
	if( *lowResImageWidth < 1 )
	{
		*lowResImageWidth = 1;
	}
	if( *lowResImageHeight < 1 )
	{
		*lowResImageHeight = 1;
	}
	*imageFormat = LOWRES_IMAGE_FORMAT;
}


//-----------------------------------------------------------------------------
// This method creates the low-res image and hooks it into the VTF Texture
//-----------------------------------------------------------------------------
static void CreateLowResImage( IVTFTexture *pVTFTexture )
{
	int iWidth, iHeight;
	ImageFormat imageFormat;
	VTFGetLowResImageInfo( pVTFTexture->Width(), pVTFTexture->Height(), &iWidth, &iHeight, &imageFormat );

	// Allocate the low-res image data
	pVTFTexture->InitLowResImage( iWidth, iHeight, imageFormat );

	// Generate the low-res image bits
	if (!pVTFTexture->ConstructLowResImage())
	{
		VTexError( "Can't convert image from %s to %s in CalcLowResImage\n",
			ImageLoader::GetName(IMAGE_FORMAT_RGBA8888), ImageLoader::GetName(imageFormat) );
	}
}


//-----------------------------------------------------------------------------
// Computes the source file name
//-----------------------------------------------------------------------------
template<intp src_size>
void MakeSrcFileName( char (&pSrcName)[src_size], unsigned int flags, const char *pFullNameWithoutExtension, int frameID, 
					 int faceID, int z, bool isCubeMap, int startFrame, int endFrame, bool bNormalToDUDV )
{
	bool bAnimated = !( startFrame == -1 || endFrame == -1 );

	char tempBuf[512];
	if( bNormalToDUDV )
	{
		if ( Q_stristr( pFullNameWithoutExtension, "_dudv" ) )
		{
			V_strcpy_safe( tempBuf, pFullNameWithoutExtension );

			char *pNormalString = Q_stristr( tempBuf, "_dudv" );
			V_strcpy( pNormalString, "_normal" );
			pFullNameWithoutExtension = tempBuf;
		}
		else
		{
			// dimhotepus: Enchance Assert message.
			AssertMsg( false, "Path '%s' missed _dudv", pFullNameWithoutExtension );
		}
	}

	if( bAnimated )
	{
		if( isCubeMap )
		{
			Assert( z == -1 );
			V_sprintf_safe( pSrcName, "%s%s%03d%s", pFullNameWithoutExtension, g_CubemapFacingNames[faceID], frameID + startFrame, GetSourceExtension() );
		}
		else
		{
			if ( z == -1 )
			{
				V_sprintf_safe( pSrcName, "%s%03d%s", pFullNameWithoutExtension, frameID + startFrame, GetSourceExtension() );
			}
			else
			{
				V_sprintf_safe( pSrcName, "%s%03d_z%03d%s", pFullNameWithoutExtension, z, frameID + startFrame, GetSourceExtension() );
			}
		}
	}
	else
	{
		if( isCubeMap )
		{
			Assert( z == -1 );
			V_sprintf_safe( pSrcName, "%s%s%s", pFullNameWithoutExtension, g_CubemapFacingNames[faceID], GetSourceExtension() );
		}
		else
		{
			if ( z == -1 )
			{
				V_sprintf_safe( pSrcName, "%s%s", pFullNameWithoutExtension, GetSourceExtension() );
			}
			else
			{
				V_sprintf_safe( pSrcName, "%s_z%03d%s", pFullNameWithoutExtension, z, GetSourceExtension() );
			}
		}
	}
}

static void ComputeBufferHash( void const *pvBuffer, size_t numBytes, CRC32_t &uiHashUpdate )
{
	CRC32_ProcessBuffer( &uiHashUpdate, pvBuffer, numBytes );
}

//-----------------------------------------------------------------------------
// Loads a file into a UTLBuffer,
// also computes the hash of the buffer.
//-----------------------------------------------------------------------------
static bool LoadFile( const char *pFileName, CUtlBuffer &buf, bool bFailOnError, CRC32_t *puiHash )
{
	// dimhotepus: Use RAII FILE wrapper.
	auto [f, errc] = se::posix::posix_file_stream_factory::open(pFileName, "rb");
	if (errc)
	{
		if ( bFailOnError )
			VTexError( "Unable to open '%s': %s.\n", pFileName, errc.message().c_str() );

		return false;
	}

	int64_t size;
	std::tie(size, errc) = f.size();
	if (errc || size > std::numeric_limits<intp>::max())
	{
		// dimhotepus: Exit on size failure.
		if ( bFailOnError )
			VTexError( "Unable to get size of '%s': %s.\n", pFileName, errc.message().c_str() );

		return false;
	}

	buf.EnsureCapacity( static_cast<intp>(size) );

	size_t bytes_read;
	std::tie(bytes_read, errc) = f.read( buf.Base(), static_cast<intp>(size), 1, static_cast<intp>(size) );
	if (errc) {
		if ( bFailOnError )
			VTexError( "Unable to read '%s': %s.\n", pFileName, errc.message().c_str() );

		return false;
	}

	buf.SeekPut( CUtlBuffer::SEEK_HEAD, bytes_read );

	{ CP4AutoAddFile autop4( pFileName ); /* add loaded file to P4 */ }

	// Auto-compute buffer hash if necessary
	if ( puiHash )
		ComputeBufferHash( buf.Base(), bytes_read, *puiHash );

	return true;
}


//-----------------------------------------------------------------------------
// Creates a texture the size of the PSD image stored in the buffer
//-----------------------------------------------------------------------------
static void InitializeSrcTexture_PSD( IVTFTexture *pTexture, const char *pInputFileName,
									  CUtlBuffer &psdBuffer, int nDepth, int nFrameCount,
									  const VTexConfigInfo_t &info )
{
	int nWidth, nHeight;
	ImageFormat imageFormat;
	float flSrcGamma; 
	bool ok = PSDGetInfo( psdBuffer, &nWidth, &nHeight, &imageFormat, &flSrcGamma );
	if (!ok)
	{
		Error( "PSD %s is bogus!\n", pInputFileName );
	}
	nWidth /= info.m_nReduceX;
	nHeight /= info.m_nReduceY;


	if (!pTexture->Init( nWidth, nHeight, nDepth, IMAGE_FORMAT_DEFAULT, info.m_nFlags, nFrameCount ))
	{
		Error( "Error initializing texture %s\n", pInputFileName );
	}
}

//-----------------------------------------------------------------------------
// Creates a texture the size of the TGA image stored in the buffer
//-----------------------------------------------------------------------------
static void InitializeSrcTexture_TGA( IVTFTexture *pTexture, const char *pInputFileName,
									  CUtlBuffer &tgaBuffer, int nDepth, int nFrameCount,
									  const VTexConfigInfo_t &info )
{
	int nWidth, nHeight;
	ImageFormat imageFormat;
	float flSrcGamma; 
	bool ok = TGALoader::GetInfo( tgaBuffer, &nWidth, &nHeight, &imageFormat, &flSrcGamma );
	if (!ok)
	{
		Error( "TGA %s is bogus!\n", pInputFileName );
	}
	nWidth /= info.m_nReduceX;
	nHeight /= info.m_nReduceY;

	if (!pTexture->Init( nWidth, nHeight, nDepth, IMAGE_FORMAT_DEFAULT, info.m_nFlags, nFrameCount ))
	{
		Error( "Error initializing texture %s\n", pInputFileName );
	}
}


// HDRFIXME: Put this somewhere better than this.
// This reads an integer from a binary CUtlBuffer.
static int ReadIntFromUtlBuffer( CUtlBuffer &buf )
{
	int val = 0;
	while( buf.IsValid() )
	{
		char c = buf.GetChar();
		if( c >= '0' && c <= '9' )
		{
			val = val * 10 + ( c - '0' );
		}
		else
		{
			buf.SeekGet( CUtlBuffer::SEEK_CURRENT, -1 );
			break;
		}
	}
	return val;
}

static inline bool IsWhitespace( char c )
{
	return c == ' ' || c == '\t' || c == 10;
}

static void EatWhiteSpace( CUtlBuffer &buf )
{
	while( buf.IsValid() )
	{
		char c = buf.GetChar();
		if( !IsWhitespace( c ) )
		{
			buf.SeekGet( CUtlBuffer::SEEK_CURRENT, -1 );
			return;
		}
	}
	return;
}

//-----------------------------------------------------------------------------
// Creates a texture the size of the PFM image stored in the buffer
//-----------------------------------------------------------------------------
static void InitializeSrcTexture_PFM( IVTFTexture *pTexture, const char *pInputFileName,
									  CUtlBuffer &fileBuffer, int nDepth, int nFrameCount, 
									  const VTexConfigInfo_t &info )
{
	fileBuffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	if( fileBuffer.GetChar() != 'P' )
	{
		Assert( 0 );
		return;
	}
	if( fileBuffer.GetChar() != 'F' )
	{
		Assert( 0 );
		return;
	}
	if( fileBuffer.GetChar() != 0xa )
	{
		Assert( 0 );
		return;
	}

	int nWidth = ReadIntFromUtlBuffer( fileBuffer );
	EatWhiteSpace( fileBuffer );
	int nHeight = ReadIntFromUtlBuffer( fileBuffer );

	//	// eat crap until the next newline
	//	while( fileBuffer.GetChar() != 0xa )
	//	{
	//	}

	nWidth /= info.m_nReduceX;

	nHeight /= info.m_nReduceY;


	if (!pTexture->Init( nWidth, nHeight, nDepth, IMAGE_FORMAT_RGB323232F, info.m_nFlags, nFrameCount ))
	{
		Error( "Error initializing texture %s\n", pInputFileName );
	}
}

static void InitializeSrcTexture( IVTFTexture *pTexture, const char *pInputFileName,
								  CUtlBuffer &tgaBuffer, int nDepth, int nFrameCount,
								  const VTexConfigInfo_t &info )
{
	switch ( g_eMode )
	{
    case Mode::eModePSD:
		InitializeSrcTexture_PSD( pTexture, pInputFileName, tgaBuffer, nDepth, nFrameCount, info );
		break;
	case Mode::eModeTGA:
		InitializeSrcTexture_TGA( pTexture, pInputFileName, tgaBuffer, nDepth, nFrameCount, info );
		break;
	case Mode::eModePFM:
		InitializeSrcTexture_PFM( pTexture, pInputFileName, tgaBuffer, nDepth, nFrameCount, info );
		break;
	}
}

constexpr inline uint8 DISTANCE_CODE_ALPHA_INOUT_THRESHOLD{10};


//-----------------------------------------------------------------------------
// Loads a face from a PSD image
//-----------------------------------------------------------------------------
static bool LoadFaceFromPSD( IVTFTexture *pTexture, CUtlBuffer &psdBuffer, int z, int nFrame, int nFace, float flGamma, const VTexConfigInfo_t &info )
{
	// NOTE: This only works because all mip levels are stored sequentially
	// in memory, starting with the highest mip level. It also only works
	// because the VTF Texture store *all* mip levels down to 1x1

	// Get the information from the file...
	int nWidth, nHeight;
	ImageFormat imageFormat;
	float flSrcGamma;
	bool ok = PSDGetInfo( psdBuffer, &nWidth, &nHeight, &imageFormat, &flSrcGamma );
	if (!ok)
		return false;

	// Seek back so PSDLoader can see the psd header...
	psdBuffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );

	// Load the psd and create all mipmap levels
	unsigned char *pDestBits = pTexture->ImageData( nFrame, nFace, 0, 0, 0, z );

	if ( ( info.m_bAlphaToDistance ) || 
		( nWidth != pTexture->Width() ) ||
		( nHeight != pTexture->Height() ) )
	{
		// Load into temp
		Bitmap_t bmPsdData;
		ok = PSDReadFileRGBA8888( psdBuffer, bmPsdData );
		if ( !ok )
			return false;

		CUtlMemory<uint8> tmpDest( (intp)0, pTexture->Width() * pTexture->Height() * 4 );

		ImageLoader::ResampleInfo_t resInfo;
		resInfo.m_pSrc = bmPsdData.GetBits();
		resInfo.m_pDest = tmpDest.Base();
		resInfo.m_nSrcWidth = nWidth;
		resInfo.m_nSrcHeight = nHeight;
		resInfo.m_nDestWidth = pTexture->Width();
		resInfo.m_nDestHeight = pTexture->Height();
		resInfo.m_flSrcGamma = flGamma;
		resInfo.m_flDestGamma = flGamma;
		if (info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_FILTER_NICE )
		{
			resInfo.m_nFlags |= ImageLoader::RESAMPLE_NICE_FILTER;
		}

		if ( !ResampleRGBA8888( resInfo ) )
		{
			VTexWarning( "%s: resample to RGBA8888 failed.\n", info.m_SrcName );
			return false;
		}

		if ( info.m_bAlphaToDistance )
		{
			float flMaxRad=info.m_flDistanceSpread*2.0f*max(info.m_nReduceX,info.m_nReduceY);
			int nSearchRad=ceil(flMaxRad);
			bool bWarnEdges = false;
			// now, do alpha to distance coded stuff
			ImageFormatInfo_t fmtInfo=ImageLoader::ImageFormatInfo( pTexture->Format() );
			if ( fmtInfo.m_NumAlphaBits == 0 )
			{
				VTexWarning( "%s: alpha to distance asked for but no alpha channel.\n", info.m_SrcName );
			}
			else
			{
				for(int x=0; x < pTexture->Width(); x++ )
				{
					for(int y=0; y < pTexture->Height(); y++ )
					{
						// map to original image coords
						int nOrig_x=FLerp(0,nWidth-1,0,pTexture->Width()-1,x);
						int nOrig_y=FLerp(0,nHeight-1,0,pTexture->Height()-1,y);

						uint8 nOrigAlpha = bmPsdData.GetColor(nOrig_x, nOrig_y).a();
						bool bInOrOut=nOrigAlpha > DISTANCE_CODE_ALPHA_INOUT_THRESHOLD;

						float flClosest_Dist=1.0e23;
						for(int iy=-nSearchRad; iy <= nSearchRad; iy++ )
						{
							for(int ix=-nSearchRad; ix <= nSearchRad; ix++ )
							{
								int cx=max( 0, min( nWidth-1, ix + nOrig_x ) );
								int cy=max( 0, min( nHeight-1, iy + nOrig_y ) );

								uint8 alphaValue = bmPsdData.GetColor(cx, cy).a();
								bool bIn =( alphaValue > DISTANCE_CODE_ALPHA_INOUT_THRESHOLD );
								if ( bInOrOut != bIn )		// transition?
								{
									float flTryDist = sqrt( (float) (ix*ix+iy*iy) );
									flClosest_Dist = min( flClosest_Dist, flTryDist );
								}
							}
						}

						// now, map signed distance to alpha value
						float flOutDist = min( 0.5f, FLerp( 0, .5, 0, flMaxRad, flClosest_Dist ) );
						if ( ! bInOrOut )
						{
							// negative distance
							flOutDist = -flOutDist;
						}
						uint8 &nOutAlpha= tmpDest[static_cast<intp>(3)+4*(x+pTexture->Width()*static_cast<intp>(y) )];
						nOutAlpha = static_cast<uint8>(min( 255.0f, 255.0f*( 0.5f+flOutDist ) ));
						if ( ( nOutAlpha != 0 ) && 
							(
							( x == 0 ) ||
							( y == 0 ) ||
							( x == pTexture->Width()-1 ) ||
							( y == pTexture->Height()-1 ) ) )
						{
							bWarnEdges = true;
							nOutAlpha = 0;					// force it.
						}
					}
				}
			}

			if ( bWarnEdges )
			{
				VTexWarning( "%s: There are non-zero distance pixels along the image edge. You may need"
					" to reduce your distance spread or reduce the image less"
					" or add a border to the image.\n",
					info.m_SrcName );
			}
		}


		// now, store in dest
		ImageLoader::ConvertImageFormat( tmpDest.Base(), IMAGE_FORMAT_RGBA8888, pDestBits,
			pTexture->Format(), pTexture->Width(), pTexture->Height(), 
			0, 0 );
		return true;

	}
	else
	{
		// Read the PSD file into a bitmap
		Bitmap_t bmPsdData;
		ok = PSDReadFileRGBA8888( psdBuffer, bmPsdData );
		if ( ok )
		{
			memcpy( pDestBits, bmPsdData.GetBits(), static_cast<intp>(bmPsdData.Height()) * bmPsdData.Stride() );
		}
		return ok;
	}
}


//-----------------------------------------------------------------------------
// Loads a face from a TGA image
//-----------------------------------------------------------------------------
static bool LoadFaceFromTGA( IVTFTexture *pTexture, CUtlBuffer &tgaBuffer, int z, int nFrame, int nFace, float flGamma, const VTexConfigInfo_t &info )
{
	// NOTE: This only works because all mip levels are stored sequentially
	// in memory, starting with the highest mip level. It also only works
	// because the VTF Texture store *all* mip levels down to 1x1

	// Get the information from the file...
	int nWidth, nHeight;
	ImageFormat imageFormat;
	float flSrcGamma;
	bool ok = TGALoader::GetInfo( tgaBuffer, &nWidth, &nHeight, &imageFormat, &flSrcGamma );
	if (!ok)
		return false;

	// Seek back so TGALoader::Load can see the tga header...
	tgaBuffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	
	// Load the tga and create all mipmap levels
	unsigned char *pDestBits = pTexture->ImageData( nFrame, nFace, 0, 0, 0, z );

	if ( ( info.m_bAlphaToDistance ) || 
		 ( nWidth != pTexture->Width() ) ||
		 ( nHeight != pTexture->Height() ) )
	{
		// load into temp and resample
		CUtlMemory<uint8> tmpImage( (intp)0, nWidth*nHeight*4 );
		if ( ! TGALoader::Load( tmpImage.Base(), tgaBuffer, nWidth, 
								nHeight, IMAGE_FORMAT_RGBA8888, flGamma, false ) )
		{
			return false;
		}

		CUtlMemory<uint8> tmpDest( (intp)0, pTexture->Width() * pTexture->Height() *4 );

		ImageLoader::ResampleInfo_t resInfo;
		resInfo.m_pSrc = tmpImage.Base();
		resInfo.m_pDest = tmpDest.Base();
		resInfo.m_nSrcWidth = nWidth;
		resInfo.m_nSrcHeight = nHeight;
		resInfo.m_nDestWidth = pTexture->Width();
		resInfo.m_nDestHeight = pTexture->Height();
		resInfo.m_flSrcGamma = flGamma;
		resInfo.m_flDestGamma = flGamma;
		if (info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_FILTER_NICE )
		{
			resInfo.m_nFlags |= ImageLoader::RESAMPLE_NICE_FILTER;
		}

		// dimhotepus: Warn if resample fails.
		if ( !ResampleRGBA8888( resInfo ) )
		{
			VTexWarning( "Unable to resample texture of size %zd bytes to RGBA8888.\n", pTexture->FileSize() );
			return false;
		}

		if ( info.m_bAlphaToDistance )
		{
			float flMaxRad=info.m_flDistanceSpread*2.0f*max(info.m_nReduceX,info.m_nReduceY);
			int nSearchRad=ceil(flMaxRad);
			bool bWarnEdges = false;
			// now, do alpha to distance coded stuff
			ImageFormatInfo_t fmtInfo=ImageLoader::ImageFormatInfo( pTexture->Format() );
			if ( fmtInfo.m_NumAlphaBits == 0 )
			{
				VTexWarning( "%s: alpha to distance asked for but no alpha channel.\n", info.m_SrcName );
			}
			else
			{
				for(int x=0; x < pTexture->Width(); x++ )
				{
					for(int y=0; y < pTexture->Height(); y++ )
					{
						// map to original image coords
						int nOrig_x=FLerp(0,nWidth-1,0,pTexture->Width()-1,x);
						int nOrig_y=FLerp(0,nHeight-1,0,pTexture->Height()-1,y);
						
						uint8 nOrigAlpha=tmpImage[3+4*(nOrig_x+static_cast<intp>(nWidth)*nOrig_y)];
						bool bInOrOut=nOrigAlpha > DISTANCE_CODE_ALPHA_INOUT_THRESHOLD;

						float flClosest_Dist=1.0e23;
						for(int iy=-nSearchRad; iy <= nSearchRad; iy++ )
						{
							for(int ix=-nSearchRad; ix <= nSearchRad; ix++ )
							{
								int cx=max( 0, min( nWidth-1, ix + nOrig_x ) );
								int cy=max( 0, min( nHeight-1, iy + nOrig_y ) );

								int nOffset = 3+ 4 * ( cx + cy * nWidth );
								uint8 alphaValue = tmpImage[nOffset];
								bool bIn =( alphaValue > DISTANCE_CODE_ALPHA_INOUT_THRESHOLD );
								if ( bInOrOut != bIn )		// transition?
								{
									float flTryDist = sqrt( (float) (ix*ix+iy*iy) );
									flClosest_Dist = min( flClosest_Dist, flTryDist );
								}
							}
						}

						// now, map signed distance to alpha value
						float flOutDist = min( 0.5f, FLerp( 0, .5, 0, flMaxRad, flClosest_Dist ) );
						if ( ! bInOrOut )
						{
							// negative distance
							flOutDist = -flOutDist;
						}
						uint8 &nOutAlpha= tmpDest[3+4*(x+pTexture->Width()*static_cast<intp>(y) )];
						nOutAlpha = static_cast<uint8>(min( 255.0f, 255.0f*( 0.5f+flOutDist ) ));
						if ( ( nOutAlpha != 0 ) && 
							 (
								 ( x == 0 ) ||
								 ( y == 0 ) ||
								 ( x == pTexture->Width()-1 ) ||
								 ( y == pTexture->Height()-1 ) ) )
						{
							bWarnEdges = true;
							nOutAlpha = 0;					// force it.
						}
					}
				}
			}

			if ( bWarnEdges )
			{
				VTexWarning( "%s: There are non-zero distance pixels along the image edge. You may need"
							 " to reduce your distance spread or reduce the image less"
							 " or add a border to the image.\n",
							 info.m_SrcName );
			}
		}
		
		
		// now, store in dest
		ImageLoader::ConvertImageFormat( tmpDest.Base(), IMAGE_FORMAT_RGBA8888, pDestBits,
										 pTexture->Format(), pTexture->Width(), pTexture->Height(), 
										 0, 0 );
		return true;
		
	}
	else
	{
		return TGALoader::Load( pDestBits, tgaBuffer, pTexture->Width(), 
			pTexture->Height(), pTexture->Format(), flGamma, false );
	}
}

//-----------------------------------------------------------------------------
// Loads a face from a PFM image
//-----------------------------------------------------------------------------
// HDRFIXME: How is this different from InitializeSrcTexture_PFM?
static bool LoadFaceFromPFM( IVTFTexture *pTexture, CUtlBuffer &fileBuffer, int z, int nFrame,
							int nFace, float flGamma, const VTexConfigInfo_t &info )
{
	fileBuffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	if( fileBuffer.GetChar() != 'P' )
	{
		Assert( 0 );
		return false;
	}
	if( fileBuffer.GetChar() != 'F' )
	{
		Assert( 0 );
		return false;
	}
	if( fileBuffer.GetChar() != 0xa )
	{
		Assert( 0 );
		return false;
	}
	int nWidth, nHeight;
	nWidth = ReadIntFromUtlBuffer( fileBuffer );
	EatWhiteSpace( fileBuffer );
	nHeight = ReadIntFromUtlBuffer( fileBuffer );

	// eat crap until the next newline
	while( fileBuffer.IsValid() && fileBuffer.GetChar() != 0xa )
	{
	}
	// eat crap until the next newline
	while( fileBuffer.IsValid() && fileBuffer.GetChar() != 0xa )
	{
	}
	Assert( ImageLoader::SizeInBytes( pTexture->Format() ) == 3 * sizeof( float ) );

	// Load the pfm and create all mipmap levels
	float *pDestBits = ( float * )pTexture->ImageData( nFrame, nFace, 0, 0, 0, z );

	for( int y = nHeight-1; y >= 0; y-- )
	{
		Assert( fileBuffer.IsValid() );
		fileBuffer.Get( pDestBits + y * nWidth * 3, nWidth * static_cast<intp>(3) * sizeof( float ) );
		for(int x=0;x<nWidth*3;x++)
			pDestBits[x+y*nWidth*3]*=info.m_pfmscale;
	}
	return true;
}

static bool LoadFaceFromX( IVTFTexture *pTexture, CUtlBuffer &tgaBuffer, int z, int nFrame, int nFace, 
						   float flGamma, const VTexConfigInfo_t &info )
{
	switch ( g_eMode )
	{
    case Mode::eModePSD:
		return LoadFaceFromPSD( pTexture, tgaBuffer, z, nFrame, nFace, flGamma, info );
    case Mode::eModeTGA:
		return LoadFaceFromTGA( pTexture, tgaBuffer, z, nFrame, nFace, flGamma, info );
    case Mode::eModePFM:
		return LoadFaceFromPFM( pTexture, tgaBuffer, z, nFrame, nFace, flGamma, info );
	default:
		return false;
	}
}

static bool LoadFace( IVTFTexture *pTexture, CUtlBuffer &tgaBuffer, int z, int nFrame, int nFace, 
					  float flGamma, const VTexConfigInfo_t &info )
{
	if ( !LoadFaceFromX( pTexture, tgaBuffer, z, nFrame, nFace, flGamma, info ) )
		return false;

	// Restricting number of channels by painting white into the rest
	if ( info.m_numChannelsMax < 1 || info.m_numChannelsMax > 4 )
	{
		VTexWarning( "%s: Invalid setting restricting number of channels to %d, discarded!\n", info.m_SrcName, info.m_numChannelsMax );
	}
	else if ( info.m_numChannelsMax < 4 )
	{
		if ( 4 != ImageLoader::SizeInBytes( pTexture->Format() ) )
		{
			VTexWarning( "%s: Channels restricted to %d, but cannot fill white"
				" because pixel format is %d (size in bytes %d)!"
				" Proceeding with unmodified channels.\n",
				info.m_SrcName,
				info.m_numChannelsMax, pTexture->Format(), ImageLoader::SizeInBytes( pTexture->Format() ) );
			Assert( 0 );
		}
		else
		{
			// Fill other channels with white

			unsigned char *pDestBits = pTexture->ImageData( nFrame, nFace, 0, 0, 0, z );
			int nWidth = pTexture->Width();
			int nHeight = pTexture->Height();
			
			int nPaintOff = info.m_numChannelsMax;
			int nPaintBytes = 4 - nPaintOff;
			
			pDestBits += nPaintOff;

			for( int j = 0; j < nHeight; ++ j )
			{
				for ( int k = 0; k < nWidth; ++ k, pDestBits += 4 )
				{
					memset( pDestBits, 0xFF, nPaintBytes );
				}
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Loads source image data 
//-----------------------------------------------------------------------------
static bool LoadSourceImages( IVTFTexture *pTexture, const char *pFullNameWithoutExtension,
							  bool *pbGenerateSphereMaps,
							  VTexConfigInfo_t &info )
{
	static char pSrcName[1024];

	bool bGenerateSpheremaps = false;

	// The input file name here is simply for error reporting
	intp sizeFileName = strlen( pFullNameWithoutExtension ) + strlen( GetSourceExtension() ) + 1;
	char *pInputFileName = stackallocT( char, sizeFileName );
	V_strncpy( pInputFileName, pFullNameWithoutExtension, sizeFileName );
	V_strncat( pInputFileName, GetSourceExtension(), sizeFileName );

	int nFrameCount;
	bool bAnimated = !( info.m_nStartFrame == -1 || info.m_nEndFrame == -1 );
	if( !bAnimated )
	{
		nFrameCount = 1;
	}
	else
	{
		nFrameCount = info.m_nEndFrame - info.m_nStartFrame + 1;
	}

	bool bIsCubeMap = (info.m_nFlags & TEXTUREFLAGS_ENVMAP) != 0;
	bool bIsVolumeTexture = ( info.m_nVolumeTextureDepth > 1 );

	// Iterate over all faces of all frames
	int nFaceCount = bIsCubeMap ? CUBEMAP_FACE_COUNT : 1;
	for( int iFrame = 0; iFrame < nFrameCount; ++iFrame )
	{
		for( int iFace = 0; iFace < nFaceCount; ++iFace )
		{
			for ( int z = 0; z < info.m_nVolumeTextureDepth; ++z )
			{
				// Generate the filename to load....
				MakeSrcFileName( pSrcName, info.m_nFlags, pFullNameWithoutExtension, 
					iFrame, iFace, bIsVolumeTexture ? z : -1, bIsCubeMap, info.m_nStartFrame, info.m_nEndFrame, info.m_bNormalToDuDv );

				// Don't fail if the 7th iFace of a cubemap isn't loaded...
				// that just means that we're gonna have to build the spheremap ourself.
				bool bFailOnError = !bIsCubeMap || (iFace != CUBEMAP_FACE_SPHEREMAP);

				// Load the TGA from disk...
				CUtlBuffer tgaBuffer;
				if ( !LoadFile( pSrcName, tgaBuffer, bFailOnError,
					( g_eMode != Mode::eModePSD ) ? &info.m_uiInputHash : NULL ) )
				{
					// If we want to fail on error and VTexError didn't abort then
					// simply notify the caller that we failed
					if ( bFailOnError )
						return false;

					// The only other way we can get here is if LoadFile tried to load a spheremap and failed
					bGenerateSpheremaps = true;
					continue;
				}

				// Initialize the VTF Texture here if we haven't already....
				// Note that we have to do it here because we have to get the width + height from the file
				if (!pTexture->ImageData())
				{
					InitializeSrcTexture( pTexture, pSrcName, tgaBuffer, info.m_nVolumeTextureDepth, nFrameCount, info );

					// Re-seek back to the front of the buffer so LoadFaceFromTGA works
					tgaBuffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
				}

				V_strcpy_safe( info.m_SrcName, pSrcName );
				// NOTE: This here will generate all mip levels of the source image
				if (!LoadFace( pTexture, tgaBuffer, z, iFrame, iFace, 2.2, info ))
				{
					Error( "Error loading texture %s\n", pInputFileName );
				}
			}
		}
	}

	if ( pbGenerateSphereMaps )
	{
		*pbGenerateSphereMaps = bGenerateSpheremaps;
	}

	return true;
}


static void PreprocessSkyBox( char *pFullNameWithoutExtension, int *iSkyboxFace )
{
	// When we get here, it means that we're processing one face of a skybox, but we're going to 
	// load all the faces and treat it as a cubemap so we can do the edge matching.

	// Since they passed in only one face of the skybox, there's a 2 letter extension we want to get rid of.
	size_t len = strlen( pFullNameWithoutExtension );
	if ( len >= 3 )
	{
		// Make sure there really is a 2 letter extension.
		char *pEnd = &pFullNameWithoutExtension[ len - 2 ];
		*iSkyboxFace = -1;
		for ( int i=0; i < ssize( g_CubemapFacingNames ); i++ )
		{
			if ( stricmp( pEnd, g_CubemapFacingNames[i] ) == 0 )
			{
				*iSkyboxFace = i;
				break;
			}
		}

		// Cut off the 2 letter extension.
		if ( *iSkyboxFace != -1 )
		{
			pEnd[0] = 0;
			return;
		}
	}

	Error( "PreprocessSkyBox: filename %s doesn't have a proper extension (bk, dn, rt, etc..)\n", pFullNameWithoutExtension );
}


// Right now, we've got a full cubemap, and we want to return the one face of the
// skybox that we're supposed to be processing.
static IVTFTexture* PostProcessSkyBox( IVTFTexture *pTexture, int iSkyboxFace )
{
	int nFlags = pTexture->Flags();
	Assert( nFlags & TEXTUREFLAGS_ENVMAP );	// Should have been treated as an envmap till now.
	nFlags &= ~TEXTUREFLAGS_ENVMAP;			// But it ends now!

	IVTFTexture *pRet = CreateVTFTexture();
	RunCodeAtScopeExit(DestroyVTFTexture( pTexture ));

	if ( !pRet->Init( pTexture->Width(), pTexture->Height(), 1, pTexture->Format(), nFlags, pTexture->FrameCount() ) )
		Error( "PostProcessSkyBox: IVTFTexture::Init() failed.\n" );

	// Now just dump the data for the face we want to keep.
	int nMips = min( pTexture->MipCount(), pRet->MipCount() );
	for ( int iMip=0; iMip < nMips; iMip++ )
	{
		intp mipSize = pTexture->ComputeMipSize( iMip );
		if ( pRet->ComputeMipSize( iMip ) != mipSize )
		{
			Error( "PostProcessSkyBox: ComputeMipSize differs (src=%zd, dest=%zd)\n", mipSize, pRet->ComputeMipSize( iMip ) );
		}

		for ( int iFrame=0; iFrame < pTexture->FrameCount(); iFrame++ )
		{
			unsigned char *pDest = pRet->ImageData( iFrame, 0, iMip );
			const unsigned char *pSrc = pTexture->ImageData( iFrame, iSkyboxFace, iMip );
			memcpy( pDest, pSrc, mipSize );
		}
	}

	// Note: there are a few things that don't get copied here, like alpha test threshold
	// and bumpscale, but we shouldn't need those for skyboxes anyway.
	// 
	// Get rid of the full cubemap one and return the single-face one.
	return pRet;
}

// dimhotepus: Report hierarchy creation errors.
[[nodiscard]] static std::error_code MakeDirHier( const char *pPath )
{
#ifdef POSIX
#define mkdir(s) mkdir(s, S_IRWXU | S_IRWXG | S_IRWXO )
#endif
	char temp[1024];
	V_strcpy_safe( temp, pPath );
	for( size_t i = 0; i < strlen( temp ); i++ )
	{
		if( temp[i] == '/' || temp[i] == '\\' )
		{
			temp[i] = '\0';

			if ( mkdir( temp ) && errno != EEXIST )
				return std::error_code{ errno, std::generic_category() };

			temp[i] = CORRECT_PATH_SEPARATOR;
		}
	}

	if ( mkdir( temp ) && errno != EEXIST )
		return std::error_code{ errno, std::generic_category() };

	return std::error_code{ 0, std::generic_category() };
}


static uint8 GetClampingValue( int nClampSize )
{
	if ( nClampSize <= 0 )
		return 30;											// ~1 billion
	uint8 nRet = 0;
	while ( nClampSize > 1 )
	{
		nClampSize >>= 1;
		nRet++;
	}
	return nRet;
}

static void SetTextureLodData( IVTFTexture *pTexture, VTexConfigInfo_t const &info )
{
	if (
		( info.m_nMaxDimensionX > 0 && info.m_nMaxDimensionX < pTexture->Width() ) ||
		( info.m_nMaxDimensionY > 0 && info.m_nMaxDimensionY < pTexture->Height() ) ||
		( info.m_nMaxDimensionX_360 > 0 && info.m_nMaxDimensionX_360 < pTexture->Width() ) ||
		( info.m_nMaxDimensionY_360 > 0 && info.m_nMaxDimensionY_360 < pTexture->Height() )
		)
	{
		TextureLODControlSettings_t lodChunk;
		memset( &lodChunk, 0, sizeof( lodChunk ) );
		lodChunk.m_ResolutionClampX = GetClampingValue( info.m_nMaxDimensionX );
		lodChunk.m_ResolutionClampY = GetClampingValue( info.m_nMaxDimensionY );
		lodChunk.m_ResolutionClampX_360 = GetClampingValue( info.m_nMaxDimensionX_360 );
		lodChunk.m_ResolutionClampY_360 = GetClampingValue( info.m_nMaxDimensionY_360 );
		pTexture->SetResourceData( VTF_RSRC_TEXTURE_LOD_SETTINGS, &lodChunk, sizeof( lodChunk ) );
	}
}


static void AttachShtFile( const char *pFullNameWithoutExtension, IVTFTexture *pTexture, CRC32_t *puiHash )
{
	char shtName[MAX_PATH];
	V_strcpy_safe( shtName, pFullNameWithoutExtension );
	Q_SetExtension( shtName, ".sht" );

	struct	_stat statBuf;
	if( _stat( shtName, &statBuf ) == -1 )
		return;

	printf( "Attaching .sht file %s.\n", shtName );

	// Ok, the file exists. Read it.
	CUtlBuffer buf;
	if ( !LoadFile( shtName, buf, false, puiHash ) )
		return;

	pTexture->SetResourceData( VTF_RSRC_SHEET, buf.Base(), buf.TellPut() );
}


//-----------------------------------------------------------------------------
// Does the dirty deed and generates a VTF file
//-----------------------------------------------------------------------------
static bool ProcessFiles( const char *pFullNameWithoutExtension, 
				  const char *pOutputDir, const char *pBaseName, 
				  bool isCubeMap, VTexConfigInfo_t &info )
{
	// force clamps/clampt for cube maps
	if ( isCubeMap )
	{
		info.m_nFlags |= TEXTUREFLAGS_ENVMAP | TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT;
	}

	// Create the texture we're gonna store out
	SmartIVtfTexture pVTFTexture( CreateVTFTexture() );

	int iSkyboxFace = 0;
	char fullNameTemp[512];
	if ( info.m_bSkyBox )
	{
		V_strcpy_safe( fullNameTemp, pFullNameWithoutExtension );
		pFullNameWithoutExtension = fullNameTemp;
		PreprocessSkyBox( fullNameTemp, &iSkyboxFace );
	}

	// Load the source images into the texture
	bool bGenerateSpheremaps = false;
	bool bLoadedSourceImages = LoadSourceImages( pVTFTexture.Get(), 
		pFullNameWithoutExtension, &bGenerateSpheremaps, info );
	if ( !bLoadedSourceImages )
	{
		VTexError( "Can't load source images for '%s'.\n", pFullNameWithoutExtension );
		return false;
	}

	// Attach a sheet file if present
	AttachShtFile( pFullNameWithoutExtension, pVTFTexture.Get(), &info.m_uiInputHash );

	// No more file loads, finalize the sources hash
	CRC32_Final( &info.m_uiInputHash );
	pVTFTexture->SetResourceData( VTexConfigInfo_t::VTF_INPUTSRC_CRC, &info.m_uiInputHash, sizeof( info.m_uiInputHash ) );
	CRC32_t crcWritten = info.m_uiInputHash;

	// Name of the destination file
	char dstFileName[1024];
	V_sprintf_safe( dstFileName, "%s/%s%s.vtf", pOutputDir, pBaseName, ( ( Mode::eModePFM == g_eMode ) && isCubeMap ) ? ".hdr" : "" );

	// Now if we are only validating the CRC
	if ( CommandLine()->FindParm( "-crcvalidate" ) )
	{
		CUtlBuffer bufFile;
		bool bLoad = LoadFile( dstFileName, bufFile, false, NULL );
		if ( !bLoad )
		{
			fprintf( stderr, "Unable to load '%s'.\n", dstFileName );
			return false;
		}

		SmartIVtfTexture spExistingVtf( CreateVTFTexture() );
		bLoad = spExistingVtf->Unserialize( bufFile );
		if ( !bLoad )
		{
			fprintf( stderr, "Unable to deserialize '%s'.\n", dstFileName );
			return false;
		}

		size_t numDataBytes;
		const void *pCrcData = spExistingVtf->GetResourceData( VTexConfigInfo_t::VTF_INPUTSRC_CRC, &numDataBytes );
		if ( !pCrcData || numDataBytes != sizeof( CRC32_t ) )
		{
			fprintf( stderr, "Old texture format '%s'.\n", dstFileName );
			return false;
		}

		CRC32_t crcFile = * reinterpret_cast< CRC32_t const * >( pCrcData );
		if ( crcFile != crcWritten )
		{
			fprintf( stderr, "CRC32 mismatch '%s'.\n", dstFileName );
			return false;
		}

		return true;
	}

	// Now if we are not forcing the CRC
	if ( !CommandLine()->FindParm( "-crcforce" ) )
	{
		CUtlBuffer bufFile;
		if ( LoadFile( dstFileName, bufFile, false, NULL ) )
		{
			SmartIVtfTexture spExistingVtf( CreateVTFTexture() );
			if ( spExistingVtf->Unserialize( bufFile ) )
			{
				size_t numDataBytes;
				const void *pCrcData = spExistingVtf->GetResourceData( VTexConfigInfo_t::VTF_INPUTSRC_CRC, &numDataBytes );
				if ( pCrcData && numDataBytes == sizeof( CRC32_t ) )
				{
					CRC32_t crcFile = * reinterpret_cast< CRC32_t const * >( pCrcData );
					if ( crcFile == crcWritten )
					{
						if( !g_Quiet )
							printf( "SUCCESS: '%s' is up-to-date.\n", dstFileName );

						return true;
					}
				}
			}
		}
	}

	// Bumpmap scale..
	pVTFTexture->SetBumpScale( info.m_flBumpScale );

	// Alphatest threshhold
	pVTFTexture->SetAlphaTestThreshholds( info.m_flAlphaThreshhold, info.m_flAlphaHiFreqThreshhold );

	// Set texture lod data
	SetTextureLodData( pVTFTexture.Get(), info );

	// Get the texture all internally consistent and happy
	bool bAllowFixCubemapOrientation = !info.m_bSkyBox;	// Don't let it rotate our pseudo-cubemap faces around if it's a skybox.
	pVTFTexture->SetPostProcessingSettings( &info.m_vtfProcOptions );
	pVTFTexture->PostProcess( bGenerateSpheremaps, info.m_LookDir, bAllowFixCubemapOrientation );

	// Compute the preferred image format
	ImageFormat vtfImageFormat = ComputeDesiredImageFormat( pVTFTexture.Get(), info );

	// Set up the low-res image
	if (pVTFTexture->IsCubeMap())
	{
		// "Stage 1" of matching cubemap borders. Sometimes, it has to store off the original image.
		pVTFTexture->MatchCubeMapBorders( 1, vtfImageFormat, info.m_bSkyBox );
	}
	else
	{
		CreateLowResImage( pVTFTexture.Get() );
	}

	// Convert to the final format
	pVTFTexture->ConvertImageFormat( vtfImageFormat, info.m_bNormalToDuDv );

	// Stage 2 of matching cubemap borders.
	pVTFTexture->MatchCubeMapBorders( 2, vtfImageFormat, info.m_bSkyBox );

	if ( info.m_bSkyBox )
	{
		pVTFTexture.Assign( PostProcessSkyBox( pVTFTexture.Get(), iSkyboxFace ) );
	}

	if ( info.IsSettings0Valid() )
	{
		pVTFTexture->SetResourceData( VTF_RSRC_TEXTURE_SETTINGS_EX, &info.m_exSettings0, sizeof( info.m_exSettings0 ) );
	}

	// Write it!
	if ( g_CreateDir == true )
	{
		const std::error_code rc = MakeDirHier( pOutputDir ); //It'll create it if it doesn't exist.
		if ( rc )
			VTexError( "Unable to create output directory '%s': %s.", pOutputDir, rc.message().c_str() );
	}

	// Make sure the CRC hasn't been modified since finalized
	Assert( crcWritten == info.m_uiInputHash );

	CUtlBuffer outputBuf;
	if (!pVTFTexture->Serialize( outputBuf ))
	{
		VTexError( "ERROR: '%s': Unable to serialize the VTF file!\n", dstFileName );
	}

	{
		CP4AutoEditAddFile autop4( dstFileName );
		// dimhotepus: Use RAII FILE wrapper.
		auto [f, errc] = se::posix::posix_file_stream_factory::open( dstFileName, "wb" );
		if ( errc )
		{
			VTexError( "Unable to open '%s': %s.\n", dstFileName, errc.message().c_str() );
		}

		std::tie(std::ignore, errc) = f.write( outputBuf.Base(), 1, outputBuf.TellPut() );
		if ( errc )
		{
			VTexError( "Unable to write '%s': %s.\n", dstFileName, errc.message().c_str() );
		}
	}

	// dimhotepus: Dump created VTF name.
	printf("SUCCESS: Vtf file '%s' created.\n", dstFileName);
	return true;
}

template<size_t out_size>
const char *GetPossiblyQuotedWord( const char *pInBuf, char (&pOutbuf)[out_size] )
{
	pInBuf += strspn( pInBuf, " \t" );						// skip whitespace

	const char *pWordEnd;
	bool bQuote = false;
	if (pInBuf[0]=='"')
	{
		pInBuf++;
		pWordEnd=strchr(pInBuf,'"');
		bQuote = true;
	}
	else
	{
		pWordEnd=strchr(pInBuf,' ');
		if (! pWordEnd )
			pWordEnd = strchr(pInBuf,'\t' );
		if (! pWordEnd )
			pWordEnd = pInBuf+strlen(pInBuf);
	}

	if ((! pWordEnd ) || (pWordEnd == pInBuf ) )
		return NULL;

	// no word found
	// dimhotepus: Prevent overflow of out buf.
	const size_t word_size = min(out_size - 1, static_cast<size_t>(pWordEnd-pInBuf));
	memcpy( pOutbuf, pInBuf, word_size );
	pOutbuf[word_size]='\0';

	pInBuf = pWordEnd;
	if ( bQuote )
		pInBuf++;

	return pInBuf;
}

// GetKeyValueFromBuffer:
//		fills in "key" and "val" respectively and returns "true" if succeeds.
//		returns false if:
//			a) end-of-buffer is reached (then "val" is empty)
//			b) error occurs (then "val" is the error message)
//
template<size_t key_size, size_t value_size>
static bool GetKeyValueFromBuffer( CUtlBuffer &buffer, char (&key)[key_size], char (&val)[value_size] )
{
	char buf[2048];

	while( buffer.GetBytesRemaining() )
	{
		buffer.GetLine( buf );

		// Scanning algorithm
		char *pComment = strpbrk( buf, "#\n\r" );
		if ( pComment )
			*pComment = '\0';

		pComment = strstr( buf, "//" );
		if ( pComment)
			*pComment = '\0';

		const char *scan = buf;
		scan = GetPossiblyQuotedWord( scan, key );
		if ( scan )
		{
			scan = GetPossiblyQuotedWord( scan, val );
			if ( scan )
				return true;

			V_sprintf_safe( val, "parameter %s has no value", key );
			return false;
		}
	}

	val[0] = '\0';
	return false;
}


//-----------------------------------------------------------------------------
// Loads the .psd file or .txt file associated with the .tga and gets out various data
//-----------------------------------------------------------------------------
static bool LoadConfigFile( const char *pFileBaseName, VTexConfigInfo_t &info, bool *isCubeMap )
{
	// Tries to load .txt, then .psd

	intp lenBaseName = V_strlen( pFileBaseName );
	intp sizeFileName = lenBaseName + std::size( ".tga" );
	char *pFileName = stackallocT( char, sizeFileName );
	V_strncpy( pFileName, pFileBaseName, sizeFileName );
	V_strncat( pFileName, ".tga", sizeFileName );

	bool bOK = false;

	info.m_LookDir = LOOK_DOWN_Z;

	// Try TGA file with config
	memcpy( pFileName + lenBaseName, ".tga", 4 );
	if ( !g_bNoTga && ( 00 == access( pFileName, 00 ) ) ) // TGA file exists
	{
		g_eMode = Mode::eModeTGA;

		memcpy( pFileName + lenBaseName, ".txt", 4 );
		CUtlBuffer bufFile( (intp)0, 0, CUtlBuffer::TEXT_BUFFER );
		bOK = LoadFile( pFileName, bufFile, false, &info.m_uiInputHash );
		if ( bOK )
		{
			printf("config file %s\n",pFileName);

			{
				char key[2048];
				char val[2048];
				while( GetKeyValueFromBuffer( bufFile, key, val ) )
				{
					info.ParseOptionKey( key, val );
				}

				if ( val[0] )
				{
					VTexError( "%s: %s\n", pFileName, val );
					return false;
				}
			}
		}
		else
		{
			memcpy( pFileName + lenBaseName, ".tga", 4 );
			VTexWarning("no config file for %s\n",pFileName);
			bOK = true;
		}
	}
	memcpy( pFileName + lenBaseName, ".tga", 4 );
	if ( g_bNoTga && ( 00 == access( pFileName, 00 ) ) )
	{
		fprintf( stderr, "Warning: -notga disables '%s'.\n", pFileName );
	}

	// PSD file attempt
	memcpy( pFileName + lenBaseName, ".psd", 4 );
	if ( !bOK && !g_bNoPsd ) // If PSD mode was not disabled
	{
		g_eMode = Mode::eModePSD;

		CUtlBuffer bufFile;
		bOK = LoadFile( pFileName, bufFile, false, &info.m_uiInputHash );
		if ( bOK )
		{
			printf("config file %s\n", pFileName);
			bOK = IsPSDFile( bufFile );
			if ( !bOK )
			{
				VTexError( "%s is not a valid PSD file!\n", pFileName );
				return false;
			}

			PSDImageResources imgres = PSDGetImageResources( bufFile );
			PSDResFileInfo resFileInfo( imgres.FindElement( PSDImageResources::eResFileInfo ) );
			PSDResFileInfo::ResFileInfoElement descr = resFileInfo.FindElement( PSDResFileInfo::eDescription );
			if ( descr.m_pvData )
			{
				CUtlBuffer bufDescr( (intp)0, 0, CUtlBuffer::TEXT_BUFFER );
				bufDescr.EnsureCapacity( descr.m_numBytes );
				bufDescr.Put( descr.m_pvData, descr.m_numBytes );

				{
					char key[2048];
					char val[2048];
					while( GetKeyValueFromBuffer( bufDescr, key, val ) )
					{
						info.ParseOptionKey( key, val );
					}

					if ( val[0] )
					{
						VTexError( "%s: %s\n", pFileName, val );
						return false;
					}
				}
			}
		}
	}
	else if ( 00 == access( pFileName, 00 ) )
	{
		if ( !bOK )
			fprintf( stderr, "Warning: -nopsd disables '%s'.\n", pFileName );
		else
			fprintf( stderr, "Warning: PSD file '%s' exists, but not used, delete TGA and TXT files to use PSD file directly.\n", pFileName );
	}

	// Try TXT file as config again for TGA cubemap / PFM
	memcpy( pFileName + lenBaseName, ".txt", 4 );
	if ( !bOK )
	{
		g_eMode = Mode::eModeTGA;

		CUtlBuffer bufFile( (intp)0, 0, CUtlBuffer::TEXT_BUFFER );
		bOK = LoadFile( pFileName, bufFile, false, &info.m_uiInputHash );
		if ( bOK )
		{
			printf("config file %s\n",pFileName);

			{
				char key[2048];
				char val[2048];
				while( GetKeyValueFromBuffer( bufFile, key, val ) )
				{
					info.ParseOptionKey( key, val );
				}

				if ( val[0] )
				{
					VTexError( "%s: %s\n", pFileName, val );
					return false;
				}
			}

			if ( g_eMode == Mode::eModePFM )
			{
				if ( g_bUsedAsLaunchableDLL && !( info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_NOCOMPRESS ) )
				{
					info.m_nFlags |= TEXTUREFLAGS_NOMIP;
				}
			}
		}
	}

	if ( !bOK )
	{
		VTexError( "'%s' does not specify valid %s%sPFM+TXT files!\n",
			pFileBaseName,
			g_bNoPsd ? "" : "PSD or ",
			g_bNoTga ? "" : "TGA or "
			);
		return false;
	}

	if ( info.m_bIsCubeMap )
		*isCubeMap = true;

	if( ( info.m_bNormalToDuDv || ( info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_NORMAL_DUDV ) ) &&
		!( info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_PREMULT_COLOR_ONEOVERMIP ) )
	{
		printf( "Implicitly setting premultcolorbyoneovermiplevel since you are generating a dudv map\n" );
		info.m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_PREMULT_COLOR_ONEOVERMIP;
	}

	if( ( info.m_bNormalToDuDv || ( info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_NORMAL_DUDV ) ) )
	{
		printf( "Implicitly setting trilinear since you are generating a dudv map\n" );
		info.m_nFlags |= TEXTUREFLAGS_TRILINEAR;
	}

	if( Q_stristr( pFileBaseName, "_normal" ) )
	{
		if( !( info.m_nFlags & TEXTUREFLAGS_NORMAL ) )
		{
			if( !g_Quiet )
			{
				fprintf( stderr, "implicitly setting:\n" );
				fprintf( stderr, "\t\"normal\" \"1\"\n" );
				fprintf( stderr, "since filename ends in \"_normal\"\n" );
			}
			info.m_nFlags |= TEXTUREFLAGS_NORMAL;
		}
	}

	if( Q_stristr( pFileBaseName, "ssbump" ) )
	{
		if( !( info.m_nFlags & TEXTUREFLAGS_SSBUMP ) )
		{
			if( !g_Quiet )
			{
				fprintf( stderr, "implicitly setting:\n" );
				fprintf( stderr, "\t\"ssbump\" \"1\"\n" );
				fprintf( stderr, "since filename includes \"ssbump\"\n" );
			}
			info.m_nFlags |= TEXTUREFLAGS_SSBUMP;
		}
	}

	if( Q_stristr( pFileBaseName, "_dudv" ) )
	{
		if( !info.m_bNormalToDuDv && !info.m_bDuDv )
		{
			if( !g_Quiet )
			{
				fprintf( stderr, "Implicitly setting:\n" );
				fprintf( stderr, "\t\"dudv\" \"1\"\n" );
				fprintf( stderr, "since filename ends in \"_dudv\"\n" );
				fprintf( stderr, "If you are trying to convert from a normal map to a dudv map, put \"normaltodudv\" \"1\" in description.\n" );
			}
			info.m_bDuDv = true;
		}
	}

	// turn off nice filtering if we are a cube map (takes too long with buildcubemaps) or
	// if we are a normal map (looks like terd.)
	if( ( info.m_nFlags & TEXTUREFLAGS_NORMAL ) || *isCubeMap )
	{
		if (info.m_vtfProcOptions.flags0 & VtfProcessingOptions::OPT_FILTER_NICE )
		{
			if ( !g_Quiet )
			{
				fprintf( stderr, "implicity disabling nice filtering\n" );
			}
		}
		info.m_vtfProcOptions.flags0 &= ~VtfProcessingOptions::OPT_FILTER_NICE;
	}

	return true;
}

static void Usage()
{
	VTexError( 
		"Usage: vtex [-outdir dir] [-quiet] [-nopause] [-mkdir] [-shader ShaderName] [-vmtparam Param Value] tex1.txt tex2.txt ...\n"
		"-quiet            : don't print anything out, don't pause for input\n"
		"-warningsaserrors : treat warnings as errors\n"
		"-nopause          : don't pause for input\n"
		"-nomkdir          : don't create destination folder if it doesn't exist\n"
		"-vmtparam         : adds parameter and value to the .vmt file\n"
		"                    eg: -vmtparam $ignorez 1 -vmtparam $translucent 1\n"
		"-outdir <dir>     : write output to the specified dir regardless of source filename and vproject\n"
		"-deducepath       : deduce path of sources by target file names\n"
		"-quickconvert     : use with \"-nop4 -dontusegamedir -quickconvert\" to upgrade old .vmt files\n"
		"-crcvalidate      : validate .vmt against the sources\n"
		"-crcforce         : generate a new .vmt even if sources crc matches\n\n"
		"Note that you can use wildcards and that you can also chain them\n"
		"e.g. materialsrc/monster1/*.tga materialsrc/monster2/*.tga\n" );
}

template<intp out_size>
static bool GetOutputDir( const char *inputName, char (&outputDir)[out_size] )
{
	if ( g_ForcedOutputDir[0] )
	{
		V_strcpy_safe( outputDir, g_ForcedOutputDir );
	}
	else
	{
		// Is inputName a relative path?
		char buf[MAX_PATH];
		V_MakeAbsolutePath( buf, inputName, NULL );
		V_FixSlashes( buf );

		char szSearch[MAX_PATH] = { 0 };
		V_snprintf( szSearch, sizeof( szSearch ), "materialsrc%c", CORRECT_PATH_SEPARATOR );
		const char *pTmp = Q_stristr( buf, szSearch );
		if( !pTmp )
		{
			return false;
		}
		pTmp += strlen( "materialsrc/" );
		V_strcpy_safe( outputDir, gamedir );
		V_strcat_safe( outputDir, "materials/" );
		V_strcat_safe( outputDir, pTmp );
		V_StripFilename( outputDir );
	}
	if( !g_Quiet )
	{
		printf( "output directory: %s\n", outputDir );
	}
	return true;
}

static bool IsCube( const char *inputName )
{
	char tgaName[MAX_PATH];
	// Do Strcmp for ".hdr" to make sure we aren't ripping too much stuff off.
	Q_StripExtension( inputName, tgaName );

	const char *pInputExtension = inputName + V_strlen( tgaName );
	V_strcat_safe( tgaName, "rt", MAX_PATH );
	V_strcat_safe( tgaName, pInputExtension, MAX_PATH );
	V_strcat_safe( tgaName, GetSourceExtension(), MAX_PATH );

	struct	_stat buf;
	if( _stat( tgaName, &buf ) != -1 )
	{
		return true;
	}
	else
	{
		return false;
	}
}

#ifdef WIN32
static int Find_Files( WIN32_FIND_DATA &wfd, HANDLE &hResult, const char *basedir, const char *extension )
{
	char	filename[MAX_PATH] = {0};

	BOOL bMoreFiles = FindNextFile( hResult, &wfd);

	if ( bMoreFiles )
	{
		// Skip . and ..
		if ( wfd.cFileName[0] == '.' )
		{
			return FF_TRYAGAIN;
		}

		// If it's a subdirectory, just recurse down it
		if ( (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
		{
			char	subdir[MAX_PATH];
			V_sprintf_safe( subdir, "%s\\%s", basedir, wfd.cFileName );

			// Recurse
			Find_Files( wfd, hResult, basedir, extension );
			return FF_TRYAGAIN;
		}

		// Check that it's a tga
		//

		char fname[_MAX_FNAME] = {0};
		char ext[_MAX_EXT] = {0};

		_splitpath( wfd.cFileName, NULL, NULL, fname, ext );

		// Not the type we want.
		if ( stricmp( ext, extension ) != 0 )
			return FF_DONTPROCESS;

		// Check for .vmt
		V_sprintf_safe( filename, "%s\\%s.vmt", basedir, fname );
		// Exists, so don't overwrite it
		if ( access( filename, 0 ) != -1 )
			return FF_TRYAGAIN;

		char texturename[ MAX_PATH ] = {0};
		char *p = ( char * )basedir;

		// Skip over the base path to get a material system relative path
		// p += strlen( wfd.cFileName ) + 1;

		// Construct texture name
		V_sprintf_safe( texturename, "%s\\%s", p, fname );

		// Convert all to lower case
		strlwr( texturename );
		strlwr( filename );
	}

	return bMoreFiles;
}
#endif

template<intp maxlen>
static bool Process_File( char (&pInputBaseName)[maxlen] )
{
	char outputDir[1024];
	Q_FixSlashes( pInputBaseName, '/' );
	Q_StripExtension( pInputBaseName, pInputBaseName );

	if ( CommandLine()->FindParm( "-deducepath" ) )
	{
		V_strcpy_safe( outputDir, pInputBaseName );

		// If it is not a full path, try making it a full path
		if ( pInputBaseName[0] != '/' &&
			 pInputBaseName[1] != ':' )
		{
			// Convert to full path
			if ( !getcwd( outputDir, sizeof( outputDir ) ) )
			{
				VTexWarning( "Unable to get current directory: %s.\n",
					std::generic_category().message(errno).c_str() );
				return false;
			}

			Q_FixSlashes( outputDir, '/' );
			V_strcat_safe( outputDir, "/" );
			V_strcat_safe( outputDir, pInputBaseName );
		}

		// If it is pointing inside "/materials/" make it go for "/materialsrc/"
		char *pGame = strstr( outputDir, "/game/" );
		char *pMaterials = strstr( outputDir, "/materials/" );
		if ( pGame && pMaterials && ( pGame < pMaterials ) )
		{
			// "u:/data/game/tf/materials/"  ->  "u:/data/content/tf/materialsrc/"
			constexpr size_t numExtraBytes = std::size( "/content/.../materialsrc/" ) - std::size( "/game/.../materials/" ) - 2;
			intp numConvertBytes = pMaterials + std::size( "/materials/" ) - 1 - outputDir;
			memmove( outputDir + numConvertBytes + numExtraBytes, outputDir + numConvertBytes, strlen( outputDir ) - numConvertBytes + 1 );
			
			intp numMidBytes = pMaterials - pGame - std::size( "/game" ) - 1;
			memmove( pGame + std::size( "/content" ) - 1, pGame + std::size( "/game" ) - 1, numMidBytes );

			memmove( pGame, "/content", std::size( "/content" ) - 1 );
			memmove( pGame + std::size( "/content" ) - 1 + numMidBytes, "/materialsrc/", std::size( "/materialsrc/" ) - 1 );
		}

		V_strcpy_safe( pInputBaseName, outputDir );
	}

	if( !g_Quiet )
	{
		printf( "input file: %s\n", pInputBaseName );
	}

	if(g_UseGameDir && !GetOutputDir( pInputBaseName, outputDir ) )
	{
		VTexError( "Problem figuring out outputdir for %s\n", pInputBaseName );
		return FALSE;
	}
	else if (!g_UseGameDir)
	{
		V_strcpy_safe(outputDir, pInputBaseName);
		V_StripFilename(outputDir);
	}

	// Usage:
	//			vtex -nop4 -dontusegamedir -quickconvert u:\data\game\tf\texture.vtf
	// Will read the old texture format and write the new texture format
	//
	if ( CommandLine()->FindParm( "-quickconvert" ) )
	{
		printf( "Quick convert of '%s'...\n", pInputBaseName );

		char chFileNameConvert[ 512 ];
		V_sprintf_safe( chFileNameConvert, "%s.vtf", pInputBaseName );

		IVTFTexture *pVtf = CreateVTFTexture();
		RunCodeAtScopeExit(DestroyVTFTexture( pVtf ));

		CUtlBuffer bufFile;
		LoadFile( chFileNameConvert, bufFile, true, NULL );
		bool bRes = pVtf->Unserialize( bufFile );
		if ( !bRes )
			VTexError( "Failed to read '%s'!\n", chFileNameConvert );

		// Determine the CRC if it was there
		// CRC32_t uiDataHash = 0;
		// CRC32_t *puiDataHash = &uiDataHash;
		// Assert( sizeof( uiDataHash ) == sizeof( int ) );
		// if ( !pVtf->GetResourceData( VTexConfigInfo_t::VTF_INPUTSRC_CRC, ... ) )

		AttachShtFile( pInputBaseName, pVtf, NULL );

		// Update the CRC
		// if ( puiDataHash )
		// {
		//	pVtf->InitResourceDataSection( VTexConfigInfo_t::VTF_INPUTSRC_CRC, *puiDataHash );
		// }
		// Remove the CRC when quick-converting
		pVtf->SetResourceData( VTexConfigInfo_t::VTF_INPUTSRC_CRC, NULL, 0 );

		bufFile.Clear();
		bRes = pVtf->Serialize( bufFile );
		if ( !bRes )
			VTexError( "Failed to write '%s'!\n", chFileNameConvert );

		auto [fw, rc] = se::posix::posix_file_stream_factory::open(chFileNameConvert, "wb");
		if ( !rc )
		{
			const intp write = bufFile.TellPut();
			size_t written;

			std::tie(written, rc) = fw.write( bufFile.Base(), 1, write );
			if ( rc || static_cast<size_t>( write ) != written )
				VTexError( "Failed to write %zd bytes to '%s'. Written %zu bytes: %s!\n",
					write, chFileNameConvert, written, rc.message().c_str() );
			else
				printf( "... succeeded.\n" );
		}
		else
			VTexError( "Failed to open '%s' for writing: %s!\n", chFileNameConvert, rc.message().c_str() );

		return TRUE;
	}

	VTexConfigInfo_t info;
	bool isCubeMap = false;
	if ( !LoadConfigFile( pInputBaseName, info, &isCubeMap ) )
		return FALSE;

	if( !isCubeMap )
	{
		isCubeMap = IsCube( pInputBaseName );
	}

	if( ( info.m_nStartFrame == -1 && info.m_nEndFrame != -1 ) ||
		( info.m_nStartFrame != -1 && info.m_nEndFrame == -1 ) )
	{
		VTexError( "%s: If you use startframe, you must use endframe, and vice versa.\n", pInputBaseName );
		return FALSE;
	}

	const char *pBaseName = &pInputBaseName[strlen( pInputBaseName ) - 1];
	while( (pBaseName >= pInputBaseName) && *pBaseName != '\\' && *pBaseName != '/' )
	{
		pBaseName--;
	}
	pBaseName++;

	bool bProcessedFilesOK = ProcessFiles( pInputBaseName, outputDir, pBaseName, isCubeMap, info );
	if ( !bProcessedFilesOK )
		return FALSE;

	// create vmts if necessary
	if( g_ShaderName )
	{
		char buf[1024];
		V_sprintf_safe( buf, "%s/%s.vmt", outputDir, pBaseName );
		const char *tmp = Q_stristr( outputDir, "materials" );
		if( tmp )
		{
			// check if the file already exists.
			auto [exists, rc] = se::posix::posix_file_stream::exists( buf );
			if( exists && !rc )
			{
				if ( !g_Quiet )
					fprintf( stderr, "vmt file '%s' already exists\n", buf );
			}
			else
			{
				auto [fp, rc2] = se::posix::posix_file_stream_factory::open( buf, "r" );
				if( !rc2 )
				{
					if ( !g_Quiet )
						fprintf( stderr, "Creating vmt file: %s/%s\n", tmp, pBaseName );

					tmp += ssize( "materials/" ) - 1;

					std::tie(std::ignore, rc) = fp.print( "\"%s\"\n", g_ShaderName );
					std::tie(std::ignore, rc) = fp.print( "{\n" );
					std::tie(std::ignore, rc) = fp.print( "\t\"$baseTexture\" \"%s/%s\"\n", tmp, pBaseName );

					for( int i=0;i<g_NumVMTParams;i++ )
					{
						std::tie(std::ignore, rc) = fp.print( "\t\"%s\" \"%s\"\n", g_VMTParams[i].m_szParam, g_VMTParams[i].m_szValue );
					}

					std::tie(std::ignore, rc) = fp.print( "}\n" );

					CP4AutoAddFile autop4( buf );
				}
				else
				{
					VTexWarning( "Couldn't open '%s' for writing: %s.\n", buf, rc2.message().c_str() );
				}
			}

		}
		else
		{
			VTexWarning( "Couldn't find 'materials/' in output path\n", buf );
		}
	}

	return TRUE;
}

static SpewRetval_t VTexOutputFunc( SpewType_t spewType, char const *pMsg )
{
	if (spewType == SPEW_ERROR || spewType == SPEW_WARNING)
	{
		fprintf(stderr, "%s", pMsg);
	}
	else
	{
		printf( "%s", pMsg );
	}

	Plat_DebugString(pMsg);

	if (spewType == SPEW_ERROR)
	{
		Pause();
		return SPEW_ABORT;
	}
	return (spewType == SPEW_ASSERT) ? SPEW_DEBUGGER : SPEW_CONTINUE; 
}


class CVTex : public CTier2AppSystem< IVTex >, public ILaunchableDLL
{
public:
	int VTex( int argc, char **argv ) override;

	// ILaunchableDLL, used by vtex executable.
	int main( int argc, char **argv ) override
	{
		// dimhotepus: Install an exception handler only when running as standalone executable.
		const se::utils::common::ScopedDefaultMinidumpHandler scoped_default_minidumps;

		// dimhotepus: Store old spew to restore later.
		const ScopedSpewOutputFunc scoped_spew_output{VTexOutputFunc};

		g_bUsedAsLaunchableDLL = true;

		// Being used as a launchable DLL, we don't want to blow away the host app's command line
		const CUtlString original_cmd_line( CommandLine()->GetCmdLine() );

		// Run the vtex logic
		const int rc = VTex( argc, argv );

		// Restore command line
		CommandLine()->CreateCmdLine( original_cmd_line.Get() );

		return rc;
	}

	int VTex( CreateInterfaceFn fsFactory, const char *pGameDir, int argc, char **argv ) override
	{
		g_pFileSystem = g_pFullFileSystem = (IFileSystem*)fsFactory( FILESYSTEM_INTERFACE_VERSION, nullptr );
		if ( !g_pFileSystem )
		{
			Error( "IVTex3::VTex - fsFactory can't get '%s' interface.", FILESYSTEM_INTERFACE_VERSION );
			return 0;
		}

		V_strcpy_safe( gamedir, pGameDir );
		V_AppendSlash( gamedir );

		// When being used embedded in a host app, we don't want to blow away the host app's command line
		CUtlString original_cmd_line( CommandLine()->GetCmdLine() );

		int rc = VTex( argc, argv );

		// Restore command line
		CommandLine()->CreateCmdLine( original_cmd_line.Get() );

		return rc;
	}
};

static class CSuggestGameDirHelper
{
public:
	static bool SuggestFn( CFSSteamSetupInfo const *pFsSteamSetupInfo, char *pchPathBuffer, int nBufferLength, bool *pbBubbleDirectories );
	bool MySuggestFn( CFSSteamSetupInfo const *pFsSteamSetupInfo, char *pchPathBuffer, int nBufferLength, bool *pbBubbleDirectories );

public:
	CSuggestGameDirHelper() : m_pszInputFiles( nullptr ), m_numInputFiles( 0 ) {}

public:
	char const * const *m_pszInputFiles;
	// dimhotepus: size_t -> int.
	int m_numInputFiles;
} g_suggestGameDirHelper;

bool CSuggestGameDirHelper::SuggestFn( CFSSteamSetupInfo const *pFsSteamSetupInfo, char *pchPathBuffer, int nBufferLength, bool *pbBubbleDirectories )
{
	return g_suggestGameDirHelper.MySuggestFn( pFsSteamSetupInfo, pchPathBuffer, nBufferLength, pbBubbleDirectories );
}

bool CSuggestGameDirHelper::MySuggestFn( CFSSteamSetupInfo const *pFsSteamSetupInfo, char *pchPathBuffer, int nBufferLength, bool *pbBubbleDirectories )
{
	if ( !m_numInputFiles || !m_pszInputFiles )
		return false;

	if ( pbBubbleDirectories )
		*pbBubbleDirectories = true;

	for ( int k = 0; k < m_numInputFiles; ++ k )
	{
		V_MakeAbsolutePath( pchPathBuffer, nBufferLength, m_pszInputFiles[ k ] );
		return true;
	}

	return false;
}

int CVTex::VTex( int argc, char **argv )
{
	CommandLine()->CreateCmdLine( argc, argv );

	if ( argc < 2 )
	{
		Usage();
		return -1;
	}

	// dimhotepus: Add header.
#ifdef PLATFORM_64BITS
	Msg("Valve Software - vtex [64 bit] (%s)\n", __DATE__);
#else
	Msg("Valve Software - vtex (%s)\n", __DATE__);
#endif

	// dimhotepus: Apply en_US UTF8 locale for printf/scanf.
	//
	// Printf/sscanf functions expect en_US UTF8 localization.
	//
	// Starting in Windows 10 version 1803 (10.0.17134.0), the Universal C Runtime
	// supports using a UTF-8 code page.
	constexpr char kEnUsUtf8Locale[]{"en_US.UTF-8"};

	const se::ScopedAppLocale scoped_app_locale{kEnUsUtf8Locale};
	if (V_stricmp(se::ScopedAppLocale::GetCurrentLocale(), kEnUsUtf8Locale)) {
		Warning("setlocale('%s') failed, current locale is '%s'.\n",
			kEnUsUtf8Locale, se::ScopedAppLocale::GetCurrentLocale());
	}

	MathLib_Init( 2.2f, 2.2f, 0.0f, 1, false, false, false, false );

	g_UseGameDir = true; // make sure this is initialized to true.
	const char *p4ChangelistLabel = "VTex Auto Checkout";
	bool bCreatedFilesystem = false;

	int i = 1;
	while( i < argc )
	{
		if( stricmp( argv[i], "-quiet" ) == 0 )
		{
			i++;
			g_Quiet = true;
			g_NoPause = true; // no point in pausing if we aren't going to print anything out.
		}
		else if( stricmp( argv[i], "-nopause" ) == 0 )
		{
			i++;
			g_NoPause = true;
		}
		else if ( stricmp( argv[i], "-WarningsAsErrors" ) == 0 )
		{
			i++;
			g_bWarningsAsErrors = true;
		}
		else if ( stricmp( argv[i], "-UseStandardError" ) == 0 )
		{
			i++;
			g_bUseStandardError = true;
		}
		else if ( stricmp( argv[i], "-nopsd" ) == 0 ) 
		{
			i++;
			g_bNoPsd = true;
		}
		else if ( stricmp( argv[i], "-notga" ) == 0 ) 
		{
			i++;
			g_bNoTga = true;
		}
		else if ( stricmp( argv[i], "-nomkdir" ) == 0 ) 
		{
			i++;
			g_CreateDir = false;
		}
		else if ( stricmp( argv[i], "-mkdir" ) == 0 ) 
		{
			i++;
			g_CreateDir = true;
		}
		else if ( stricmp( argv[i], "-game" ) == 0 )
		{
			i += 2;
		}
		else if ( stricmp( argv[i], "-outdir" ) == 0 )
		{
			V_strcpy_safe( g_ForcedOutputDir, argv[i+1] );
			i += 2;
		}
		else if ( stricmp( argv[i], "-p4changelistlabel" ) == 0 )
		{
			p4ChangelistLabel = argv[i+1];
			i += 2;
		}
		else if ( stricmp( argv[i], "-p4skipchangelistlabel" ) == 0 )
		{
			p4ChangelistLabel = NULL;
			i++;
		}
		else if ( stricmp( argv[i], "-dontusegamedir" ) == 0)
		{
			++i;
			g_UseGameDir = false;
		}
		else if( stricmp( argv[i], "-shader" ) == 0 )
		{
			i++;
			if( i < argc )
			{
				g_ShaderName = argv[i];
				i++;
			}
		}
		else if( stricmp( argv[i], "-vproject" ) == 0 )
		{
			// skip this one. . we dont' use it internally.
			i += 2;
		}
		else if( stricmp( argv[i], "-allowdebug" ) == 0 )
		{
			// skip this one. . we dont' use it internally.
			i++;
		}
		else if( stricmp( argv[i], "-vmtparam" ) == 0 )
		{
			if( g_NumVMTParams < MAX_VMT_PARAMS )
			{
				i++;
				if( i < argc - 1 )
				{
					g_VMTParams[g_NumVMTParams].m_szParam = argv[i];
					i++;

					if( i < argc - 1 )
					{
						g_VMTParams[g_NumVMTParams].m_szValue = argv[i];
						i++;
					}
					else
					{
						g_VMTParams[g_NumVMTParams].m_szValue = "";
					}

					if( !g_Quiet )
					{
						printf( "Adding .vmt parameter: \"%s\"\t\"%s\"\n", 
							g_VMTParams[g_NumVMTParams].m_szParam,
							g_VMTParams[g_NumVMTParams].m_szValue );
					}

					g_NumVMTParams++;
				}
			}
			else
			{
				fprintf( stderr, "Exceeded max number of vmt parameters, extra ignored (max %d)\n", MAX_VMT_PARAMS );
			}
		}
		else if( stricmp( argv[i], "-nop4" ) == 0 )
		{
			// Just here to signify that -nop4 is a valid flag
			++ i;
		}
		else if( stricmp( argv[i], "-deducepath" ) == 0 )
		{
			// Just here to signify that -deducepath is a valid flag
			++ i;
		}
		else if( stricmp( argv[i], "-quickconvert" ) == 0 )
		{
			// Just here to signify that -quickconvert is a valid flag
			++ i;
		}
		else if( stricmp( argv[i], "-crcvalidate" ) == 0 )
		{
			// Just here to signify that -crcvalidate is a valid flag
			++ i;
		}
		else if( stricmp( argv[i], "-crcforce" ) == 0 )
		{
			// Just here to signify that -crcforce is a valid flag
			++ i;
		}
		else if( stricmp( argv[i], "-p4skip" ) == 0 )
		{
			// Just here to signify that -p4skip is a valid flag
			++ i;
		}
		else
		{
			break;
		}
	}

	// Set the suggest game info directory helper
	g_suggestGameDirHelper.m_pszInputFiles = argv + i;
	g_suggestGameDirHelper.m_numInputFiles = argc - i;
	const ScopedSuggestGameInfoDir scoped_suggest_game_info_dir( CSuggestGameDirHelper::SuggestFn );

	// g_pFileSystem may have been inherited with -inherit_filesystem.
	if (g_UseGameDir && !g_pFileSystem)
	{
		FileSystem_Init( argv[i] );
		bCreatedFilesystem = true;

		Q_FixSlashes( gamedir, '/' );
	}

	if ( !CommandLine()->FindParm( "-p4skip" ) )
	{
		// Initialize P4
		bool bP4DLLExists = false;
		if ( g_pFullFileSystem )
		{
			bP4DLLExists = g_pFullFileSystem->FileExists( "p4lib" DLL_EXT_STRING, "EXECUTABLE_PATH" );
		}

		if ( g_bUsedAsLaunchableDLL && !CommandLine()->FindParm( "-nop4" ) && bP4DLLExists )
		{
			constexpr char pModuleName[] = "p4lib" DLL_EXT_STRING;
			CSysModule *pModule = Sys_LoadModule( pModuleName );
			if ( !pModule )
			{
				fprintf( stderr, "Can't load %s.\n", pModuleName );
				return -1;
			}
			CreateInterfaceFnT<IP4> fn = Sys_GetFactory<IP4>( pModule );
			if ( !fn )
			{
				fprintf( stderr, "Can't get factory from %s.\n", pModuleName );
				Sys_UnloadModule( pModule );
				return -1;
			}
			
			// dimhotepus: No Perforce support.
			// p4 = fn( P4_INTERFACE_VERSION, NULL );
			// if ( !p4 )
			{
				fprintf( stderr, "Can't get IP4 interface from %s, proceeding with -nop4.\n", pModuleName );
				g_p4factory->SetDummyMode( true );
			}
			// else
			// {
			// 	p4->Connect( FileSystem_GetFactory() );
			// 	p4->Init();
			// }
		}
		else
		{
			g_p4factory->SetDummyMode( true );
		}

		// Setup p4 factory
		if ( p4ChangelistLabel && p4ChangelistLabel[0] != '\0' )
		{
			// Set the named changelist
			g_p4factory->SetOpenFileChangeList( p4ChangelistLabel );
		}
	}

	// Parse args
	for( ; i < argc; i++ )
	{
		if ( argv[i][0] == '-' )
			continue; // Assuming flags

		char pInputBaseName[MAX_PATH];
		V_strcpy_safe( pInputBaseName, argv[i] );

		if ( !Q_strstr( pInputBaseName, "*." ) )
		{
			Process_File( pInputBaseName );
			continue;
		}

#ifdef WIN32
		char	search[ 128 ];
		char	basedir[MAX_PATH];
		char	ext[_MAX_EXT];
		char    filename[_MAX_FNAME];

		_splitpath( pInputBaseName, NULL, NULL, NULL, ext ); //find extension wanted

		if ( !V_ExtractFilePath ( pInputBaseName, basedir ) )
			V_strcpy_safe( basedir, ".\\" );

		V_sprintf_safe( search, "%s\\*.*", basedir );

		WIN32_FIND_DATA wfd = {};

		HANDLE hResult = FindFirstFile( search, &wfd );
		if ( hResult != INVALID_HANDLE_VALUE )
		{
			RunCodeAtScopeExit(FindClose( hResult ));

			V_sprintf_safe( filename, "%s%s", basedir, wfd.cFileName );

			if ( wfd.cFileName[0] != '.' ) 
				Process_File( filename );

			int iFFType = Find_Files( wfd, hResult, basedir, ext );

			while ( iFFType )
			{
				V_sprintf_safe( filename, "%s%s", basedir, wfd.cFileName );

				if ( wfd.cFileName[0] != '.' && iFFType != FF_DONTPROCESS ) 
					Process_File( filename );

				iFFType = Find_Files( wfd, hResult, basedir, ext );
			}
		}
#endif

	}
	
	// dimhotepus: Disable Perforce support.
	// Shutdown P4
	// if ( g_bUsedAsLaunchableDLL && p4 && !CommandLine()->FindParm( "-p4skip" ) )
	// {
	// 	p4->Shutdown();
	// 	p4->Disconnect();
	// }

	if ( bCreatedFilesystem )
	{
		FileSystem_Term();
	}

	Pause();
	return 0;
}

CVTex g_VTex;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CVTex, IVTex, IVTEX_VERSION_STRING, g_VTex );
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CVTex, ILaunchableDLL, LAUNCHABLE_DLL_INTERFACE_VERSION, g_VTex );
