@ECHO OFF

CALL create_game_projects.bat episodic x64
IF ERRORLEVEL 1 (
  ECHO Generating Half-Life 2 Episode 1/2 x64 episodic.sln failed.
  EXIT /B 1
)
