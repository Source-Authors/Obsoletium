@ECHO OFF

call create_hammer.bat hammer x86
if ERRORLEVEL 1 (
  ECHO Generating x86 hammer_x86.sln failed.
  EXIT /B 1
)
