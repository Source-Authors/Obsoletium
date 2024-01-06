// Copyright Valve Corporation, All rights reserved.

#include "../vpc/vpc.h"
#include "crccheck_shared.h"
#include "tier1/checksum_crc.h"
#include "tier1/strtools.h"

#include <cstring>
#include <cstdio>
#include <cstdarg>

#include <algorithm>

#ifdef _WIN32
#include <process.h>
#else
#include <cstdlib>
#define stricmp strcasecmp
#endif

#include "tier0/memdbgon.h"

#define MAX_INCLUDE_STACK_DEPTH 10

extern const char *g_szArrPlatforms[];

static bool IsValidPathChar(char token) {
  // does it look like a file?  If this ends up too tight, can probably just
  // check that it's not '[' or '{' cause conditional blocks are what we really
  // want to avoid.
  return isalpha(token) || isdigit(token) || (token == '.') ||
         (token == '\\') || (token == '/');
}

static void BuildReplacements(const char *token, char *replacements) {
  // Now go pickup the any files that exist, but were non-matches
  *replacements = '\0';

  for (ptrdiff_t i = 0; g_szArrPlatforms[i] != nullptr; i++) {
    char path[MAX_PATH];
    char path_expanded[MAX_PATH];

    V_strncpy(path, token, sizeof(path));
    Sys_ReplaceString(path, "$os", g_szArrPlatforms[i], path_expanded,
                      sizeof(path_expanded));
    V_FixSlashes(path_expanded);
    V_RemoveDotSlashes(path_expanded);
    V_FixDoubleSlashes(path_expanded);

    // this fopen is probably using a relative path, but that's ok, as
    // everything in the crc code is opening relative paths and assuming the cwd
    // is set ok.
    FILE *f{fopen(path_expanded, "rb")};
    if (f) {
      fclose(f);

      // strcat - blech
      // really just need to stick the existing platforms seen in
      strcat(replacements, g_szArrPlatforms[i]);
      strcat(replacements, ";");
    }
  }
}

static const char *GetToken(const char *ln, char *token) {
  *token = '\0';

  while (*ln && isspace(*ln)) ln++;

  if (!ln[0]) return NULL;

  if (ln[0] == '"') {  // does vpc allow \" inside the filename string -
                       // shouldn't matter, but we're going to assume no.
    ln++;
    while (*ln) {
      if (ln[0] == '"') break;
      *token++ = *ln++;
    }
    *token = '\0';
  } else if (IsValidPathChar(*ln)) {
    while (*ln) {
      if (isspace(*ln)) break;
      *token++ = *ln++;
    }
    *token = '\0';
  } else {
    token[0] = ln[0];
    token[1] = '\0';
  }

  return ln;
}

static void PerformFileSubstitions(char *line, size_t line_length) {
  static bool is_searching_file{false};
  const char *ln{line};

  if (!is_searching_file) {
    ln = V_stristr(ln, "$file ");

    if (ln) is_searching_file = true;
  }

  if (is_searching_file) {
    char token[1024];
    if (strlen(ln) >= std::size(token)) {
      Sys_Error(
          "Unable to find file in line. Line %s is too large to get token "
          "from");
    }

    ln = GetToken(ln, token);
    if (!ln) return;  // no more tokens on line, we should try the next line

    is_searching_file = false;

    if (V_stristr(token, "$os")) {
      if (!IsValidPathChar(*token))
        fprintf(stderr,
                "Warning: can't expand %s for crc calculation.  Changes to "
                "this file set won't trigger automatic rebuild\n",
                token);

      char replacements[2048];
      char buffer[4096];

      BuildReplacements(token, replacements);
      Sys_ReplaceString(line, "$os", replacements, buffer, sizeof(buffer));
      V_strncpy(line, buffer, line_length);
    }
  }

  static bool is_searching_for_file_pattern{false};
  ln = line;

  if (!is_searching_for_file_pattern) {
    ln = V_stristr(ln, "$filepattern");
    while (ln) {
      ln += 13;
      if (isspace(ln[-1])) {
        is_searching_for_file_pattern = true;
        break;
      }
    }
  }

  if (is_searching_for_file_pattern) {
    char token[1024];
    if (strlen(ln) >= std::size(token)) {
      Sys_Error(
          "Unable to find file pattern in line. Line %s is too large to get "
          "token from");
    }

    ln = GetToken(ln, token);
    if (!ln) return;  // no more tokens on line, we should try the next line

    is_searching_for_file_pattern = false;

    char replacements[2048];
    replacements[0] = '\0';

    char buffer[4096];
    CUtlVector<CUtlString> results;
    Sys_ExpandFilePattern(token, results);

    if (results.Count()) {
      for (auto &&r : results) {
        V_strncat(replacements, CFmtStr("%s;", r.String()).Access(),
                  V_ARRAYSIZE(replacements));
      }

      CRC32_t crc{
          CRC32_ProcessSingleBuffer(replacements, V_strlen(replacements))};

      Sys_ReplaceString(line, token, CFmtStr("%s:%u", token, crc).Access(),
                        buffer, sizeof(buffer));
      V_strncpy(line, buffer, line_length);
    } else {
      if (!IsValidPathChar(*token))
        fprintf(
            stderr,
            "Warning: %s couldn't be expanded during crc calculation.  Changes "
            "to this file set won't trigger automatic project rebuild\n",
            token);
    }
  }
}

//-----------------------------------------------------------------------------
//	Sys_Error
//
//-----------------------------------------------------------------------------
[[noreturn]] void Sys_Error(PRINTF_FORMAT_STRING const char *format, ...) {
  va_list argptr;

  va_start(argptr, format);
  vfprintf(stderr, format, argptr);
  va_end(argptr);

  exit(1);
}

static void SafeSnprintf(char *out, int out_length,
                         PRINTF_FORMAT_STRING const char *format, ...) {
  va_list marker;
  va_start(marker, format);
  V_vsnprintf(out, out_length, format, marker);
  va_end(marker);

  out[out_length - 1] = '\0';
}

// for linked lists of strings
struct StringNode_t {
  StringNode_t *m_pNext;
  char m_Text[1];  // the string data
};

static StringNode_t *MakeStrNode(char const *str) {
  constexpr size_t kNodeSize{sizeof(StringNode_t)};
  const size_t node_size{kNodeSize + strlen(str)};

  alignas(unsigned char *) auto *rc =
      reinterpret_cast<StringNode_t *>(new unsigned char[node_size]);

  strcpy(rc->m_Text, str);
  rc->m_pNext = nullptr;
  return rc;
}

//-----------------------------------------------------------------------------
//	Sys_LoadTextFileWithIncludes
//-----------------------------------------------------------------------------
size_t Sys_LoadTextFileWithIncludes(const char *file_name, char **buffer,
                                    bool should_insert_file_macro_expansion) {
  FILE *file_stack[MAX_INCLUDE_STACK_DEPTH];
  int file_stack_it{MAX_INCLUDE_STACK_DEPTH};

  StringNode_t *file_lines{nullptr};  // tail ptr for fast adds

  size_t total_file_bytes{0};
  FILE *handle{fopen(file_name, "r")};
  if (!handle) return std::numeric_limits<size_t>::max();

  char line_buffer[4096];

  file_stack[--file_stack_it] = handle;  // push
  while (file_stack_it < MAX_INCLUDE_STACK_DEPTH) {
    // read lines
    for (;;) {
      char *ln{
          fgets(line_buffer, sizeof(line_buffer), file_stack[file_stack_it])};
      if (!ln) break;  // out of text

      ln += strspn(ln, "\t ");  // skip white space

      // Need to insert actual files to make sure crc changes if disk-matched
      // files match
      if (should_insert_file_macro_expansion)
        PerformFileSubstitions(ln, sizeof(line_buffer) - (ln - line_buffer));

      if (memcmp(ln, "#include", 8) == 0) {
        // omg, an include
        ln += 8;
        ln += strspn(ln, " \t\"<");  // skip whitespace, ", and <

        size_t path_name_length{strcspn(ln, " \t\">\n")};
        if (!path_name_length) {
          Sys_Error("bad include %s via %s\n", line_buffer, file_name);
        }
        ln[path_name_length] = 0;  // kill everything after end of filename

        FILE *include_file{fopen(ln, "r")};
        if (!include_file) {
          Sys_Error("can't open #include of %s\n", ln);
        }

        if (!file_stack_it) {
          Sys_Error("include nesting too deep via %s", file_name);
        }

        file_stack[--file_stack_it] = include_file;
      } else {
        const size_t line_length{strlen(ln)};

        total_file_bytes += line_length;

        StringNode_t *new_line{MakeStrNode(ln)};

        new_line->m_pNext = file_lines;
        file_lines = new_line;
      }
    }

    fclose(file_stack[file_stack_it]);
    file_stack_it++;  // pop stack
  }

  // Reverse the pFileLines list so it goes the right way.
  StringNode_t *prev{nullptr};
  StringNode_t *it;

  for (it = file_lines; it;) {
    StringNode_t *next{it->m_pNext};

    it->m_pNext = prev;
    prev = it;
    it = next;
  }
  file_lines = prev;

  // Now dump all the lines out into a single buffer.
  char *result_buffer = new char[total_file_bytes + 1];  // and null
  *buffer = result_buffer;                               // tell caller

  // copy all strings and null terminate
  size_t line{0};
  StringNode_t *next;
  for (it = file_lines; it; it = next) {
    next = it->m_pNext;

    const size_t length{strlen(it->m_Text)};

    memcpy(result_buffer, it->m_Text, length);
    result_buffer += length;
    line++;

    // Cleanup the line..
    // delete [] (unsigned char*)pCur;
  }

  *(result_buffer++) = '\0';  // null

  return total_file_bytes;
}

// Just like fgets() but it removes trailing newlines.
template <int out_bytes>
static char *ChompLineFromFile(char (&out)[out_bytes], FILE *file) {
  char *line{fgets(out, out_bytes, file)};
  if (line) {
    const size_t length{strlen(line)};
    if (length > 0 && line[length - 1] == '\n') {
      line[length - 1] = '\0';

      if (length > 1 && line[length - 2] == '\r') line[length - 2] = '\0';
    }
  }

  return line;
}

static bool CheckSupplementalString(const char *supplemental,
                                    const char *reference) {
  // The supplemental string is only checked while VPC is determining if a
  // project file is stale or not. It's not used by the pre-build event's CRC
  // check. The supplemental string contains various options that tell how the
  // project was built. It's generated in VPC_GenerateCRCOptionString.
  //
  // If there's no reference supplemental string (which is the case if we're
  // running vpccrccheck.exe), then we ignore it and continue.
  if (!reference) return true;

  return (supplemental && stricmp(supplemental, reference) == 0);
}

static bool CheckVPCExeCRC(char *vpc_crc_check, const char *file_name,
                           char *error, int error_length) {
  if (vpc_crc_check == NULL) {
    SafeSnprintf(error, error_length, "Unexpected end-of-file in %s",
                 file_name);
    return false;
  }

  char *space{strchr(vpc_crc_check, ' ')};
  if (!space) {
    SafeSnprintf(error, error_length, "Invalid line ('%s') in %s",
                 vpc_crc_check, file_name);
    return false;
  }

  // Null-terminate it so we have the CRC by itself and the filename follows the
  // space.
  *space = '\0';
  const char *vpc_file_name{space + 1};

  // Parse the CRC out.
  unsigned int reference_crc;
  if (sscanf(vpc_crc_check, "%x", &reference_crc) != 1) {
    SafeSnprintf(error, error_length,
                 "Missed reference CRC for %s: %s is not CRC.", vpc_file_name,
                 vpc_crc_check);
    return false;
  }

  char *buffer;
  const int vpc_exe_size{Sys_LoadFile(vpc_file_name, (void **)&buffer)};
  if (!buffer) {
    SafeSnprintf(error, error_length, "Unable to load %s for comparison.",
                 vpc_file_name);
    return false;
  }

  if (vpc_exe_size < 0) {
    SafeSnprintf(error, error_length, "Could not load file '%s' to check CRC",
                 vpc_file_name);
    return false;
  }

  // Calculate the CRC from the contents of the file.
  const CRC32_t actual_crc{CRC32_ProcessSingleBuffer(buffer, vpc_exe_size)};
  // Allocated via malloc buffer.
  free(buffer);

  // Compare them.
  if (actual_crc != reference_crc) {
    SafeSnprintf(error, error_length,
                 "VPC executable has changed since the project was generated.");
    return false;
  }

  return true;
}

bool VPC_CheckProjectDependencyCRCs(const char *project_file_name,
                                    const char *reference_supplemental,
                                    char *error, int error_length) {
  // Build the xxxxx.vcproj.vpc_crc filename
  char file_name[512];
  SafeSnprintf(file_name, sizeof(file_name), "%s.%s", project_file_name,
               VPCCRCCHECK_FILE_EXTENSION);

  // Open it up.
  FILE *file{fopen(file_name, "rt")};
  if (!file) {
    SafeSnprintf(error, error_length, "Unable to load %s to check CRC strings",
                 file_name);
    return false;
  }

  bool rc{false};
  char line_buffer[2048];

  // Check the version of the CRC file.
  const char *version{ChompLineFromFile(line_buffer, file)};
  if (version && stricmp(version, VPCCRCCHECK_FILE_VERSION_STRING) == 0) {
    char *vpc_exe_crc{ChompLineFromFile(line_buffer, file)};
    if (CheckVPCExeCRC(vpc_exe_crc, file_name, error, error_length)) {
      // Check the supplemental CRC string.
      const char *supplemental{ChompLineFromFile(line_buffer, file)};
      if (CheckSupplementalString(supplemental, reference_supplemental)) {
        // Now read each line. Each line has a CRC and a filename on it.
        while (1) {
          char *line{ChompLineFromFile(line_buffer, file)};
          if (!line) {
            // We got all the way through the file without a CRC error, so all's
            // well.
            rc = true;
            break;
          }

          char *space = strchr(line, ' ');
          if (!space) {
            SafeSnprintf(error, error_length, "Invalid line ('%s') in %s", line,
                         file_name);
            break;
          }

          // Null-terminate it so we have the CRC by itself and the filename
          // follows the space.
          *space = '\0';
          const char *vpc_file_name = space + 1;

          // Parse the CRC out.
          unsigned int reference_crc;
          if (sscanf(line, "%x", &reference_crc) != 1) {
            SafeSnprintf(error, error_length,
                         "Invalid reference CRC at line ('%s') in %s", line,
                         file_name);
            break;
          }

          // Calculate the CRC from the contents of the file.
          char *buffer;
          const size_t total_file_bytes{
              Sys_LoadTextFileWithIncludes(vpc_file_name, &buffer, true)};
          if (total_file_bytes == std::numeric_limits<size_t>::max()) {
            SafeSnprintf(error, error_length,
                         "Unable to load %s for CRC comparison.",
                         vpc_file_name);
            break;
          }

          const CRC32_t actual_crc{
              CRC32_ProcessSingleBuffer(buffer, total_file_bytes)};
          delete[] buffer;

          // Compare them.
          if (actual_crc != reference_crc) {
            SafeSnprintf(error, error_length,
                         "This VCPROJ is out of sync with its VPC scripts.\n  "
                         "%s mismatches (0x%x vs 0x%x).\n  Please use VPC to "
                         "re-generate!\n  \n",
                         vpc_file_name, reference_crc, actual_crc);
            break;
          }
        }
      } else {
        SafeSnprintf(error, error_length, "Supplemental string mismatch.");
      }
    }
  } else {
    SafeSnprintf(error, error_length,
                 "CRC file %s has an invalid version string ('%s')", file_name,
                 version ? version : "[null]");
  }

  fclose(file);
  return rc;
}

static int VPC_OldStyleCRCChecks(int argc, const char **argv) {
  for (int i{1}; i + 2 < argc;) {
    const char *arg{argv[i]};

    if (stricmp(arg, "-crc") != 0) {
      ++i;
      continue;
    }

    const char *vpc_file_name{argv[i + 1]};

    // Get the CRC value on the command line.
    const char *test_crc{argv[i + 2]};
    unsigned int crc_from_cmd;

    if (sscanf(test_crc, "%x", &crc_from_cmd) != 1) {
      Sys_Error("Unable to parse CRC from command line: %s not a hex CRC.",
                test_crc);
    }

    // Calculate the CRC from the contents of the file.
    char *buffer;
    const size_t file_size{
        Sys_LoadTextFileWithIncludes(vpc_file_name, &buffer, true)};
    if (file_size == std::numeric_limits<size_t>::max()) {
      Sys_Error("Unable to load %s for CRC comparison.", vpc_file_name);
    }

    CRC32_t actual_crc{CRC32_ProcessSingleBuffer(buffer, file_size)};
    delete[] buffer;

    // Compare them.
    if (actual_crc != crc_from_cmd) {
      Sys_Error(
          "  \n  This VCPROJ is out of sync with its VPC scripts.\n  %s "
          "mismatches (0x%x vs 0x%x).\n  Please use VPC to re-generate!\n  \n",
          vpc_file_name, crc_from_cmd, actual_crc);
    }

    i += 2;
  }

  return 0;
}

int VPC_CommandLineCRCChecks(int argc, const char **argv) {
  if (argc < 2) {
    fprintf(stderr,
            "Invalid arguments to " VPCCRCCHECK_EXE_FILENAME
            ". Format: " VPCCRCCHECK_EXE_FILENAME " [project filename]\n");
    return EINVAL;
  }

  const char *first_crc{argv[1]};

  // If the first argument starts with -crc but is not -crc2, then this is an
  // old CRC check command line with all the CRCs and filenames directly on the
  // command line. The new format puts all that in a separate file.
  if (first_crc[0] == '-' && first_crc[1] == 'c' && first_crc[2] == 'r' &&
      first_crc[3] == 'c' && first_crc[4] != '2') {
    return VPC_OldStyleCRCChecks(argc, argv);
  }

  if (stricmp(first_crc, "-crc2") != 0) {
    fprintf(stderr, "Missing -crc2 parameter on vpc CRC check command line.");
    return EINVAL;
  }

  const char *project_file_name{argv[2]};

  char error[1024];
  bool is_crc_valid{
      VPC_CheckProjectDependencyCRCs(project_file_name, nullptr, error)};

  if (is_crc_valid) return 0;

  fprintf(stderr, "%s", error);
  return EINVAL;
}
