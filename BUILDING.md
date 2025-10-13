# Building Obsoletium

Depending on what platform or features you need, the build process may
differ. After you've built a binary, running the
test suite to confirm that the binary works as intended is a good next step.

If you can reproduce a test failure, search for it in the
[Obsoletium issue tracker](https://github.com/Source-Authors/Obsoletium/issues) or
file a new issue.

## Table of contents

* [Supported platforms](#supported-platforms)
  * [Strategy](#strategy)
  * [Platform list](#platform-list)
  * [Supported toolchains](#supported-toolchains)
  * [Official binary platforms and toolchains](#official-binary-platforms-and-toolchains)
* [Building Obsoletium on supported platforms](#building-obsoletium-on-supported-platforms)
  * [Prerequisites](#prerequisites)
  * [Windows](#windows)
    * [Windows Prerequisites](#windows-prerequisites)
    * [How to build games](#how-to-build-games)
    * [How to debug games](#how-to-debug-games)
    * [How to release games](#how-to-release-games)
    * [How to build Hammer](#how-to-build-hammer)
    * [How to debug Hammer](#how-to-debug-hammer)

## Supported platforms

This list of supported platforms is current as of the branch/release to
which it belongs.

### Strategy

There are three support tiers:

* **Tier 1**: These platforms represent the majority of Obsoletium users. Infrastructure has full test coverage.
  Test failures on tier 1 platforms will block releases.
* **Tier 2**: These platforms represent smaller segments of the Obsoletium user
  base. Infrastructure has full test coverage. Test failures on tier 2 platforms will block releases.
  Infrastructure issues may delay the release of binaries for these platforms.
* **Experimental**: May not compile or test suite may not pass. Releases are not created for these platforms. Test failures on experimental
  platforms do not block releases. Contributions to improve support for these
  platforms are welcome.

Platforms may move between tiers between major release lines. The table below
will reflect those changes.

### Platform list

Obsoletium compilation/execution support depends on operating system, architecture,
and libc version. The table below lists the support tier for each supported
combination. A list of [supported compile toolchains](#supported-toolchains) is
also supplied for tier 1 platforms.

**For production applications, run Obsoletium on supported platforms only.**

Obsoletium does not support a platform version if a vendor has expired support
for it. In other words, Obsoletium does not support running on End-of-Life (EoL)
platforms. This is true regardless of entries in the table below.

| Operating System | Architectures    | Versions                          | Support Type | Notes                                |
| ---------------- | ---------------- | --------------------------------- | ------------ | ------------------------------------ |
| GNU/Linux        | x64              | kernel >= 4.18[^1], glibc >= 2.28 | Experimental       | e.g. Ubuntu 20.04, Debian 10, RHEL 8 |
| GNU/Linux        | arm64            | kernel >= 4.18[^1], glibc >= 2.28 | Experimental       | e.g. Ubuntu 20.04, Debian 10, RHEL 8 |
| GNU/Linux        | loong64          | kernel >= 5.19, glibc >= 2.36     | Experimental |                                      |
| Windows          | x64              | >= Windows 10/Server 2016         | Tier 1       | [^2],                                |
| Windows          | arm64            | >= Windows 10                     | Experimental       |                                      |
| macOS            | x64              | >= 13.5                           | Experimental       | For notes about compilation see [^3] |
| macOS            | arm64            | >= 13.5                           | Experimental       |                                      |
| FreeBSD          | x64              | >= 13.2                           | Experimental |                                      |

[^1]: Older kernel versions may work.

[^2]: The Windows Subsystem for Linux (WSL) is not
    supported, but the GNU/Linux build process and binaries should work. The
    community will only address issues that reproduce on native GNU/Linux
    systems. Issues that only reproduce on WSL should be reported in the
    [WSL issue tracker](https://github.com/Microsoft/WSL/issues). Running the
    Windows binary (`hl2{_win64}.exe`) in WSL will not work without workarounds such as
    stdio redirection.

[^3]: Our macOS Binaries are compiled with 13.5 as a target. Xcode 16 is
    required to compile.

### Supported toolchains

Depending on the host platform, the selection of toolchains may vary.

| Operating System | Compiler Versions                                              |
| ---------------- | -------------------------------------------------------------- |
| Linux            | GCC >= 12.2 or Clang >= 19.1                                   |
| Windows          | Visual Studio >= 2022 with the Windows 11 SDK on a 64-bit host |
| macOS            | Xcode >= 16.3 (Apple LLVM >= 19)                               |

### Official binary platforms and toolchains

Binaries at <https://github.com/Source-Authors/Obsoletium/releases> are produced on:

| Binary package          | Platform and Toolchain                                        |
| ----------------------- | ------------------------------------------------------------- |
| darwin-x64              | macOS 13, Xcode 16 with -mmacosx-version-min=13.5             |
| darwin-arm64 (and .pkg) | macOS 13 (arm64), Xcode 16 with -mmacosx-version-min=13.5     |
| linux-arm64             | RHEL 8 with Clang 19.1 and gcc-toolset-14-libatomic-devel[^1] |
| linux-x64               | RHEL 8 with Clang 19.1 and gcc-toolset-14-libatomic-devel[^1] |
| win-arm64               | Windows Server 2022 (x64) with Visual Studio 2022             |
| win-x64                 | Windows Server 2022 (x64) with Visual Studio 2022             |

[^1]: Binaries produced on these systems are compatible with glibc >= 2.28
    and libstdc++ >= 6.0.25 (`GLIBCXX_3.4.25`). These are available on
    distributions natively supporting GCC 8.1 or higher, such as Debian 10,
    RHEL 8 and Ubuntu 20.04.

## Building Obsoletium on supported platforms

### Prerequisites

To run the games you need to _own_ ones. Check Steam store pages for Half-Life 2, Half-Life 2 Episode 1/2 or Portal and buy them. **Note you need ```steam_legacy``` branch for Half-Life 2 and episodes, as [`20th Anniversary Update`](https://www.half-life.com/en/halflife2/20th) not supported *yet***.

#### Windows Prerequisites

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

### How to build games

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

### How to debug games

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
* Set `launcher_main` project `Command Arguments` property to `+mat_dxlevel 95 -windowed`.
* Set `launcher_main` project `Working Directory` property to `$(SolutionDir)..\game\`.
* Click on `Set as Startup Project` menu for `launcher_main` project.
* Start debugging.

### How to release games

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

### How to build Hammer

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

### How to debug Hammer

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
