@ECHO OFF

CALL build_game_thirdparty.bat x64
IF ERRORLEVEL 1 (
  ECHO Building thirdparty x64 failed.
  EXIT /B 1
)

CALL build_vpc.bat
IF ERRORLEVEL 1 (
  ECHO Building VPC failed.
  EXIT /B 1
)

CALL generate_build_info.bat
IF ERRORLEVEL 1 (
  ECHO Generating build info failed.
  EXIT /B 1
)

CALL create_game_projects_steam.bat hl2 x64
IF ERRORLEVEL 1 (
  ECHO Generating Half-Life 2 x64 Steam solution failed.
  EXIT /B 1
)

REM Copy SteamAPI runtime DLL to game\bin\x64
SET "STEAM_API_DLL=%~dp0lib\public\x64\steam_api64.dll"
SET "GAME_BIN_X64=%~dp0..\game\bin\x64"

IF NOT EXIST "%GAME_BIN_X64%" (
  MKDIR "%GAME_BIN_X64%"
)

COPY /Y "%STEAM_API_DLL%" "%GAME_BIN_X64%\steam_api64.dll"
IF ERRORLEVEL 1 (
  ECHO Copying steam_api64.dll failed.
  EXIT /B 1
)

ECHO SteamAPI runtime installed successfully.
