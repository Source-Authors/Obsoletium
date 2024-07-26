@ECHO OFF
Setlocal EnableDelayedExpansion

REM Check MSBuild present in PATH.
MSBuild.exe /? >NUL
IF ERRORLEVEL 1 (
  ECHO MsBuild not found in PATH. Please, start from Developer Command Prompt or add MSVC MsBuild directory to the PATH.
  EXIT /B 1
)

if ["%~1"]==[""] (
  ECHO Please pass solution name as first argument.
  EXIT /B 1
)

SET "SOLUTION_NAME=%~1"
SET "GROUP_NAME=%SOLUTION_NAME%"
SET CMAKE_MSVC_ARCH_NAME=None
SET MSBUILD_PLATFORM=None

if ["%~2"]==["x86"] (
  SET CMAKE_MSVC_ARCH_NAME=Win32
  SET MSBUILD_PLATFORM=x86

  ECHO "Generating solution %SOLUTION_NAME for x86"
  SET "SOLUTION_NAME=%SOLUTION_NAME%_x86"
) else if ["%~2"]==["x64"] (
  SET CMAKE_MSVC_ARCH_NAME=x64
  SET MSBUILD_PLATFORM=x64

  ECHO "Generating solution %SOLUTION_NAME_x64 for x64"
  SET "SOLUTION_NAME=%SOLUTION_NAME%_x64"
) else (
  ECHO Please pass CPU architecture as second argument. Supported x86 and x64.
  EXIT /B 1
)

REM Build VPC.
MSBuild.exe /m /p:Platform=x64 /p:Configuration=Release external/vpc/vpc.sln
if ERRORLEVEL 1 (
  ECHO MSBuild Release x64 for external/vpc/vpc.sln failed.
  EXIT /B 1
)

SET "WIN_X64= "
if ["%CMAKE_MSVC_ARCH_NAME%"]==["x64"] (
  SET "WIN_X64=/windows"

  echo Add /windows arg to VPC as generating x64 solution. 
)


REM Finally create solution.
devtools\bin\vpc.exe /2022 %WIN_X64% /define:WORKSHOP_IMPORT_DISABLE /define:SIXENSE_DISABLE /define:NO_X360_XDK /define:RAD_TELEMETRY_DISABLED /define:DISABLE_ETW /define:NO_STEAM /define:NO_ATI_COMPRESS /define:NO_NVTC /define:LTCG /no_ceg /nofpo +%GROUP_NAME% /mksln %SOLUTION_NAME%.sln
if ERRORLEVEL 1 (
  ECHO MSBuild for %SOLUTION_NAME%.sln failed.
  EXIT /B 1
)