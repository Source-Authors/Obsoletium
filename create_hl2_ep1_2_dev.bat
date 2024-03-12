@ECHO OFF

call creategameprojects_debug.bat episodic
if ERRORLEVEL 1 (
  ECHO Generating Half-Life 2 Episode 1/2 episodic.sln failed.
  EXIT /B 1
)


