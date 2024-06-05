//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Local header for CVTFTexture class declaration - allows platform-specific
//			implementation to be placed in separate cpp files.
//
// $NoKeywords: $
//===========================================================================//

#ifndef CVTF_H
#define CVTF_H

#include <cstring>

#include "s3tc_decode.h"
#include "vtf/vtf.h"
#include "tier1/byteswap.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlvector.h"
#include "bitmap/imageformat.h"
#include "tier0/basetypes.h"
#include "tier0/platform.h"
#include "mathlib/vector.h"

class CEdgePos
{
public:
	CEdgePos() = default;
	CEdgePos( int ix, int iy )
	{
		x = ix;
		y = iy;
	}

	void operator +=( const CEdgePos &other ) 
	{ 
		x += other.x;
		y += other.y;
	}

	void operator /=( int val )
	{ 
		x /= val;
		y /= val;
	}

	CEdgePos operator >>( int shift )
	{ 
		return CEdgePos( x >> shift, y >> shift );
	}

	CEdgePos operator *( int shift )
	{ 
		return CEdgePos( x * shift, y * shift );
	}

	CEdgePos operator -( const CEdgePos &other ) 
	{ 
		return CEdgePos( x - other.x, y - other.y );
	}

	CEdgePos operator +( const CEdgePos &other ) 
	{ 
		return CEdgePos( x + other.x, y + other.y );
	}

	bool operator!=( const CEdgePos &other )
	{
		return !( *this == other );
	}

	bool operator==( const CEdgePos &other )
	{
		return x==other.x && y==other.y;
	}

	int x, y;
};


struct CEdgeIncrements
{
	CEdgePos iFace1Start, iFace1End;
	CEdgePos iFace1Inc, iFace2Inc;
	CEdgePos iFace2Start, iFace2End;
};


struct CEdgeMatch
{
	int m_iFaces[2];	// Which faces are touching.
	int m_iEdges[2];	// Which edge on each face is touching.
	int m_iCubeVerts[2];// Which of the cube's verts comprise this edge?
	bool m_bFlipFace2Edge;
};


struct CCornerMatch
{
	// The info for the 3 edges that match at this corner.
	int m_iFaces[3];
	int m_iFaceEdges[3];
};


struct CEdgeFaceIndex
{
	int m_iEdge;
	int m_iFace;
};


constexpr int NUM_EDGE_MATCHES{12};
constexpr int NUM_CORNER_MATCHES{8};


//-----------------------------------------------------------------------------
// Implementation of the VTF Texture
//-----------------------------------------------------------------------------
class CVTFTexture final : public IVTFTexture
{
public:
	CVTFTexture();
	virtual ~CVTFTexture();

	bool Init( int nWidth, int nHeight, int nDepth, ImageFormat fmt, int iFlags, int iFrameCount, int nForceMipCount ) override;

	// Methods to initialize the low-res image
	void InitLowResImage( int nWidth, int nHeight, ImageFormat fmt ) override;

	void *SetResourceData( uint32 eType, void const *pData, size_t nDataSize ) override;
	void *GetResourceData( uint32 eType, size_t *pDataSize ) const override;

	// Locates the resource entry info if it's present, easier than crawling array types
	bool HasResourceEntry( uint32 eType ) const override;

	// Retrieve available resource types of this IVTFTextures
	//		arrTypesBuffer			buffer to be filled with resource types available.
	//		numTypesBufferElems		how many resource types the buffer can accomodate.
	// Returns:
	//		number of resource types available (can be greater than "numTypesBufferElems"
	//		in which case only first "numTypesBufferElems" are copied to "arrTypesBuffer")
	size_t GetResourceTypes( uint32 *arrTypesBuffer, size_t numTypesBufferElems ) const override;

	// Methods to set other texture fields
	void SetBumpScale( float flScale ) override;
	void SetReflectivity( const Vector &vecReflectivity ) override;

	// These are methods to help with optimization of file access
	void LowResFileInfo( intp *pStartLocation, intp *pSizeInBytes ) const override;
	void ImageFileInfo( int nFrame, int nFace, int nMip, intp *pStartLocation, intp *pSizeInBytes) const override;
	intp FileSize( int nMipSkipCount = 0 ) const override;

	// When unserializing, we can skip a certain number of mip levels,
	// and we also can just load everything but the image data
	bool Unserialize( CUtlBuffer &buf, bool bBufferHeaderOnly = false, int nSkipMipLevels = 0 ) override;
	bool UnserializeEx( CUtlBuffer &buf, bool bHeaderOnly = false, int nForceFlags = 0, int nSkipMipLevels = 0 ) override;
	bool Serialize( CUtlBuffer &buf ) override;

	void GetMipmapRange( int* pOutFinest, int* pOutCoarsest ) override;

	// Attributes...
	int Width() const override;
	int Height() const override;
	int Depth() const override;
	int MipCount() const override;

	intp RowSizeInBytes( int nMipLevel ) const override;
	intp FaceSizeInBytes( int nMipLevel ) const override;

	ImageFormat Format() const override;
	int FaceCount() const override;
	int FrameCount() const override;
	int Flags() const override;

	float BumpScale() const override;
	const Vector &Reflectivity() const override;

	bool IsCubeMap() const override;
	bool IsNormalMap() const override;
	bool IsVolumeTexture() const override;

	int LowResWidth() const override;
	int LowResHeight() const override;
	ImageFormat LowResFormat() const override;

	// Computes the size (in bytes) of a single mipmap of a single face of a single frame 
	intp ComputeMipSize( int iMipLevel ) const override;

	// Computes the size (in bytes) of a single face of a single frame
	// All mip levels starting at the specified mip level are included
	intp ComputeFaceSize( int iStartingMipLevel = 0 ) const override;

	// Computes the total size of all faces, all frames
	intp ComputeTotalSize( ) const override;

	// Computes the dimensions of a particular mip level
	void ComputeMipLevelDimensions( int iMipLevel, int *pWidth, int *pHeight, int *pMipDepth ) const override;

	// Computes the size of a subrect (specified at the top mip level) at a particular lower mip level
	void ComputeMipLevelSubRect( Rect_t* pSrcRect, int nMipLevel, Rect_t *pSubRect ) const override;

	// Returns the base address of the image data
	unsigned char *ImageData() override;

	// Returns a pointer to the data associated with a particular frame, face, and mip level
	unsigned char *ImageData( int iFrame, int iFace, int iMipLevel ) override;

	// Returns a pointer to the data associated with a particular frame, face, mip level, and offset
	unsigned char *ImageData( int iFrame, int iFace, int iMipLevel, int x, int y, int z ) override;

	// Returns the base address of the low-res image data
	unsigned char *LowResImageData() override;

	// Converts the texture's image format. Use IMAGE_FORMAT_DEFAULT
	void ConvertImageFormat( ImageFormat fmt, bool bNormalToDUDV ) override;

	// Generate spheremap based on the current cube faces (only works for cubemaps)
	// The look dir indicates the direction of the center of the sphere
	void GenerateSpheremap( LookDir_t lookDir ) override;

	void GenerateHemisphereMap( unsigned char *pSphereMapBitsRGBA, int targetWidth, 
		int targetHeight, LookDir_t lookDir, int iFrame ) override;

	// Fixes the cubemap faces orientation from our standard to the
	// standard the material system needs.
	void FixCubemapFaceOrientation( ) override;

	// Normalize the top mip level if necessary
	virtual void NormalizeTopMipLevel();
	
	// Generates mipmaps from the base mip levels
	void GenerateMipmaps() override;

	// Put 1/miplevel (1..n) into alpha.
	void PutOneOverMipLevelInAlpha() override;

	// Computes the reflectivity
	void ComputeReflectivity( ) override;

	// Computes the alpha flags
	void ComputeAlphaFlags() override;

	// Gets the texture all internally consistent assuming you've loaded
	// mip 0 of all faces of all frames
	void PostProcess(bool bGenerateSpheremap, LookDir_t lookDir = LOOK_DOWN_Z, bool bAllowFixCubemapOrientation = true) override;
	void SetPostProcessingSettings( VtfProcessingOptions const *pOptions ) override;

	// Generate the low-res image bits
	bool ConstructLowResImage() override;

	void MatchCubeMapBorders( int iStage, ImageFormat finalFormat, bool bSkybox ) override;

	// Sets threshhold values for alphatest mipmapping
	void SetAlphaTestThreshholds( float flBase, float flHighFreq ) override;

private:
	// Unserialization
	bool ReadHeader( CUtlBuffer &buf, VTFFileHeader_t &header );

	void BlendCubeMapEdgePalettes(
		int iFrame,
		int iMipLevel,
		const CEdgeMatch *pMatch );

	void BlendCubeMapCornerPalettes(
		int iFrame,
		int iMipLevel,
		const CCornerMatch *pMatch );

	void MatchCubeMapS3TCPalettes(
		CEdgeMatch edgeMatches[NUM_EDGE_MATCHES],
		CCornerMatch cornerMatches[NUM_CORNER_MATCHES]
		);
	
	void SetupFaceVert( int iMipLevel, int iVert, CEdgePos &out );
	void SetupEdgeIncrement( CEdgePos &start, CEdgePos &end, CEdgePos &inc );

	void SetupTextureEdgeIncrements( 
		int iMipLevel,
		int iFace1Edge,
		int iFace2Edge,
		bool bFlipFace2Edge,
		CEdgeIncrements *incs );
	
	void BlendCubeMapFaceEdges(
		int iFrame,
		int iMipLevel,
		const CEdgeMatch *pMatch );

	void BlendCubeMapFaceCorners(
		int iFrame,
		int iMipLevel,
		const CCornerMatch *pMatch );

	void BuildCubeMapMatchLists( CEdgeMatch edgeMatches[NUM_EDGE_MATCHES], CCornerMatch cornerMatches[NUM_CORNER_MATCHES], bool bSkybox );

	// Allocate image data blocks with an eye toward re-using memory
	bool AllocateImageData( intp nMemorySize );
	bool AllocateLowResImageData( intp nMemorySize );

	// Compute the mip count based on the size + flags
	int ComputeMipCount( ) const;

	// Unserialization of low-res data
	bool LoadLowResData( CUtlBuffer &buf );

	// Unserialization of new resource data
	bool LoadNewResources( CUtlBuffer &buf );

	// Unserialization of image data
	bool LoadImageData( CUtlBuffer &buf, const VTFFileHeader_t &header, int nSkipMipLevels );

	// Shutdown
	void Shutdown();
	void ReleaseResources();

	// Makes a single frame of spheremap
	void ComputeSpheremapFrame( unsigned char **ppCubeFaces, unsigned char *pSpheremap, LookDir_t lookDir );

	// Makes a single frame of spheremap
	void ComputeHemispheremapFrame( unsigned char **ppCubeFaces, unsigned char *pSpheremap, LookDir_t lookDir );

	// Serialization of image data
	bool WriteImageData( CUtlBuffer &buf );

	// Computes the size (in bytes) of a single mipmap of a single face of a single frame 
	intp ComputeMipSize( int iMipLevel, ImageFormat fmt ) const;

	// Computes the size (in bytes) of a single face of a single frame
	// All mip levels starting at the specified mip level are included
	intp ComputeFaceSize( int iStartingMipLevel, ImageFormat fmt ) const;

	// Computes the total size of all faces, all frames
	intp ComputeTotalSize( ImageFormat fmt ) const;

	// Computes the location of a particular face, frame, and mip level
	intp GetImageOffset( int iFrame, int iFace, int iMipLevel, ImageFormat fmt ) const;

	// Determines if the vtf or vtfx file needs to be swapped to the current platform
	bool SetupByteSwap( CUtlBuffer &buf );

	// Locates the resource entry info if it's present
	ResourceEntryInfo *FindResourceEntryInfo( unsigned int eType );
	ResourceEntryInfo const *FindResourceEntryInfo( unsigned int eType ) const;
	
	// Inserts the resource entry info if it's not present
	ResourceEntryInfo *FindOrCreateResourceEntryInfo( unsigned int eType );

	// Removes the resource entry info if it's present
	bool RemoveResourceEntryInfo( unsigned int eType );

private:
	// This is to make sure old-format .vtf files are read properly
	int				m_nVersion[2];

	int				m_nWidth;
	int				m_nHeight;
	int 			m_nDepth;
	ImageFormat		m_Format;

	int				m_nMipCount;
	int				m_nFaceCount;
	int				m_nFrameCount;

	intp				m_nImageAllocSize;
	int				m_nFlags;
	unsigned char	*m_pImageData;

	Vector			m_vecReflectivity;
	float			m_flBumpScale;
	
	// FIXME: Do I need this?
	int				m_iStartFrame;

	// Low res data
	intp				m_nLowResImageAllocSize;
	ImageFormat		m_LowResImageFormat;
	int				m_nLowResImageWidth;
	int				m_nLowResImageHeight;
	unsigned char	*m_pLowResImageData;

	// Used while fixing mipmap edges.
	CUtlVector<S3RGBA> m_OriginalData;

	// Alpha threshholds
	float			m_flAlphaThreshhold;
	float			m_flAlphaHiFreqThreshhold;

	CByteswap		m_Swap;

	int				m_nFinestMipmapLevel;
	int				m_nCoarsestMipmapLevel;

	CUtlVector< ResourceEntryInfo > m_arrResourcesInfo;

	struct ResourceMemorySection
	{
		ResourceMemorySection() { memset( this, 0, sizeof( *this ) ); }

		intp				m_nDataAllocSize;
		intp				m_nDataLength;
		unsigned char	*m_pData;

		bool AllocateData( intp nMemorySize );
		bool LoadData( CUtlBuffer &buf, CByteswap &byteSwap );
		bool WriteData( CUtlBuffer &buf ) const;
	};
	CUtlVector< ResourceMemorySection > m_arrResourcesData;
	CUtlVector< ResourceMemorySection > m_arrResourcesData_ForReuse;	// Maintained to keep allocated memory blocks when unserializing from files

	VtfProcessingOptions m_Options;
};

#endif	// CVTF_H
