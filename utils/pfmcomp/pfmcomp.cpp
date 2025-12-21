//========= Copyright Valve Corporation, All rights reserved. ============//

#include "tier0/platform.h"
#include "tier2/tier2.h"
#include "bitmap/float_bm.h"
#include "mathlib/mathlib.h"





int main(int argc,char **argv)
{
	InitCommandLineProgram( argc, argv );
	if ((argc<2) || (argc>3))
	{
		fprintf(stderr, "format is %s imagefile.pfm [scalefactor]\n",argv[0]);
		return EINVAL;
	}

		FloatBitMap_t cmap;
		if (!cmap.LoadFromPFM(argv[1]))
		{
			fprintf(stderr, "Unable to load %s\n",argv[1]);
		}

		if (argc==3)										// scale factor specified
		{
			float scale_factor=atof(argv[2]);
			for(int y=0;y<cmap.Height;y++)
				for(int x=0;x<cmap.Width;x++)
					for(int c=0;c<3;c++)
						cmap.Pixel(x,y,c) *= scale_factor;
		}
		FloatBitMap_t save_orig(&cmap);
		cmap.CompressTo8Bits(8.0);
		char fname[1024];
		strcpy(fname,argv[1]);
		char *dot=strstr(fname,".pfm");
		if (! dot)
			dot=fname+strlen(fname);
		strcpy(dot,"_compressed.tga");
		cmap.WriteTGAFile(fname);

#ifdef WRITE_OUT_COMPARISON_IMAGES
		// now, write out comparison images
		cmap.Uncompress(8.0);
		cmap.RaiseToPower(1.0/2.2);
		strcpy(dot,"_ref_comp.tga");
		cmap.WriteTGAFile(fname);
		save_orig.RaiseToPower(1.0/2.2);
		strcpy(dot,"_ref_orig.tga");
		save_orig.WriteTGAFile(fname);
#endif
	return 0;
}

