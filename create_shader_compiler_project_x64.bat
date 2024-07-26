@ECHO OFF

call create_shader_compiler_project.bat shadercompile x64
if ERRORLEVEL 1 (
  ECHO Generating Half-Life 2 x64 shadercompile_x64.sln failed.
  EXIT /B 1
)


