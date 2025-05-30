@ECHO OFF
Setlocal EnableDelayedExpansion

if ["%~1"]==[""] (
  ECHO Please pass solution name as first argument.
  EXIT /B 1
)

SET "SOLUTION_NAME=%~1"
SET "GROUP_NAME=%SOLUTION_NAME%"
SET "GAME_NAME=%SOLUTION_NAME%"
SET CMAKE_MSVC_ARCH_NAME=None

if ["%~2"]==["x86"] (
  SET CMAKE_MSVC_ARCH_NAME=Win32

  ECHO "Generating solution %SOLUTION_NAME% for x86"
  SET "SOLUTION_NAME=%SOLUTION_NAME%_x86"
) else if ["%~2"]==["x64"] (
  SET CMAKE_MSVC_ARCH_NAME=x64

  ECHO "Generating solution %SOLUTION_NAME% for x64"
  SET "SOLUTION_NAME=%SOLUTION_NAME%_x64"
) else (
  ECHO Please pass CPU architecture as second argument. Supported x86 and x64.
  EXIT /B 1
)

REM Build VPC.
MKDIR external\vpc\out
PUSHD external\vpc\out
cmake -G Ninja ..
if ERRORLEVEL 1 (
  ECHO cmake generation for external\vpc failed.
  EXIT /B 1
)

cmake --build . --config Release
if ERRORLEVEL 1 (
  ECHO cmake --build for external\vpc failed.
  EXIT /B 1
)

COPY /V /Y vpc.exe /B ..\..\..\devtools\bin\vpc.exe /B
if ERRORLEVEL 1 (
  ECHO No vpc.exe to copy to devtools\bin\.
  EXIT /B 1
)

COPY /V /Y vpc.pdb /B ..\..\..\devtools\bin\vpc.pdb /B
if ERRORLEVEL 1 (
  ECHO No vpc.pdb to copy to devtools\bin\.
  EXIT /B 1
)
POPD


SET "WIN_X64= "
if ["%CMAKE_MSVC_ARCH_NAME%"]==["x64"] (
  SET "WIN_X64=/windows"

  echo Add /windows arg to VPC as generating x64 solution. 
)

REM Finally create solution.
devtools\bin\vpc.exe /2022 %WIN_X64% /define:WORKSHOP_IMPORT_DISABLE /define:SIXENSE_DISABLE /define:NO_X360_XDK /define:RAD_TELEMETRY_DISABLED /define:DISABLE_ETW /define:NO_STEAM /define:NO_ATI_COMPRESS /define:NO_NVTC /define:LTCG /no_ceg /nofpo +%GROUP_NAME% /mksln %SOLUTION_NAME%.sln /slnitems .slnitems
if ERRORLEVEL 1 (
  ECHO MSBuild for %SOLUTION_NAME%.sln failed.
  EXIT /B 1
)
