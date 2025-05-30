// Copyright Valve Corporation, All rights reserved.
//
// A simple application demonstrating the HL2 demo file format (subject to
// change!!!).

#include "tier0/dbg.h"
#include "filesystem.h"
#include "FileSystem_Tools.h"
#include "cmdlib.h"
#include "tooldemofile.h"

#ifdef _WIN32
#include <sal.h>

extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(
    _In_opt_ const char *lpOutputString);
#endif

namespace {

bool uselogfile = false;
bool spewed = false;

constexpr inline int COM_COPY_CHUNK_SIZE{8192};
constexpr inline char LOGFILE_NAME[]{"log.txt"};

// Prints to stdout and to the developer console and optionally to a log file
void vprint(FILE *stream, int depth, const char *fmt, ...) {
  char string[8192];
  va_list va;
  va_start(va, fmt);
  vsprintf(string, fmt, va);
  va_end(va);

  FILE *fp = nullptr;

  if (uselogfile) {
    fp = fopen(LOGFILE_NAME, "ab");
  }

  while (depth-- > 0) {
    fprintf(stream, "  ");
    OutputDebugStringA("  ");

    if (fp) {
      fprintf(fp, "  ");
    }
  }

  fprintf(stream, "%s", string);
  OutputDebugStringA(string);

  if (fp) {
    char *p = string;
    while (*p) {
      if (*p == '\n') {
        fputc('\r', fp);
      }
      fputc(*p, fp);
      p++;
    }

    fclose(fp);
  }
}

// Warning/Msg call back through this API
SpewRetval_t SpewFunc(SpewType_t type, char const *pMsg) {
  spewed = true;

  switch (type) {
    default:
    case SPEW_MESSAGE:
    case SPEW_ASSERT:
    case SPEW_LOG:
      vprint(stdout, 0, "%s", pMsg);
      break;
    case SPEW_WARNING:
      if (verbose) vprint(stderr, 0, "%s", pMsg);
      break;
    case SPEW_ERROR:
      vprint(stderr, 0, "%s\n", pMsg);
      break;
  }

  return SPEW_CONTINUE;
}

// Shows usage information
void printusage() {
  vprint(stderr, 0,
         "usage:  demoinfo <.dem file>\n\t-v = verbose output\n\t-l = log to "
         "file log.txt\n\ne.g.:  demoinfo -v u:/hl2/hl2/foo.dem\n");

  // Exit app
  exit(1);
}

// Removes previous log file
void CheckLogFile() {
  if (uselogfile) {
    _unlink(LOGFILE_NAME);
    vprint(stdout, 0, "    Outputting to log.txt\n");
  }
}

// Prints banner
void PrintHeader() {
  vprint(stdout, 0, "Valve Software - demoinfo.exe (%s)\n", __DATE__);
  vprint(stdout, 0, "--- Demo File Info Sample ---\n");
}

// Parses all "smoothing" info from .dem file
void ParseSmoothingInfo(CToolDemoFile &demoFile,
                        CUtlVector<demosmoothing_t> &smooth) {
  democmdinfo_t info;
  int dummy;

  bool demofinished = false;
  while (!demofinished) {
    int tick = 0;
    byte cmd;

    bool swallowmessages = true;
    do {
      demoFile.ReadCmdHeader(cmd, tick);

      // COMMAND HANDLERS
      switch (cmd) {
        case dem_synctick:
          break;
        case dem_stop: {
          swallowmessages = false;
          demofinished = true;
        } break;
        case dem_consolecmd: {
          demoFile.ReadConsoleCommand();
        } break;
        case dem_datatables: {
          demoFile.ReadNetworkDataTables(NULL);
        } break;
        case dem_usercmd: {
          demoFile.ReadUserCmd(NULL, dummy);

        } break;
        default: {
          swallowmessages = false;
        } break;
      }
    } while (swallowmessages);

    if (demofinished) {
      // StopPlayback();
      return;
    }

    int curpos = demoFile.GetCurPos();

    demoFile.ReadCmdInfo(info);
    demoFile.ReadSequenceInfo(dummy, dummy);
    demoFile.ReadRawData(NULL, 0);

    demosmoothing_t smoothing_entry;

    smoothing_entry.file_offset = curpos;
    smoothing_entry.frametick = tick;
    smoothing_entry.info = info;
    smoothing_entry.samplepoint = false;
    smoothing_entry.vecmoved = info.GetViewOrigin();
    smoothing_entry.angmoved = info.GetViewAngles();
    smoothing_entry.targetpoint = false;
    smoothing_entry.vectarget = info.GetViewOrigin();

    // Add to end of list

    smooth.AddToTail(smoothing_entry);
  }
}

// Resets all smoothing data back to original values
void ClearSmoothingInfo(CSmoothingContext &smoothing) {
  for (auto &p : smoothing.smooth) {
    p.info.Reset();
    p.vecmoved = p.info.GetViewOrigin();
    p.angmoved = p.info.GetViewAngles();
    p.samplepoint = false;
    p.vectarget = p.info.GetViewOrigin();
    p.targetpoint = false;
  }
}

// Helper for copying sub-chunk of file
void COM_CopyFileChunk(FileHandle_t dst, FileHandle_t src, int nSize) {
  int copysize = nSize;
  char copybuf[COM_COPY_CHUNK_SIZE];

  while (copysize > COM_COPY_CHUNK_SIZE) {
    g_pFileSystem->Read(copybuf, COM_COPY_CHUNK_SIZE, src);
    g_pFileSystem->Write(copybuf, COM_COPY_CHUNK_SIZE, dst);
    copysize -= COM_COPY_CHUNK_SIZE;
  }

  g_pFileSystem->Read(copybuf, copysize, src);
  g_pFileSystem->Write(copybuf, copysize, dst);

  g_pFileSystem->Flush(src);
  g_pFileSystem->Flush(dst);
}

// Writes out a new .dem file based on the existing dem file with new camera
// positions saved into the dem file.  NOTE: The new file is named
// filename_smooth.dem.
void SaveSmoothingInfo(char const *filename, CSmoothingContext &smoothing) {
  // Nothing to do
  intp c = smoothing.smooth.Count();
  if (!c) return;

  IBaseFileSystem *fs = g_pFileSystem;

  FileHandle_t infile = fs->Open(filename, "rb", "GAME");
  if (infile == FILESYSTEM_INVALID_HANDLE) return;

  unsigned filesize = fs->Size(infile);

  char outfilename[512];
  Q_StripExtension(filename, outfilename, sizeof(outfilename));
  Q_strncat(outfilename, "_smooth", sizeof(outfilename), COPY_ALL_CHARACTERS);
  Q_DefaultExtension(outfilename, ".dem", sizeof(outfilename));

  FileHandle_t outfile = fs->Open(outfilename, "wb", "GAME");
  if (outfile == FILESYSTEM_INVALID_HANDLE) {
    fs->Close(infile);
    return;
  }

  // The basic algorithm is to seek to each sample and "overwrite" it during
  // copy with the new data...
  int lastwritepos = 0;
  for (intp i = 0; i < c; i++) {
    demosmoothing_t *p = &smoothing.smooth[i];

    int copyamount = p->file_offset - lastwritepos;

    COM_CopyFileChunk(outfile, infile, copyamount);

    fs->Seek(infile, p->file_offset, FILESYSTEM_SEEK_HEAD);

    // wacky hacky overwriting
    fs->Write(&p->info, sizeof(democmdinfo_t), outfile);

    lastwritepos = fs->Tell(outfile);
    fs->Seek(infile, p->file_offset + sizeof(democmdinfo_t),
             FILESYSTEM_SEEK_HEAD);
  }

  // Copy the final bit of data, if any...
  int final = filesize - lastwritepos;

  COM_CopyFileChunk(outfile, infile, final);

  fs->Close(outfile);
  fs->Close(infile);
}

// Helper for spewing verbose sample information
char const *DescribeFlags(int flags) {
  static char outbuf[256];

  outbuf[0] = '\0';

  if (flags & FDEMO_USE_ORIGIN2) {
    V_strcat_safe(outbuf, "USE_ORIGIN2, ");
  }
  if (flags & FDEMO_USE_ANGLES2) {
    V_strcat_safe(outbuf, "USE_ANGLES2, ");
  }
  if (flags & FDEMO_NOINTERP) {
    V_strcat_safe(outbuf, "NOINTERP, ");
  }

  const intp len = Q_strlen(outbuf);

  if (len > 2) {
    outbuf[len - 2] = '\0';
  } else {
    V_strcpy_safe(outbuf, "N/A");
  }

  return outbuf;
}

// Loads up all camera samples from a .dem file into the passed in
void LoadSmoothingInfo(const char *filename, CSmoothingContext &smoothing) {
  char name[MAX_OSPATH];
  V_strcpy_safe(name, filename);
  Q_DefaultExtension(name, ".dem", sizeof(name));

  CToolDemoFile demoFile;
  if (!demoFile.Open(filename, true)) {
    Warning("ERROR: couldn't open %s.\n", name);
    return;
  }

  demoheader_t *header = demoFile.ReadDemoHeader();
  if (!header) {
    demoFile.Close();
    return;
  }

  Msg("\n\n");
  Msg("--------------------------------------------------------------\n");
  Msg("demofilestamp:     '%s'\n", header->demofilestamp);
  Msg("demoprotocol:      %i\n", header->demoprotocol);
  Msg("networkprotocol:   %i\n", header->networkprotocol);
  Msg("servername:        '%s'\n", header->servername);
  Msg("clientname:        '%s'\n", header->clientname);
  Msg("mapname:           '%s'\n", header->mapname);
  Msg("gamedirectory:     '%s'\n", header->gamedirectory);
  Msg("playback_time:     %f seconds\n", header->playback_time);
  Msg("playback_ticks:    %i ticks\n", header->playback_ticks);
  Msg("playback_frames:   %i frames\n", header->playback_frames);
  Msg("signonlength:      %s\n", Q_pretifymem(header->signonlength));

  smoothing.active = true;
  V_strcpy_safe(smoothing.filename, name);

  smoothing.smooth.RemoveAll();

  ClearSmoothingInfo(smoothing);

  ParseSmoothingInfo(demoFile, smoothing.smooth);

  Msg("--------------------------------------------------------------\n");
  Msg("smoothing data:    %zi samples\n", smoothing.smooth.Count());

  if (verbose) {
    intp i = 0;
    for (auto &sample : smoothing.smooth) {
      Msg("Sample %zi:\n", i);
      Msg("  file pos:         %i\n", sample.file_offset);
      Msg("  tick:             %i\n", sample.frametick);
      Msg("  flags:	          %s\n", DescribeFlags(sample.info.flags));

      Msg("  Original Data:\n");
      Msg("  origin:           %.4f %.4f %.4f\n", sample.info.viewOrigin.x,
          sample.info.viewOrigin.y, sample.info.viewOrigin.z);
      Msg("  viewangles:       %.4f %.4f %.4f\n", sample.info.viewAngles.x,
          sample.info.viewAngles.y, sample.info.viewAngles.z);
      Msg("  localviewangles:  %.4f %.4f %.4f\n", sample.info.localViewAngles.x,
          sample.info.localViewAngles.y, sample.info.localViewAngles.z);

      Msg("  Resampled Data:\n");
      Msg("  origin:           %.4f %.4f %.4f\n", sample.info.viewOrigin2.x,
          sample.info.viewOrigin2.y, sample.info.viewOrigin2.z);
      Msg("  viewangles:       %.4f %.4f %.4f\n", sample.info.viewAngles2.x,
          sample.info.viewAngles2.y, sample.info.viewAngles2.z);
      Msg("  localviewangles:  %.4f %.4f %.4f\n",
          sample.info.localViewAngles2.x, sample.info.localViewAngles2.y,
          sample.info.localViewAngles2.z);

      Msg("\n");

      ++i;
    }
  }

  demoFile.Close();
}

}  // namespace

int main(int argc, char *argv[]) {
  SpewOutputFunc(SpewFunc);
  SpewActivate("demoinfo", 2);

  int i = 1;
  for (i; i < argc; i++) {
    if (argv[i][0] == '-') {
      switch (argv[i][1]) {
        case 'l':
          uselogfile = true;
          break;
        case 'v':
          verbose = true;
          break;
        case 'g':
          ++i;
          break;
        default:
          printusage();
          break;
      }
    }
  }

  if (argc < 2 || (i != argc)) {
    PrintHeader();
    printusage();
  }

  CheckLogFile();

  PrintHeader();

  vprint(stdout, 0, "    Info for %s..\n", argv[i - 1]);

  char workingdir[256];
  workingdir[0] = 0;

  if (!Q_getwd(workingdir)) {
    Warning("Unable to get current directory.\n");
    return 1;
  }

  if (!FileSystem_Init(NULL, 0, FS_INIT_FULL)) return 1;

  // Add this so relative filenames work.
  g_pFullFileSystem->AddSearchPath(workingdir, "game", PATH_ADD_TO_HEAD);

  // Load the demo
  CSmoothingContext context;

  LoadSmoothingInfo(argv[i - 1], context);

  // Note to tool makers:
  // Do your work here!!!
  // Performsmoothing( context );

  // Save out updated .dem file
  // UNCOMMENT THIS TO ENABLE OUTPUTTING NEW .DEM FILES!!!
  // SaveSmoothingInfo( argv[ i - 1 ], context );

  FileSystem_Term();

  return 0;
}
