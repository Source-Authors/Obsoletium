@ECHO OFF

CALL create_game_projects.bat portal x86
IF ERRORLEVEL 1 (
  ECHO Generating Portal x86 portal.sln failed.
  EXIT /B 1
)


