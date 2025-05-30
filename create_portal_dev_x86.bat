@ECHO OFF

CALL build_game_thirdparty.bat x86
IF ERRORLEVEL 1 (
  ECHO Building thirdparty x86 failed.
  EXIT /B 1
)

CALL create_game_projects.bat portal x86
IF ERRORLEVEL 1 (
  ECHO Generating Portal x86 portal.sln failed.
  EXIT /B 1
)


