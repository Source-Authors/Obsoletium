@ECHO OFF

CALL create_game_projects.bat hl2 x64
IF ERRORLEVEL 1 (
  ECHO Generating Half-Life 2 x64 hl2.sln failed.
  EXIT /B 1
)
