// Copyright Valve Corporation, All rights reserved.
//
// This file provides some of the goldsrc functionality for xwad.

#ifndef SE_UTILS_XWAD_GOLDSRC_STANDIN_H_
#define SE_UTILS_XWAD_GOLDSRC_STANDIN_H_

#include <cstdio>

typedef float vec_t;
typedef float vec3_t[3];

typedef unsigned char byte;
typedef int qboolean;

void Msg(const char *pMsg, ...);
void Warning(const char *pMsg, ...);
[[noreturn]] void Error(const char *pMsg, ...);

long LoadFile(char *filename, void **bufferptr);
void SaveFile(char *filename, void *buffer, std::ptrdiff_t count);

short BigShort(short l);
short LittleShort(short l);
int BigLong(int l);
int LittleLong(int l);
unsigned LittleULong(unsigned l);
float BigFloat(float l);
float LittleFloat(float l);

FILE *SafeOpenWrite(char *filename);
FILE *SafeOpenRead(char *filename);
void SafeRead(FILE *f, void *buffer, std::ptrdiff_t count);
void SafeWrite(FILE *f, void *buffer, std::ptrdiff_t count);

#endif  // !SE_UTILS_XWAD_GOLDSRC_STANDIN_H_
