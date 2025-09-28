@ECHO OFF
Setlocal EnableDelayedExpansion

REM Generate version info, etc.
MKDIR out
PUSHD out
cmake ..\CMakeLists.txt
if ERRORLEVEL 1 (
  ECHO cmake version generation failed.
  EXIT /B 1
)
POPD


EXIT /B 0