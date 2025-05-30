// Copyright Valve Corporation, All rights reserved.

#ifndef SE_UTILS_XWAD_LBMLIB_H_
#define SE_UTILS_XWAD_LBMLIB_H_

typedef unsigned char byte;
typedef unsigned char UBYTE;

#ifndef _WINDOWS_
typedef unsigned short WORD;
#endif

typedef unsigned short UWORD;
typedef long LONG;

enum mask_t { ms_none, ms_mask, ms_transcolor, ms_lasso };
enum compress_t { cm_none, cm_rle1 };

struct bmhd_t {
  UWORD w, h;
  WORD x, y;
  UBYTE nPlanes;
  UBYTE masking;
  UBYTE compression;
  UBYTE pad1;
  UWORD transparentColor;
  UBYTE xAspect, yAspect;
  WORD pageWidth, pageHeight;
};

extern bmhd_t bmhd;  // will be in native byte order

void LoadLBM(char *filename, byte **picture, byte **palette);
int LoadBMP(const char *szFile, byte **ppbBits, byte **ppbPalette);
void WriteLBMfile(char *filename, byte *data, int width, int height,
                  byte *palette);
int WriteBMPfile(char *szFile, byte *pbBits, int width, int height,
                 byte *pbPalette);

#endif  // !SE_UTILS_XWAD_LBMLIB_H_
