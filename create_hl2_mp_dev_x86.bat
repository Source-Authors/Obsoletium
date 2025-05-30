@ECHO OFF

CALL build_game_thirdparty.bat x86
IF ERRORLEVEL 1 (
  ECHO Building thirdparty x86 failed.
  EXIT /B 1
)

CALL create_game_projects.bat hl2mp x86
IF ERRORLEVEL 1 (
  ECHO Generating Half-Life 2: Deathmatch x86 hl2mp.sln failed.
  EXIT /B 1
)
