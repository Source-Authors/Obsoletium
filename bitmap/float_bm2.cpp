//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "bitmap/float_bm.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

[[nodiscard]] static constexpr float ScaleValue(float f, float overbright)
{
	// map a value between 0..255 to the scale factor
	int ival=static_cast<int>(f);
	return ival*(overbright/255.0f);
}

[[nodiscard]] static constexpr float IScaleValue(float f, float overbright)
{
	f*=(1.0f/overbright);
	float ival=min(255.0f,ceilf(f*255.0f));
	return ival;
}

void FloatBitMap_t::Uncompress(float overbright) const
{
	for(int y=0;y<Height;y++)
		for(int x=0;x<Width;x++)
		{
			int iactual_alpha_value=static_cast<int>(255.0f*Pixel(x,y,3));
			float actual_alpha_value=iactual_alpha_value*(1.0f/255.0f);
			for(int c=0;c<3;c++)
			{
				int iactual_color_value=static_cast<int>(255.0f*Pixel(x,y,c));
				float actual_color_value=iactual_color_value*(1.0f/255.0f);
				Pixel(x,y,c)=actual_alpha_value*actual_color_value*overbright;
			}
		}
}

void FloatBitMap_t::CompressTo8Bits(float overbright) const
{
	FloatBitMap_t TmpFBM(Width,Height);
	// first, saturate to max overbright
	for(int y=0;y<Height;y++)
		for(int x=0;x<Width;x++)
			for(int c=0;c<3;c++)
				Pixel(x,y,c)=min(overbright,Pixel(x,y,c));
	// first pass - choose nominal scale values to convert to rgb,scale
	for(int y=0;y<Height;y++)
		for(int x=0;x<Width;x++)
		{
			// determine maximum component
			float maxc=max({Pixel(x,y,0),Pixel(x,y,1),Pixel(x,y,2)});
			if (maxc==0)
			{
				for(int c=0;c<4;c++)
					TmpFBM.Pixel(x,y,c)=0;
			}
			else
			{
				float desired_floatscale=maxc;
				float closest_iscale=IScaleValue(desired_floatscale, overbright);
				float scale_value_we_got=ScaleValue(closest_iscale, overbright );
				TmpFBM.Pixel(x,y,3)=closest_iscale;
				for(int c=0;c<3;c++)
					TmpFBM.Pixel(x,y,c)=Pixel(x,y,c)/scale_value_we_got;
			}
		}

	memcpy(RGBAData,TmpFBM.RGBAData,Width*Height*4*sizeof(float));
	// now, map scale to real value
	for(int y=0;y<Height;y++)
		for(int x=0;x<Width;x++)
			Pixel(x,y,3)*=(1.0f/255.0f);
}
