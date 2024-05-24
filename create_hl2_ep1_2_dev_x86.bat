@ECHO OFF

call creategameprojects_debug.bat episodic x86
if ERRORLEVEL 1 (
  ECHO Generating Half-Life 2 Episode 1/2 x86 episodic.sln failed.
  EXIT /B 1
)


