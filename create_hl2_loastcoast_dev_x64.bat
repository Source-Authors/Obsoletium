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

CALL create_game_projects.bat loastcoast x64
IF ERRORLEVEL 1 (
  ECHO Generating Half-Life 2: Loast Coast x64 hl2_loastcoast.sln failed.
  EXIT /B 1
)
