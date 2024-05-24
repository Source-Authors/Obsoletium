@ECHO OFF

call creategameprojects_debug.bat hl2 x64
if ERRORLEVEL 1 (
  ECHO Generating Half-Life 2 x64 hl2.sln failed.
  EXIT /B 1
)


