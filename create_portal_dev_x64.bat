@ECHO OFF

CALL create_game_projects.bat portal x64
IF ERRORLEVEL 1 (
  ECHO Generating Portal x64 portal.sln failed.
  EXIT /B 1
)
