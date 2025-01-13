@ECHO OFF

call create_hammer.bat hammer x64
if ERRORLEVEL 1 (
  ECHO Generating x64 hammer_x64.sln failed.
  EXIT /B 1
)
