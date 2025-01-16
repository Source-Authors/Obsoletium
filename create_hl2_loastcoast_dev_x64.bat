@ECHO OFF

CALL create_game_projects.bat loastcoast x64
IF ERRORLEVEL 1 (
  ECHO Generating Half-Life 2: Loast Coast x64 hl2_loastcoast.sln failed.
  EXIT /B 1
)
