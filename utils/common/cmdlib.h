// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SRC_UTILS_COMMON_CMDLIB_H_
#define SRC_UTILS_COMMON_CMDLIB_H_

#include "tier0/basetypes.h"

// This can go away when everything is in bin.
#if defined(CMDLIB_NODBGLIB)
void Error(PRINTF_FORMAT_STRING char const *pMsg, ...);
#else
#include "tier0/dbg.h"
#endif

#include "filesystem.h"
#include "filesystem_tools.h"
#include "tier1/utlstring.h"

// Tools should use this as the read path ID.  It'll look into the paths
// specified by gameinfo.txt
constexpr inline char TOOLS_READ_PATH_ID[]{"GAME"};

// Tools should use this to fprintf data to files.
void CmdLib_FPrintf(FileHandle_t hFile,
                    PRINTF_FORMAT_STRING const char *pFormat, ...);
char *CmdLib_FGets(char *pOut, int outSize, FileHandle_t hFile);

// This can be set so Msg() sends output to hook functions (like the VMPI MySQL
// database), but doesn't actually printf the output.
extern bool g_bSuppressPrintfOutput;

extern IBaseFileSystem *g_pFileSystem;

// These call right into the functions in filesystem_tools.h
void CmdLib_InitFileSystem(const char *pFilename, int maxMemoryUsage = 0);
void CmdLib_TermFileSystem();  // GracefulExit calls this.

CreateInterfaceFn CmdLib_GetFileSystemFactory();

// the dec offsetof macro doesnt work very well...
#define myoffsetof(type, identifier) offsetof(type, identifier)

// set these before calling CheckParm
extern int myargc;
extern char **myargv;

int Q_filelength(FileHandle_t f);
time_t FileTime(char *path);

void Q_mkdir(char *path);

char *ExpandArg(char *path);   // expand relative to CWD
char *ExpandPath(char *path);  // expand relative to gamedir

char *ExpandPathAndArchive(char *path);

// Fills in pOut with "X hours, Y minutes, Z seconds". Leaves out hours or
// minutes if they're zero.
void GetHourMinuteSecondsString(int nInputSeconds, char *pOut, int outLen);

int CheckParm(char *check);

FileHandle_t SafeOpenWrite(const char *filename);
FileHandle_t SafeOpenRead(const char *filename);
void SafeRead(FileHandle_t f, void *buffer, int count);
void SafeWrite(FileHandle_t f, void *buffer, int count);

int LoadFile(const char *filename, void **bufferptr);
void SaveFile(const char *filename, void *buffer, int count);
qboolean FileExists(const char *filename);

int ParseNum(char *str);

// Do a printf in the specified color.
#define CP_ERROR stderr, 1, 0, 0, 1  // default colors..
#define CP_WARNING stderr, 1, 1, 0, 1
#define CP_STARTUP stdout, 0, 1, 1, 1
#define CP_NOTIFY stdout, 1, 1, 1, 1

void ColorPrintf(FILE *pFile, bool red, bool green, bool blue, bool intensity,
                 PRINTF_FORMAT_STRING char const *pFormat, ...);

// Initialize spew output.
void InstallSpewFunction();

// This registers an extra callback for spew output.
typedef void (*SpewHookFn)(const char *);
void InstallExtraSpewHook(SpewHookFn pFn);

// Install allocation hooks so we error out if an allocation can't happen.
void InstallAllocationFunctions();

// This shuts down mgrs that use threads gracefully.  If you just call exit(),
// the threads can get in a state where you can't tell if they are shutdown or
// not, and it can stall forever.
using CleanupFn = void (*)();
// register a callback when Cleanup() is called.
void CmdLib_AtCleanup(CleanupFn pFn);
void CmdLib_Cleanup();
// Use this to cleanup and call exit().
void CmdLib_Exit(int exitCode);

// entrypoint if chaining spew functions
SpewRetval_t CmdLib_SpewOutputFunc(SpewType_t type, char const *pMsg);
unsigned short SetConsoleTextColor(int red, int green, int blue, int intensity);
void RestoreConsoleTextColor(unsigned short color);

// Append all spew output to the specified file.
void SetSpewFunctionLogFile(char const *pFilename);

char *COM_Parse(char *data);

extern char com_token[1024];

char *copystring(const char *s);

void CreatePath(char *path);
void QCopyFile(char *from, char *to);
void SafeCreatePath(char *path);

extern qboolean archive;
extern char archivedir[1024];

extern qboolean verbose;

void qprintf(PRINTF_FORMAT_STRING const char *format, ...);

void ExpandWildcards(int *argc, char ***argv);

void CmdLib_AddBasePath(const char *pBasePath);
bool CmdLib_HasBasePath(const char *pFileName, intp &pathLength);
intp CmdLib_GetNumBasePaths(void);
const char *CmdLib_GetBasePath(intp i);
// Like ExpandPath but expands the path for each base path like SafeOpenRead
intp CmdLib_ExpandWithBasePaths(CUtlVector<CUtlString> &expandedPathList,
                                const char *pszPath);

extern bool g_bStopOnExit;

// for compression routines
struct cblock_t {
  byte *data;
  int count;
};

#endif  // SRC_UTILS_COMMON_CMDLIB_H_
