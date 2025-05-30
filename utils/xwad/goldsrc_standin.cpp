// Copyright Valve Corporation, All rights reserved.

#include "goldsrc_standin.h"

#include <Windows.h>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <cstdarg>

static unsigned short g_InitialColor = 0xFFFF;
static unsigned short g_LastColor = 0xFFFF;
static unsigned short g_BadColor = 0xFFFF;
static WORD g_BackgroundFlags = 0xFFFF;
static bool g_bGotInitialColors = false;

static void GetInitialColors() {
  if (g_bGotInitialColors) return;

  g_bGotInitialColors = true;

  // Get the old background attributes.
  CONSOLE_SCREEN_BUFFER_INFO oldInfo;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &oldInfo);
  g_InitialColor = g_LastColor =
      oldInfo.wAttributes & (FOREGROUND_RED | FOREGROUND_GREEN |
                             FOREGROUND_BLUE | FOREGROUND_INTENSITY);
  g_BackgroundFlags =
      oldInfo.wAttributes & (BACKGROUND_RED | BACKGROUND_GREEN |
                             BACKGROUND_BLUE | BACKGROUND_INTENSITY);

  g_BadColor = 0;
  if (g_BackgroundFlags & BACKGROUND_RED) g_BadColor |= FOREGROUND_RED;
  if (g_BackgroundFlags & BACKGROUND_GREEN) g_BadColor |= FOREGROUND_GREEN;
  if (g_BackgroundFlags & BACKGROUND_BLUE) g_BadColor |= FOREGROUND_BLUE;
  if (g_BackgroundFlags & BACKGROUND_INTENSITY)
    g_BadColor |= FOREGROUND_INTENSITY;
}

static WORD SetConsoleTextColor(int red, int green, int blue, int intensity) {
  GetInitialColors();

  WORD ret = g_LastColor;

  g_LastColor = 0;
  if (red) g_LastColor |= FOREGROUND_RED;
  if (green) g_LastColor |= FOREGROUND_GREEN;
  if (blue) g_LastColor |= FOREGROUND_BLUE;
  if (intensity) g_LastColor |= FOREGROUND_INTENSITY;

  // Just use the initial color if there's a match...
  if (g_LastColor == g_BadColor) g_LastColor = g_InitialColor;

  SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                          g_LastColor | g_BackgroundFlags);
  return ret;
}

static void RestoreConsoleTextColor(WORD color) {
  SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                          color | g_BackgroundFlags);
  g_LastColor = color;
}

void Msg(const char *pMsg, ...) {
  va_list marker;
  va_start(marker, pMsg);
  vprintf(pMsg, marker);
  va_end(marker);
}

void Warning(const char *pMsg, ...) {
  WORD old = SetConsoleTextColor(1, 1, 0, 1);

  va_list marker;
  va_start(marker, pMsg);
  vprintf(pMsg, marker);
  va_end(marker);

  RestoreConsoleTextColor(old);
}

[[noreturn]] void Error(const char *error, ...) {
  WORD old = SetConsoleTextColor(1, 0, 0, 1);

  va_list argptr;

  printf("\n************ ERROR ************\n");

  va_start(argptr, error);
  vfprintf(stderr, error, argptr);
  va_end(argptr);
  fprintf(stderr, "\n");

  RestoreConsoleTextColor(old);

  exit(1);
}

/*
================
filelength
================
*/
long filelength(FILE *f) {
  long pos = ftell(f);
  fseek(f, 0, SEEK_END);
  long end = ftell(f);
  fseek(f, pos, SEEK_SET);

  return end;
}

FILE *SafeOpenWrite(char *filename) {
  FILE *f = fopen(filename, "wb");

  if (!f) Error("Error opening %s: %s", filename, strerror(errno));

  return f;
}

FILE *SafeOpenRead(char *filename) {
  FILE *f = fopen(filename, "rb");

  if (!f) Error("Error opening %s: %s", filename, strerror(errno));

  return f;
}

void SafeRead(FILE *f, void *buffer, ptrdiff_t count) {
  if (fread(buffer, 1, count, f) != (size_t)count) Error("File read failure");
}

void SafeWrite(FILE *f, void *buffer, ptrdiff_t count) {
  if (fwrite(buffer, 1, count, f) != (size_t)count) Error("File read failure");
}

/*
==============
LoadFile
==============
*/
long LoadFile(char *filename, void **bufferptr) {
  FILE *f = SafeOpenRead(filename);
  long length = filelength(f);
  void *buffer = malloc(length + 1);
  if (!buffer) {
    fclose(f);
    *bufferptr = nullptr;
    return 0;
  }

  SafeRead(f, buffer, length);
  ((char *)buffer)[length] = '\0';

  fclose(f);

  *bufferptr = buffer;
  return length;
}

void SaveFile(char *filename, void *buffer, ptrdiff_t count) {
  FILE *f = SafeOpenWrite(filename);
  SafeWrite(f, buffer, count);
  fclose(f);
}

#ifdef __BIG_ENDIAN__

short LittleShort(short l) {
  byte b1, b2;

  b1 = l & 255;
  b2 = (l >> 8) & 255;

  return (b1 << 8) + b2;
}

short BigShort(short l) { return l; }

int LittleLong(int l) {
  byte b1, b2, b3, b4;

  b1 = l & 255;
  b2 = (l >> 8) & 255;
  b3 = (l >> 16) & 255;
  b4 = (l >> 24) & 255;

  return ((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4;
}

unsigned LittleULong(unsigned l) {
  byte b1, b2, b3, b4;

  b1 = l & 255;
  b2 = (l >> 8) & 255;
  b3 = (l >> 16) & 255;
  b4 = (l >> 24) & 255;

  return ((unsigned)b1 << 24) + ((unsigned)b2 << 16) + ((unsigned)b3 << 8) + b4;
}

int BigLong(int l) { return l; }

float LittleFloat(float l) {
  union {
    byte b[4];
    float f;
  } in, out;

  in.f = l;
  out.b[0] = in.b[3];
  out.b[1] = in.b[2];
  out.b[2] = in.b[1];
  out.b[3] = in.b[0];

  return out.f;
}

float BigFloat(float l) { return l; }

#else

short BigShort(short l) {
  byte b1, b2;

  b1 = l & 255;
  b2 = (l >> 8) & 255;

  return (b1 << 8) + b2;
}

short LittleShort(short l) { return l; }

int BigLong(int l) {
  byte b1, b2, b3, b4;

  b1 = l & 255;
  b2 = (l >> 8) & 255;
  b3 = (l >> 16) & 255;
  b4 = (l >> 24) & 255;

  return ((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4;
}

int LittleLong(int l) { return l; }

unsigned LittleULong(unsigned l) { return l; }

float BigFloat(float l) {
  union {
    byte b[4];
    float f;
  } in, out;

  in.f = l;
  out.b[0] = in.b[3];
  out.b[1] = in.b[2];
  out.b[2] = in.b[1];
  out.b[3] = in.b[0];

  return out.f;
}

float LittleFloat(float l) { return l; }

#endif
