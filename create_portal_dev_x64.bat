@ECHO OFF

call creategameprojects_debug.bat portal x64
if ERRORLEVEL 1 (
  ECHO Generating Portal x64 portal.sln failed.
  EXIT /B 1
)


