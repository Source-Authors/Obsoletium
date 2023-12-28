# Source engine

[![CI](https://github.com/Source-Authors/obsolete-source-engine/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/Source-Authors/obsolete-source-engine/actions/workflows/build.yml)


## Prerequisites

* CMake. Can be [`C++ CMake Tools`](https://learn.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio#installation) component of Visual Studio.
* [windows] Visual Studio 2022 with v143+ platform toolset.


## How to build

* Use `git clone --recurse-submodules`
* Add registry keys:
  | Path                        | Name  | Type | Data |
  | ----                        | ----  | ---- | ---- |
  | `HKLM\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\10.0\Projects\{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}` | `DefaultProjectExtension` | string | vcproj
* Run `creategameprojects_debug.bat` from `Developer Command Prompt`.
* Open `hl2.sln`.
* Build.


## How to debug

* Ensure you placed hl2 / episodic / portal game into `game` folder near cloned repo.
* Open `hl2.sln`.
* Set `launcher_main` project `Command` property to `$(SolutionDir)..\game\hl2.exe`.
* Set `launcher_main` project `Command Arguments` property to `-dxlevel 85 -windowed`.
* Set `launcher_main` project `Working Directory` property to `$(SolutionDir)..\game\`.
* Use `Set as Startup Project` menu for `launcher_main` project.
* Start debugging.


## How to release

* Ensure you placed hl2 / episodic / portal game into `game` folder near cloned repo.
* Open `hl2.sln`.
* Choose `Release` Configuration.
* Build.
* `game` folder contains the ready game.


If you found a bug, please file it at https://github.com/Source-Authors/obsolete-source-engine/issues.
