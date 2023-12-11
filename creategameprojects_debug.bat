@ECHO OFF

REM Check MSBuild present in PATH.
MSBuild.exe /? >NUL
IF ERRORLEVEL 1 (
  ECHO MsBuild not found in PATH. Please, start from Developer Command Prompt or add MSVC MsBuild directory to the PATH.
  EXIT /B 1
)

REM Add/check registry keys for VPC MSVC project genereation.
REG QUERY HKLM\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\10.0\Projects\{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942} /t REG_SZ /v DefaultProjectExtension /f vcproj
if ERRORLEVEL 1 (
  ECHO Missed registry key HKLM\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\10.0\Projects\{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942} REG_SZ DefaultProjectExtension vcproj.
  REG ADD HKLM\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\10.0\Projects\{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942} /t REG_SZ /v DefaultProjectExtension /d vcproj

  if ERRORLEVEL 1 (
    ECHO Unable to add registry key HKLM\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\10.0\Projects\{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942} REG_SZ DefaultProjectExtension vcproj.
    EXIT /B 1
  )
)

REM Set CMake / MSVC generator / architecture / platform.
SET CMAKE_MSVC_ARCH_NAME=Win32
SET CMAKE_MSVC_GEN_NAME="Visual Studio 17 2022"
SET MSBUILD_PLATFORM=x86

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


SET ZLIB_SOURCE_DIR=%cd%\thirdparty\zlib


REM Build libpng.
MKDIR thirdparty\libpng\out
PUSHD thirdparty\libpng\out
cmake -G %CMAKE_MSVC_GEN_NAME% -A %CMAKE_MSVC_ARCH_NAME% -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_TOOLS=OFF -DPNG_TESTS=OFF -DPNG_BUILD_ZLIB=ON -DZLIB_INCLUDE_DIRS=%ZLIB_SOURCE_DIR%;%ZLIB_SOURCE_DIR%\out -DZLIB_LIBRARIES=%ZLIB_SOURCE_DIR%\out\zlibstatic.lib ..
if ERRORLEVEL 1 (
  ECHO cmake generation for thirdparty\libpng failed.
  EXIT /B 1
)

cmake --build . --config Release
if ERRORLEVEL 1 (
  ECHO cmake --build for thirdparty\libpng failed.
  EXIT /B 1
)
POPD


REM Build MPAHeaderInfo.
MKDIR thirdparty\MPAHeaderInfo\out
PUSHD thirdparty\MPAHeaderInfo\out
cmake -G %CMAKE_MSVC_GEN_NAME% -A %CMAKE_MSVC_ARCH_NAME% -DMPA_BUILD_WITHOUT_ATL=ON ..
if ERRORLEVEL 1 (
  ECHO cmake generation for thirdparty\MPAHeaderInfo failed.
  EXIT /B 1
)

cmake --build . --config Release
if ERRORLEVEL 1 (
  ECHO cmake --build Release for thirdparty\MPAHeaderInfo failed.
  EXIT /B 1
)

cmake --build . --config Debug
if ERRORLEVEL 1 (
  ECHO cmake --build Debug for thirdparty\MPAHeaderInfo failed.
  EXIT /B 1
)
POPD


REM Build protobuf.
MSBuild.exe /m /p:Platform=%MSBUILD_PLATFORM% /p:Configuration=Debug thirdparty\protobuf\vsprojects\libprotobuf.vcxproj
if ERRORLEVEL 1 (
  ECHO MSBuild Debug for thirdparty\protobuf\libprotobuf failed.
  EXIT /B 1
)
MSBuild.exe /m /p:Platform=%MSBUILD_PLATFORM% /p:Configuration=Release thirdparty\protobuf\vsprojects\libprotobuf.vcxproj
if ERRORLEVEL 1 (
  ECHO MSBuild Release for thirdparty\protobuf\libprotobuf failed.
  EXIT /B 1
)
MSBuild.exe /m /p:Platform=%MSBUILD_PLATFORM% /p:Configuration=Debug thirdparty\protobuf\vsprojects\protoc.vcxproj
if ERRORLEVEL 1 (
  ECHO MSBuild Debug for thirdparty\protobuf\protoc failed.
  EXIT /B 1
)
MSBuild.exe /m /p:Platform=%MSBUILD_PLATFORM% /p:Configuration=Release thirdparty\protobuf\vsprojects\protoc.vcxproj
if ERRORLEVEL 1 (
  ECHO MSBuild Release for thirdparty\protobuf\protoc failed.
  EXIT /B 1
)


REM Build SDL.
MKDIR thirdparty\SDL\out
PUSHD thirdparty\SDL\out
cmake -G %CMAKE_MSVC_GEN_NAME% -A %CMAKE_MSVC_ARCH_NAME% -DSDL_TEST=OFF ..
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


REM Build snappy.
MKDIR thirdparty\snappy\out
PUSHD thirdparty\snappy\out
cmake -G %CMAKE_MSVC_GEN_NAME% -A %CMAKE_MSVC_ARCH_NAME% -DBUILD_SHARED_LIBS=OFF ..
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


REM Build VPC.
MSBuild.exe /m /p:Platform=%MSBUILD_PLATFORM% /p:Configuration=Release external/vpc/vpc.sln
if ERRORLEVEL 1 (
  ECHO MSBuild Release for external/vpc/vpc.sln failed.
  EXIT /B 1
)


REM Finally create solution.
devtools\bin\vpc.exe /2015 /define:WORKSHOP_IMPORT_DISABLE /define:SIXENSE_DISABLE /define:NO_X360_XDK /define:RAD_TELEMETRY_DISABLED /define:DISABLE_ETW /define:NO_STEAM /define:NO_ATI_COMPRESS /define:NO_NVTC /define:LTCG /no_ceg /nofpo /hl2 +game /mksln hl2.sln
if ERRORLEVEL 1 (
  ECHO MSBuild for hl2.sln failed.
  EXIT /B 1
)
