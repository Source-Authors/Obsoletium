// Valve Corporation, All rights reserved.
//
// Purpose: Valve font compiler.

#include <cstdlib>
#include <cstdio>

#include "tier1/utlbuffer.h"
#include "valvefont.h"

#include "scoped_app_locale.h"
#include "posix_file_stream.h"

int Usage() {
  fprintf(stderr, "Usage:   vfont inputfont.ttf [outputfont.vfont]\n");
  return EINVAL;
}

int main(int argc, char **argv) {
#ifdef PLATFORM_64BITS
  printf("Valve Software - vfont [64 bit] (" __DATE__ ")\n");
#else
  printf("Valve Software - vfont (" __DATE__ ")\n");
#endif

  // dimhotepus: Apply en_US UTF8 locale for printf/scanf.
  // 
  // Printf/sscanf functions expect en_US UTF8 localization.
  //
  // Starting in Windows 10 version 1803 (10.0.17134.0), the Universal C Runtime
  // supports using a UTF-8 code page.
  constexpr char kEnUsUtf8Locale[]{"en_US.UTF-8"};

  const se::ScopedAppLocale scoped_app_locale{kEnUsUtf8Locale};
  if (V_stricmp(se::ScopedAppLocale::GetCurrentLocale(), kEnUsUtf8Locale)) {
    fprintf(stderr, "setlocale('%s') failed, current locale is '%s'.\n",
            kEnUsUtf8Locale, se::ScopedAppLocale::GetCurrentLocale());
  }

  int iArg = 1;
  if (iArg >= argc) return Usage();

  //
  // Check if we are in decompiler mode
  //
#if VFONT_DECOMPILER
  bool bDecompiler = !stricmp(argv[iArg], "-d");
  if (bDecompiler) ++iArg;
#endif

  if (iArg >= argc) return Usage();

  //
  //	Input file
  //
  char const *szInput = argv[iArg];
  ++iArg;

  //
  //	Output file
  //
  char szOutput[512] = {0};

  if (iArg >= argc) {
    intp numBytes = V_strlen(szInput);
    V_strcpy_safe(szOutput, szInput);

    char *pDot = strrchr(szOutput, '.');
    char *pSlash1 = strrchr(szOutput, '/');
    char *pSlash2 = strrchr(szOutput, '\\');

    if (!pDot) {
      pDot = szOutput + numBytes;
    } else if (pDot < pSlash1 || pDot < pSlash2) {
      pDot = szOutput + numBytes;
    }

    sprintf(pDot, ".vfont");
  } else {
    V_strcpy_safe(szOutput, argv[iArg]);
  }

  CUtlBuffer buf;

  {
    //
    //	Read the input
    //
    auto [fin, errc] =
        se::posix::posix_file_stream_factory::open(szInput, "rb");
    if (errc) {
      fprintf(stderr, "Error: cannot open input file '%s': %s!\n", szInput,
              errc.message().c_str());
      return errc.value();
    }

    std::int64_t lSize;
    std::tie(lSize, errc) = fin.size();
    if (errc) {
      fprintf(stderr, "Error: cannot get input file '%s' size: %s!\n", szInput,
              errc.message().c_str());
      return errc.value();
    }
    if (lSize > std::numeric_limits<intp>::max()) {
      fprintf(
          stderr,
          "Error: input file '%s' size %lld is too large. Max supported size "
          "is %zd!\n",
          szInput, lSize, std::numeric_limits<intp>::max());
      return errc.value();
    }

    const intp readSize = static_cast<intp>(lSize);
    buf.EnsureCapacity(readSize);

    std::tie(std::ignore, errc) = fin.read(buf.Base(), 1, readSize, readSize);
    if (errc) {
      fprintf(stderr,
              "Error: cannot read %zd bytes from input file '%s': %s!\n",
              readSize, szInput, errc.message().c_str());
      return errc.value();
    }

    buf.SeekPut(CUtlBuffer::SEEK_HEAD, readSize);
  }

  //
  //	Compile
  //
#if VFONT_DECOMPILER
  if (bDecompiler) {
    bool bResult = ValveFont::DecodeFont(buf);
    if (!bResult) {
      fprintf(stderr, "Error: cannot decompile input file '%s'!\n", szInput);
      return 1;
    }
  } else
    ValveFont::EncodeFont(buf);
#else
  ValveFont::EncodeFont(buf);
#endif

  {
    //
    //	Write the output
    //
    auto [fout, errc] =
        se::posix::posix_file_stream_factory::open(szOutput, "wb");
    if (errc) {
      fprintf(stderr, "Error: cannot open output file '%s': %s!\n", szOutput,
              errc.message().c_str());
      return errc.value();
    }

    const intp writeSize = buf.TellPut();

    std::tie(std::ignore, errc) = fout.write(buf.Base(), 1, writeSize);
    if (errc) {
      fprintf(stderr,
              "Error: cannot write %zd bytes to output file '%s': %s!\n",
              writeSize, szOutput, errc.message().c_str());
      return errc.value();
    }
  }

  printf("vfont successfully %scompiled '%s' as '%s'.\n",
#if VFONT_DECOMPILER
         bDecompiler ? "de" : "",
#else
         "",
#endif
         szInput, szOutput);
  return 0;
}
