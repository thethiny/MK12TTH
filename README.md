# MK12TTH
### Formerly ASIMK12
MK12 TTH is the name I'm giving to the dll hook mods. Basically they're similar to Cheat Engine mods but they're applied to the game directly, this means you don't have to manually open Cheat Engine everytime you want to apply some mods, and you don't have to patch the exe to apply anti cheats.
What is it used for? It's used to modify game functions to make them do whatever you want them to, making the game act like you coded it.

### Current Features:
- Anti Pak Signature Patch. Allows you to run `.pak` files with invalid `.sig` signatures.
- Anti uToc Signature Patch. Allows you to use `uToc` files (and `.uCas`) with invalid Signature Headers
- Anti Pak Chunk Signature Patch. Optional, allows you to delete `.sig` files, since the game still loads the `.sig` files and runs checks against them on load, but will only give a warning. However, some large files require a `.sig` file due to the warning assuming they're there, so this Patch disables this warning.

### How to use:
Download the Latest MK12TTH version, and select the mods you want in the provided ini file. When a game updates, you might need to change the patches in the [Patterns] section. The updated patterns will be provided by me below, and in the downloaded file.
The zip file will contain 3 files. MK12TTH.ehp, MK12TTH.ini, and dsound.dll. You need to place the files into your game's folder, next to SDL2.dll. `MK1Folder/MK12/Binaries/Win64/`.

To load modded files, create a folder called Pakchunk99 in the content directory `MK1Folder/MK12/Content/Paks`, and inside it create a new folder with the mod you're planning to install (optional). If the mod was a texture/model/utoc/ucas file, then place it in Paks directly, since they need to be loaded last, and MK1 loads folders first.

### Ermaccer's MK1Hook Compatibility:
This mod is designed to be compatible with [Ermaccer's MK1Hook](https://github.com/ermaccer/MK1Hook). When the hook is detected to be present, MK12TTH becomes a plugin instead of a standalone mod. If you intend to use the 2 together, then use the dsound.dll from Ermaccer's MK1Hook and not the one provided in the downloaded zip file.
#### _NOTE: The current version of MK1Hook (0.3) doesn't support mod patches, therefore using the 2 hooks together is not currently possible. Wait for un update from me on when they will work together!_
#### _NOTE2: MK1Hook (0.4) will support mod patches._

### All Ini Settings
```ini
    [Debug]
    bEnableConsoleWindow = false
    bPauseOnStart = true
    bDebug = true
    iVerbose = 10

    [Settings]
    iLogSize = 100
    szModLoader = Kernel32.CreateFileW
    szCurlSetOpt = libcurl.curl_easy_setopt
    szCurlPerform = libcurl.curl_easy_perform

    [Patches]
    bPatchCurl = false

    [Patches.AntiCheat]
    bDisableSignatureCheck = true
    bDisableChunkSigCheck = true
    bDisableTOCSigCheck = true

    [Patterns]
    pSigCheck = 80 b9 ? ? ? ? 00 49 8b f0 48 8b fa 48 8b d9 75
    pSigWarn = 4c 03 f1 80 3D ? ? ? ? 03 0f 82
    pTocCheck = 4D 8B CE 45 33 C0 E8 ? ? ? ? 44 39 Bd ? ? ? ? 0F 84
    pChunkSigCheck = 0F B6 51 ? 48 8b f1 48 8b 0d ? ? ? ? E8 ? ? ? ? C6 46 ? ? 0F AE F8
    pChunkSigCheckFunc = 48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 83 EC 20 48 8D 59 08 49 63 F9 48 8B F1 49 8B E8 48 8B CB 44 0F B6 F2

```
You can find the [sample ini file](sample.ini) in this repo

## Download
[Download Here](https://github.com/thethiny/MK12TTH/releases)
