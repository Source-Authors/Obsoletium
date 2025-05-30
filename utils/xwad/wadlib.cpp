// Copyright Valve Corporation, All rights reserved.

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <cctype>
#include <cstdarg>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef NeXT
#include <libc.h>
#endif

#include "goldsrc_standin.h"
#include "wadlib.h"

/*
============================================================================

WAD READING

============================================================================
*/

lumpinfo_t *lumpinfo;  // location of each lump on disk
int numlumps;

wadinfo_t header;
FILE *wadhandle;

/*
====================
W_OpenWad
====================
*/
void W_OpenWad(const char *filename) {
  lumpinfo_t *lump_p;
  int i;
  int length;

  //
  // open the file and add to directory
  //
  wadhandle = SafeOpenRead((char *)filename);
  SafeRead(wadhandle, &header, sizeof(header));

  if (strncmp(header.identification, "WAD2", 4) &&
      strncmp(header.identification, "WAD3", 4))
    Error("Wad file %s doesn't have WAD2/WAD3 id\n", filename);

  header.numlumps = LittleLong(header.numlumps);
  header.infotableofs = LittleLong(header.infotableofs);

  numlumps = header.numlumps;

  length = numlumps * sizeof(lumpinfo_t);
  lumpinfo = (lumpinfo_t *)malloc(length);
  if (!lumpinfo) {
    Error("Unable to allocate %d bytes for Wad file %s lump info\n", length,
          filename);
  }

  lump_p = lumpinfo;

  fseek(wadhandle, header.infotableofs, SEEK_SET);
  SafeRead(wadhandle, lumpinfo, length);

  //
  // Fill in lumpinfo
  //

  for (i = 0; i < numlumps; i++, lump_p++) {
    lump_p->filepos = LittleLong(lump_p->filepos);
    lump_p->size = LittleLong(lump_p->size);
  }
}

void CleanupName(char *in, char *out) {
  int i;

  for (i = 0; i < sizeof(((lumpinfo_t *)0)->name); i++) {
    if (!in[i]) break;

    out[i] = static_cast<char>(toupper(static_cast<unsigned char>(in[i])));
  }

  for (; i < sizeof(((lumpinfo_t *)0)->name); i++) out[i] = 0;
}

/*
====================
W_CheckNumForName

Returns -1 if name not found
====================
*/
int W_CheckNumForName(char *name) {
  char cleanname[16];
  int v1, v2, v3, v4;
  int i;
  lumpinfo_t *lump_p;

  CleanupName(name, cleanname);

  // make the name into four integers for easy compares

  v1 = *(int *)cleanname;
  v2 = *(int *)&cleanname[4];
  v3 = *(int *)&cleanname[8];
  v4 = *(int *)&cleanname[12];

  // find it

  lump_p = lumpinfo;
  for (i = 0; i < numlumps; i++, lump_p++) {
    if (*(int *)lump_p->name == v1 && *(int *)&lump_p->name[4] == v2 &&
        *(int *)&lump_p->name[8] == v3 && *(int *)&lump_p->name[12] == v4)
      return i;
  }

  return -1;
}

/*
====================
W_GetNumForName

Calls W_CheckNumForName, but bombs out if not found
====================
*/
int W_GetNumForName(char *name) {
  int i = W_CheckNumForName(name);
  if (i != -1) return i;

  Error("W_GetNumForName: %s not found!", name);
}

/*
====================
W_LumpLength

Returns the buffer size needed to load the given lump
====================
*/
int W_LumpLength(int lump) {
  if (lump >= numlumps) Error("W_LumpLength: %i >= numlumps", lump);
  return lumpinfo[lump].size;
}

/*
====================
W_ReadLumpNum

Loads the lump into the given buffer, which must be >= W_LumpLength()
====================
*/
void W_ReadLumpNum(int lump, void *dest) {
  lumpinfo_t *l;

  if (lump >= numlumps) Error("W_ReadLump: %i >= numlumps", lump);
  l = lumpinfo + lump;

  fseek(wadhandle, l->filepos, SEEK_SET);
  SafeRead(wadhandle, dest, l->size);
}

/*
====================
W_LoadLumpNum
====================
*/
void *W_LoadLumpNum(int lump) {
  void *buf;

  if ((unsigned)lump >= (unsigned)numlumps)
    Error("W_CacheLumpNum: %i >= numlumps", lump);

  buf = malloc(W_LumpLength(lump));
  W_ReadLumpNum(lump, buf);

  return buf;
}

/*
====================
W_LoadLumpName
====================
*/
void *W_LoadLumpName(char *name) {
  return W_LoadLumpNum(W_GetNumForName(name));
}

/*
===============================================================================

                                                WAD CREATION

===============================================================================
*/

FILE *outwad;

lumpinfo_t outinfo[4096];
int outlumps;

short (*wadshort)(short l);
int (*wadlong)(int l);

/*
===============
NewWad
===============
*/

void NewWad(char *pathname, qboolean bigendien) {
  outwad = SafeOpenWrite(pathname);
  fseek(outwad, sizeof(wadinfo_t), SEEK_SET);
  memset(outinfo, 0, sizeof(outinfo));

  if (bigendien) {
    wadshort = BigShort;
    wadlong = BigLong;
  } else {
    wadshort = LittleShort;
    wadlong = LittleLong;
  }

  outlumps = 0;
}

/*
===============
AddLump
===============
*/

void AddLump(char *name, void *buffer, int length, char type, char compress) {
  lumpinfo_t *info = &outinfo[outlumps];
  outlumps++;

  memset(info, 0, sizeof(info));

  strcpy(info->name, name);
  strupr(info->name);

  long ofs = ftell(outwad);
  info->filepos = wadlong(ofs);
  info->size = info->disksize = wadlong(length);
  info->type = type;
  info->compression = compress;

  // FIXME: do compression

  SafeWrite(outwad, buffer, length);
}

/*
===============
WriteWad
===============
*/

void WriteWad(int wad3) {
  // write the lumpingo
  long ofs = ftell(outwad);

  SafeWrite(outwad, outinfo, outlumps * sizeof(lumpinfo_t));

  // write the hdr

  wadinfo_t hdr;
  // a program will be able to tell the ednieness of a wad by the id
  hdr.identification[0] = 'W';
  hdr.identification[1] = 'A';
  hdr.identification[2] = 'D';
  hdr.identification[3] = wad3 ? '3' : '2';

  hdr.numlumps = wadlong(outlumps);
  hdr.infotableofs = wadlong(ofs);

  fseek(outwad, 0, SEEK_SET);
  SafeWrite(outwad, &hdr, sizeof(hdr));
  fclose(outwad);
}
