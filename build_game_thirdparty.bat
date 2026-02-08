@ECHO OFF
Setlocal EnableDelayedExpansion

REM Check MSBuild present in PATH.
MSBuild.exe /? >NUL
IF ERRORLEVEL 1 (
  ECHO MsBuild not found in PATH. Please, start from Developer Command Prompt or add MSVC MsBuild directory to the PATH.
  EXIT /B 1
)

SET CMAKE_MSVC_ARCH_NAME=None
SET MSBUILD_PLATFORM=None

if ["%~1"]==["x86"] (
  SET CMAKE_MSVC_ARCH_NAME=Win32
  SET MSBUILD_PLATFORM=x86
) else if ["%~1"]==["x64"] (
  SET CMAKE_MSVC_ARCH_NAME=x64
  SET MSBUILD_PLATFORM=x64
) else (
  ECHO Please pass CPU architecture as second argument. Supported x86 and x64.
  EXIT /B 1
)

REM This check previously was in create_game_projects.bat but we split a part of it into here since this way earlier in the build process
if ["%CMAKE_MSVC_ARCH_NAME%"]==["x64"] (
  ECHO Searching for ml64 in PATH... >NUL
  WHERE ml64.exe >NUL

  if ERRORLEVEL 1 (
    REM RaphaelIT7: Had it happen that this errored since I used the x86 one but there all things already compiled for 32x making a mess to clean up
    ECHO Unable to find ml64 in PATH. Are you using the x64 Command Prompt?
    EXIT /B 1
  )
)

REM Set CMake / MSVC generator / architecture / platform.
SET CMAKE_MSVC_GEN_NAME="Visual Studio 18 2026"

REM Build bzip2.
PUSHD thirdparty\bzip2
nmake -f makefile.msc

if ERRORLEVEL 1 (
  ECHO nmake for thirdparty\bzip2 failed.
  EXIT /B 1
)
POPD


REM Build celt.
MSBuild.exe /m /p:Platform=%MSBUILD_PLATFORM% /p:Configuration=Debug thirdparty\celt\win32\VS2022\libcelt\libcelt.vcxproj
if ERRORLEVEL 1 (
  ECHO MSBuild Debug for thirdparty\celt failed.
  EXIT /B 1
)

MSBuild.exe /m /p:Platform=%MSBUILD_PLATFORM% /p:Configuration=Release thirdparty\celt\win32\VS2022\libcelt\libcelt.vcxproj
if ERRORLEVEL 1 (
  ECHO MSBuild Release for thirdparty\celt failed.
  EXIT /B 1
)


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
cmake -G %CMAKE_MSVC_GEN_NAME% -A %CMAKE_MSVC_ARCH_NAME% -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_TOOLS=OFF -DWITH_TURBOJPEG=OFF ..
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


SET ZLIB_SOURCE_DIR="%cd%\thirdparty\zlib"


REM Build libpng.
MKDIR thirdparty\libpng\out
PUSHD thirdparty\libpng\out
cmake -G %CMAKE_MSVC_GEN_NAME% -A %CMAKE_MSVC_ARCH_NAME% -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_TOOLS=OFF -DPNG_TESTS=OFF -DPNG_BUILD_ZLIB=ON -DZLIB_INCLUDE_DIRS='"%ZLIB_SOURCE_DIR%";"%ZLIB_SOURCE_DIR%"\out' -DZLIB_LIBRARIES="%ZLIB_SOURCE_DIR%\out\zlibstatic.lib" ..
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


REM Build lua.
MKDIR thirdparty\lua\out
PUSHD thirdparty\lua\out
cmake -G %CMAKE_MSVC_GEN_NAME% -A %CMAKE_MSVC_ARCH_NAME% -DLUA_ENABLE_TESTING=OFF -DLUA_BUILD_COMPILER=OFF -DLUA_BUILD_AS_CXX=ON ..
if ERRORLEVEL 1 (
  ECHO cmake generation for thirdparty\lua failed.
  EXIT /B 1
)

cmake --build . --config Debug
if ERRORLEVEL 1 (
  ECHO cmake --build Debug for thirdparty\lua failed.
  EXIT /B 1
)

cmake --build . --config Release
if ERRORLEVEL 1 (
  ECHO cmake --build Release for thirdparty\lua failed.
  EXIT /B 1
)
POPD


REM Build mpaheaderinfo.
MKDIR thirdparty\mpaheaderinfo\out
PUSHD thirdparty\mpaheaderinfo\out
cmake -G %CMAKE_MSVC_GEN_NAME% -A %CMAKE_MSVC_ARCH_NAME% -DSA_MPA_BUILD_WITHOUT_ATL=ON ..
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
REM Use Ninja generator to overcome Github CI issues
REM https://github.com/actions/runner-images/issues/10980
cmake -G Ninja ..
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


REM Build nvtristrip.
MKDIR thirdparty\nvtristrip\out
PUSHD thirdparty\nvtristrip\out
cmake -G %CMAKE_MSVC_GEN_NAME% -A %CMAKE_MSVC_ARCH_NAME% ..
if ERRORLEVEL 1 (
  ECHO cmake generation for thirdparty\nvtristrip failed.
  EXIT /B 1
)

cmake --build . --config Release
if ERRORLEVEL 1 (
  ECHO cmake --build Release for thirdparty\nvtristrip failed.
  EXIT /B 1
)

cmake --build . --config Debug
if ERRORLEVEL 1 (
  ECHO cmake --build Debug for thirdparty\nvtristrip failed.
  EXIT /B 1
)
POPD


REM Build shadercompile2.
MKDIR thirdparty\shadercompile\out
PUSHD thirdparty\shadercompile\out
cmake -G %CMAKE_MSVC_GEN_NAME% -A x64 ..
if ERRORLEVEL 1 (
  ECHO cmake generation for thirdparty\shadercompile failed.
  EXIT /B 1
)

cmake --build . --config Release
if ERRORLEVEL 1 (
  ECHO cmake --build Release for thirdparty\shadercompile failed.
  EXIT /B 1
)
POPD
COPY /Y thirdparty\shadercompile\out\Release\D3DX9_43.dll devtools\bin\D3DX9_43.dll
COPY /Y thirdparty\shadercompile\out\Release\D3DCompiler_43.dll devtools\bin\D3DCompiler_43.dll
COPY /Y thirdparty\shadercompile\out\Release\ShaderCompile.exe devtools\bin\ShaderCompile2.exe


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


REM Build speex.
MSBuild.exe /m /p:Platform=%MSBUILD_PLATFORM% /p:Configuration=Debug thirdparty\speex\win32\VS2022\libspeex\libspeex.vcxproj
if ERRORLEVEL 1 (
  ECHO MSBuild Debug for thirdparty\speex failed.
  EXIT /B 1
)

MSBuild.exe /m /p:Platform=%MSBUILD_PLATFORM% /p:Configuration=Release thirdparty\speex\win32\VS2022\libspeex\libspeex.vcxproj
if ERRORLEVEL 1 (
  ECHO MSBuild Release for thirdparty\speex failed.
  EXIT /B 1
)


REM Build squirrel.
MKDIR thirdparty\squirrel\out
PUSHD thirdparty\squirrel\out
cmake -G %CMAKE_MSVC_GEN_NAME% -A %CMAKE_MSVC_ARCH_NAME% -DBUILD_SHARED_LIBS=ON -DDISABLE_STATIC=ON -DLONG_OUTPUT_NAMES=ON -DSQ_DISABLE_INTERPRETER=ON ..
if ERRORLEVEL 1 (
  ECHO cmake generation for thirdparty\squirrel failed.
  EXIT /B 1
)

cmake --build . --config Debug
if ERRORLEVEL 1 (
  ECHO cmake --build Debug for thirdparty\squirrel failed.
  EXIT /B 1
)

cmake --build . --config Release
if ERRORLEVEL 1 (
  ECHO cmake --build Release for thirdparty\squirrel failed.
  EXIT /B 1
)
POPD


REM Build zip-utils.
MKDIR thirdparty\zip-utils\out
PUSHD thirdparty\zip-utils\out
cmake -G %CMAKE_MSVC_GEN_NAME% -A %CMAKE_MSVC_ARCH_NAME% -DZU_ENABLE_UNIT_TESTS=OFF ..
if ERRORLEVEL 1 (
  ECHO cmake generation for thirdparty\zip-utils failed.
  EXIT /B 1
)

cmake --build . --config Debug
if ERRORLEVEL 1 (
  ECHO cmake --build Debug for thirdparty\zip-utils failed.
  EXIT /B 1
)

cmake --build . --config Release
if ERRORLEVEL 1 (
  ECHO cmake --build Release for thirdparty\zip-utils failed.
  EXIT /B 1
)
POPD


EXIT /B 0