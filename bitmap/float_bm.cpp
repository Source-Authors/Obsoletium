//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "bitmap/float_bm.h"

#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <utility>

#include "tier2/tier2.h"
#include "bitmap/imageformat.h"
#include "bitmap/tgaloader.h"
#include "tier0/platform.h"
#include "tier0/wchartypes.h"
#include "tier1/strtools.h"
#include "filesystem.h"
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "posix_file_stream.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// linear interpolate between 2 control points (L,R)


constexpr static inline float LinInterp(float frac, float L, float R)
{
	return (((R-L) * frac) + L);
}

// bilinear interpolate between 4 control points (UL,UR,LL,LR)

constexpr static inline float BiLinInterp(float Xfrac, float Yfrac, float UL, float UR, float LL, float LR)
{
	float iu = LinInterp(Xfrac, UL, UR);
	float il = LinInterp(Xfrac, LL, LR);

	return( LinInterp(Yfrac, iu, il) );
}

[[nodiscard]] bool PFMWrite(IFileSystem* file_system, float* img,
							const char* file_path, int width, int height) {
  FileHandle_t f = file_system->Open(file_path, "wb");
  if (!f) {
    Warning("Unable to open PFM '%s' for writing.\n", file_path);
    return false;
  }
  RunCodeAtScopeExit(file_system->Close(f));

  if (!file_system->FPrintf(f, "PF\n%d %d\n-1.000000\n", width, height)) {
    Warning("Unable to write PFM header to '%s'.\n", file_path);
    return false;
  }

  for (int i = height - 1; i >= 0; i--) {
    const float *row = &img[3 * width * i];

    if (!file_system->Write(row, width * sizeof(float) * 3, f)) {
      Warning("Unable to write PFM row #%d to '%s'.\n", height - i,
              file_path);
      return false;
    }
  }

  return true;
}

[[nodiscard]] bool PFMWrite(float *img, const char *file_path, int width, int height) {
  auto [f, errc] = se::posix::posix_file_stream_factory::open(file_path, "wb");
  if (errc) {
    Warning("Unable to open PFM '%s' for writing: %s.\n", file_path,
            errc.message().c_str());
    return false;
  }

  std::tie(std::ignore, errc) =
      f.print("PF\n%d %d\n-1.000000\n", width, height);
  if (errc) {
    Warning("Unable to write PFM header to '%s': %s.\n", file_path,
            errc.message().c_str());
    return false;
  }

  for (int i = height - 1; i >= 0; i--) {
    const float *row = &img[3 * width * i];

    std::tie(std::ignore, errc) = f.write(row, width * sizeof(float) * 3, 1);
    if (errc) {
      Warning("Unable to write PFM row #%d to '%s': %s.\n", height - i,
			  file_path, errc.message().c_str());
      return false;
    }
  }

  return true;
}

FloatBitMap_t::FloatBitMap_t(int width, int height) : FloatBitMap_t{}
{
	RGBAData = AllocateRGB(width,height);
}

FloatBitMap_t::FloatBitMap_t(FloatBitMap_t const *orig) : FloatBitMap_t{}
{
	RGBAData = AllocateRGB(orig->Width,orig->Height);
	if (RGBAData) memcpy(RGBAData, orig->RGBAData, orig->Width * orig->Height * sizeof(float) * 4);
}

static char GetChar(IFileSystem *file_system, FileHandle_t &f)
{
	char a;
	file_system->Read(&a,1,f);
	return a;
}

static int GetInt(IFileSystem *file_system, FileHandle_t &f)
{
	char buf[16];
	intp i = ssize(buf) - 1;
	char *bout=buf;
	while(i--)
	{
		char c=GetChar(file_system, f);
		if ((c<'0') || (c>'9'))
			break;
		*(bout++)=c;
	}
	*(bout++)='\0';
	return atoi(buf);

}

bool FloatBitMap_t::LoadFromPFM(char const *fname)
{
	FileHandle_t f = g_pFullFileSystem->Open(fname, "rb");
	if (!f) return false;

	RunCodeAtScopeExit(g_pFullFileSystem->Close( f ));

	if( GetChar(g_pFullFileSystem, f) == 'P' &&
		GetChar(g_pFullFileSystem, f) == 'F' &&
		GetChar(g_pFullFileSystem, f) == '\n' )
	{
		Width = GetInt(g_pFullFileSystem, f);
		Height = GetInt(g_pFullFileSystem, f);

		// dimhotepus: Protect from overflow as input is untrusted.
		constexpr int kMaxDimension = std::numeric_limits<int>::max() /
			( static_cast<int>( sizeof(float) ) * 3 );
		if ( Width < 0 || Width > kMaxDimension ||
				Height < 0 || Height > kMaxDimension )
		{
			fprintf(stderr, "'%s': width (%d) and height (%d) should be in range [0...%d].\n",
				fname, Width, Height, kMaxDimension );
			return false;
		}

		// eat crap until the next newline
		while( GetChar(g_pFullFileSystem, f) != '\n')
		{
		}
			
		// dimhotepus: Handle OOM.
		std::unique_ptr<float[]> linebuffer = std::make_unique<float[]>( Width * 3 );
		if ( !linebuffer )
		{
			fprintf(stderr, "'%s': unable to allocate %zu bytes line buffer.\n",
				fname, sizeof(float) * Width * 3 );
			return false;
		}

		// dimhotepus: Handle OOM.
		if ( !AllocateRGB(Width, Height) )
		{
			fprintf(stderr, "'%s': unable to allocate %zu bytes result buffer.\n",
				fname, sizeof(float) * Width * Height * 4 );
			return false;
		}

		for( int y = Height-1; y >= 0; y-- )
		{
			g_pFullFileSystem->Read(linebuffer.get(), 3*Width*sizeof(float), f);
			for (int x=0;x<Width;x++)
			{
				for (int c=0;c<3;c++)
				{
					Pixel(x,y,c) = linebuffer[x*3+c];
				}
			}
		}

		// dimhotepus: Only loaded PFMs should be accepted.
		return true;
	}

	return false;
}

bool FloatBitMap_t::WritePFM(char const *fname) const
{
	// dimhotepus: Handle OOM.
	std::unique_ptr<float[]> data =
		std::make_unique<float[]>( static_cast<size_t>(4) * Width * Height );
	if ( !data )
	{
		fprintf(stderr, "'%s': unable to allocate %zu bytes line buffer.\n",
			fname, static_cast<size_t>(4) * Width * Height );
		return false;
	}

	for( int y = 0; y < Height; y++ )
	{
		const intp base = static_cast<intp>(y) * Width * 3;
		for ( int x = 0; x < Width; x++ )
		{
			for ( int c = 0; c < 3; c++ )
			{
				data[base + x * 3 + c] = Pixel(x, y, c);
			}
		}
	}

	return PFMWrite( g_pFullFileSystem, data.get(), fname, Width, Height );
}


float FloatBitMap_t::InterpolatedPixel(float x, float y, int comp) const
{
	int Top= static_cast<int>(floorf(y));
	float Yfrac= y - Top;
	int Bot= min(Height-1,Top+1);
	int Left= static_cast<int>(floorf(x));
	float Xfrac= x - Left;
	int Right= min(Width-1,Left+1);
	return
		BiLinInterp(Xfrac, Yfrac, 
		Pixel(Left, Top, comp),
		Pixel(Right, Top, comp),
		Pixel(Left, Bot, comp),
		Pixel(Right, Bot, comp));

}

//-----------------------------------------------------------------
// resize (with bilinear filter) truecolor bitmap in place

void FloatBitMap_t::ReSize(int NewWidth, int NewHeight)
{
	float XRatio= (float)Width / (float)NewWidth;
	float YRatio= (float)Height / (float)NewHeight;
	float SourceX, SourceY, Xfrac, Yfrac;
	int Top, Bot, Left, Right;

	auto * RESTRICT newrgba=new float[NewWidth * NewHeight * 4];
	if (!newrgba)
		Error( "Unable to allocate new bitmap %dx%d for resize from %dx%d.\n",
			NewWidth, NewHeight, Width, Height );

	SourceY= 0;
	for(int y=0;y<NewHeight;y++)
	{
		Yfrac= SourceY - floorf(SourceY);
		Top= static_cast<int>(SourceY);
		Bot= static_cast<int>(SourceY+1);
		if (Bot>=Height) Bot= Height-1;
		SourceX= 0;
		for(int x=0;x<NewWidth;x++)
		{
			Xfrac= SourceX - floorf(SourceX);
			Left= static_cast<int>(SourceX);
			Right= static_cast<int>(SourceX+1);
			if (Right>=Width) Right= Width-1;
			for(int c=0;c<4;c++)
			{
				newrgba[4*(y*NewWidth+x)+c] = BiLinInterp(Xfrac, Yfrac, 
					Pixel(Left, Top, c),
					Pixel(Right, Top, c),
					Pixel(Left, Bot, c),
					Pixel(Right, Bot, c));
			}
			SourceX+= XRatio;
		}
		SourceY+= YRatio;
	}

	delete[] RGBAData;
	RGBAData=newrgba;

	Width=NewWidth; 
	Height=NewHeight;
}

struct TGAHeader_t
{
	unsigned char 	id_length, colormap_type, image_type;
	unsigned char	colormap_index0,colormap_index1, colormap_length0,colormap_length1;
	unsigned char	colormap_size;
	unsigned char	x_origin0,x_origin1, y_origin0,y_origin1, width0, width1,height0,height1;
	unsigned char	pixel_size, attributes;
};

bool FloatBitMap_t::WriteTGAFile(char const *filename) const
{
	FileHandle_t f = g_pFullFileSystem->Open(filename, "wb");
	if (!f) return false;

	RunCodeAtScopeExit(g_pFullFileSystem->Close(f));

	TGAHeader_t myheader = {};
	myheader.image_type=2;
	myheader.pixel_size=32;
	myheader.width0= Width & 0xff;
	myheader.width1= static_cast<unsigned char>(Width>>8);
	myheader.height0= Height & 0xff;
	myheader.height1= static_cast<unsigned char>(Height>>8);
	myheader.attributes=0x20;
	g_pFullFileSystem->Write(&myheader,sizeof(myheader),f);
	// now, write the pixels
	for(int y=0;y<Height;y++)
	{
		for(int x=0;x<Width;x++)
		{
			PixRGBAF fpix = PixelRGBAF( x, y );
			PixRGBA8 pix8 = PixRGBAF_to_8( fpix );

			// dimhotepus: 4x speedup - write 4 colors at once.
			const unsigned char pixels[]{pix8.Blue, pix8.Green, pix8.Red, pix8.Alpha};

			g_pFullFileSystem->Write(pixels, sizeof(pixels), f);
		}
	}
		
	return true;
}


FloatBitMap_t::FloatBitMap_t(char const *tgafilename) : FloatBitMap_t{}
{
	// load from a tga or pfm
	if (Q_stristr(tgafilename, ".pfm"))
	{
		if (!LoadFromPFM(tgafilename))
		{
			fprintf(stderr, "Unable to load '%s'. Expected PFM.\n", tgafilename);
		}
		return;
	}

	int width1, height1;
	ImageFormat imageFormat1;
	float gamma1;

	if( !TGALoader::GetInfo( tgafilename, &width1, &height1, &imageFormat1, &gamma1 ) )
	{
		fprintf(stderr, "Error loading '%s'. Expected TGA.\n", tgafilename);
		return;
	}

	if ( !AllocateRGB(width1,height1))
	{
		fprintf(stderr, "Error allocating memory bytes for '%s'.\n", tgafilename);
		return;
	}

	std::unique_ptr<uint8[]> pImage1Tmp = 
		std::make_unique<uint8[]>(ImageLoader::GetMemRequired( width1, height1, 1, imageFormat1, false ));

	if( !TGALoader::Load( pImage1Tmp.get(), tgafilename, width1, height1, imageFormat1, 2.2f, false ) )
	{
		fprintf(stderr, "Error loading '%s'. Expected TGA.\n", tgafilename);
		return;
	}

	std::unique_ptr<uint8[]> pImage1 = 
		std::make_unique<uint8[]>(ImageLoader::GetMemRequired( width1, height1, 1, IMAGE_FORMAT_ABGR8888, false ));

	ImageLoader::ConvertImageFormat( pImage1Tmp.get(), imageFormat1, pImage1.get(), IMAGE_FORMAT_ABGR8888, width1, height1, 0, 0 );

	for(int y=0;y<height1;y++)
	{
		for(int x=0;x<width1;x++)
		{
			for(int c=0;c<4;c++)
			{
				Pixel(x,y,3-c)=pImage1[c+4*(x+(y*width1))]/255.0f;
			}
		}
	}
}

FloatBitMap_t::~FloatBitMap_t()
{
	delete[] RGBAData;
}


ALLOC_CALL FloatBitMap_t *FloatBitMap_t::QuarterSize() const
{
	// generate a new bitmap half on each axis
	auto * RESTRICT newbm=new FloatBitMap_t(Width/2,Height/2);
	if (!newbm)
		Error( "Unable to allocate new bitmap %dx%d.\n", Width/2, Height/2 );

	for(int y=0;y<Height/2;y++)
		for(int x=0;x<Width/2;x++)
		{
			for(int c=0;c<4;c++)
				newbm->Pixel(x,y,c)=((Pixel(x*2,y*2,c)+Pixel(x*2+1,y*2,c)+
				Pixel(x*2,y*2+1,c)+Pixel(x*2+1,y*2+1,c))/4);
		}
	return newbm;
}

ALLOC_CALL FloatBitMap_t *FloatBitMap_t::QuarterSizeBlocky() const
{
	// generate a new bitmap half on each axis
	auto * RESTRICT newbm=new FloatBitMap_t(Width/2,Height/2);
	if (!newbm)
		Error( "Unable to allocate new bitmap %dx%d.\n", Width/2, Height/2 );

	for(int y=0;y<Height/2;y++)
		for(int x=0;x<Width/2;x++)
		{
			for(int c=0;c<4;c++)
				newbm->Pixel(x,y,c)=Pixel(x*2,y*2,c);
		}
	return newbm;
}

Vector FloatBitMap_t::AverageColor() const
{
	Vector ret(0,0,0);
	for(int y=0;y<Height;y++)
		for(int x=0;x<Width;x++)
			for(int c=0;c<3;c++)
				ret[c]+=Pixel(x,y,c);
	ret*=1.0f/(Width*Height);
	return ret;
}

float FloatBitMap_t::BrightestColor() const
{
	float ret=0.0;
	for(int y=0;y<Height;y++)
		for(int x=0;x<Width;x++)
		{
			Vector v(Pixel(x,y,0),Pixel(x,y,1),Pixel(x,y,2));
			ret=max(ret,v.Length());
		}
	return ret;
}

void FloatBitMap_t::RaiseToPower(float power) const
{
	for(int y=0;y<Height;y++)
		for(int x=0;x<Width;x++)
			for(int c=0;c<3;c++)
				Pixel(x,y,c)=powf(max(0.0f,Pixel(x,y,c)),power);

}

void FloatBitMap_t::Logize() const
{
	for(int y=0;y<Height;y++)
		for(int x=0;x<Width;x++)
			for(int c=0;c<3;c++)
				Pixel(x,y,c)=logf(1.0f+Pixel(x,y,c));

}

void FloatBitMap_t::UnLogize() const
{
	for(int y=0;y<Height;y++)
		for(int x=0;x<Width;x++)
			for(int c=0;c<3;c++)
				Pixel(x,y,c)=expf(Pixel(x,y,c))-1;
}


void FloatBitMap_t::Clear(float r, float g, float b, float alpha) const
{
	for(int y=0;y<Height;y++)
		for(int x=0;x<Width;x++)
		{
			Pixel(x,y,0)=r;
			Pixel(x,y,1)=g;
			Pixel(x,y,2)=b;
			Pixel(x,y,3)=alpha;
		}
}

void FloatBitMap_t::ScaleRGB(float scale_factor) const
{
	for(int y=0;y<Height;y++)
		for(int x=0;x<Width;x++)
			for(int c=0;c<3;c++)
				Pixel(x,y,c)*=scale_factor;
}

static int dx[4]={0,-1,1,0};
static int dy[4]={-1,0,0,1};

constexpr int NDELTAS{4};

void FloatBitMap_t::SmartPaste(FloatBitMap_t const &b, int xofs, int yofs, uint32 Flags)
{
	// now, need to make Difference map
	FloatBitMap_t DiffMap0(this);
	FloatBitMap_t DiffMap1(this);
	FloatBitMap_t DiffMap2(this);
	FloatBitMap_t DiffMap3(this);
	FloatBitMap_t *deltas[4]={&DiffMap0,&DiffMap1,&DiffMap2,&DiffMap3};
	for(int x=0;x<Width;x++)
		for(int y=0;y<Height;y++)
			for(int c=0;c<3;c++)
			{
				for(int i=0;i<NDELTAS;i++)
				{
					int x1=x+dx[i];
					int y1=y+dy[i];
					x1=max(0,x1);
					x1=min(Width-1,x1);
					y1=max(0,y1);
					y1=min(Height-1,y1);
					float dx1=Pixel(x,y,c)-Pixel(x1,y1,c);
					deltas[i]->Pixel(x,y,c)=dx1;
				}
			}

	for(int x=1;x<b.Width-1;x++)
		for(int y=1;y<b.Height-1;y++)
			for(int c=0;c<3;c++)
			{
				for(int i=0;i<NDELTAS;i++)
				{
					float diff=b.Pixel(x,y,c)-b.Pixel(x+dx[i],y+dy[i],c);
					deltas[i]->Pixel(x+xofs,y+yofs,c)=diff;
					if (Flags & SPFLAGS_MAXGRADIENT)
					{
						float dx1=Pixel(x+xofs,y+yofs,c)-Pixel(x+dx[i]+xofs,y+dy[i]+yofs,c);
						if (fabs(dx1)>fabs(diff))
							deltas[i]->Pixel(x+xofs,y+yofs,c)=dx1;
					}
				}
			}

	// now, calculate modifiability
	for(int x=0;x<Width;x++)
		for(int y=0;y<Height;y++)
		{
			float modify=0;
			if (
				(x>xofs+1) && (x<=xofs+b.Width-2) &&
				(y>yofs+1) && (y<=yofs+b.Height-2))
				modify=1;
			Alpha(x,y)=modify;
		}

	//   // now, force a fex pixels in center to be constant
	//   int midx=xofs+b.Width/2;
	//   int midy=yofs+b.Height/2;
	//   for(x=midx-10;x<midx+10;x++)
	//     for(int y=midy-10;y<midy+10;y++)
	//     {
	//       Alpha(x,y)=0;
	//       for(int c=0;c<3;c++)
	//         Pixel(x,y,c)=b.Pixel(x-xofs,y-yofs,c);
	//     }
	Poisson(deltas,6000,Flags);
}

void FloatBitMap_t::ScaleGradients()
{
	// now, need to make Difference map
	FloatBitMap_t DiffMap0(this);
	FloatBitMap_t DiffMap1(this);
	FloatBitMap_t DiffMap2(this);
	FloatBitMap_t DiffMap3(this);
	FloatBitMap_t *deltas[4]={&DiffMap0,&DiffMap1,&DiffMap2,&DiffMap3};
	for(int x=0;x<Width;x++)
		for(int y=0;y<Height;y++)
			for(int c=0;c<3;c++)
			{
				for(int i=0;i<NDELTAS;i++)
				{
					int x1=x+dx[i];
					int y1=y+dy[i];
					x1=max(0,x1);
					x1=min(Width-1,x1);
					y1=max(0,y1);
					y1=min(Height-1,y1);
					float dx1=Pixel(x,y,c)-Pixel(x1,y1,c);
					deltas[i]->Pixel(x,y,c)=dx1;
				}
			}
	// now, reduce gradient changes
	for(int x=0;x<Width;x++)
		for(int y=0;y<Height;y++)
			for(int c=0;c<3;c++)
			{
				for(auto &delta : deltas)
				{
					float norml=1.1f*delta->Pixel(x,y,c);
					delta->Pixel(x,y,c)=norml;
				}
			}

	// now, calculate modifiability
	for(int x=0;x<Width;x++)
		for(int y=0;y<Height;y++)
		{
			float modify=0;
			if (
				(x>0) && (x<Width-1) &&
				(y) && (y<Height-1))
			{
				modify=1;
				Alpha(x,y)=modify;
			}
		}

	Poisson(deltas,2200,0);
}



void FloatBitMap_t::MakeTileable() const
{
	FloatBitMap_t rslta(this);
	// now, need to make Difference map
	FloatBitMap_t DiffMapX(this);
	FloatBitMap_t DiffMapY(this);
	// set each pixel=avg-pixel
	FloatBitMap_t *cursrc=&rslta;
	for(int x=1;x<Width-1;x++)
		for(int y=1;y<Height-1;y++)
			for(int c=0;c<3;c++)
			{
				DiffMapX.Pixel(x,y,c)=Pixel(x,y,c)-Pixel(x+1,y,c);
				DiffMapY.Pixel(x,y,c)=Pixel(x,y,c)-Pixel(x,y+1,c);
			}
	// initialize edge conditions
	for(int x=0;x<Width;x++)
	{
		for(int c=0;c<3;c++)
		{
			float a=0.5f*(Pixel(x,Height-1,c)+=Pixel(x,0,c));
			rslta.Pixel(x,Height-1,c)=a;
			rslta.Pixel(x,0,c)=a;
		}
	}
	for(int y=0;y<Height;y++)
	{
		for(int c=0;c<3;c++)
		{
			float a=0.5f*(Pixel(Width-1,y,c)+Pixel(0,y,c));
			rslta.Pixel(Width-1,y,c)=a;
			rslta.Pixel(0,y,c)=a;
		}
	}
	FloatBitMap_t rsltb(&rslta);
	FloatBitMap_t *curdst=&rsltb;

	// now, ready to iterate
	for(int pass=0;pass<10;pass++)
	{
		[[maybe_unused]] float error=0.0;
		for(int x=1;x<Width-1;x++)
			for(int y=1;y<Height-1;y++)
				for(int c=0;c<3;c++)
				{
					float desiredx=DiffMapX.Pixel(x,y,c)+cursrc->Pixel(x+1,y,c);
					float desiredy=DiffMapY.Pixel(x,y,c)+cursrc->Pixel(x,y+1,c);
					float desired=0.5f*(desiredy+desiredx);
					curdst->Pixel(x,y,c)=Lerp(0.5f,cursrc->Pixel(x,y,c),desired);
					error+=Square(desired-cursrc->Pixel(x,y,c));
				}
		std::swap(cursrc,curdst);
	}
	// paste result
	for(int x=0;x<Width;x++)
		for(int y=0;y<Height;y++)
			for(int c=0;c<3;c++)
				Pixel(x,y,c)=curdst->Pixel(x,y,c);
}


void FloatBitMap_t::GetAlphaBounds(int &minx, int &miny, int &maxx,int &maxy) const
{
	for(minx=0;minx<Width;minx++)
	{
		int y;
		for(y=0;y<Height;y++)
			if (Alpha(minx,y))
				break;
		if (y!=Height)
			break;
	}
	for(maxx=Width-1;maxx>=0;maxx--)
	{
		int y;
		for(y=0;y<Height;y++)
			if (Alpha(maxx,y))
				break;
		if (y!=Height)
			break;
	}
	for(miny=0;minx<Height;miny++)
	{
		int x;
		for(x=minx;x<=maxx;x++)
			if (Alpha(x,miny))
				break;
		if (x<maxx)
			break;
	}
	for(maxy=Height-1;maxy>=0;maxy--)
	{
		int x;
		for(x=minx;x<=maxx;x++)
			if (Alpha(x,maxy))
				break;
		if (x<maxx)
			break;
	}
}

void FloatBitMap_t::Poisson(FloatBitMap_t *deltas[4],
							int n_iters,
							uint32 flags                                  // SPF_xxx
							)
{
	int minx,miny,maxx,maxy;
	GetAlphaBounds(minx,miny,maxx,maxy);
	minx=max(1,minx);
	miny=max(1,miny);
	maxx=min(Width-2,maxx);
	maxy=min(Height-2,maxy);
	if (((maxx-minx)>25) && (maxy-miny)>25)
	{
		// perform at low resolution
		FloatBitMap_t *lowdeltas[NDELTAS];
		for(int i=0;i<NDELTAS;i++)
			lowdeltas[i]=deltas[i]->QuarterSize();
		FloatBitMap_t *tmp=QuarterSize();
		tmp->Poisson(lowdeltas,n_iters*4,flags);
		// now, propagate results from tmp to us
		for(int x=0;x<tmp->Width;x++)
			for(int y=0;y<tmp->Height;y++)
				for(int xi=0;xi<2;xi++)
					for(int yi=0;yi<2;yi++)
						if (Alpha(x*2+xi,y*2+yi))
						{
							for(int c=0;c<3;c++)
								Pixel(x*2+xi,y*2+yi,c)=
								Lerp(Alpha(x*2+xi,y*2+yi),Pixel(x*2+xi,y*2+yi,c),tmp->Pixel(x,y,c));
						}

		char fname[80];
		V_sprintf_safe(fname,"sub%dx%d.tga",tmp->Width,tmp->Height);
		if ( !tmp->WriteTGAFile(fname) )
			Warning("Unable to write sub TGA '%s'.\n", fname);

		V_sprintf_safe(fname,"submrg%dx%d.tga",tmp->Width,tmp->Height);
		if ( !WriteTGAFile(fname) )
			Warning("Unable to write submerged TGA '%s'.\n", fname);

		delete tmp;
		for(auto &lowdelta : lowdeltas)
			delete lowdelta;
	}
	FloatBitMap_t work1(this);
	FloatBitMap_t work2(this);
	FloatBitMap_t *curdst=&work1;
	FloatBitMap_t *cursrc=&work2;
	// now, ready to iterate
	while(n_iters--)
	{
		[[maybe_unused]] float error=0.0;
		for(int x=minx;x<=maxx;x++)
		{
			for(int y=miny;y<=maxy;y++)
			{
				if (Alpha(x,y))
				{
					for(int c=0;c<3;c++)
					{
						float desired=0.0;
						for(int i=0;i<NDELTAS;i++)
							desired+=deltas[i]->Pixel(x,y,c)+cursrc->Pixel(x+dx[i],y+dy[i],c);
						desired*=(1.0f/NDELTAS);
						//            desired=Lerp(Alpha(x,y),Pixel(x,y,c),desired);
						curdst->Pixel(x,y,c)=Lerp(0.5f,cursrc->Pixel(x,y,c),desired);
						error+=Square(desired-cursrc->Pixel(x,y,c));
					}
				}
				std::swap(cursrc,curdst);
			}
		}
	}
	// paste result
	for(int x=0;x<Width;x++)
	{
		for(int y=0;y<Height;y++)
		{
			for(int c=0;c<3;c++)
			{
				Pixel(x,y,c)=curdst->Pixel(x,y,c);
			}
		}
	}
}


