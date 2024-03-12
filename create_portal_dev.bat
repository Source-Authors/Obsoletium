@ECHO OFF

call creategameprojects_debug.bat portal
if ERRORLEVEL 1 (
  ECHO Generating Portal portal.sln failed.
  EXIT /B 1
)


