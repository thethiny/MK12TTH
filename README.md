# MK12TTH

[![Total Downloads](https://img.shields.io/github/downloads/thethiny/MK12TTH/total.svg)](https://github.com/thethiny/MK12TTH/releases)
[![Latest Release Downloads](https://img.shields.io/github/downloads/thethiny/MK12TTH/latest/total.svg)](https://github.com/thethiny/MK12TTH/releases/latest)
[![Ko-fi](https://img.shields.io/badge/Ko--fi-FF5E5B?logo=ko-fi&logoColor=white)](https://ko-fi.com/D1D219Y0XH)

### Formerly ASIMK12
MK12TTH is a hook mod for Mortal Kombat 1. It patches game functions at runtime using pattern scanning, so mods load without modifying the game executable or using Cheat Engine. Everything is set from inside the game.

### Features
- **In-game settings**: **MODS** in the main menu, **MK12TTH** in Settings, and **MOD SETTINGS** in the pause menu during fights. Nothing to edit by hand.
- **Pak mods**: put mods in `Content\Mods`, one folder per mod. Turn each mod, pak or folder on and off, see which mods override which, and give mods their own engine settings. See the [Modding Guide](ModdingGuide.md).
- **Announcer**: the game's own, your fighter's announcer in fights (AUTO), or any announcer.
- **Floyd**: your 10 challenges, encounters and wins, the Floyd Tower schedule and an alert when it shows up.
- **Floyd fight**: set his difficulty level and your retries, spawn the Floyd Tower now, or summon Floyd.
- **Profile**: your platform, platform ID, save key, MK1 and WB IDs, platform ticket and access token, with COPY.
- **Quality of life**: keep running in the background, fast boot, attract mode delay.
- **String swap**: swap game names using `string_swaps.txt`.
- **Private server**: send the game's server traffic to your own server (see the `[Server]` section of the ini).
- **MK1Hook plugin**: loads as a plugin for [Ermaccer's MK1Hook](https://github.com/ermaccer/MK1Hook), with its own overlay tab showing the same pages as MODS.

### How to Use
1. Download the latest release from the [Releases Page](https://github.com/thethiny/MK12TTH/releases).
2. Extract the zip into `MK1Folder/MK12/Binaries/Win64/` (next to `MK12.exe`).
3. Start the game. Open **MODS** from the main menu, or **Settings → MK12TTH**, to set everything up.
4. Put pak mods in `MK1Folder/MK12/Content/Mods/` (see the [Modding Guide](ModdingGuide.md)).

The zip contains:
- `MK12TTH.ehp`: the mod, loaded by MK1Hook or by the ASI loader
- `MK12TTH.asi`: loads the `.ehp` when MK1Hook isn't installed
- `MK12TTH.ini`: startup switches and byte patterns
- `dsound.dll`: the ASI loader

Your in-game choices are saved in `MK12TTH_State.ini` next to the mod. When the game updates and a feature stops working, its patterns in `MK12TTH.ini` may need updating; the menus show which features aren't running and why.

**Turn mods off before playing online.**

### MK1Hook Compatibility
MK12TTH works with [Ermaccer's MK1Hook](https://github.com/ermaccer/MK1Hook). When MK1Hook is installed, MK12TTH loads as its plugin and adds an MK12TTH tab to its overlay. If you use both, keep MK1Hook's `dsound.dll` instead of the one in this zip.

### INI Configuration
The [sample ini](source/sample.ini) lists every startup switch and pattern. A missing key takes the value shown there. Most players never need to open it: the same switches are in **Settings → MK12TTH**.

### Guides
- [Modding Guide](ModdingGuide.md): where mods go, load order, turning them on and off, `description.txt`, engine settings (`Engine.ini`) and string swaps.
- [Developer Guide](DeveloperGuide.md): adding a feature, from its pattern and hook to its switch in the ini, its rows in Settings, MODS and the pause menu, and the overlay.
- [Memory Patch Utility Guide](MemoryPatchUtilityGuide.md): the hooking and patching API, with assembly examples.

### Building
Requires Visual Studio 2022 with the v142 toolset installed (or Visual Studio 2019):
```
msbuild ASIMK12.sln /p:Configuration=Release /p:Platform=x64
```
The build outputs `MK12TTH.ehp` and `MK12TTH.asi` in `out/Release/Binaries/`.

### Download
[Download Latest](https://github.com/thethiny/MK12TTH/releases/latest)
