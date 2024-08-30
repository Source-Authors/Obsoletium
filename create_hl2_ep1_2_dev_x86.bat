@ECHO OFF

CALL create_game_projects.bat episodic x86
IF ERRORLEVEL 1 (
  ECHO Generating Half-Life 2 Episode 1/2 x86 episodic.sln failed.
  EXIT /B 1
)
