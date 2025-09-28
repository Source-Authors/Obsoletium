@ECHO OFF

CALL build_game_thirdparty.bat x64
IF ERRORLEVEL 1 (
  ECHO Building thirdparty x64 failed.
  EXIT /B 1
)

CALL generate_build_info.bat
IF ERRORLEVEL 1 (
  ECHO Generating build info failed.
  EXIT /B 1
)

CALL create_game_projects.bat portal x64
IF ERRORLEVEL 1 (
  ECHO Generating Portal x64 portal.sln failed.
  EXIT /B 1
)
