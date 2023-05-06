
## How to build

* [windows] Visual Studio 2022 with v143+ platform toolset.
* Use `git clone --recurse-submodules`
* Add registry keys:
  | Path                        | Name  | Type | Data |
  | ----                        | ----  | ---- | ---- |
  | `HKLM\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\10.0\Projects\{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}` | `DefaultProjectExtension` | string 
* Run `creategameprojects_debug.bat` from Developer command prompt.
* Open `hl2.sln`
* Build

If you found a bug, please file it at
https://github.com/Source-Authors/obsolete-source-engine/issues.
