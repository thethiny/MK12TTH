# MK12TTH

[![Total Downloads](https://img.shields.io/github/downloads/thethiny/MK12TTH/total.svg)](https://github.com/thethiny/MK12TTH/releases)
[![Latest Release Downloads](https://img.shields.io/github/downloads/thethiny/MK12TTH/latest/total.svg)](https://github.com/thethiny/MK12TTH/releases/latest)
[![Ko-fi](https://img.shields.io/badge/Ko--fi-FF5E5B?logo=ko-fi&logoColor=white)](https://ko-fi.com/D1D219Y0XH)

### Formerly ASIMK12
MK12 TTH is a DLL hook mod for Mortal Kombat 1. It patches game functions at runtime using pattern scanning, allowing you to apply mods without modifying the game executable or using Cheat Engine.

### Features
- **Anti-Cheat Bypass**: Patches Pak, Chunk, and TOC signature checks to allow loading modded `.pak`, `.utoc`, and `.ucas` files.
- **FName/FString Hooking**: Hooks Unreal Engine's internal FName-to-string conversion for asset swapping.
- **Announcer Swap**: Swap in-game announcer voices via INI configuration.
- **String Swap**: Swap arbitrary asset paths using a `string_swaps.txt` file.
- **Server Proxy**: Redirect game server endpoints to a custom/MITM server.
- **Floyd Tracking**: Extract and display secret fight (Floyd) challenge data including challenge IDs and conditions.
- **Profile Info**: Capture player profile identifiers (Platform, PlatformId, SaveKey, HydraId, WBId).
- **MK1Hook Plugin**: Integrates as an EHP plugin for [Ermaccer's MK1Hook](https://github.com/ermaccer/MK1Hook) with ImGui tab support.

### How to Use
1. Download the latest release from the [Releases Page](https://github.com/thethiny/MK12TTH/releases).
2. Extract the zip into your game folder: `MK1Folder/MK12/Binaries/Win64/` (next to `SDL2.dll`).
3. Edit `MK12TTH.ini` to configure which features to enable and set the appropriate byte patterns.
4. When a game updates, you may need to update the patterns in the `[Patterns]` section.

The zip contains:
- `MK12TTH.ehp` - Main hook DLL - Compatible with MK1Hook
- `MK12TTH.asi` - ASI loader (loads the EHP automatically when MK1Hook is missing)
- `MK12TTH.ini` - Configuration file
- `dsound.dll` - Proxy loader from MK1Hook

### Loading Modded Files
Create the following folder structure inside `MK1Folder/MK12/Content/Paks/`:

```
Paks/
├── global.ucas
├── global.utoc
├── pakchunk91-ucas/    ← Non-audio mods (textures, models, .ucas/.utoc files)
├── pakchunk92-og/      ← Original game files (required for some mods to reference)
└── pakchunk93-pak/     ← Audio mods (.pak files)
```

- Place non-audio mods (textures, models) in `pakchunk91-ucas/`.
- Place original game files in `pakchunk92-og/`.
- Place audio mods (.pak files) in `pakchunk93-pak/`.
- Keep `global.ucas` and `global.utoc` directly in `Paks/`, not inside any subfolder.

This is the most stable way to get mods working consistently.

### MK1Hook Compatibility
This mod is compatible with [Ermaccer's MK1Hook](https://github.com/ermaccer/MK1Hook). When MK1Hook is detected, MK12TTH loads as a plugin with an ImGui tab. If using both together, use MK1Hook's `dsound.dll` instead of the one provided.

### INI Configuration

You can find the [sample ini file](source/sample.ini) in this repo.

### Building
Requires Visual Studio 2019 (v142 toolset) or VS2022 with v142 installed:
```
msbuild ASIMK12.sln /p:Configuration=Release /p:Platform=x64
```

### Download
[Download Latest](https://github.com/thethiny/MK12TTH/releases/latest)
