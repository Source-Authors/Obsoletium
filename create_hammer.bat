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

REM Set CMake / MSVC generator / architecture / platform.
SET CMAKE_MSVC_GEN_NAME="Visual Studio 17 2022"


REM Build bzip2.
PUSHD thirdparty\bzip2
nmake -f makefile.msc

if ERRORLEVEL 1 (
  ECHO nmake for thirdparty\bzip2 failed.
  EXIT /B 1
)
POPD


REM Build cryptopp.
MSBuild.exe /m /p:Platform=%MSBUILD_PLATFORM% /p:Configuration=Debug thirdparty\cryptopp\cryptlib.vcxproj
if ERRORLEVEL 1 (
  ECHO MSBuild Debug for thirdparty\cryptopp\cryptlib failed.
  EXIT /B 1
)

MSBuild.exe /m /p:Platform=%MSBUILD_PLATFORM% /p:Configuration=Release thirdparty\cryptopp\cryptlib.vcxproj
if ERRORLEVEL 1 (
  ECHO MSBuild Release for thirdparty\cryptopp\cryptlib failed.
  EXIT /B 1
)

REM Build libjpeg-turbo.
MKDIR thirdparty\libjpeg-turbo\out
PUSHD thirdparty\libjpeg-turbo\out
cmake -G %CMAKE_MSVC_GEN_NAME% -A %CMAKE_MSVC_ARCH_NAME% -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_CDJPEG=OFF -DWITH_TURBOJPEG=OFF ..
if ERRORLEVEL 1 (
  ECHO cmake generation for thirdparty\libjpeg-turbo failed.
  EXIT /B 1
)

cmake --build . --config Release
if ERRORLEVEL 1 (
  ECHO cmake --build for thirdparty\libjpeg-turbo failed.
  EXIT /B 1
)
POPD


REM Build zlib.
MKDIR thirdparty\zlib\out
PUSHD thirdparty\zlib\out
cmake -G %CMAKE_MSVC_GEN_NAME% -A %CMAKE_MSVC_ARCH_NAME% ..
if ERRORLEVEL 1 (
  ECHO cmake generation for thirdparty\zlib failed.
  EXIT /B 1
)

cmake --build . --config Release
if ERRORLEVEL 1 (
  ECHO cmake --build for thirdparty\zlib failed.
  EXIT /B 1
)
POPD


REM Build SDL.
MKDIR thirdparty\SDL\out
PUSHD thirdparty\SDL\out
REM Use Ninja generator to overcome Github CI issues
REM https://github.com/actions/runner-images/issues/10980
cmake -G Ninja -DSDL_TEST=OFF ..
if ERRORLEVEL 1 (
  ECHO cmake generation for thirdparty\SDL failed.
  EXIT /B 1
)

cmake --build . --config Release
if ERRORLEVEL 1 (
  ECHO cmake --build for thirdparty\SDL failed.
  EXIT /B 1
)
POPD


REM Build mpaheaderinfo.
MKDIR thirdparty\mpaheaderinfo\out
PUSHD thirdparty\mpaheaderinfo\out
cmake -G %CMAKE_MSVC_GEN_NAME% -A %CMAKE_MSVC_ARCH_NAME% -DMPA_BUILD_WITHOUT_ATL=ON ..
if ERRORLEVEL 1 (
  ECHO cmake generation for thirdparty\mpaheaderinfo failed.
  EXIT /B 1
)

cmake --build . --config Release
if ERRORLEVEL 1 (
  ECHO cmake --build Release for thirdparty\mpaheaderinfo failed.
  EXIT /B 1
)

cmake --build . --config Debug
if ERRORLEVEL 1 (
  ECHO cmake --build Debug for thirdparty\mpaheaderinfo failed.
  EXIT /B 1
)
POPD


REM Build NvTriStrip.
MKDIR thirdparty\nvtristrip\out
PUSHD thirdparty\nvtristrip\out
cmake -G Ninja ..
if ERRORLEVEL 1 (
  ECHO cmake generation for thirdparty\NvTriStrip failed.
  EXIT /B 1
)

cmake --build . --config Release
if ERRORLEVEL 1 (
  ECHO cmake --build for thirdparty\NvTriStrip failed.
  EXIT /B 1
)
POPD


REM Build snappy.
MKDIR thirdparty\snappy\out
PUSHD thirdparty\snappy\out
cmake -G %CMAKE_MSVC_GEN_NAME% -A %CMAKE_MSVC_ARCH_NAME% -DBUILD_SHARED_LIBS=OFF -DSNAPPY_BUILD_TESTS=OFF -DSNAPPY_BUILD_BENCHMARKS=OFF ..
if ERRORLEVEL 1 (
  ECHO cmake generation for thirdparty\snappy failed.
  EXIT /B 1
)

cmake --build . --config Release
if ERRORLEVEL 1 (
  ECHO cmake --build Release for thirdparty\snappy failed.
  EXIT /B 1
)

cmake --build . --config Debug
if ERRORLEVEL 1 (
  ECHO cmake --build Debug for thirdparty\snappy failed.
  EXIT /B 1
)
POPD


REM Build zip-utils.
MKDIR thirdparty\zip-utils\out
PUSHD thirdparty\zip-utils\out
cmake -G Ninja ..
if ERRORLEVEL 1 (
  ECHO cmake generation for thirdparty\zip-utils failed.
  EXIT /B 1
)

cmake --build . --config Release
if ERRORLEVEL 1 (
  ECHO cmake --build for thirdparty\zip-utils failed.
  EXIT /B 1
)
POPD


REM Generate version info, etc.
MKDIR out
PUSHD out
cmake ..\CMakeLists.txt
if ERRORLEVEL 1 (
  ECHO cmake version generation failed.
  EXIT /B 1
)
POPD


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
devtools\bin\vpc.exe /2022 %WIN_X64% /define:WORKSHOP_IMPORT_DISABLE /define:SIXENSE_DISABLE /define:NO_X360_XDK /define:RAD_TELEMETRY_DISABLED /define:DISABLE_ETW /define:NO_STEAM /define:NO_ATI_COMPRESS /define:NO_NVTC /define:LTCG /no_ceg /nofpo +%GROUP_NAME% /mksln %SOLUTION_NAME%.sln /slnitems .slnitems
if ERRORLEVEL 1 (
  ECHO MSBuild for %SOLUTION_NAME%.sln failed.
  EXIT /B 1
)
