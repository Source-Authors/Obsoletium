@ECHO OFF

CALL build_game_thirdparty.bat x86
IF ERRORLEVEL 1 (
  ECHO Building thirdparty x86 failed.
  EXIT /B 1
)

CALL generate_build_info.bat
IF ERRORLEVEL 1 (
  ECHO Generating build info failed.
  EXIT /B 1
)

call create_hammer.bat hammer x86
if ERRORLEVEL 1 (
  ECHO Generating x86 hammer_x86.sln failed.
  EXIT /B 1
)
