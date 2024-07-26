@ECHO OFF

call create_shader_compiler.bat shadercompile x64
if ERRORLEVEL 1 (
  ECHO Generating x64 shadercompile_x64.sln failed.
  EXIT /B 1
)
