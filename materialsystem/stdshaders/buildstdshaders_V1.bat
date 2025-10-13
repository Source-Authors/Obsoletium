@echo off
setlocal

set TTEXE=..\..\devtools\bin\timeprecise.exe
if not exist %TTEXE% goto no_ttexe
goto no_ttexe_end

:no_ttexe
set TTEXE=time /t
:no_ttexe_end


rem echo.
rem echo ~~~~~~ buildstdshaders_V1 %* ~~~~~~
%TTEXE% -cur-Q
set tt_all_start=%ERRORLEVEL%
set tt_all_chkpt=%tt_start%



set sourcedir="shaders"
set targetdir="..\..\..\game\hl2\shaders"

call clean.bat

set BUILD_SHADER=call buildshaders_V1.bat

%BUILD_SHADER% stdshader_dx9_20b
%BUILD_SHADER% stdshader_dx9_20b_new	-dx9_30
%BUILD_SHADER% stdshader_dx9_30	-dx9_30	-force30
REM %BUILD_SHADER% stdshader_dx11     -dx11



rem echo.
if not "%dynamic_shaders%" == "1" (
  rem echo Finished full buildstdshaders_V1 %*
) else (
  rem echo Finished dynamic buildstdshaders_V1 %*
)

rem %TTEXE% -diff %tt_all_start% -cur
rem echo.
