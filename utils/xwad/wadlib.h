// Copyright Valve Corporation, All rights reserved.
//
// wad reading

#ifndef SE_UTILS_XWAD_WADLIB_H_
#define SE_UTILS_XWAD_WADLIB_H_

#define CMP_NONE 0
#define CMP_LZSS 1

#define TYP_NONE 0
#define TYP_LABEL 1
#define TYP_LUMPY 64  // 64 + grab command number

struct wadinfo_t {
  char identification[4];  // should be WAD2 or 2DAW
  int numlumps;
  int infotableofs;
};

struct lumpinfo_t {
  int filepos;
  int disksize;
  int size;  // uncompressed
  char type;
  char compression;
  char pad1, pad2;
  char name[16];  // must be null terminated
};

extern lumpinfo_t *lumpinfo;  // location of each lump on disk
extern int numlumps;
extern wadinfo_t header;

void W_OpenWad(const char *filename);
int W_CheckNumForName(char *name);
int W_GetNumForName(char *name);
int W_LumpLength(int lump);
void W_ReadLumpNum(int lump, void *dest);
void *W_LoadLumpNum(int lump);
void *W_LoadLumpName(char *name);

void CleanupName(char *in, char *out);

//
// wad creation
//
void NewWad(char *pathname, qboolean bigendien);
void AddLump(char *name, void *buffer, int length, char type, char compress);
void WriteWad(int wad3);

#endif  // !SE_UTILS_XWAD_WADLIB_H_
