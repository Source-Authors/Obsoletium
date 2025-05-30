//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

#ifndef FLOAT_BM_H
#define FLOAT_BM_H

#ifdef _WIN32
#pragma once
#endif

#include <algorithm>  // clamp
#include <cstring>
#include <utility>    // min, max

#include "tier0/commonmacros.h"
#include "tier0/dbg.h"
#include "tier0/platform.h"
#include "mathlib/vector.h"

struct PixRGBAF 
{
	float Red;
	float Green;
	float Blue;
	float Alpha;
};

struct PixRGBA8
{
	unsigned char Red;
	unsigned char Green;
	unsigned char Blue;
	unsigned char Alpha;
};

[[nodiscard]] constexpr inline PixRGBAF PixRGBA8_to_F( PixRGBA8 x )
{
	PixRGBAF f{x.Red / 255.f, x.Green / 255.f, x.Blue / 255.f, x.Alpha / 255.f};
	return f;
}

[[nodiscard]] constexpr inline PixRGBA8 PixRGBAF_to_8( PixRGBAF const &f )
{
	PixRGBA8 x{
		static_cast<unsigned char>(max( 0.f, min( 255.f,255.f*f.Red ) )),
		static_cast<unsigned char>(max( 0.f, min( 255.f,255.f*f.Green ) )),
		static_cast<unsigned char>(max( 0.f, min( 255.f,255.f*f.Blue ) )),
		static_cast<unsigned char>(max( 0.f, min( 255.f,255.f*f.Alpha ) ))
	};
	return x;
}

constexpr int SPFLAGS_MAXGRADIENT{1};

// bit flag options for ComputeSelfShadowedBumpmapFromHeightInAlphaChannel:
// generate ambient occlusion only
constexpr int SSBUMP_OPTION_NONDIRECTIONAL{1};
// scale so that a flat unshadowed
// value is 0.5, and bake rgb luminance
// in.
constexpr int SSBUMP_MOD2X_DETAIL_TEXTURE{2};

abstract_class IFileSystem;

// Writes PFM using filesystem.
[[nodiscard]] bool PFMWrite(IFileSystem *file_system,
                            float *img, const char *file_path, int width, int height);

// Writes PFM using stdio.
[[nodiscard]] bool PFMWrite(float *img, const char *file_path, int width, int height);

class FloatBitMap_t 
{
public:
	int Width, Height;										// bitmap dimensions
	float *RGBAData;										// actual data

	FloatBitMap_t()										// empty one
	{
		Width=Height=0;
		RGBAData=nullptr;
	}

	FloatBitMap_t(int width, int height);                  // make one and allocate space
	FloatBitMap_t(char const *filename);                   // read one from a file (tga or pfm)
	FloatBitMap_t(FloatBitMap_t const *orig);

	// dimhotepus: Add API to check validity.
	[[nodiscard]] bool IsValid() const { return RGBAData != nullptr; }

	// quantize one to 8 bits
	[[nodiscard]] bool WriteTGAFile(char const *filename) const;

	[[nodiscard]] bool LoadFromPFM(char const *filename);					// load from floating point pixmap (.pfm) file
	[[nodiscard]] bool WritePFM(char const *filename) const;				// save to floating point pixmap (.pfm) file


	void InitializeWithRandomPixelsFromAnotherFloatBM(FloatBitMap_t const &other) const;

	[[nodiscard]] inline float & Pixel(int x, int y, int comp) const
	{
		Assert((x>=0) && (x<Width));
		Assert((y>=0) && (y<Height));
		return RGBAData[4*(x+Width*y)+comp];
	}

	[[nodiscard]] inline float & PixelWrapped(int x, int y, int comp) const
	{
		// like Pixel except wraps around to other side
		if (x < 0)
			x+=Width;
		else
			if (x>= Width)
				x -= Width;

		if ( y < 0 )
			y+=Height;
		else
			if ( y >= Height )
				y -= Height;

		return RGBAData[4*(x+Width*y)+comp];
	}

	[[nodiscard]] inline float & PixelClamped(int x, int y, int comp) const
	{
		// like Pixel except wraps around to other side
		x=std::clamp(x,0,Width-1);
		y=std::clamp(y,0,Height-1);
		return RGBAData[4*(x+Width*y)+comp];
	}


	[[nodiscard]] inline float & Alpha(int x, int y) const
	{
		Assert((x>=0) && (x<Width));
		Assert((y>=0) && (y<Height));
		return RGBAData[3+4*(x+Width*y)];
	}


	// look up a pixel value with bilinear interpolation
	[[nodiscard]] float InterpolatedPixel(float x, float y, int comp) const;

	[[nodiscard]] inline PixRGBAF PixelRGBAF(int x, int y) const
	{
		Assert((x>=0) && (x<Width));
		Assert((y>=0) && (y<Height));
		
		int RGBoffset= 4*(x+Width*y); //-V112
		PixRGBAF RetPix {
			RGBAData[RGBoffset+0],
			RGBAData[RGBoffset+1],
			RGBAData[RGBoffset+2],
			RGBAData[RGBoffset+3]
		};

		return RetPix;
	}


	inline void WritePixelRGBAF(int x, int y, PixRGBAF value) const
	{
		Assert((x>=0) && (x<Width));
		Assert((y>=0) && (y<Height));

		int RGBoffset= 4*(x+Width*y); //-V112
		RGBAData[RGBoffset+0]= value.Red;
		RGBAData[RGBoffset+1]= value.Green;
		RGBAData[RGBoffset+2]= value.Blue;
		RGBAData[RGBoffset+3]= value.Alpha;

	}


	inline void WritePixel(int x, int y, int comp, float value)
	{
		Assert((x>=0) && (x<Width));
		Assert((y>=0) && (y<Height));
		RGBAData[4*(x+Width*y)+comp]= value;
	}

	// paste, performing boundary matching. Alpha channel can be used to make
	// brush shape irregular
	void SmartPaste(FloatBitMap_t const &brush, int xofs, int yofs, uint32 flags);

	// force to be tileable using poisson formula
	void MakeTileable(void) const;

	void ReSize(int NewXSize, int NewYSize);

	// find the bounds of the area that has non-zero alpha.
	void GetAlphaBounds(int &minx, int &miny, int &maxx,int &maxy) const;

	// Solve the poisson equation for an image. The alpha channel of the image controls which
	// pixels are "modifiable", and can be used to set boundary conditions. Alpha=0 means the pixel
	// is locked.  deltas are in the order [(x,y)-(x,y-1),(x,y)-(x-1,y),(x,y)-(x+1,y),(x,y)-(x,y+1)
	void Poisson(FloatBitMap_t *deltas[4],
		int n_iters,
		uint32 flags                                  // SPF_xxx
		);

	[[nodiscard]] ALLOC_CALL FloatBitMap_t *QuarterSize() const;					// get a new one downsampled 
	[[nodiscard]] ALLOC_CALL FloatBitMap_t *QuarterSizeBlocky() const;          // get a new one downsampled 

	[[nodiscard]] ALLOC_CALL FloatBitMap_t *QuarterSizeWithGaussian() const;		// downsample 2x using a gaussian


	void RaiseToPower(float pow) const;
	void ScaleGradients(void);
	void Logize(void) const;                                        // pix=log(1+pix)
	void UnLogize(void) const;                                      // pix=exp(pix)-1

	// compress to 8 bits converts the hdr texture to an 8 bit texture, encoding a scale factor
	// in the alpha channel. upon return, the original pixel can be (approximately) recovered
	// by the formula rgb*alpha*overbright. 
	// this function performs special numerical optimization on the texture to minimize the error
	// when using bilinear filtering to read the texture.
	void CompressTo8Bits(float overbright) const;
	// decompress a bitmap converted by CompressTo8Bits
	void Uncompress(float overbright) const;


	[[nodiscard]] Vector AverageColor() const;								// average rgb value of all pixels
	[[nodiscard]] float BrightestColor() const;								// highest vector magnitude

	void Clear(float r, float g, float b, float alpha) const;		// set all pixels to speicifed values (0..1 nominal)

	void ScaleRGB(float scale_factor) const;						// for all pixels, r,g,b*=scale_factor

	// given a bitmap with height stored in the alpha channel, generate vector positions and normals
	void ComputeVertexPositionsAndNormals( float flHeightScale, Vector * RESTRICT *ppPosOut, Vector * RESTRICT *ppNormalOut ) const;

	// generate a normal map with height stored in alpha.  uses hl2 tangent basis to support baked
	// self shadowing.  the bump scale maps the height of a pixel relative to the edges of the
	// pixel. This function may take a while - many millions of rays may be traced.  applications
	// using this method need to link w/ raytrace.lib
	[[nodiscard]] ALLOC_CALL FloatBitMap_t *ComputeSelfShadowedBumpmapFromHeightInAlphaChannel(
		float bump_scale, int nrays_to_trace_per_pixel=100, 
		uint32 nOptionFlags = 0								// SSBUMP_OPTION_XXX
		) const;


	// generate a conventional normal map from a source with height stored in alpha.
	[[nodiscard]] ALLOC_CALL FloatBitMap_t *ComputeBumpmapFromHeightInAlphaChannel( float bump_scale ) const;


	// bilateral (edge preserving) smoothing filter. edge_threshold_value defines the difference in
	// values over which filtering will not occur. Each channel is filtered independently. large
	// radii will run slow, since the bilateral filter is neither separable, nor is it a
	// convolution that can be done via fft.
	void TileableBilateralFilter( int radius_in_pixels, float edge_threshold_value );

	~FloatBitMap_t();

	RESTRICT_FUNC float* AllocateRGB(int w, int h)
	{
		delete[] RGBAData;
		RGBAData = new float[w*h*4];
		Width=w;
		Height=h;
		return RGBAData;
	}
};


// a FloatCubeMap_t holds the floating point bitmaps for 6 faces of a cube map
class FloatCubeMap_t
{
public:
	FloatBitMap_t face_maps[6];

	FloatCubeMap_t(int xfsize, int yfsize)
	{
		// make an empty one with face dimensions xfsize x yfsize
		for(int f=0;f<6;f++)
			face_maps[f].AllocateRGB(xfsize,yfsize);
	}

	// load basenamebk,pfm, basenamedn.pfm, basenameft.pfm, ...
	FloatCubeMap_t(char const *basename);

	// save basenamebk,pfm, basenamedn.pfm, basenameft.pfm, ...
	[[nodiscard]] bool WritePFMs(char const *basename) const;

	[[nodiscard]] Vector AverageColor() const
	{
		Vector ret{0, 0, 0};

		int nfaces = 0;
		for (auto &map : face_maps)
			if (map.RGBAData)
			{
				nfaces++;
				ret += map.AverageColor();
			}

		if (nfaces)
			ret *= 1.0f / nfaces;

		return ret;
	}

	[[nodiscard]] float BrightestColor() const
	{
		float ret=0.0;

		for (auto &map : face_maps)
			if (map.RGBAData)
			{
				ret = max(ret, map.BrightestColor());
			}

		return ret;
	}


	// resample a cubemap to one of possibly a lower resolution, using a given phong exponent.
	// dot-product weighting will be used for the filtering operation.
	void Resample(FloatCubeMap_t &dest, float flPhongExponent);

	// returns the normalized direciton vector through a given pixel of a given face
	[[nodiscard]] Vector PixelDirection(int face, int x, int y) const;

	// returns the direction vector throught the center of a cubemap face
	[[nodiscard]] Vector FaceNormal( int nFaceNumber );
};

// Image Pyramid class.
constexpr int MAX_IMAGE_PYRAMID_LEVELS{16};  // up to 64kx64k;

enum ImagePyramidMode_t 
{
	PYRAMID_MODE_GAUSSIAN,
};

class FloatImagePyramid_t
{
public:
	int m_nLevels;
	FloatBitMap_t *m_pLevels[MAX_IMAGE_PYRAMID_LEVELS];		// level 0 is highest res

	FloatImagePyramid_t()
	{
		m_nLevels=0;
		memset(m_pLevels,0,sizeof(m_pLevels));
	}

	// build one. clones data from src for level 0.
	FloatImagePyramid_t(FloatBitMap_t const &src, ImagePyramidMode_t mode);

	// read or write a Pixel from a given level. All coordinates are specified in the same domain as the base level.
	[[nodiscard]] float &Pixel(int x, int y, int component, int level) const;

	[[nodiscard]] FloatBitMap_t *Level(int lvl) const
	{
		Assert(lvl<m_nLevels);
		Assert(lvl<ssize(m_pLevels));
		return m_pLevels[lvl];
	}
	// rebuild all levels above the specified level
	void ReconstructLowerResolutionLevels(int starting_level);

	~FloatImagePyramid_t();

	[[nodiscard]] bool WriteTGAs(char const *basename) const;				// outputs name_00.tga, name_01.tga,...
};

#endif
