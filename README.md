# Source V1 Engine

[![Build](https://github.com/Source-Authors/Obsoletium/actions/workflows/build.yml/badge.svg)](https://github.com/Source-Authors/Obsoletium/actions/workflows/build.yml)
[![GitHub Repo Size in Bytes](https://img.shields.io/github/repo-size/Source-Authors/Obsoletium.svg)](https://github.com/Source-Authors/Obsoletium)

## Notes

Valve, the Valve logo, Half-Life, the Half-Life logo, the Lambda logo, Steam, the Steam logo, Team Fortress, the Team Fortress logo, Opposing Force, Day of Defeat, the Day of Defeat logo, Counter-Strike, the Counter-Strike logo, Source, the Source logo, Counter-Strike: Condition Zero, Portal, the Portal logo, Dota, the Dota 2 logo, and Defense of the Ancients are trademarks and/or registered trademarks of Valve Corporation. All other trademarks are property of their respective owners. See https://store.steampowered.com/legal for Valve Corporation legal details.

This repo is based on Valve's Source Engine from 2018. Please, use this for studying only, not for commercial purposes.
See https://partner.steamgames.com/doc/sdk/uploading/distributing_source_engine if you want to distribute something using Source Engine.

## Prerequisites [Windows]

* Git. Follow [`Git downloads`](https://git-scm.com/downloads).
* CMake. Can be [`C++ CMake Tools for Windows`](https://learn.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio#installation) component of Visual Studio.
* Visual Studio 2022 with v143+ platform toolset.
  Use `Desktop development with C++` **workload** and select additional
  `Windows 11 SDK <latest version>`,
  `C++ ATL for latest v143 build tools`,
  `C++ MFC for latest v143 build tools`
  **individual components**.
* Create [`Github account`](https://docs.github.com/en/get-started/start-your-journey/creating-an-account-on-github) if needed.
* Setup [`SSH key connection`](https://docs.github.com/en/authentication/connecting-to-github-with-ssh/adding-a-new-ssh-key-to-your-github-account#prerequisites) for your Github account.
* Half-Life 2, Half-Life 2 Episode 1/2 or Portal game.
  You need ```steam_legacy``` branch for Half-Life 2 and episodes, as [`20th Anniversary Update`](https://www.half-life.com/en/halflife2/20th)
  not supported *yet*.

## How to build games

* Open Git bash (search in Start menu).
* Change directory to place where source code be located by typing:
  `cd <directory to place source>`.
* Clone repository by `git clone --recurse-submodules git@github.com:Source-Authors/Obsoletium.git`
  in Git bash.
* Open Visual Studio 2022 `<ARCH> Native Tools Command Prompt For VS2022`
  shortcut (search in Start menu) where `<ARCH>` is CPU architecture you want to
  build for.  Supported values are `x86` and `x64`. `x64` is **experimental**.
* Run `create_<GAME_NAME>_dev_<ARCH>.bat` in opened command prompt where
  `<GAME_NAME>` is game to build, `<ARCH>` is same as in step above.  See cloned
  repository directory tree for supported games.
* Open `<GAME_NAME>_<ARCH>.sln`.
* Build by selecting `Build` menu, `Build Solution` submenu.


## How to build Hammer

* Open Git bash (search in Start menu).
* Change directory to place where source code be located by typing:
  `cd <directory to place source>`.
* Clone repository by `git clone --recurse-submodules git@github.com:Source-Authors/Obsoletium.git`
  in Git bash.
* Open Visual Studio 2022 `<ARCH> Native Tools Command Prompt For VS2022`
  shortcut (search in Start menu) where `<ARCH>` is CPU architecture you want to
  build for.  Supported values are `x86` and `x64`. `x64` is **experimental**.
* Run `create_hammer_<ARCH>.bat` in opened command prompt where `<ARCH>` is same
  as in step above.
* Open `hammer_<ARCH>.sln`.
* Build by selecting `Build` menu, `Build Solution` submenu.


## How to debug games

* Ensure you placed hl2 / episodic / portal game into `game` folder near cloned 
  repository.  Your directory structure should look like this:
```
  |
  |-- game <directory with game>
  |
  |-- source <directory with cloned source repository>
```
* Open `<GAME_NAME>_<ARCH>.sln` in source directory.
* Set `launcher_main` project `Command` property to `$(SolutionDir)..\game\hl2.exe`.
* Set `launcher_main` project `Command Arguments` property to `+mat_dxlevel 85 -windowed`.
* Set `launcher_main` project `Working Directory` property to `$(SolutionDir)..\game\`.
* Click on `Set as Startup Project` menu for `launcher_main` project.
* Start debugging.


## How to release games

* Ensure you placed hl2 / episodic / portal game into `game` folder near cloned 
  repository.  Your directory structure should look like this:
```
  |
  |-- game <directory with game>
  |
  |-- source <directory with cloned source repository>
```
* Open `<GAME_NAME>_<ARCH>.sln` in source directory.
* Choose `Release` Configuration.
* Build.
* `game` folder contains the ready game.


## How to debug Hammer

* Ensure you placed hl2 / episodic / portal game into `game` folder near cloned 
  repository.  Your directory structure should look like this:
```
  |
  |-- game <directory with game>
  |
  |-- source <directory with cloned source repository>
```
* Open `hammer_<ARCH>.sln`.  **Append ```x64\``` to ```bin``` in commands below
  for x64 Hammer**.
* Set `hammer_launcher` project `Command` property to `$(SolutionDir)..\game\bin\hammer.exe`.
* Set `hammer_launcher` project `Working Directory` property to `$(SolutionDir)..\game\bin`.
* Click on `Set as Startup Project` menu for `hammer_launcher` project.
* Start debugging.


If you found a bug, please file it at https://github.com/Source-Authors/obsolete-source-engine/issues.


## SAST Tools

[PVS-Studio](https://pvs-studio.com/en/pvs-studio/?utm_source=website&utm_medium=github&utm_campaign=open_source) - static analyzer for C, C++, C#, and Java code.
