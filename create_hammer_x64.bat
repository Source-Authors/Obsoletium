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

call create_hammer.bat hammer x64
if ERRORLEVEL 1 (
  ECHO Generating x64 hammer_x64.sln failed.
  EXIT /B 1
)
