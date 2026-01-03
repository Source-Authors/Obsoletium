// Copyright Valve Corporation, All rights reserved.
//
// Build a sheet data file and a large image out of multiple images.
//
// See https://developer.valvesoftware.com/wiki/Animated_Particles

#include "filesystem.h"

#include "bitmap/float_bm.h"
#include "mathlib/mathlib.h"
#include "tier0/dbg.h"
#include "tier0/platform.h"
#include "tier0/progressbar.h"
#include "tier1/utlstringmap.h"
#include "tier1/strtools.h"
#include "tier1/utlmap.h"
#include "tier2/fileutils.h"
#include "tier2/tier2.h"
#include "tools_minidump.h"

#include "tier0/memdbgon.h"

namespace {

struct Sequence;

struct SequenceFrame {
  SequenceFrame()
      : m_pImage{nullptr},
        m_XCoord{-1},
        m_YCoord{-1},
        m_mapSequences(DefLessFunc(Sequence *)) {}

  FloatBitMap_t *m_pImage;
  int m_XCoord, m_YCoord;  // where it ended up packed
  CUtlMap<Sequence *, int> m_mapSequences;
};

enum class PackingMode_t {
  PCKM_INVALID = 0,
  PCKM_FLAT,   // Default mode - every frame consumes entire RGBA space
  PCKM_RGB_A,  // Some sequences consume RGB space and some Alpha space
};

CUtlStringMap<SequenceFrame *> ImageList;
PackingMode_t s_ePackingMode = PackingMode_t::PCKM_FLAT;

struct SequenceEntry {
  SequenceFrame *m_pSeqFrame[4];
  float m_fDisplayTime;
};

struct Sequence {
  enum SeqMode_t {
    SQM_RGBA = 0,  // Sequence occupies entire RGBA space
    SQM_RGB = 1,   // Sequence occupies only RGB space
    SQM_ALPHA = 2  // Sequence occupies only Alpha space
  };

  int m_nSequenceNumber;
  bool m_Clamp;  // as opposed to loop
  SeqMode_t m_eMode;
  CUtlVector<SequenceEntry> m_Frames;

  Sequence() {
    m_nSequenceNumber = -1;
    m_Clamp = true;
    m_eMode = SQM_RGBA;
  }
};

[[nodiscard]] int GetChannelIndexFromChar(char c) {
  // r->0 b->1 g->2 a->3 else -1

  constexpr char s_ChannelIDs[] = "rgba";

  char const *pChanChar = strchr(s_ChannelIDs, c);
  if (!pChanChar) {
    fprintf(stderr, " bad channel name '%c'.\n", c);
    return -1;
  }

  return size_cast<int>(pChanChar - s_ChannelIDs);
}

[[nodiscard]] FloatBitMap_t *CreateFBM(const char *fname) {
  if (strchr(fname, ',')) {
    // parse extended specifications
    CUtlVector<char *> Images;
    V_SplitString(fname, ",", Images);

    FloatBitMap_t *pBM = NULL;
    // now, process bitmaps, performing copy operations specified by {} syntax
    for (intp i = 0; i < Images.Count(); i++) {
      char fnamebuf[MAX_PATH];
      V_strcpy_safe(fnamebuf, Images[i]);

      char *pBrace = strchr(fnamebuf, '{');
      if (pBrace) {
        *pBrace = 0;  // null it
        pBrace++;     // point at control specifier

        char *pEndBrace = strchr(pBrace, '}');
        if (!pEndBrace)
          fprintf(stderr, "bad extended bitmap synax (no close brace) - %s\n",
                  Images[i]);
      }

      FloatBitMap_t NewBM(fnamebuf);
      if (!pBM) {
        // first image sets size
        pBM = new FloatBitMap_t(&NewBM);
      }

      // now, process operation specifiers of the form "{chan=chan}" or
      // "{chan=0}"
      if (pBrace) {
        if (pBrace[1] == '=') {
          int nDstChan = GetChannelIndexFromChar(pBrace[0]);
          if (nDstChan != -1) {
            if (pBrace[2] == '0') {
              // zero the channel
              for (int y = 0; y < NewBM.Height; y++)
                for (int x = 0; x < NewBM.Width; x++) {
                  pBM->Pixel(x, y, nDstChan) = 0;
                }
            } else {
              int nSrcChan = GetChannelIndexFromChar(pBrace[2]);
              if (nSrcChan != -1) {
                // perform the channel copy
                for (int y = 0; y < NewBM.Height; y++)
                  for (int x = 0; x < NewBM.Width; x++) {
                    pBM->Pixel(x, y, nDstChan) = NewBM.Pixel(x, y, nSrcChan);
                  }
              }
            }
          }
        }
      }
    }

    return pBM;
  }

  return new FloatBitMap_t(fname);
}

CUtlVector<Sequence *> Sequences;
Sequence *pCurSequence = nullptr;

int s_nWidth, s_nHeight;

void ApplyMacros(char *in_buf, intp buffer_size) {
  CUtlVector<char *> Words;
  V_SplitString(in_buf, " ", Words);

  if ((Words.Count() == 4) && (!stricmp(Words[0], "ga_frame"))) {
    // ga_frame frm1 frm2 n -> frame frm1{r=a},frm1{g=a},frm1{b=a},frm2{a=a} n
    V_snprintf(in_buf, buffer_size, "frame %s{r=0},%s{g=a},%s{b=0},%s{a=a} %s",
               Words[1], Words[1], Words[1], Words[2], Words[3]);
  }

  Words.PurgeAndDeleteElements();
}

void ReadTextControlFile(char const *fname) {
  CRequiredInputTextFile f(fname);
  char linebuffer[4096];
  int numActualLinesRead = 0;
  while (f.ReadLine(linebuffer, sizeof(linebuffer))) {
    ++numActualLinesRead;

    // kill newline
    char *pChop = strchr(linebuffer, '\n');
    if (pChop) *pChop = 0;

    char *comment = Q_strstr(linebuffer, "//");
    if (comment) *comment = 0;
    char *in_str = linebuffer;
    while ((in_str[0] == ' ') || (in_str[0] == '\t')) in_str++;

    if (in_str[0]) {
      strlwr(in_str);
      ApplyMacros(in_str, ssize(linebuffer) + linebuffer - in_str);

      CUtlVector<char *> Words;
      V_SplitString(in_str, " ", Words);

      if ((Words.Count() == 1) && (!stricmp(Words[0], "loop"))) {
        if (pCurSequence) pCurSequence->m_Clamp = false;
      } else if ((Words.Count() == 2) && (!stricmp(Words[0], "packmode"))) {
        PackingMode_t eRequestedMode = PackingMode_t::PCKM_INVALID;
        if (!stricmp(Words[1], "flat") || !stricmp(Words[1], "rgba"))
          eRequestedMode = PackingMode_t::PCKM_FLAT;
        else if (!stricmp(Words[1], "rgb+a"))
          eRequestedMode = PackingMode_t::PCKM_RGB_A;

        if (eRequestedMode == PackingMode_t::PCKM_INVALID) {
          fprintf(stderr,
                  "*** line %d: invalid packmode specified, allowed values are "
                  "'rgba' or 'rgb+a'!\n",
                  numActualLinesRead);
          exit(EINVAL);
        } else if (!Sequences.Count())
          s_ePackingMode = eRequestedMode;
        else if (s_ePackingMode != eRequestedMode) {
          // Allow special changes:
          // flat -> rgb+a
          if (s_ePackingMode == PackingMode_t::PCKM_FLAT &&
              eRequestedMode == PackingMode_t::PCKM_RGB_A)
            s_ePackingMode = eRequestedMode;
          // everything else
          else {
            fprintf(
                stderr,
                "*** line %d: incompatible packmode change when %zd sequences "
                "already defined!\n",
                numActualLinesRead, Sequences.Count());
            exit(EINVAL);
          }
        }
      } else if ((Words.Count() == 2) &&
                 StringHasPrefix(Words[0], "sequence")) {
        int seq_no = atoi(Words[1]);
        pCurSequence = new Sequence;
        pCurSequence->m_nSequenceNumber = seq_no;

        // Figure out the sequence type
        char const *szSeqType = StringAfterPrefix(Words[0], "sequence");
        if (!stricmp(szSeqType, "") || !stricmp(szSeqType, "-rgba"))
          pCurSequence->m_eMode = Sequence::SQM_RGBA;
        else if (!stricmp(szSeqType, "-rgb"))
          pCurSequence->m_eMode = Sequence::SQM_RGB;
        else if (!stricmp(szSeqType, "-a"))
          pCurSequence->m_eMode = Sequence::SQM_ALPHA;
        else {
          fprintf(stderr,
                  "*** line %d: invalid sequence type '%s', allowed "
                  "'sequence-rgba' or 'sequence-rgb' or 'sequence-a'!\n",
                  numActualLinesRead, Words[0]);
          exit(EINVAL);
        }

        // Validate sequence type
        switch (s_ePackingMode) {
          case PackingMode_t::PCKM_FLAT:
            switch (pCurSequence->m_eMode) {
              case Sequence::SQM_RGBA:
                break;
              default:
                fprintf(
                    stderr,
                    "*** line %d: invalid sequence type '%s', packing 'flat' "
                    "allows only 'sequence-rgba'!\n",
                    numActualLinesRead, Words[0]);
                exit(EINVAL);
            }
            break;
          case PackingMode_t::PCKM_RGB_A:
            switch (pCurSequence->m_eMode) {
              case Sequence::SQM_RGB:
              case Sequence::SQM_ALPHA:
                break;
              default:
                fprintf(
                    stderr,
                    "*** line %d: invalid sequence type '%s', packing 'rgb+a' "
                    "allows only 'sequence-rgb' or 'sequence-a'!\n",
                    numActualLinesRead, Words[0]);
                exit(EINVAL);
            }
            break;
        }

        Sequences.AddToTail(pCurSequence);
      } else if ((Words.Count() >= 3) && (!stricmp(Words[0], "frame"))) {
        if (pCurSequence) {
          float ftime = strtof(Words[Words.Count() - 1], nullptr);
          SequenceEntry new_entry;
          new_entry.m_fDisplayTime = ftime;
          for (int i = 0; i < Words.Count() - 2; i++) {
            SequenceFrame *pBM;
            char *fnamebuf = Words[i + 1];
            if (!(ImageList.Defined(fnamebuf))) {
              SequenceFrame *pNew_frm = new SequenceFrame;
              pNew_frm->m_pImage = CreateFBM(fnamebuf);
              pBM = pNew_frm;
              ImageList[fnamebuf] = pNew_frm;
            } else
              pBM = ImageList[fnamebuf];

            new_entry.m_pSeqFrame[i] = pBM;

            // Validate that frame packing is correct
            if (s_ePackingMode == PackingMode_t::PCKM_RGB_A) {
              for (uint16 idx = 0; idx < pBM->m_mapSequences.Count(); ++idx) {
                Sequence *pSeq = pBM->m_mapSequences.Key(idx);
                if (pSeq->m_eMode != Sequence::SQM_RGBA &&
                    pSeq->m_eMode != pCurSequence->m_eMode) {
                  fprintf(stderr,
                          "*** line %d: 'rgb+a' packing cannot pack frame '%s' "
                          "belonging to sequences %d and %d!\n",
                          numActualLinesRead, fnamebuf, pSeq->m_nSequenceNumber,
                          pCurSequence->m_nSequenceNumber);
                  exit(EINVAL);
                }
              }
            }

            pBM->m_mapSequences.Insert(pCurSequence, 1);

            if (i == 0)
              for (intp j = 1; j < ssize(new_entry.m_pSeqFrame); j++)
                new_entry.m_pSeqFrame[j] = new_entry.m_pSeqFrame[0];
          }
          pCurSequence->m_Frames.AddToTail(new_entry);
        }
      } else {
        fprintf(stderr, "*** line %d: Bad command \"%s\"!\n",
                numActualLinesRead, in_str);
        exit(EINVAL);
      }
      Words.PurgeAndDeleteElements();
    }
  }
}

[[nodiscard]] inline float UCoord(int u) {
  float uc = u + 0.5f;
  return uc / (float)s_nWidth;
}

[[nodiscard]] inline float VCoord(int v) {
  float vc = v + 0.5f;
  return vc / (float)s_nHeight;
}

[[nodiscard]] bool PackImages_Flat(char const *pFname, int nWidth) {
  // !! bug !! packing algorithm is dumb and no error checking is done!
  FloatBitMap_t output(nWidth, 2048);
  int cur_line = 0;
  int cur_column = 0;
  int next_line = 0;
  int max_column_written = 0;

  for (unsigned short i = 0; i < ImageList.GetNumStrings(); i++) {
    SequenceFrame &frm = *(ImageList[i]);
    if (cur_column + frm.m_pImage->Width > output.Width) {
      // no room!
      cur_column = 0;
      cur_line = next_line;
      next_line = cur_line;
    }
    // now, pack
    if ((cur_column + frm.m_pImage->Width > output.Width) ||
        (cur_line + frm.m_pImage->Height > output.Height)) {
      return false;  // didn't fit! doh
    }

    frm.m_XCoord = cur_column;
    frm.m_YCoord = cur_line;

    if (pFname)  // don't actually pack the pixel if we're not keeping them
    {
      for (int y = 0; y < frm.m_pImage->Height; y++)
        for (int x = 0; x < frm.m_pImage->Width; x++)
          for (int c = 0; c < 4; c++) {
            output.Pixel(x + cur_column, y + cur_line, c) =
                frm.m_pImage->Pixel(x, y, c);
          }
    }

    next_line = max(next_line, cur_line + frm.m_pImage->Height);
    cur_column += frm.m_pImage->Width;
    max_column_written = max(max_column_written, cur_column);
  }

  // now, truncate height
  int h = 1;
  for (h; h < next_line; h *= 2);
  // truncate width;
  int w = 1;
  for (1; w < max_column_written; w *= 2);

  if (pFname) {
    FloatBitMap_t cropped_output(w, h);
    for (int y = 0; y < cropped_output.Height; y++)
      for (int x = 0; x < cropped_output.Width; x++)
        for (int c = 0; c < 4; c++)
          cropped_output.Pixel(x, y, c) = output.Pixel(x, y, c);

    bool bWritten = cropped_output.WriteTGAFile(pFname);
    if (!bWritten)
      fprintf(stderr, "error: failed to save TGA \"%s\"!\n", pFname);
    else
      printf("ok: successfully saved TGA \"%s\"\n", pFname);
  }

  // Store these for UV calculation later on
  s_nHeight = h;
  s_nWidth = w;
  return true;
}

bool PackImages_Rgb_A(char const *pFname, int nWidth) {
  // !! bug !! packing algorithm is dumb and no error checking is done!
  FloatBitMap_t output(nWidth, 2048);
  int cur_line[2] = {0};
  int cur_column[2] = {0};
  int next_line[2] = {0};
  int max_column_written[2] = {0};

  bool bPackingRGBA = true;

  for (unsigned short i = 0; i < ImageList.GetNumStrings(); i++) {
    SequenceFrame &frm = *(ImageList[i]);

    int idxfrm;
    Sequence::SeqMode_t eMode = frm.m_mapSequences.Key(0)->m_eMode;
    switch (eMode) {
      case Sequence::SQM_RGB:
        idxfrm = 0;
        bPackingRGBA = false;
        break;
      case Sequence::SQM_ALPHA:
        idxfrm = 1;
        bPackingRGBA = false;
        break;
      case Sequence::SQM_RGBA:
        if (!bPackingRGBA) {
          fprintf(
              stderr,
              "*** error when packing 'rgb+a', bad sequence %d encountered for "
              "frame '%s' after all rgba frames packed!\n",
              frm.m_mapSequences.Key(0)->m_nSequenceNumber,
              ImageList.String(i));
          exit(EINVAL);
        }
        idxfrm = 0;
        break;
      default:
        fprintf(
            stderr,
            "*** error when packing 'rgb+a', bad sequence %d encountered for "
            "frame '%s'!\n",
            frm.m_mapSequences.Key(0)->m_nSequenceNumber, ImageList.String(i));
        exit(EINVAL);
    }

    if (cur_column[idxfrm] + frm.m_pImage->Width > output.Width) {
      // no room!
      cur_column[idxfrm] = 0;
      cur_line[idxfrm] = next_line[idxfrm];
      next_line[idxfrm] = cur_line[idxfrm];
    }
    // now, pack
    if ((cur_column[idxfrm] + frm.m_pImage->Width > output.Width) ||
        (cur_line[idxfrm] + frm.m_pImage->Height > output.Height)) {
      return false;  // didn't fit! doh
    }

    frm.m_XCoord = cur_column[idxfrm];
    frm.m_YCoord = cur_line[idxfrm];

    if (pFname)  // don't actually pack the pixel if we're not keeping them
    {
      for (int y = 0; y < frm.m_pImage->Height; y++)
        for (int x = 0; x < frm.m_pImage->Width; x++)
          for (int c = 0; c < 4; c++) switch (eMode) {
              case Sequence::SQM_RGB:
                if (c < 3)
                  goto setpx;
                else
                  break;
              case Sequence::SQM_ALPHA:
                if (c == 3)
                  goto setpx;
                else
                  break;
              case Sequence::SQM_RGBA:
                if (c < 4)
                  goto setpx;
                else
                  break;
              setpx:
                output.Pixel(x + cur_column[idxfrm], y + cur_line[idxfrm], c) =
                    frm.m_pImage->Pixel(x, y, c);
            }
    }

    next_line[idxfrm] =
        max(next_line[idxfrm], cur_line[idxfrm] + frm.m_pImage->Height);
    cur_column[idxfrm] += frm.m_pImage->Width;
    max_column_written[idxfrm] =
        max(max_column_written[idxfrm], cur_column[idxfrm]);

    if (bPackingRGBA) {
      cur_line[1] = cur_line[0];
      cur_column[1] = cur_column[0];
      next_line[1] = next_line[0];
      max_column_written[1] = max_column_written[0];
    }
  }

  // now, truncate height
  int h = 1;
  for (int idxfrm = 0; idxfrm < 2; ++idxfrm)
    for (h; h < next_line[idxfrm]; h *= 2) continue;
  // truncate width;
  int w = 1;
  for (int idxfrm = 0; idxfrm < 2; ++idxfrm)
    for (w; w < max_column_written[idxfrm]; w *= 2) continue;

  if (pFname) {
    FloatBitMap_t cropped_output(w, h);
    for (int y = 0; y < cropped_output.Height; y++)
      for (int x = 0; x < cropped_output.Width; x++)
        for (int c = 0; c < 4; c++)
          cropped_output.Pixel(x, y, c) = output.Pixel(x, y, c);

    bool bWritten = cropped_output.WriteTGAFile(pFname);
    if (!bWritten)
      fprintf(stderr, "error: failed to save TGA \"%s\"!\n", pFname);
    else
      printf("ok: successfully saved TGA \"%s\"\n", pFname);
  }

  // Store these for UV calculation later on
  s_nHeight = h;
  s_nWidth = w;
  return true;
}

bool PackImages(char const *pFname, int nWidth) {
  switch (s_ePackingMode) {
    case PackingMode_t::PCKM_FLAT:
      return PackImages_Flat(pFname, nWidth);
    case PackingMode_t::PCKM_RGB_A:
      return PackImages_Rgb_A(pFname, nWidth);
    case PackingMode_t::PCKM_INVALID:
    default:
      return false;
  }
}

}  // namespace

int main(int argc, char **argv) {
  // Install an exception handler.
  const se::utils::common::ScopedDefaultMinidumpHandler
      scoped_default_minidumps;

  const ScopedCommandLineProgram scoped_command_line_program(argc, argv);

#ifdef PLATFORM_64BITS
  Msg("\nValve Software - mksheet [64 bit] (%s)\n", __DATE__);
#else
  Msg("\nValve Software - mksheet (%s)\n", __DATE__);
#endif

  if (argc < 2 || argc > 4) {
    fprintf(stderr,
            "format is 'mksheet sheet.mks [output.sht] [output.tga]'\n");
    return EINVAL;
  }

  char pMksFileBuf[MAX_PATH];
  char pShtFileBuf[MAX_PATH];
  char pTgaFileBuf[MAX_PATH];

  V_strcpy_safe(pMksFileBuf, argv[1]);
  V_DefaultExtension(pMksFileBuf, ".mks");
  const char *pSourceFile = pMksFileBuf;

  const char *pTgaFile;
  if (argc < 4) {
    V_StripExtension(pSourceFile, pTgaFileBuf);
    V_SetExtension(pTgaFileBuf, ".tga");
    pTgaFile = pTgaFileBuf;
  } else {
    pTgaFile = argv[3];
  }

  const char *pShtFile;
  if (argc < 3) {
    V_StripExtension(pSourceFile, pShtFileBuf);
    V_SetExtension(pShtFileBuf, ".sht");
    pShtFile = pShtFileBuf;
  } else {
    pShtFile = argv[2];
  }

  ReportProgress("reading text file", 0, 0);
  ReadTextControlFile(pSourceFile);

  // now, determine best packing
  int nBestWidth = -1;
  int nBestSize = (1 << 30);
  int nBestSquareness = (1 << 30);  // how square the texture is

  for (int nTryWidth = 2048; nTryWidth >= 64; nTryWidth >>= 1) {
    bool bSuccess = PackImages(NULL, nTryWidth);
    if (bSuccess) {
      printf("Packing option: %dx%d (%d pixels)\n", s_nWidth, s_nHeight,
             s_nWidth * s_nHeight);

      bool bPreferThisPack = false;

      int thisSize = s_nHeight * s_nWidth;
      int thisSquareness = (s_nWidth == s_nHeight)
                               ? 1
                               : (s_nHeight / s_nWidth + s_nWidth / s_nHeight);

      if (thisSize < nBestSize)
        bPreferThisPack = true;
      else if (thisSize == nBestSize && thisSquareness < nBestSquareness)
        bPreferThisPack = true;

      if (bPreferThisPack) {
        nBestWidth = nTryWidth;
        nBestSize = thisSize;
        nBestSquareness = thisSquareness;
      }
    } else {
      break;
    }
  }

  if (nBestWidth < 0) {
    fprintf(stderr, "Packing error: failed to pack images!\n");
    exit(2);
  }

  s_nWidth = nBestWidth;
  s_nHeight = nBestSize / nBestWidth;
  printf("Best option: %dx%d (%d pixels)%s\n", s_nWidth, s_nHeight,
         s_nWidth * s_nHeight,
         (s_nWidth == s_nHeight) ? " : square texture" : "");
  PackImages(pTgaFile, nBestWidth);

  // now, write output
  ReportProgress("Writing SHT output file", 0, 0);

  COutputFile Outfile(pShtFile);
  if (Outfile.IsOk()) {
    Outfile.PutInt(1);  // version #
    Outfile.PutInt(size_cast<int>(Sequences.Count()));

    for (const auto *s : Sequences) {
      Outfile.PutInt(s->m_nSequenceNumber);
      Outfile.PutInt(s->m_Clamp);
      Outfile.PutInt(size_cast<int>(s->m_Frames.Count()));

      // write total sequence length
      float fTotal = 0.0f;
      for (const auto &f : s->m_Frames) {
        fTotal += f.m_fDisplayTime;
      }
      Outfile.PutFloat(fTotal);

      for (const auto &ff : s->m_Frames) {
        Outfile.PutFloat(ff.m_fDisplayTime);

        // intp t = 0;
        // output texture coordinates
        for (auto *sf : ff.m_pSeqFrame) {
          // xmin
          Outfile.PutFloat(UCoord(sf->m_XCoord));
          // ymin
          Outfile.PutFloat(VCoord(sf->m_YCoord));
          // xmax
          Outfile.PutFloat(UCoord(sf->m_XCoord + sf->m_pImage->Width - 1));
          // ymax
          Outfile.PutFloat(VCoord(sf->m_YCoord + sf->m_pImage->Height - 1));

          // printf("T %zd UV1:( %.2f, %.2f ) UV2:(%.2f, %.2f )\n", t,
          //        UCoord(sf->m_XCoord), VCoord(sf->m_YCoord),
          //        UCoord(sf->m_XCoord + sf->m_pImage->Width - 1),
          //        VCoord(sf->m_YCoord + sf->m_pImage->Height - 1));
          //
          // ++t;
        }
      }
    }

    printf("ok: successfully saved SHT \"%s\"\n", pShtFile);
    return 0;
  }

  fprintf(stderr, "error: failed to write SHT \"%s\"!\n", pShtFile);
  return 3;
}
