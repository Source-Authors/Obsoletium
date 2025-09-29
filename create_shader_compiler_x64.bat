@ECHO OFF

CALL build_vpc.bat
IF ERRORLEVEL 1 (
  ECHO Builidng VPC failed.
  EXIT /B 1
)

CALL generate_build_info.bat
IF ERRORLEVEL 1 (
  ECHO Generating build info failed.
  EXIT /B 1
)

CALL create_shader_compiler.bat shadercompile x64
if ERRORLEVEL 1 (
  ECHO Generating x64 shadercompile_x64.sln failed.
  EXIT /B 1
)
