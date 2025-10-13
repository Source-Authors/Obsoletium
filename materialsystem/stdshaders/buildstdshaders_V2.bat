@echo off
setlocal

rem This script builds all the shaders... like you're a real game or something.

set BUILD_SHADER=call buildshaders_V2.bat

set SOURCE_DIR="..\..\"
set GAME_DIR="..\..\..\game\hl2"

%BUILD_SHADER% stdshader_dx9_20b       -game %GAME_DIR% -source %SOURCE_DIR%
%BUILD_SHADER% stdshader_dx9_20b_new   -game %GAME_DIR% -source %SOURCE_DIR%
%BUILD_SHADER% stdshader_dx9_30        -game %GAME_DIR% -source %SOURCE_DIR% -force30
