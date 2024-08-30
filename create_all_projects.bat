@echo OFF
MSBuild.exe /?
if ERRORLEVEL 1 (
  echo MsBuild not found in PATH. Please, start from Developer Command Prompt or add MSVC MsBuild directory to the PATH.
  exit /B 1
)

MSBuild.exe /m /p:Platform="x86" /p:Configuration=Release external/vpc/vpc.sln
devtools\bin\vpc.exe /2013 +everything /mksln everything.sln
