@ECHO OFF

CALL create_game_projects.bat hl2mp x64
IF ERRORLEVEL 1 (
  ECHO Generating Half-Life 2 Multiplayer x64 hl2mp.sln failed.
  EXIT /B 1
)
