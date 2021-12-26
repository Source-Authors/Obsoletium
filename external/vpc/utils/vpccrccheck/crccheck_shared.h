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
int Sys_LoadTextFileWithIncludes(const char *filename, char **bufferptr,
                                 bool bInsertFileMacroExpansion);

bool VPC_CheckProjectDependencyCRCs(const char *pProjectFilename,
                                    const char *pReferenceSupplementalString,
                                    char *pErrorString, int nErrorStringLength);

// Used by vpccrccheck.exe or by vpc.exe to do the CRC check that's initiated in
// the pre-build steps.
int VPC_CommandLineCRCChecks(int argc, const char **argv);

#endif  // VPCCRCHECK_CRCCHECK_SHARED_H_
