@echo off
REM Usage: texttoarray.bat <filename> <name>
if "%~2"=="" (
    echo Usage: texttoarray.bat ^<filename^> ^<name^>
    exit /b 1
)

set "filename=%~1"
set "objname=%~2"

powershell -Command ^
    "$data = Get-Content -Path '%filename%' -Encoding Byte;" ^
    "$out = 'static constexpr unsigned char %objname%[] = {';" ^
    "$out += \"`n    \";" ^
    "$i = 0;" ^
    "foreach ($b in $data) {" ^
    "  $out += ('0x{0:x2},' -f $b);" ^
    "  $i++;" ^
    "  if ($i %% 20 -eq 0) { $out += \"`n    \" }" ^
    "}" ^
    "$out += '0x00};';" ^
    "Write-Output $out"
