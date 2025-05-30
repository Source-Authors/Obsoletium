@ECHO OFF

CALL build_game_thirdparty.bat x64
IF ERRORLEVEL 1 (
  ECHO Building thirdparty x64 failed.
  EXIT /B 1
)

CALL create_game_projects.bat episodic x64
IF ERRORLEVEL 1 (
  ECHO Generating Half-Life 2: Episode 1 & 2 x64 episodic.sln failed.
  EXIT /B 1
)
