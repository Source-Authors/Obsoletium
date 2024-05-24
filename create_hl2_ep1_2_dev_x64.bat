@ECHO OFF

call creategameprojects_debug.bat episodic x64
if ERRORLEVEL 1 (
  ECHO Generating Half-Life 2 Episode 1/2 x64 episodic.sln failed.
  EXIT /B 1
)


