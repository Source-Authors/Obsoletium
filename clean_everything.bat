@echo off

# RaphaelIT7: A useful helper script since when changing between x86 & x64 you may need a completely clean setup or such (I need this quite frequently)

echo WARNING: This will delete ALL untracked files, reset ALL changes, and clean all submodules.
echo Are you sure you want to continue? (Y/N)
set /p CONFIRM="> "

if /I not "%CONFIRM%"=="Y" (
    echo Aborted.
    exit /b 1
)

git clean -xfd
git submodule foreach --recursive git clean -xfd
git reset --hard
git submodule foreach --recursive git reset --hard
git submodule update --init --recursive