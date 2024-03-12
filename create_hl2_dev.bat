@ECHO OFF

call creategameprojects_debug.bat hl2
if ERRORLEVEL 1 (
  ECHO Generating Half-Life 2 hl2.sln failed.
  EXIT /B 1
)


