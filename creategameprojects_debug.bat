@echo ON
MSBuild.exe /? >NUL
if ERRORLEVEL 1 (
  echo MsBuild not found in PATH. Please, start from Developer Command Prompt or add MSVC MsBuild directory to the PATH.
  exit /B 1
)

rem Add/check registry keys for VPC
reg query HKLM\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\10.0\Projects\{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942} /t REG_SZ /v DefaultProjectExtension /f vcproj
if ERRORLEVEL 1 (
  echo Missed registry key HKLM\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\10.0\Projects\{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942} REG_SZ DefaultProjectExtension vcproj.
  reg add HKLM\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\10.0\Projects\{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942} /t REG_SZ /v DefaultProjectExtension /d vcproj

  if ERRORLEVEL 1 (
    echo Unable to add registry key HKLM\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\10.0\Projects\{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942} REG_SZ DefaultProjectExtension vcproj.
    exit /B 1
  )
)

start .\download_libs.bat

set ARCH=Win32

rem Build bzip2
pushd thirdparty\bzip2
nmake -f makefile.msc

if ERRORLEVEL 1 (
  echo nmake for thirdparty\bzip2 failed.
  exit /B 1
)
popd

rem Build libjpeg-turbo
mkdir thirdparty\libjpeg-turbo\out
pushd thirdparty\libjpeg-turbo\out
cmake -G "Visual Studio 17 2022" -A %ARCH% -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_CDJPEG=OFF -DWITH_TURBOJPEG=OFF ..
if ERRORLEVEL 1 (
  echo cmake generation for thirdparty\libjpeg-turbo failed.
  exit /B 1
)

cmake --build . --config Release
if ERRORLEVEL 1 (
  echo cmake --build for thirdparty\libjpeg-turbo failed.
  exit /B 1
)
popd

rem Build zlib
mkdir thirdparty\zlib\out
pushd thirdparty\zlib\out
cmake -G "Visual Studio 17 2022" -A %ARCH% ..
if ERRORLEVEL 1 (
  echo cmake generation for thirdparty\zlib failed.
  exit /B 1
)

cmake --build . --config Release
if ERRORLEVEL 1 (
  echo cmake --build for thirdparty\zlib failed.
  exit /B 1
)
popd

set ZLIB_SOURCE_DIR=%cd%\thirdparty\zlib

rem Build libpng
mkdir thirdparty\libpng\out
pushd thirdparty\libpng\out
cmake -G "Visual Studio 17 2022" -A %ARCH% -DPNG_BUILD_ZLIB=ON -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_EXECUTABLES=OFF -DPNG_TESTS=OFF -DZLIB_INCLUDE_DIRS=%ZLIB_SOURCE_DIR%;%ZLIB_SOURCE_DIR%\out -DZLIB_LIBRARIES=%ZLIB_SOURCE_DIR%\out\zlibstatic.lib ..
if ERRORLEVEL 1 (
  echo cmake generation for thirdparty\libpng failed.
  exit /B 1
)

cmake --build . --config Release
if ERRORLEVEL 1 (
  echo cmake --build for thirdparty\libpng failed.
  exit /B 1
)
popd

rem Build SDL
mkdir thirdparty\SDL\out
pushd thirdparty\SDL\out
cmake -G "Visual Studio 17 2022" -A %ARCH% -DSDL_TEST=OFF ..
if ERRORLEVEL 1 (
  echo cmake generation for thirdparty\SDL failed.
  exit /B 1
)

cmake --build . --config Release
if ERRORLEVEL 1 (
  echo cmake --build for thirdparty\SDL failed.
  exit /B 1
)
popd

rem Build protobuf
MSBuild.exe /m /p:Platform="x86" /p:Configuration=Debug thirdparty\protobuf\vsprojects\libprotobuf.vcxproj
if ERRORLEVEL 1 (
  echo MSBuild Debug for thirdparty\protobuf\libprotobuf failed.
  exit /B 1
)
MSBuild.exe /m /p:Platform="x86" /p:Configuration=Release thirdparty\protobuf\vsprojects\libprotobuf.vcxproj
if ERRORLEVEL 1 (
  echo MSBuild Release for thirdparty\protobuf\libprotobuf failed.
  exit /B 1
)
MSBuild.exe /m /p:Platform="x86" /p:Configuration=Debug thirdparty\protobuf\vsprojects\protoc.vcxproj
if ERRORLEVEL 1 (
  echo MSBuild Debug for thirdparty\protobuf\protoc failed.
  exit /B 1
)
MSBuild.exe /m /p:Platform="x86" /p:Configuration=Release thirdparty\protobuf\vsprojects\protoc.vcxproj
if ERRORLEVEL 1 (
  echo MSBuild Release for thirdparty\protobuf\protoc failed.
  exit /B 1
)

MSBuild.exe /m /p:Platform="x86" /p:Configuration=Release external/vpc/vpc.sln
devtools\bin\vpc.exe /2015 /define:WORKSHOP_IMPORT_DISABLE /define:SIXENSE_DISABLE /define:NO_X360_XDK /define:RAD_TELEMETRY_DISABLED /define:DISABLE_ETW /define:NO_STEAM /define:NO_ATI_COMPRESS /define:NO_NVTC /define:LTCG /no_ceg /nofpo /hl2 +game /mksln hl2.sln
