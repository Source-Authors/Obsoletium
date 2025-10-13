@ECHO OFF


REM Build VPC.
MKDIR external\vpc\out
PUSHD external\vpc\out
cmake -G Ninja ..
if ERRORLEVEL 1 (
  ECHO cmake generation for external\vpc failed.
  EXIT /B 1
)

cmake --build . --config Release
if ERRORLEVEL 1 (
  ECHO cmake --build for external\vpc failed.
  EXIT /B 1
)

COPY /V /Y vpc.exe /B ..\..\..\devtools\bin\vpc.exe /B
if ERRORLEVEL 1 (
  ECHO No vpc.exe to copy to devtools\bin\.
  EXIT /B 1
)

COPY /V /Y vpc.pdb /B ..\..\..\devtools\bin\vpc.pdb /B
if ERRORLEVEL 1 (
  ECHO No vpc.pdb to copy to devtools\bin\.
  EXIT /B 1
)
POPD


EXIT /B 0
