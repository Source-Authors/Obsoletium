// Copyright Valve Corporation, All rights reserved.

#ifndef VPCCRCHECK_CRCCHECK_SHARED_H_
#define VPCCRCHECK_CRCCHECK_SHARED_H_

#ifdef STANDALONE_VPC
#define VPCCRCCHECK_EXE_FILENAME "vpc.exe"
#else
#define VPCCRCCHECK_EXE_FILENAME "vpccrccheck.exe"
#endif

// The file extension for the file that contains the CRCs that a vcproj depends
// on.
#define VPCCRCCHECK_FILE_EXTENSION "vpc_crc"
#define VPCCRCCHECK_FILE_VERSION_STRING "[vpc crc file version 2]"

[[noreturn]] void Sys_Error(PRINTF_FORMAT_STRING const char *format, ...);

size_t Sys_LoadTextFileWithIncludes(const char *file_name, char **buffer,
                                    bool should_insert_file_macro_expansion);

bool VPC_CheckProjectDependencyCRCs(const char *project_file_name,
                                    const char *reference_summplemental,
                                    char *error, int error_length);

template <int error_length>
bool VPC_CheckProjectDependencyCRCs(const char *project_file_name,
                                    const char *reference_summplemental,
                                    char (&error)[error_length]) {
  return VPC_CheckProjectDependencyCRCs(
      project_file_name, reference_summplemental, error, error_length);
}

// Used by vpccrccheck.exe or by vpc.exe to do the CRC check that's initiated in
// the pre-build steps.
int VPC_CommandLineCRCChecks(int argc, const char **argv);

#endif  // VPCCRCHECK_CRCCHECK_SHARED_H_
